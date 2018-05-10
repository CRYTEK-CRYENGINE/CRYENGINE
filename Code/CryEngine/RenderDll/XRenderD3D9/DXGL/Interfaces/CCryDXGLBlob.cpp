// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLBlob.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for ID3D10Blob
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLBlob.hpp"

CCryDXGLBlob::CCryDXGLBlob(size_t uBufferSize)
	: m_uBufferSize(uBufferSize)
#if defined(DXGL_BLOB_INTEROPERABILITY)
	, m_uRefCount(1)
#endif //defined(DXGL_BLOB_INTEROPERABILITY)
{
	DXGL_INITIALIZE_INTERFACE(D3D10Blob)
	m_pBuffer = new uint8[m_uBufferSize];
}

CCryDXGLBlob::~CCryDXGLBlob()
{
	delete[] m_pBuffer;
}

#if defined(DXGL_BLOB_INTEROPERABILITY)

////////////////////////////////////////////////////////////////////////////////
// IUnknown implementation
////////////////////////////////////////////////////////////////////////////////

HRESULT CCryDXGLBlob::QueryInterface(REFIID riid, void** ppvObject)
{
	return E_NOINTERFACE;
}

ULONG CCryDXGLBlob::AddRef(void)
{
	return ++m_uRefCount;
}

ULONG CCryDXGLBlob::Release(void)
{
	--m_uRefCount;
	if (m_uRefCount == 0)
	{
		delete this;
		return 0;
	}
	return m_uRefCount;
}

#endif //defined(DXGL_BLOB_INTEROPERABILITY)

////////////////////////////////////////////////////////////////////////////////
// ID3D10Blob implementation
////////////////////////////////////////////////////////////////////////////////

LPVOID CCryDXGLBlob::GetBufferPointer()
{
	return m_pBuffer;
}

SIZE_T CCryDXGLBlob::GetBufferSize()
{
	return m_uBufferSize;
}
