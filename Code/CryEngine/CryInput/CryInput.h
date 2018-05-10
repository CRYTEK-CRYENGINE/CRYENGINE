// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	Here the actual input implementation gets chosen for the
              different platforms.
   -------------------------------------------------------------------------
   History:
   - Dec 05,2005: Created by Marco Koegler
   - Sep 09,2012: Updated by Dario Sancho (added support for Durango)

*************************************************************************/
#ifndef __CRYINPUT_H__
#define __CRYINPUT_H__
#pragma once

#if !defined(DEDICATED_SERVER)
	#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
// Linux client (not dedicated server)
		#define USE_LINUXINPUT
		#include "LinuxInput.h"
	#elif CRY_PLATFORM_ORBIS
		#define USE_ORBIS_INPUT
		#include "OrbisInput.h"
	#elif CRY_PLATFORM_DURANGO
		#define USE_DURANGOINPUT
		#include "DurangoInput.h"
	#else
// Win32
		#define USE_DXINPUT
		#include "DXInput.h"
	#endif
	#if !defined(_RELEASE) && !CRY_PLATFORM_WINDOWS
		#define USE_SYNERGY_INPUT
	#endif
	#include "InputCVars.h"
#endif

#endif //__CRYINPUT_H__
