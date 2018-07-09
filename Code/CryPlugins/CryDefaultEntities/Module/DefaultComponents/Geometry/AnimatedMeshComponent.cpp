// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AnimatedMeshComponent.h"

#include <Cry3DEngine/IRenderNode.h>

namespace Cry
{
namespace DefaultComponents
{
void CAnimatedMeshComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	// Functions
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAnimatedMeshComponent::PlayAnimation, "{DCEBBA52-CECE-4823-AC35-1A590A944F99}"_cry_guid, "PlayAnimation");
		pFunction->SetDescription("Plays a low-level animation on the animated mesh");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::None);
		pFunction->BindInput(1, 'name', "Name");
		pFunction->BindInput(2, 'loop', "Looped");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAnimatedMeshComponent::SetPlaybackSpeed, "{DDF693D5-100C-43C2-8351-46FD6D382425}"_cry_guid, "SetPlaybackSpeed");
		pFunction->SetDescription("Sets the speed at which animations are played");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'sped', "Speed");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAnimatedMeshComponent::SetPlaybackWeight, "{79D18B21-F255-43C5-98F8-42E131FB2915}"_cry_guid, "SetPlaybackWeight");
		pFunction->SetDescription("Sets the weight at which animations are played");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'weig', "Weight");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAnimatedMeshComponent::SetLayer, "{3CF9CE51-5520-4D6D-8077-8112DA4A48B6}"_cry_guid, "SetLayer");
		pFunction->SetDescription("Sets the layer at which animations are played");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'layr', "Layer");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAnimatedMeshComponent::SetMeshType, "{2C8885E0-79F4-42A3-B67A-2F450DDBDFD0}"_cry_guid, "SetType");
		pFunction->BindInput(1, 'type', "Type");
		pFunction->SetDescription("Changes the type of the object");
		pFunction->SetFlags({ Schematyc::EEnvFunctionFlags::Member });
		componentScope.Register(pFunction);
	}
}

void CAnimatedMeshComponent::Initialize()
{
	LoadFromDisk();
	ResetObject();
}

void CAnimatedMeshComponent::LoadFromDisk()
{
	if (m_filePath.value.size() > 0)
	{
		m_pCachedCharacter = gEnv->pCharacterManager->CreateInstance(m_filePath.value);
	}
	else
	{
		m_pCachedCharacter = nullptr;
	}
}

void CAnimatedMeshComponent::ResetObject()
{
	if (m_pCachedCharacter == nullptr)
	{
		FreeEntitySlot();
		return;
	}

	m_pEntity->SetCharacter(m_pCachedCharacter, GetOrMakeEntitySlotId(), false);

	if (m_defaultAnimation.value.size() > 0)
	{
		m_animationParams.m_fPlaybackSpeed = m_defaultAnimationSpeed;
		PlayAnimation(m_defaultAnimation, m_bLoopDefaultAnimation);
	}
}

void CAnimatedMeshComponent::ProcessEvent(const SEntityEvent& event)
{
	if (event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
	{
		m_pEntity->UpdateComponentEventMask(this);

		LoadFromDisk();
		ResetObject();
	}

	CBaseMeshComponent::ProcessEvent(event);
}

void CAnimatedMeshComponent::SetCharacterFile(const char* szPath)
{
	m_filePath = szPath;
}

void CAnimatedMeshComponent::SetDefaultAnimationName(const char* szPath)
{
	m_defaultAnimation = szPath;
}
}
}
