// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   version.h
//  Version:     v1.00
//  Created:     16/09/2013 by Leander Beernaert
//  Compilers:   All
//  Description: Contains the build version of the Engine
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __version_h__
#define __version_h__

// The macros below define the engine version. Their names are self-
// -explanatory. These macros can be overriden by defines in two ways:
// 1) Hard coded - Define each of the CRYENGINE_VERSION_* in CryCommon/Version_override.h
// 2) Config tool - Define eacho of the CRYENGINE_CONFIG_VERSION_*
//
// If there are no defines, all the values will default to 0
//

#include "Version_override.h"

#ifndef CRYENGINE_VERSION_MAJOR
	#ifndef CRYENGINE_CONFIG_VERSION_MAJOR
		#define CRYENGINE_VERSION_MAJOR 0
	#else
		#define CRYENGINE_VERSION_MAJOR CRYENGINE_CONFIG_VERSION_MAJOR
	#endif
#endif

#ifndef CRYENGINE_VERSION_MINOR
	#ifndef CRYENGINE_CONFIG_VERSION_MINOR
		#define CRYENGINE_VERSION_MINOR 0
	#else
		#define CRYENGINE_VERSION_MINOR CRYENGINE_CONFIG_VERSION_MINOR
	#endif
#endif

#ifndef CRYENGINE_VERSION_REVISION
	#ifndef CRYENGINE_CONFIG_VERSION_REVISION
		#define CRYENGINE_VERSION_REVISION 0
	#else
		#define CRYENGINE_VERSION_REVISION CRYENGINE_CONFIG_VERSION_REVISION
	#endif
#endif

#ifndef CRYENGINE_VERSION_BUILDNUM
	#ifndef CRYENGINE_CONFIG_VERSION_BUILDNUM
		#define CRYENGINE_VERSION_BUILDNUM 0
	#else
		#define CRYENGINE_VERSION_BUILDNUM CRYENGINE_CONFIG_VERSION_BUILDNUM
	#endif
#endif

#endif // __version_h__
