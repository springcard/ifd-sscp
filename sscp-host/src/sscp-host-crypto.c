#include "sscp-host-crypto_i.h"

BOOL SSCP_DEBUG_CRYPTO = FALSE;

BOOL SSCP_ComputeSessionKeys(SSCP_CTX_ST* ctx, const BYTE authKeyValue[16], const BYTE rndA[16], const BYTE rndB[16])
{
    BYTE Kp[16];
    BYTE W[16];
    BYTE T[64];
    DWORD i;

    if (ctx == NULL)
        return FALSE;
    if (authKeyValue == NULL)
        return FALSE;
    if (rndA == NULL)
        return FALSE;
    if (rndB == NULL)
        return FALSE;

    memcpy(Kp, authKeyValue, 16);
    memcpy(W, rndB, 16);

    /*
     * DON'T REVEAL THE AUTHENTICATION KEY !!!
    if (SSCP_DEBUG_CRYPTO)
    {
        SSCP_Trace("K =");
        for (i = 0; i < 16; i++)
            SSCP_Trace("%02X", Kp[i]);
        SSCP_Trace("\n");
    }
     */

    /* K' = AES (K, K) */
    {
        AES_CTX_ST aes_ctx;
        AES_Init(&aes_ctx, Kp);
        AES_Encrypt(&aes_ctx, Kp);
    }

    if (SSCP_DEBUG_CRYPTO)
    {
        SSCP_Trace("K'=");
        for (i = 0; i < 16; i++)
            SSCP_Trace("%02X", Kp[i]);
        SSCP_Trace("\n");
    }

    /* W = AES (K', RndB) */
    {
        AES_CTX_ST aes_ctx;
        AES_Init(&aes_ctx, Kp);
        AES_Encrypt(&aes_ctx, W);
    }

    if (SSCP_DEBUG_CRYPTO)
    {
        SSCP_Trace("W=");
        for (i = 0; i < 16; i++)
            SSCP_Trace("%02X", W[i]);
        SSCP_Trace("\n");
    }

    /* Buffer with Info1 (0x00000000 | W | Info1) */
    {
        SHA256_CTX_ST sha256_ctx;
        static const BYTE SSCP_INFO_1[] = { 0x02, 0x6A, 0x53, 0x82, 0xE6, 0x53 };
        BYTE buffer[4 + 16 + sizeof(SSCP_INFO_1)];

        memset(&buffer[0], 0, 4);
        memcpy(&buffer[4], W, 16);
        memcpy(&buffer[4 + 16], SSCP_INFO_1, sizeof(SSCP_INFO_1));

        if (SSCP_DEBUG_CRYPTO)
        {
            SSCP_Trace("B1=");
            for (i = 0; i < sizeof(buffer); i++)
                SSCP_Trace("%02X", buffer[i]);
            SSCP_Trace("\n");
        }

        SHA256_Init(&sha256_ctx);
        SHA256_Update(&sha256_ctx, buffer, sizeof(buffer));
        SHA256_Final(&sha256_ctx, &T[0]);
    }

    /* Buffer with Info2 (0x00000001 | W | Info2) */
    {
        SHA256_CTX_ST sha256_ctx;
        static const BYTE SSCP_INFO_2[] = { 0x02, 0x6A };
        BYTE buffer[4 + 16 + sizeof(SSCP_INFO_2)];

        memset(&buffer[0], 0, 4);
        buffer[3] = 0x01;
        memcpy(&buffer[4], W, 16);
        memcpy(&buffer[4 + 16], SSCP_INFO_2, sizeof(SSCP_INFO_2));

        if (SSCP_DEBUG_CRYPTO)
        {
            SSCP_Trace("B2=");
            for (i = 0; i < sizeof(buffer); i++)
                SSCP_Trace("%02X", buffer[i]);
            SSCP_Trace("\n");
        }

        SHA256_Init(&sha256_ctx);
        SHA256_Update(&sha256_ctx, buffer, sizeof(buffer));
        SHA256_Final(&sha256_ctx, &T[32]);
    }

    /* T = SHA256(0x00000000 | W | Info1) | Hash(0x00000001 | W | Info2) with Info1 = 0x026A5382E653 and Info2 = 0x026A */

    if (SSCP_DEBUG_CRYPTO)
    {
        SSCP_Trace("T=");
        for (i = 0; i < 64; i++)
            SSCP_Trace("%02X", T[i]);
        SSCP_Trace("\n");
    }

    /* Gather subkeys */
    memcpy(ctx->sessionKeyCipherAB, &T[0], 16);
    memcpy(ctx->sessionKeyCipherBA, &T[16], 16);
    memcpy(ctx->sessionKeySignAB, &T[32], 16);
    memcpy(ctx->sessionKeySignBA, &T[48], 16);

    if (SSCP_DEBUG_CRYPTO)
    {
        SSCP_Trace("Kcab=");
        for (i = 0; i < 16; i++)
            SSCP_Trace("%02X", ctx->sessionKeyCipherAB[i]);
        SSCP_Trace("\n");
        SSCP_Trace("Kcba=");
        for (i = 0; i < 16; i++)
            SSCP_Trace("%02X", ctx->sessionKeyCipherBA[i]);
        SSCP_Trace("\n");
        SSCP_Trace("Ksab=");
        for (i = 0; i < 16; i++)
            SSCP_Trace("%02X", ctx->sessionKeySignAB[i]);
        SSCP_Trace("\n");
        SSCP_Trace("Ksba=");
        for (i = 0; i < 16; i++)
            SSCP_Trace("%02X", ctx->sessionKeySignBA[i]);
        SSCP_Trace("\n");
    }

    return TRUE;
}

BOOL SSCP_Cipher(const BYTE keyValue[16], const BYTE initVector[16], BYTE buffer[], DWORD length)
{
    AES_CTX_ST aes_ctx;
    BYTE carry[16];
    DWORD i, j;

    if (keyValue == NULL)
        return FALSE;
    if (initVector == NULL)
        return FALSE;
    if (buffer == NULL)
        return FALSE;
    if ((length % 16) != 0)
        return FALSE;

    AES_Init(&aes_ctx, keyValue);

    memcpy(carry, initVector, 16);

    for (i = 0; i < length; i += 16)
    {
        /* Plain <- Plain XOR IV */
        for (j = 0; j < 16; j++)
            buffer[i + j] ^= carry[j];

        /* Cipher <- E ( Plain XOR IV ) */
        AES_Encrypt(&aes_ctx, &buffer[i]);

        /* IV <- Cipher */
        memcpy(carry, &buffer[i], 16);
    }

    return TRUE;
}

BOOL SSCP_Decipher(const BYTE keyValue[16], const BYTE initVector[16], BYTE buffer[], DWORD length)
{
    AES_CTX_ST aes_ctx;
    BYTE carry[16];
    DWORD i, j;

    if (keyValue == NULL)
        return FALSE;
    if (initVector == NULL)
        return FALSE;
    if (buffer == NULL)
        return FALSE;
    if ((length % 16) != 0)
        return FALSE;

    AES_Init(&aes_ctx, keyValue);

    memcpy(carry, initVector, 16);

    for (i = 0; i < length; i += 16)
    {
        BYTE next_carry[16];
        memcpy(next_carry, &buffer[i], 16);

        /* Plain XOR IV <- D ( Cipher ) */
        AES_Decrypt(&aes_ctx, &buffer[i]);

        /* Plain <- ( Plain XOR IV ) XOR IV */
        for (j = 0; j < 16; j++)
            buffer[i + j] ^= carry[j];

        /* IV <- Cipher */
        memcpy(carry, next_carry, 16);
    }

    return TRUE;
}
