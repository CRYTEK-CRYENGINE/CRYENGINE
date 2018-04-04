// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

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

//--------------------------------------------------------------------------------------------------
// Name: mfPrepare
// Desc: Prepares rendering
//--------------------------------------------------------------------------------------------------
void CREGameEffect::mfPrepare(bool bCheckOverflow)
{
	if (bCheckOverflow)
		gRenDev->FX_CheckOverflow(0, 0, this);
	gRenDev->m_RP.m_pRE = this;
	gRenDev->m_RP.m_RendNumIndices = 0;
	gRenDev->m_RP.m_RendNumVerts = 0;

	if (m_pImpl)
	{
		m_pImpl->mfPrepare(false);
	}
}//-------------------------------------------------------------------------------------------------

bool CREGameEffect::mfDraw(CShader* ef, SShaderPass* sfm)
{
	CRY_ASSERT_MESSAGE(gRenDev->m_pRT->IsRenderThread(), "Trying to render from wrong thread");
	CRY_ASSERT(ef);
	CRY_ASSERT(sfm);

	if (m_pImpl)
	{
#ifndef _RELEASE
		IMaterial* pMaterial = (gRenDev->m_RP.m_pCurObject) ? (gRenDev->m_RP.m_pCurObject->m_pCurrMaterial) : NULL;
		const char* pEffectName = (pMaterial) ? (PathUtil::GetFileName(pMaterial->GetName())) : "GameEffectRenderElement";
		PROFILE_LABEL_SCOPE(pEffectName);
#endif

		uint32 passCount = 0;
		bool successFlag = true;

		// Begin drawing
		ef->FXBegin(&passCount, 0);
		if (passCount > 0)
		{
			// Begin pass
			ef->FXBeginPass(0);

			// Draw element
			successFlag = m_pImpl->mfDraw(ef, sfm, gRenDev->m_RP.m_pCurObject);

			// End pass
			ef->FXEndPass();
		}
		// End drawing
		ef->FXEnd();

		return successFlag;
	}
	return false;
}
