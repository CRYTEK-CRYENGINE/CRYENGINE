// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AxisHelperExtended.h"

#include <CryPhysics/IPhysics.h>

#include "Objects/DisplayContext.h"
#include "Objects/ISelectionGroup.h"
#include "IDisplayViewport.h"
#include "IEditor.h"
#include "IObjectManager.h"

//////////////////////////////////////////////////////////////////////////
// Helper Extended Axis object.
//////////////////////////////////////////////////////////////////////////
CAxisHelperExtended::CAxisHelperExtended()
	: m_matrix(IDENTITY)
	, m_position(ZERO)
	, m_pCurObject(0)
	, m_lastUpdateTime(0)
	, m_lastDistNegX(-1.0f)
	, m_lastDistNegY(-1.0f)
	, m_lastDistNegZ(-1.0f)
	, m_lastDistPosX(-1.0f)
	, m_lastDistPosY(-1.0f)
	, m_lastDistPosZ(-1.0f)
{
}

//////////////////////////////////////////////////////////////////////////
void CAxisHelperExtended::DrawAxes(DisplayContext& dc, const Matrix34& matrix)
{
	const DWORD updateTime = 2000; // 2 sec
	const ISelectionGroup* const pSelection = GetIEditor()->GetISelectionGroup();
	const size_t numSels = pSelection->GetCount();
	if (numSels == 0)
	{
		return;
	}

	m_skipEntities.clear();
	m_skipEntities.reserve(numSels);
	for (size_t i = 0; i < numSels; ++i)
	{
		const CBaseObject* const pObject = pSelection->GetObject(i);
		IPhysicalEntity* const pSkipEntity = pObject->GetCollisionEntity();
		if (pSkipEntity)
		{
			m_skipEntities.push_back(pSkipEntity);
		}
	}

	const Vec3 unitX = Vec3Constants<float>::fVec3_OneX;
	const Vec3 unitY = Vec3Constants<float>::fVec3_OneY;
	const Vec3 unitZ = Vec3Constants<float>::fVec3_OneZ;

	const Vec3 colR(1.0f, 0.0f, 0.0f);
	const Vec3 colG(0.0f, 1.0f, 0.0f);
	const Vec3 colB(0.0f, 0.8f, 1.0f);

	m_position = matrix.GetTranslation();
	const Vec3 vDirX = (matrix * unitX - m_position).GetNormalized();
	const Vec3 vDirY = (matrix * unitY - m_position).GetNormalized();
	const Vec3 vDirZ = (matrix * unitZ - m_position).GetNormalized();

	CBaseObject* const pCurObject = pSelection->GetObject(numSels - 1); // get just last object for simple check
	if (m_pCurObject != pCurObject || GetTickCount() - m_lastUpdateTime > updateTime || !Matrix34::IsEquivalent(m_matrix, matrix))
	{
		IPhysicalEntity** pSkip = (m_skipEntities.size() != 0) ? &m_skipEntities[0] : nullptr;
		ray_hit xHitPos, xHitNeg, yHitPos, yHitNeg, zHitPos, zHitNeg;

		const bool bHasHitPosX = gEnv->pPhysicalWorld->RayWorldIntersection(m_position, vDirX * ms_maxDist, ent_all, rwi_any_hit, &xHitPos, 1, pSkip, m_skipEntities.size());
		const bool bHasHitPosY = gEnv->pPhysicalWorld->RayWorldIntersection(m_position, vDirY * ms_maxDist, ent_all, rwi_any_hit, &yHitPos, 1, pSkip, m_skipEntities.size());
		const bool bHasHitPosZ = gEnv->pPhysicalWorld->RayWorldIntersection(m_position, vDirZ * ms_maxDist, ent_all, rwi_any_hit, &zHitPos, 1, pSkip, m_skipEntities.size());
		const bool bHasHitNegX = gEnv->pPhysicalWorld->RayWorldIntersection(m_position, -vDirX * ms_maxDist, ent_all, rwi_any_hit, &xHitNeg, 1, pSkip, m_skipEntities.size());
		const bool bHasHitNegY = gEnv->pPhysicalWorld->RayWorldIntersection(m_position, -vDirY * ms_maxDist, ent_all, rwi_any_hit, &yHitNeg, 1, pSkip, m_skipEntities.size());
		const bool bHasHitNegZ = gEnv->pPhysicalWorld->RayWorldIntersection(m_position, -vDirZ * ms_maxDist, ent_all, rwi_any_hit, &zHitNeg, 1, pSkip, m_skipEntities.size());

		m_lastDistPosX = bHasHitPosX ? xHitPos.dist : -1.0f;
		m_lastDistNegX = bHasHitNegX ? xHitNeg.dist : -1.0f;
		m_lastDistPosY = bHasHitPosY ? yHitPos.dist : -1.0f;
		m_lastDistNegY = bHasHitNegY ? yHitNeg.dist : -1.0f;
		m_lastDistPosZ = bHasHitPosZ ? zHitPos.dist : -1.0f;
		m_lastDistNegZ = bHasHitNegZ ? zHitNeg.dist : -1.0f;

		m_lastUpdateTime = GetTickCount();
	}
	m_pCurObject = pCurObject;
	m_matrix = matrix;

	DrawAxis(dc, vDirX, m_lastDistPosX, unitZ, colR);
	DrawAxis(dc, -vDirX, m_lastDistNegX, unitZ, colR);
	DrawAxis(dc, vDirY, m_lastDistPosY, unitZ, colG);
	DrawAxis(dc, -vDirY, m_lastDistNegY, unitZ, colG);
	DrawAxis(dc, vDirZ, m_lastDistPosZ, unitX, colB);
	DrawAxis(dc, -vDirZ, m_lastDistNegZ, unitX, colB);
}

//////////////////////////////////////////////////////////////////////////
void CAxisHelperExtended::DrawAxis(DisplayContext& dc, const Vec3& direction, const float dist, const Vec3& up, const Vec3& color)
{
	const float BALL_SIZE = 0.005f;
	const float TEXT_SIZE = 1.4f;
	const int MIN_STEP_COUNT = 5;
	const int MAX_STEP_COUNT = 20;

	if (dist > 0.0f && dist < ms_maxDist)
	{
		const Vec3 point = m_position + direction * dist;

		dc.SetColor(color);
		dc.DrawLine(m_position, point);
		const float screenScale = dc.view->GetScreenScaleFactor(point);
		dc.DrawBall(point, BALL_SIZE * screenScale);
		string label;
		label.Format("%.2f", dist);
		dc.DrawTextOn2DBox((point + m_position) * 0.5f, label, TEXT_SIZE, color, ColorF(0.0f, 0.0f, 0.0f, 0.7f));

		const Vec3 vUp = (m_matrix * up - m_position).GetNormalized();
		const Vec3 v = direction ^ vUp;
		const float alphaMax = 1.0f, alphaMin = 0.0f;
		const ColorF colAlphaMin = ColorF(color.x, color.y, color.z, alphaMin);

		const float planeSize = dist * 0.25f;
		const float stepSize = dc.view->GetGridStep();
		const int steps = std::min(std::max((int)(planeSize / stepSize + 0.5f), MIN_STEP_COUNT), MAX_STEP_COUNT);
		const float gridSize = steps * stepSize;

		for (int i = -steps; i <= steps; ++i)
		{
			const Vec3 stepV = v * (stepSize * i);
			const Vec3 stepU = vUp * (stepSize * i);
			const ColorF colCurAlpha(color.x, color.y, color.z, alphaMax - fabsf(static_cast<float>(i) / static_cast<float>(steps)) * (alphaMax - alphaMin));

			// Draw u lines.
			dc.DrawLine(point + stepV, point + vUp * gridSize + stepV, colCurAlpha, colAlphaMin);
			dc.DrawLine(point + stepV, point - vUp * gridSize + stepV, colCurAlpha, colAlphaMin);

			// Draw v lines.
			dc.DrawLine(point + stepU, point + v * gridSize + stepU, colCurAlpha, colAlphaMin);
			dc.DrawLine(point + stepU, point - v * gridSize + stepU, colCurAlpha, colAlphaMin);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
const float CAxisHelperExtended::ms_maxDist = 1000.0f;

