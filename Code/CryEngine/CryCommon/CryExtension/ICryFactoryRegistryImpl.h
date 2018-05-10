// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ICryFactoryRegistryImpl.h
//  Version:     v1.00
//  Created:     02/25/2009 by CarstenW
//  Description: Part of CryEngine's extension framework.
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef _ICRYFACTORYREGISTRYIMPL_H_
#define _ICRYFACTORYREGISTRYIMPL_H_

#pragma once

#include <CryExtension/ICryFactoryRegistry.h>

struct SRegFactoryNode;

struct ICryFactoryRegistryCallback
{
	virtual void OnNotifyFactoryRegistered(ICryFactory* pFactory) = 0;
	virtual void OnNotifyFactoryUnregistered(ICryFactory* pFactory) = 0;

protected:
	virtual ~ICryFactoryRegistryCallback() {}
};

struct ICryFactoryRegistryImpl : public ICryFactoryRegistry
{
	virtual ICryFactory* GetFactory(const CryClassID& cid) const = 0;
	virtual void         IterateFactories(const CryInterfaceID& iid, ICryFactory** pFactories, size_t& numFactories) const = 0;

	virtual void         RegisterCallback(ICryFactoryRegistryCallback* pCallback) = 0;
	virtual void         UnregisterCallback(ICryFactoryRegistryCallback* pCallback) = 0;

	virtual void         RegisterFactories(const SRegFactoryNode* pFactories) = 0;
	virtual void         UnregisterFactories(const SRegFactoryNode* pFactories) = 0;

	virtual void         UnregisterFactory(ICryFactory* const pFactory) = 0;

protected:
	//! Prevent explicit destruction from client side (delete, shared_ptr, etc).
	virtual ~ICryFactoryRegistryImpl() {}
};

#endif // #ifndef _ICRYFACTORYREGISTRYIMPL_H_
