// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __NETWORKDEFINES_H__
#define __NETWORKDEFINES_H__

//---------------------------------------------------------------------------
#define IMPLEMENT_PC_BLADES 1

#if /*!defined(PROFILE) && */!defined(_RELEASE)
#define GFM_ENABLE_EXTRA_DEBUG 1
#else
#define GFM_ENABLE_EXTRA_DEBUG 0
#endif

//---------------------------------------------------------------------------

#endif // __NETWORKDEFINES_H__
