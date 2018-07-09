// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLView.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D11View
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLVIEW__
#define __CRYDXGLVIEW__

#include "CCryDXGLDeviceChild.hpp"

class CCryDXGLResource;

class CCryDXGLView : public CCryDXGLDeviceChild
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLView, D3D11View)
	virtual ~CCryDXGLView();

	inline CCryDXGLResource* GetGLResource() { return m_spResource; }

	// Implementation of ID3D11View
	void GetResource(ID3D11Resource** ppResource);

#if !DXGL_FULL_EMULATION
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
	{
		if (SingleInterface<CCryDXGLView>::Query(this, riid, ppvObject))
			return S_OK;
		return CCryDXGLDeviceChild::QueryInterface(riid, ppvObject);
	}
#endif //!DXGL_FULL_EMULATION
protected:
	CCryDXGLView(CCryDXGLResource* pResource, CCryDXGLDevice* pDevice);

	_smart_ptr<CCryDXGLResource> m_spResource;
};

#endif //__CRYDXGLVIEW__
