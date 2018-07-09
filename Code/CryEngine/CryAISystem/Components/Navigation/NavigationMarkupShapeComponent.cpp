// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NavigationMarkupShapeComponent.h"

#include "CrySchematyc/Utils/SharedString.h"

//////////////////////////////////////////////////////////////////////////
void CAINavigationMarkupShapeComponent::SMarkupShapeProperties::ReflectType(Schematyc::CTypeDesc<CAINavigationMarkupShapeComponent::SMarkupShapeProperties>& desc)
{
	desc.SetGUID("7070E098-2FB0-4C7B-8DFF-B9ED76B93D6D"_cry_guid);
}
void CAINavigationMarkupShapeComponent::SMarkupShapeProperties::Serialize(Serialization::IArchive& archive)
{
	archive(name, "name", "^Name");
	archive.doc("Name of the shape. Needs to be unique among all defined shapes.");

	archive(position, "position", "Position");
	archive.doc("Position offset of the shape relative to the owning entity's pivot.");

	archive(Serialization::RadiansAsDeg(rotation), "rotation", "Rotation");
	archive.doc("Rotation angles around axes in local space.");

	archive(areaTypeId, "areaTypeId", "Area Type");
	archive.doc("Area type that should be applied to NavMesh.");

	archive(affectedAgentTypesMask, "affectedAgentTypes", "Affected Agent Types");
	archive.doc("NavMesh of whose navigation agent type should be affected.");

	archive(size, "size", "Size");
	archive.doc("Size of the markup shape box.");

	archive(bIsStatic, "static", "Static");
	archive.doc("True if the markup shape's triangles annotation doesn't need to be modified at run-time.");

	archive(bExpandByAgentRadius, "expand", "Expand By Agent Radius");
	archive.doc("True if the marked area on the NavMesh should be expanded by the agent radius.");
}

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

//////////////////////////////////////////////////////////////////////////

void CAINavigationMarkupShapeComponent::ReflectType(Schematyc::CTypeDesc<CAINavigationMarkupShapeComponent>& desc)
{
	desc.SetGUID(CAINavigationMarkupShapeComponent::IID());
	desc.SetLabel("AI Navigation Markup Shape");
	desc.SetDescription("Navigation Markup Shape Component");
	desc.SetEditorCategory("AI");
	desc.SetIcon("icons:Navigation/Move_Classic.ico");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Singleton });

	desc.AddMember(&CAINavigationMarkupShapeComponent::m_isGeometryIgnoredInNavMesh, 'gi', "ignoreGeometry", "Ignore Geometry in MNM", "Ignore entity's geometry in NavMesh Generation", false);
	desc.AddMember(&CAINavigationMarkupShapeComponent::m_shapesProperties, 'spa', "shapesArray", "Shapes", nullptr, SShapesArray());
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
				pFunction->SetDescription("Enable or disable annotation flag for a given shape");
				pFunction->BindInput(1, 'sn', "Shape Name", "Shape name");
				pFunction->BindInput(2, 'flag', "Flag", "Flag to be changed");
				pFunction->BindInput(3, 'en', "Enabled", "New flag state");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAINavigationMarkupShapeComponent::ToggleAnnotationFlags, "9E1F9DCE-58D4-4A11-B5B3-3D9D2578DDDE"_cry_guid, "ToggleAnnotationFlags");
				pFunction->SetDescription("Toggle selected annotation flags for a given shape.");
				pFunction->BindInput(1, 'sn', "Shape Name", "Shape name");
				pFunction->BindInput(2, 'fgmk', "Flags", "Flags to be switched", NavigationComponentHelpers::SAnnotationFlagsMask());
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAINavigationMarkupShapeComponent::ResetAnotations, "2BDD2A21-25EE-4C07-A851-7CCA758EAE0C"_cry_guid, "ResetAnotation");
				pFunction->SetDescription("Reset annotations of all shapes to default.");
				componentScope.Register(pFunction);
			}
		}
	}
}

void CAINavigationMarkupShapeComponent::Initialize()
{
	gEnv->pAISystem->GetNavigationSystem()->RegisterListener(this, GetEntity()->GetName());

	RegisterAndUpdateComponent();
}

void CAINavigationMarkupShapeComponent::OnShutDown()
{
	gEnv->pAISystem->GetNavigationSystem()->UnregisterEntityMarkups(*GetEntity());
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
		gEnv->pAISystem->GetNavigationSystem()->UnregisterEntityMarkups(*GetEntity());
		m_runtimeData.clear();

		RegisterAndUpdateComponent();
	}
}

void CAINavigationMarkupShapeComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED:
	{
		RegisterAndUpdateComponent();
		break;
	}
	case ENTITY_EVENT_XFORM:
		UpdateVolumes();
		break;
	case ENTITY_EVENT_RESET:
		ResetAnotations();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////

void CAINavigationMarkupShapeComponent::RegisterAndUpdateComponent()
{
	IEntity* pEntity = GetEntity();

	if (m_isGeometryIgnoredInNavMesh)
	{
		pEntity->SetFlagsExtended(pEntity->GetFlagsExtended() | ENTITY_FLAG_EXTENDED_IGNORED_IN_NAVMESH_GENERATION);
	}
	else
	{
		pEntity->SetFlagsExtended(pEntity->GetFlagsExtended() & ~ENTITY_FLAG_EXTENDED_IGNORED_IN_NAVMESH_GENERATION);
	}

	const size_t shapesCount = m_shapesProperties.shapes.size();

	std::vector<NavigationVolumeID> volumeIds;
	volumeIds.resize(shapesCount);

	std::vector<const char*> names;
	names.reserve(shapesCount);
	for (const SMarkupShapeProperties& properties : m_shapesProperties.shapes)
	{
		names.push_back(properties.name.c_str());
	}

	if (!gEnv->pAISystem->GetNavigationSystem()->RegisterEntityMarkups(*pEntity, names.data(), shapesCount, volumeIds.data()))
		return;

	m_runtimeData.resize(shapesCount);

	for (size_t i = 0; i < shapesCount; ++i)
	{
		m_runtimeData[i].m_volumeId = volumeIds[i];
	}

	UpdateAnnotations();
	UpdateVolumes();
}

void CAINavigationMarkupShapeComponent::ToggleAnnotationFlags(const Schematyc::CSharedString& shapeName, const NavigationComponentHelpers::SAnnotationFlagsMask& flags)
{
	size_t index = 0;
	for (size_t count = m_shapesProperties.shapes.size(); index < count; ++index)
	{
		if (m_shapesProperties.shapes[index].name == shapeName.c_str())
			break;
	}

	if (index >= m_runtimeData.size())
	{
		CryWarning(VALIDATOR_MODULE_AI, VALIDATOR_WARNING, "NavigationMarkupShapes::SwitchAnnotationFlag, invalid shape name '%s'!", shapeName.c_str());
		return;
	}
	
	SRuntimeData& runtimeData = m_runtimeData[index];

	runtimeData.m_currentAreaAnotation.SetFlags(runtimeData.m_currentAreaAnotation.GetFlags() ^ flags.mask);
	gEnv->pAISystem->GetNavigationSystem()->SetAnnotationForMarkupTriangles(runtimeData.m_volumeId, runtimeData.m_currentAreaAnotation);
}

void CAINavigationMarkupShapeComponent::SetAnnotationFlag(const Schematyc::CSharedString& shapeName, const NavigationAreaFlagID& flagId, bool bEnable)
{
	size_t index = 0;
	for (size_t count = m_shapesProperties.shapes.size(); index < count; ++index)
	{
		if (m_shapesProperties.shapes[index].name == shapeName.c_str())
			break;
	}

	if (index >= m_runtimeData.size())
	{
		CryWarning(VALIDATOR_MODULE_AI, VALIDATOR_WARNING, "NavigationMarkupShapes::SetAnnotationFlag, invalid shape name '%s'!", shapeName.c_str());
		return;
	}

	const MNM::SAreaFlag* pAreaFlag = gEnv->pAISystem->GetNavigationSystem()->GetAnnotationLibrary().GetAreaFlag(flagId);
	if (!pAreaFlag)
	{
		return;
	}

	SRuntimeData& runtimeData = m_runtimeData[index];
	MNM::AreaAnnotation::value_type currentFlagMask = runtimeData.m_currentAreaAnotation.GetFlags();
	if (bEnable)
	{
		currentFlagMask |= pAreaFlag->value;
	}
	else
	{
		currentFlagMask &= ~pAreaFlag->value;
	}
	runtimeData.m_currentAreaAnotation.SetFlags(currentFlagMask);

	gEnv->pAISystem->GetNavigationSystem()->SetAnnotationForMarkupTriangles(runtimeData.m_volumeId, runtimeData.m_currentAreaAnotation);
}

//////////////////////////////////////////////////////////////////////////

void CAINavigationMarkupShapeComponent::ResetAnotations()
{
	INavigationSystem* pNavigationSystem = gEnv->pAISystem->GetNavigationSystem();

	for (SRuntimeData& runtimeData : m_runtimeData)
	{
		if (runtimeData.m_currentAreaAnotation.GetFlags() != runtimeData.m_defaultAreaAnotation.GetFlags())
		{
			runtimeData.m_currentAreaAnotation = runtimeData.m_defaultAreaAnotation;
			pNavigationSystem->SetAnnotationForMarkupTriangles(runtimeData.m_volumeId, runtimeData.m_defaultAreaAnotation);
		}
	}
}

void CAINavigationMarkupShapeComponent::UpdateAnnotations()
{
	INavigationSystem* pNavigationSystem = gEnv->pAISystem->GetNavigationSystem();

	for (size_t i = 0, count = m_runtimeData.size(); i < count; ++i)
	{
		SRuntimeData& runtimeData = m_runtimeData[i];
		runtimeData.m_defaultAreaAnotation = pNavigationSystem->GetAreaTypeAnnotation(m_shapesProperties.shapes[i].areaTypeId);
		runtimeData.m_currentAreaAnotation = runtimeData.m_defaultAreaAnotation;
	}
}

void CAINavigationMarkupShapeComponent::UpdateVolumes()
{
	INavigationSystem* pNavigationSystem = gEnv->pAISystem->GetNavigationSystem();

	for (size_t i = 0, count = m_runtimeData.size(); i < count; ++i)
	{
		const SMarkupShapeProperties& properties = m_shapesProperties.shapes[i];
		SRuntimeData& runtimeData = m_runtimeData[i];

		const Quat rotation = Quat::CreateRotationXYZ(properties.rotation);
		const Matrix34 localTM = Matrix34::Create(Vec3(1.0f), rotation, properties.position);
		const Matrix34 worldTM = GetEntity()->GetWorldTM();
		const Matrix34 tranform = worldTM * localTM;
		const Vec3 sizeHalf = properties.size * 0.5f;

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
		params.height = properties.size.z;
		params.bStoreTriangles = !properties.bIsStatic;
		params.bExpandByAgentRadius = properties.bExpandByAgentRadius;
		params.areaAnnotation = runtimeData.m_currentAreaAnotation;

		pNavigationSystem->SetMarkupVolume(properties.affectedAgentTypesMask.mask, vertices, 4, runtimeData.m_volumeId, params);
	}
}

void CAINavigationMarkupShapeComponent::RenderVolume() const
{
	const float thickness = 3.0f;

	for (const SMarkupShapeProperties& properties : m_shapesProperties.shapes)
	{
		//TODO: Expose area color in interface and use it here
		//const MNM::SAreaType* pAreaType = gEnv->pAISystem->GetNavigationSystem()->GetAnnotationLibrary().GetAreaType(properties.areaTypeId);
		const ColorB color(0, 255, 80, 255);

		const Quat rotation = Quat::CreateRotationXYZ(properties.rotation);
		const Matrix34 localTM = Matrix34::Create(Vec3(1.0f), rotation, properties.position);
		const Matrix34 worldTM = GetEntity()->GetWorldTM();
		const Matrix34 tranform = worldTM * localTM;
		const Vec3 sizeHalf = properties.size * 0.5f;

		Vec3 verticesDown[4], verticesUp[4];
		verticesDown[0] = Vec3(-sizeHalf.x, -sizeHalf.y, 0.0f);
		verticesDown[1] = Vec3(sizeHalf.x, -sizeHalf.y, 0.0f);
		verticesDown[2] = Vec3(sizeHalf.x, sizeHalf.y, 0.0f);
		verticesDown[3] = Vec3(-sizeHalf.x, sizeHalf.y, 0.0f);

		for (size_t i = 0; i < 4; ++i)
		{
			verticesUp[i] = verticesDown[i];
			verticesUp[i].z = properties.size.z;

			verticesDown[i] = tranform.TransformPoint(verticesDown[i]);
			verticesUp[i] = tranform.TransformPoint(verticesUp[i]);
		}

		IRenderAuxGeom& renderAuxGeom = *gEnv->pRenderer->GetIRenderAuxGeom();
		renderAuxGeom.DrawPolyline(verticesDown, 4, true, color, thickness);
		renderAuxGeom.DrawPolyline(verticesUp, 4, true, color, thickness);

		for (size_t i = 0; i < 4; ++i)
		{
			renderAuxGeom.DrawLine(verticesDown[i], color, verticesUp[i], color, thickness);
		}
	}
}

IEntityComponentPreviewer* CAINavigationMarkupShapeComponent::GetPreviewer()
{
	static CComponentPreviewer previewer;
	return &previewer;
}

