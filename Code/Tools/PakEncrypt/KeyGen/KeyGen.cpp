// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "../stdafx.h" // For key file name
#include <time.h>
#include <string>
#include <errno.h>
#include <direct.h>
#include "tchar.h"

//Template public key - Replace with actual key data
const char *szPublicKeyPattern ="#ifndef __PUBLICKEY__H__\n#define __PUBLICKEY__H__\nstatic unsigned char g_rsa_public_key_data[] =\n{\n%s\n};\n#endif";
const char *szPrivateKeyPattern ="#ifndef __PRIVATEKEY__H__\n#define __PRIVATEKEY__H__\nstatic unsigned char g_rsa_private_key_data[] =\n{\n%s\n};\n#endif";

//////////////////////////////////////////////////////////////////
// LibTom - Encryption
//////////////////////////////////////////////////////////////////

#include <tomcrypt.h>

#define mp_count_bits(a)             ltc_mp.count_bits(a)
#define mp_unsigned_bin_size(a)      ltc_mp.unsigned_size(a)

#define STREAM_CIPHER_NAME "twofish"
prng_state g_yarrow_prng_state;

int ExportKey(const char *szFilePattern, const unsigned char *szExportedKey, const unsigned long ulExportedKeyLength, const TCHAR *tszFileName)
{
	std::string sExportString;
	sExportString.reserve((ulExportedKeyLength*6)+1);	//allow for "0xNN,\n" per byte
	for(unsigned long byteIndex=0; byteIndex < ulExportedKeyLength; byteIndex++)
	{
		char szExportBuf[5];
		sprintf_s(szExportBuf, sizeof(szExportBuf), "0x%X%X", szExportedKey[byteIndex]>>4, szExportedKey[byteIndex]&0x0F);
		sExportString += szExportBuf;
		if(byteIndex < (ulExportedKeyLength-1))
		{
			sExportString += ",";
			if((byteIndex+1) % 20 == 0)
			{
				sExportString += "\n";
			}
		}
	}

	char szHeaderBuffer[4096];
	sprintf_s(szHeaderBuffer, sizeof(szHeaderBuffer), szFilePattern, sExportString.c_str());

	FILE *pHeaderFile;
	errno_t fopenResult = _tfopen_s(&pHeaderFile, tszFileName, _T("wb"));
	if(fopenResult != 0)
	{
		_ftprintf(stderr, _T("Failed to open %s to write: %d"), tszFileName, fopenResult);
		return 1;
	}
	size_t fwriteBytes = fwrite(szHeaderBuffer, 1, strlen(szHeaderBuffer), pHeaderFile);
	if(fwriteBytes < strlen(szHeaderBuffer))
	{
		_ftprintf(stderr, _T("Wrote an unexpected number of bytes to %s: %d (expected %d)"), tszFileName, fwriteBytes, strlen(szHeaderBuffer));
		return 1;
	}
	fclose(pHeaderFile);

	return 0;
}

void usage()
{
	fprintf(stderr, "KeyGen.exe - Usage:\n");
	fprintf(stderr, "KeyGen.exe prefix\n");
	fprintf(stderr, "Where prefix is the string prefixed to the exported public and private key header files.\n");
	fprintf(stderr, "A prefix of \"Crysis3_Dev\" would create Crysis3_Dev_PrivateKey.h and Crysis3_Dev_PublicKey.h in the ExportedKeys directory.\n");
	fprintf(stderr, "e.g.\n");
	fprintf(stderr, "KeyGen.exe \"Crysis3_Dev\"\n");
}

typedef std::basic_string<TCHAR> tstring;

int _tmain(int argc, _TCHAR* argv[])
{
	FILE *keyFile = fopen(EAAS_KEY_FILE, "rb");
	if (keyFile)
	{
		fclose(keyFile);
		fprintf(stderr, "A key file already exists, this program will not overwrite it to prevent accidental loss of keys\n");
		fprintf(stderr, "When you overwrite a key, you can no longer access the contents of existing encrypted pak files\n");
		fprintf(stderr, "If you are 100%% certain you want to create a new key, delete " EAAS_KEY_FILE " and run this tool again\n");
		return 1;
	}
	keyFile = fopen(EAAS_KEY_FILE, "wb");
	if (!keyFile)
	{
		fprintf(stderr, "Cannot open " EAAS_KEY_FILE " for writing, please check that you have sufficient privileges");
		return 1;
	}

	ltc_mp = ltm_desc;
	register_hash (&sha1_desc);
	register_hash (&sha256_desc);

	register_cipher (&twofish_desc);

	int prng_idx = register_prng(&yarrow_desc);
	//fprintf(stderr, "register_prng is %d\n", prng_idx);
	prng_idx = find_prng("yarrow");
	//fprintf(stderr, "prng_idx is %d\n", prng_idx);
	assert( prng_idx != -1 );
	int result = rng_make_prng(128, prng_idx, &g_yarrow_prng_state, NULL);
	if(result != CRYPT_OK)
	{
		fprintf(stderr, "Failed to make a PRNG in order to create an RSA key: %d", result);
		return 1;
	}

	//Generate an RSA key pair
	rsa_key generatedKey;
	result = rsa_make_key(&g_yarrow_prng_state, prng_idx, 128, 65537, &generatedKey);
	if(result != CRYPT_OK)
	{
		fprintf(stderr, "Failed to generate an RSA key for export: %d", result);
		return 1;
	}

	unsigned char exportBuffer[1024];
	unsigned long bufferLen = 1024;
	result = rsa_export(exportBuffer, &bufferLen, PK_PUBLIC, &generatedKey);
	if(result != CRYPT_OK)
	{
		fprintf(stderr, "Failed to export an RSA public key: %d", result);
		return 1;
	}

	//Test the public key import
	rsa_key testPublicKey;
	result = rsa_import(exportBuffer, bufferLen, &testPublicKey );
	rsa_free(&testPublicKey);
	if(result != CRYPT_OK)
	{
		fprintf(stderr, "Exported RSA public key failed to import as a test (should never happen): %d", result);
		return 1;
	}

	bool bPubLen = (bufferLen <= EAAS_PUB_SIZE) && (fwrite(&bufferLen, sizeof(bufferLen), 1, keyFile) == 1);
	bool bPubKey = bPubLen && (fwrite(exportBuffer, bufferLen, 1, keyFile) == 1);
	int exportResult = ExportKey(szPublicKeyPattern, exportBuffer, bufferLen, _T(EAAS_H_FILE));

	bufferLen = 1024;
	result = rsa_export(exportBuffer, &bufferLen, PK_PRIVATE, &generatedKey);
	if(result != CRYPT_OK)
	{
		fprintf(stderr, "Failed to export an RSA private key: %d", result);
		return 1;
	}

	rsa_key testPrivateKey;
	result = rsa_import(exportBuffer, bufferLen, &testPrivateKey );
	rsa_free(&testPrivateKey);
	if(result != CRYPT_OK)
	{
		fprintf(stderr, "Exported RSA private key failed to import as a test (should never happen): %d", result);
		return 1;
	}

	bool bPrivLen = bPubKey && (fwrite(&bufferLen, sizeof(bufferLen), 1, keyFile) == 1);
	bool bPrivKey = bPubKey && (bufferLen <= EAAS_PRIV_SIZE) && (fwrite(exportBuffer, bufferLen, 1, keyFile) == 1);
	fclose(keyFile);
	if (!bPubKey || !bPubLen || !bPrivLen || !bPrivKey || exportResult != 0)
	{
		keyFile = fopen(EAAS_KEY_FILE, "wb"); // Truncate in case of failure
		if (keyFile) fclose(keyFile);

		fprintf(stderr, "Failed to write key data to " EAAS_KEY_FILE ", check privileges and try again\n");
		return 1;
	}

	rsa_free(&generatedKey);

	return 0;
}

