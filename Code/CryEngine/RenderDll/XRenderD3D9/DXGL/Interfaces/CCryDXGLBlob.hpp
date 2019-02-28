// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLBlob.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D10Blob
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLBLOB__
#define __CRYDXGLBLOB__

#include "CCryDXGLBase.hpp"

#if defined(DXGL_BLOB_INTEROPERABILITY) && !DXGL_FULL_EMULATION
class CCryDXGLBlob : public ID3D10Blob
#else
class CCryDXGLBlob : public CCryDXGLBase
#endif
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLBlob, D3D10Blob)

	CCryDXGLBlob(size_t uBufferSize);
	virtual ~CCryDXGLBlob();

#if defined(DXGL_BLOB_INTEROPERABILITY)
	//IUnknown implementation
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
	ULONG STDMETHODCALLTYPE   AddRef();
	ULONG STDMETHODCALLTYPE   Release();
#endif //defined(DXGL_BLOB_INTEROPERABILITY)

	// ID3D10Blob implementation
	LPVOID STDMETHODCALLTYPE GetBufferPointer();
	SIZE_T STDMETHODCALLTYPE GetBufferSize();
protected:
#if defined(DXGL_BLOB_INTEROPERABILITY)
	uint32 m_uRefCount;
#endif //defined(DXGL_BLOB_INTEROPERABILITY)

	uint8 * m_pBuffer;
	size_t m_uBufferSize;
};

#endif //__CRYDXGLBLOB__
