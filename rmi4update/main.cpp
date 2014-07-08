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
#include <dirent.h>

#include "hiddevice.h"
#include "rmi4update.h"

#define RMI4UPDATE_GETOPTS	"hfd:"

void printHelp(const char *prog_name)
{
	fprintf(stdout, "Usage: %s [OPTIONS] FIRMWAREFILE\n", prog_name);
	fprintf(stdout, "\t-h, --help\tPrint this message\n");
	fprintf(stdout, "\t-f, --force\tForce updating firmware even it the image provided is older\n\t\t\tthen the current firmware on the device.\n");
	fprintf(stdout, "\t-d, --device\thidraw device file associated with the device being updated.\n");
}

int UpdateDevice(FirmwareImage & image, bool force, const char * deviceFile) {
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

int main(int argc, char **argv)
{
	int rc;
	FirmwareImage image;
	int opt;
	int index;
	const char *deviceName = NULL;
	const char *firmwareName = NULL;
	bool force = false;
	static struct option long_options[] = {
		{"help", 0, NULL, 'h'},
		{"force", 0, NULL, 'f'},
		{"device", 1, NULL, 'd'},
		{0, 0, 0, 0},
	};
	struct dirent * devDirEntry;
	DIR * devDir;

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
			default:
				break;

		}
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
		return UpdateDevice(image, force, deviceName);
	} else {
		char buf[PATH_MAX];
		bool found = false;

		devDir = opendir("/dev");
		if (!devDir)
			return -1;

		while ((devDirEntry = readdir(devDir)) != NULL) {
			if (strstr(devDirEntry->d_name, "hidraw")) {
				snprintf(buf, PATH_MAX, "/dev/%s", devDirEntry->d_name);
				rc = UpdateDevice(image, force, buf);
				if (rc != 0) {
					continue;
				} else {
					found = true;
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
