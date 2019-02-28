// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLShaderResourceView.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D11ShaderResourceView
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLSHADERRESOURCEVIEW__
#define __CRYDXGLSHADERRESOURCEVIEW__

#include "CCryDXGLView.hpp"

namespace NCryOpenGL
{
struct SShaderView;
class CContext;
}

class CCryDXGLShaderResourceView : public CCryDXGLView
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLShaderResourceView, D3D11ShaderResourceView)

	CCryDXGLShaderResourceView(CCryDXGLResource* pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC& kDesc, CCryDXGLDevice* pDevice);
	virtual ~CCryDXGLShaderResourceView();

	bool Initialize(NCryOpenGL::CContext* pContext);

#if OGL_SINGLE_CONTEXT
	NCryOpenGL::SShaderView*       GetGLView(NCryOpenGL::CContext* pContext);
#else
	ILINE NCryOpenGL::SShaderView* GetGLView()
	{
		return m_spGLView;
	}
#endif

	// Implementation of ID3D11ShaderResourceView
	void GetDesc(D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc);
protected:
	D3D11_SHADER_RESOURCE_VIEW_DESC     m_kDesc;
	_smart_ptr<NCryOpenGL::SShaderView> m_spGLView;
};

#endif //__CRYDXGLSHADERRESOURCEVIEW__
