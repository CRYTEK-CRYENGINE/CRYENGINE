// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLGIObject.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for IDXGIObject
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLGIObject.hpp"
#include "../Implementation/GLCommon.hpp"

CCryDXGLGIObject::CCryDXGLGIObject()
{
	DXGL_INITIALIZE_INTERFACE(DXGIObject)
}

CCryDXGLGIObject::~CCryDXGLGIObject()
{
}

////////////////////////////////////////////////////////////////////////////////
// IDXGIObject implementation
////////////////////////////////////////////////////////////////////////////////

HRESULT CCryDXGLGIObject::SetPrivateData(REFGUID Name, UINT DataSize, const void* pData)
{
	return m_kPrivateDataContainer.SetPrivateData(Name, DataSize, pData);
}

HRESULT CCryDXGLGIObject::SetPrivateDataInterface(REFGUID Name, const IUnknown* pUnknown)
{
	return m_kPrivateDataContainer.SetPrivateDataInterface(Name, pUnknown);
}

HRESULT CCryDXGLGIObject::GetPrivateData(REFGUID Name, UINT* pDataSize, void* pData)
{
	return m_kPrivateDataContainer.GetPrivateData(Name, pDataSize, pData);
}

HRESULT CCryDXGLGIObject::GetParent(REFIID riid, void** ppParent)
{
	DXGL_TODO("Implement if required")
	* ppParent = NULL;
	return E_FAIL;
}
