/*
 * Copyright (C) 2014 - 2016 Andrew Duggan
 * Copyright (C) 2014 - 2016 Synaptics Inc
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
#include <signal.h>
#include <stdlib.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <alloca.h>

#include "i2cdevice.h"

int I2CDevice::SetRMIPage(unsigned char page)
{
	int rc;

	if (m_page == page)
		return 0;

	m_page = page;
	rc = _Write(RMI_DEVICE_PAGE_SELECT_REGISTER, &page, 1);
	if (rc < 0 || rc < 1) {
		m_page = -1;
		return rc;
	}
	return 0;
}

int I2CDevice::Open(const char * filename)
{
	int rc;

	if (!filename)
		return -EINVAL;

	m_fd = open(filename, O_RDWR);
	if (m_fd < 0)
		return -1;

	rc = ioctl(m_fd, I2C_SLAVE, m_deviceAddress);
	if (rc < 0) {
		fprintf(stderr, "Failed to set I2C slave address (0x%02x): %s\n", m_deviceAddress,
			strerror(errno));
		return -1;
	}

	return 0;
}

int I2CDevice::Read(unsigned short addr, unsigned char *buf, unsigned short len)
{
	int rc;
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg messages[2];
	unsigned char output_buf;

	rc = SetRMIPage(addr >> 8);
	if (rc < 0) {
		fprintf(stderr, "Failed to SetRMIPage: %s\n", strerror(errno));
		return -1;
	}

	output_buf = addr & 0xff;
	messages[0].addr = m_deviceAddress;
	messages[0].flags = 0;
	messages[0].len = sizeof(output_buf);
	messages[0].buf = &output_buf;

	messages[1].addr = m_deviceAddress;
	messages[1].flags = I2C_M_RD;
	messages[1].len = len;
	messages[1].buf = buf;

	packets.msgs = messages;
	packets.nmsgs = 2;

	rc = ioctl(m_fd, I2C_RDWR, &packets);
	if (rc < 0) {
		fprintf(stderr, "Failed to do I2C rdwr transfer: %s\n", strerror(errno));
		return -1;
	}

	return len;
}

int I2CDevice::_Write(unsigned short addr, const unsigned char *buf, unsigned short len)
{
	int rc;
	char *data = (char *)alloca(len + 1);

	data[0] = addr & 0xff;
	memcpy(&data[1], buf, len);

	rc = write(m_fd, data, len + 1);
	if (rc < 0) {
		fprintf(stderr, "Failed to write I2C data: %s\n", strerror(errno));
		return -1;
	}
	return len;
}

int I2CDevice::Write(unsigned short addr, const unsigned char *buf, unsigned short len)
{
	int rc;

	rc = SetRMIPage(addr >> 8);
	if (rc < 0) {
		fprintf(stderr, "Failed to SetRMIPage: %s\n", strerror(errno));
		return -1;
	}

	return _Write(addr, buf, len);
}

void I2CDevice::Close()
{
	close(m_fd);
	m_fd = -1;
}

int I2CDevice::WaitForAttention(struct timeval * timeout, unsigned int source_mask)
{
	Sleep(500);
	return 0;
}

void I2CDevice::PrintDeviceInfo()
{
	fprintf(stdout, "I2C device info:\nAddr: 0x%x\n", m_deviceAddress);
}