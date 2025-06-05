#include "sscp-host_i.h"

BOOL SSCP_DEBUG_EXCHANGE = FALSE;

/**
  * \brief compute CRC
 */
static void SSCP_SCR16(const BYTE part1[], DWORD part1Sz, const BYTE part2[], DWORD part2Sz, BYTE pcrc[2])
{
    short crc = 0xFFFF;
    DWORD i = 0;
    unsigned char j = 0x00;
    short dbyte = 0;
    short mix = 0;

    for (i = 0; i < part1Sz; ++i)
    {
        dbyte = part1[i];
        crc ^= dbyte << 8;

        for (j = 0; j < 8; ++j)
        {
            mix = crc & 0x8000;
            crc = (crc << 1);
            if (mix)
                crc = crc ^ 0x1021;
        }
    }

    for (i = 0; i < part2Sz; ++i)
    {
        dbyte = part2[i];
        crc ^= dbyte << 8;

        for (j = 0; j < 8; ++j)
        {
            mix = crc & 0x8000;
            crc = (crc << 1);
            if (mix)
                crc = crc ^ 0x1021;
        }
    }

	pcrc[0] = (BYTE)(crc >> 8);
	pcrc[1] = (BYTE)(crc);
}

LONG SSCP_ExchangeRaw(SSCP_CTX_ST* ctx, BYTE address, BYTE protocol, const BYTE command[], DWORD commandSz, BYTE response[], DWORD maxResponseSz, DWORD* actResponseSz)
{
    BYTE header[5];
    BYTE crcA[2], crcB[2];
    DWORD length;
    LONG rc;

    if (ctx == NULL)
        return SSCP_ERR_INVALID_CONTEXT;
    if ((command == NULL) && (commandSz > 0))
        return SSCP_ERR_INVALID_PARAMETER;
    if (commandSz > 4096)
        return SSCP_ERR_COMMAND_TOO_LONG;

    /* Set the timeouts */
    rc = SSCP_SerialSetTimeouts(ctx, SSCP_RESPONSE_FIRST_TIMEOUT, SSCP_RESPONSE_NEXT_TIMEOUT);
    if (rc)
        return rc;

    /* Prepare frame to be sent */
    /* ------------------------ */

    header[0] = 0x02; /* SOF */
    header[1] = (BYTE)(commandSz >> 8);
    header[2] = (BYTE)(commandSz);
    header[3] = address;
    header[4] = protocol;

    SSCP_SCR16(&header[1], 4, command, commandSz, crcA);

    /* Send */
    /* ---- */

    rc = SSCP_SerialSend(ctx, header, sizeof(header));
    if (rc)
        return rc;

    rc = SSCP_SerialSend(ctx, command, commandSz);
    if (rc)
        return rc;

    rc = SSCP_SerialSend(ctx, crcA, sizeof(crcA));
    if (rc)
        return rc;    

    /* Recv */
    /* ---- */

    rc = SSCP_SerialRecv(ctx, header, sizeof(header));
    if (rc)
        return rc;

    if (header[0] != 0x02)
        return SSCP_ERR_WRONG_RESPONSE_COMMAND;
    length = header[1];
    length <<= 8;
    length |= header[2];

    if (length > maxResponseSz) /* Payload will not fit */
        return SSCP_ERR_RESPONSE_TOO_LONG;

    /* Set the timeouts */
    rc = SSCP_SerialSetTimeouts(ctx, SSCP_RESPONSE_NEXT_TIMEOUT, SSCP_RESPONSE_NEXT_TIMEOUT);
    if (rc)
        return rc;

    rc = SSCP_SerialRecv(ctx, response, length);
    if (rc)
    {
        if (rc == SSCP_ERR_COMM_RECV_MUTE)
            rc = SSCP_ERR_COMM_RECV_STOPPED; /* We already have the header, right? */
        return rc;
    }

    rc = SSCP_SerialRecv(ctx, crcB, sizeof(crcB));
    if (rc)
    {
        if (rc == SSCP_ERR_COMM_RECV_MUTE)
            rc = SSCP_ERR_COMM_RECV_STOPPED; /* We already have the header and the payload, right? */
        return rc;
    }

    /* Check CRC */
    /* --------- */

    SSCP_SCR16(&header[1], 4, response, length, crcA);
    if (memcmp(crcA, crcB, 2))
        return SSCP_ERR_WRONG_RESPONSE_CRC;

    if (actResponseSz != NULL)
        *actResponseSz = length;

    return SSCP_SUCCESS;
}

static LONG SSCP_ExchangeEx(SSCP_CTX_ST* ctx, DWORD commandHeader, const BYTE commandData[], DWORD commandDataSz, BYTE responseData[], DWORD maxResponseDataSz, DWORD *actResponseDataSz, BOOL selftest)
{
    BYTE padding[16] = { 0 };
    BYTE initVector[16] = { 0 };
    BYTE commandType = (BYTE)(commandHeader >> 16);
    WORD commandCode = (WORD)(commandHeader);
    DWORD maxCommandSz = 0;
    DWORD commandSz = 0;
    BYTE* command = NULL;
    const DWORD maxResponseSz = 4096;
    DWORD responseSz = 0;
    BYTE *response = NULL;
    BYTE responseCode;
    DWORD t, i;
    LONG rc;

    if (ctx == NULL)
        return SSCP_ERR_INVALID_CONTEXT;
    if ((commandData == NULL) && (commandDataSz > 0))
        return SSCP_ERR_INVALID_PARAMETER;
    if (commandDataSz > 4096)
        return SSCP_ERR_COMMAND_TOO_LONG;

    maxCommandSz = 4 + 1 + 2 + 2 + commandDataSz + 32 + 16 + 16; /* 16 for padding + 16 for IV */

    /* Allocated the two buffers */
    command = calloc(1, maxCommandSz);
    if (command == NULL)
        return SSCP_ERR_OUT_OF_MEMORY;
    response = calloc(1, maxResponseSz);
    if (response == NULL)
    {
        free(command);
        return SSCP_ERR_OUT_OF_MEMORY;
    }

    /* Prepare the command */
    command[commandSz++] = (BYTE)(ctx->counter >> 24);
    command[commandSz++] = (BYTE)(ctx->counter >> 16);
    command[commandSz++] = (BYTE)(ctx->counter >> 8);
    command[commandSz++] = (BYTE)(ctx->counter);
    command[commandSz++] = commandType;
    command[commandSz++] = (BYTE)(commandCode >> 8);
    command[commandSz++] = (BYTE)(commandCode);
    command[commandSz++] = (BYTE)(commandDataSz >> 8);
    command[commandSz++] = (BYTE)(commandDataSz);
    if (commandData != NULL)
    {
        memcpy(&command[commandSz], commandData, commandDataSz);
        commandSz += commandDataSz;
    }

    if (SSCP_DEBUG_EXCHANGE)
    {
        SSCP_Trace("Command=");
        for (i = 0; i < commandSz; i++)
            SSCP_Trace("%02X", command[i]);
        SSCP_Trace("\n");
    }

    /* Compute the signature of the command */
    if (!SSCP_HMAC(ctx->sessionKeySignAB, command, commandSz, &command[commandSz]))
    {
        rc = SSCP_ERR_INTERNAL_FAILURE;
        goto failed;
    }

    if (SSCP_DEBUG_EXCHANGE)
    {
        SSCP_Trace("Sign=   ");
        for (i = 0; i < 32; i++)
            SSCP_Trace("%02X", command[commandSz + i]);
        SSCP_Trace("\n");
    }

    commandSz += 32;

    /* Padd the command to reach a multiple of 16 bytes */
    if (selftest)
    {
        static const BYTE PADD[4] = { 0xBA, 0x40, 0x5E, 0xDD };
        i = 0;
        while ((commandSz % 16) != 0)
            command[commandSz++] = PADD[i++ % sizeof(PADD)];
    }
    else
    {
        /* Standard padding */
        if ((commandSz % 16) != 0)
            command[commandSz++] = 0x80;
        while ((commandSz % 16) != 0)
            command[commandSz++] = 0x00;
    }

    if (SSCP_DEBUG_EXCHANGE)
    {
        SSCP_Trace("Padded= ");
        for (i = 0; i < commandSz; i++)
            SSCP_Trace("%02X", command[i]);
        SSCP_Trace("\n");
    }

    if (selftest)
    {
        static const BYTE IV[16] = { 0x7C, 0x3D, 0xE3, 0xF3, 0xE1, 0x91, 0xD3, 0xCD, 0x3A, 0x09, 0x3E, 0x64, 0x3B, 0xF0, 0x35, 0xCE };
        memcpy(initVector, IV, 16);
    }
    else
    {
        /* Randomize the Init Vector */
        if (!SSCP_GetRandom(initVector, 16))
        {
            rc = SSCP_ERR_INTERNAL_FAILURE;
            goto failed;
        }
    }

    /* Encrypt the command */
    if (!SSCP_Cipher(ctx->sessionKeyCipherAB, initVector, command, commandSz))
    {
        rc = SSCP_ERR_INTERNAL_FAILURE;
        goto failed;
    }

    if (SSCP_DEBUG_EXCHANGE)
    {
        SSCP_Trace("Crypted=");
        for (i = 0; i < commandSz; i++)
            SSCP_Trace("%02X", command[i]);
        SSCP_Trace("\n");
    }

    /* Don't forget to append the IV at the end */
    memcpy(&command[commandSz], initVector, 16);
    commandSz += 16;

    if (SSCP_DEBUG_EXCHANGE)
    {
        SSCP_Trace("Sending=");
        for (i = 0; i < commandSz; i++)
            SSCP_Trace("%02X", command[i]);
        SSCP_Trace("\n");
    }


    if (selftest)
    {
        static const BYTE R[] = {
            0xEE, 0x3F, 0x77, 0x22, 0x6E, 0x77, 0xEF, 0xF3, 0x05, 0x89, 0xBB, 0x40, 0xF1, 0xA1, 0x7C, 0x8E,
            0x6D, 0x7B, 0x5D, 0x89, 0xFB, 0x6D, 0x86, 0xF2, 0x52, 0x04, 0xFC, 0x4D, 0x31, 0x80, 0x0F, 0x17,
            0x7F, 0xED, 0xA6, 0x42, 0x00, 0x8F, 0x0A, 0x60, 0x37, 0x01, 0xC4, 0x34, 0xC8, 0x56, 0x9B, 0xA9,
            0xEC, 0x89, 0xEC, 0xA7, 0xB6, 0x33, 0xF3, 0x35, 0x77, 0xCE, 0xC2, 0x4A, 0x74, 0x85, 0x98, 0x5E
        };
        memcpy(response, R, sizeof(R));
        responseSz = sizeof(R);
        rc = SSCP_SUCCESS;
    }
    else
    {
        /* Send the command (and get the response) */
        BYTE retry;

        for (retry = 0; retry < SSCP_MAX_TIMEOUT_RETRY; retry++)
        {
            rc = SSCP_ExchangeRaw(ctx, ctx->address, SSCP_PROTOCOL_SECURE, command, commandSz, response, maxResponseSz, &responseSz);
            if (rc == SSCP_SUCCESS)
            {
                if (retry > 0)
                    ctx->stats.errorCount++; /* We have recovered this error */
                break;
            }
            if ((rc != SSCP_ERR_COMM_RECV_MUTE) && (rc != SSCP_ERR_COMM_RECV_STOPPED))
                break; /* Not a timeout error? So fatal! */
        }
    }

    if (rc)
        goto failed;

    if (SSCP_DEBUG_EXCHANGE)
    {
        SSCP_Trace("Received=");
        for (i = 0; i < responseSz; i++)
            SSCP_Trace("%02X", response[i]);
        SSCP_Trace("\n");
    }

    /* Verify that the length is correct */
    if ((responseSz < 16) || ((responseSz % 16) != 0))
    {
        rc = SSCP_ERR_WRONG_RESPONSE_LENGTH;
        goto failed;
    }

    /* Extract the init vector */
    responseSz -= 16;
    memcpy(initVector, &response[responseSz], 16);

    /* Decrypt the response */
    if (!SSCP_Decipher(ctx->sessionKeyCipherBA, initVector, response, responseSz))
    {
        rc = SSCP_ERR_INTERNAL_FAILURE;
        goto failed;
    }

    if (SSCP_DEBUG_EXCHANGE)
    {
        SSCP_Trace("Decrypted=");
        for (i = 0; i < responseSz; i++)
            SSCP_Trace("%02X", response[i]);
        SSCP_Trace("\n");
    }

    /* Verify the counter */
    t = response[0];
    t <<= 8;
    t |= response[1];
    t <<= 8;
    t |= response[2];
    t <<= 8;
    t |= response[3];
    
    if (t > ctx->counter)
    {
        /* Counter has been incremented by the device */
        ctx->counter = t + 1;
    }
    else
    {
        /* Counter has not been incremented by the device */
        if (SSCP_DEBUG_EXCHANGE)
            SSCP_Trace("Invalid response, current counter is %d, received %d\n", ctx->counter, t);
        rc = SSCP_ERR_WRONG_RESPONSE_COUNTER;
        goto failed;
    }

    /* Verify the opcode */
    if ((response[4] != (BYTE)(commandCode >> 8)) || (response[5] != (BYTE)(commandCode)))
    {
        if (SSCP_DEBUG_EXCHANGE)
            SSCP_Trace("Invalid response, sent command %04X, received %02X%02X\n", commandCode, response[4], response[5]);
        rc = SSCP_ERR_WRONG_RESPONSE_COMMAND;
        goto failed;
    }

    /* Gather the length */
    t = response[6];
    t <<= 8;
    t |= response[7];

    /* Is the length correct? */
    if ((responseSz < 4 + 2 + 2 + t + 2 + 32) || (responseSz > 4 + 2 + 2 + t + 2 + 32 + 16))
    {
        if (SSCP_DEBUG_EXCHANGE)
            SSCP_Trace("Invalid response, expected length >= %d and < %d, received %d\n", 4 + 2 + 2 + t + 2 + 32, 4 + 2 + 2 + t + 2 + 32 + 16, responseSz);
        rc = SSCP_ERR_WRONG_RESPONSE_FORMAT;
        goto failed;
    }

    responseSz = 8 + t + 2;

    if (SSCP_DEBUG_EXCHANGE)
    {
        SSCP_Trace("Counter+Data+Status=");
        for (i = 0; i < responseSz; i++)
            SSCP_Trace("%02X", response[i]);
        SSCP_Trace("\n");
    }

    /* Check the HMAC */
    {
        BYTE hmac[32];
        if (!SSCP_HMAC(ctx->sessionKeySignBA, response, responseSz, hmac))
        {
            if (SSCP_DEBUG_EXCHANGE)
                SSCP_Trace("Failed to verify HMAC in Exchange\n");
            rc = SSCP_ERR_INTERNAL_FAILURE;
            goto failed;
        }

        if (memcmp(hmac, &response[responseSz], 32))
        {
            if (SSCP_DEBUG_EXCHANGE)
            {
                SSCP_Trace("Wrong HMAC in Exchange\n");
                SSCP_Trace("Received: ");
                for (i = 0; i < 32; i++)
                    SSCP_Trace("%02X", response[i]);
                SSCP_Trace("\n");
                SSCP_Trace("Computed: ");
                for (i = 0; i < 32; i++)
                    SSCP_Trace("%02X", hmac[i]);
                SSCP_Trace("\n");
            }

            rc = SSCP_ERR_WRONG_RESPONSE_SIGNATURE;
            goto failed;
        }
    }

    /* Verify the status type */
    if (response[responseSz - 2] != commandType)
    {
        if (SSCP_DEBUG_EXCHANGE)
            SSCP_Trace("Wrong Response Type after Exchange\n");
        rc = SSCP_ERR_WRONG_RESPONSE_TYPE;
        goto failed;
    }

    /* Remember the status code */
    responseCode = response[responseSz - 1];

    /* Remember the length */
    if (actResponseDataSz != NULL)
        *actResponseDataSz = t;

    /* Can we retrieve the length? */
    if (t > 0)
    {
        if (t > maxResponseDataSz)        
        {
            rc = SSCP_ERR_OUTPUT_BUFFER_OVERFLOW;
            goto failed;
        }
        if (responseData != NULL)
        {
            memcpy(responseData, &response[8], t);
        }
    }

    /* Done with both buffers */
    if (response != NULL)
        free(response);
    if (command != NULL)
        free(command);

    if (responseCode != 0)
    {
        if (SSCP_DEBUG_EXCHANGE)
            SSCP_Trace("Exchange returns error %02X\n", responseCode);
        return responseCode;
    }

    return SSCP_SUCCESS;

failed:
    if (response != NULL)
        free(response);
    if (command != NULL)
        free(command);
    return rc;
}

LONG SSCP_Exchange(SSCP_CTX_ST* ctx, DWORD commandHeader, const BYTE commandData[], DWORD commandDataSz, BYTE responseData[], DWORD maxResponseDataSz, DWORD* actResponseDataSz)
{
    return SSCP_ExchangeEx(ctx, commandHeader, commandData, commandDataSz, responseData, maxResponseDataSz, actResponseDataSz, FALSE);
}

LONG SSCP_Exchange_SelfTest(SSCP_CTX_ST* ctx, DWORD commandHeader, const BYTE commandData[], DWORD commandDataSz, BYTE responseData[], DWORD maxResponseDataSz, DWORD* actResponseDataSz)
{
    return SSCP_ExchangeEx(ctx, commandHeader, commandData, commandDataSz, responseData, maxResponseDataSz, actResponseDataSz, TRUE);
}

LONG SSCP_Exchange_NoDataIn(SSCP_CTX_ST* ctx, DWORD commandHeader, BYTE responseData[], DWORD maxResponseDataSz, DWORD* actResponseDataSz)
{
    return SSCP_ExchangeEx(ctx, commandHeader, NULL, 0, responseData, maxResponseDataSz, actResponseDataSz, FALSE);
}

LONG SSCP_Exchange_NoDataOut(SSCP_CTX_ST* ctx, DWORD commandHeader, const BYTE commandData[], DWORD commandDataSz)
{
    return SSCP_ExchangeEx(ctx, commandHeader, commandData, commandDataSz, NULL, 0, NULL, FALSE);
}

LONG SSCP_Exchange_NoDataInOut(SSCP_CTX_ST* ctx, DWORD commandHeader)
{
    return SSCP_ExchangeEx(ctx, commandHeader, NULL, 0, NULL, 0, NULL, FALSE);
}


