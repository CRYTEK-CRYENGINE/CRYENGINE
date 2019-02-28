// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ObservableComponent.h"

#include "Components/Factions/FactionComponent.h"

struct SObservableVisibilityChangeSignal
{
	static void ReflectType(Schematyc::CTypeDesc<SObservableVisibilityChangeSignal>& desc)
	{
		desc.SetGUID("c0989593-1022-4438-bedf-1039714f21e2"_cry_guid);
		desc.SetLabel("VisibilityChange");

		desc.AddMember(&SObservableVisibilityChangeSignal::observerEntityId, 'oeid', "ObserverEntity", "ObserverEntity", nullptr, Schematyc::ExplicitEntityId::Invalid);
		desc.AddMember(&SObservableVisibilityChangeSignal::bVisibile, 'vis', "Visible", "Visible", nullptr, false);
	}

	Schematyc::ExplicitEntityId observerEntityId;
	bool bVisibile;
};

CEntityAIObservableComponent::CEntityAIObservableComponent()
	: m_entityEventMask(ENTITY_EVENT_BIT(ENTITY_EVENT_RESET) | ENTITY_EVENT_BIT(ENTITY_EVENT_START_GAME))
{

}

CEntityAIObservableComponent::~CEntityAIObservableComponent()
{

}

void CEntityAIObservableComponent::ReflectType(Schematyc::CTypeDesc<CEntityAIObservableComponent>& desc)
{
	desc.AddBase<IEntityObservableComponent>();
	desc.SetGUID("5A32746A-9CEA-4877-B9E4-5C4E06EEE90C"_cry_guid);

	desc.SetLabel("AI Observable");
	desc.SetDescription("Observable component");
	desc.SetEditorCategory("AI");
	desc.SetIcon("icons:Navigation/Move_Classic.ico");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });
	desc.AddComponentInteraction(SEntityComponentRequirements::EType::SoftDependency, IEntity::SComponentType<CEntityAIFactionComponent>::GetGUID());

	desc.AddMember(&CEntityAIObservableComponent::m_visionMapType, 'vmt', "visionMapType", "Vision Map Type", "", Perception::ComponentHelpers::SVisionMapType());
	desc.AddMember(&CEntityAIObservableComponent::m_observableLocations, 'oba', "observableLocations", "Observable Locations", "", Perception::ComponentHelpers::SLocationsArray());
}

void CEntityAIObservableComponent::Register(Schematyc::IEnvRegistrar& registrar)
{
	Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
	{
		Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CEntityAIObservableComponent));

		// Signals
		{
			componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SObservableVisibilityChangeSignal));
		}
	}
}

void CEntityAIObservableComponent::OnShutDown()
{
	Reset(EEntitySimulationMode::Idle);
}

void CEntityAIObservableComponent::ProcessEvent(const SEntityEvent& event)
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
		m_entityEventMask |= IsUsingBones() ? ENTITY_EVENT_BIT(ENTITY_EVENT_UPDATE) : ENTITY_EVENT_BIT(ENTITY_EVENT_XFORM);
		GetEntity()->UpdateComponentEventMask(this);
		break;
	}
	}
}

void CEntityAIObservableComponent::Reset(EEntitySimulationMode simulationMode)
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

void CEntityAIObservableComponent::RegisterToVisionMap()
{
	IVisionMap* pVisionMap = gEnv->pAISystem->GetVisionMap();
	if (!pVisionMap)
		return;
	
	IEntity* pEntity = GetEntity();

	// Init parameters
	m_params = ObservableParams();

	m_params.entityId = pEntity->GetId();
	m_params.callback = functor(*this, &CEntityAIObservableComponent::OnObservableVisionChanged);

	//TODO: Remove faction and vision map types dependency from vision map
	m_params.typeMask = m_visionMapType.mask;

	CEntityAIFactionComponent* pFactionComponent = pEntity->GetComponent<CEntityAIFactionComponent>();
	m_params.faction = pFactionComponent ? pFactionComponent->GetFactionId() : IFactionMap::InvalidFactionID;

	//	, userData(0)
	//	, skipListSize(0)

	for (const Perception::ComponentHelpers::SLocation& location : m_observableLocations.locations)
	{
		location.Validate(pEntity, "AI Observable Component");
	}
	SyncWithEntity();

	// Register in Vision map
	stack_string visionIdName = pEntity->GetName();
	visionIdName += ".Observable";

	m_changeHintFlags = EChangeHint();
	m_observableId = pVisionMap->CreateVisionID(visionIdName);
	pVisionMap->RegisterObservable(m_observableId, m_params);
}

void CEntityAIObservableComponent::UnregisterFromVisionMap()
{
	if (IsRegistered())
	{
		gEnv->pAISystem->GetVisionMap()->UnregisterObservable(m_observableId);
		m_observableId = ObservableID();
	}
}

void CEntityAIObservableComponent::Update()
{
	if (IsRegistered())
	{
		SyncWithEntity();
		UpdateChange();
	}
}

void CEntityAIObservableComponent::OnObservableVisionChanged(const VisionID& observerId, const ObserverParams& observerParams, const VisionID& observableId, const ObservableParams& observableParams, bool visible)
{
	CRY_ASSERT(IsRegistered() && observableId == m_observableId);

	if (GetEntity()->GetSchematycObject())
	{
		GetEntity()->GetSchematycObject()->ProcessSignal(SObservableVisibilityChangeSignal{ Schematyc::ExplicitEntityId(observerParams.entityId), visible }, GetGUID());
	}
}

void CEntityAIObservableComponent::UpdateChange()
{
	gEnv->pAISystem->GetVisionMap()->ObservableChanged(m_observableId, m_params, m_changeHintFlags);
	m_changeHintFlags = EChangeHint();
}

void CEntityAIObservableComponent::UpdateChange(const uint32 changeHintFlags)
{
	if (IsRegistered())
	{
		gEnv->pAISystem->GetVisionMap()->ObservableChanged(m_observableId, m_params, changeHintFlags);
	}
}

void CEntityAIObservableComponent::SyncWithEntity()
{
	m_changeHintFlags = EChangeHint(m_changeHintFlags | eChangedPosition);

	size_t locationsCount = min(size_t(ObservableParams::MaxPositionCount), m_observableLocations.locations.size());
	if (locationsCount > 0)
	{
		for (size_t i = 0; i < locationsCount; ++i)
		{
			m_observableLocations.locations[i].GetPositionAndOrientation(GetEntity(), &m_params.observablePositions[i], nullptr);
		}
		m_params.observablePositionsCount = uint8(locationsCount);
	}
	else
	{
		m_params.observablePositions[0] = GetWorldTransformMatrix().GetTranslation();
		m_params.observablePositionsCount = 1;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityAIObservableComponent::SetTypeMask(const uint32 typeMask)
{
	m_visionMapType.mask = typeMask;

	m_params.typeMask = typeMask;
	UpdateChange(eChangedTypeMask);
}

void CEntityAIObservableComponent::AddObservableLocationOffsetFromPivot(const Vec3& offsetFromPivot)
{
	Perception::ComponentHelpers::SLocation location;
	location.offset = offsetFromPivot;
	location.type = Perception::ComponentHelpers::SLocation::EType::Pivot;
	
	m_observableLocations.locations.push_back(location);
	Update();
}

void CEntityAIObservableComponent::AddObservableLocationOffsetFromBone(const Vec3& offsetFromBone, const char* szBoneName)
{
	Perception::ComponentHelpers::SLocation location;
	location.offset = offsetFromBone;
	location.boneName = szBoneName;
	location.type = Perception::ComponentHelpers::SLocation::EType::Bone;

	m_observableLocations.locations.push_back(location);
	Update();
}

void CEntityAIObservableComponent::SetObservableLocationOffsetFromPivot(const size_t index, const Vec3& offsetFromPivot)
{
	if (index < m_observableLocations.locations.size())
	{
		Perception::ComponentHelpers::SLocation& location = m_observableLocations.locations[index];
		location.offset = offsetFromPivot;
		location.type = Perception::ComponentHelpers::SLocation::EType::Pivot;

		Update();
	}
}

void CEntityAIObservableComponent::SetObservableLocationOffsetFromBone(const size_t index, const Vec3& offsetFromBone, const char* szBoneName)
{
	if (index < m_observableLocations.locations.size())
	{
		Perception::ComponentHelpers::SLocation& location = m_observableLocations.locations[index];		
		location.offset = offsetFromBone;
		location.boneName = szBoneName;
		location.type = Perception::ComponentHelpers::SLocation::EType::Bone;

		Update();
	}
}

//////////////////////////////////////////////////////////////////////////
