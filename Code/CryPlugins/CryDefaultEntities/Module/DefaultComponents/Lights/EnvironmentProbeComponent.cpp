#include "StdAfx.h"
#include "EnvironmentProbeComponent.h"

#include <Cry3DEngine/IRenderNode.h>
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.inl>
#include <CryCore/ToolsHelpers/EngineSettingsManager.inl>
#include <CryCore/ToolsHelpers/SettingsManagerHelpers.inl>

namespace Cry
{
namespace DefaultComponents
{
void CEnvironmentProbeComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	// Functions
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEnvironmentProbeComponent::LoadFromDisk, "122049AA-015F-4F30-933D-BF2E7C6BA0BC"_cry_guid, "LoadFromDisk");
		pFunction->SetDescription("Loads a cube map texture from disk and applies it");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'path', "Cube map Texture Path");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEnvironmentProbeComponent::SetCubemap, "F305D0C4-6AD2-4FD8-93A9-330A91206360"_cry_guid, "SetCubemap");
		pFunction->SetDescription("Sets the cube map from already loaded textures");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'spec', "Specular cube map texture id");
		pFunction->BindInput(2, 'diff', "Diffuse cube map texture id");
		componentScope.Register(pFunction);
	}
#ifdef SUPPORT_ENVIRONMENT_PROBE_GENERATION
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEnvironmentProbeComponent::Generate, "024A11F7-8C74-493A-A0A7-3D613281BEDE"_cry_guid, "Generate");
		pFunction->SetDescription("Generates the cubemap to the specified path");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'path', "Output Texture Path");
		componentScope.Register(pFunction);
	}
#endif
}

void CEnvironmentProbeComponent::Initialize()
{
#ifndef RELEASE
#ifdef SUPPORT_ENVIRONMENT_PROBE_GENERATION
	m_generation.m_generateButton = Serialization::ActionButton(std::function<void()>([this]() { GenerateToDefaultPath(); }));
#endif

	if (gEnv->IsEditor())
	{
		m_generation.pSelectionObject = gEnv->p3DEngine->LoadStatObj("%EDITOR%/Objects/envcube.cgf", nullptr, nullptr, false);
	}
#endif

	if (m_generation.m_bAutoLoad && m_generation.m_generatedCubemapPath.value.size() > 0 && gEnv->pRenderer != nullptr)
	{
		LoadFromDisk(m_generation.m_generatedCubemapPath);
	}
	else
	{
		FreeEntitySlot();
	}
}

void CEnvironmentProbeComponent::ProcessEvent(const SEntityEvent& event)
{
	if (event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
	{
		Initialize();
	}
}

uint64 CEnvironmentProbeComponent::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);
}

#ifndef RELEASE
void CEnvironmentProbeComponent::Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const
{
	if (context.bSelected)
	{
		Matrix34 slotTransform = GetWorldTransformMatrix();

		OBB obb = OBB::CreateOBBfromAABB(Matrix33(IDENTITY), AABB(-m_extents * 0.5f, m_extents * 0.5f));
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawOBB(obb, slotTransform, false, context.debugDrawInfo.color, eBBD_Faceted);

		if (m_generation.pSelectionObject != nullptr)
		{
			SRenderingPassInfo passInfo = SRenderingPassInfo::CreateGeneralPassRenderingInfo(gEnv->p3DEngine->GetRenderingCamera());

			SRendParams rp;
			rp.AmbientColor = ColorF(0.0f, 0.0f, 0.0f, 1);
			rp.dwFObjFlags |= FOB_TRANS_MASK;
			rp.fAlpha = 1;
			Matrix34 slotTransform = GetWorldTransformMatrix();
			rp.pMatrix = &slotTransform;
			rp.pMaterial = m_generation.pSelectionObject->GetMaterial();
			m_generation.pSelectionObject->Render(rp, passInfo);
		}
	}
}
#endif

void CEnvironmentProbeComponent::SetOutputCubemapPath(const char* szPath)
{
	m_generation.m_generatedCubemapPath = szPath;
}
}
}
