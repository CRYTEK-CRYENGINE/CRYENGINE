// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <cstdio>
#include <cstdlib>
#include <cstdint>

void usage()
{
	fprintf(stderr, "KeyImport.exe - combine public and private key header files into a key.dat - Usage:\n");
	fprintf(stderr, "KeyImport.exe\n");
	fprintf(stderr, "It will produce a single output: 'key.dat'.\n");
}

int main()
{
	FILE *keyFile = fopen("key.dat", "wb");
	if (!keyFile)
	{
		fprintf(stderr, "Cannot open key.dat.");
		usage();
		exit(EXIT_FAILURE);
	}

	#include "key_public.h"
	#include "key_private.h"

	uint32_t keylength = sizeof(g_rsa_public_key_data);
	fwrite(&keylength, sizeof(keylength), 1, keyFile);
	fwrite(g_rsa_public_key_data, sizeof(g_rsa_public_key_data), 1, keyFile);

	keylength = sizeof(g_rsa_private_key_data);
	fwrite(&keylength, sizeof(keylength), 1, keyFile);
	fwrite(g_rsa_private_key_data, sizeof(g_rsa_private_key_data), 1, keyFile);

	fclose(keyFile);
	exit(EXIT_SUCCESS);
}
