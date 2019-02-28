// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DX11Base.hpp"

int g_nPrintDX11 = 0;

namespace NCryDX11
{

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
CDeviceObject::CDeviceObject(CDevice* device)
	: CRefCounted()
	, m_pDevice(device)
{

}

//---------------------------------------------------------------------------------------------------------------------
CDeviceObject::~CDeviceObject()
{
	DX11_LOG(g_nPrintDX11, "DX11 object destroyed: %p", this);
}

}
