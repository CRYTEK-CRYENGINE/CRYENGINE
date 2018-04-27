// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/****************************************************
   A simple stream cipher based on RC4
****************************************************/

#ifndef __STREAMCIPHER_H__
#define __STREAMCIPHER_H__

class CStreamCipher
{
public:
	CStreamCipher();
	~CStreamCipher();

	void Init(const uint8* key, int keyLen);
	void Encrypt(const uint8* input, int inputLen, uint8* output)       { ProcessBuffer(input, inputLen, output, true); }
	void Decrypt(const uint8* input, int inputLen, uint8* output)       { ProcessBuffer(input, inputLen, output, true); }
	void EncryptStream(const uint8* input, int inputLen, uint8* output) { ProcessBuffer(input, inputLen, output, false); }
	void DecryptStream(const uint8* input, int inputLen, uint8* output) { ProcessBuffer(input, inputLen, output, false); }

private:
	uint8 GetNext();
	void  ProcessBuffer(const uint8* input, int inputLen, uint8* output, bool resetKey);

	uint8 m_StartS[256];
	uint8 m_S[256];
	int   m_StartI;
	int   m_I;
	int   m_StartJ;
	int   m_J;
};

#endif
