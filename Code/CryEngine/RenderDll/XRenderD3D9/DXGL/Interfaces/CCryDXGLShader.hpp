// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLShader.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for D3D11 shader interfaces
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLSHADER__
#define __CRYDXGLSHADER__

#include "CCryDXGLDeviceChild.hpp"

namespace NCryOpenGL
{
struct SShader;
}

class CCryDXGLShader : public CCryDXGLDeviceChild
{
public:
	CCryDXGLShader(NCryOpenGL::SShader* pGLShader, CCryDXGLDevice* pDevice);
	virtual ~CCryDXGLShader();

	NCryOpenGL::SShader* GetGLShader();
private:
	_smart_ptr<NCryOpenGL::SShader> m_spGLShader;
};

class CCryDXGLVertexShader : public CCryDXGLShader
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLVertexShader, D3D11VertexShader)

	CCryDXGLVertexShader(NCryOpenGL::SShader* pGLShader, CCryDXGLDevice* pDevice) : CCryDXGLShader(pGLShader, pDevice)
	{
		DXGL_INITIALIZE_INTERFACE(D3D11VertexShader)
	}
};

class CCryDXGLHullShader : public CCryDXGLShader
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLHullShader, D3D11HullShader)

	CCryDXGLHullShader(NCryOpenGL::SShader* pGLShader, CCryDXGLDevice* pDevice) : CCryDXGLShader(pGLShader, pDevice)
	{
		DXGL_INITIALIZE_INTERFACE(D3D11HullShader)
	}
};

class CCryDXGLDomainShader : public CCryDXGLShader
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLDomainShader, D3D11DomainShader)

	CCryDXGLDomainShader(NCryOpenGL::SShader* pGLShader, CCryDXGLDevice* pDevice) : CCryDXGLShader(pGLShader, pDevice)
	{
		DXGL_INITIALIZE_INTERFACE(D3D11DomainShader)
	}
};

class CCryDXGLGeometryShader : public CCryDXGLShader
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLGeometryShader, D3D11GeometryShader)

	CCryDXGLGeometryShader(NCryOpenGL::SShader* pGLShader, CCryDXGLDevice* pDevice) : CCryDXGLShader(pGLShader, pDevice)
	{
		DXGL_INITIALIZE_INTERFACE(D3D11GeometryShader)
	}
};

class CCryDXGLPixelShader : public CCryDXGLShader
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLPixelShader, D3D11PixelShader)

	CCryDXGLPixelShader(NCryOpenGL::SShader* pGLShader, CCryDXGLDevice* pDevice) : CCryDXGLShader(pGLShader, pDevice)
	{
		DXGL_INITIALIZE_INTERFACE(D3D11PixelShader)
	}
};

class CCryDXGLComputeShader : public CCryDXGLShader
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLComputeShader, D3D11ComputeShader)

	CCryDXGLComputeShader(NCryOpenGL::SShader* pGLShader, CCryDXGLDevice* pDevice) : CCryDXGLShader(pGLShader, pDevice)
	{
		DXGL_INITIALIZE_INTERFACE(D3D11ComputeShader)
	}
};

#endif  //__CRYDXGLSHADER__
