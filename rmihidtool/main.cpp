#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <getopt.h>

#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#include <signal.h>
#include <stdlib.h>

#include "hiddevice.h"

#define RMI4UPDATE_GETOPTS      "hp:"

static int report_attn = 0;
static RMIDevice * g_device = NULL;

void printArgsHelp(const char *prog_name)
{
	fprintf(stdout, "Usage: %s [OPTIONS] DEVICEFILE FIRMWAREFILE\n", prog_name);
	fprintf(stdout, "\t-h, --help\tPrint this message\n");
	fprintf(stdout, "\t-p, --protocol [protocol]\tSet which transport prototocl to use.\n");
}

void print_help()
{
	fprintf(stdout, "Commands:\n");
	fprintf(stdout, "s [0,1,2]: Set RMIMode\n");
	fprintf(stdout, "r address size: read size bytes from address\n");
	fprintf(stdout, "w address { values }: write bytes to address\n");
	fprintf(stdout, "a: Wait for attention\n");
	fprintf(stdout, "q: quit\n");
}

int find_token(char * input, char * result, char ** endpp)
{
	int i = 0;
	char * start = input;
	char * end;

	while (input[i] == ' ') {
		++start;
		++i;
	}

	while (input[i] != '\0') {
		++i;
		if (input[i] == ' ') {
			break;
		}
	}
	end = &input[i];

	if (start == end) {
		return 0;
	}

	*endpp = end;
	strncpy(result, start, end - start);
	result[end - start + 1] = '\0';

	return 1;
}

int process(RMIDevice * device, char * input)
{
	unsigned char report[256];
	char token[256];
	char * start;
	char * end;
	int rc;

	memset(token, 0, 256);

	if (input[0] == 's') {
		start = input + 2;
		find_token(start, token, &end);
		int mode = strtol(token, NULL, 0);
		if (mode >= 0 && mode <= 2) {
			if (device->SetMode(mode)) {
				fprintf(stderr, "Set RMI Mode to: %d\n", mode);
			} else {
				fprintf(stderr, "Set RMI Mode FAILED!\n");
				return -1;
			}
		}
	} else if (input[0] == 'r') {
		start = input + 2;
		find_token(start, token, &end);
		start = end + 1;
		int addr = strtol(token, NULL, 0);
		find_token(start, token, &end);
		start = end + 1;
		int len = strtol(token, NULL, 0);
		fprintf(stdout, "Address = 0x%02x Length = %d\n", addr, len);

		memset(report, 0, sizeof(report));
		rc = device->Read(addr, report, len);
		if (rc < 0)
			fprintf(stderr, "Failed to read report: %d\n", rc);
		
		fprintf(stdout, "Data:\n");
		for (int i = 0; i < len; ++i) {
			fprintf(stdout, "0x%02X ", report[i]);
			if (i % 8 == 7) {
				fprintf(stdout, "\n");
			}
		}
		fprintf(stdout, "\n");
	} else if (input[0] == 'w') {
		int index = 0;
		start = input + 2;
		find_token(start, token, &end);
		start = end + 1;
		int addr = strtol(token, NULL, 0);
		int len = 0;

		while (find_token(start, token, &end)) {
			start = end + 1;
			report[index++] = (unsigned char)strtol(token, NULL, 0);
			++len;
		}

		memset(report, 0, sizeof(report));

		if (device->Write(addr, report, len) < 0) {
			fprintf(stderr, "Failed to Write Report\n");
			return -1;
		}
	} else if (input[0] == 'a') {
		report_attn = 1;
		while(report_attn) {
			int bytes = 256;
			device->GetAttentionReport(NULL, NULL, report, &bytes);
			fprintf(stdout, "Data:\n");
			for (int i = 0; i < bytes; ++i) {
				fprintf(stdout, "0x%02X ", report[i]);
				if (i % 8 == 7) {
					fprintf(stdout, "\n");
				}
			}
			fprintf(stdout, "\n\n");
		}
	} else if (input[0] == 'q') {
		return 1;
	} else {
		print_help();
	}
	return 0;
}

static void cleanup(int status)
{
	if (report_attn) {
		report_attn = 0;
		if (g_device)
			g_device->Cancel();
	} else {
		exit(0);
	}
}

int main(int argc, char ** argv)
{
	int rc;
	struct sigaction sig_cleanup_action;
	int opt;
	int index;
	RMIDevice *device;
	const char *protocol = "HID";
	static struct option long_options[] = {
		{"help", 0, NULL, 'h'},
		{"protocol", 1, NULL, 'p'},
		{0, 0, 0, 0},
	};

	memset(&sig_cleanup_action, 0, sizeof(struct sigaction));
        sig_cleanup_action.sa_handler = cleanup;
        sig_cleanup_action.sa_flags = SA_RESTART;
        sigaction(SIGINT, &sig_cleanup_action, NULL);

        while ((opt = getopt_long(argc, argv, RMI4UPDATE_GETOPTS, long_options, &index)) != -1) {
                switch (opt) {
                        case 'h':
                                printArgsHelp(argv[0]);
                                return 0;
                        case 'p':
                                protocol = optarg;
                                break;
                        default:
                                break;

                }
        }

	if (!strncasecmp("hid", protocol, 3)) {
		device = new HIDDevice();
        } else {
		fprintf(stderr, "Invalid Protocol: %s\n", protocol);
		return -1;
        }

	rc = device->Open(argv[optind++]);
	if (rc) {
		fprintf(stderr, "%s: failed to initialize rmi device (%d): %s\n", argv[0], errno,
			strerror(errno));
		return 1;
	}

	device->ScanPDT();
	device->QueryBasicProperties();
	device->PrintProperties();

	g_device = device;

	fprintf(stdout, "\n");
	for (;;) {
		fprintf(stdout, "\n");
		print_help();
		char buf[256];

		if (fgets(buf, 256, stdin)) {
			if (process(device, buf) == 1)
				break;
		}
	}

	device->Close();

	return 0;
}