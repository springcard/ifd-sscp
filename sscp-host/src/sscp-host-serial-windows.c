#include "sscp-host-serial_i.h"

#ifdef _WIN32

BOOL SSCP_DEBUG_SERIAL = FALSE;

LONG SSCP_SerialOpen(SSCP_CTX_ST* ctx, const char* commName)
{
	if (ctx == NULL)
		return SSCP_ERR_INVALID_CONTEXT;
	if (commName == NULL)
		return SSCP_ERR_INVALID_PARAMETER;

	/* Don't forget to close in case of it were previously open */
	SSCP_SerialClose(ctx);

	/* Start-up here */
	if (SSCP_DEBUG_SERIAL)
		SSCP_Trace("Opening device %s...\n", commName);

	ctx->commHandle = CreateFile(commName, GENERIC_READ | GENERIC_WRITE, 0,	// comm devices must be opened w/exclusive- 
		NULL,		// no security attributes
		OPEN_EXISTING,	// comm devices must use OPEN_EXISTING
		0,		// not overlapped I/O
		NULL		// hTemplate must be NULL for comm devices
	);

	if (ctx->commHandle == INVALID_HANDLE_VALUE)
		return SSCP_ERR_COMM_NOT_AVAILABLE;

	SetupComm(ctx->commHandle, 512, 512);

	return SSCP_SUCCESS;
}

LONG SSCP_SerialClose(SSCP_CTX_ST* ctx)
{
	if (ctx == NULL)
		return SSCP_ERR_INVALID_CONTEXT;
	if (ctx->commHandle == INVALID_HANDLE_VALUE)
		return SSCP_ERR_COMM_NOT_OPEN;

	if (SSCP_DEBUG_SERIAL)
		SSCP_Trace("Closing device\n");

	CloseHandle(ctx->commHandle);

	ctx->commHandle = INVALID_HANDLE_VALUE;

	return SSCP_SUCCESS;
}

LONG SSCP_SerialConfigure(SSCP_CTX_ST* ctx, DWORD baudrate)
{
	DCB dcb;

	if (ctx == NULL)
		return SSCP_ERR_INVALID_CONTEXT;
	if (ctx->commHandle == INVALID_HANDLE_VALUE)
		return SSCP_ERR_COMM_NOT_OPEN;

	if (!GetCommState(ctx->commHandle, &dcb))
	{
		if (SSCP_DEBUG_SERIAL)
			SSCP_Trace("GetCommState failed (%d)\n", GetLastError());
		return SSCP_ERR_COMM_CONTROL_FAILED;
	}

	dcb.BaudRate = baudrate;

	dcb.fBinary = TRUE;
	dcb.fParity = FALSE;
	dcb.fOutxCtsFlow = FALSE;
	dcb.fOutxDsrFlow = FALSE;
	dcb.fDsrSensitivity = FALSE;
	dcb.fOutX = FALSE;
	dcb.fInX = FALSE;
	dcb.fNull = FALSE;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.fRtsControl = RTS_CONTROL_ENABLE;	/* New 1.73 : for compatibility with RL78 flash board */
	dcb.fDtrControl = DTR_CONTROL_ENABLE;	/* New 1.73 : for compatibility with RL78 flash board */

	dcb.fAbortOnError = TRUE;
	dcb.fTXContinueOnXoff = TRUE;

	if (!SetCommState(ctx->commHandle, &dcb))
	{
		if (SSCP_DEBUG_SERIAL)
			SSCP_Trace("SetCommState failed (%d)\n", GetLastError());
		return SSCP_ERR_COMM_CONTROL_FAILED;
	}

	return SSCP_SUCCESS;
}

LONG SSCP_SerialSetTimeouts(SSCP_CTX_ST* ctx, DWORD first_byte, DWORD inter_byte)
{
	COMMTIMEOUTS stTimeout = { 0 };

	if (ctx == NULL)
		return SSCP_ERR_INVALID_CONTEXT;
	if (ctx->commHandle == INVALID_HANDLE_VALUE)
		return SSCP_ERR_COMM_NOT_OPEN;

	stTimeout.ReadIntervalTimeout = inter_byte;
	stTimeout.ReadTotalTimeoutConstant = first_byte;
	stTimeout.ReadTotalTimeoutMultiplier = inter_byte;
	stTimeout.WriteTotalTimeoutConstant = inter_byte;
	stTimeout.WriteTotalTimeoutMultiplier = inter_byte;

	if (!SetCommTimeouts(ctx->commHandle, &stTimeout))
	{
		if (SSCP_DEBUG_SERIAL)
			SSCP_Trace("SetCommTimeouts failed (%d)\n", GetLastError());
		return SSCP_ERR_COMM_CONTROL_FAILED;
	}

	return SSCP_SUCCESS;
}

LONG SSCP_SerialSend(SSCP_CTX_ST* ctx, const BYTE buffer[], DWORD length)
{
	const BYTE* pSendBuffer;
	DWORD dwTotalLen;

	if (ctx == NULL)
		return SSCP_ERR_INVALID_CONTEXT;
	if (ctx->commHandle == INVALID_HANDLE_VALUE)
		return SSCP_ERR_COMM_NOT_OPEN;
	if (buffer == NULL)
		return SSCP_ERR_INVALID_PARAMETER;

	pSendBuffer = buffer;
	dwTotalLen = length;

	while (dwTotalLen)
	{		
		DWORD dwWriteLen;
		DWORD dwWritten = 0;		
		DWORD i;

		if (dwTotalLen < 260)
			dwWriteLen = dwTotalLen;
		else
			dwWriteLen = 256;

		if (!WriteFile(ctx->commHandle, pSendBuffer, dwWriteLen, &dwWritten, 0))
		{
			if (SSCP_DEBUG_SERIAL)
				SSCP_Trace("WriteFile(%d) error (%d)\n", dwWriteLen, GetLastError());
			return SSCP_ERR_COMM_SEND_FAILED;
		}

		ctx->stats.bytesSent += dwWritten;

		if (SSCP_DEBUG_SERIAL)
		{
			SSCP_Trace("<");
			for (i = 0; i < dwWritten; i++)
				SSCP_Trace("%02X", pSendBuffer[i]);
			SSCP_Trace("\n");
		}

		if (dwWritten < dwWriteLen)
		{
			if (SSCP_DEBUG_SERIAL)
				SSCP_Trace("WriteFile(%d/%d) failed (%d)\n", dwWritten, dwWriteLen, GetLastError());
			return SSCP_ERR_COMM_SEND_FAILED;
		}

		dwTotalLen -= dwWritten;
		pSendBuffer += dwWritten;
	}

	return SSCP_SUCCESS;
}

LONG SSCP_SerialRecv(SSCP_CTX_ST* ctx, BYTE buffer[], DWORD length)
{
	BYTE* pRecvBuffer;
	DWORD dwRemainingLen = length;
	DWORD dwReceivedLen = 0;
	DWORD i;

	if (ctx == NULL)
		return SSCP_ERR_INVALID_CONTEXT;
	if (ctx->commHandle == INVALID_HANDLE_VALUE)
		return SSCP_ERR_COMM_NOT_OPEN;
	if (buffer == NULL)
		return SSCP_ERR_INVALID_PARAMETER;

	pRecvBuffer = buffer;

	while (dwRemainingLen)
	{
		DWORD dwWantLen, dwGotLen;

		if (dwRemainingLen < 32)
			dwWantLen = dwRemainingLen;
		else
			dwWantLen = 32;

		if (!ReadFile(ctx->commHandle, pRecvBuffer, dwWantLen, &dwGotLen, 0))
		{
			if (SSCP_DEBUG_SERIAL)
				SSCP_Trace("ReadFile failed (%d)\n", GetLastError());
			return SSCP_ERR_COMM_RECV_FAILED;
		}

		ctx->stats.bytesReceived += dwGotLen;

		if (SSCP_DEBUG_SERIAL)
		{
			SSCP_Trace(">");
			for (i = 0; i < dwGotLen; i++)
				SSCP_Trace("%02X", pRecvBuffer[i]);
			SSCP_Trace("\n");
		}

		dwRemainingLen -= dwGotLen;
		dwReceivedLen += dwGotLen;
		pRecvBuffer += dwGotLen;

		if (dwGotLen < dwWantLen)
		{
			if (SSCP_DEBUG_SERIAL)
				SSCP_Trace("ReadFile timeout (%d<%d, total %d/%d, code %d)\n", dwGotLen, dwWantLen, dwReceivedLen, length, GetLastError());
			break;
		}
	}

	if (dwRemainingLen)	/* A timeout has occured */
	{
		if (dwReceivedLen == 0)
			return SSCP_ERR_COMM_RECV_MUTE;
		else
			return SSCP_ERR_COMM_RECV_STOPPED;
	}

	return SSCP_SUCCESS;
}

#endif
