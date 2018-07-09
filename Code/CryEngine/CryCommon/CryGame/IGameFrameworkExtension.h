// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryExtension/ICryUnknown.h>

struct IGameFramework;

struct IGameFrameworkExtensionCreator : public ICryUnknown
{
	CRYINTERFACE_DECLARE_GUID(IGameFrameworkExtensionCreator, "86197e35-ad02-4da8-a8d4-83a98f424ffd"_cry_guid);

#if (eCryModule != eCryM_LegacyGameDLL && eCryModule != eCryM_EditorPlugin && eCryModule != eCryM_Legacy)
	CRY_DEPRECATED("(v5.5) Game framework extensions are deprecated, please use ICryPlugin instead") IGameFrameworkExtensionCreator() = default;
#endif

	//! Creates an extension and returns the interface to it (extension interface must derivate for ICryUnknown).
	//! \param pIGameFramework Pointer to game framework interface, so the new extension can be registered against it.
	//! \param pData Pointer to data needed for creation. Each system interface is responsible for explaining what data is expected to be passed here, if any, 'nullptr' is valid for default extensions.
	//! \return ICryUnknown pointer to just created extension (it can be safely casted with cryinterface_cast< > to the corresponding interface).
	virtual ICryUnknown* Create(IGameFramework* pIGameFramework, void* pData) = 0;
};
DECLARE_SHARED_POINTERS(IGameFrameworkExtensionCreator);
