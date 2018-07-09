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
#ifndef __AUTHENTICATION_H__
#define __AUTHENTICATION_H__

#pragma once

#include "Cryptography/Whirlpool.h"

// this is a "salt" parameter... we send it to a client, who hashes their
// password with the parameters contained herein, and check this hash
// to check the client has the correct password
struct SAuthenticationSalt : public ISerializable
{
	SAuthenticationSalt();
	void           SerializeWith(TSerialize ser);
	CWhirlpoolHash Hash(const string& password) const;
	float  fTime;
	uint32 nRand;
};

#endif
