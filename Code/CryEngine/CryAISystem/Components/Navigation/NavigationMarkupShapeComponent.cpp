// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NavigationMarkupShapeComponent.h"

//////////////////////////////////////////////////////////////////////////
class CComponentPreviewer : public IEntityComponentPreviewer
{
public:
	virtual void SerializeProperties(Serialization::IArchive& archive) override {}
	virtual void Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext& context) const override
	{
		const CAINavigationMarkupShapeComponent& markupComponent = static_cast<const CAINavigationMarkupShapeComponent&>(component);
		markupComponent.RenderVolume();
	}
};

CAINavigationMarkupShapeComponent::CAINavigationMarkupShapeComponent()
	: m_size(2.0f, 2.0f, 2.0f)
{
	// nothing
}

void CAINavigationMarkupShapeComponent::ReflectType(Schematyc::CTypeDesc<CAINavigationMarkupShapeComponent>& desc)
{
	desc.SetGUID(CAINavigationMarkupShapeComponent::IID());
	desc.SetLabel("AI Navigation Markup Shape");
	desc.SetDescription("Navigation Markup Shape Component");
	desc.SetEditorCategory("AI");
	desc.SetIcon("icons:Navigation/Move_Classic.ico");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Transform });
	desc.AddMember(&CAINavigationMarkupShapeComponent::m_affectedAgentTypesMask, 'aat', "affectedAgentTypes", "Affected Agent Types", nullptr, NavigationComponentHelpers::SAgentTypesMask());
	desc.AddMember(&CAINavigationMarkupShapeComponent::m_areaTypeId, 'atid', "areaType", "Area Type", nullptr, NavigationAreaTypeID());
	desc.AddMember(&CAINavigationMarkupShapeComponent::m_bStoreTriangles, 'stri', "storeTriangles", "Store Triangles", nullptr, false);
	desc.AddMember(&CAINavigationMarkupShapeComponent::m_bExpandByAgentRadius, 'exp', "expand", "Expand by Agent Radius", nullptr, false);
	desc.AddMember(&CAINavigationMarkupShapeComponent::m_size, 'sz', "size", "Size", nullptr, Vec3(2.0f, 2.0f, 2.0f));

	desc.AddMember(&CAINavigationMarkupShapeComponent::m_volumeId, 'nvid', "volumeId", "", nullptr, NavigationVolumeID());
}

void CAINavigationMarkupShapeComponent::Register(Schematyc::IEnvRegistrar& registrar)
{
	Schematyc::CEnvRegistrationScope entityScope = registrar.Scope(IEntity::GetEntityScopeGUID());
	{
		Schematyc::CEnvRegistrationScope componentScope = entityScope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CAINavigationMarkupShapeComponent));
		{
			// Functions
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAINavigationMarkupShapeComponent::SetAnnotationFlag, "299851E7-89E1-431B-A9E0-E085361F674D"_cry_guid, "SetAnnotationFlag");
				pFunction->SetDescription("Sends an arbitrary event (in the form of a string) to the Behavior Tree.");
				pFunction->BindInput(1, 'flag', "Flag", "Flag to be changed");
				pFunction->BindInput(2, 'en', "Enabled", "New flag state");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAINavigationMarkupShapeComponent::ChangeAreaAnotation, "9E1F9DCE-58D4-4A11-B5B3-3D9D2578DDDE"_cry_guid, "ChangeAreaAnotation");
				pFunction->SetDescription("Sends an arbitrary event (in the form of a string) to the Behavior Tree.");
				pFunction->BindInput(1, 'fgmk', "Flags", "Flags to be set", NavigationComponentHelpers::SAnnotationFlagsMask());
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAINavigationMarkupShapeComponent::ResetAnotation, "2BDD2A21-25EE-4C07-A851-7CCA758EAE0C"_cry_guid, "ResetAnotation");
				pFunction->SetDescription("Sends an arbitrary event (in the form of a string) to the Behavior Tree.");
				//pFunction->BindInput(1, 'evnt', "Event", "Name of the event to send to the Behavior Tree", Schematyc::CSharedString(""));
				componentScope.Register(pFunction);
			}
		}
	}
}

void CAINavigationMarkupShapeComponent::Initialize()
{
	gEnv->pAISystem->GetNavigationSystem()->RegisterListener(this, GetEntity()->GetName());
	m_volumeId = gEnv->pAISystem->GetNavigationSystem()->CreateMarkupVolume(m_volumeId);
	if (m_volumeId)
	{
		UpdateAnnotation();
		UpdateVolume();
	}
}

void CAINavigationMarkupShapeComponent::OnShutDown()
{
	gEnv->pAISystem->GetNavigationSystem()->DestroyMarkupVolume(m_volumeId);
	gEnv->pAISystem->GetNavigationSystem()->UnRegisterListener(this);
}

uint64 CAINavigationMarkupShapeComponent::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED) | ENTITY_EVENT_BIT(ENTITY_EVENT_XFORM) | ENTITY_EVENT_BIT(ENTITY_EVENT_RESET);
}

void CAINavigationMarkupShapeComponent::OnNavigationEvent(const INavigationSystem::ENavigationEvent event)
{
	if (event == INavigationSystem::NavigationCleared)
	{
		if (m_volumeId)
		{
			m_volumeId = gEnv->pAISystem->GetNavigationSystem()->CreateMarkupVolume(m_volumeId);
			UpdateVolume();
		}
	}
}

void CAINavigationMarkupShapeComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED:
		UpdateAnnotation();
	case ENTITY_EVENT_XFORM:
		UpdateVolume();
		break;
	case ENTITY_EVENT_RESET:
		ResetAnotation();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////

void CAINavigationMarkupShapeComponent::ChangeAreaAnotation(const NavigationComponentHelpers::SAnnotationFlagsMask& flags)
{
	m_currentAreaAnotation.SetFlags(flags.mask);
	
	gEnv->pAISystem->GetNavigationSystem()->SetAnnotationForMarkupTriangles(m_volumeId, m_currentAreaAnotation);
}

void CAINavigationMarkupShapeComponent::SetAnnotationFlag(const NavigationAreaFlagID& flagId, bool bEnable)
{
	const MNM::SAreaFlag* pAreaFlag = gEnv->pAISystem->GetNavigationSystem()->GetAnnotationLibrary().GetAreaFlag(flagId);
	if (!pAreaFlag)
	{
		return;
	}
	
	auto currentFlagMask = m_currentAreaAnotation.GetFlags();
	if (bEnable)
	{
		currentFlagMask |= pAreaFlag->value;
	}
	else
	{
		currentFlagMask &= ~pAreaFlag->value;
	}
	m_currentAreaAnotation.SetFlags(currentFlagMask);

	gEnv->pAISystem->GetNavigationSystem()->SetAnnotationForMarkupTriangles(m_volumeId, m_currentAreaAnotation);
}

//////////////////////////////////////////////////////////////////////////

void CAINavigationMarkupShapeComponent::ResetAnotation()
{
	if (m_currentAreaAnotation.GetFlags() != m_defaultAreaAnotation.GetFlags())
	{
		m_currentAreaAnotation = m_defaultAreaAnotation;
		gEnv->pAISystem->GetNavigationSystem()->SetAnnotationForMarkupTriangles(m_volumeId, m_defaultAreaAnotation);
	}
}

void CAINavigationMarkupShapeComponent::UpdateAnnotation()
{
	m_defaultAreaAnotation = gEnv->pAISystem->GetNavigationSystem()->GetAreaTypeAnnotation(m_areaTypeId);
	m_currentAreaAnotation = m_defaultAreaAnotation;
}

void CAINavigationMarkupShapeComponent::UpdateVolume()
{
	const IEntity* pEntity = GetEntity();
	const Matrix34 tranform = GetWorldTransformMatrix();
	const Vec3 sizeHalf = m_size * 0.5f;

	Vec3 vertices[4];
	vertices[0] = Vec3(-sizeHalf.x, -sizeHalf.y, 0.0f);
	vertices[1] = Vec3(sizeHalf.x, -sizeHalf.y, 0.0f);
	vertices[2] = Vec3(sizeHalf.x, sizeHalf.y, 0.0f);
	vertices[3] = Vec3(-sizeHalf.x, sizeHalf.y, 0.0f);

	for (size_t i = 0; i < 4; ++i)
	{
		vertices[i] = tranform.TransformPoint(vertices[i]);
	}

	MNM::SMarkupVolumeParams params;
	params.height = m_size.z;
	params.bStoreTriangles = m_bStoreTriangles;
	params.bExpandByAgentRadius = m_bExpandByAgentRadius;
	params.areaAnnotation = m_currentAreaAnotation;

	gEnv->pAISystem->GetNavigationSystem()->SetMarkupVolume(m_affectedAgentTypesMask.mask, vertices, 4, m_volumeId, params);
}

void CAINavigationMarkupShapeComponent::RenderVolume() const
{
	const IEntity* pEntity = GetEntity();
	const Vec3 sizeHalf = m_size * 0.5f;

	const Matrix34 tranform = GetWorldTransformMatrix();

	Vec3 verticesDown[4], verticesUp[4];
	verticesDown[0] = Vec3(-sizeHalf.x, -sizeHalf.y, 0.0f);
	verticesDown[1] = Vec3(sizeHalf.x, -sizeHalf.y, 0.0f);
	verticesDown[2] = Vec3(sizeHalf.x, sizeHalf.y, 0.0f);
	verticesDown[3] = Vec3(-sizeHalf.x, sizeHalf.y, 0.0f);

	for (size_t i = 0; i < 4; ++i)
	{
		verticesUp[i] = verticesDown[i];
		verticesUp[i].z = m_size.z;

		verticesDown[i] = tranform.TransformPoint(verticesDown[i]);
		verticesUp[i] = tranform.TransformPoint(verticesUp[i]);
	}

	const ColorB color(0, 255, 80, 255);
	const float thickness = 3.0f;

	IRenderAuxGeom& renderAuxGeom = *gEnv->pRenderer->GetIRenderAuxGeom();
	renderAuxGeom.DrawPolyline(verticesDown, 4, true, color, thickness);
	renderAuxGeom.DrawPolyline(verticesUp, 4, true, color, thickness);

	for (size_t i = 0; i < 4; ++i)
	{
		renderAuxGeom.DrawLine(verticesDown[i], color, verticesUp[i], color, thickness);
	}
}

IEntityComponentPreviewer* CAINavigationMarkupShapeComponent::GetPreviewer()
{
	static CComponentPreviewer previewer;
	return &previewer;
}

