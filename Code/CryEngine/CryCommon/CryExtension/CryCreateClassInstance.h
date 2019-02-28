// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CryCreateClassInstance.h
//  Version:     v1.00
//  Created:     02/25/2009 by CarstenW
//  Description: Part of CryEngine's extension framework.
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef _CRYCREATECLASSINSTANCE_H_
#define _CRYCREATECLASSINSTANCE_H_

#pragma once

#include "ICryUnknown.h"
#include "ICryFactory.h"
#include "ICryFactoryRegistry.h"
#include <CrySystem/ISystem.h> // <> required for Interfuscator

template<class T>
bool CryCreateClassInstance(const CryClassID& cid, std::shared_ptr<T>& p)
{
	p = std::shared_ptr<T>();
	ICryFactoryRegistry* pFactoryReg = gEnv->pSystem->GetCryFactoryRegistry();
	if (pFactoryReg)
	{
		ICryFactory* pFactory = pFactoryReg->GetFactory(cid);
		if (pFactory && pFactory->ClassSupports(cryiidof<T>()))
		{
			ICryUnknownPtr pUnk = pFactory->CreateClassInstance();
			std::shared_ptr<T> pT = cryinterface_cast<T>(pUnk);
			if (pT)
				p = pT;
		}
	}
	return p.get() != NULL;
}

template<class T>
bool CryCreateClassInstanceForInterface(const CryInterfaceID& iid, std::shared_ptr<T>& p)
{
	p = std::shared_ptr<T>();
	ICryFactoryRegistry* pFactoryReg = gEnv->pSystem->GetCryFactoryRegistry();
	if (pFactoryReg)
	{
		size_t numFactories = 1;
		ICryFactory* pFactory = 0;
		pFactoryReg->IterateFactories(iid, &pFactory, numFactories);
		if (numFactories == 1 && pFactory)
		{
			ICryUnknownPtr pUnk = pFactory->CreateClassInstance();
			std::shared_ptr<T> pT = cryinterface_cast<T>(pUnk);
			if (pT)
				p = pT;
		}
	}
	return p.get() != NULL;
}

#endif // #ifndef _CRYCREATECLASSINSTANCE_H_
