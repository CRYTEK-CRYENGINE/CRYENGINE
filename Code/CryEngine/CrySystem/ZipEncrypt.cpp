// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "System.h"
#include "ZipEncrypt.h"
#include <CryCore/smartptr.h>
#include <zlib.h>
#include "ZipFileFormat.h"
#include "ZipDirStructures.h"

//////////////////////////////////////////////////////////////////////////
#ifdef INCLUDE_LIBTOMCRYPT

prng_state g_yarrow_prng_state;
// Main public RSA key used for verifying Cry Pak comments
rsa_key g_rsa_key_public_for_sign;

// Until we have attempted to initialize ZipEncrypt, there is no point in warning about possible initialization failure
bool g_using_zipencrypt = false;

	#define mp_count_bits(a)        ltc_mp.count_bits(a)
	#define mp_unsigned_bin_size(a) ltc_mp.unsigned_size(a)

void ZipEncrypt::Init(const uint8* pKeyData, uint32 keyLen)
{
	LOADING_TIME_PROFILE_SECTION;

	g_using_zipencrypt = true;

	ltc_mp = ltm_desc;
	register_hash(&sha1_desc);
	register_hash(&sha256_desc);

	register_cipher(&twofish_desc);

	int prng_idx = register_prng(&yarrow_desc) != -1;
	assert(prng_idx != -1);
	rng_make_prng(128, find_prng("yarrow"), &g_yarrow_prng_state, NULL);

	int importReturn = rsa_import(pKeyData, (unsigned long)keyLen, &g_rsa_key_public_for_sign);
	if (CRYPT_OK != importReturn)
	{
	#if !defined(_RELEASE)
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "RSA Public Key failed to initialize. Returned %d", importReturn);
	#endif //_RELEASE
	}
}

bool ZipEncrypt::StartStreamCipher(unsigned char key[16], unsigned char IV[16], symmetric_CTR* pCTR, const unsigned int offset)
{
	int err;

	int cipher_idx = find_cipher(STREAM_CIPHER_NAME);
	if (cipher_idx < 0)
		return false;

	err = ctr_start(cipher_idx, IV, key, 16, 0, CTR_COUNTER_LITTLE_ENDIAN, pCTR);
	if (err != CRYPT_OK)
	{
		//printf("ctr_start error: %s\n",error_to_string(errno));
		return false;
	}

	// Seek forward into the stream cipher by offset bytes
	unsigned int offset_blocks = offset / pCTR->blocklen;
	unsigned int offset_remaining = offset - (offset_blocks * pCTR->blocklen);

	if (offset_blocks > 0)
	{
		SwapEndian((uint32*)(&pCTR->ctr[0]), 4);
		*((uint32*)(&pCTR->ctr[0])) += offset_blocks;
		SwapEndian((uint32*)(&pCTR->ctr[0]), 4);

		ctr_setiv(pCTR->ctr, pCTR->ctrlen, pCTR);
	}

	// Seek into the last block to initialize the padding
	unsigned int bytesConsumed = 0;
	while (bytesConsumed < offset_remaining)
	{
		const static unsigned int bufSize = 1024;
		unsigned char buffer[bufSize] = { 0 };
		unsigned int bytesToConsume = min(bufSize, offset_remaining - bytesConsumed);
		ctr_decrypt(buffer, buffer, bytesToConsume, pCTR);
		bytesConsumed += bytesToConsume;
	}

	return true;
}

void ZipEncrypt::FinishStreamCipher(symmetric_CTR* pCTR)
{
	ctr_done(pCTR);
}

bool ZipEncrypt::DecryptBufferWithStreamCipher(unsigned char* inBuffer, unsigned char* outBuffer, size_t bufferSize, symmetric_CTR* pCTR)
{
	int err;
	err = ctr_decrypt(inBuffer, outBuffer, bufferSize, pCTR);
	if (err != CRYPT_OK)
	{
		//printf("ctr_encrypt error: %s\n", error_to_string(errno));
		return false;
	}
	return true;
}

bool ZipEncrypt::DecryptBufferWithStreamCipher(unsigned char* inBuffer, size_t bufferSize, unsigned char key[16], unsigned char IV[16])
{
	LOADING_TIME_PROFILE_SECTION

	symmetric_CTR ctr;

	if (!StartStreamCipher(key, IV, &ctr))
	{
		return false;
	}

	if (!DecryptBufferWithStreamCipher(inBuffer, inBuffer, bufferSize, &ctr))
	{
		return false;
	}

	ctr_done(&ctr);

	return true;
}

int ZipEncrypt::GetEncryptionKeyIndex(const ZipDir::FileEntry* pFileEntry)
{
	return (~(pFileEntry->desc.lCRC32 >> 2)) & 0xF;
}

void ZipEncrypt::GetEncryptionInitialVector(const ZipDir::FileEntry* pFileEntry, unsigned char IV[16])
{
	uint32 intIV[4]; //16 byte

	intIV[0] = pFileEntry->desc.lSizeUncompressed ^ (pFileEntry->desc.lSizeCompressed << 12);
	intIV[1] = (!pFileEntry->desc.lSizeCompressed);
	intIV[2] = pFileEntry->desc.lCRC32 ^ (pFileEntry->desc.lSizeCompressed << 12);
	intIV[3] = !pFileEntry->desc.lSizeUncompressed ^ pFileEntry->desc.lSizeCompressed;

	memcpy(IV, intIV, sizeof(intIV));
}

//////////////////////////////////////////////////////////////////////////
bool ZipEncrypt::RSA_VerifyData(void* inBuffer, int sizeIn, unsigned char* signedHash, int signedHashSize, rsa_key& publicKey)
{
	// verify hash

	int sha256 = find_hash("sha256");
	if (sha256 == -1)
	{
	#if !defined(_RELEASE)
		if (g_using_zipencrypt)
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR_DBGBRK, "Hash program for RSA_VerifyData could not be found. LibTomCrypt has failed to start.");
	#endif
		return false;
	}

	int hashSize = 32; // 32 bytes for SHA 256

	unsigned char hash_digest[1024]; // 32 bytes should be enough

	hash_state md;
	hash_descriptor[sha256].init(&md);
	hash_descriptor[sha256].process(&md, (unsigned char*)inBuffer, sizeIn);
	hash_descriptor[sha256].done(&md, hash_digest); // 32 bytes

	assert(hash_descriptor[sha256].hashsize == hashSize);

	int prng_idx = find_prng("yarrow");
	assert(prng_idx != -1);

	// Verify generated hash with RSA public key
	int statOut = 0;
	int res = rsa_verify_hash(signedHash, signedHashSize, hash_digest, hashSize, sha256, 0, &statOut, &publicKey);
	if (res != CRYPT_OK || statOut != 1)
	{
		return false;
	}
	return true;
}

bool ZipEncrypt::RSA_VerifyData(const unsigned char** inBuffers, unsigned int* sizesIn, const int numBuffers, unsigned char* signedHash, int signedHashSize, rsa_key& publicKey)
{
	// verify hash from multiple buffers

	int sha256 = find_hash("sha256");
	if (sha256 == -1)
	{
	#if !defined(_RELEASE)
		if (g_using_zipencrypt)
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR_DBGBRK, "Hash program for RSA_VerifyData could not be found. LibTomCrypt has failed to start.");
	#endif
		return false;
	}

	int hashSize = 32; // 32 bytes for SHA 256

	unsigned char hash_digest[1024]; // 32 bytes should be enough

	hash_state md;
	hash_descriptor[sha256].init(&md);
	for (int i = 0; i < numBuffers; i++)
	{
		hash_descriptor[sha256].process(&md, inBuffers[i], sizesIn[i]);
	}
	hash_descriptor[sha256].done(&md, hash_digest); // 32 bytes

	assert(hash_descriptor[sha256].hashsize == hashSize);

	int prng_idx = find_prng("yarrow");
	assert(prng_idx != -1);

	// Verify generated hash with RSA public key
	int statOut = 0;
	int res = rsa_verify_hash(signedHash, signedHashSize, hash_digest, hashSize, sha256, 0, &statOut, &publicKey);
	if (res != CRYPT_OK || statOut != 1)
	{
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
int ZipEncrypt::custom_rsa_encrypt_key_ex(const unsigned char* in, unsigned long inlen,
                                          unsigned char* out, unsigned long* outlen,
                                          const unsigned char* lparam, unsigned long lparamlen,
                                          prng_state* prng, int prng_idx, int hash_idx, int padding, rsa_key* key)
{
	unsigned long modulus_bitlen, modulus_bytelen, x;
	int err;

	CRY_ASSERT(in != NULL);
	CRY_ASSERT(out != NULL);
	CRY_ASSERT(outlen != NULL);
	CRY_ASSERT(key != NULL);

	/* valid padding? */
	if ((padding != LTC_LTC_PKCS_1_V1_5) &&
	    (padding != LTC_LTC_PKCS_1_OAEP))
	{
		return CRYPT_PK_INVALID_PADDING;
	}

	/* valid prng? */
	if ((err = prng_is_valid(prng_idx)) != CRYPT_OK)
	{
		return err;
	}

	if (padding == LTC_LTC_PKCS_1_OAEP)
	{
		/* valid hash? */
		if ((err = hash_is_valid(hash_idx)) != CRYPT_OK)
		{
			return err;
		}
	}

	/* get modulus len in bits */
	modulus_bitlen = mp_count_bits((key->N));

	/* outlen must be at least the size of the modulus */
	modulus_bytelen = mp_unsigned_bin_size((key->N));
	if (modulus_bytelen > *outlen)
	{
		*outlen = modulus_bytelen;
		return CRYPT_BUFFER_OVERFLOW;
	}

	if (padding == LTC_LTC_PKCS_1_OAEP)
	{
		/* OAEP pad the key */
		x = *outlen;
		if ((err = pkcs_1_oaep_encode(in, inlen, lparam,
		                              lparamlen, modulus_bitlen, prng, prng_idx, hash_idx,
		                              out, &x)) != CRYPT_OK)
		{
			return err;
		}
	}
	else
	{
		/* LTC_PKCS #1 v1.5 pad the key */
		x = *outlen;
		if ((err = pkcs_1_v1_5_encode(in, inlen, LTC_LTC_PKCS_1_EME,
		                              modulus_bitlen, prng, prng_idx,
		                              out, &x)) != CRYPT_OK)
		{
			return err;
		}
	}

	/* rsa exptmod the OAEP or LTC_PKCS #1 v1.5 pad */
	return ltc_mp.rsa_me(out, x, out, outlen, PK_PRIVATE, key);
}

//////////////////////////////////////////////////////////////////////////
int ZipEncrypt::custom_rsa_decrypt_key_ex(const unsigned char* in, unsigned long inlen,
                                          unsigned char* out, unsigned long* outlen,
                                          const unsigned char* lparam, unsigned long lparamlen,
                                          int hash_idx, int padding,
                                          int* stat, rsa_key* key)
{
	unsigned long modulus_bitlen, modulus_bytelen, x;
	int err;
	unsigned char* tmp;

	CRY_ASSERT(out != NULL);
	CRY_ASSERT(outlen != NULL);
	CRY_ASSERT(key != NULL);
	CRY_ASSERT(stat != NULL);

	/* default to invalid */
	*stat = 0;

	/* valid padding? */

	if ((padding != LTC_LTC_PKCS_1_V1_5) &&
	    (padding != LTC_LTC_PKCS_1_OAEP))
	{
		return CRYPT_PK_INVALID_PADDING;
	}

	if (padding == LTC_LTC_PKCS_1_OAEP)
	{
		/* valid hash ? */
		if ((err = hash_is_valid(hash_idx)) != CRYPT_OK)
		{
			return err;
		}
	}

	/* get modulus len in bits */
	modulus_bitlen = mp_count_bits((key->N));

	/* outlen must be at least the size of the modulus */
	modulus_bytelen = mp_unsigned_bin_size((key->N));
	if (modulus_bytelen != inlen)
	{
		return CRYPT_INVALID_PACKET;
	}

	/* allocate ram */
	tmp = (unsigned char*)XMALLOC(inlen);
	if (tmp == NULL)
	{
		return CRYPT_MEM;
	}

	/* rsa decode the packet */
	x = inlen;
	if ((err = ltc_mp.rsa_me(in, inlen, tmp, &x, PK_PUBLIC, key)) != CRYPT_OK)
	{
		XFREE(tmp);
		return err;
	}

	if (padding == LTC_LTC_PKCS_1_OAEP)
	{
		/* now OAEP decode the packet */
		err = pkcs_1_oaep_decode(tmp, x, lparam, lparamlen, modulus_bitlen, hash_idx,
		                         out, outlen, stat);
	}
	else
	{
		/* now LTC_PKCS #1 v1.5 depad the packet */
		err = pkcs_1_v1_5_decode(tmp, x, LTC_LTC_PKCS_1_EME, modulus_bitlen, out, outlen, stat);
	}

	XFREE(tmp);
	return err;
}

#endif //INCLUDE_LIBTOMCRYPT
//////////////////////////////////////////////////////////////////////////
