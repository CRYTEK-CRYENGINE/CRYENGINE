// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AIMoveSimulation.h"
#include "Objects\EntityObject.h"
#include "GameEngine.h"
#include <CryAISystem/IAIObject.h>
#include "Viewport.h"
#include <CryAISystem/IAIActorProxy.h>
#include <CryAISystem/IMovementSystem.h>
#include <CryAISystem/MovementRequest.h>

//////////////////////////////////////////////////////////////////////////
CAIMoveSimulation::CAIMoveSimulation()
{
}

//////////////////////////////////////////////////////////////////////////
CAIMoveSimulation::~CAIMoveSimulation()
{
	CancelMove();
}

//////////////////////////////////////////////////////////////////////////
void CAIMoveSimulation::CancelMove()
{
	CRY_ASSERT(gEnv && gEnv->pAISystem && gEnv->pAISystem->GetMovementSystem());

	for (const SMovingAI& movingAI : m_movingAIs)
	{
		gEnv->pAISystem->GetMovementSystem()->CancelRequest(movingAI.m_movementRequestID);
	}
	m_movingAIs.clear();
}

//////////////////////////////////////////////////////////////////////////
bool CAIMoveSimulation::UpdateAIMoveSimulation(CViewport* pView, const CPoint& point)
{
	CGameEngine* pGameEngine = GetIEditorImpl()->GetGameEngine();
	CRY_ASSERT(pGameEngine);

	// Cancel the current move order
	CancelMove();

	const CSelectionGroup* selection = GetIEditorImpl()->GetSelection();
	if (!selection->GetCount())
		return false;

	std::vector<CEntityObject*> selectedEntityObjects;
	selectedEntityObjects.reserve(selection->GetCount());
	for (int i = 0; i < selection->GetCount(); ++i)
	{
		CBaseObject* selectedObject = selection->GetObject(i);
		if (selectedObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			selectedEntityObjects.push_back(static_cast<CEntityObject*>(selectedObject));
		}
	}

	if (selectedEntityObjects.size() == 0)
		return false;

	bool bResult = false;	
	Vec3 vGotoPoint(ZERO);
	if (GetAIMoveSimulationDestination(pView, point, vGotoPoint))
	{
		for (CEntityObject* pEntityObject : selectedEntityObjects)
		{
			IEntity* pEntity = pEntityObject->GetIEntity();
			if (pEntity) // Not every EntityObject actually has entity...
			{
				MovementRequestID movementRequestId = SendAIMoveSimulation(pEntity, vGotoPoint);
				if (!movementRequestId.IsInvalid())
				{
					m_movingAIs.push_back({ pEntityObject->GetId(), movementRequestId });
					bResult = true;
				}
			}
		}
	}
	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CAIMoveSimulation::GetAIMoveSimulationDestination(CViewport* pView, const CPoint& point, Vec3& outGotoPoint) const
{
	HitContext hitInfo;
	pView->HitTest(point, hitInfo);

	// TODO Get point or projected point on hit object's bounds
	CBaseObject* pHitObj = hitInfo.object;
	if (pHitObj)
	{
		AABB bbox;
		pHitObj->GetBoundBox(bbox);

		// TODO Get closest approachable point to bounds
		outGotoPoint = pView->SnapToGrid(pView->ViewToWorld(point));
	}
	else
	{
		outGotoPoint = pView->SnapToGrid(pView->ViewToWorld(point));
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
MovementStyle::Speed GetSpeedToUseForSimulation()
{
	MovementStyle::Speed speedToUse = MovementStyle::Run;

	if (const bool bShift = ((GetKeyState(VK_SHIFT) & 0x8000) != 0))
	{
		speedToUse = MovementStyle::Walk;
	}

	if (const bool bControl = ((GetKeyState(VK_MENU) & 0x8000) != 0))
	{
		speedToUse = MovementStyle::Sprint;
	}

	return speedToUse;
}

MovementRequestID CAIMoveSimulation::SendAIMoveSimulation(IEntity* pEntity, const Vec3& vGotoPoint)
{
	CRY_ASSERT(pEntity);

	// ensure that a potentially running animation doesn't block the movement
	if (pEntity->HasAI())
	{
		IAIObject* pAI = pEntity->GetAI();
		if (pAI)
		{
			if (IAIActorProxy* aiProxy = pAI->GetProxy())
			{
				aiProxy->ResetAGInput(AIAG_ACTION);
				aiProxy->SetAGInput(AIAG_ACTION, "idle", true);
			}
		}
	}

	MovementRequest request;
	request.type = MovementRequest::MoveTo;
	request.destination = vGotoPoint;
	request.style.SetSpeed(GetSpeedToUseForSimulation());
	request.style.SetStance(MovementStyle::Stand);
	request.entityID = pEntity->GetId();

	CRY_ASSERT(gEnv && gEnv->pAISystem && gEnv->pAISystem->GetMovementSystem());
	
	return gEnv->pAISystem->GetMovementSystem()->QueueRequest(request);
}

