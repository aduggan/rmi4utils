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

#ifndef _I2CDEVICE_H_
#define _I2CDEVICE_H_

#include "rmidevice.h"

class I2CDevice : public RMIDevice
{
public:
	I2CDevice() : RMIDevice(), m_deviceOpen(false)
	{}
	virtual int Open(const char * devicename);
	virtual int Read(unsigned short addr, unsigned char *buf,
				unsigned short len);
	virtual int Write(unsigned short addr, const unsigned char *buf,
				 unsigned short len);
	virtual int SetMode(int mode) { return 0; }
	virtual int SetRMIPage(unsigned char page);
	virtual int WaitForAttention(int timeout_ms = 0,
					unsigned int source_mask = RMI_INTERUPT_SOURCES_ALL_MASK);
	virtual int GetAttentionData(int timeout_ms, unsigned int source_mask,
					unsigned char *buf, unsigned int *len);
	virtual void Close();
	virtual void RebindDriver() {}
	~I2CDevice() { Close(); }

	virtual void PrintDeviceInfo();

private:
	int m_fd;
	bool m_deviceOpen;
	int m_deviceAddress;
	int m_deviceBus;
	std::string m_deviceName;
	std::string m_driverName;

	int _Write(unsigned short addr, const unsigned char *buf,
				 unsigned short len);

	bool UnbindDriver();
	bool BindDriver();

	static bool ParseDeviceName(const char * devicename, int * bus, int * addr, char * filename);
 };

#endif /* _I2CDEVICE_H_ */