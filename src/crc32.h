#ifndef _JUCE_CRC32_
#define _JUCE_CRC32_

#include <windows.h>

DWORD Sign(BYTE* buffer);
DWORD GetCRC(BYTE* data, DWORD len);

#endif
