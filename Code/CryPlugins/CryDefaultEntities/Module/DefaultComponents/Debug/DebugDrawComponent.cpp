#include "StdAfx.h"
#include "DebugDrawComponent.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include <CryGame/IGameFramework.h>

namespace Cry
{
namespace DefaultComponents
{
void CDebugDrawComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CDebugDrawComponent::DrawSphere, "{96281FF5-B032-409C-8128-D02800D6876E}"_cry_guid, "DrawSphere");
		pFunction->SetDescription("Draws a sphere in the world");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'pos', "Position");
		pFunction->BindInput(2, 'rad', "Radius");
		pFunction->BindInput(3, 'col', "Color");
		pFunction->BindInput(4, 'dur', "Duration");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CDebugDrawComponent::DrawDirection, "{8ED2D2BD-BD20-4A28-AE8B-9948EFE45E12}"_cry_guid, "DrawDirection");
		pFunction->SetDescription("Draws a direction in the world");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'pos', "Position");
		pFunction->BindInput(2, 'rad', "Radius");
		pFunction->BindInput(3, 'dir', "Direction");
		pFunction->BindInput(4, 'col', "Color");
		pFunction->BindInput(5, 'dur', "Duration");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CDebugDrawComponent::DrawLine, "{ECBDFE43-7518-43CA-9C81-227BC3BB849D}"_cry_guid, "DrawLine");
		pFunction->SetDescription("Draws a line in the world");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'pos1', "Start Position");
		pFunction->BindInput(2, 'pos2', "End Position");
		pFunction->BindInput(3, 'col', "Color");
		pFunction->BindInput(4, 'dur', "Duration");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CDebugDrawComponent::DrawPlanarDisc, "{34F062B4-018B-4CE1-84ED-92005C042C3D}"_cry_guid, "DrawPlanarDisc");
		pFunction->SetDescription("Draws a planar disc in the world");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'pos', "Position");
		pFunction->BindInput(2, 'inra', "Inner Radius");
		pFunction->BindInput(3, 'oura', "Outer Radius");
		pFunction->BindInput(4, 'col', "Color");
		pFunction->BindInput(5, 'dur', "Duration");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CDebugDrawComponent::DrawCone, "{7E4FAF6E-A664-49B9-909F-90B91AEEEBE9}"_cry_guid, "DrawCone");
		pFunction->SetDescription("Draws a cone in the world");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'pos', "Position");
		pFunction->BindInput(2, 'dir', "Direction");
		pFunction->BindInput(3, 'bara', "Base Radius");
		pFunction->BindInput(4, 'hei', "Height");
		pFunction->BindInput(5, 'col', "Color");
		pFunction->BindInput(6, 'dur', "Duration");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CDebugDrawComponent::DrawCylinder, "{0C851E60-9798-4DF5-BE36-7F671D685D83}"_cry_guid, "DrawCylinder");
		pFunction->SetDescription("Draws a cylinder in the world");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'pos', "Position");
		pFunction->BindInput(2, 'dir', "Direction");
		pFunction->BindInput(3, 'rad', "Radius");
		pFunction->BindInput(4, 'hei', "Height");
		pFunction->BindInput(5, 'col', "Color");
		pFunction->BindInput(6, 'dur', "Duration");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CDebugDrawComponent::Draw2DText, "{6E62F8B7-E52A-47F5-BD08-3B78E9EEE634}"_cry_guid, "Draw2DText");
		pFunction->SetDescription("Draws 2D text onto the screen");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'text', "Text");
		pFunction->BindInput(2, 'size', "Size");
		pFunction->BindInput(3, 'col', "Color");
		pFunction->BindInput(4, 'dur', "Duration");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CDebugDrawComponent::DrawText, "{DCC8C877-5970-4D29-B8D7-3C01A2DA8ECA}"_cry_guid, "DrawText");
		pFunction->SetDescription("Draws 2D text onto a specific part of the screen");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'text', "Text");
		pFunction->BindInput(2, 'x', "X");
		pFunction->BindInput(3, 'y', "Y");
		pFunction->BindInput(4, 'size', "Size");
		pFunction->BindInput(5, 'col', "Color");
		pFunction->BindInput(6, 'dur', "Duration");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CDebugDrawComponent::DrawText3D, "{A462ACC5-0BB0-45A4-8FC0-3EC14778934B}"_cry_guid, "DrawText3D");
		pFunction->SetDescription("Draws text in the world");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'text', "Text");
		pFunction->BindInput(2, 'pos', "Position");
		pFunction->BindInput(3, 'size', "Size");
		pFunction->BindInput(4, 'col', "Color");
		pFunction->BindInput(5, 'dur', "Duration");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CDebugDrawComponent::Draw2DLine, "{1DB96257-4C86-4A52-84A1-81F5FF9505B5}"_cry_guid, "Draw2DLine");
		pFunction->SetDescription("Draws a 2D line on the screen");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'x1', "Start X");
		pFunction->BindInput(2, 'y1', "Start Y");
		pFunction->BindInput(3, 'x2', "End X");
		pFunction->BindInput(4, 'y2', "End Y");
		pFunction->BindInput(5, 'col', "Color");
		pFunction->BindInput(6, 'dur', "Duration");
		componentScope.Register(pFunction);
	}
}

void CDebugDrawComponent::DrawSphere(const Vec3& pos, float radius, const ColorF& color, float duration)
{
	gEnv->pGameFramework->GetIPersistantDebug()->Begin(__FUNCTION__, false);
	gEnv->pGameFramework->GetIPersistantDebug()->AddSphere(pos, radius, color, duration);
}

void CDebugDrawComponent::DrawDirection(const Vec3& pos, float radius, const Vec3& dir, const ColorF& color, float duration)
{
	gEnv->pGameFramework->GetIPersistantDebug()->Begin(__FUNCTION__, false);
	gEnv->pGameFramework->GetIPersistantDebug()->AddDirection(pos, radius, dir, color, duration);
}

void CDebugDrawComponent::DrawLine(const Vec3& posStart, const Vec3& posEnd, const ColorF& color, float duration)
{
	gEnv->pGameFramework->GetIPersistantDebug()->Begin(__FUNCTION__, false);
	gEnv->pGameFramework->GetIPersistantDebug()->AddLine(posStart, posEnd, color, duration);
}

void CDebugDrawComponent::DrawPlanarDisc(const Vec3& pos, float innerRadius, float outerRadius, const ColorF& color, float duration)
{
	gEnv->pGameFramework->GetIPersistantDebug()->Begin(__FUNCTION__, false);
	gEnv->pGameFramework->GetIPersistantDebug()->AddPlanarDisc(pos, innerRadius, outerRadius, color, duration);
}

void CDebugDrawComponent::DrawCone(const Vec3& pos, const Vec3& dir, float baseRadius, float height, const ColorF& color, float duration)
{
	gEnv->pGameFramework->GetIPersistantDebug()->Begin(__FUNCTION__, false);
	gEnv->pGameFramework->GetIPersistantDebug()->AddCone(pos, dir, baseRadius, height, color, duration);
}

void CDebugDrawComponent::DrawCylinder(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorF& color, float duration)
{
	gEnv->pGameFramework->GetIPersistantDebug()->Begin(__FUNCTION__, false);
	gEnv->pGameFramework->GetIPersistantDebug()->AddCylinder(pos, dir, radius, height, color, duration);
}

void CDebugDrawComponent::Draw2DText(Schematyc::CSharedString text, float size, const ColorF& color, float duration)
{
	gEnv->pGameFramework->GetIPersistantDebug()->Begin(__FUNCTION__, false);
	gEnv->pGameFramework->GetIPersistantDebug()->Add2DText(text.c_str(), size, color, duration);
}

void CDebugDrawComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_UPDATE:
		{
			Vec3 textPosition = GetWorldTransformMatrix().GetTranslation();
			float cameraDistance = (textPosition - gEnv->pSystem->GetViewCamera().GetPosition()).GetLength();

			if (cameraDistance > m_persistentViewDistance)
				return;

			float fontSize = m_persistentFontSize;

			if (m_shouldScaleWithCameraDistance)
			{
				fontSize *= 1 - (std::min(cameraDistance, static_cast<float>(m_persistentViewDistance)) / m_persistentViewDistance);
			}

			IRenderAuxText::DrawLabelEx(textPosition, fontSize, m_persistentTextColor, true, true, m_persistentText.c_str());
		}
		break;
	case ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED:
		{
			m_pEntity->UpdateComponentEventMask(this);
		}
		break;
	}
}

uint64 CDebugDrawComponent::GetEventMask() const
{
	uint64 bitFlags = m_drawPersistent ? ENTITY_EVENT_BIT(ENTITY_EVENT_UPDATE) : 0;
	bitFlags |= ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);

	return bitFlags;
}

void CDebugDrawComponent::SetPersistentText(const char* szText)
{
	m_persistentText = szText;
}

void CDebugDrawComponent::DrawText(Schematyc::CSharedString text, float x, float y, float size, const ColorF& color, float duration)
{
	gEnv->pGameFramework->GetIPersistantDebug()->Begin(__FUNCTION__, false);
	gEnv->pGameFramework->GetIPersistantDebug()->AddText(x, y, size, color, duration, text.c_str());
}

void CDebugDrawComponent::DrawText3D(Schematyc::CSharedString text, const Vec3& pos, float size, const ColorF& color, float duration)
{
	gEnv->pGameFramework->GetIPersistantDebug()->Begin(__FUNCTION__, false);
	gEnv->pGameFramework->GetIPersistantDebug()->AddText3D(pos, size, color, duration, text.c_str());
}

void CDebugDrawComponent::Draw2DLine(float x1, float y1, float x2, float y2, const ColorF& color, float duration)
{
	gEnv->pGameFramework->GetIPersistantDebug()->Begin(__FUNCTION__, false);
	gEnv->pGameFramework->GetIPersistantDebug()->Add2DLine(x1, y1, x2, y2, color, duration);
}

}
}
