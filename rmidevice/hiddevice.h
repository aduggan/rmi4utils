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

#ifndef _HIDDEVICE_H_
#define _HIDDEVICE_H_

#include <linux/hidraw.h>
#include "rmidevice.h"

class HIDDevice : public RMIDevice
{
public:
	HIDDevice(int bytesPerReadRequest = 0) : RMIDevice(bytesPerReadRequest), m_headIdx(0),
			m_tailIdx(0), m_deviceOpen(false), m_attnQueueCount(0)
	{}
	virtual int Open(const char * filename);
	virtual int Read(unsigned short addr, unsigned char *buf,
				unsigned short len);
	virtual int Write(unsigned short addr, const unsigned char *buf,
				 unsigned short len);
	virtual int SetMode(int mode);
	virtual int WaitForAttention(struct timeval * timeout = NULL, int *sources = NULL);
	virtual int GetAttentionReport(struct timeval * timeout, int *sources, unsigned char *buf,
					unsigned int *len);
	virtual void Close();
	~HIDDevice() { Close(); }

private:
	int m_fd;

	struct hidraw_report_descriptor m_rptDesc;
	struct hidraw_devinfo m_info;

	unsigned char *m_inputReport;
	unsigned char *m_outputReport;

	unsigned char *m_attnReportQueue;
	int m_headIdx;
	int m_tailIdx;
	unsigned char *m_readData;
	int m_dataBytesRead;

	size_t m_inputReportSize;
	size_t m_outputReportSize;
	size_t m_featureReportSize;

	bool m_deviceOpen;

	int m_attnQueueCount;

	enum mode_type {
		HID_RMI4_MODE_MOUSE			= 0,
		HID_RMI4_MODE_ATTN_REPORTS		= 1,
		HID_RMI4_MODE_NO_PACKED_ATTN_REPORTS	= 2,
	};

	int GetReport(int reportid, struct timeval * timeout = NULL);
	void PrintReport(const unsigned char *report);
	void ParseReportSizes();

	static const int HID_REPORT_QUEUE_MAX_SIZE = 8;
 };

#endif /* _HIDDEVICE_H_ */