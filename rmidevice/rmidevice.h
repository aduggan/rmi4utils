#ifndef _RMIDEVICE_H_
#define _RMIDEVICE_H_

#include <cstddef>
#include <vector>

#include "rmifunction.h"

#define RMI_PRODUCT_ID_LENGTH		10

class RMIDevice
{
public:
	RMIDevice(int bytesPerReadRequest = 0) : m_bCancel(false),
			m_bytesPerReadRequest(bytesPerReadRequest), m_page(-1)
	{}
	virtual int Open(const char * filename) = 0;
	virtual int Read(unsigned short addr, unsigned char *data,
				unsigned short len) = 0;
	virtual int Write(unsigned short addr, const unsigned char *data,
				 unsigned short len) = 0;
	virtual int SetMode(int mode) { return -1; /* Unsupported */ }
	virtual int WaitForAttention(struct timeval * timeout = NULL, int *sources = NULL) = 0;
	virtual int GetAttentionReport(struct timeval * timeout, int *sources, unsigned char *buf,
					int *len)
	{ return -1; /* Unsupported */ }
	virtual void Close() = 0;
	virtual void Cancel() { m_bCancel = true; }

	unsigned long GetFirmwareID() { return m_buildID; }
	virtual int QueryBasicProperties();
	
	int SetRMIPage(unsigned char page);
	
	int ScanPDT();
	void PrintProperties();
	int Reset();

	bool GetFunction(RMIFunction &func, int functionNumber);

	void SetBytesPerReadRequest(int bytes) { m_bytesPerReadRequest = bytes; }

protected:
	std::vector<RMIFunction> m_functionList;
	unsigned char m_manufacturerID;
	bool m_hasLTS;
	bool m_hasSensorID;
	bool m_hasAdjustableDoze;
	bool m_hasAdjustableDozeHoldoff;
	bool m_hasQuery42;
	char m_dom[11];
	unsigned char m_productID[RMI_PRODUCT_ID_LENGTH + 1];
	unsigned short m_productInfo;
	unsigned short m_packageID;
	unsigned short m_packageRev;
	unsigned long m_buildID;
	unsigned char m_sensorID;
	unsigned long m_boardID;

	bool m_hasDS4Queries;
	bool m_hasMultiPhysical;

	unsigned char m_ds4QueryLength;

	bool m_hasPackageIDQuery;
	bool m_hasBuildIDQuery;

	bool m_bCancel;
	int m_bytesPerReadRequest;
	int m_page;
 };

long long diff_time(struct timespec *start, struct timespec *end);
int Sleep(int ms);

#endif /* _RMIDEVICE_H_ */