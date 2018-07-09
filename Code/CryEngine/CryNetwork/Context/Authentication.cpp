// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  Helper classes for authentication
   -------------------------------------------------------------------------
   History:
   - 26/07/2004   : Created by Craig Tiller
*************************************************************************/
#include "StdAfx.h"
#include "Authentication.h"
#include <CrySystem/ITimer.h>

ILINE static uint32 GetRandomNumber()
{
	NetWarning("Using low quality random numbers: security compromised");
	return cry_random_uint32();
}

SAuthenticationSalt::SAuthenticationSalt() :
	fTime(gEnv->pTimer->GetCurrTime()),
	nRand(GetRandomNumber())
{
}

void SAuthenticationSalt::SerializeWith(TSerialize ser)
{
	ser.Value("fTime", fTime);
	ser.Value("nRand", nRand);
}

CWhirlpoolHash SAuthenticationSalt::Hash(const string& password) const
{
	char n1[32];
	char n2[32];
	cry_sprintf(n1, "%f", fTime);
	cry_sprintf(n2, "%.8x", nRand);
	string buffer = password + ":" + n1 + ":" + n2;
	return CWhirlpoolHash(buffer);
}
