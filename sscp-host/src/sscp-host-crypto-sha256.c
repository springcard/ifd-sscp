#include "sscp-host-crypto_i.h"

/*
 * SHA-256 hash implementation and interface functions
 * Copyright (c) 2003-2006, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#define WPA_GET_LE16(a) ((WORD) (((a)[1] << 8) | (a)[0]))
#define WPA_PUT_LE16(a, val)			\
	do {					\
		(a)[1] = ((WORD) (val)) >> 8;	\
		(a)[0] = ((WORD) (val)) & 0xff;	\
	} while (0)

#define WPA_GET_BE32(a) ((((DWORD) (a)[0]) << 24) | (((DWORD) (a)[1]) << 16) | \
			 (((DWORD) (a)[2]) << 8) | ((DWORD) (a)[3]))
#define WPA_PUT_BE32(a, val)				\
	do {						\
		(a)[0] = (BYTE) (((DWORD) (val)) >> 24);	\
		(a)[1] = (BYTE) (((DWORD) (val)) >> 16);	\
		(a)[2] = (BYTE) (((DWORD) (val)) >> 8);	\
		(a)[3] = (BYTE) (((DWORD) (val)) & 0xff);	\
	} while (0)

#define WPA_PUT_BE64(a, val)				\
	do {						\
		(a)[0] = 0;	\
		(a)[1] = 0;	\
		(a)[2] = 0;	\
		(a)[3] = 0;	\
		(a)[4] = (BYTE) (((DWORD) (val)) >> 24);	\
		(a)[5] = (BYTE) (((DWORD) (val)) >> 16);	\
		(a)[6] = (BYTE) (((DWORD) (val)) >> 8);	\
		(a)[7] = (BYTE) (((DWORD) (val)) & 0xff);	\
	} while (0)


 /* This is based on SHA256 implementation in LibTomCrypt that was released into
  * public domain by Tom St Denis. */
  /* the K array */
static const unsigned long K[64] = {
	0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL,
	0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL, 0xd807aa98UL, 0x12835b01UL,
	0x243185beUL, 0x550c7dc3UL, 0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL,
	0xc19bf174UL, 0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
	0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL, 0x983e5152UL,
	0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL,
	0x06ca6351UL, 0x14292967UL, 0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL,
	0x53380d13UL, 0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
	0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL,
	0xd6990624UL, 0xf40e3585UL, 0x106aa070UL, 0x19a4c116UL, 0x1e376c08UL,
	0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL,
	0x682e6ff3UL, 0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
	0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

/* Various logical functions */
#define RORc(x, y) \
( ((((unsigned long) (x) & 0xFFFFFFFFUL) >> (unsigned long) ((y) & 31)) | \
   ((unsigned long) (x) << (unsigned long) (32 - ((y) & 31)))) & 0xFFFFFFFFUL)
#define Ch(x,y,z)       (z ^ (x & (y ^ z)))
#define Maj(x,y,z)      (((x | y) & z) | (x & y))
#define S(x, n)         RORc((x), (n))
#define R(x, n)         (((x)&0xFFFFFFFFUL)>>(n))
#define Sigma0(x)       (S(x, 2) ^ S(x, 13) ^ S(x, 22))
#define Sigma1(x)       (S(x, 6) ^ S(x, 11) ^ S(x, 25))
#define Gamma0(x)       (S(x, 7) ^ S(x, 18) ^ R(x, 3))
#define Gamma1(x)       (S(x, 17) ^ S(x, 19) ^ R(x, 10))
#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif
/* compress 512-bits */
static int sha256_compress(SHA256_CTX_ST* ctx, unsigned char* buf)
{
	DWORD S[8], W[64], t0, t1;
	DWORD t;
	int i;

	/* copy state into S */
	for (i = 0; i < 8; i++)
	{
		S[i] = ctx->state[i];
	}
	/* copy the state into 512-bits into W[0..15] */
	for (i = 0; i < 16; i++)
		W[i] = WPA_GET_BE32(buf + (4 * i));

	/* fill W[16..63] */
	for (i = 16; i < 64; i++)
	{
		W[i] = Gamma1(W[i - 2]) + W[i - 7] + Gamma0(W[i - 15]) + W[i - 16];
	}
	/* Compress */
#define RND(a,b,c,d,e,f,g,h,i)                          \
	t0 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i];	\
	t1 = Sigma0(a) + Maj(a, b, c);			\
	d += t0;					\
	h  = t0 + t1;
	for (i = 0; i < 64; ++i)
	{
		RND(S[0], S[1], S[2], S[3], S[4], S[5], S[6], S[7], i);
		t = S[7];
		S[7] = S[6];
		S[6] = S[5];
		S[5] = S[4];
		S[4] = S[3];
		S[3] = S[2];
		S[2] = S[1];
		S[1] = S[0];
		S[0] = t;
	}
	/* feedback */
	for (i = 0; i < 8; i++)
	{
		ctx->state[i] = ctx->state[i] + S[i];
	}
	return 0;
}

/* Initialize the hash state */
void SHA256_Init(SHA256_CTX_ST* ctx)
{
	ctx->curlen = 0;
	ctx->length = 0;
	ctx->state[0] = 0x6A09E667UL;
	ctx->state[1] = 0xBB67AE85UL;
	ctx->state[2] = 0x3C6EF372UL;
	ctx->state[3] = 0xA54FF53AUL;
	ctx->state[4] = 0x510E527FUL;
	ctx->state[5] = 0x9B05688CUL;
	ctx->state[6] = 0x1F83D9ABUL;
	ctx->state[7] = 0x5BE0CD19UL;
}

/**
   Process a block of memory though the hash
   @param ctx    The hash state
   @param data   The data to hash
   @param len    The length of the data (octets)
   @return CRYPT_OK if successful
*/
void SHA256_Update(SHA256_CTX_ST* ctx, const BYTE data[], size_t len)
{
	size_t n;

#define block_size 64
	if (ctx->curlen > sizeof(ctx->buf))
		return;
	while (len > 0)
	{
		if (ctx->curlen == 0 && len >= block_size)
		{
			if (sha256_compress(ctx, (unsigned char*)data) < 0)
				return;
			ctx->length += block_size * 8;
			data += block_size;
			len -= block_size;
		}
		else
		{
			n = MIN(len, (block_size - ctx->curlen));
			memcpy(ctx->buf + ctx->curlen, data, n);
			ctx->curlen += (DWORD) n;
			data += n;
			len -= n;
			if (ctx->curlen == block_size)
			{
				if (sha256_compress(ctx, ctx->buf) < 0)
					return;
				ctx->length += 8 * block_size;
				ctx->curlen = 0;
			}
		}
	}
}

/**
   Terminate the hash to get the digest
   @param ctx The hash state
   @param out [out] The destination of the hash (32 bytes)
   @return CRYPT_OK if successful
*/
void SHA256_Final(SHA256_CTX_ST* ctx, BYTE hash[SHA256_DIGEST_SIZE])
{
	int i;

	if (ctx->curlen >= sizeof(ctx->buf))
		return;

	/* increase the length of the message */
	ctx->length += ctx->curlen * 8;
	/* append the '1' bit */
	ctx->buf[ctx->curlen++] = (unsigned char)0x80;
	/* if the length is currently above 56 bytes we append zeros
	 * then compress.  Then we can fall back to padding zeros and length
	 * encoding like normal.
	 */
	if (ctx->curlen > 56)
	{
		while (ctx->curlen < 64)
		{
			ctx->buf[ctx->curlen++] = (unsigned char)0;
		}
		sha256_compress(ctx, ctx->buf);
		ctx->curlen = 0;
	}
	/* pad upto 56 bytes of zeroes */
	while (ctx->curlen < 56)
	{
		ctx->buf[ctx->curlen++] = (unsigned char)0;
	}
	/* store length */
	WPA_PUT_BE64(ctx->buf + 56, ctx->length);

	sha256_compress(ctx, ctx->buf);

	/* copy output */
	for (i = 0; i < 8; i++)
		WPA_PUT_BE32(hash + (4 * i), ctx->state[i]);

	return;
}

/* ===== end - public domain SHA256 implementation ===== */

