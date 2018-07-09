// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ObserverComponent.h"

#include "Components/Factions/FactionComponent.h"
#include "Factions/FactionSystemSchematyc.h"

#include <CryAnimation/ICryAnimation.h>
#include <CryAISystem/IAIDebugRenderer.h>

struct SObserverUserConditionSignal
{
	static void ReflectType(Schematyc::CTypeDesc<SObserverUserConditionSignal>& desc)
	{
		desc.SetGUID("b3ad9aba-8faa-4d2e-aba8-175bf0422eea"_cry_guid);
		desc.SetLabel("ObserverCustomFilter");

		desc.AddMember(&SObserverUserConditionSignal::observableEntityId, 'oeid', "ObservableEntity", "Observable Entity id", nullptr, Schematyc::ExplicitEntityId::Invalid);
	}
	Schematyc::ExplicitEntityId observableEntityId;
};

struct SObserverVisibilityChangeSignal
{
	static void ReflectType(Schematyc::CTypeDesc<SObserverVisibilityChangeSignal>& desc)
	{
		desc.SetGUID("043f89a0-e486-4db1-90ed-51c8c1247d32"_cry_guid);
		desc.SetLabel("VisibilityChanged");

		desc.AddMember(&SObserverVisibilityChangeSignal::observableEntityId, 'oeid', "ObservableEntity", "Observable Entity id", nullptr, Schematyc::ExplicitEntityId::Invalid);
		desc.AddMember(&SObserverVisibilityChangeSignal::bVisible, 'vis', "Visible", "Visible", nullptr, false);
	}

	Schematyc::ExplicitEntityId observableEntityId;
	bool                        bVisible;
};

//////////////////////////////////////////////////////////////////////////

CEntityAIObserverComponent::CEntityAIObserverComponent()
	: m_entityEventMask(ENTITY_EVENT_BIT(ENTITY_EVENT_RESET) | ENTITY_EVENT_BIT(ENTITY_EVENT_START_GAME))
{
}

CEntityAIObserverComponent::~CEntityAIObserverComponent()
{
}

void CEntityAIObserverComponent::ReflectType(Schematyc::CTypeDesc<CEntityAIObserverComponent>& desc)
{
	desc.AddBase<IEntityObserverComponent>();
	desc.SetGUID("C8B964E9-31B8-4231-9454-BA7BFC877527"_cry_guid);

	desc.SetLabel("AI Observer");
	desc.SetDescription("Observer component");
	desc.SetEditorCategory("AI");
	desc.SetIcon("icons:Navigation/Move_Classic.ico");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });
	desc.AddComponentInteraction(SEntityComponentRequirements::EType::SoftDependency, IEntity::SComponentType<CEntityAIFactionComponent>::GetGUID());

	desc.AddMember(&CEntityAIObserverComponent::m_visionMapType, 'vmt', "visionMapType", "Vision Map Type", "Combination of flags to identify type of the observer.", Perception::ComponentHelpers::SVisionMapType());
	desc.AddMember(&CEntityAIObserverComponent::m_visionProperties, 'vpt', "visionProps", "Vision", "Configuration of the vision sensor with which the entity can observe the world.", ObserverProperties::SVisionProperties());
	desc.AddMember(&CEntityAIObserverComponent::m_typesToObserve, 'tto', "typesToObserve", "Types To Observe", "Only entities that belong to these vision map types will be processed for sight.", Perception::ComponentHelpers::SVisionMapType());
	desc.AddMember(&CEntityAIObserverComponent::m_factionsToObserve, 'fto', "factionsToObserve", "Factions To Observe", "Only entities that belong to these factions will be processed for sight.", SFactionFlagsMask());
	desc.AddMember(&CEntityAIObserverComponent::m_bUseUserCustomCondition, 'cfc', "useCustomFilter", "Use Custom Filter", "Flag for enabling custom condition filter.", false);
}

void CEntityAIObserverComponent::Register(Schematyc::IEnvRegistrar& registrar)
{
	Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
	{
		Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CEntityAIObserverComponent));

		// Functions
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAIObserverComponent::ObserverUserConditionResult, "0a8b5ff0-ee54-473b-b59c-f56e08c26dcd"_cry_guid, "ReturnUserConditionResult");
			pFunction->SetDescription("Used for filtering out potential visible targets. Return true if observer should continue with checking visibility with observable entity.");
			pFunction->BindInput(1, 'res', "IsObservable", "Is Observable?", false);
			componentScope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAIObserverComponent::CanSeeSchematyc, "e6b5e92d-397a-474d-b322-6998c31f5b74"_cry_guid, "CanSee");
			pFunction->SetDescription("Returns true if the entity is currently visible by the observer.");
			pFunction->BindInput(1, 'ent', "ObservableEntity", "Observable Entity id");
			pFunction->BindOutput(0, 'see', "IsVisible", "True if the entity is visible by the Observer");
			componentScope.Register(pFunction);
		}
		// Signals
		{
			componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SObserverUserConditionSignal));
			componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SObserverVisibilityChangeSignal));
		}
	}
}

void CEntityAIObserverComponent::OnShutDown()
{
	Reset(EEntitySimulationMode::Idle);
}

void CEntityAIObserverComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_UPDATE:
	case ENTITY_EVENT_XFORM:
		Update();
		break;
	case ENTITY_EVENT_RESET:
		Reset(GetEntity()->GetSimulationMode());
		break;
	case ENTITY_EVENT_START_GAME:
		{
			m_entityEventMask |= m_visionProperties.location.type == Perception::ComponentHelpers::SLocation::EType::Bone
			                     ? ENTITY_EVENT_BIT(ENTITY_EVENT_UPDATE) : ENTITY_EVENT_BIT(ENTITY_EVENT_XFORM);
			GetEntity()->UpdateComponentEventMask(this);
			break;
		}
	}
}

void CEntityAIObserverComponent::Reset(EEntitySimulationMode simulationMode)
{
	if (simulationMode == EEntitySimulationMode::Game)
	{
		RegisterToVisionMap();
	}
	else
	{
		UnregisterFromVisionMap();
		m_entityEventMask = ENTITY_EVENT_BIT(ENTITY_EVENT_RESET) | ENTITY_EVENT_BIT(ENTITY_EVENT_START_GAME);
	}
}

void CEntityAIObserverComponent::RegisterToVisionMap()
{
	IVisionMap* pVisionMap = gEnv->pAISystem->GetVisionMap();
	if (!pVisionMap)
		return;

	IEntity* pEntity = GetEntity();

	// Init parameters
	m_params = ObserverParams();

	m_params.entityId = pEntity->GetId();
	m_params.callback = functor(*this, &CEntityAIObserverComponent::OnObserverVisionChanged);
	if (m_bUseUserCustomCondition)
	{
		m_params.userConditionCallback = functor(*this, &CEntityAIObserverComponent::OnObserverUserCondition);
	}

	//TODO: Remove faction and vision map types dependency from vision map
	CEntityAIFactionComponent* pFactionComponent = pEntity->GetComponent<CEntityAIFactionComponent>();
	m_params.faction = pFactionComponent ? pFactionComponent->GetFactionId() : IFactionMap::InvalidFactionID;

	m_params.factionsToObserveMask = m_factionsToObserve.mask;
	m_params.typeMask = m_visionMapType.mask;
	m_params.typesToObserveMask = m_typesToObserve.mask;

	m_params.sightRange = m_visionProperties.range;
	m_params.fovCos = crymath::cos(m_visionProperties.fov.ToRadians());
	
	m_visionProperties.location.Validate(pEntity, "AI Observer Component");
	SyncWithEntity();

	m_params.raycastFlags = GetRaycastFlags();

	//, userData(0)
	//, skipListSize(0)

	// Register in Vision map
	stack_string visionIdName = pEntity->GetName();
	visionIdName += ".Observer";

	m_changeHintFlags = EChangeHint();
	m_observerId = pVisionMap->CreateVisionID(visionIdName);
	pVisionMap->RegisterObserver(m_observerId, m_params);
}

void CEntityAIObserverComponent::UnregisterFromVisionMap()
{
	if (IsRegistered())
	{
		gEnv->pAISystem->GetVisionMap()->UnregisterObserver(m_observerId);
		m_observerId = ObserverID();
	}
}

void CEntityAIObserverComponent::Update()
{
	if (IsRegistered())
	{
		SyncWithEntity();
		UpdateChange();
	}
}

bool CEntityAIObserverComponent::CanSee(const EntityId entityId) const
{
	if (!IsRegistered())
		return false;

	return m_visibleEntitiesSet.find(static_cast<EntityId>(entityId)) != m_visibleEntitiesSet.end();
}

bool CEntityAIObserverComponent::CanSeeSchematyc(Schematyc::ExplicitEntityId entityId) const
{
	return CanSee(static_cast<EntityId>(entityId));
}

bool CEntityAIObserverComponent::OnObserverUserCondition(const VisionID& observerId, const ObserverParams& observerParams, const VisionID& observableId, const ObservableParams& observableParams)
{
	if (m_userConditionCallback)
	{
		return m_userConditionCallback(observerId, observerParams, observableId, observableParams);
	}
	
	m_bUserConditionResult = true;
	if (GetEntity()->GetSchematycObject())
	{
		GetEntity()->GetSchematycObject()->ProcessSignal(SObserverUserConditionSignal { Schematyc::ExplicitEntityId(observableParams.entityId) }, GetGUID());
	}
	return m_bUserConditionResult;
}

void CEntityAIObserverComponent::ObserverUserConditionResult(bool bResult)
{
	m_bUserConditionResult = bResult;
}

void CEntityAIObserverComponent::OnObserverVisionChanged(const VisionID& observerId, const ObserverParams& observerParams, const VisionID& observableId, const ObservableParams& observableParams, bool visible)
{
	CRY_ASSERT(IsRegistered() && observerId == m_observerId);

	if (visible)
	{
		m_visibleEntitiesSet.insert(observableParams.entityId);
	}
	else
	{
		m_visibleEntitiesSet.erase(observableParams.entityId);
	}
	if (GetEntity()->GetSchematycObject())
	{
		GetEntity()->GetSchematycObject()->ProcessSignal(SObserverVisibilityChangeSignal { Schematyc::ExplicitEntityId(observableParams.entityId), visible }, GetGUID());
	}
}

void CEntityAIObserverComponent::UpdateChange()
{
	gEnv->pAISystem->GetVisionMap()->ObserverChanged(m_observerId, m_params, m_changeHintFlags);
	m_changeHintFlags = EChangeHint();
}

void CEntityAIObserverComponent::UpdateChange(uint32 changeHintFlags)
{
	if (IsRegistered())
	{
		gEnv->pAISystem->GetVisionMap()->ObserverChanged(m_observerId, m_params, changeHintFlags);
	}
}

void CEntityAIObserverComponent::SyncWithEntity()
{
	m_changeHintFlags = EChangeHint(m_changeHintFlags | eChangedPosition | eChangedOrientation);
	m_visionProperties.location.GetPositionAndOrientation(GetEntity(), &m_params.eyePosition, &m_params.eyeDirection);
}

uint32 CEntityAIObserverComponent::GetRaycastFlags() const
{
	uint32 flags = 0;
	if (m_visionProperties.blockingProperties.bBlockedBySolids)
	{
		flags |= AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER;
	}
	if (m_visionProperties.blockingProperties.bBlockedBySoftCover)
	{
		flags |= AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOFT_COVER;
	}
	return flags;
}

//////////////////////////////////////////////////////////////////////////

void CEntityAIObserverComponent::SetUserConditionCallback(const UserConditionCallback callback)
{
	m_userConditionCallback = callback;

	uint32 changeHint = eChangedUserCondition;

	if (!m_bUseUserCustomCondition)
	{
		changeHint = eChangedUserConditionCallback;
		m_bUseUserCustomCondition = true;
	}

	if (IsRegistered())
	{
		m_params.userConditionCallback = functor(*this, &CEntityAIObserverComponent::OnObserverUserCondition);
		UpdateChange(changeHint);
	}
}

void CEntityAIObserverComponent::SetTypeMask(const uint32 typeMask)
{
	m_visionMapType.mask = typeMask;

	m_params.typeMask = typeMask;
	UpdateChange(eChangedTypeMask);
}

void CEntityAIObserverComponent::SetTypesToObserveMask(const uint32 typesToObserverMask)
{
	m_typesToObserve.mask = typesToObserverMask;

	m_params.typesToObserveMask = typesToObserverMask;
	UpdateChange(eChangedTypesToObserveMask);
}

void CEntityAIObserverComponent::SetFactionsToObserveMask(const uint32 factionsToObserveMask)
{
	m_factionsToObserve.mask = factionsToObserveMask;

	m_params.factionsToObserveMask = factionsToObserveMask;
	UpdateChange(eChangedFactionsToObserveMask);
}

void CEntityAIObserverComponent::SetFOV(const float fovInRad)
{
	m_visionProperties.fov = CryTransform::CAngle::FromRadians(fovInRad);
	
	m_params.fovCos = crymath::cos(m_visionProperties.fov.ToRadians());
	UpdateChange(eChangedFOV);
}

void CEntityAIObserverComponent::SetRange(const float range)
{
	m_visionProperties.range = range;

	m_params.sightRange = range;
	UpdateChange(eChangedSightRange);
}

void CEntityAIObserverComponent::SetPivotOffset(const Vec3& offsetFromPivot)
{
	m_visionProperties.location.offset = offsetFromPivot;
	m_visionProperties.location.type = Perception::ComponentHelpers::SLocation::EType::Pivot;
	
	Update();
}

void CEntityAIObserverComponent::SetBoneOffset(const Vec3& offsetFromBone, const char* szBoneName)
{
	m_visionProperties.location.offset = offsetFromBone;
	m_visionProperties.location.boneName = szBoneName;
	m_visionProperties.location.type = Perception::ComponentHelpers::SLocation::EType::Pivot;

	Update();
}

uint32 CEntityAIObserverComponent::GetTypeMask() const
{
	return m_visionMapType.mask;
}

uint32 CEntityAIObserverComponent::GetTypesToObserveMask() const
{
	return m_typesToObserve.mask;
}

uint32 CEntityAIObserverComponent::GetFactionsToObserveMask() const
{
	return m_factionsToObserve.mask;
}

float CEntityAIObserverComponent::GetFOV() const
{
	return m_visionProperties.fov.ToRadians();
}

float CEntityAIObserverComponent::GetRange() const
{
	return m_visionProperties.range;
}

//////////////////////////////////////////////////////////////////////////
