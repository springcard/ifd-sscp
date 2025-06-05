#ifndef __SSCP_CRYPTO_I_H__
#define __SSCP_CRYPTO_I_H__

#include "sscp-host_i.h"

#define SHA256_BLOCK_SIZE 64  	// SHA256 works on 64 byte blocks
#define SHA256_DIGEST_SIZE 32	// SHA256 outputs a 32 byte digest

typedef struct
{
	DWORD length;
	DWORD state[8];
	unsigned long curlen;
	BYTE buf[64];
} SHA256_CTX_ST;

void SHA256_Init(SHA256_CTX_ST* ctx);
void SHA256_Update(SHA256_CTX_ST* ctx, const BYTE data[], size_t len);
void SHA256_Final(SHA256_CTX_ST* ctx, BYTE hash[SHA256_DIGEST_SIZE]);

typedef struct
{
	DWORD key_bits;		/* Size of the key (bits)                */
	DWORD rounds;		/* Key-length-dependent number of rounds */
	DWORD enc_schd[60];	/* Key schedule                          */
	DWORD dec_schd[60];	/* Key schedule                          */
} AES_CTX_ST;

void AES_Init(AES_CTX_ST* aes_ctx, const BYTE key[16]);
void AES_Encrypt(AES_CTX_ST* aes_ctx, BYTE data[16]);
void AES_Encrypt2(AES_CTX_ST* aes_ctx, BYTE outbuf[16], const BYTE inbuf[16]);
void AES_Decrypt(AES_CTX_ST* aes_ctx, BYTE data[16]);
void AES_Decrypt2(AES_CTX_ST* aes_ctx, BYTE outbuf[16], const BYTE inbuf[16]);

#endif
