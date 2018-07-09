// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLView.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for ID3D11View
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLView.hpp"
#include "CCryDXGLResource.hpp"
#include "../Implementation/GLResource.hpp"

CCryDXGLView::CCryDXGLView(CCryDXGLResource* pResource, CCryDXGLDevice* pDevice)
	: CCryDXGLDeviceChild(pDevice)
	, m_spResource(pResource)
{
	DXGL_INITIALIZE_INTERFACE(D3D11View)
}

CCryDXGLView::~CCryDXGLView()
{
}

////////////////////////////////////////////////////////////////////////////////
// Implementation of ID3D11View
////////////////////////////////////////////////////////////////////////////////

void CCryDXGLView::GetResource(ID3D11Resource** ppResource)
{
	if (m_spResource != NULL)
		m_spResource->AddRef();
	CCryDXGLResource::ToInterface(ppResource, m_spResource);
}
