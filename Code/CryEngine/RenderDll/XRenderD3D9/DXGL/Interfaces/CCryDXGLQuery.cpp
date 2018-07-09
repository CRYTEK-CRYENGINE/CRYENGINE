// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLQuery.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for ID3D11Query
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLQuery.hpp"
#include "../Implementation/GLResource.hpp"

CCryDXGLQuery::CCryDXGLQuery(const D3D11_QUERY_DESC& kDesc, NCryOpenGL::SQuery* pGLQuery, CCryDXGLDevice* pDevice)
	: CCryDXGLDeviceChild(pDevice)
	, m_kDesc(kDesc)
	, m_spGLQuery(pGLQuery)
{
	DXGL_INITIALIZE_INTERFACE(D3D11Asynchronous)
	DXGL_INITIALIZE_INTERFACE(D3D11Query)
}

CCryDXGLQuery::~CCryDXGLQuery()
{
}

#if OGL_SINGLE_CONTEXT

NCryOpenGL::SQuery* CCryDXGLQuery::GetGLQuery(NCryOpenGL::CContext* pContext)
{
	IF_UNLIKELY (!m_spGLQuery)
	{
		m_spGLQuery = NCryOpenGL::CreateQuery(m_kDesc, pContext);
		if (!m_spGLQuery)
			DXGL_FATAL("Deferred query creation failed");
	}
	return m_spGLQuery;
}

#else

NCryOpenGL::SQuery* CCryDXGLQuery::GetGLQuery()
{
	return m_spGLQuery;
}

#endif

////////////////////////////////////////////////////////////////////////////////
// ID3D11Asynchronous implementation
////////////////////////////////////////////////////////////////////////////////

UINT CCryDXGLQuery::GetDataSize(void)
{
	return m_spGLQuery->GetDataSize();
}

////////////////////////////////////////////////////////////////////////////////
// ID3D11Query implementation
////////////////////////////////////////////////////////////////////////////////

void CCryDXGLQuery::GetDesc(D3D11_QUERY_DESC* pDesc)
{
	(*pDesc) = m_kDesc;
}
