// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <cstdio>
#include <cstdlib>
#include <cstdint>

// Preamble to the keys themselves
const char *szPublicKeyPattern ="#ifndef __PUBLICKEY__H__\n#define __PUBLICKEY__H__\nstatic unsigned char g_rsa_public_key_data[] =\n{";
const char *szPrivateKeyPattern ="#ifndef __PRIVATEKEY__H__\n#define __PRIVATEKEY__H__\nstatic unsigned char g_rsa_private_key_data[] =\n{";

void usage()
{
	fprintf(stderr, "KeyExtract.exe - extract public and private keys from a key.dat file - Usage:\n");
	fprintf(stderr, "KeyExtract.exe\n");
	fprintf(stderr, "Must be in the same directory as a key.dat file created by KeyGen.\n");
	fprintf(stderr, "It will produce two header files: key_public.h and key_private.h.\n");
}

void export_key(FILE *keyFile, const char *keyPattern, const char *exportName)
{
	unsigned char exportBuffer[1024];
	uint32_t bufferLen = 1024;

	// Read the information from key.dat.
	fread(&bufferLen, sizeof(bufferLen), 1, keyFile);
	fread(exportBuffer, bufferLen, 1, keyFile);

	FILE *header = fopen(exportName, "w");
	if (!header)
	{
		fprintf(stderr, "Unable to open '%s' for writing, exiting.", exportName);
		exit(EXIT_FAILURE);
	}
	fprintf(header, keyPattern);
	for(int i=0; i!=bufferLen; ++i)
	{
		if ((i%20) == 0)
		{
			fprintf(header, "\n");
		}
		fprintf(header, "0x%02X", exportBuffer[i]);
		if (i != (bufferLen - 1))
		{
			fprintf(header, ",");
		}
	}
	fprintf(header, "\n};\n#endif\n");
	fclose(header);
}

int main()
{
	FILE *keyFile = fopen("key.dat", "rb");
	if (!keyFile)
	{
		fprintf(stderr, "Cannot open key.dat.");
		usage();
		exit(EXIT_FAILURE);
	}

	export_key(keyFile, szPublicKeyPattern, "key_public.h");
	export_key(keyFile, szPrivateKeyPattern, "key_private.h");

	fclose(keyFile);
	exit(EXIT_SUCCESS);
}
