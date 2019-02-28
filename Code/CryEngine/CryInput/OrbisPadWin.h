// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   ---------------------------------------------------------------------
   File name:   OrbisPadWin.h
   Description: Pad for Orbis in Windows
   ---------------------------------------------------------------------
   History:
   - 16.06.2014 : Added by Matthijs van der Meide
   - 20.05.2015 : Updated to Pad for Windows PC Games (v1.4)

 *********************************************************************/
#ifndef __ORBISPADWIN_H__
#define __ORBISPADWIN_H__
#pragma once

// If you don't want this feature at all, just:
//#undef WANT_ORBISPAD_WIN

#if _MSC_VER > 1900 && defined(WANT_ORBISPAD_WIN)
// Missing OrbisPad libs for Visual Studio 2015
	#undef WANT_ORBISPAD_WIN
	#pragma message("Unable to use OrbisPad on Windows with MSVC newer than 2015 due to missing libs.")
#endif

#if defined(USE_DXINPUT) && (CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT) && defined(WANT_ORBISPAD_WIN)
	#ifndef USE_ORBISPAD_WIN
		#define USE_ORBISPAD_WIN
	#endif
#else
	#undef USE_ORBISPAD_WIN
#endif

#ifdef USE_ORBISPAD_WIN
	#include "CryInput.h"
	#include "InputDevice.h"

// Construct a new COrbisPadWin instance
extern CInputDevice* CreateOrbisPadWin(ISystem* pSystem, CBaseInput& input, int deviceNo);

// Destroy an existing COrbisPadWin instance
extern void DestroyOrbisPadWin(CInputDevice* pDevice);

#endif //#ifdef USE_ORBISPAD_WIN
#endif //#ifdef __ORBISPADWIN_H__
