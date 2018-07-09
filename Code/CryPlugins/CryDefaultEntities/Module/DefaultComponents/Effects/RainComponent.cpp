#include "StdAfx.h"
#include "RainComponent.h"

namespace Cry
{
	namespace DefaultComponents
	{
		void CRainComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
		{
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CRainComponent::Enable, "{DA911B91-3C7F-47A8-B18E-AAF48672F5F9}"_cry_guid, "Enable");
				pFunction->SetDescription("Enables or disables the component");
				pFunction->SetFlags({ Schematyc::EEnvFunctionFlags::Member, Schematyc::EEnvFunctionFlags::Construction });
				pFunction->BindInput(1, 'ena', "Enable");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CRainComponent::IsEnabled, "{C13A68AE-15B8-4C42-A35D-9118D7F7DD8F}"_cry_guid, "IsEnabled");
				pFunction->SetDescription("Returns if the component is enabled");
				pFunction->SetFlags({ Schematyc::EEnvFunctionFlags::Member, Schematyc::EEnvFunctionFlags::Construction });
				pFunction->BindOutput(0, 'ena', "Enabled");
				componentScope.Register(pFunction);
			}
		}

		void CRainComponent::Initialize()
		{
			PreloadTextures();
		}

		void CRainComponent::ProcessEvent(const SEntityEvent& event)
		{
			switch (event.event)
			{
			case ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED:
			{
				m_pEntity->UpdateComponentEventMask(this);
				Reset();
			}
			break;
			case ENTITY_EVENT_UPDATE:
			{
					const Vec3 vCamPos = GetISystem()->GetViewCamera().GetPosition();
					Vec3 vR = (GetEntity()->GetWorldPos() - vCamPos) / max(static_cast<float>(m_radius), 1e-3f);
					float fAttenAmount = max(0.f, 1.0f - vR.dot(vR));
					fAttenAmount *= m_amount;

					// Force set if current values not valid
					SRainParams currRainParams;
					bool bSet = !gEnv->p3DEngine->GetRainParams(currRainParams);

					// Set if stronger
					bSet |= fAttenAmount > currRainParams.fCurrentAmount;

					if (bSet)
					{
						SRainParams params;
						SetParams(params);
						params.vWorldPos = GetEntity()->GetWorldPos();
						params.qRainRotation = GetEntity()->GetWorldRotation();
						params.fCurrentAmount = fAttenAmount;

						// Remove Z axis rotation as it solely affects occlusion quality (not in a good way!)
						Ang3 rainRot(params.qRainRotation);
						rainRot.z = 0.0f;
						params.qRainRotation.SetRotationXYZ(rainRot);
						params.qRainRotation.NormalizeSafe();

						gEnv->p3DEngine->SetRainParams(params);
					}
			}
			break;
			case ENTITY_EVENT_HIDE:
			case ENTITY_EVENT_DONE:
			{
				Reset();
			}
			break;
			}
		}

		uint64 CRainComponent::GetEventMask() const
		{
			uint64 flags = ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED) | ENTITY_EVENT_BIT(ENTITY_EVENT_HIDE) | ENTITY_EVENT_BIT(ENTITY_EVENT_DONE);
			
			if (m_bEnable)
				flags |= ENTITY_EVENT_BIT(ENTITY_EVENT_UPDATE);

			return flags;
		}

		CRainComponent::~CRainComponent()
		{
			Reset();

			for (ITexture* pTexture : m_Textures)
			{
				pTexture->Release();
			}
		}

		void CRainComponent::Enable(bool bEnable)
		{
			m_bEnable = bEnable;
		
			if (!bEnable)
				Reset();

			m_pEntity->UpdateComponentEventMask(this);
		}

		bool CRainComponent::IsEnabled() const
		{
			return m_bEnable;
		}

		void CRainComponent::PreloadTextures()
		{
			uint32 nDefaultFlags = FT_DONT_STREAM;

			XmlNodeRef root = GetISystem()->LoadXmlFromFile("%ENGINE%/EngineAssets/raintextures.xml");
			if (root)
			{
				for (int i = 0; i < root->getChildCount(); i++)
				{
					XmlNodeRef entry = root->getChild(i);
					if (!entry->isTag("entry"))
						continue;

					uint32 nFlags = nDefaultFlags;

					// check attributes to modify the loading flags
					int nNoMips = 0;
					if (entry->getAttr("nomips", nNoMips) && nNoMips)
						nFlags |= FT_NOMIPS;

					ITexture* pTexture = gEnv->pRenderer->EF_LoadTexture(entry->getContent(), nFlags);
					if (pTexture)
					{
						m_Textures.push_back(pTexture);
					}
				}
			}
		}

		void CRainComponent::Reset()
		{
			static const Vec3 vZero(ZERO);
			SRainParams rainParams;
			rainParams.fAmount = 0.f;
			rainParams.vWorldPos = vZero;
			gEnv->p3DEngine->SetRainParams(rainParams);
		}

		void CRainComponent::SetParams(SRainParams& params)
		{
			params.bIgnoreVisareas = m_ignoreVisareas;
			params.bDisableOcclusion = m_disableOcclusion;
			params.fRadius = m_radius;
			params.fAmount = m_amount;
			params.vColor = Vec3(m_color.r, m_color.g, m_color.b);
			params.fFakeGlossiness = m_fakeGlossiness;
			params.fDiffuseDarkening = m_diffuseDarkening;
			params.fPuddlesAmount = m_puddlesAmount;
			params.fPuddlesMaskAmount = m_puddlesMaskAmount;
			params.fPuddlesRippleAmount = m_puddlesRippleAmount;
			params.fRainDropsAmount = m_rainDropsAmount;
			params.fRainDropsLighting = m_rainDropsLighting;
			params.fRainDropsSpeed = m_rainDropsSpeed;
			params.fMistAmount = m_mistAmount;
			params.fMistHeight = m_mistHeight;
			params.fFakeReflectionAmount = m_fakeReflectionAmount;
			params.fSplashesAmount = m_splashesAmount;
		}
	}
}
