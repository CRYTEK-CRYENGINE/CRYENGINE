// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MannequinObject.h"

#include <CrySerialization/Decorators/Resources.h>

#include "IAnimatedCharacter.h"
#include "GameObjects/GameObject.h"

CRYREGISTER_CLASS(CMannequinObject);

CMannequinObject::CProperties::CProperties(CMannequinObject& owner)
	: modelFilePath()
	, actionControllerFilePath()
	, animationDatabaseFilePath()
	, ownerBackReference(owner)
{
}

const char* CMannequinObject::CProperties::GetLabel() const
{
	return "Mannequin";
}

void CMannequinObject::CProperties::SerializeProperties(Serialization::IArchive& archive)
{
	archive(Serialization::ModelFilename(modelFilePath), "model", "Model");
	archive(Serialization::GeneralFilename(actionControllerFilePath), "actionController", "Action Controller");
	archive(Serialization::GeneralFilename(animationDatabaseFilePath), "animationDatabase", "Animation Database (3P)");

	if (archive.isInput())
	{
		ownerBackReference.Reset();
	}
}

CMannequinObject::CMannequinObject()
	: m_properties(*this)
	, m_pAnimatedCharacter(nullptr)
{
}

void CMannequinObject::Initialize()
{
	const auto pGameObject = gEnv->pGameFramework->GetIGameObjectSystem()->CreateGameObjectForEntity(GetEntity()->GetId());
	assert(pGameObject);

	pGameObject->EnablePrePhysicsUpdate(ePPU_Always);
	pGameObject->EnablePhysicsEvent(true, eEPE_OnPostStepImmediate);
}

void CMannequinObject::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_START_LEVEL:
	case ENTITY_EVENT_RESET:
	case ENTITY_EVENT_EDITOR_PROPERTY_CHANGED:
	case ENTITY_EVENT_XFORM_FINISHED_EDITOR:
		Reset();
		break;
	default:
		break;
	}
}

uint64 CMannequinObject::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_START_LEVEL)
	       | ENTITY_EVENT_BIT(ENTITY_EVENT_EDITOR_PROPERTY_CHANGED)
	       | ENTITY_EVENT_BIT(ENTITY_EVENT_RESET)
	       | ENTITY_EVENT_BIT(ENTITY_EVENT_XFORM_FINISHED_EDITOR);
}

IEntityPropertyGroup* CMannequinObject::GetPropertyGroup()
{
	return &m_properties;
}

void CMannequinObject::GetMemoryUsage(ICrySizer* s) const
{
	s->Add(*this);
}

void CMannequinObject::Reset()
{
	IEntity& entity = *GetEntity();

	const auto pGameObject = gEnv->pGameFramework->GetIGameObjectSystem()->CreateGameObjectForEntity(entity.GetId());
	assert(pGameObject);

	if (!m_pAnimatedCharacter)
	{
		m_pAnimatedCharacter = static_cast<IAnimatedCharacter*>(pGameObject->AcquireExtension("AnimatedCharacter"));
		if (!m_pAnimatedCharacter)
		{
			return;
		}
	}

	m_pAnimatedCharacter->ResetState();
	m_pAnimatedCharacter->Init(pGameObject);
	m_pAnimatedCharacter->SetMovementControlMethods(eMCM_Animation, eMCM_Animation);
	if (IActionController* pActionController = m_pAnimatedCharacter->GetActionController())
	{
		pActionController->GetContext().state.Clear();
	}

	if (!m_properties.modelFilePath.empty())
	{
		entity.LoadCharacter(0, m_properties.modelFilePath.c_str());
	}

	{
		SEntityPhysicalizeParams physicsParams;
		physicsParams.type = PE_LIVING;
		physicsParams.nSlot = 0;
		physicsParams.mass = 80.0f;
		physicsParams.nFlagsOR = pef_monitor_poststep;

		pe_player_dynamics playerDyn;
		playerDyn.gravity = Vec3(0, 0, 9.81f);
		playerDyn.kAirControl = 0.9f;
		playerDyn.mass = 80;
		playerDyn.minSlideAngle = 45;
		playerDyn.maxClimbAngle = 50;
		playerDyn.minFallAngle = 50;
		playerDyn.maxVelGround = 16;
		physicsParams.pPlayerDynamics = &playerDyn;

		pe_player_dimensions playerDim;
		playerDim.heightCollider = 1.0f;
		playerDim.sizeCollider = Vec3(0.4f, 0.4f, 0.1f);
		playerDim.heightPivot = 0.0f;
		playerDim.maxUnproj = 0.0f;
		playerDim.bUseCapsule = true;
		physicsParams.pPlayerDimensions = &playerDim;

		entity.Physicalize(physicsParams);
	}

	m_pAnimatedCharacter->ResetInertiaCache();
}
