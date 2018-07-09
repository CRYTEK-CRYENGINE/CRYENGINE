// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  Diffie-Hellman-Merkle Exponential Key Exchange Protocol
   -------------------------------------------------------------------------
   History:
   - 13/03/2007   : Created by Lin Luo
*************************************************************************/

#ifndef __EXPONENTIALKEYEXCHANGE_H__
#define __EXPONENTIALKEYEXCHANGE_H__

#pragma once

#include <CrySystem/IConsole.h>

class CExponentialKeyExchange
{
public:
	CExponentialKeyExchange();
	~CExponentialKeyExchange();

	static const size_t KEY_SIZE = 32;
	static const size_t KEY_SIZE_IN_BITS = KEY_SIZE * 8;

	struct KEY_TYPE
	{
		uint8 key[KEY_SIZE];
		KEY_TYPE() { memset(key, 0, sizeof(key)); }
	};

	void            Generate(KEY_TYPE& gg, KEY_TYPE& pp, KEY_TYPE& AA);
	void            Generate(KEY_TYPE& BB, const KEY_TYPE& gg, const KEY_TYPE& pp, const KEY_TYPE& AA);

	void            Calculate(const KEY_TYPE& BB);

	const KEY_TYPE& GetKey() const;

	static void     Test(IConsoleCmdArgs* pArgs);

private:
	static int RandFunc(unsigned char* buf, size_t nbytes, const unsigned char* seed, size_t slen);

	KEY_TYPE m_x;
	KEY_TYPE m_g;
	KEY_TYPE m_p;
	KEY_TYPE m_A;
	KEY_TYPE m_B;
	KEY_TYPE m_k;
};

#endif
