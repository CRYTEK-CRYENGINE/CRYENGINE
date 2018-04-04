// Copyright 2001-2015 Crytek GmbH. All rights reserved.

#include "StdAfx.h"

#include "CryLinkFramework.h"
#include "CryLinkCVars.h"

#include <CryGame/IGameFramework.h>
#include <CryExtension/CryCreateClassInstance.h>

#define CRY_LINK_EXTENSION_CREATOR "CryExtension_CryLinkFrameworkCreator"

using namespace CryLinkService;

CRYREGISTER_CLASS(CCryLinkFramework)

CCryLinkFramework::~CCryLinkFramework()
{
	CVars::GetInstance().Unregister();
}

ICryLinkService& CCryLinkFramework::GetService()
{
	return m_cryLinkService;
}

//////////////////////////////////////////////////////////////////////////
// Framework creation

IFramework* CryLinkService::CreateFramework(IGameFramework* pGameFramework)
{
		LOADING_TIME_PROFILE_SECTION;
		IGameFrameworkExtensionCreatorPtr pCreator;
		if (!CryCreateClassInstance(CRY_LINK_EXTENSION_CREATOR , pCreator))
			return nullptr;

		return cryinterface_cast<IFramework>(pCreator->Create(pGameFramework, nullptr));
}

class CryLinkFrameworkCreator : public IGameFrameworkExtensionCreator
{
	CRYINTERFACE_SIMPLE(IGameFrameworkExtensionCreator)
	CRYGENERATE_SINGLETONCLASS_GUID(CryLinkFrameworkCreator, CRY_LINK_EXTENSION_CREATOR, "89f52cbf-dc2f-4ad6-b573-5b7cdcc6cc7f"_cry_guid)

	virtual ~CryLinkFrameworkCreator() {}

	virtual ICryUnknown* Create(IGameFramework* pGameFramework, void* pData) override
	{
		std::shared_ptr<IFramework> pCryLinkFramework;
		if (CryCreateClassInstance(CRY_LINK_EXTENSION_CRYCLASS_NAME , pCryLinkFramework))
		{
			CVars::GetInstance().Register();

			pGameFramework->RegisterExtension(pCryLinkFramework);
			CryLogAlways("CryLink extension initialized.");

			return pCryLinkFramework.get();
		}

		return nullptr;
	}
};

CRYREGISTER_SINGLETON_CLASS(CryLinkFrameworkCreator)