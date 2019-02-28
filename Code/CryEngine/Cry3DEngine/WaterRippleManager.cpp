// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "WaterRippleManager.h"

int CWaterRippleManager::OnEventPhysCollision(const EventPhys* pEvent)
{
	EventPhysCollision* pEventPhysArea = (EventPhysCollision*)pEvent;
	if (!pEventPhysArea)
		return 1;

	// only add nearby hits
	if (pEventPhysArea->idmat[1] == gEnv->pPhysicalWorld->GetWaterMat())
	{
		// Compute the momentum of the object.
		// Clamp the mass so that particles and other "massless" objects still cause ripples.
		const float v = pEventPhysArea->vloc[0].GetLength();
		float fMomentum = v * max(pEventPhysArea->mass[0], 0.025f);

		// Removes small velocity collisions because too many small collisions cause too strong ripples.
		const float velRampMin = 1.0f;
		const float velRampMax = 10.0f;
		const float t = min(1.0f, max(0.0f, (v - velRampMin) / (velRampMax - velRampMin)));
		const float smoothstep = t * t * (3.0f - 2.0f * t);
		fMomentum *= smoothstep;

		if(gEnv->p3DEngine && fMomentum > 0.0f)
		{
			gEnv->p3DEngine->AddWaterRipple(pEventPhysArea->pt, 1.0f, fMomentum);
		}
	}

	return 1;
}

CWaterRippleManager::CWaterRippleManager()
{
}

CWaterRippleManager::~CWaterRippleManager()
{
	Finalize();
}

void CWaterRippleManager::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_waterRippleInfos);
}

void CWaterRippleManager::Initialize()
{
	if (gEnv->pPhysicalWorld)
	{
		gEnv->pPhysicalWorld->AddEventClient(EventPhysCollision::id, &OnEventPhysCollision, 1);
	}
}

void CWaterRippleManager::Finalize()
{
	if (gEnv->pPhysicalWorld)
	{
		gEnv->pPhysicalWorld->RemoveEventClient(EventPhysCollision::id, &OnEventPhysCollision, 1);
	}
}

void CWaterRippleManager::OnFrameStart()
{
	m_waterRippleInfos.clear();
}

void CWaterRippleManager::Render(const SRenderingPassInfo& passInfo)
{
	auto* pRenderView = passInfo.GetIRenderView();

	if (pRenderView && passInfo.IsRecursivePass())
	{
		return;
	}

	const float simGridSize = static_cast<float>(SWaterRippleInfo::WaveSimulationGridSize);
	const float simGridRadius = 1.41421356f * 0.5f * simGridSize;
	const float looseSimGridRadius = simGridRadius * 1.5f;
	const float looseSimGridRadius2 = looseSimGridRadius * looseSimGridRadius;
	const Vec3 camPos3D = passInfo.GetCamera().GetPosition();
	const Vec2 camPos2D(camPos3D.x, camPos3D.y);

	for (auto& ripple : m_waterRippleInfos)
	{
		// add a ripple which is approximately in the water simulation grid.
		const Vec2 pos2D(ripple.position.x, ripple.position.y);
		if ((pos2D - camPos2D).GetLength2() <= looseSimGridRadius2)
		{
			pRenderView->AddWaterRipple(ripple);
		}
	}
}

void CWaterRippleManager::AddWaterRipple(const Vec3 position, float scale, float strength)
{
	if (m_waterRippleInfos.size() < SWaterRippleInfo::MaxWaterRipplesInScene)
	{
		m_waterRippleInfos.emplace_back(position, scale, strength);
	}
}
