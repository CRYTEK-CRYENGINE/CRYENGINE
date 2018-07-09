// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description: Flash animated door panel HSM

-------------------------------------------------------------------------
History:
- 03:04:2012: Created by Dean Claassen

*************************************************************************/

#include "StdAfx.h"
#include "DoorPanel.h"

#include "../Game.h"
#include "../GameCache.h"

#include "../SShootHelper.h"
#include "../WeaponSystem.h"
#include "../Audio/AudioSignalPlayer.h"
#include "UI/HUD/HUDUtils.h"

#define DOOR_PANEL_MODEL_NORMAL								"Objects/default/primitive_box.cgf"
#define DOOR_PANEL_MODEL_DESTROYED						"Objects/default/primitive_sphere.cgf"

#define DOOR_PANEL_DESTROYED_EXPLOSION_TYPE		"SmartMineExplosive"

#define DOOR_PANEL_ORIGINA_ANIMATED_MATERIAL  "Materials/basecolors/base_green_bright" 

#define DOOR_PANEL_FLASHOBJECT_ROOT						"_root"

#define DOOR_PANEL_VISIBLEDISTANCE						50.0f

#define DOOR_PANEL_SCANSUCCESS_TIMEDELAY			0.5f
#define DOOR_PANEL_SCANFAILURE_TIMEDELAY			1.5f

struct SDoorPanelMaterialInstanceCountData
{
	SDoorPanelMaterialInstanceCountData()
		: m_iCount(0)
	{
	}

	int m_iCount;
};

typedef CryFixedStringT<64> TDoorPanelMaterialName;

typedef std::map<TDoorPanelMaterialName, SDoorPanelMaterialInstanceCountData> TDoorPanelMaterialNameToInstanceCountMap; // Need to support different materials to avoid memory lost from incorrect ref count

// cppcheck-suppress uninitMemberVar
class CDoorPanelBehavior : public CStateHierarchy< CDoorPanel >
{
	DECLARE_STATE_CLASS_BEGIN( CDoorPanel, CDoorPanelBehavior )
		DECLARE_STATE_CLASS_ADD( CDoorPanel, Idle )
		DECLARE_STATE_CLASS_ADD( CDoorPanel, Scanning )
		DECLARE_STATE_CLASS_ADD( CDoorPanel, ScanSuccess )
		DECLARE_STATE_CLASS_ADD( CDoorPanel, ScanComplete )
		DECLARE_STATE_CLASS_ADD( CDoorPanel, ScanFailure )
		DECLARE_STATE_CLASS_ADD( CDoorPanel, Destroyed )
	DECLARE_STATE_CLASS_END( CDoorPanel )

private:
	void SetupDoorPanel( CDoorPanel& doorPanel );
	void DrawSlot( CDoorPanel& doorPanel, const int iSlot, const bool bEnable );
	void SwitchSlot( CDoorPanel& doorPanel, const int iSlot );
	IMaterial* CreateClonedMaterial( IEntity* pEntity, const int iSlotIndex );
	bool SetupFlash( CDoorPanel& doorPanel );
	bool SetupFlashFromMaterial( IMaterial* pMaterial, CDoorPanel& doorPanel );
	void UpdateFlashVisibility( CDoorPanel& doorPanel, const bool bVisible );
	void DeinitFlashResources( CDoorPanel& doorPanel );
	void PreCacheResources( CDoorPanel& doorPanel );
	const char* GetDestroyedExplosionType( IEntity* pEntity ); // Temporary till destroyed details worked out
	
	void ChangeMaterial( CDoorPanel& doorPanel, const int iSlot, IMaterial* pMaterial );
	void ChangeMaterial( CDoorPanel& doorPanel, const int iSlot, const char* szMaterial );
	void ReplaceAnimatedMaterial( CDoorPanel& doorPanel, IMaterial* pAnimatedMaterial );
	void RestoreOriginalMaterial( CDoorPanel& doorPanel );

	const CDoorPanelBehavior::TStateIndex GetStateFromStateId( const EDoorPanelBehaviorState stateId );

	CryFixedStringT<32>		m_flashState;
	_smart_ptr<IMaterial> m_pOriginalAnimatedMaterial;
	_smart_ptr<IMaterial> m_pClonedAnimatedMaterial; // Can't destroy during runtime so keep this here even if not using it
	_smart_ptr<IMaterial> m_pActualAnimatedMaterial;
	IFlashVariableObject* m_pFlashObjectRoot;
	float									m_fScanSuccessTimeDelay;
	float									m_fScanFailureTimeDelay;
	int										m_iCurrentVisibleSlot;
	bool									m_bScannable;

	static void IncrementNumUniqueFlashInstances(const char* szMaterialName);
	static void DecrementNumUniqueFlashInstances(const char* szMaterialName);
	static int GetNumNumUniqueFlashInstances(const char* szMaterialName);
	
	static TDoorPanelMaterialNameToInstanceCountMap	s_materialNameToInstanceCountMap;
};

DEFINE_STATE_CLASS_BEGIN( CDoorPanel, CDoorPanelBehavior, eDoorPanelState_Behavior, Idle )
	// Constructor member initialization
	m_pFlashObjectRoot = nullptr;
	m_fScanSuccessTimeDelay = 0.0f;
	m_fScanFailureTimeDelay = 0.0f;
	m_iCurrentVisibleSlot = 0;
	m_bScannable = false;
	DEFINE_STATE_CLASS_ADD( CDoorPanel, CDoorPanelBehavior, Idle, Root )
	DEFINE_STATE_CLASS_ADD( CDoorPanel, CDoorPanelBehavior, Scanning, Root )
	DEFINE_STATE_CLASS_ADD( CDoorPanel, CDoorPanelBehavior, ScanSuccess, Root )
	DEFINE_STATE_CLASS_ADD( CDoorPanel, CDoorPanelBehavior, ScanComplete, Root )
	DEFINE_STATE_CLASS_ADD( CDoorPanel, CDoorPanelBehavior, ScanFailure, Root )
	DEFINE_STATE_CLASS_ADD( CDoorPanel, CDoorPanelBehavior, Destroyed, Root )
DEFINE_STATE_CLASS_END( CDoorPanel, CDoorPanelBehavior )

const CDoorPanelBehavior::TStateIndex CDoorPanelBehavior::Root( CDoorPanel& doorPanel, const SStateEvent& event )
{
	switch ( event.GetEventId() )
	{
	case STATE_EVENT_INIT:
		{
			m_iCurrentVisibleSlot = -1;
			m_pFlashObjectRoot = NULL;
			m_bScannable = false;
			m_flashState = "";

			PreCacheResources( doorPanel );
			SetupDoorPanel( doorPanel );
		}
		break;
	case STATE_EVENT_RELEASE:
		{
			DeinitFlashResources( doorPanel );
			RestoreOriginalMaterial( doorPanel );
			m_pOriginalAnimatedMaterial.reset();
			m_pClonedAnimatedMaterial.reset();
		}
		break;
	case STATE_EVENT_DOOR_PANEL_RESET:
		{
			const SDoorPanelResetEvent& eventCasted = static_cast< const SDoorPanelResetEvent& >( event );
			const bool bEnteringGameMode = eventCasted.IsEnteringGameMode();
			if (bEnteringGameMode)
			{
				SetupFlash( doorPanel );
			}
			else
			{
				DeinitFlashResources( doorPanel );
				RestoreOriginalMaterial( doorPanel );
			}
		}
		break;
	case STATE_EVENT_DOOR_PANEL_FORCE_STATE:
		{
			const SDoorPanelStateEventForceState& stateEventForceState = static_cast< const SDoorPanelStateEventForceState& >( event );
			const EDoorPanelBehaviorState forcedStateId = stateEventForceState.GetForcedState();
			return GetStateFromStateId(forcedStateId);
		}
		break;
	case STATE_EVENT_DOOR_PANEL_VISIBLE:
		{
			const SDoorPanelVisibleEvent& eventCasted = static_cast< const SDoorPanelVisibleEvent& >( event );
			const bool bVisible = eventCasted.IsVisible();
			UpdateFlashVisibility( doorPanel, bVisible );
		}
		break;
	}

	return State_Done;
}

const CDoorPanelBehavior::TStateIndex CDoorPanelBehavior::Idle( CDoorPanel& doorPanel, const SStateEvent& event )
{
	switch ( event.GetEventId() )
	{
	case STATE_EVENT_ENTER:
		{
			if (m_bScannable)
			{
				doorPanel.AddToTacticalManager();
			}

			doorPanel.NotifyBehaviorStateEnter( eDoorPanelBehaviorState_Idle );

			const char* szFlashState = "Idle";
			m_flashState = szFlashState;
			if (m_pFlashObjectRoot)
			{
				m_pFlashObjectRoot->GotoAndStop(szFlashState);
			}
		}
		break;
	}

	return State_Continue;
}

const CDoorPanelBehavior::TStateIndex CDoorPanelBehavior::Scanning( CDoorPanel& doorPanel, const SStateEvent& event )
{
	switch ( event.GetEventId() )
	{
	case STATE_EVENT_ENTER:
		{
			doorPanel.NotifyBehaviorStateEnter( eDoorPanelBehaviorState_Scanning );

			const char* szFlashState = "Scanning";
			m_flashState = szFlashState;
			if (m_pFlashObjectRoot)
			{
				m_pFlashObjectRoot->GotoAndStop(szFlashState);
			}
		}
		break;
	}

	return State_Continue;
}

const CDoorPanelBehavior::TStateIndex CDoorPanelBehavior::ScanSuccess( CDoorPanel& doorPanel, const SStateEvent& event )
{
	switch ( event.GetEventId() )
	{
	case STATE_EVENT_ENTER:
		{
			doorPanel.NotifyBehaviorStateEnter( eDoorPanelBehaviorState_ScanSuccess );

			const char* szFlashState = "ScanSuccess";
			m_flashState = szFlashState;
			if (m_pFlashObjectRoot)
			{
				m_pFlashObjectRoot->GotoAndStop(szFlashState);
			}

			// Set up delayed state switch for animation
			doorPanel.SetDelayedStateChange(eDoorPanelBehaviorState_ScanComplete, m_fScanSuccessTimeDelay);
		}
		break;
	}

	return State_Continue;
}

const CDoorPanelBehavior::TStateIndex CDoorPanelBehavior::ScanComplete( CDoorPanel& doorPanel, const SStateEvent& event )
{
	switch ( event.GetEventId() )
	{
	case STATE_EVENT_ENTER:
		{
			doorPanel.NotifyBehaviorStateEnter( eDoorPanelBehaviorState_ScanComplete );

			if (m_bScannable)
			{
				doorPanel.RemoveFromTacticalManager();
			}

			const char* szFlashState = "ScanSuccessComplete";
			m_flashState = szFlashState;
			if (m_pFlashObjectRoot)
			{
				m_pFlashObjectRoot->GotoAndStop(szFlashState);
			}
		}
		break;
	}

	return State_Continue;
}

const CDoorPanelBehavior::TStateIndex CDoorPanelBehavior::ScanFailure( CDoorPanel& doorPanel, const SStateEvent& event )
{
	switch ( event.GetEventId() )
	{
	case STATE_EVENT_ENTER:
		{
			doorPanel.NotifyBehaviorStateEnter( eDoorPanelBehaviorState_ScanFailure );
			const char* szFlashState = "ScanFailure";
			m_flashState = szFlashState;

			if (m_pFlashObjectRoot)
			{
				m_pFlashObjectRoot->GotoAndStop(szFlashState);
			}

			// Set up delayed state switch for animation
			doorPanel.SetDelayedStateChange(eDoorPanelBehaviorState_Idle, m_fScanFailureTimeDelay);
		}
		break;
	}

	return State_Continue;
}

const CDoorPanelBehavior::TStateIndex CDoorPanelBehavior::Destroyed( CDoorPanel& doorPanel, const SStateEvent& event )
{
	switch ( event.GetEventId() )
	{
	case STATE_EVENT_ENTER:
		{
			doorPanel.NotifyBehaviorStateEnter( eDoorPanelBehaviorState_Destroyed );
			const char* szFlashState = "BrokenScreen";
			m_flashState = szFlashState;

			if (m_pFlashObjectRoot)
			{
				m_pFlashObjectRoot->GotoAndStop(szFlashState);
			}

			IEntity* pEntity = doorPanel.GetEntity();
			Matrix34 doorPanelWorldTM = pEntity->GetWorldTM();
			SShootHelper::Explode( doorPanel.GetEntityId(), 0, GetDestroyedExplosionType(pEntity), doorPanelWorldTM.GetTranslation(), doorPanelWorldTM.GetColumn2().GetNormalized(), 800 );
			if (m_bScannable)
			{
				doorPanel.RemoveFromTacticalManager();
			}

			SwitchSlot( doorPanel, DOOR_PANEL_MODEL_DESTROYED_SLOT );
		}
		break;
	}

	return State_Continue;
}

//////////////////////////////////////////////////////////////////////////

const CDoorPanelBehavior::TStateIndex CDoorPanelBehavior::GetStateFromStateId( const EDoorPanelBehaviorState stateId )
{
	if (stateId == eDoorPanelBehaviorState_Scanning)
	{
		return State_Scanning;
	}
	else if (stateId == eDoorPanelBehaviorState_ScanSuccess)
	{
		return State_ScanSuccess;
	}
	else if (stateId == eDoorPanelBehaviorState_ScanComplete)
	{
		return State_ScanComplete;
	}
	else if (stateId == eDoorPanelBehaviorState_ScanFailure)
	{
		return State_ScanFailure;
	}
	else if (stateId == eDoorPanelBehaviorState_Destroyed)
	{
		return State_Destroyed;
	}
	else
	{
		return State_Idle;
	}
}

void CDoorPanelBehavior::SetupDoorPanel( CDoorPanel& doorPanel )
{
	const char* szModelName = DOOR_PANEL_MODEL_NORMAL;
	const char* szModelDestroyedName = DOOR_PANEL_MODEL_DESTROYED;
	bool bDestroyable = true;
	bool bScannable = m_bScannable;
	float fVisibleDistance = DOOR_PANEL_VISIBLEDISTANCE;
	float fScanSuccessTimeDelay = DOOR_PANEL_SCANSUCCESS_TIMEDELAY;
	float fScanFailureTimeDelay = DOOR_PANEL_SCANFAILURE_TIMEDELAY;

	IEntity* pEntity = doorPanel.GetEntity();
	CRY_ASSERT(pEntity != NULL);

	IScriptTable* pScriptTable = pEntity->GetScriptTable();
	if (pScriptTable != NULL)
	{
		SmartScriptTable propertiesTable;
		if (pScriptTable->GetValue("Properties", propertiesTable))
		{
			propertiesTable->GetValue("objModel", szModelName);
			propertiesTable->GetValue("objModelDestroyed", szModelDestroyedName);
			propertiesTable->GetValue("bDestroyable", bDestroyable);
			propertiesTable->GetValue("VisibleFlashDistance", fVisibleDistance);
			propertiesTable->GetValue("ScanSuccessTimeDelay", fScanSuccessTimeDelay);
			propertiesTable->GetValue("ScanFailureTimeDelay", fScanFailureTimeDelay);

			SmartScriptTable tacticalInfoTable;
			if (propertiesTable->GetValue("TacticalInfo", tacticalInfoTable))
			{
				tacticalInfoTable->GetValue("bScannable", bScannable);
				m_bScannable = bScannable;
			}
		}
	}

	m_fScanSuccessTimeDelay = fScanSuccessTimeDelay;
	m_fScanFailureTimeDelay = fScanFailureTimeDelay;

	doorPanel.m_fVisibleDistanceSq = fVisibleDistance*fVisibleDistance;

	// Load model
	pEntity->LoadGeometry( DOOR_PANEL_MODEL_NORMAL_SLOT, szModelName );

	if (bDestroyable)
	{
		pEntity->LoadGeometry( DOOR_PANEL_MODEL_DESTROYED_SLOT, szModelDestroyedName );
		DrawSlot(doorPanel, DOOR_PANEL_MODEL_DESTROYED_SLOT, false);
	}

	// Switch model and physicalize it
	SwitchSlot(doorPanel, DOOR_PANEL_MODEL_NORMAL_SLOT);

	// Other proxies
	pEntity->GetOrCreateComponent<IEntityAudioComponent>();
}

bool CDoorPanelBehavior::SetupFlash( CDoorPanel& doorPanel )
{
	DeinitFlashResources( doorPanel );

	bool bResult = false;

	IEntity* pEntity = doorPanel.GetEntity();
	CRY_ASSERT(pEntity != NULL);

	// Setup flash material
	_smart_ptr<IMaterial> pAnimatedMaterial;
	_smart_ptr<IMaterial> pOriginalMaterial;

	if ( m_pOriginalAnimatedMaterial )
	{
		pOriginalMaterial = m_pOriginalAnimatedMaterial;
	}
	else
	{
		IEntityRender* pIEntityRender = pEntity->GetRenderInterface();
		
		{
			pOriginalMaterial = pIEntityRender->GetRenderMaterial(DOOR_PANEL_MODEL_NORMAL_SLOT);
			m_pOriginalAnimatedMaterial = pOriginalMaterial;
		}
	}

	const EntityId entityToShareScreen = doorPanel.m_sharingMaterialEntity;

	if (pOriginalMaterial)
	{
		//if (CDoorPanelBehavior::GetNumNumUniqueFlashInstances(pOriginalMaterial->GetName()) >= 0) // Need to clone

		// Currently want to always clone if not sharing and remove flash player instance from original to save its memory (Layer switching off would always cause original to be deactivated which would be a problem since wrong door panel could have its texture removed)
		pOriginalMaterial->ActivateDynamicTextureSources(false);

		if (entityToShareScreen == 0)
		{
			if ( m_pClonedAnimatedMaterial == NULL )
			{
				pAnimatedMaterial = CreateClonedMaterial(pEntity, DOOR_PANEL_MODEL_NORMAL_SLOT);
			}
			else // Materials don't get deleted at runtime so if its created once will be here
			{
				pAnimatedMaterial = m_pClonedAnimatedMaterial;
			}

			if (pAnimatedMaterial)
			{
				ReplaceAnimatedMaterial(doorPanel, pAnimatedMaterial);
			}
			else
			{
				GameWarning("CDoorPanelBehavior::SetupFlash: Failed to clone material");
				return false;
			}
		}
		else
		{
			IEntity* pEntityToShareScreen = gEnv->pEntitySystem->GetEntity(entityToShareScreen);
			CRY_ASSERT(pEntityToShareScreen != NULL);
			if (pEntityToShareScreen != NULL)
			{
				IEntityRender* pEntityToShareRenderProxy = (pEntityToShareScreen->GetRenderInterface());
				if (pEntityToShareRenderProxy)
				{
					pAnimatedMaterial = pEntityToShareRenderProxy->GetRenderMaterial(DOOR_PANEL_MODEL_NORMAL_SLOT);
					if (pAnimatedMaterial)
					{
						ReplaceAnimatedMaterial(doorPanel, pAnimatedMaterial);
					}
					else
					{
						GameWarning("CDoorPanelBehavior::SetupFlash: Failed to find material from sharing screen entity");
					}
				}
			}
			else
			{
				GameWarning("CDoorPanelBehavior::SetupFlash: Unable to find entity to share screen");
			}
		}
	}
	else
	{
		GameWarning("CDoorPanelBehavior::SetupFlash: Failed finding original material");
	}
	
	// Setup flash object + callback
	if (pAnimatedMaterial)
	{
		bResult = SetupFlashFromMaterial( pAnimatedMaterial, doorPanel );
		if (bResult)
		{
			if (entityToShareScreen == 0) // Only increment if not sharing
			{
				CDoorPanelBehavior::IncrementNumUniqueFlashInstances(pAnimatedMaterial->GetName());
				doorPanel.NotifyScreenSharingEvent(eDoorPanelGameObjectEvent_StartShareScreen);
			}
			doorPanel.AssignAsFSCommandHandler();
		}
		else
		{
			DeinitFlashResources( doorPanel );
			GameWarning("CDoorPanelBehavior::SetupFlash: Failed getting flash object: %s", DOOR_PANEL_FLASHOBJECT_ROOT);
		}
	}
	
	return bResult;
}

void CDoorPanelBehavior::DrawSlot( CDoorPanel& doorPanel, const int iSlot, const bool bEnable )
{
	IEntity* pEntity = doorPanel.GetEntity();
	
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

void CDoorPanelBehavior::SwitchSlot( CDoorPanel& doorPanel, const int iSlot )
{
	if (m_iCurrentVisibleSlot != -1)
	{
		DrawSlot(doorPanel, m_iCurrentVisibleSlot, false);
	}
	DrawSlot(doorPanel, iSlot, true);
	m_iCurrentVisibleSlot = iSlot;

	SEntityPhysicalizeParams physicalizeParams;
	physicalizeParams.nSlot = iSlot;
	physicalizeParams.type = PE_STATIC;

	doorPanel.GetEntity()->Physicalize( physicalizeParams );
}

IMaterial* CDoorPanelBehavior::CreateClonedMaterial( IEntity* pEntity, const int iSlotIndex )
{
	CRY_ASSERT( m_pClonedAnimatedMaterial == NULL );

	IMaterial* pClonedMaterial = NULL;

	IEntityRender* pIEntityRender = pEntity->GetRenderInterface();
	
	{
		IMaterial* pOriginalMaterial = pIEntityRender->GetRenderMaterial(iSlotIndex);
		if (pOriginalMaterial)
		{
			IMaterialManager* pMatManager = gEnv->p3DEngine->GetMaterialManager();
			CRY_ASSERT(pMatManager);

			pClonedMaterial = pMatManager->CloneMaterial( pOriginalMaterial );
			if (pClonedMaterial)
			{
				m_pClonedAnimatedMaterial = pClonedMaterial;
				pClonedMaterial->SetFlags( pClonedMaterial->GetFlags()|MTL_FLAG_PURE_CHILD ); // Notify that its not shared so won't try to remove by name in material manager which removes original material
			}
			else
			{
				GameWarning("CDoorPanelBehavior::CreateClonedMaterial: Failed to clone material");
			}
		}
		else
		{
			GameWarning("CDoorPanelBehavior::CreateClonedMaterial: Failed to get material from slot: %d", DOOR_PANEL_MODEL_NORMAL_SLOT);
		}
	}

	return pClonedMaterial;
}

bool CDoorPanelBehavior::SetupFlashFromMaterial( IMaterial* pMaterial, CDoorPanel& doorPanel )
{
	bool bSetupFlash = false;

	pMaterial->ActivateDynamicTextureSources(true); // Ensure player is created

	IFlashPlayer* pFlashPlayer = CHUDUtils::GetFlashPlayerFromMaterialIncludingSubMaterials(pMaterial, true);
	if (pFlashPlayer)
	{
		IFlashVariableObject* pFlashObject = NULL;
		pFlashPlayer->GetVariable(DOOR_PANEL_FLASHOBJECT_ROOT, pFlashObject);
		if (pFlashObject)
		{
			m_pFlashObjectRoot = pFlashObject;
			bSetupFlash = true;
		}
		else
		{
			GameWarning("CDoorPanelBehavior::SetupFlashFromMaterial: Failed to get flash object: %s", DOOR_PANEL_FLASHOBJECT_ROOT);
		}
	}
	else
	{
		GameWarning("CDoorPanelBehavior::SetupFlashFromMaterial: Failed to get flash player from material");
	}
	
	return bSetupFlash;
}

void CDoorPanelBehavior::UpdateFlashVisibility( CDoorPanel& doorPanel, const bool bVisible )
{
	if ( bVisible )
	{
		if (m_pFlashObjectRoot == NULL)
		{
			SetupFlash( doorPanel );

			// Need to force update flash
			if (m_pFlashObjectRoot)
			{
				m_pFlashObjectRoot->GotoAndStop(m_flashState.c_str());
			}
		}
	}
	else
	{
		if (m_pFlashObjectRoot != NULL)
		{
			DeinitFlashResources( doorPanel );
			RestoreOriginalMaterial( doorPanel );
		}
	}
}

void CDoorPanelBehavior::DeinitFlashResources( CDoorPanel& doorPanel )
{
	if (m_pFlashObjectRoot)
	{
		SAFE_RELEASE(m_pFlashObjectRoot);

		if (doorPanel.m_sharingMaterialEntity == 0)
		{
			IEntity* pEntity = doorPanel.GetEntity();

			IEntityRender* pIEntityRender = pEntity->GetRenderInterface();
			
			{
				IMaterial* pMaterial = pIEntityRender->GetRenderMaterial(DOOR_PANEL_MODEL_NORMAL_SLOT);
				if (pMaterial)
				{
					IFlashPlayer* pFlashPlayer = CHUDUtils::GetFlashPlayerFromMaterialIncludingSubMaterials(pMaterial, true);
					if (pFlashPlayer)
					{
						pFlashPlayer->SetFSCommandHandler(NULL);
					}

					CDoorPanelBehavior::DecrementNumUniqueFlashInstances(pMaterial->GetName());
					doorPanel.NotifyScreenSharingEvent(eDoorPanelGameObjectEvent_StopShareScreen);
				}
			}

			if (m_pActualAnimatedMaterial)
			{
				m_pActualAnimatedMaterial->ActivateDynamicTextureSources(false);
			}
		}
	}
}

void CDoorPanelBehavior::PreCacheResources( CDoorPanel& doorPanel )
{
	IEntity* pEntity = doorPanel.GetEntity();
	CRY_ASSERT(pEntity != NULL);

	IEntityClass* pAmmoClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass( GetDestroyedExplosionType(pEntity) );
	if (pAmmoClass != NULL)
	{
		const SAmmoParams* pAmmoParams = g_pGame->GetWeaponSystem()->GetAmmoParams( pAmmoClass );
		if (pAmmoParams != NULL)
		{
			pAmmoParams->CacheResources();
		}
	}
}

const char* CDoorPanelBehavior::GetDestroyedExplosionType( IEntity* pEntity )
{
	CRY_ASSERT(pEntity != NULL);

	const char* szModelDestroyedName = DOOR_PANEL_MODEL_DESTROYED;

	IScriptTable* pScriptTable = pEntity->GetScriptTable();
	if (pScriptTable != NULL)
	{
		SmartScriptTable propertiesTable;
		if (pScriptTable->GetValue("Properties", propertiesTable))
		{
			propertiesTable->GetValue("DestroyedExplosionType", szModelDestroyedName);
		}
	}

	return szModelDestroyedName;
}

void CDoorPanelBehavior::ChangeMaterial( CDoorPanel& doorPanel, const int iSlot, IMaterial* pMaterial )
{
	if (pMaterial != NULL)
	{
		doorPanel.GetEntity()->SetSlotMaterial( iSlot, pMaterial );
	}
}

void CDoorPanelBehavior::ChangeMaterial( CDoorPanel& doorPanel, const int iSlot, const char* szMaterial )
{
	IMaterial* pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial( szMaterial );
	ChangeMaterial( doorPanel, iSlot, pMaterial );
}

void CDoorPanelBehavior::ReplaceAnimatedMaterial( CDoorPanel& doorPanel, IMaterial* pAnimatedMaterial )
{
	ChangeMaterial( doorPanel, DOOR_PANEL_MODEL_NORMAL_SLOT, pAnimatedMaterial );
	ChangeMaterial( doorPanel, DOOR_PANEL_MODEL_DESTROYED_SLOT, pAnimatedMaterial ); // For now destroyed model is using an animated texture as well
	m_pActualAnimatedMaterial = pAnimatedMaterial;
}

void CDoorPanelBehavior::RestoreOriginalMaterial( CDoorPanel& doorPanel )
{
	if (m_pOriginalAnimatedMaterial)
	{
		ChangeMaterial(doorPanel, DOOR_PANEL_MODEL_NORMAL_SLOT, m_pOriginalAnimatedMaterial);
		ChangeMaterial(doorPanel, DOOR_PANEL_MODEL_DESTROYED_SLOT, m_pOriginalAnimatedMaterial);
		m_pActualAnimatedMaterial.reset();
	}
}

// Static related for instance count management
TDoorPanelMaterialNameToInstanceCountMap CDoorPanelBehavior::s_materialNameToInstanceCountMap;

void CDoorPanelBehavior::IncrementNumUniqueFlashInstances(const char* szMaterialName)
{
	SDoorPanelMaterialInstanceCountData& countData = s_materialNameToInstanceCountMap[szMaterialName]; // Creates if not there already
	countData.m_iCount++;
}

void CDoorPanelBehavior::DecrementNumUniqueFlashInstances(const char* szMaterialName)
{
	TDoorPanelMaterialNameToInstanceCountMap::iterator iter = s_materialNameToInstanceCountMap.find(szMaterialName);
	if (iter != s_materialNameToInstanceCountMap.end())
	{
		SDoorPanelMaterialInstanceCountData& countData = iter->second;
		const int iCount = countData.m_iCount - 1;
		if (iCount == 0) // Nolonger need data in map
		{
			s_materialNameToInstanceCountMap.erase(iter);
		}
		else
		{
			countData.m_iCount--;
		}
	}
	else
	{
		GameWarning("CDoorPanelBehavior::DecrementNumUniqueFlashInstances: Failed to find data for material: %s", szMaterialName);
	}
}

int CDoorPanelBehavior::GetNumNumUniqueFlashInstances(const char* szMaterialName)
{
	TDoorPanelMaterialNameToInstanceCountMap::iterator iter = s_materialNameToInstanceCountMap.find(szMaterialName);
	if (iter != s_materialNameToInstanceCountMap.end())
	{
		SDoorPanelMaterialInstanceCountData& countData = iter->second;
		return countData.m_iCount;
	}
	else
	{
		return 0;
	}
}
