// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"

// constructor/destructor
CREDeferredShading::CREDeferredShading()
{
	// setup screen process renderer type
	mfSetType(eDATA_DeferredShading);
	mfUpdateFlags(FCEF_TRANSFORM);
}

CREDeferredShading::~CREDeferredShading()
{
};

// prepare screen processing
void CREDeferredShading::mfPrepare(bool bCheckOverflow)
{
	if (bCheckOverflow)
		gRenDev->FX_CheckOverflow(0, 0, this);

	gRenDev->m_RP.m_pRE = this;
	gRenDev->m_RP.m_RendNumIndices = 0;
	gRenDev->m_RP.m_RendNumVerts = 0;
}

void CREDeferredShading::mfReset()
{
}

void CREDeferredShading::mfActivate(int iProcess)
{
}
