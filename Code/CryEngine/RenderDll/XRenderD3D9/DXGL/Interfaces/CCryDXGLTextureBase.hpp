// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLTextureBase.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL common base class for textures
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLTEXTUREBASE__
#define __CRYDXGLTEXTUREBASE__

#include "CCryDXGLResource.hpp"

namespace NCryOpenGL
{
struct STexture;
};

class CCryDXGLTextureBase : public CCryDXGLResource
{
public:
	virtual ~CCryDXGLTextureBase();

	NCryOpenGL::STexture* GetGLTexture();
protected:
	CCryDXGLTextureBase(D3D11_RESOURCE_DIMENSION eDimension, NCryOpenGL::STexture* pGLTexture, CCryDXGLDevice* pDevice);
#if OGL_SINGLE_CONTEXT
	CCryDXGLTextureBase(D3D11_RESOURCE_DIMENSION eDimension, NCryOpenGL::SInitialDataCopy* pInitialData, CCryDXGLDevice* pDevice);
#endif
};

#endif //__CRYDXGLTEXTUREBASE__
