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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#include <signal.h>
#include <stdlib.h>

#include "hiddevice.h"

#define RMI_WRITE_REPORT_ID                 0x9 // Output Report
#define RMI_READ_ADDR_REPORT_ID             0xa // Output Report
#define RMI_READ_DATA_REPORT_ID             0xb // Input Report
#define RMI_ATTN_REPORT_ID                  0xc // Input Report
#define RMI_SET_RMI_MODE_REPORT_ID          0xf // Feature Report

enum rmi_hid_mode_type {
	HID_RMI4_MODE_MOUSE                     = 0,
	HID_RMI4_MODE_ATTN_REPORTS              = 1,
	HID_RMI4_MODE_NO_PACKED_ATTN_REPORTS    = 2,
};

#define HID_RMI4_REPORT_ID			0
#define HID_RMI4_READ_INPUT_COUNT		1
#define HID_RMI4_READ_INPUT_DATA		2
#define HID_RMI4_READ_OUTPUT_ADDR		2
#define HID_RMI4_READ_OUTPUT_COUNT		4
#define HID_RMI4_WRITE_OUTPUT_COUNT		1
#define HID_RMI4_WRITE_OUTPUT_ADDR		2
#define HID_RMI4_WRITE_OUTPUT_DATA		4
#define HID_RMI4_FEATURE_MODE			1
#define HID_RMI4_ATTN_INTERUPT_SOURCES		1
#define HID_RMI4_ATTN_DATA			2

int HIDDevice::Open(const char * filename)
{
	int rc;
	int desc_size;

	if (!filename)
		return -EINVAL;

	m_fd = open(filename, O_RDWR);
	if (m_fd < 0)
		return -1;

	memset(&m_rptDesc, 0, sizeof(m_rptDesc));
	memset(&m_info, 0, sizeof(m_info));

	rc = ioctl(m_fd, HIDIOCGRDESCSIZE, &desc_size);
	if (rc < 0)
		return rc;
	
	m_rptDesc.size = desc_size;
	rc = ioctl(m_fd, HIDIOCGRDESC, &m_rptDesc);
	if (rc < 0)
		return rc;
	
	rc = ioctl(m_fd, HIDIOCGRAWINFO, &m_info);
	if (rc < 0)
		return rc;

	/* FIXME: get there from the report descriptor */
	m_inputReportSize = 30;
	m_outputReportSize = 25;
	m_featureReportSize = 2;

	m_inputReport = new unsigned char[m_inputReportSize]();
	if (!m_inputReport)
		return -ENOMEM;

	m_outputReport = new unsigned char[m_outputReportSize]();
	if (!m_outputReport)
		return -ENOMEM;

	m_readData = new unsigned char[m_inputReportSize]();
	if (!m_readData)
		return -ENOMEM;

	m_attnReportQueue = new unsigned char[m_inputReportSize * HID_REPORT_QUEUE_MAX_SIZE]();
	if (!m_attnReportQueue)
		return -ENOMEM;

	m_deviceOpen = true;

	rc = SetMode(HID_RMI4_MODE_ATTN_REPORTS);
	if (rc)
		return -1;

	return 0;
}

int HIDDevice::Read(unsigned short addr, unsigned char *buf, unsigned short len)
{
	size_t count;
	size_t bytesReadPerRequest;
	size_t bytesInDataReport;
	size_t totalBytesRead;
	size_t bytesPerRequest;
	size_t bytesWritten;
	size_t bytesToRequest;
	int rc;

	if (!m_deviceOpen)
		return -1;

	if (m_bytesPerReadRequest)
		bytesPerRequest = m_bytesPerReadRequest;
	else
		bytesPerRequest = len;

	for (totalBytesRead = 0; totalBytesRead < len; totalBytesRead += bytesReadPerRequest) {
		count = 0;
		if ((len - totalBytesRead) < bytesPerRequest)
			bytesToRequest = len % bytesPerRequest;
		else
			bytesToRequest = bytesPerRequest;

		m_outputReport[HID_RMI4_REPORT_ID] = RMI_READ_ADDR_REPORT_ID;
		m_outputReport[1] = 0; /* old 1 byte read count */
		m_outputReport[HID_RMI4_READ_OUTPUT_ADDR] = addr & 0xFF;
		m_outputReport[HID_RMI4_READ_OUTPUT_ADDR + 1] = (addr >> 8) & 0xFF;
		m_outputReport[HID_RMI4_READ_OUTPUT_COUNT] = bytesToRequest  & 0xFF;
		m_outputReport[HID_RMI4_READ_OUTPUT_COUNT + 1] = (bytesToRequest >> 8) & 0xFF;

		m_dataBytesRead = 0;

		for (bytesWritten = 0; bytesWritten < m_outputReportSize; bytesWritten += count) {
			m_bCancel = false;
			count = write(m_fd, m_outputReport + bytesWritten,
					m_outputReportSize - bytesWritten);
			if (count < 0) {
				if (errno == EINTR && m_deviceOpen && !m_bCancel)
					continue;
				else
					return count;
			}
			break;
		}

		bytesReadPerRequest = 0;
		while (bytesReadPerRequest < bytesToRequest) {
			rc = GetReport(RMI_READ_DATA_REPORT_ID);
			if (rc > 0) {
				bytesInDataReport = m_readData[HID_RMI4_READ_INPUT_COUNT];
				memcpy(buf + bytesReadPerRequest, &m_readData[HID_RMI4_READ_INPUT_DATA],
					bytesInDataReport);
				bytesReadPerRequest += bytesInDataReport;
				m_dataBytesRead = 0;
			}
		}
		addr += bytesPerRequest;
	}

	return totalBytesRead;
}

int HIDDevice::Write(unsigned short addr, const unsigned char *buf, unsigned short len)
{
	size_t count;

	if (!m_deviceOpen)
		return -1;

	m_outputReport[HID_RMI4_REPORT_ID] = RMI_WRITE_REPORT_ID;
	m_outputReport[HID_RMI4_WRITE_OUTPUT_COUNT] = len;
	m_outputReport[HID_RMI4_WRITE_OUTPUT_ADDR] = addr & 0xFF;
	m_outputReport[HID_RMI4_WRITE_OUTPUT_ADDR + 1] = (addr >> 8) & 0xFF;
	memcpy(&m_outputReport[HID_RMI4_WRITE_OUTPUT_DATA], buf, len);

	for (;;) {
		m_bCancel = false;
		count = write(m_fd, m_outputReport, m_outputReportSize);
		if (count < 0) {
			if (errno == EINTR && m_deviceOpen && !m_bCancel)
				continue;
			else
				return count;
		}
		return count;
	}
}

int HIDDevice::SetMode(int mode)
{
	int rc;
	char buf[2];

	if (!m_deviceOpen)
		return -1;

	buf[0] = 0xF;
	buf[1] = mode;
	rc = ioctl(m_fd, HIDIOCSFEATURE(2), buf);
	if (rc < 0) {
		perror("HIDIOCSFEATURE");
		return rc;
	}

	return 0;
}

void HIDDevice::Close()
{
	if (!m_deviceOpen)
		return;

	SetMode(HID_RMI4_MODE_MOUSE);
	m_deviceOpen = false;
	close(m_fd);
	m_fd = -1;

	delete[] m_inputReport;
	m_inputReport = NULL;
	delete[] m_outputReport;
	m_outputReport = NULL;
	delete[] m_readData;
	m_readData = NULL;
	delete[] m_attnReportQueue;
	m_attnReportQueue = NULL;
}

int HIDDevice::WaitForAttention(struct timeval * timeout, int *sources)
{
	return GetAttentionReport(timeout, sources, NULL, NULL);
}

int HIDDevice::GetAttentionReport(struct timeval * timeout, int *sources, unsigned char *buf, unsigned int *len)
{
	int rc;
	int interrupt_sources;
	const unsigned char * queue_report;
	int bytes = m_inputReportSize;

	if (len && m_inputReportSize < *len) {
		bytes = *len;
		*len = m_inputReportSize;
	}

	rc = GetReport(RMI_ATTN_REPORT_ID, timeout);
	if (rc > 0) {
		queue_report = m_attnReportQueue
					+ m_inputReportSize * m_tailIdx;
		interrupt_sources = queue_report[HID_RMI4_ATTN_INTERUPT_SOURCES];
		if (buf)
			memcpy(buf, queue_report, bytes);
		m_tailIdx = (m_tailIdx + 1) & (HID_REPORT_QUEUE_MAX_SIZE - 1);
		--m_attnQueueCount;
		if (sources)
			*sources = interrupt_sources;
		return rc;
	}

	return rc;
}

int HIDDevice::GetReport(int reportid, struct timeval * timeout)
{
	size_t count = 0;
	unsigned char *queue_report;
	fd_set fds;
	int rc;
	int report_count = 0;

	if (!m_deviceOpen)
		return -1;

	for (;;) {
		FD_ZERO(&fds);
		FD_SET(m_fd, &fds);

		rc = select(m_fd + 1, &fds, NULL, NULL, timeout);
		if (rc == 0) {
			return -ETIMEDOUT;
		} else if (rc < 0) {
			if (errno == EINTR && m_deviceOpen && !m_bCancel)
				continue;
			else
				return rc;
		} else if (rc > 0 && FD_ISSET(m_fd, &fds)) {
			size_t offset = 0;
			for (;;) {
				m_bCancel = false;
				count = read(m_fd, m_inputReport + offset, m_inputReportSize - offset);
				if (count < 0) {
					if (errno == EINTR && m_deviceOpen && !m_bCancel)
						continue;
					else
						return count;
				}
				offset += count;
				if (offset == m_inputReportSize)
					break;
			}
		}

		if (m_inputReport[HID_RMI4_REPORT_ID] == RMI_ATTN_REPORT_ID) {
			queue_report = m_attnReportQueue
					+ m_inputReportSize * m_headIdx;
			memcpy(queue_report, m_inputReport, count);
			m_headIdx = (m_headIdx + 1) & (HID_REPORT_QUEUE_MAX_SIZE - 1);
			++m_attnQueueCount;
		} else if (m_inputReport[HID_RMI4_REPORT_ID] == RMI_READ_DATA_REPORT_ID) {
			memcpy(m_readData, m_inputReport, count);
			m_dataBytesRead = count;
		}
		++report_count;

		if (m_inputReport[HID_RMI4_REPORT_ID] == reportid)
			break;
	}
	return report_count;
}

void HIDDevice::PrintReport(const unsigned char *report)
{
	int i;
	int len = 0;
	const unsigned char * data;
	int addr = 0;

	switch (report[HID_RMI4_REPORT_ID]) {
		case RMI_WRITE_REPORT_ID:
			len = report[HID_RMI4_WRITE_OUTPUT_COUNT];
			data = &report[HID_RMI4_WRITE_OUTPUT_DATA];
			addr = (report[HID_RMI4_WRITE_OUTPUT_ADDR] & 0xFF)
				| ((report[HID_RMI4_WRITE_OUTPUT_ADDR + 1] & 0xFF) << 8);
			fprintf(stdout, "Write Report:\n");
			fprintf(stdout, "Address = 0x%02X\n", addr);
			fprintf(stdout, "Length = 0x%02X\n", len);
			break;
		case RMI_READ_ADDR_REPORT_ID:
			addr = (report[HID_RMI4_READ_OUTPUT_ADDR] & 0xFF)
				| ((report[HID_RMI4_READ_OUTPUT_ADDR + 1] & 0xFF) << 8);
			len = (report[HID_RMI4_READ_OUTPUT_COUNT] & 0xFF)
				| ((report[HID_RMI4_READ_OUTPUT_COUNT + 1] & 0xFF) << 8);
			fprintf(stdout, "Read Request (Output Report):\n");
			fprintf(stdout, "Address = 0x%02X\n", addr);
			fprintf(stdout, "Length = 0x%02X\n", len);
			return;
			break;
		case RMI_READ_DATA_REPORT_ID:
			len = report[HID_RMI4_READ_INPUT_COUNT];
			data = &report[HID_RMI4_READ_INPUT_DATA];
			fprintf(stdout, "Read Data Report:\n");
			fprintf(stdout, "Length = 0x%02X\n", len);
			break;
		case RMI_ATTN_REPORT_ID:
			fprintf(stdout, "Attention Report:\n");
			len = 28;
			data = &report[HID_RMI4_ATTN_DATA];
			fprintf(stdout, "Interrupt Sources: 0x%02X\n", 
				report[HID_RMI4_ATTN_INTERUPT_SOURCES]);
			break;
		default:
			fprintf(stderr, "Unknown Report: ID 0x%02x\n", report[HID_RMI4_REPORT_ID]);
			return;
	}

	fprintf(stdout, "Data:\n");
	for (i = 0; i < len; ++i) {
		fprintf(stdout, "0x%02X ", data[i]);
		if (i % 8 == 7) {
			fprintf(stdout, "\n");
		}
	}
	fprintf(stdout, "\n\n");
}