#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include "hiddevice.h"
#include "rmi4update.h"

#define RMI4UPDATE_GETOPTS	"hf"

void printHelp(const char *prog_name)
{
	fprintf(stdout, "Usage: %s [OPTIONS] DEVICEFILE FIRMWAREFILE\n", prog_name);
	fprintf(stdout, "\t-h, --help\tPrint this message\n");
	fprintf(stdout, "\t-f, --force\tForce updating firmware even it the image provided is older\n\t\t\tthen the current firmware on the device.\n");
}

int main(int argc, char **argv)
{
	int rc;
	HIDDevice rmidevice;
	FirmwareImage image;
	int opt;
	int index;
	const char *deviceName;
	const char *firmwareName;
	bool force = false;
	static struct option long_options[] = {
		{"help", 0, NULL, 'h'},
		{"force", 0, NULL, 'f'},
		{0, 0, 0, 0},
	};

	while ((opt = getopt_long(argc, argv, RMI4UPDATE_GETOPTS, long_options, &index)) != -1) {
		switch (opt) {
			case 'h':
				printHelp(argv[0]);
				return 0;
			case 'f':
				force = true;
				break;
			default:
				break;

		}
	}

	if (optind < argc)
		deviceName = argv[optind++];

	if (optind < argc)
		firmwareName = argv[optind];

	rc = rmidevice.Open(deviceName);
	if (rc) {
		fprintf(stderr, "Failed to open rmi device: %s\n", strerror(errno));
		return rc;
	}

	rc = image.Initialize(firmwareName);
	if (rc != UPDATE_SUCCESS) {
		fprintf(stderr, "Failed to initialize the firmware image: %s\n", update_err_to_string(rc));
		return 1;
	}



	RMI4Update update(rmidevice, image);
	rc = update.UpdateFirmware(force);
	if (rc != UPDATE_SUCCESS) {
		fprintf(stderr, "Firmware update failed because: %s\n", update_err_to_string(rc));
		return 1;
	}

	return 0;
}
