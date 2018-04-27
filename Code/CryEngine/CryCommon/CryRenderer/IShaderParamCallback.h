// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include <CryExtension/ICryUnknown.h>

struct ICharacterInstance;
struct IShaderPublicParams;

//! Callback object which can be used to override shader params for the game side.
struct IShaderParamCallback : public ICryUnknown
{
	CRYINTERFACE_DECLARE_GUID(IShaderParamCallback, "4fb87a5f-83f7-4323-a7e4-2ca947c549d8"_cry_guid);

	// <interfuscator:shuffle>
	//! Setting actual object to be worked on, but should ideally all derive from a same base pointer for characters, rendermeshes, vegetation.
	virtual void                SetCharacterInstance(ICharacterInstance* pCharInstance) {}
	virtual ICharacterInstance* GetCharacterInstance() const                            { return NULL; }

	virtual bool                Init() = 0;

	//! Called just before submitting the render proxy for rendering.
	//! Can be used to setup game specific shader params.
	virtual void SetupShaderParams(IShaderPublicParams* pParams, IMaterial* pMaterial) = 0;

	virtual void Disable(IShaderPublicParams* pParams) = 0;
	// </interfuscator:shuffle>
};

DECLARE_SHARED_POINTERS(IShaderParamCallback);                                                                \

//! \endcond