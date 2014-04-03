//===========================================================================
// Copyright (c) 2012 - 2014 Synaptics Incorporated. All rights reserved.
//===========================================================================

#ifndef _RMI4UPDATE_H_
#define _RMI4UPDATE_H_

#include "rmidevice.h"
#include "firmware_image.h"

#define RMI_BOOTLOADER_ID_SIZE		2

class RMI4Update
{
public:
	RMI4Update(RMIDevice & device, FirmwareImage & firmwareImage) : m_device(device), 
			m_firmwareImage(firmwareImage)
	{}
	int UpdateFirmware(bool force = false);

private:
	int FindUpdateFunctions();
	int ReadF34Queries();
	int ReadF34Controls();
	int WriteBootloaderID();
	int EnterFlashProgramming();
	int WriteBlocks(unsigned char *block, unsigned short count, unsigned char cmd);
	int WaitForIdle(int timeout_ms);
	int GetFirmwareSize() { return m_blockSize * m_fwBlockCount; }
	int GetConfigSize() { return m_blockSize * m_configBlockCount; }

private:
	RMIDevice & m_device;
	FirmwareImage & m_firmwareImage;

	RMIFunction m_f01;
	RMIFunction m_f34;

	unsigned char m_deviceStatus;
	unsigned char m_bootloaderID[RMI_BOOTLOADER_ID_SIZE];

	/* F34 Controls */
	unsigned char m_f34Command;
	unsigned char m_f34Status;
	bool m_programEnabled;

	/* F34 Query */
	bool m_hasNewRegmap;
	bool m_unlocked;
	bool m_hasConfigID;
	unsigned short m_blockSize;
	unsigned short m_fwBlockCount;
	unsigned short m_configBlockCount;

	unsigned short m_f34StatusAddr;
};

#endif // _RMI4UPDATE_H_
