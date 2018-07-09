// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLDeviceChild.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for ID3D11DeviceChild
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLDeviceChild.hpp"
#include "CCryDXGLDevice.hpp"

CCryDXGLDeviceChild::CCryDXGLDeviceChild(CCryDXGLDevice* pDevice)
	: m_pDevice(pDevice)
{
	DXGL_INITIALIZE_INTERFACE(D3D11DeviceChild)
	if (m_pDevice != NULL)
		m_pDevice->AddRef();
}

CCryDXGLDeviceChild::~CCryDXGLDeviceChild()
{
	if (m_pDevice != NULL)
		m_pDevice->Release();
}

void CCryDXGLDeviceChild::SetDevice(CCryDXGLDevice* pDevice)
{
	if (m_pDevice != pDevice)
	{
		if (m_pDevice != NULL)
			m_pDevice->Release();
		m_pDevice = pDevice;
		if (pDevice != NULL)
			m_pDevice->AddRef();
	}
}

////////////////////////////////////////////////////////////////////////////////
// ID3D11DeviceChild implementation
////////////////////////////////////////////////////////////////////////////////

void CCryDXGLDeviceChild::GetDevice(ID3D11Device** ppDevice)
{
	CCryDXGLDevice::ToInterface(ppDevice, m_pDevice);
}

HRESULT CCryDXGLDeviceChild::GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData)
{
	return m_kPrivateDataContainer.GetPrivateData(guid, pDataSize, pData);
}

HRESULT CCryDXGLDeviceChild::SetPrivateData(REFGUID guid, UINT DataSize, const void* pData)
{
	return m_kPrivateDataContainer.SetPrivateData(guid, DataSize, pData);
}

HRESULT CCryDXGLDeviceChild::SetPrivateDataInterface(REFGUID guid, const IUnknown* pData)
{
	return m_kPrivateDataContainer.SetPrivateDataInterface(guid, pData);
}
