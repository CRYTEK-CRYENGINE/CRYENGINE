// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Includes
#include "StdAfx.h"
#include <CryRenderer/RenderElements/CREGameEffect.h>
#include "XRenderD3D9/DriverD3D.h"

//--------------------------------------------------------------------------------------------------
// Name: CREGameEffect
// Desc: Constructor
//--------------------------------------------------------------------------------------------------
CREGameEffect::CREGameEffect()
{
	m_pImpl = NULL;
	mfSetType(eDATA_GameEffect);
	mfUpdateFlags(FCEF_TRANSFORM);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ~CREGameEffect
// Desc: Destructor
//--------------------------------------------------------------------------------------------------
CREGameEffect::~CREGameEffect()
{
	SAFE_DELETE(m_pImpl);
}//-------------------------------------------------------------------------------------------------
