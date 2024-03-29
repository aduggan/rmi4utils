/*
 * Copyright (C) 2012 - 2014 Andrew Duggan
 * Copyright (C) 2012 - 2014 Synaptics Inc
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

#include <iostream>
#include <fstream>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "rmidevice.h"
#include "firmware_image.h"

using namespace std;

unsigned long FirmwareImage::Checksum(unsigned short * data, unsigned long len)
{
	unsigned long checksum = 0xFFFFFFFF;
	unsigned long lsw = checksum & 0xFFFF;
	unsigned long msw = checksum >> 16;

	while (len--) {
		lsw += *data++;
		msw += lsw;
		lsw = (lsw & 0xffff) + (lsw >> 16);
		msw = (msw & 0xffff) + (msw >> 16);
	}

	checksum = msw << 16 | lsw;

	return checksum;
}

void FirmwareImage::ParseHierarchicalImg()
{
	struct container_descriptor *descriptor;
	int numOfCntrs;
	int ii;
	unsigned int addr;
	unsigned int offset;
	unsigned int length;
	unsigned char *content;
	unsigned short container_id;
	unsigned int sigature_size;
	
	for (ii = 0; ii < BLv7_MAX; ii++) {
		m_signatureInfo[ii].bExisted = false;
		m_signatureInfo[ii].size = 0;
	}

	if (m_bootloaderVersion == RMI_IMG_V10_SIGNATURE_VERSION_NUMBER) {
		fprintf (stdout, "has signature\n");	
	}

	m_cntrAddr = extract_long(&m_memBlock[RMI_IMG_V10_CNTR_ADDR_OFFSET]);
	descriptor = (struct container_descriptor *)(m_memBlock + m_cntrAddr);
	offset = extract_long(descriptor->content_address);
	numOfCntrs = extract_long(descriptor->content_length) / 4;

	for (ii = 0; ii < numOfCntrs; ii++) {
		addr = extract_long(m_memBlock + offset);
		offset += 4;
		descriptor = (struct container_descriptor *)(m_memBlock + addr);
		container_id = descriptor->container_id[0] |
				descriptor->container_id[1] << 8;
		content = m_memBlock + extract_long(descriptor->content_address);
		length = extract_long(descriptor->content_length);
		sigature_size = extract_long(descriptor->signature_size);
		switch (container_id) {
		case BL_CONTAINER:
			m_bootloaderVersion = *content;
			break;
		case UI_CONTAINER:
		case CORE_CODE_CONTAINER:
			if (sigature_size != 0) {
				fprintf(stdout, "CORE CODE signature size : 0x%x\n", sigature_size);
				m_signatureInfo[BLv7_CORE_CODE].bExisted = true;
				m_signatureInfo[BLv7_CORE_CODE].size = sigature_size;
			}
			m_firmwareData = content;
			m_firmwareSize = length;
			break;
		case FLASH_CONFIG_CONTAINER:
			if (sigature_size != 0) {
				fprintf(stdout, "FLASH CONFIG signature size : 0x%x\n", sigature_size);
				m_signatureInfo[BLv7_FLASH_CONFIG].bExisted = true;
				m_signatureInfo[BLv7_FLASH_CONFIG].size = sigature_size;
			}
			m_flashConfigData = content;
			m_flashConfigSize = length;
			break;
		case UI_CONFIG_CONTAINER:
		case CORE_CONFIG_CONTAINER:
			if (sigature_size != 0) {
				fprintf(stdout, "CORE CONFIG signature size : 0x%x\n", sigature_size);
				m_signatureInfo[BLv7_CORE_CONFIG].bExisted = true;
				m_signatureInfo[BLv7_CORE_CONFIG].size = sigature_size;
			}
			m_configData = content;
			m_configSize = length;
			break;
		case PERMANENT_CONFIG_CONTAINER:
		case GUEST_SERIALIZATION_CONTAINER:
			m_lockdownData = content;
			m_lockdownSize = length;
			break;
		case GENERAL_INFORMATION_CONTAINER:
			m_io = true;
			m_packageID = extract_long(content);
			m_firmwareBuildID = extract_long(content + 4);
			memcpy(m_productID, (content + 0x18), RMI_PRODUCT_ID_LENGTH);
			m_productID[RMI_PRODUCT_ID_LENGTH] = 0;
			if ((descriptor->major_version == 0) && 
				(descriptor->minor_version > 0)) {
				m_hasFirmwareVersion = true;
				fprintf(stdout, "General Information version : %d.%d\n", descriptor->major_version, descriptor->minor_version);
				m_firmwareVersion = *(content + 0x26) << 8 | *(content + 0x27);
				fprintf(stdout, "Firmware version : 0x%x\n", m_firmwareVersion);
			}
			break;
		case FIXED_LOCATION_DATA_CONTAINER:
			if (sigature_size != 0) {
				fprintf(stdout, "FLD signature size : 0x%x\n", sigature_size);
				m_signatureInfo[BLv7_FLD].bExisted = true;
				m_signatureInfo[BLv7_FLD].size = sigature_size;
			}
			m_fldData = content;
			m_fldSize = length;
			break;
		case GLOBAL_PARAMETERS_CONTAINER:
			m_globalparaData = content;
			m_globalparaSize = length;
			break;
		default:
			break;
		}
	}
}

int FirmwareImage::Initialize(const char * filename)
{
	if (!filename)
		return UPDATE_FAIL_INVALID_PARAMETER;

	ifstream ifsFile(filename, ios::in|ios::binary|ios::ate);
	if (!ifsFile)
		return UPDATE_FAIL_OPEN_FIRMWARE_IMAGE;

	ifsFile.seekg(0, ios::end);
	m_imageSize = ifsFile.tellg();
	if (m_imageSize < 0)
		return UPDATE_FAIL_OPEN_FIRMWARE_IMAGE;

	m_memBlock = new unsigned char[m_imageSize];
	ifsFile.seekg(0, ios::beg);
	ifsFile.read((char*)m_memBlock, m_imageSize);

	if (m_imageSize < 0x100)
		return UPDATE_FAIL_VERIFY_IMAGE;

	m_checksum = extract_long(&m_memBlock[RMI_IMG_CHECKSUM_OFFSET]);

	unsigned long imageSizeMinusChecksum = m_imageSize - 4;
	if ((imageSizeMinusChecksum % 2) != 0)
		/*
		 * Since the header size is fixed and the firmware is
		 * in 16 byte blocks a valid image size should always be
		 * divisible by 2.
		 */
		return UPDATE_FAIL_VERIFY_IMAGE;

	unsigned long calculated_checksum = Checksum((uint16_t *)&(m_memBlock[4]),
		imageSizeMinusChecksum >> 1);

	if (m_checksum != calculated_checksum) {
		fprintf(stderr, "Firmware image checksum verification failed, saw 0x%08lX, calculated 0x%08lX\n",
			m_checksum, calculated_checksum);
		return UPDATE_FAIL_VERIFY_CHECKSUM;
	}

	m_io = m_memBlock[RMI_IMG_IO_OFFSET];
	m_bootloaderVersion = m_memBlock[RMI_IMG_BOOTLOADER_VERSION_OFFSET];
	m_firmwareSize = extract_long(&m_memBlock[RMI_IMG_IMAGE_SIZE_OFFSET]);
	m_configSize = extract_long(&m_memBlock[RMI_IMG_CONFIG_SIZE_OFFSET]);
	if (m_io == 1) {
		m_firmwareBuildID = extract_long(&m_memBlock[RMI_IMG_FW_BUILD_ID_OFFSET]);
		m_packageID = extract_long(&m_memBlock[RMI_IMG_PACKAGE_ID_OFFSET]);
	}
	memcpy(m_productID, &m_memBlock[RMI_IMG_PRODUCT_ID_OFFSET], RMI_PRODUCT_ID_LENGTH);
	m_productID[RMI_PRODUCT_ID_LENGTH] = 0;
	m_productInfo = extract_short(&m_memBlock[RMI_IMG_PRODUCT_INFO_OFFSET]);

	m_firmwareData = &m_memBlock[RMI_IMG_FW_OFFSET];
	m_configData = &m_memBlock[RMI_IMG_FW_OFFSET + m_firmwareSize];

	switch (m_bootloaderVersion) {
		case 2:
			m_lockdownSize = RMI_IMG_LOCKDOWN_V2_SIZE;
			m_lockdownData = &m_memBlock[RMI_IMG_LOCKDOWN_V2_OFFSET];
			break;
		case 3:
		case 4:
			m_lockdownSize = RMI_IMG_LOCKDOWN_V3_SIZE;
			m_lockdownData = &m_memBlock[RMI_IMG_LOCKDOWN_V3_OFFSET];
			break;
		case 5:
		case 6:
			m_lockdownSize = RMI_IMG_LOCKDOWN_V5_SIZE;
			m_lockdownData = &m_memBlock[RMI_IMG_LOCKDOWN_V5_OFFSET];
			break;
		case 16:
		case RMI_IMG_V10_SIGNATURE_VERSION_NUMBER:
			ParseHierarchicalImg();
			break;
		default:
			return UPDATE_FAIL_UNSUPPORTED_IMAGE_VERSION;
	}

	fprintf(stdout, "Firmware Header:\n");
	PrintHeaderInfo();

	return UPDATE_SUCCESS;
}

void FirmwareImage::PrintHeaderInfo()
{
	fprintf(stdout, "Checksum:\t\t0x%lx\n", m_checksum);
	fprintf(stdout, "Firmware Size:\t\t%ld\n", m_firmwareSize);
	fprintf(stdout, "Config Size:\t\t%ld\n", m_configSize);
	fprintf(stdout, "Lockdown Size:\t\t%ld\n", m_lockdownSize);
	fprintf(stdout, "Firmware Build ID:\t%ld\n", m_firmwareBuildID);
	fprintf(stdout, "Package ID:\t\t%d\n", m_packageID);
	fprintf(stdout, "Bootloader Version:\t%d\n", m_bootloaderVersion);
	fprintf(stdout, "Product ID:\t\t%s\n", m_productID);
	fprintf(stdout, "Product Info:\t\t%d\n", m_productInfo);
	fprintf(stdout, "\n");
}

int FirmwareImage::VerifyImageMatchesDevice(unsigned long deviceFirmwareSize,
						unsigned long deviceConfigSize)
{
	if (m_firmwareSize != deviceFirmwareSize) {
		fprintf(stderr, "Firmware image size verfication failed, size in image %ld did "
			"not match device size %ld\n", m_firmwareSize, deviceFirmwareSize);
		return UPDATE_FAIL_VERIFY_FIRMWARE_SIZE;
	}

	if (m_configSize != deviceConfigSize) {
		fprintf(stderr, "Firmware image size verfication failed, size in image %ld did "
			"not match device size %ld\n", m_firmwareSize, deviceConfigSize);
		return UPDATE_FAIL_VERIFY_CONFIG_SIZE;
	}
	
	return UPDATE_SUCCESS;
}

int FirmwareImage::VerifyImageProductID(char* deviceProductID)
{
	if (strcmp(m_productID, deviceProductID) == 0) {
		fprintf(stdout, "image matched\n");
		return UPDATE_SUCCESS;
	} else {
		fprintf (stdout, "image not match, terminated\n");
		return UPDATE_FAIL_VERIFY_IMAGE_PRODUCTID_NOT_MATCH;
	}
}

FirmwareImage::~FirmwareImage()
{
	delete [] m_memBlock;
	m_memBlock = NULL;
}
