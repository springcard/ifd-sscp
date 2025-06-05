#include "sscp-host_i.h"

BOOL SSCP_DEBUG_AUTHENTICATE = FALSE;

SSCP_CTX_ST* SSCP_Alloc(void)
{
	struct _SSCP_CTX_ST* ctx = calloc(1, sizeof(struct _SSCP_CTX_ST));
	if (ctx == NULL)
		return NULL;

#ifdef _WIN32
	ctx->commHandle = INVALID_HANDLE_VALUE;
#else
	ctx->commFd = -1;
#endif

	return ctx;
}

void SSCP_Free(SSCP_CTX_ST *ctx)
{
	/* Just in case... */
	SSCP_Close(ctx);

	if (ctx != NULL)
		free(ctx);
}

LONG SSCP_Open(SSCP_CTX_ST* ctx, const char* commName, DWORD commBaudrate, DWORD commFlags)
{
	LONG rc;

	if (ctx == NULL)
		return SSCP_ERR_INVALID_CONTEXT;

	if (commName == NULL)
		return SSCP_ERR_INVALID_PARAMETER;

	rc = SSCP_SerialOpen(ctx, commName);
	if (rc)
		return rc;

	rc = SSCP_SerialConfigure(ctx, commBaudrate);
	if (rc)
	{
		SSCP_SerialClose(ctx);
		return rc;
	}

	/* Default timeouts */
	rc = SSCP_SerialSetTimeouts(ctx, SSCP_RESPONSE_FIRST_TIMEOUT, SSCP_RESPONSE_NEXT_TIMEOUT);
	if (rc)
	{
		SSCP_SerialClose(ctx);
		return rc;
	}

	ctx->address = 0x00; /* Default is RS232 */

	ctx->stats.whenOpen = time(NULL);

	return SSCP_SUCCESS;
}

LONG SSCP_Close(SSCP_CTX_ST* ctx)
{
	LONG rc;

	if (ctx == NULL)
		return SSCP_ERR_INVALID_CONTEXT;

	rc = SSCP_SerialClose(ctx);
	
	return rc;
}

LONG SSCP_SetAddress(SSCP_CTX_ST* ctx, BYTE address)
{
	if (ctx == NULL)
		return SSCP_ERR_INVALID_CONTEXT;

	ctx->address = address;

	return SSCP_SUCCESS;
}

static LONG SSCP_AuthenticateEx(SSCP_CTX_ST* ctx, const BYTE authKeyValue[16], BOOL selftest)
{
	static const BYTE SSCP_DEFAULT_AUTH_KEY[16] = { 0xE7, 0x4A, 0x54, 0x0F, 0xA0, 0x7C, 0x4D, 0xB1, 0xB4, 0x64, 0x21, 0x12, 0x6D, 0xF7, 0xAD, 0x36 };	
	BYTE command[256] = { 0 };
	DWORD commandSz = 0;
	BYTE response[256] = { 0 };
	DWORD responseSz = 0;
	BYTE rndA[16] = { 0 };
	BYTE rndB[16] = { 0 };
	BYTE rndAp[16] = { 0 };
	BYTE A[4] = { 0 };
	BYTE B[4] = { 0 };
	BYTE hA[32] = { 0 };
	BYTE hB[32] = { 0 };
	LONG rc;
	int offset;

	if (ctx == NULL)
		return SSCP_ERR_INVALID_CONTEXT;

	if (authKeyValue == NULL)
		authKeyValue = SSCP_DEFAULT_AUTH_KEY;

	if (selftest)
	{
		static const BYTE R[] = { 0x75, 0xCC, 0xF7, 0xB1, 0xF7, 0xFE, 0xA6, 0xF7, 0x58, 0x71, 0xFC, 0xF6, 0xDC, 0x75, 0x59, 0x23 };
		memcpy(rndA, R, 16);
	}
	else
	{
		if (!SSCP_GetRandom(rndA, sizeof(rndA)))
			return SSCP_ERR_INTERNAL_FAILURE;
	}


	/* 1st step */
	/* -------- */
	commandSz = 0;
	command[commandSz++] = 0x00;
	command[commandSz++] = 0x00;
	memcpy(&command[commandSz], rndA, 16);
	commandSz += 16;

	if (selftest)
	{
		static const BYTE R[] = {
			0x53, 0x77, 0x07, 0xAD, 0x48, 0x6F, 0x07, 0xAD, 0x75, 0xCC, 0xF7, 0xB1, 0xF7, 0xFE, 0xA6, 0xF7,
			0x58, 0x71, 0xFC, 0xF6, 0xDC, 0x75, 0x59, 0x23, 0xC8, 0xEE, 0x7C, 0x37, 0x5C, 0x21, 0xEA, 0xC5,
			0x1B, 0xD9, 0x7C, 0x51, 0xC6, 0x9F, 0x39, 0x5B, 0x69, 0xF6, 0x61, 0x77, 0x07, 0xD9, 0x44, 0x29,
			0x40, 0xC3, 0x9B, 0xEB, 0xFA, 0x0B, 0x44, 0x59, 0xCE, 0xBF, 0x6C, 0xD5, 0xE6, 0x10, 0xEA, 0x1F,
			0xF4, 0x4B, 0x34, 0x1E, 0x29, 0x16, 0x54, 0xA9
		};

		if (SSCP_DEBUG_AUTHENTICATE)
		{
			DWORD i;
			SSCP_Trace("<");
			for (i = 0; i < commandSz; i++)
				SSCP_Trace("%02X", command[i]);
			SSCP_Trace("\n");
		}

		memcpy(response, R, sizeof(R));
		responseSz = sizeof(R);

		if (SSCP_DEBUG_AUTHENTICATE)
		{
			DWORD i;
			SSCP_Trace(">");
			for (i = 0; i < responseSz; i++)
				SSCP_Trace("%02X", response[i]);
			SSCP_Trace("\n");
		}
	}
	else
	{
		rc = SSCP_ExchangeRaw(ctx, ctx->address, SSCP_PROTOCOL_AUTHENTICATE, command, commandSz, response, sizeof(response), &responseSz);
		if (rc)
			return rc;
	}

	offset = 0;
	memcpy(B, &response[offset], 4);
	offset += 4;
	memcpy(A, &response[offset], 4);
	offset += 4;
	memcpy(rndAp, &response[offset], 16);
	offset += 16;
	memcpy(rndB, &response[offset], 16);
	offset += 16;
	/* Offset is now on hB */

	if (SSCP_DEBUG_AUTHENTICATE)
	{
		DWORD i;
		SSCP_Trace("B ");
		for (i = 0; i < 4; i++)
			SSCP_Trace("%02X", B[i]);
		SSCP_Trace("\n");
		SSCP_Trace("A ");
		for (i = 0; i < 4; i++)
			SSCP_Trace("%02X", A[i]);
		SSCP_Trace("\n");
		SSCP_Trace("RndA' ");
		for (i = 0; i < 16; i++)
			SSCP_Trace("%02X", rndAp[i]);
		SSCP_Trace("\n");
		SSCP_Trace("RndB  ");
		for (i = 0; i < 16; i++)
			SSCP_Trace("%02X", rndB[i]);
		SSCP_Trace("\n");
	}

	/* Compute hB on our side */
	if (!SSCP_HMAC(authKeyValue, response, offset, hB))
		return SSCP_ERR_INTERNAL_FAILURE;

	/* Compare with received hB */
	if (memcmp(hB, &response[offset], 32))
	{
		if (SSCP_DEBUG_AUTHENTICATE)
		{
			DWORD i;
			SSCP_Trace("Wrong HCMAC in Authenticate\n");
			SSCP_Trace("Received: ");
			for (i = 0; i < 32; i++)
				SSCP_Trace("%02X", response[offset + i]);
			SSCP_Trace("\n");
			SSCP_Trace("Computed: ");
			for (i = 0; i < 32; i++)
				SSCP_Trace("%02X", hB[i]);
			SSCP_Trace("\n");
		}

		return SSCP_ERR_WRONG_RESPONSE_SIGNATURE;
	}

	/* 2nd step */
	/* -------- */
	commandSz = 0;
	memcpy(&command[commandSz], A, 4);
	commandSz += 4;
	memcpy(&command[commandSz], rndB, 16);
	commandSz += 16;

	/* Compute hA */
	if (!SSCP_HMAC(authKeyValue, command, commandSz, hA))
		return SSCP_ERR_INTERNAL_FAILURE;

	/* Append hA to the command */
	memcpy(&command[commandSz], hA, 32);
	commandSz += 32;

	if (selftest)
	{
		static const BYTE R[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x08 };
		memcpy(response, R, sizeof(R));
		responseSz = sizeof(R);
	}
	else
	{
		rc = SSCP_ExchangeRaw(ctx, ctx->address, 0x20, command, commandSz, response, sizeof(response), &responseSz);
		if (rc)
			return rc;
	}

	/* Expected response is an ACK */

	/* Compute session keys */
	/* -------------------- */
	if (!SSCP_ComputeSessionKeys(ctx, authKeyValue, rndA, rndB))
		return SSCP_ERR_INTERNAL_FAILURE;

	/* Initialize the counter to 1 */
	ctx->counter = 1;

	ctx->stats.sessionCount++;
	ctx->stats.whenSession = time(NULL);

	return SSCP_SUCCESS;
}

LONG SSCP_Authenticate(SSCP_CTX_ST* ctx, const BYTE authKeyValue[16])
{
	return SSCP_AuthenticateEx(ctx, authKeyValue, FALSE);
}

LONG SSCP_Authenticate_SelfTest(SSCP_CTX_ST* ctx, const BYTE authKeyValue[16])
{
	return SSCP_AuthenticateEx(ctx, authKeyValue, TRUE);
}

static LONG SSCP_OutputsEx(SSCP_CTX_ST* ctx, BYTE ledColor, BYTE ledDuration, BYTE buzzerDuration, BOOL selftest)
{
	BYTE data[3];
	LONG rc;

	data[0] = ledColor;
	data[1] = ledDuration;
	data[2] = buzzerDuration;

	if (selftest)
	{
		rc = SSCP_Exchange_SelfTest(ctx, SSCP_CMD_OUTPUTS, data, sizeof(data), NULL, 0, NULL);
		if (rc)
			return rc;
	}
	else
	{
		rc = SSCP_Exchange(ctx, SSCP_CMD_OUTPUTS, data, sizeof(data), NULL, 0, NULL);
		if (rc)
			return rc;
	}

	return SSCP_SUCCESS;
}

LONG SSCP_Outputs(SSCP_CTX_ST* ctx, BYTE ledColor, BYTE ledDuration, BYTE buzzerDuration)
{
	return SSCP_OutputsEx(ctx, ledColor, ledDuration, buzzerDuration, FALSE);
}

LONG SSCP_Outputs_SelfTest(SSCP_CTX_ST* ctx, BYTE ledColor, BYTE ledDuration, BYTE buzzerDuration)
{
	return SSCP_OutputsEx(ctx, ledColor, ledDuration, buzzerDuration, TRUE);
}

LONG SSCP_GetInfos(SSCP_CTX_ST* ctx, BYTE *version, BYTE* baudrate, BYTE* address, WORD* voltage)
{
	BYTE responseData[16] = { 0 };
	DWORD responseDataSz = 0;
	LONG rc;

	rc = SSCP_Exchange_NoDataIn(ctx, SSCP_CMD_GET_INFOS, responseData, sizeof(responseData), &responseDataSz);
	if (rc)
		return rc;

	if (responseDataSz < 5)
		return SSCP_ERR_UNSUPPORTED_RESPONSE_LENGTH;

	if (version != NULL)
		*version = responseData[0];
	if (baudrate != NULL)
		*baudrate = responseData[1];
	if (address != NULL)
		*address = responseData[2];
	if (voltage != NULL)
		*voltage = (responseData[3] << 8) | (responseData[4]);

	return SSCP_SUCCESS;
}

LONG SSCP_GetSerialNumber(SSCP_CTX_ST* ctx, char* serialNumber, BYTE maxSerialNumberSz)
{
	BYTE responseData[16] = { 0 };
	DWORD responseDataSz = 0;
	LONG rc;

	if (serialNumber == NULL)
		return SSCP_ERR_INVALID_PARAMETER;

	memset(serialNumber, 0, maxSerialNumberSz);

	rc = SSCP_Exchange_NoDataIn(ctx, SSCP_CMD_GET_SERIAL_NUMBER, responseData, sizeof(responseData), &responseDataSz);
	if (rc)
		return rc;

	if (responseDataSz != 5)
		return SSCP_ERR_UNSUPPORTED_RESPONSE_LENGTH;

	snprintf(serialNumber, maxSerialNumberSz, "%c%02X%02X%02X%02X", responseData[0], responseData[1], responseData[2], responseData[3], responseData[4]);

	return SSCP_SUCCESS;
}

LONG SSCP_GetReaderType(SSCP_CTX_ST* ctx, char* readerType, BYTE maxReaderTypeSz)
{
	BYTE responseData[32] = { 0 };
	DWORD responseDataSz = 0;
	DWORD i;
	LONG rc;

	if (readerType == NULL)
		return SSCP_ERR_INVALID_PARAMETER;

	memset(readerType, 0, maxReaderTypeSz);

	rc = SSCP_Exchange_NoDataIn(ctx, SSCP_CMD_GET_READER_TYPE, responseData, sizeof(responseData), &responseDataSz);
	if (rc)
		return rc;

	for (i = 0; i < responseDataSz; i++)
	{
		if (i >= maxReaderTypeSz)
			break; /* Don't overwrite the last '\0' */
		readerType[i] = (char)responseData[i];
		if (responseData[i] == 0x00)
			break; /* EOT */
	}

	return SSCP_SUCCESS;
}

LONG SSCP_ScanNFC(SSCP_CTX_ST* ctx, WORD* protocol, BYTE uid[], BYTE maxUidSz, BYTE* actUidSz, BYTE ats[], BYTE maxAtsSz, BYTE* actAtsSz)
{
	BYTE filter[] = { 0x00, 0x07 };
	BYTE responseData[32] = { 0 };
	DWORD responseDataSz = 0;
	BYTE responseType = 0;
	LONG rc;
	BYTE length;
	DWORD offset = 0;

	if (ctx == NULL)
		return SSCP_ERR_INVALID_CONTEXT;

	if (protocol == NULL)
		return SSCP_ERR_INVALID_PARAMETER;
	if ((uid != NULL) && (actUidSz == NULL))
		return SSCP_ERR_INVALID_PARAMETER;
	if ((ats != NULL) && (actAtsSz == NULL))
		return SSCP_ERR_INVALID_PARAMETER;

	*protocol = 0;
	if (actUidSz != NULL)
		*actUidSz = 0;
	if (actAtsSz != NULL)
		*actAtsSz = 0;

	/* Make sure we don't call this function too often, because the reader is __slow__ */
	SSCP_GuardTime(ctx, SSCP_SCAN_GLOBAL_GUARD_TIME);

	/* Command is SCAN_GLOBAL */
	rc = SSCP_Exchange(ctx, SSCP_CMD_SCAN_GLOBAL, filter, sizeof(filter), responseData, sizeof(responseData), &responseDataSz);
	if (rc)
		return rc;

	if (responseDataSz < 1)
		return SSCP_ERR_WRONG_RESPONSE_LENGTH;

	responseType = responseData[offset++];

	switch (responseType)
	{
		case 0x00 :
			/* No tag */
		break;

		case 0x01 :
			/* ISOA */
			*protocol = 0x0001;
			if (responseDataSz < 6)
				return SSCP_ERR_UNSUPPORTED_RESPONSE_LENGTH;
			if (responseData[offset++] != 1)
				return SSCP_ERR_UNSUPPORTED_RESPONSE_VALUE;
			/* Skip ATQA and SAK */
			offset += 3;
			/* This is UIDLen */
			length = responseData[offset++];
			if (offset + length > responseDataSz)
				return SSCP_ERR_UNSUPPORTED_RESPONSE_VALUE; /* Not a valid length */
			if (actUidSz != NULL)
				*actUidSz = length;
			if (length > maxUidSz)
				return SSCP_ERR_OUTPUT_BUFFER_OVERFLOW;
			if (uid != NULL)
				memcpy(uid, &responseData[offset], length);
			offset += length;
			if (offset < responseDataSz)
			{
				/* This is ATSLen */
				length = responseData[offset]; /* ATSLen is part of the ATS itself, so no offset increment */
				if (offset + length > responseDataSz)
					return SSCP_ERR_UNSUPPORTED_RESPONSE_VALUE; /* Not a valid length */
				if (actAtsSz != NULL)
					*actAtsSz = length;
				if (length > maxAtsSz)
					return SSCP_ERR_OUTPUT_BUFFER_OVERFLOW;
				if (ats != NULL)
					memcpy(ats, &responseData[offset], length);
				offset += length;
			}
		break;

		case 0x02 :
			/* ISOB */
			*protocol = 0x0002;
			if (responseDataSz < 4)
				return SSCP_ERR_UNSUPPORTED_RESPONSE_LENGTH;
			if (responseData[offset++] != 1)
				return SSCP_ERR_UNSUPPORTED_RESPONSE_VALUE;
			/* Skip RFU */
			offset += 1;
			/* This is UIDLen */
			length = responseData[offset++];
			if (offset + length > responseDataSz)
				return SSCP_ERR_UNSUPPORTED_RESPONSE_VALUE; /* Not a valid length */
			if (actUidSz != NULL)
				*actUidSz = length;
			if (length > maxUidSz)
				return SSCP_ERR_OUTPUT_BUFFER_OVERFLOW;
			if (uid != NULL)
				memcpy(uid, &responseData[offset], length);
		break;

		default:
			return SSCP_ERR_UNSUPPORTED_RESPONSE_STATUS;
	}

	return SSCP_SUCCESS;
}

LONG SSCP_TransceiveNFC(SSCP_CTX_ST* ctx, const BYTE commandApdu[], DWORD commandApduSz, BYTE responseApdu[], DWORD maxResponseApduSz, DWORD* actResponseApduSz)
{
	BYTE responseData[256] = { 0 };
	DWORD responseDataSz = 0;
	BYTE responseStatus = 0;
	LONG rc;

	if (ctx == NULL)
		return SSCP_ERR_INVALID_CONTEXT;

	if (actResponseApduSz != NULL)
		*actResponseApduSz = 0;

	/* Command is TRANSCEIVE APDU */
	rc = SSCP_Exchange(ctx, SSCP_CMD_TRANSCEIVE_APDU, commandApdu, commandApduSz, responseData, sizeof(responseData), &responseDataSz);
	if (rc)
		return rc;

	if (responseDataSz < 1)
		return SSCP_ERR_WRONG_RESPONSE_LENGTH;

	responseStatus = responseData[0];

	switch (responseStatus)
	{
		case 0x00:
			/* No error */
			if (actResponseApduSz != NULL)
				*actResponseApduSz = responseDataSz - 1;
			if (responseDataSz - 1 > maxResponseApduSz)
				return SSCP_ERR_OUTPUT_BUFFER_OVERFLOW;
			memcpy(responseApdu, &responseData[1], responseDataSz - 1);
		break;

		case 0x01:
			/* Card timeout */
			return SSCP_ERR_NFC_CARD_MUTE_OR_REMOVED;
		break;

		case 0x02:
			/* Card communication error */
			return SSCP_ERR_NFC_CARD_COMM_ERROR;
		break;

	default:
		return SSCP_ERR_UNSUPPORTED_RESPONSE_STATUS;
	}

	return SSCP_SUCCESS;
}

LONG SSCP_ReleaseNFC(SSCP_CTX_ST* ctx)
{
	return SSCP_Exchange_NoDataInOut(ctx, SSCP_CMD_RELEASE_RF);
}

LONG SSCP_GetStatistics(SSCP_CTX_ST* ctx, SSCP_STATISTICS_ST* stats)
{
	if (ctx == NULL)
		return SSCP_ERR_INVALID_CONTEXT;
	if (stats == NULL)
		return SSCP_ERR_INVALID_PARAMETER;

	memset(stats, 0, sizeof(SSCP_STATISTICS_ST));

	if (ctx->stats.whenOpen)
		stats->totalTime = (DWORD) (time(NULL) - ctx->stats.whenOpen);
	stats->bytesSent = ctx->stats.bytesSent;
	stats->bytesReceived = ctx->stats.bytesReceived;
	stats->totalErrors = ctx->stats.errorCount;
	stats->sessionCount = ctx->stats.sessionCount;
	if (ctx->stats.whenSession)
		stats->sessionTime = (DWORD)(time(NULL) - ctx->stats.whenSession);
	stats->sessionCounter = ctx->counter;

	return SSCP_SUCCESS;
}

