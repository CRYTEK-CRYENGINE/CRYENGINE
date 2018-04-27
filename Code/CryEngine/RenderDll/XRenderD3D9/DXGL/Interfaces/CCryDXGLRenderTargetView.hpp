// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLRenderTargetView.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D11RenderTargetView
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLRENDERTARGETVIEW__
#define __CRYDXGLRENDERTARGETVIEW__

#include "CCryDXGLView.hpp"

namespace NCryOpenGL
{
struct SOutputMergerView;
class CContext;
}

class CCryDXGLRenderTargetView : public CCryDXGLView
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLRenderTargetView, D3D11RenderTargetView)

	CCryDXGLRenderTargetView(CCryDXGLResource* pResource, const D3D11_RENDER_TARGET_VIEW_DESC& kDesc, CCryDXGLDevice* pDevice);
	virtual ~CCryDXGLRenderTargetView();

	bool Initialize(NCryOpenGL::CContext* pContext);

#if OGL_SINGLE_CONTEXT
	NCryOpenGL::SOutputMergerView* GetGLView(NCryOpenGL::CContext* pContext);
#else
	NCryOpenGL::SOutputMergerView* GetGLView();
#endif

	// Implementation of ID3D11RenderTargetView
	void GetDesc(D3D11_RENDER_TARGET_VIEW_DESC* pDesc);
private:
	D3D11_RENDER_TARGET_VIEW_DESC             m_kDesc;
	_smart_ptr<NCryOpenGL::SOutputMergerView> m_spGLView;
};

#endif //__CRYDXGLRENDERTARGETVIEW__
