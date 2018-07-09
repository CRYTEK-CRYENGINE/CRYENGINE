// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryExtension/ICryUnknown.h>

struct SSystemInitParams;
struct SSystemGlobalEnvironment;

namespace Cry
{
	//! Base Interface for all engine module extensions.
	struct IDefaultModule : public ICryUnknown
	{
		CRYINTERFACE_DECLARE_GUID(IDefaultModule, "f899cf66-1df0-4f61-a341-a8a7ffdf9de4"_cry_guid);

		// <interfuscator:shuffle>
		//! Retrieve name of the extension module.
		virtual const char* GetName() const = 0;

		//! Retrieve category for the extension module (CryEngine for standard modules).
		virtual const char* GetCategory() const = 0;

		//! This is called to initialize the new module.
		virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) = 0;
		// </interfuscator:shuffle>
	};
}