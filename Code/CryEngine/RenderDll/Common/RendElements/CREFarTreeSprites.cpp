// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryRenderer/RenderElements/RendElement.h>
#include <CryRenderer/RenderElements/CREFarTreeSprites.h>
#include <Cry3DEngine/I3DEngine.h>

void CREFarTreeSprites::mfPrepare(bool bCheckOverflow)
{
	if (bCheckOverflow)
		gRenDev->FX_CheckOverflow(0, 0, this);

	gRenDev->m_RP.m_pRE = this;
	gRenDev->m_RP.m_RendNumIndices = 0;
	gRenDev->m_RP.m_RendNumVerts = 0;
}

bool CREFarTreeSprites::mfDraw(CShader* ef, SShaderPass* sfm)
{
	gRenDev->DrawObjSprites(m_arrVegetationSprites[gRenDev->m_RP.m_nProcessThreadID][0]);
	return true;
}
