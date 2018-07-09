// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLTexture2D.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D11Texture2D
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLTEXTURE2D__
#define __CRYDXGLTEXTURE2D__

#include "CCryDXGLTextureBase.hpp"

class CCryDXGLTexture2D : public CCryDXGLTextureBase
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLTexture2D, D3D11Texture2D)

	CCryDXGLTexture2D(const D3D11_TEXTURE2D_DESC& kDesc, NCryOpenGL::STexture* pGLTexture, CCryDXGLDevice* pDevice);
#if OGL_SINGLE_CONTEXT
	CCryDXGLTexture2D(const D3D11_TEXTURE2D_DESC& kDesc, NCryOpenGL::SInitialDataCopy* pInitialData, CCryDXGLDevice* pDevice);
#endif
	virtual ~CCryDXGLTexture2D();

#if OGL_SINGLE_CONTEXT
	virtual void Initialize();
#endif

	// Implementation of ID3D11Texture2D
	void GetDesc(D3D11_TEXTURE2D_DESC* pDesc);

#if !DXGL_FULL_EMULATION
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
	{
		if (SingleInterface<CCryDXGLTexture2D>::Query(this, riid, ppvObject))
			return S_OK;
		return CCryDXGLTextureBase::QueryInterface(riid, ppvObject);
	}
#endif //!DXGL_FULL_EMULATION
private:
	D3D11_TEXTURE2D_DESC m_kDesc;
};

#endif //__CRYDXGLTEXTURE2D__
