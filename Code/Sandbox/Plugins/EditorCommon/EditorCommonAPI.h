// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if defined(EDITOR_COMMON_API)
#error EDITOR_COMMON_API should only be defined in this header. Use EDITOR_COMMON_EXPORTS and EDITOR_COMMON_IMPORTS to control it.
#endif

#if defined(EDITOR_COMMON_EXPORTS) && defined(EDITOR_COMMON_IMPORTS)
#error EDITOR_COMMON_EXPORTS and EDITOR_COMMON_IMPORTS can't be defined at the same time
#endif

#if defined(EDITOR_COMMON_EXPORTS)
	#define EDITOR_COMMON_API DLL_EXPORT
#elif defined(EDITOR_COMMON_IMPORTS)
	#define EDITOR_COMMON_API DLL_IMPORT
#else
	#define EDITOR_COMMON_API
#endif

