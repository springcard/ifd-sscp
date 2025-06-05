#ifndef __SSCP_HOST_I_H__
#define __SSCP_HOST_I_H__

#include <sscp-host.h>
#include <sscp-consts.h>

struct _SSCP_CTX_ST
{
#ifdef _WIN32
	HANDLE commHandle;
#else
	int commFd;
	DWORD firstByteTimeout;
	DWORD interByteTimeout;
#endif
	BYTE address;
	DWORD counter;
	BYTE sessionKeyCipherAB[16];
	BYTE sessionKeyCipherBA[16];
	BYTE sessionKeySignAB[16];
	BYTE sessionKeySignBA[16];

	BOOL guardRunning;
#ifdef _WIN32	
	LARGE_INTEGER guardFreq;
	LARGE_INTEGER guardStart;
#else
	struct timespec guardStart;
#endif
	DWORD guardValue;

	struct
	{
		time_t whenOpen;
		time_t whenSession;
		DWORD sessionCount;
		DWORD errorCount;
		DWORD bytesSent;
		DWORD bytesReceived;
	} stats;
};

LONG SSCP_ExchangeRaw(SSCP_CTX_ST* ctx, BYTE address, BYTE protocol, const BYTE command[], DWORD commandSz, BYTE response[], DWORD maxResponseSz, DWORD* actResponseSz);

LONG SSCP_Exchange(SSCP_CTX_ST* ctx, DWORD commandHeader, const BYTE commandData[], DWORD commandDataSz, BYTE responseData[], DWORD maxResponseDataSz, DWORD* actResponseDataSz);
LONG SSCP_Exchange_NoDataIn(SSCP_CTX_ST* ctx, DWORD commandHeader, BYTE responseData[], DWORD maxResponseDataSz, DWORD* actResponseDataSz);
LONG SSCP_Exchange_NoDataOut(SSCP_CTX_ST* ctx, DWORD commandHeader, const BYTE commandData[], DWORD commandDataSz);
LONG SSCP_Exchange_NoDataInOut(SSCP_CTX_ST* ctx, DWORD commandHeader);

LONG SSCP_Exchange_SelfTest(SSCP_CTX_ST* ctx, DWORD commandHeader, const BYTE commandData[], DWORD commandDataSz, BYTE responseData[], DWORD maxResponseDataSz, DWORD* actResponseDataSz);

BOOL SSCP_HMAC(const BYTE keyValue[16], const BYTE buffer[], DWORD length, BYTE hmac[32]);
BOOL SSCP_Cipher(const BYTE keyValue[16], const BYTE initVector[16], BYTE buffer[], DWORD length);
BOOL SSCP_Decipher(const BYTE keyValue[16], const BYTE initVector[16], BYTE buffer[], DWORD length);
BOOL SSCP_ComputeSessionKeys(SSCP_CTX_ST* ctx, const BYTE authKeyValue[16], const BYTE rndA[16], const BYTE rndB[16]);

void SSCP_GuardTime(SSCP_CTX_ST* ctx, DWORD guardTimeMs);
void SSCP_InitGuardTime(SSCP_CTX_ST* ctx, DWORD guardTimeMs);
void SSCP_WaitGuardTime(SSCP_CTX_ST* ctx);

LONG SSCP_SerialOpen(SSCP_CTX_ST* ctx, const char* commName);
LONG SSCP_SerialClose(SSCP_CTX_ST* ctx);
LONG SSCP_SerialConfigure(SSCP_CTX_ST* ctx, DWORD baudrate);
LONG SSCP_SerialSetTimeouts(SSCP_CTX_ST* ctx, DWORD first_byte, DWORD inter_byte);
LONG SSCP_SerialSend(SSCP_CTX_ST* ctx, const BYTE buffer[], DWORD length);
LONG SSCP_SerialRecv(SSCP_CTX_ST* ctx, BYTE buffer[], DWORD length);

BOOL SSCP_GetRandom(BYTE buffer[], DWORD bufferSz);

#define SSCP_Trace printf

#include "sscp-host-crypto_i.h"
#include "sscp-host-serial_i.h"

#endif
