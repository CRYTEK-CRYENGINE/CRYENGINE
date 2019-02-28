// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#if defined(PLUGIN_API)
#error PLUGIN_API should only be defined in this header. Use PLUGIN_EXPORTS and PLUGIN_IMPORTS to control it.
#endif

#if defined(PLUGIN_EXPORTS) && defined(PLUGIN_IMPORTS)
#error PLUGIN_EXPORTS and PLUGIN_IMPORTS can't be defined at the same time
#endif

#if defined(PLUGIN_EXPORTS)
#define PLUGIN_API DLL_EXPORT
#elif defined(PLUGIN_IMPORTS)
#define PLUGIN_API DLL_IMPORT
#else
#define PLUGIN_API
#endif
