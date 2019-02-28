// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description: Object that moves based on range to player

-------------------------------------------------------------------------
History:
- 10:11:2012: Created by Dean Claassen

*************************************************************************/

#include "StdAfx.h"
#include "VicinityDependentObjectMover.h"

#include "EntityUtility/EntityScriptCalls.h"
#include <GameObjects/GameObject.h>
#include "GameCache.h"

//////////////////////////////////////////////////////////////////////////

namespace VICINITYDEPENDENTOBJECTMOVER
{
	void RegisterEvents( IGameObjectExtension& goExt, IGameObject& gameObject )
	{
		gameObject.UnRegisterExtForEvents( &goExt, NULL, 0 );
		const int iScriptEventID = eGFE_ScriptEvent;
		gameObject.RegisterExtForEvents( &goExt, &iScriptEventID, 1 );
	}
}

//////////////////////////////////////////////////////////////////////////

#define VICINITYDEPENDENTOBJECTMOVER_MODEL_NORMAL						"Objects/default/primitive_box.cgf"
#define VICINITYDEPENDENTOBJECTMOVER_MODEL_NORMAL_SLOT			0

CVicinityDependentObjectMover::CVicinityDependentObjectMover()
: m_vOriginalPos(0.0f,0.0f,0.0f)
, m_vMoveToPos(0.0f,0.0f,0.0f)
, m_fMoveToDistance(10.0f)
, m_fMoveToDistanceSq(100.0f)
, m_fMoveToSpeed(10.0f)
, m_fMoveBackSpeed(10.0f)
, m_fAreaTriggerRange(10.0f)
, m_fAreaTriggerRangeSq(100.0f)
, m_fBackAreaTriggerRange(10.0f)
, m_fBackAreaTriggerRangeSq(100.0f)
, m_fForceMoveCompleteDistanceSq(1.0f)
, m_currentState(eObjectRangeMoverState_None)
, m_bUseAreaTrigger(false)
, m_bDisableAreaTriggerOnMoveComplete(false)
, m_bForcedReverseMoveToStartPos(false)
{
}

CVicinityDependentObjectMover::~CVicinityDependentObjectMover()
{
}

bool CVicinityDependentObjectMover::Init( IGameObject * pGameObject )
{
	SetGameObject( pGameObject );

	return true;
}

void CVicinityDependentObjectMover::PostInit( IGameObject * pGameObject )
{
	VICINITYDEPENDENTOBJECTMOVER::RegisterEvents( *this, *pGameObject );

	Reset( !gEnv->IsEditor() );
}

bool CVicinityDependentObjectMover::ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params )
{
	ResetGameObject();
	VICINITYDEPENDENTOBJECTMOVER::RegisterEvents( *this, *pGameObject );

	CRY_ASSERT_MESSAGE( false, "CVicinityDependentObjectMover::ReloadExtension not implemented" );

	return false;
}

bool CVicinityDependentObjectMover::GetEntityPoolSignature( TSerialize signature )
{
	CRY_ASSERT_MESSAGE( false, "CVicinityDependentObjectMover::GetEntityPoolSignature not implemented" );

	return true;
}

void CVicinityDependentObjectMover::Release()
{
	delete this;
}

void CVicinityDependentObjectMover::FullSerialize( TSerialize serializer )
{
	serializer.Value( "m_currentState", (int&)m_currentState );
	serializer.Value( "m_bUseAreaTrigger", m_bUseAreaTrigger );
	serializer.Value( "m_vOriginalPos", m_vOriginalPos );
	serializer.Value( "m_bForcedReverseMoveToStartPos", m_bForcedReverseMoveToStartPos );
	if (serializer.IsReading())
	{
		SetUpdate();
	}
}

void CVicinityDependentObjectMover::Update( SEntityUpdateContext& ctx, int slot )
{
#if !defined(_RELEASE)
	if ( gEnv->IsEditing() )
	{
		return;
	}
#endif //!defined(_RELEASE)

	IEntity* pEntity = GetEntity();
	CRY_ASSERT(pEntity);
	const Vec3& vCurWorldPos = pEntity->GetWorldPos();

	IEntity* pPlayerEntity = gEnv->pEntitySystem->GetEntity(g_pGame->GetIGameFramework()->GetClientActorId());
	if (pPlayerEntity)
	{
		const Vec3& vPlayerPos = pPlayerEntity->GetWorldPos();

		EObjectRangeMoverState currentState = m_currentState;
		if (m_bUseAreaTrigger) // Check if player is moving in/out
		{
			Vec3 vPosToTest; // Always want to take original position to determine distance if moved
			if (currentState != eObjectRangeMoverState_None)
			{
				vPosToTest = m_vOriginalPos;
			}
			else
			{
				vPosToTest = vCurWorldPos;
			}

			const float fSqDistance = vPosToTest.GetSquaredDistance( vPlayerPos );
			if( fSqDistance < m_fAreaTriggerRangeSq )
			{
				if (currentState != eObjectRangeMoverState_Moved)
				{
					currentState = eObjectRangeMoverState_MovingTo;
					SetState(currentState);
				}
			}
			else if ( fSqDistance >= m_fBackAreaTriggerRangeSq ) // Have separate trigger areas for moveto and moveback
			{
				if (currentState != eObjectRangeMoverState_None)
				{
					currentState = eObjectRangeMoverState_MovingBack;
					SetState(currentState);
				}
			}
		}

		// For now, always moving up or down
		switch (currentState)
		{
		case eObjectRangeMoverState_MovingTo:
			{
				Vec3 vMoveNormal = m_vMoveToPos - m_vOriginalPos;
				vMoveNormal.Normalize();

				const Vec3 vNewWorldPos = pEntity->GetWorldPos() + (vMoveNormal*m_fMoveToSpeed*ctx.fFrameTime);

				// Snap to keep final pos accurate
				const float fCurSqDistance = m_vOriginalPos.GetSquaredDistance( vNewWorldPos );
				if( fCurSqDistance >= m_fMoveToDistanceSq ) // Snap to keep final pos accurate
				{
					SetState(eObjectRangeMoverState_Moved);				
					ActivateOutputPortBool( "MoveToComplete" );
				}
				else
				{
					// Workaround for not using physics and incase player is moving really fast and will get there before movement is complete
					// Perform player close test, ensure reaches max height before player gets there
					const float fCurPlayerToCurPosSqDistance = vCurWorldPos.GetSquaredDistance( vPlayerPos );
					if( fCurPlayerToCurPosSqDistance <= m_fForceMoveCompleteDistanceSq )
					{
						SetState(eObjectRangeMoverState_Moved);				
						ActivateOutputPortBool( "MoveToComplete" );
					}
					else
					{
						pEntity->SetPos( vNewWorldPos );
					}
				}
			}
			break;
		case eObjectRangeMoverState_MovingBack:
			{
				Vec3 vMoveNormal = m_vOriginalPos - m_vMoveToPos;
				vMoveNormal.Normalize();

				const Vec3 vNewWorldPos = pEntity->GetWorldPos() + (vMoveNormal*m_fMoveBackSpeed*ctx.fFrameTime);

				// Snap to keep final pos accurate
				const float fCurSqDistance = m_vMoveToPos.GetSquaredDistance( vNewWorldPos );
				if( fCurSqDistance >= m_fMoveToDistanceSq ) // Snap to keep final pos accurate
				{
					SetState(eObjectRangeMoverState_None);
					ActivateOutputPortBool( "MoveBackComplete" );
				}
				else
				{
					// Perform player close test, ensure reaches max height before player gets there
					const float fCurPlayerToCurPosSqDistance = vCurWorldPos.GetSquaredDistance( vPlayerPos );
					if( fCurPlayerToCurPosSqDistance <= m_fForceMoveCompleteDistanceSq )
					{
						SetState(eObjectRangeMoverState_None);
						ActivateOutputPortBool( "MoveBackComplete" );
					}
					else
					{
						pEntity->SetPos( vNewWorldPos );
					}
				}
			}
			break;
		}
	}
}

void CVicinityDependentObjectMover::HandleEvent( const SGameObjectEvent& gameObjectEvent )
{
	const uint32 eventId = gameObjectEvent.event;
	void* pParam = gameObjectEvent.param;
	if ( (eventId == eGFE_ScriptEvent) && (pParam != NULL) )
	{
		const char* szEventName = static_cast<const char*>( pParam );
		if ( strcmp(szEventName, "EnableAreaTrigger") == 0 )
		{
			m_bUseAreaTrigger = true;
		}
		else if ( strcmp(szEventName, "DisableAreaTrigger") == 0 )
		{
			m_bUseAreaTrigger = false;
		}
		else if ( strcmp(szEventName, "MoveTo") == 0 )
		{
			SetState(eObjectRangeMoverState_MovingTo);
		}
		else if ( strcmp(szEventName, "MoveBack") == 0 )
		{
			SetState(eObjectRangeMoverState_MovingBack);
		}
		else if ( strcmp(szEventName, "Reset") == 0 )
		{
			SetState(eObjectRangeMoverState_None);
			ActivateOutputPortBool( "OnReset" );
		}
		else if ( strcmp(szEventName, "ForceToTargetPos") == 0 )
		{
			SetState(eObjectRangeMoverState_MovingTo);
			SetState(eObjectRangeMoverState_Moved);
			ActivateOutputPortBool( "OnForceToTargetPos" );
		}
		else if ( strcmp(szEventName, "ForceReverseMoveToStartPos") == 0 )
		{
			if (!m_bForcedReverseMoveToStartPos)
			{
				m_bForcedReverseMoveToStartPos = true;
				SetState(eObjectRangeMoverState_None);
				IEntity* pEntity = GetEntity();
				CRY_ASSERT(pEntity);

				const Matrix34 worldTransform = pEntity->GetWorldTM();
				const Vec3& vUpNormal = worldTransform.GetColumn2();
				const Vec3 newEntityStartPos = pEntity->GetWorldPos() + (-1.0f*vUpNormal*m_fMoveToDistance); // Set entity pos in opposite direction
				pEntity->SetPos(newEntityStartPos);
			}
		}
		else if ( strcmp(szEventName, "UpdateFromProperties") == 0 )
		{
			Reset(false);
		}

		SetUpdate();
	}
}

void CVicinityDependentObjectMover::ProcessEvent( const SEntityEvent& entityEvent )
{
	switch( entityEvent.event )
	{
	case ENTITY_EVENT_RESET:
		{
			const bool bEnteringGameMode = ( entityEvent.nParam[ 0 ] == 1 );
			Reset( bEnteringGameMode );
		}
		break;
	}
}

uint64 CVicinityDependentObjectMover::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_RESET);
}

void CVicinityDependentObjectMover::GetMemoryUsage( ICrySizer *pSizer ) const
{

}

void CVicinityDependentObjectMover::Reset( const bool bEnteringGameMode )
{
	m_bForcedReverseMoveToStartPos = false;
	m_currentState = eObjectRangeMoverState_None;
	SetupEntity();
	SetUpdate();
}

void CVicinityDependentObjectMover::SetUpdate()
{
#if !defined(_RELEASE)
	if ( gEnv->IsEditing() )
	{
		GetGameObject()->DisableUpdateSlot( this, VICINITYDEPENDENTOBJECTMOVER_MODEL_NORMAL_SLOT );
		return;
	}
#endif //!defined(_RELEASE)

	if (m_bUseAreaTrigger || m_currentState == eObjectRangeMoverState_MovingTo || m_currentState == eObjectRangeMoverState_MovingBack)
	{
		GetGameObject()->EnableUpdateSlot( this, VICINITYDEPENDENTOBJECTMOVER_MODEL_NORMAL_SLOT );
	}
	else
	{
		GetGameObject()->DisableUpdateSlot( this, VICINITYDEPENDENTOBJECTMOVER_MODEL_NORMAL_SLOT );
	}
}

void CVicinityDependentObjectMover::ActivateOutputPortBool( const char* szPortName )
{
	IEntity* pEntity = GetEntity();
	CRY_ASSERT( pEntity );

	IEntityClass::SEventInfo eventInfo;
	IEntityClass *pEntityClass = pEntity->GetClass();
	if ( pEntityClass->FindEventInfo( szPortName,eventInfo ) )
	{
		SEntityEvent event( ENTITY_EVENT_SCRIPT_EVENT );
		event.nParam[0] = (INT_PTR)szPortName;
		event.nParam[1] = IEntityClass::EVT_BOOL;

		bool bValue = true;
		event.nParam[2] = (INT_PTR)&bValue;

		GetEntity()->SendEvent( event );
	}
	else
	{
		GameWarning( "CVicinityDependentObjectMover::ActivateOutputPortBool: Called with undefined port %s", szPortName );
	}
}

void CVicinityDependentObjectMover::SetupEntity()
{
	const char* szModelName = VICINITYDEPENDENTOBJECTMOVER_MODEL_NORMAL;
	float fMoveToDistance = 10.0f;
	float fAreaTriggerRange = 10.0f;
	float fBackAreaTriggerRange = 10.0f;
	float fForceMoveCompleteDistance = 1.0f;

	IEntity* pEntity = GetEntity();
	CRY_ASSERT( pEntity != NULL );

	IScriptTable* pScriptTable = pEntity->GetScriptTable();
	if ( pScriptTable != NULL )
	{
		SmartScriptTable propertiesTable;
		if ( pScriptTable->GetValue( "Properties", propertiesTable) )
		{
			propertiesTable->GetValue( "objModel", szModelName );
			propertiesTable->GetValue( "fMoveToDistance", fMoveToDistance );
			propertiesTable->GetValue( "fMoveToSpeed", m_fMoveToSpeed );
			propertiesTable->GetValue( "fMoveBackSpeed", m_fMoveBackSpeed );
			propertiesTable->GetValue( "fAreaTriggerRange", fAreaTriggerRange );
			propertiesTable->GetValue( "fBackAreaTriggerRange", fBackAreaTriggerRange );
			propertiesTable->GetValue( "fForceMoveCompleteDistance", fForceMoveCompleteDistance );
			propertiesTable->GetValue( "bUseAreaTrigger", m_bUseAreaTrigger );
			propertiesTable->GetValue( "bDisableAreaTriggerOnMoveComplete", m_bDisableAreaTriggerOnMoveComplete );
		}
	}

	m_fMoveToDistance = fMoveToDistance;
	m_fMoveToDistanceSq = fMoveToDistance*fMoveToDistance;
	m_fAreaTriggerRange = fAreaTriggerRange;
	m_fAreaTriggerRangeSq = fAreaTriggerRange*fAreaTriggerRange;
	m_fBackAreaTriggerRange = fBackAreaTriggerRange;
	m_fBackAreaTriggerRangeSq = fBackAreaTriggerRange*fBackAreaTriggerRange;
	m_fForceMoveCompleteDistanceSq = fForceMoveCompleteDistance*fForceMoveCompleteDistance;

	// Load model
	pEntity->LoadGeometry( VICINITYDEPENDENTOBJECTMOVER_MODEL_NORMAL_SLOT, szModelName );

	// Draw slot and physicalize it
	DrawSlot( VICINITYDEPENDENTOBJECTMOVER_MODEL_NORMAL_SLOT, true );

	SEntityPhysicalizeParams physicalizeParams;
	physicalizeParams.nSlot = VICINITYDEPENDENTOBJECTMOVER_MODEL_NORMAL_SLOT;
	physicalizeParams.type = PE_RIGID;
	physicalizeParams.mass = 0;

	GetEntity()->Physicalize( physicalizeParams );
}

void CVicinityDependentObjectMover::DrawSlot( const int iSlot, const bool bEnable )
{
	IEntity* pEntity = GetEntity();

	if (pEntity)
	{
		int flags = pEntity->GetSlotFlags(iSlot);

		if (bEnable)
		{
			flags |= ENTITY_SLOT_RENDER;
		}
		else
		{
			flags &= ~(ENTITY_SLOT_RENDER);
		}

		pEntity->SetSlotFlags(iSlot, flags);	
	}
}

void CVicinityDependentObjectMover::SetState( const EObjectRangeMoverState state)
{
	if (m_currentState != state)
	{
		switch (state)
		{
		case eObjectRangeMoverState_None:
			{
				IEntity* pEntity = GetEntity();
				CRY_ASSERT(pEntity);

				/*
				if(IPhysicalEntity *pPhysics = GetEntity()->GetPhysics()) // Stop movement
				{
					pe_action_set_velocity	action_set_velocity;
					action_set_velocity.v = Vec3(0.0f, 0.0f, 0.0f);
					pPhysics->Action(&action_set_velocity);
				}
				*/

				pEntity->SetPos(m_vOriginalPos);
				m_currentState = eObjectRangeMoverState_None;
			}
			break;
		case eObjectRangeMoverState_MovingTo:
			{
				if (m_currentState != eObjectRangeMoverState_Moved)
				{
					if (m_currentState == eObjectRangeMoverState_None)
					{
						// Need to remember original pos before moving
						IEntity* pEntity = GetEntity();
						CRY_ASSERT(pEntity);
						const Vec3 vOriginalPos = pEntity->GetWorldPos();
						m_vOriginalPos = vOriginalPos;

						// Remember target pos
						const Matrix34 worldTransform = pEntity->GetWorldTM();
						const Vec3& vUpNormal = worldTransform.GetColumn2();
						m_vMoveToPos = vOriginalPos + (vUpNormal*m_fMoveToDistance);

						// Move with physics
						/*
						if(IPhysicalEntity *pPhysics = GetEntity()->GetPhysics())
						{
							pe_action_set_velocity	action_set_velocity;
							action_set_velocity.v = vUpNormal * m_fMoveToSpeed;
							pPhysics->Action(&action_set_velocity);
						}
						*/
					}

					m_currentState = eObjectRangeMoverState_MovingTo;

					ActivateOutputPortBool( "MoveToStart" );
				}
			}
			break;
		case eObjectRangeMoverState_MovingBack:
			{
				if (m_currentState != eObjectRangeMoverState_None)
				{
					m_currentState = eObjectRangeMoverState_MovingBack;

					// Move with physics
					/*
					IEntity* pEntity = GetEntity();
					CRY_ASSERT(pEntity);
					const Matrix34 worldTransform = pEntity->GetWorldTM();
					const Vec3& vUpNormal = worldTransform.GetColumn2();
					if(IPhysicalEntity *pPhysics = GetEntity()->GetPhysics())
					{
						pe_action_set_velocity	action_set_velocity;
						action_set_velocity.v = vUpNormal * -1.0f * m_fMoveBackSpeed;
						pPhysics->Action(&action_set_velocity);
					}
					*/
				}

				ActivateOutputPortBool( "MoveBackStart" );
			}
			break;
		case eObjectRangeMoverState_Moved:
			{
				IEntity* pEntity = GetEntity();
				CRY_ASSERT(pEntity);

				/*
				if(IPhysicalEntity *pPhysics = GetEntity()->GetPhysics()) // Stop movement
				{
					pe_action_set_velocity	action_set_velocity;
					action_set_velocity.v = Vec3(0.0f, 0.0f, 0.0f);
					pPhysics->Action(&action_set_velocity);
				}
				*/

				pEntity->SetPos( m_vMoveToPos );
				m_currentState = eObjectRangeMoverState_Moved;

				if (m_bDisableAreaTriggerOnMoveComplete)
				{
					m_bUseAreaTrigger = false;
				}

				ActivateOutputPortBool( "MoveToComplete" );
			}
			break;
		}
	}
}