#include "rmifunction.h"

#define RMI_FUNCTION_QUERY_OFFSET		0
#define RMI_FUNCTION_COMMAND_OFFSET		1
#define RMI_FUNCTION_CONTROL_OFFSET		2
#define RMI_FUNCTION_DATA_OFFSET		3
#define RMI_FUNCTION_INTERRUPT_SOURCES_OFFSET	4
#define RMI_FUNCTION_NUMBER			5

#define RMI_FUNCTION_VERSION_MASK		0x60
#define RMI_FUNCTION_INTERRUPT_SOURCES_MASK	0x7

RMIFunction::RMIFunction(const unsigned char * pdtEntry)
{
	if (pdtEntry) {
		m_queryBase = pdtEntry[RMI_FUNCTION_QUERY_OFFSET];
		m_commandBase = pdtEntry[RMI_FUNCTION_COMMAND_OFFSET];
		m_controlBase = pdtEntry[RMI_FUNCTION_CONTROL_OFFSET];
		m_dataBase = pdtEntry[RMI_FUNCTION_DATA_OFFSET];
		m_interruptSourceCount = pdtEntry[RMI_FUNCTION_INTERRUPT_SOURCES_OFFSET]
						& RMI_FUNCTION_INTERRUPT_SOURCES_MASK;
		m_functionNumber = pdtEntry[RMI_FUNCTION_NUMBER];
		m_functionVersion = (pdtEntry[RMI_FUNCTION_INTERRUPT_SOURCES_OFFSET]
						& RMI_FUNCTION_VERSION_MASK) >> 5;
	}
}