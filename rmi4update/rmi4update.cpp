#include <alloca.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "rmi4update.h"

#define RMI_F34_QUERY_SIZE		7
#define RMI_F34_HAS_NEW_REG_MAP		(1 << 0)
#define RMI_F34_IS_UNLOCKED		(1 << 1)
#define RMI_F34_HAS_CONFIG_ID		(1 << 2)
#define RMI_F34_BLOCK_SIZE_OFFSET	1
#define RMI_F34_FW_BLOCKS_OFFSET	3
#define RMI_F34_CONFIG_BLOCKS_OFFSET	5

#define RMI_F34_BLOCK_DATA_OFFSET	2

#define RMI_F34_COMMAND_MASK		0x0F
#define RMI_F34_STATUS_MASK		0x07
#define RMI_F34_STATUS_SHIFT		4
#define RMI_F34_ENABLED_MASK		0x80

#define RMI_F34_WRITE_FW_BLOCK        0x02
#define RMI_F34_ERASE_ALL             0x03
#define RMI_F34_WRITE_LOCKDOWN_BLOCK  0x04
#define RMI_F34_WRITE_CONFIG_BLOCK    0x06
#define RMI_F34_ENABLE_FLASH_PROG     0x0f

#define RMI_F34_ENABLE_WAIT_MS 300
#define RMI_F34_ERASE_WAIT_MS (5 * 1000)
#define RMI_F34_IDLE_WAIT_MS 500

/* Most recent device status event */
#define RMI_F01_STATUS_CODE(status)		((status) & 0x0f)
/* Indicates that flash programming is enabled (bootloader mode). */
#define RMI_F01_STATUS_BOOTLOADER(status)	(!!((status) & 0x40))
/* The device has lost its configuration for some reason. */
#define RMI_F01_STATUS_UNCONFIGURED(status)	(!!((status) & 0x80))

/*
 * Sleep mode controls power management on the device and affects all
 * functions of the device.
 */
#define RMI_F01_CTRL0_SLEEP_MODE_MASK	0x03

#define RMI_SLEEP_MODE_NORMAL		0x00
#define RMI_SLEEP_MODE_SENSOR_SLEEP	0x01
#define RMI_SLEEP_MODE_RESERVED0	0x02
#define RMI_SLEEP_MODE_RESERVED1	0x03

/*
 * This bit disables whatever sleep mode may be selected by the sleep_mode
 * field and forces the device to run at full power without sleeping.
 */
#define RMI_F01_CRTL0_NOSLEEP_BIT	(1 << 2)

int RMI4Update::UpdateFirmware(bool force)
{
	struct timespec start;
	struct timespec end;
	int64_t duration_ns = 0;
	int rc;
	const unsigned char eraseAll = RMI_F34_ERASE_ALL;

	rc = FindUpdateFunctions();
	if (rc != UPDATE_SUCCESS)
		return rc;

	rc = m_device.QueryBasicProperties();
	if (rc < 0)
		return UPDATE_FAIL_QUERY_BASIC_PROPERTIES;

	fprintf(stdout, "Device Properties:\n");
	m_device.PrintProperties();

	rc = ReadF34Queries();
	if (rc != UPDATE_SUCCESS)
		return rc;

	rc = m_firmwareImage.VerifyImage(GetFirmwareSize(), GetConfigSize());
	if (rc != UPDATE_SUCCESS)
		return rc;

	rc = EnterFlashProgramming();
	if (rc != UPDATE_SUCCESS) {
		fprintf(stderr, "%s: %s\n", __func__, update_err_to_string(rc));
		return rc;
	}

	if (!force && m_firmwareImage.HasIO()) {
		if (m_firmwareImage.GetFirmwareID() <= m_device.GetFirmwareID()) {
			m_device.Reset();
			return UPDATE_FAIL_FIRMWARE_IMAGE_IS_OLDER;
		}
	}

	if (m_unlocked) {
		if (m_firmwareImage.GetLockdownData()) {
			fprintf(stdout, "Writing lockdown...\n");
			clock_gettime(CLOCK_MONOTONIC, &start);
			rc = WriteBlocks(m_firmwareImage.GetLockdownData(),
					m_firmwareImage.GetLockdownSize() / 0x10,
					RMI_F34_WRITE_LOCKDOWN_BLOCK);
			if (rc != UPDATE_SUCCESS) {
				fprintf(stderr, "%s: %s\n", __func__, update_err_to_string(rc));
				return rc;
			}
			clock_gettime(CLOCK_MONOTONIC, &end);
#if 0 // TODO: convert to userspace
			duration_ns = timespec_to_ns(&end) - timespec_to_ns(&start);
#endif
			fprintf(stdout, "Done writing lockdown, time: %ld ns.\n", duration_ns);
		}

		rc = EnterFlashProgramming();
		if (rc != UPDATE_SUCCESS) {
			fprintf(stderr, "%s: %s\n", __func__, update_err_to_string(rc));
			return rc;
		}
		
	}

	rc = WriteBootloaderID();
	if (rc != UPDATE_SUCCESS) {
		fprintf(stderr, "%s: %s\n", __func__, update_err_to_string(rc));
		return rc;
	}

	fprintf(stdout, "Erasing FW...\n");
	clock_gettime(CLOCK_MONOTONIC, &start);
	rc = m_device.Write(m_f34StatusAddr, &eraseAll, 1);
	if (rc < 0) {
		fprintf(stderr, "%s: %s\n", __func__, update_err_to_string(UPDATE_FAIL_ERASE_ALL));
		return UPDATE_FAIL_ERASE_ALL;
	}

	rc = WaitForIdle(RMI_F34_ERASE_WAIT_MS);
	if (rc != UPDATE_SUCCESS) {
		fprintf(stderr, "%s: %s\n", __func__, update_err_to_string(rc));
		return rc;
	}
	clock_gettime(CLOCK_MONOTONIC, &end);
#if 0 // TODO: convert to userspace
	duration_ns = timespec_to_ns(&end) - timespec_to_ns(&start);
#endif
	fprintf(stdout, "Erase complete, time: %ld ns.\n", duration_ns);

	if (m_firmwareImage.GetFirmwareData()) {
		fprintf(stdout, "Writing firmware...\n");
		clock_gettime(CLOCK_MONOTONIC, &start);
		rc = WriteBlocks(m_firmwareImage.GetFirmwareData(), m_fwBlockCount,
						RMI_F34_WRITE_FW_BLOCK);
		if (rc != UPDATE_SUCCESS) {
			fprintf(stderr, "%s: %s\n", __func__, update_err_to_string(rc));
			return rc;
		}
		clock_gettime(CLOCK_MONOTONIC, &end);
#if 0 // TODO: convert to userspace
		duration_ns = timespec_to_ns(&end) - timespec_to_ns(&start);
#endif
		fprintf(stdout, "Done writing FW, time: %ld ns.\n", duration_ns);
	}

	if (m_firmwareImage.GetConfigData()) {
		fprintf(stdout, "Writing configuration...\n");
		clock_gettime(CLOCK_MONOTONIC, &start);
		rc = WriteBlocks(m_firmwareImage.GetConfigData(), m_configBlockCount,
				RMI_F34_WRITE_CONFIG_BLOCK);
		if (rc != UPDATE_SUCCESS) {
			fprintf(stderr, "%s: %s\n", __func__, update_err_to_string(rc));
			return rc;
		}
		clock_gettime(CLOCK_MONOTONIC, &end);
#if 0 // TODO: convert to userspace
		duration_ns = timespec_to_ns(&end) - timespec_to_ns(&start);
#endif
		fprintf(stdout, "Done writing config, time: %ld ns.\n", duration_ns);
	}
	m_device.Reset();

	return UPDATE_SUCCESS;

}

int RMI4Update::FindUpdateFunctions()
{
	if (0 > m_device.ScanPDT())
		return UPDATE_FAIL_SCAN_PDT;

	if (!m_device.GetFunction(m_f01, 0x01))
		return UPDATE_FAIL_NO_FUNCTION_01;

	if (!m_device.GetFunction(m_f34, 0x34))
		return UPDATE_FAIL_NO_FUNCTION_34;

	return UPDATE_SUCCESS;
}

int RMI4Update::ReadF34Queries()
{
	int rc;
	unsigned char idStr[3];
	unsigned char buf[RMI_F34_QUERY_SIZE];
	unsigned short queryAddr = m_f34.GetQueryBase();

	rc = m_device.Read(queryAddr, m_bootloaderID, RMI_BOOTLOADER_ID_SIZE);
	if (rc < 0)
		return UPDATE_FAIL_READ_BOOTLOADER_ID;

	queryAddr += RMI_BOOTLOADER_ID_SIZE;

	rc = m_device.Read(queryAddr, buf, RMI_F34_QUERY_SIZE);
	if (rc < 0)
		return UPDATE_FAIL_READ_F34_QUERIES;

	m_hasNewRegmap = buf[0] & RMI_F34_HAS_NEW_REG_MAP;
	m_unlocked = buf[0] & RMI_F34_IS_UNLOCKED;;
	m_hasConfigID = buf[0] & RMI_F34_HAS_CONFIG_ID;
	m_blockSize = extract_short(buf + RMI_F34_BLOCK_SIZE_OFFSET);
	m_fwBlockCount = extract_short(buf + RMI_F34_FW_BLOCKS_OFFSET);
	m_configBlockCount = extract_short(buf + RMI_F34_CONFIG_BLOCKS_OFFSET);

	idStr[0] = m_bootloaderID[0];
	idStr[1] = m_bootloaderID[1];
	idStr[2] = 0;

	fprintf(stdout, "F34 bootloader id: %s (%#04x %#04x)\n", idStr, m_bootloaderID[0],
		m_bootloaderID[1]);
	fprintf(stdout, "F34 has config id: %d\n", m_hasConfigID);
	fprintf(stdout, "F34 unlocked:      %d\n", m_unlocked);
	fprintf(stdout, "F34 new reg map:   %d\n", m_hasNewRegmap);
	fprintf(stdout, "F34 block size:    %d\n", m_blockSize);
	fprintf(stdout, "F34 fw blocks:     %d\n", m_fwBlockCount);
	fprintf(stdout, "F34 config blocks: %d\n", m_configBlockCount);
	fprintf(stdout, "\n");

	m_f34StatusAddr = m_f34.GetDataBase() + RMI_F34_BLOCK_DATA_OFFSET + m_blockSize;

	return UPDATE_SUCCESS;
}

int RMI4Update::ReadF34Controls()
{
	int rc;
	unsigned char buf;

	rc = m_device.Read(m_f34StatusAddr, &buf, 1);
	if (rc < 0)
		return UPDATE_FAIL_READ_F34_CONTROLS;

	m_f34Command = buf & RMI_F34_COMMAND_MASK;
	m_f34Status = (buf >> RMI_F34_STATUS_SHIFT) & RMI_F34_STATUS_MASK;
	m_programEnabled = !!(buf & RMI_F34_ENABLED_MASK);
	
	return UPDATE_SUCCESS;
}

int RMI4Update::WriteBootloaderID()
{
	int rc;

	rc = m_device.Write(m_f34.GetDataBase() + RMI_F34_BLOCK_DATA_OFFSET,
				m_bootloaderID, RMI_BOOTLOADER_ID_SIZE);
	if (rc < 0)
		return UPDATE_FAIL_WRITE_BOOTLOADER_ID;

	return UPDATE_SUCCESS;
}

int RMI4Update::EnterFlashProgramming()
{
	int rc;
	unsigned char f01Control_0;
	const unsigned char enableProg = RMI_F34_ENABLE_FLASH_PROG;

	rc = WriteBootloaderID();
	if (rc != UPDATE_SUCCESS)
		return rc;

	fprintf(stdout, "Enabling flash programming.\n");
	rc = m_device.Write(m_f34StatusAddr, &enableProg, 1);
	if (rc < 0)
		return UPDATE_FAIL_ENABLE_FLASH_PROGRAMMING;

	rc = WaitForIdle(RMI_F34_ENABLE_WAIT_MS);
	if (rc != UPDATE_SUCCESS)
		return UPDATE_FAIL_NOT_IN_IDLE_STATE;

	if (!m_programEnabled)
		return UPDATE_FAIL_PROGRAMMING_NOT_ENABLED;

	fprintf(stdout, "HOORAY! Programming is enabled!\n");
	rc = FindUpdateFunctions();
	if (rc != UPDATE_SUCCESS)
		return rc;

	rc = m_device.Read(m_f01.GetDataBase(), &m_deviceStatus, 1);
	if (rc < 0)
		return UPDATE_FAIL_READ_DEVICE_STATUS;

	if (!RMI_F01_STATUS_BOOTLOADER(m_deviceStatus))
		return UPDATE_FAIL_DEVICE_NOT_IN_BOOTLOADER;

	rc = ReadF34Queries();
	if (rc != UPDATE_SUCCESS)
		return rc;

	rc = m_device.Read(m_f01.GetControlBase(), &f01Control_0, 1);
	if (rc < 0)
		return UPDATE_FAIL_READ_F01_CONTROL_0;

	f01Control_0 |= RMI_F01_CRTL0_NOSLEEP_BIT;
	f01Control_0 = (f01Control_0 & ~RMI_F01_CTRL0_SLEEP_MODE_MASK) | RMI_SLEEP_MODE_NORMAL;

	rc = m_device.Write(m_f01.GetControlBase(), &f01Control_0, 1);
	if (rc < 0)
		return UPDATE_FAIL_WRITE_F01_CONTROL_0;

	return UPDATE_SUCCESS;
}

int RMI4Update::WriteBlocks(unsigned char *block, unsigned short count, unsigned char cmd)
{
	int blockNum;
	unsigned char zeros[] = { 0, 0 };
	int rc;
	unsigned short addr = m_f34.GetDataBase() + RMI_F34_BLOCK_DATA_OFFSET;

	rc = m_device.Write(m_f34.GetDataBase(), zeros, 2);
	if (rc < 0)
		return UPDATE_FAIL_WRITE_INITIAL_ZEROS;

	for (blockNum = 0; blockNum < count; ++blockNum) {
		rc = m_device.Write(addr, block, m_blockSize);
		if (rc < 0) {
			fprintf(stderr, "failed to write block %d\n", blockNum);
			return UPDATE_FAIL_WRITE_BLOCK;
		}


		rc = m_device.Write(m_f34StatusAddr, &cmd, 1);
		if (rc < 0) {
			fprintf(stderr, "failed to write command for block %d\n", blockNum);
			return UPDATE_FAIL_WRITE_FLASH_COMMAND;
		}

		rc = WaitForIdle(RMI_F34_IDLE_WAIT_MS);
		if (rc != UPDATE_SUCCESS) {
			fprintf(stderr, "failed to go into idle after writing block %d\n", blockNum);
			return UPDATE_FAIL_NOT_IN_IDLE_STATE;
		}

		block += m_blockSize;
	}

	return UPDATE_SUCCESS;
}

/*
 * This is a limited implementation of WaitForIdle which assumes WaitForAttention is supported
 * this will be true for HID, but other protocols will need to revert polling. Polling
 * is not implemented yet.
 */
int RMI4Update::WaitForIdle(int timeout_ms)
{
	int rc;
	struct timeval tv;

	tv.tv_sec = timeout_ms / 1000;
	tv.tv_usec = (timeout_ms % 1000) * 1000;

	rc = m_device.WaitForAttention(&tv);
	if (rc > 0) {
		rc = ReadF34Controls();
		if (rc != UPDATE_SUCCESS)
			return rc;

		if (!m_f34Status && !m_f34Command) {
			if (!m_programEnabled) {
				fprintf(stderr, "Bootloader is idle but program_enabled bit isn't set.\n");
				return UPDATE_FAIL_PROGRAMMING_NOT_ENABLED;
			} else {
				return UPDATE_SUCCESS;
			}
		}

		fprintf(stderr, "ERROR: Waiting for idle status.\n");
		fprintf(stderr, "Command: %#04x\n", m_f34Command);
		fprintf(stderr, "Status:  %#04x\n", m_f34Status);
		fprintf(stderr, "Enabled: %d\n", m_programEnabled);
		fprintf(stderr, "Idle:    %d\n", !m_f34Command && !m_f34Status);

		return UPDATE_FAIL_NOT_IN_IDLE_STATE;
	}
	
	return UPDATE_FAIL_TIMEOUT;
}