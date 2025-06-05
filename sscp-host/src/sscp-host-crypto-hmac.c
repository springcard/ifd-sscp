#include "sscp-host-crypto_i.h"

static void HMAC_SHA256_Init(SHA256_CTX_ST *sha256_ctx, const BYTE* key, BYTE key_size)
{
	BYTE ipad[SHA256_BLOCK_SIZE];
	BYTE i;

	memset(ipad, 0x36, SHA256_BLOCK_SIZE);

	for (i = 0; i < key_size; i++)
		ipad[i] ^= key[i];

	SHA256_Init(sha256_ctx);
	SHA256_Update(sha256_ctx, ipad, SHA256_BLOCK_SIZE);
}

static void HMAC_SHA256_Final(SHA256_CTX_ST* sha256_ctx, const BYTE* key, BYTE key_size, BYTE digest[SHA256_DIGEST_SIZE])
{
	BYTE opad[SHA256_BLOCK_SIZE];
	BYTE i;

	SHA256_Final(sha256_ctx, digest);
	memset(opad, 0x5c, SHA256_BLOCK_SIZE);
	for (i = 0; i < key_size; i++)
		opad[i] ^= key[i];

	SHA256_Init(sha256_ctx);
	SHA256_Update(sha256_ctx, opad, SHA256_BLOCK_SIZE);
	SHA256_Update(sha256_ctx, digest, SHA256_DIGEST_SIZE);
	SHA256_Final(sha256_ctx, digest);
}

BOOL SSCP_HMAC(const BYTE keyValue[16], const BYTE buffer[], DWORD length, BYTE hmac[32])
{
	SHA256_CTX_ST sha256_ctx;

	if (keyValue == NULL)
		return FALSE;
	if ((buffer == NULL) && (length > 0))
		return FALSE;
	if (hmac == NULL)
		return FALSE;

	HMAC_SHA256_Init(&sha256_ctx, keyValue, 16);
	SHA256_Update(&sha256_ctx, buffer, length);
	HMAC_SHA256_Final(&sha256_ctx, keyValue, 16, hmac);

	return TRUE;
}