// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ICryDXGLUnknown.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Contains the cross platform replacement for COM IUnknown
//               interface on non-windows platforms
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLUNKNOWN__
#define __CRYDXGLUNKNOWN__

#if !CRY_PLATFORM_WINDOWS

	#include "CryDXGLGuid.hpp"

struct IUnknown
{
public:
	#if !DXGL_FULL_EMULATION
	virtual ~IUnknown(){};
	#endif //!DXGL_FULL_EMULATION

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) = 0;
	virtual ULONG STDMETHODCALLTYPE AddRef(void) = 0;
	virtual ULONG STDMETHODCALLTYPE Release(void) = 0;
};

#endif

#endif
