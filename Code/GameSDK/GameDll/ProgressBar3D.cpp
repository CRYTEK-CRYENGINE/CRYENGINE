// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProgressBar3D.h"

#include <CryRenderer/IRenderAuxGeom.h>

CProgressBar3D::CProgressBar3D()
{
}

CProgressBar3D::~CProgressBar3D()
{
}

void CProgressBar3D::Init(const ProgressBarParams& params, const ProgressBar3DParams& params3D)
{
	m_params = params;
	m_params3D = params3D;
}

void CProgressBar3D::Render3D()
{
	IRenderAuxGeom  *pRenderAuxGeom ( gEnv->pRenderer->GetIRenderAuxGeom() );
	if(pRenderAuxGeom)
	{
		IActor* pClient = g_pGame->GetIGameFramework()->GetClientActor();
		if(!pClient)
		{
			return;
		}

		SAuxGeomRenderFlags oldFlags = pRenderAuxGeom->GetRenderFlags();
		pRenderAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags|e_AlphaBlended);

		IEntity* pEntity = m_params3D.m_EntityId ? gEnv->pEntitySystem->GetEntity(m_params3D.m_EntityId) : NULL;

		//Get target matrix
		Matrix34 targetMatrix;
		if(pEntity)
		{
			targetMatrix = pEntity->GetWorldTM();
		}
		else
		{
			targetMatrix.CreateIdentity();
			targetMatrix.AddTranslation(m_params3D.m_Offset);
		}

		//Work out bar facing direction
		Vec3 centre = targetMatrix.GetTranslation();

		Vec3 dir = centre - pClient->GetEntity()->GetWorldPos();
		dir.Normalize();
		Vec3 up = targetMatrix.GetColumn2();
		up.Normalize();
		Vec3 cross = dir.cross(up);
		cross.Normalize();

		if(pEntity)
		{
			//Add on offset using facing/cross directions
			centre += m_params3D.m_Offset.x * cross;
			centre += m_params3D.m_Offset.y * dir;
			centre += m_params3D.m_Offset.z * up;
		}

		//Work out the 6 points required to draw the triangles
		float fillRatio = m_params.m_zeroToOneProgressValue;

		float fillWidth = m_params3D.m_width * m_params.m_zeroToOneProgressValue;

		Vec3 halfWidth = ( m_params3D.m_width * 0.5f) * cross;
		Vec3 halfHeight = (m_params3D.m_height * 0.5f) * up;

		Vec3 point1 = centre - ( halfWidth ) + (halfHeight);
		Vec3 point2 = point1 + ( cross * fillWidth );
		Vec3 point3 = centre + ( halfWidth ) + (halfHeight);

		Vec3 point4 = centre + ( halfWidth ) - (halfHeight);
		Vec3 point6 = centre - ( halfWidth ) - (halfHeight);
		Vec3 point5 = point6 + ( cross * fillWidth );

		//And draw the necessary triangles
		if(fillRatio > 0.0001f)
		{
			pRenderAuxGeom->DrawTriangle(point1, m_params.m_filledBarColor, point5, m_params.m_filledBarColor, point2, m_params.m_filledBarColor);
			pRenderAuxGeom->DrawTriangle(point1, m_params.m_filledBarColor, point6, m_params.m_filledBarColor, point5, m_params.m_filledBarColor);
		}

		if(fillRatio < 0.9999f)
		{
			pRenderAuxGeom->DrawTriangle(point2, m_params.m_emptyBarColor, point4, m_params.m_emptyBarColor, point3, m_params.m_emptyBarColor);
			pRenderAuxGeom->DrawTriangle(point2, m_params.m_emptyBarColor, point5, m_params.m_emptyBarColor, point4, m_params.m_emptyBarColor);
		}

		pRenderAuxGeom->SetRenderFlags(oldFlags);
	}
}

void CProgressBar3D::SetProgressValue( float zeroToOneProgress )
{
	m_params.m_zeroToOneProgressValue = zeroToOneProgress;
}
