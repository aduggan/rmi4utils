/*
 * Copyright (C) 2014 Andrew Duggan
 * Copyright (C) 2014 Synaptics Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include <string>
#include <sstream>

#include "hiddevice.h"
#include "rmi4update.h"

#define RMI4UPDATE_GETOPTS	"hfd:p"

void printHelp(const char *prog_name)
{
	fprintf(stdout, "Usage: %s [OPTIONS] FIRMWAREFILE\n", prog_name);
	fprintf(stdout, "\t-h, --help\tPrint this message\n");
	fprintf(stdout, "\t-f, --force\tForce updating firmware even it the image provided is older\n\t\t\tthen the current firmware on the device.\n");
	fprintf(stdout, "\t-d, --device\thidraw device file associated with the device being updated.\n");
	fprintf(stdout, "\t-p, --fw-props\tPrint the firmware properties.\n");
}

int UpdateDevice(FirmwareImage & image, bool force, const char * deviceFile)
{
	HIDDevice rmidevice;
	int rc;

	rc = rmidevice.Open(deviceFile);
	if (rc)
		return rc;

	RMI4Update update(rmidevice, image);
	rc = update.UpdateFirmware(force);
	if (rc != UPDATE_SUCCESS)
		return rc;

	return rc;
}

int WriteDeviceNameToFile(const char * file, const char * str)
{
	int fd;
	ssize_t size;

	fd = open(file, O_WRONLY);
	if (fd < 0)
		return UPDATE_FAIL;

	for (;;) {
		size = write(fd, str, 19);
		if (size < 0) {
			if (errno == EINTR)
				continue;

			return UPDATE_FAIL;
		}
		break;
	}

	close(fd);

	return UPDATE_SUCCESS;
}

/*
 * We need to rebind the driver to the device after firmware update because the
 * parameters of the device may have changed in the new firmware and the
 * driver should redo the initialization ensure it is using the new values.
 */
void RebindDriver(const char * hidraw)
{
	int rc;
	ssize_t size;
	char bindFile[PATH_MAX];
	char unbindFile[PATH_MAX];
	char deviceLink[PATH_MAX];
	char driverName[PATH_MAX];
	char driverLink[PATH_MAX];
	char linkBuf[PATH_MAX];
	char hidDeviceString[20];
	int i;

	snprintf(unbindFile, PATH_MAX, "/sys/class/hidraw/%s/device/driver/unbind", hidraw);
	snprintf(deviceLink, PATH_MAX, "/sys/class/hidraw/%s/device", hidraw);

	size = readlink(deviceLink, linkBuf, PATH_MAX);
	if (size < 0) {
		fprintf(stderr, "Failed to find the HID string for this device: %s\n",
			hidraw);
		return;
	}
	linkBuf[size] = 0;

	strncpy(hidDeviceString, StripPath(linkBuf, size), 20);

	snprintf(driverLink, PATH_MAX, "/sys/class/hidraw/%s/device/driver", hidraw);

	size = readlink(driverLink, linkBuf, PATH_MAX);
	if (size < 0) {
		fprintf(stderr, "Failed to find the HID string for this device: %s\n",
			hidraw);
		return;
	}
	linkBuf[size] = 0;

	strncpy(driverName, StripPath(linkBuf, size), PATH_MAX);

	snprintf(bindFile, PATH_MAX, "/sys/bus/hid/drivers/%s/bind", driverName);

	rc = WriteDeviceNameToFile(unbindFile, hidDeviceString);
	if (rc != UPDATE_SUCCESS) {
		fprintf(stderr, "Failed to unbind HID device %s: %s\n",
			hidDeviceString, strerror(errno));
		return;
	}

	for (i = 0;; ++i) {
		struct timespec req;
		struct timespec rem;

		rc = WriteDeviceNameToFile(bindFile, hidDeviceString);
		if (rc == UPDATE_SUCCESS)
			return;

		if (i <= 4)
			break;

		/* device might not be ready yet to bind to */
		req.tv_sec = 0;
		req.tv_nsec = 100 * 1000 * 1000; /* 100 ms */
		for (;;) {
			rc = nanosleep(&req, &rem);
			if (rc < 0) {
				if (errno == EINTR) {
					req = rem;
					continue;
				}
			}
			break;
		}
	}

	fprintf(stderr, "Failed to bind HID device %s: %s\n",
		hidDeviceString, strerror(errno));
}

int GetFirmwareProps(const char * deviceFile, std::string &props)
{
	HIDDevice rmidevice;
	int rc = UPDATE_SUCCESS;
	std::stringstream ss;

	rc = rmidevice.Open(deviceFile);
	if (rc)
		return rc;

	rmidevice.ScanPDT(0x1);
	rmidevice.QueryBasicProperties();

	ss << rmidevice.GetFirmwareVersionMajor() << "."
		<< rmidevice.GetFirmwareVersionMinor() << "."
		<< std::hex << rmidevice.GetFirmwareID();

	if (rmidevice.InBootloader())
		ss << " bootloader";

	props = ss.str();

	return rc;
}

int main(int argc, char **argv)
{
	int rc;
	FirmwareImage image;
	int opt;
	int index;
	char *deviceName = NULL;
	const char *firmwareName = NULL;
	bool force = false;
	static struct option long_options[] = {
		{"help", 0, NULL, 'h'},
		{"force", 0, NULL, 'f'},
		{"device", 1, NULL, 'd'},
		{"fw-props", 0, NULL, 'p'},
		{0, 0, 0, 0},
	};
	struct dirent * devDirEntry;
	DIR * devDir;
	bool printFirmwareProps = false;

	while ((opt = getopt_long(argc, argv, RMI4UPDATE_GETOPTS, long_options, &index)) != -1) {
		switch (opt) {
			case 'h':
				printHelp(argv[0]);
				return 0;
			case 'f':
				force = true;
				break;
			case 'd':
				deviceName = optarg;
				break;
			case 'p':
				printFirmwareProps = true;
				break;
			default:
				break;

		}
	}

	if (printFirmwareProps) {
		std::string props;

		if (!deviceName) {
			fprintf(stderr, "Specifiy which device to query\n");
			return 1;
		}
		rc = GetFirmwareProps(deviceName, props);
		if (rc) {
			fprintf(stderr, "Failed to read properties from device: %s\n", update_err_to_string(rc));
			return 1;
		}
		fprintf(stdout, "%s\n", props.c_str());
		return 0;
	}

	if (optind < argc) {
		firmwareName = argv[optind];
	} else {
		printHelp(argv[0]);
		return -1;
	}

	rc = image.Initialize(firmwareName);
	if (rc != UPDATE_SUCCESS) {
		fprintf(stderr, "Failed to initialize the firmware image: %s\n", update_err_to_string(rc));
		return 1;
	}

	if (deviceName) {
		char * rawDevice;
		rc = UpdateDevice(image, force, deviceName);
		if (rc)
			return rc;

		rawDevice = strcasestr(deviceName, "hidraw");
		if (rawDevice)
			RebindDriver(rawDevice);
		return rc;
	} else {
		char rawDevice[PATH_MAX];
		char deviceFile[PATH_MAX];
		bool found = false;

		devDir = opendir("/dev");
		if (!devDir)
			return -1;

		while ((devDirEntry = readdir(devDir)) != NULL) {
			if (strstr(devDirEntry->d_name, "hidraw")) {
				strncpy(rawDevice, devDirEntry->d_name, PATH_MAX);
				snprintf(deviceFile, PATH_MAX, "/dev/%s", devDirEntry->d_name);
				rc = UpdateDevice(image, force, deviceFile);
				if (rc != 0) {
					continue;
				} else {
					found = true;
					RebindDriver(rawDevice);
					break;
				}
			}
		}
		closedir(devDir);

		if (!found)
			return rc;
	}

	return 0;
}
