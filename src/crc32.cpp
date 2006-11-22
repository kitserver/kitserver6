#include "crc32.h"
#include "kserv.h"

#define QUOTIENT 0x04c11db7

// This function computes CRC32 of the encoded buffer
DWORD Sign(BYTE* encBuffer)
{
	ENCBUFFERHEADER* header = (ENCBUFFERHEADER*)encBuffer;
	DWORD len = header->dwEncSize;
	BYTE* data = encBuffer + sizeof(ENCBUFFERHEADER);

    DWORD result;
    int i,j;
    BYTE octet;
    
    result = -1;
    
    for (i=0; i<len; i++)
    {
        octet = *(data++);
        for (j=0; j<8; j++)
        {
            if ((octet >> 7) ^ (result >> 31))
            {
                result = (result << 1) ^ QUOTIENT;
            }
            else
            {
                result = (result << 1);
            }
            octet <<= 1;
        }
    }
    
    return ~result; // The complement of the remainder
}

DWORD GetCRC(BYTE* data, DWORD len)
{
    DWORD result;
    int i,j;
    BYTE octet;
    
    result = -1;
    
    for (i=0; i<len; i++)
    {
        octet = *(data++);
        for (j=0; j<8; j++)
        {
            if ((octet >> 7) ^ (result >> 31))
            {
                result = (result << 1) ^ QUOTIENT;
            }
            else
            {
                result = (result << 1);
            }
            octet <<= 1;
        }
    }
    
    return ~result; // The complement of the remainder
}
