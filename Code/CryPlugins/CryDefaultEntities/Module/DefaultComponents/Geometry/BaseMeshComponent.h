// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>

#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

#include "DefaultComponents/ComponentHelpers/PhysicsParameters.h"

namespace Cry
{
namespace DefaultComponents
{
enum class EMeshType : uint32
{
	None              = 0,
	Render            = 1 << 0,
	Collider          = 1 << 1,
	RenderAndCollider = Render | Collider,
};

static void ReflectType(Schematyc::CTypeDesc<EMeshType>& desc)
{
	desc.SetGUID("{A786ABAF-3B6F-4EB4-A073-502A87EC5F64}"_cry_guid);
	desc.SetLabel("Mesh Type");
	desc.SetDescription("Determines what to use a mesh for");
	desc.SetDefaultValue(EMeshType::RenderAndCollider);
	desc.AddConstant(EMeshType::None, "None", "Disabled");
	desc.AddConstant(EMeshType::Render, "Render", "Render");
	desc.AddConstant(EMeshType::Collider, "Collider", "Collider");
	desc.AddConstant(EMeshType::RenderAndCollider, "RenderAndCollider", "Render and Collider");
}

// Used to indicate the minimum graphical setting for an effect
enum class EMiniumSystemSpec
{
	Disabled = 0,
	Always,
	Medium,
	High,
	VeryHigh
};

static void ReflectType(Schematyc::CTypeDesc<EMiniumSystemSpec>& desc)
{
	desc.SetGUID("{9DDF8F33-CB8C-4BEE-A539-01BC8DAFED2E}"_cry_guid);
	desc.SetLabel("Minimum Graphical Setting");
	desc.SetDescription("Minimum graphical setting to enable an effect");
	desc.SetDefaultValue(EMiniumSystemSpec::Disabled);
	desc.AddConstant(EMiniumSystemSpec::Disabled, "None", "None");
	desc.AddConstant(EMiniumSystemSpec::Always, "Low", "Low");
	desc.AddConstant(EMiniumSystemSpec::Medium, "Medium", "Medium");
	desc.AddConstant(EMiniumSystemSpec::High, "High", "High");
	desc.AddConstant(EMiniumSystemSpec::VeryHigh, "VeryHigh", "Very High");
}

// Used to indicate the minimum graphical setting for an effect
enum class EMeshGIMode
{
	Disabled             = IRenderNode::EGIMode::eGM_None,
	StaticVoxelization   = IRenderNode::EGIMode::eGM_StaticVoxelization,
	AnalyticalProxySoft  = IRenderNode::EGIMode::eGM_AnalyticalProxy_Soft,
	AnalyticalProxyHard  = IRenderNode::EGIMode::eGM_AnalyticalProxy_Hard,
	AnalyticalOccluder   = IRenderNode::EGIMode::eGM_AnalytPostOccluder,
	IntegrateIntoTerrain = IRenderNode::EGIMode::eGM_IntegrateIntoTerrain
};

static void ReflectType(Schematyc::CTypeDesc<EMeshGIMode>& desc)
{
	desc.SetGUID("{9C646BE3-861D-44E7-88EA-BD2252557D7C}"_cry_guid);
	desc.SetLabel("Global Illumination Mode");
	desc.SetDefaultValue(EMeshGIMode::Disabled);
	desc.AddConstant(EMeshGIMode::Disabled, "None", "Disabled");
	desc.AddConstant(EMeshGIMode::StaticVoxelization, "StaticVoxelization", "Static Voxelization");
	desc.AddConstant(EMeshGIMode::AnalyticalProxySoft, "AnalyticalProxySoft", "Analytical Proxy Soft");
	desc.AddConstant(EMeshGIMode::AnalyticalProxyHard, "AnalyticalProxyHard", "Analytical Proxy Hard");
	desc.AddConstant(EMeshGIMode::AnalyticalOccluder, "AnalyticalOccluder", "Analytical Occluder");
	desc.AddConstant(EMeshGIMode::IntegrateIntoTerrain, "IntegrateIntoTerrain", "Integrate Into Terrain");
}

static void UpdateGIModeEntitySlotFlags(uint8 giModeFlags, uint32& slotFlags)
{
	if (((uint8)giModeFlags & 1) != 0)
	{
		slotFlags |= ENTITY_SLOT_GI_MODE_BIT0;
	}
	else
	{
		slotFlags &= ~ENTITY_SLOT_GI_MODE_BIT0;
	}

	if (((uint8)giModeFlags & 2) != 0)
	{
		slotFlags |= ENTITY_SLOT_GI_MODE_BIT1;
	}
	else
	{
		slotFlags &= ~ENTITY_SLOT_GI_MODE_BIT1;
	}

	if (((uint8)giModeFlags & 4) != 0)
	{
		slotFlags |= ENTITY_SLOT_GI_MODE_BIT2;
	}
	else
	{
		slotFlags &= ~ENTITY_SLOT_GI_MODE_BIT2;
	}
}

struct SRenderParameters
{
	inline bool operator==(const SRenderParameters& rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

	EMiniumSystemSpec                     m_castShadowSpec = EMiniumSystemSpec::Always;
	bool                                  m_bIgnoreVisAreas = false;
	Schematyc::Range<0, 100, 0, 100, int> m_viewDistanceRatio = 100;
	Schematyc::Range<0, 100, 0, 100, int> m_lodDistance = 100;
	EMeshGIMode                           m_giMode = EMeshGIMode::Disabled;
};

static void ReflectType(Schematyc::CTypeDesc<SRenderParameters>& desc)
{
	desc.SetGUID("{291C60F6-A493-41B3-AC63-B9E0187DC074}"_cry_guid);
	desc.AddMember(&SRenderParameters::m_castShadowSpec, 'shad', "ShadowSpec", "Minimum Shadow Graphics", "Minimum graphical setting to cast shadows from this light", EMiniumSystemSpec::Always);
	desc.AddMember(&SRenderParameters::m_bIgnoreVisAreas, 'visa', "IgnoreVisArea", "Ignore Visareas", "Whether this component will ignore vis areas", false);
	desc.AddMember(&SRenderParameters::m_viewDistanceRatio, 'view', "ViewDistRatio", "View Distance", "View distance from 0 to 100, 100 being always visible", 100);
	desc.AddMember(&SRenderParameters::m_lodDistance, 'lodd', "LODDistance", "LOD Distance", "Level of Detail distance from 0 to 100, 100 being always best LOD", 100);
	desc.AddMember(&SRenderParameters::m_giMode, 'gimo', "GIMode", "GI and Usage Mode", "The way object is used by GI and by some other systems", EMeshGIMode::Disabled);
}

// Base implementation for our physics mesh components
class CBaseMeshComponent
	: public IEntityComponent
{
protected:
	// IEntityComponent
	virtual void ProcessEvent(const SEntityEvent& event) override
	{
		if (event.event == ENTITY_EVENT_PHYSICAL_TYPE_CHANGED || event.event == ENTITY_EVENT_SLOT_CHANGED)
		{
			ApplyBaseMeshProperties();
		}
		else
		{
			if (event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
			{
				switch (m_physics.m_weightType)
				{
				case SPhysicsParameters::EWeightType::Mass:
					{
						m_physics.m_mass = max((float)m_physics.m_mass, SPhysicsParameters::s_weightTypeEpsilon);
					}
					break;
				case SPhysicsParameters::EWeightType::Density:
					{
						m_physics.m_density = max((float)m_physics.m_density, SPhysicsParameters::s_weightTypeEpsilon);
					}
					break;
				}

				m_pEntity->UpdateComponentEventMask(this);

				ApplyBaseMeshProperties();
			}
		}
	}

	virtual uint64 GetEventMask() const override
	{
		uint64 bitFlags = ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED) | ENTITY_EVENT_BIT(ENTITY_EVENT_SLOT_CHANGED);

		if (((uint32)m_type & (uint32)EMeshType::Collider) != 0)
		{
			bitFlags |= ENTITY_EVENT_BIT(ENTITY_EVENT_PHYSICAL_TYPE_CHANGED);
		}

		return bitFlags;
	}
	// ~IEntityComponent

	void ApplyBaseMeshProperties()
	{
		if (GetEntitySlotId() != EmptySlotId)
		{
			if (((uint32)m_type & (uint32)EMeshType::Render) != 0)
			{
				m_pEntity->SetSlotFlags(GetEntitySlotId(), ENTITY_SLOT_RENDER);
			}
			else
			{
				m_pEntity->SetSlotFlags(GetEntitySlotId(), 0);
			}

			if (IRenderNode* pRenderNode = m_pEntity->GetSlotRenderNode(GetEntitySlotId()))
			{
				uint32 slotFlags = m_pEntity->GetSlotFlags(GetEntitySlotId());
				if (m_renderParameters.m_castShadowSpec != EMiniumSystemSpec::Disabled && (int)gEnv->pSystem->GetConfigSpec() >= (int)m_renderParameters.m_castShadowSpec)
				{
					slotFlags |= ENTITY_SLOT_CAST_SHADOW;
				}
				if (m_renderParameters.m_bIgnoreVisAreas)
				{
					slotFlags |= ENTITY_SLOT_IGNORE_VISAREAS;
				}

				UpdateGIModeEntitySlotFlags((uint8)m_renderParameters.m_giMode, slotFlags);

				m_pEntity->SetSlotFlags(GetEntitySlotId(), slotFlags);

				// We want to manage our own view distance ratio
				m_pEntity->AddFlags(ENTITY_FLAG_CUSTOM_VIEWDIST_RATIO);
				pRenderNode->SetViewDistRatio((int)floor(((float)m_renderParameters.m_viewDistanceRatio / 100.f) * 255));
				pRenderNode->SetLodRatio((int)floor(((float)m_renderParameters.m_lodDistance / 100.f) * 255));
			}

			m_pEntity->UnphysicalizeSlot(GetEntitySlotId());

			if (((uint32)m_type & (uint32)EMeshType::Collider) != 0)
			{
				if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysicalEntity())
				{
					SEntityPhysicalizeParams physParams;
					physParams.nSlot = GetEntitySlotId();
					physParams.type = pPhysicalEntity->GetType();

					switch (m_physics.m_weightType)
					{
					case SPhysicsParameters::EWeightType::Mass:
						{
							physParams.mass = m_physics.m_mass;
							physParams.density = -1.0f;
						}
						break;
					case SPhysicsParameters::EWeightType::Density:
						{
							physParams.density = m_physics.m_density;
							physParams.mass = -1.0f;
						}
						break;
					case SPhysicsParameters::EWeightType::Immovable:
						{
							physParams.density = -1.0f;
							physParams.mass = 0.0f;
						}
						break;
					}

					m_pEntity->PhysicalizeSlot(GetEntitySlotId(), physParams);
				}
			}
		}
	}

public:
	virtual void SetType(EMeshType type)
	{
		m_type = type;

		ApplyBaseMeshProperties();
	}
	EMeshType                   GetType() const              { return m_type; }

	virtual SPhysicsParameters& GetPhysicsParameters()       { return m_physics; }
	const SPhysicsParameters&   GetPhysicsParameters() const { return m_physics; }

protected:
	SPhysicsParameters m_physics;
	EMeshType          m_type = EMeshType::RenderAndCollider;
	SRenderParameters  m_renderParameters;
};
}
}
