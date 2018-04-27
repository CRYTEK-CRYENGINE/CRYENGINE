// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRY_SHA1_H__
#define __CRY_SHA1_H__

namespace CrySHA1
{
void sha1Calc(const char* str, uint32* digest);
void sha1Calc(const unsigned char* str, uint64 length, uint32* digest);
};

#endif // __CRY_SHA1_H__
