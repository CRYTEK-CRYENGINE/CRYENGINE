// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CryTypeID.h"

struct ICryUnknown;
struct SRegFactoryNode;

typedef std::shared_ptr<ICryUnknown> ICryUnknownPtr;
typedef std::shared_ptr<const ICryUnknown> ICryUnknownConstPtr;

struct ICryFactory
{
	virtual const char*            GetName() const = 0;
	virtual const CryClassID&      GetClassID() const = 0;
	virtual bool                   ClassSupports(const CryInterfaceID& iid) const = 0;
	virtual void                   ClassSupports(const CryInterfaceID*& pIIDs, size_t& numIIDs) const = 0;
	virtual ICryUnknownPtr         CreateClassInstance() const = 0;

protected:
	//! Prevent explicit destruction from client side (delete, shared_ptr, etc).
	virtual ~ICryFactory() = default;
};
