#include "StdAfx.h"
#include "WaterRippleComponent.h"
#include <CryMath/Random.h>

namespace Cry
{
	namespace DefaultComponents
	{
		void CWaterRippleComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
		{
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CWaterRippleComponent::Enable, "{BB44DB39-BD03-402F-B2E4-152B6D2E295A}"_cry_guid, "Enable");
				pFunction->SetDescription("Enables or disables the component");
				pFunction->SetFlags({ Schematyc::EEnvFunctionFlags::Member, Schematyc::EEnvFunctionFlags::Construction });
				pFunction->BindInput(1, 'ena', "Enable");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CWaterRippleComponent::SpawnRippleAtPosition, "{DF99F8E1-56B5-4E59-86C0-D0A00020839F}"_cry_guid, "SpawnRippleAtPosition");
				pFunction->SetDescription("Spawns ripple, if there is water at the position");
				pFunction->SetFlags({ Schematyc::EEnvFunctionFlags::Member, Schematyc::EEnvFunctionFlags::Construction });
				pFunction->BindInput(1, 'pos', "Position");
				pFunction->BindInput(2, 'str', "Strength");
				pFunction->BindInput(3, 'sca', "Scale");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CWaterRippleComponent::IsEnabled, "{EC45ADC0-3CDE-4739-958B-2D8662A9D703}"_cry_guid, "IsEnabled");
				pFunction->SetDescription("Returns if the component is enabled");
				pFunction->SetFlags({ Schematyc::EEnvFunctionFlags::Member, Schematyc::EEnvFunctionFlags::Construction });
				pFunction->BindOutput(0, 'ena', "Enabled");
				componentScope.Register(pFunction);
			}
		}

		void CWaterRippleComponent::Initialize()
		{
		}

		void CWaterRippleComponent::ProcessEvent(const SEntityEvent& event)
		{
			switch (event.event)
			{
			case ENTITY_EVENT_XFORM:
			{
				if (m_bSpawnOnMovement)
					ProcessHit(true);

				if (gEnv->IsEditor())
				{
					m_currentLocationOk = TestLocation(GetEntity()->GetWorldPos());
				}
			}
			break;
			case ENTITY_EVENT_RESET:
			{
				if (gEnv->IsEditor())
				{
					const bool leavingGameMode = (event.nParam[0] == 0);
					if (leavingGameMode)
					{
						Reset();
					}
				}
			}
			break;
			case ENTITY_EVENT_UPDATE:
			{
				float fFrequency = m_frequency + cry_random(-1.0f, 1.0f)*m_randFrequency;
				bool allowHit = (gEnv->pTimer->GetCurrTime() - m_lastSpawnTime) > fFrequency;
				if (m_bAutoSpawn && allowHit)
					ProcessHit(false);
			}
			break;
			}
		}

		uint64 CWaterRippleComponent::GetEventMask() const
		{
			uint64 flags = ENTITY_EVENT_BIT(ENTITY_EVENT_RESET) | ENTITY_EVENT_BIT(ENTITY_EVENT_XFORM);
			if (m_bEnabled)
				flags |= ENTITY_EVENT_BIT(ENTITY_EVENT_UPDATE);

			return flags;
		}

		void CWaterRippleComponent::Enable(const bool bEnable)
		{
			if (bEnable && (gEnv->IsEditor() || m_bAutoSpawn))
			{
				m_bEnabled = true;
			}
			else
			{
				m_bEnabled = false;
			}

			m_pEntity->UpdateComponentEventMask(this);
		}

		bool CWaterRippleComponent::IsEnabled() const
		{
			return m_bEnabled;
		}

		void CWaterRippleComponent::SpawnRippleAtPosition(const Vec3& position, const float strength, const float scale) const
		{
			if (gEnv->p3DEngine)
			{
				gEnv->p3DEngine->AddWaterRipple(position, scale, strength);
			}
		}

		void CWaterRippleComponent::ProcessHit(bool bIsMoving)
		{
			if (m_bEnabled)
			{
				Vec3 vWorldPos = GetEntity()->GetWorldPos();
				if (!bIsMoving) // No random offsets during movement spawning.
				{
					vWorldPos += Vec3(m_randomOffset).CompMul(cry_random_componentwise(Vec3(-1, -1, 0), Vec3(1, 1, 0)));
				}

				float fScale = m_scale + cry_random(-1.0f, 1.0f) * m_randScale;
				float fStrength = m_strength + cry_random(-1.0f, 1.0f) * m_randStrength;

				if (gEnv->p3DEngine)
				{
					gEnv->p3DEngine->AddWaterRipple(vWorldPos, fScale, fStrength);
				}

				float fTime = gEnv->pTimer->GetCurrTime();
				m_lastSpawnTime = fTime;
			}
		}

		void CWaterRippleComponent::Reset()
		{
			Enable(m_bEnabled);

			m_currentLocationOk = TestLocation(GetEntity()->GetWorldPos());
		}

		bool CWaterRippleComponent::TestLocation(const Vec3& testPosition)
		{
			const float threshold = 0.4f;
			const float waterLevel = gEnv->p3DEngine->GetWaterLevel(&testPosition);

			return (fabs_tpl(waterLevel - testPosition.z) < threshold);
		}

#ifndef RELEASE
		void CWaterRippleComponent::Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const
		{
			IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();

			const ColorF colorGoodPosition0(0.0f, 0.0f, 0.5f, 0.0f);
			const ColorF colorGoodPosition1(0.0f, 0.5f, 1.0f, 1.0f);
			const ColorF colorBadPosition0(1.0f, 0.1f, 0.1f, 0.0f);
			const ColorF colorBadPosition1(1.0f, 0.1f, 0.1f, 1.0f);

			const ColorF& surfaceColor0 = m_currentLocationOk ? colorGoodPosition0 : colorBadPosition0;
			const ColorF& surfaceColor1 = m_currentLocationOk ? colorGoodPosition1 : colorBadPosition1;

			const Vec3 surfacePosition(GetEntity()->GetWorldPos());

			pRenderAux->DrawSphere(surfacePosition, 0.1f, surfaceColor1);

			const static int lines = 8;
			const static float radius0 = 0.5f;
			const static float radius1 = 1.0f;
			const static float radius2 = 2.0f;
			for (int i = 0; i < lines; ++i)
			{
				const float radians = ((float)i / (float)lines) * gf_PI2;
				const float cosine = cos_tpl(radians);
				const float sinus = sin_tpl(radians);
				const Vec3 offset0(radius0 * cosine, radius0 * sinus, 0);
				const Vec3 offset1(radius1 * cosine, radius1 * sinus, 0);
				const Vec3 offset2(radius2 * cosine, radius2 * sinus, 0);
				pRenderAux->DrawLine(surfacePosition + offset0, surfaceColor0, surfacePosition + offset1, surfaceColor1, 2.0f);
				pRenderAux->DrawLine(surfacePosition + offset1, surfaceColor1, surfacePosition + offset2, surfaceColor0, 2.0f);
			}
		}
#endif
	}
}
