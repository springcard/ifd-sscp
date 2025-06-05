#ifndef __SSCP_HOST_H__
#define __SSCP_HOST_H__

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else

/* From PCSD Lite */
#include <PCSC/wintypes.h>

#ifndef TRUE
#define TRUE true
#endif

#ifndef FALSE
#define FALSE false
#endif

#endif

typedef struct _SSCP_CTX_ST SSCP_CTX_ST;

SSCP_CTX_ST* SSCP_Alloc(void);
void SSCP_Free(SSCP_CTX_ST* ctx);

LONG SSCP_Open(SSCP_CTX_ST* ctx, const char* commName, DWORD commBaudrate, DWORD commFlags);
LONG SSCP_Close(SSCP_CTX_ST* ctx);

LONG SSCP_SetAddress(SSCP_CTX_ST* ctx, BYTE address);

LONG SSCP_Authenticate(SSCP_CTX_ST* ctx, const BYTE authKeyValue[16]);
LONG SSCP_Outputs(SSCP_CTX_ST* ctx, BYTE ledColor, BYTE ledDuration, BYTE buzzerDuration);
LONG SSCP_GetInfos(SSCP_CTX_ST* ctx, BYTE* version, BYTE* baudrate, BYTE* address, WORD* voltage);
LONG SSCP_GetSerialNumber(SSCP_CTX_ST* ctx, char *serialNumber, BYTE maxSerialNumberSz);
LONG SSCP_GetReaderType(SSCP_CTX_ST* ctx, char *readerType, BYTE maxReaderTypeSz);

LONG SSCP_ScanNFC(SSCP_CTX_ST* ctx, WORD *protocol, BYTE uid[], BYTE maxUidSz, BYTE* actUidSz, BYTE ats[], BYTE maxAtsSz, BYTE* actAtsSz);
LONG SSCP_TransceiveNFC(SSCP_CTX_ST* ctx, const BYTE commandApdu[], DWORD commandApduSz, BYTE responseApdu[], DWORD maxResponseApduSz, DWORD *actResponseApduSz);
LONG SSCP_ReleaseNFC(SSCP_CTX_ST* ctx);

typedef struct
{
	DWORD totalTime;
	DWORD totalErrors;
	DWORD bytesSent;
	DWORD bytesReceived;
	DWORD sessionCount;
	DWORD sessionTime;
	DWORD sessionCounter;
} SSCP_STATISTICS_ST;

LONG SSCP_GetStatistics(SSCP_CTX_ST* ctx, SSCP_STATISTICS_ST *stats);

LONG SSCP_Authenticate_SelfTest(SSCP_CTX_ST* ctx, const BYTE authKeyValue[16]);
LONG SSCP_Outputs_SelfTest(SSCP_CTX_ST* ctx, BYTE ledColor, BYTE ledDuration, BYTE buzzerDuration);

#include <sscp-consts.h>
#include <sscp-errors.h>

#endif
