// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLBase.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the reference counted base class for all
//               DXGL interface implementations
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLBase.hpp"
#include "../Implementation/GLCommon.hpp"

CCryDXGLBase::CCryDXGLBase()
	: m_uRefCount(1)
{
	DXGL_INITIALIZE_INTERFACE(Unknown);
}

CCryDXGLBase::~CCryDXGLBase()
{
}

////////////////////////////////////////////////////////////////////////////////
// Implementation of IUnknown
////////////////////////////////////////////////////////////////////////////////

#if DXGL_FULL_EMULATION

SAggregateNode& CCryDXGLBase::GetAggregateHead()
{
	return m_kAggregateHead;
}

#else

HRESULT CCryDXGLBase::QueryInterface(REFIID riid, void** ppvObject)
{
	return E_NOINTERFACE;
}

#endif

ULONG CCryDXGLBase::AddRef(void)
{
	return ++m_uRefCount;
}

ULONG CCryDXGLBase::Release(void)
{
	--m_uRefCount;
	if (m_uRefCount == 0)
	{
		delete this;
		return 0;
	}
	return m_uRefCount;
}

////////////////////////////////////////////////////////////////////////////////
// CCryDXGLPrivateDataContainer
////////////////////////////////////////////////////////////////////////////////

struct CCryDXGLPrivateDataContainer::SPrivateData
{
	union
	{
		uint8*    m_pBuffer;
		IUnknown* m_pInterface;
	};
	uint32 m_uSize;
	bool   m_bInterface;

	SPrivateData(const void* pData, uint32 uSize)
	{
		m_pBuffer = new uint8[uSize];
		m_uSize = uSize;
		m_bInterface = false;
		NCryOpenGL::Memcpy(m_pBuffer, pData, uSize);
	}

	SPrivateData(IUnknown* pInterface)
	{
		pInterface->AddRef();
		m_pInterface = pInterface;
		m_uSize = sizeof(IUnknown*);
		m_bInterface = true;
	}

	~SPrivateData()
	{
		if (m_bInterface)
			m_pInterface->Release();
		else
			delete[] m_pBuffer;
	}
};

CCryDXGLPrivateDataContainer::CCryDXGLPrivateDataContainer()
{
}

CCryDXGLPrivateDataContainer::~CCryDXGLPrivateDataContainer()
{
	TPrivateDataMap::iterator kPrivateIter(m_kPrivateDataMap.begin());
	TPrivateDataMap::iterator kPrivateEnd(m_kPrivateDataMap.end());
	for (; kPrivateIter != kPrivateEnd; ++kPrivateIter)
	{
		delete kPrivateIter->second;
	}
}

HRESULT CCryDXGLPrivateDataContainer::GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData)
{
	if (pData == NULL)
	{
		if (*pDataSize != 0)
			return E_FAIL;
		RemovePrivateData(guid);
	}
	else
	{
		assert(pDataSize != NULL);
		TPrivateDataMap::const_iterator kFound(m_kPrivateDataMap.find(guid));
		if (kFound == m_kPrivateDataMap.end() || *pDataSize < kFound->second->m_uSize)
			return E_FAIL;

		if (kFound->second->m_bInterface)
		{
			kFound->second->m_pInterface->AddRef();
			*static_cast<IUnknown**>(pData) = kFound->second->m_pInterface;
		}
		else
			NCryOpenGL::Memcpy(pData, kFound->second->m_pBuffer, kFound->second->m_uSize);
		*pDataSize = kFound->second->m_uSize;
	}

	return S_OK;
}

HRESULT CCryDXGLPrivateDataContainer::SetPrivateData(REFGUID guid, UINT DataSize, const void* pData)
{
	RemovePrivateData(guid);

	m_kPrivateDataMap.insert(TPrivateDataMap::value_type(guid, new SPrivateData(pData, DataSize)));
	return S_OK;
}

HRESULT CCryDXGLPrivateDataContainer::SetPrivateDataInterface(REFGUID guid, const IUnknown* pData)
{
	RemovePrivateData(guid);

	m_kPrivateDataMap.insert(TPrivateDataMap::value_type(guid, new SPrivateData(const_cast<IUnknown*>(pData))));  // The specification requires that IUnknown::AddRef, Release are called on pData thus the const cast
	return S_OK;
}

void CCryDXGLPrivateDataContainer::RemovePrivateData(REFGUID guid)
{
	TPrivateDataMap::iterator kFound(m_kPrivateDataMap.find(guid));
	if (kFound != m_kPrivateDataMap.end())
	{
		delete kFound->second;
		m_kPrivateDataMap.erase(kFound);
	}
}

size_t CCryDXGLPrivateDataContainer::SGuidHashCompare::operator()(const GUID& kGuid) const
{
	return (size_t)NCryOpenGL::GetCRC32(&kGuid, sizeof(kGuid));
}

bool CCryDXGLPrivateDataContainer::SGuidHashCompare::operator()(const GUID& kLeft, const GUID& kRight) const
{
	return memcmp(&kLeft, &kRight, sizeof(kLeft)) == 0;
}
