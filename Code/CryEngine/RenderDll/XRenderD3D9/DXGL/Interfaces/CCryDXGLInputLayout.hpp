// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLInputLayout.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D11InputLayout
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLINPUTLAYOUT__
#define __CRYDXGLINPUTLAYOUT__

#include "CCryDXGLDeviceChild.hpp"

namespace NCryOpenGL
{
struct SInputLayout;
}

class CCryDXGLInputLayout : public CCryDXGLDeviceChild
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLInputLayout, D3D11InputLayout)

	CCryDXGLInputLayout(NCryOpenGL::SInputLayout* pGLLayout, CCryDXGLDevice* pDevice);
	virtual ~CCryDXGLInputLayout();

	NCryOpenGL::SInputLayout* GetGLLayout();
private:
	_smart_ptr<NCryOpenGL::SInputLayout> m_spGLLayout;
};

#endif //__CRYDXGLINPUTLAYOUT__
