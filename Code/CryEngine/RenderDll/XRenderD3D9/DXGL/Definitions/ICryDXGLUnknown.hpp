// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if !CRY_PLATFORM_WINDOWS

#include "CryDXGLGuid.hpp"

// Cross platform replacement for COM IUnknown interface on non-windows platforms
struct IUnknown
{
public:
	#if !DXGL_FULL_EMULATION
	virtual ~IUnknown(){}
	#endif //!DXGL_FULL_EMULATION

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) = 0;
	virtual ULONG STDMETHODCALLTYPE AddRef(void) = 0;
	virtual ULONG STDMETHODCALLTYPE Release(void) = 0;
};

#endif
