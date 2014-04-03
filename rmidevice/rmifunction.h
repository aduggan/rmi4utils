#ifndef _RMIFUNCTION_H_
#define _RMIFUNCTION_H_

class RMIFunction
{
public:
	RMIFunction() {}
	RMIFunction(const unsigned char * pdtEntry);
	unsigned short GetQueryBase() { return m_queryBase; }
	unsigned short GetCommandBase() { return m_commandBase; }
	unsigned short GetControlBase() { return m_controlBase; }
	unsigned short GetDataBase() { return m_dataBase; }
	unsigned char GetInterruptSourceCount() { return m_interruptSourceCount; }
	unsigned char GetFunctionNumber() { return m_functionNumber; }
	unsigned char GetFunctionVersion() { return m_functionVersion; }

private:
	unsigned short m_queryBase;
	unsigned short m_commandBase;
	unsigned short m_controlBase;
	unsigned short m_dataBase;
	unsigned char m_interruptSourceCount;
	unsigned char m_functionNumber;
	unsigned char m_functionVersion;
};

#endif // _RMIFUNCTION_H_