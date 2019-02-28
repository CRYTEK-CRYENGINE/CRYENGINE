// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#if defined(SANDBOX_API)
	#error SANDBOX_API should only be defined in this header. Use SANDBOX_EXPORTS and SANDBOX_IMPORTS to control it.
#endif

#if defined(SANDBOX_IMPORTS) && defined(SANDBOX_EXPORTS)
	#error SANDBOX_EXPORTS and SANDBOX_IMPORTS can't be defined at the same time
#endif

#if defined(SANDBOX_EXPORTS)
// Sandbox.exe case
	#define SANDBOX_API DLL_EXPORT
#elif defined(SANDBOX_IMPORTS)
// Sandbox plugins that rely on/extend Editor types.
	#define SANDBOX_API DLL_IMPORT
#else
// Standalone plugins that use Editor plugins.
	#define SANDBOX_API
#endif

// Surround text that needs localiation in this macro
#define LOCALIZE(x) x

