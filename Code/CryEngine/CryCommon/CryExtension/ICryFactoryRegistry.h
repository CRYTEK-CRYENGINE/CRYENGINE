// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ICryFactoryRegistry.h
//  Version:     v1.00
//  Created:     02/25/2009 by CarstenW
//  Description: Part of CryEngine's extension framework.
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef _ICRYFACTORYREGISTRY_H_
#define _ICRYFACTORYREGISTRY_H_

#pragma once

#include "CryTypeID.h"

struct ICryFactory;

struct ICryFactoryRegistry
{
	virtual ICryFactory* GetFactory(const CryClassID& cid) const = 0;
	virtual void         IterateFactories(const CryInterfaceID& iid, ICryFactory** pFactories, size_t& numFactories) const = 0;

protected:
	//! Prevent explicit destruction from client side (delete, shared_ptr, etc).
	virtual ~ICryFactoryRegistry() {}
};

#endif // #ifndef _ICRYFACTORYREGISTRY_H_
