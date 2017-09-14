#include "StdAfx.h"
#include "CameraComponent.h"

#include <array>

namespace Cry
{
	namespace DefaultComponents
	{
		void CCameraComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
		{
			// Functions
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCameraComponent::Activate, "{56EFC341-8541-4A85-9870-68F2774BDED4}"_cry_guid, "Activate");
				pFunction->SetDescription("Makes this camera active");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCameraComponent::IsActive, "{5DBFFF7C-7D82-4645-ACE3-561748376568}"_cry_guid, "IsActive");
				pFunction->SetDescription("Is this camera active?");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				pFunction->BindOutput(0, 'iact', "IsActive");
				componentScope.Register(pFunction);
			}
		}

#ifndef RELEASE
		void CCameraComponent::Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const
		{
			if (context.bSelected)
			{
				Matrix34 slotTransform = GetWorldTransformMatrix();

				// Don't use actual far plane as it's usually huge for cameras
				float distance = 10.f;
				float size = distance * tan(m_fieldOfView.ToRadians());

				std::array<Vec3, 4> points =
				{ 
					{
						Vec3(size, distance, size),
						Vec3(-size, distance, size),
						Vec3(-size, distance, -size),
						Vec3(size, distance, -size)
					}
				};

				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(slotTransform.GetTranslation(), context.debugDrawInfo.color, slotTransform.TransformPoint(points[0]), context.debugDrawInfo.color);
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(slotTransform.GetTranslation(), context.debugDrawInfo.color, slotTransform.TransformPoint(points[1]), context.debugDrawInfo.color);
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(slotTransform.GetTranslation(), context.debugDrawInfo.color, slotTransform.TransformPoint(points[2]), context.debugDrawInfo.color);
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(slotTransform.GetTranslation(), context.debugDrawInfo.color, slotTransform.TransformPoint(points[3]), context.debugDrawInfo.color);

				Vec3 p1 = slotTransform.TransformPoint(points[0]);
				Vec3 p2;
				for (int i = 0; i < points.size(); i++)
				{
					int j = (i + 1) % points.size();
					p2 = slotTransform.TransformPoint(points[j]);
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(p1, context.debugDrawInfo.color, p2, context.debugDrawInfo.color);
					p1 = p2;
				}
			}
		}
#endif
	}
}