// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Rain.h"

#include <CryFlowGraph/IFlowBaseNode.h>

CRain::CRain() : m_bEnabled(false)
{
}

CRain::~CRain()
{
	for (TTextureList::iterator it = m_Textures.begin(), itEnd = m_Textures.end(); it != itEnd; ++it)
	{
		(*it)->Release();
	}
}

//------------------------------------------------------------------------
bool CRain::Init(IGameObject* pGameObject)
{
	SetGameObject(pGameObject);

	if (!gEnv->IsDedicated())
	{
		PreloadTextures();
	}

	return Reset();
}

//------------------------------------------------------------------------
void CRain::PreloadTextures()
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

//------------------------------------------------------------------------
void CRain::PostInit(IGameObject* pGameObject)
{
	GetGameObject()->EnableUpdateSlot(this, 0);
}

//------------------------------------------------------------------------
bool CRain::ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params)
{
	ResetGameObject();

	CRY_ASSERT_MESSAGE(false, "CRain::ReloadExtension not implemented");

	return false;
}

//------------------------------------------------------------------------
bool CRain::GetEntityPoolSignature(TSerialize signature)
{
	CRY_ASSERT_MESSAGE(false, "CRain::GetEntityPoolSignature not implemented");

	return true;
}

//------------------------------------------------------------------------
void CRain::Release()
{
	delete this;
}

//------------------------------------------------------------------------
void CRain::FullSerialize(TSerialize ser)
{
	ser.Value("bEnabled", m_bEnabled);
	ser.Value("bIgnoreVisareas", m_params.bIgnoreVisareas);
	ser.Value("bDisableOcclusion", m_params.bDisableOcclusion);
	ser.Value("fRadius", m_params.fRadius);
	ser.Value("fAmount", m_params.fAmount);
	ser.Value("clrColor", m_params.vColor);
	ser.Value("fFakeGlossiness", m_params.fFakeGlossiness);
	ser.Value("fFakeReflectionAmount", m_params.fFakeReflectionAmount);
	ser.Value("fDiffuseDarkening", m_params.fDiffuseDarkening);
	ser.Value("fRainDropsAmount", m_params.fRainDropsAmount);
	ser.Value("fRainDropsSpeed", m_params.fRainDropsSpeed);
	ser.Value("fRainDropsLighting", m_params.fRainDropsLighting);
	ser.Value("fMistAmount", m_params.fMistAmount);
	ser.Value("fMistHeight", m_params.fMistHeight);
	ser.Value("fPuddlesAmount", m_params.fPuddlesAmount);
	ser.Value("fPuddlesMaskAmount", m_params.fPuddlesMaskAmount);
	ser.Value("fPuddlesRippleAmount", m_params.fPuddlesRippleAmount);
	ser.Value("fSplashesAmount", m_params.fSplashesAmount);
}

//------------------------------------------------------------------------
void CRain::Update(SEntityUpdateContext& ctx, int updateSlot)
{
	const IActor* pClient = g_pGame->GetIGameFramework()->GetClientActor();
	if (pClient && Reset())
	{
		const Vec3 vCamPos = GetISystem()->GetViewCamera().GetPosition();
		Vec3 vR = (GetEntity()->GetWorldPos() - vCamPos) / max(m_params.fRadius, 1e-3f);
		float fAttenAmount = max(0.f, 1.0f - vR.dot(vR));
		fAttenAmount *= m_params.fAmount;

		// Force set if current values not valid
		SRainParams currRainParams;
		bool bSet = !gEnv->p3DEngine->GetRainParams(currRainParams);

		// Set if stronger
		bSet |= fAttenAmount > currRainParams.fCurrentAmount;

		if (bSet)
		{
			m_params.vWorldPos = GetEntity()->GetWorldPos();
			m_params.qRainRotation = GetEntity()->GetWorldRotation();
			m_params.fCurrentAmount = fAttenAmount;

			// Remove Z axis rotation as it solely affects occlusion quality (not in a good way!)
			Ang3 rainRot(m_params.qRainRotation);
			rainRot.z = 0.0f;
			m_params.qRainRotation.SetRotationXYZ(rainRot);
			m_params.qRainRotation.NormalizeSafe();

			gEnv->p3DEngine->SetRainParams(m_params);
		}
	}
}

//------------------------------------------------------------------------
void CRain::HandleEvent(const SGameObjectEvent& event)
{
}

uint64 CRain::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_RESET) | ENTITY_EVENT_BIT(ENTITY_EVENT_HIDE) | ENTITY_EVENT_BIT(ENTITY_EVENT_DONE);
}

//------------------------------------------------------------------------
void CRain::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_RESET:
		Reset();
		break;
	case ENTITY_EVENT_HIDE:
	case ENTITY_EVENT_DONE:
		if (gEnv && gEnv->p3DEngine)
		{
			static const Vec3 vZero(ZERO);
			SRainParams rainParams;
			rainParams.fAmount = 0.f;
			rainParams.vWorldPos = vZero;
			gEnv->p3DEngine->SetRainParams(rainParams);
		}
		break;
	}
}

//------------------------------------------------------------------------
bool CRain::Reset()
{
	//Initialize default values before (in case ScriptTable fails)
	m_bEnabled = false;
	m_params.bIgnoreVisareas = false;
	m_params.bDisableOcclusion = false;
	m_params.fRadius = 10000.f;
	m_params.fAmount = 1.f;
	m_params.vColor.Set(1, 1, 1);
	m_params.fFakeGlossiness = 0.5f;
	m_params.fDiffuseDarkening = 0.5f;
	m_params.fPuddlesAmount = 1.5f;
	m_params.fPuddlesMaskAmount = 1.0f;
	m_params.fPuddlesRippleAmount = 2.0f;
	m_params.fRainDropsAmount = 0.5f;
	m_params.fRainDropsSpeed = 1.f;
	m_params.fRainDropsLighting = 1.f;
	m_params.fMistAmount = 3.f;
	m_params.fMistHeight = 8.f;
	m_params.fFakeReflectionAmount = 1.5f;
	m_params.fSplashesAmount = 1.3f;

	SmartScriptTable props;
	IScriptTable* pScriptTable = GetEntity()->GetScriptTable();
	if (!pScriptTable || !pScriptTable->GetValue("Properties", props))
		return false;

	props->GetValue("bIgnoreVisareas", m_params.bIgnoreVisareas);
	props->GetValue("bDisableOcclusion", m_params.bDisableOcclusion);
	props->GetValue("fRadius", m_params.fRadius);
	props->GetValue("fAmount", m_params.fAmount);
	props->GetValue("clrColor", m_params.vColor);
	props->GetValue("fFakeGlossiness", m_params.fFakeGlossiness);
	props->GetValue("fFakeReflectionsAmount", m_params.fFakeReflectionAmount);
	props->GetValue("fDiffuseDarkening", m_params.fDiffuseDarkening);
	props->GetValue("fRainDropsAmount", m_params.fRainDropsAmount);
	props->GetValue("fRainDropsSpeed", m_params.fRainDropsSpeed);
	props->GetValue("fRainDropsLighting", m_params.fRainDropsLighting);
	props->GetValue("fMistAmount", m_params.fMistAmount);
	props->GetValue("fMistHeight", m_params.fMistHeight);
	props->GetValue("fPuddlesAmount", m_params.fPuddlesAmount);
	props->GetValue("fPuddlesMaskAmount", m_params.fPuddlesMaskAmount);
	props->GetValue("fPuddlesRippleAmount", m_params.fPuddlesRippleAmount);
	props->GetValue("fSplashesAmount", m_params.fSplashesAmount);

	props->GetValue("bEnabled", m_bEnabled);
	if (!m_bEnabled)
		m_params.fAmount = 0;

	return true;
}

//////////////////////////////////////////////////////////////////////////
class CFlowNode_RainProperties : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowNode_RainProperties(SActivationInfo* pActInfo)
	{
	}

	enum EInputs
	{
		eI_Trigger,
		eI_Amount,
		eI_PuddlesAmount,
		eI_PuddlesRippleAmount,
		eI_RainDropsAmount,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Trigger",               _HELP("Update the engine")),
			InputPortConfig<float>("Amount",              1.0f,                       _HELP("Overall Amount")),
			InputPortConfig<float>("PuddlesAmount",       1.5f,                       _HELP("PuddlesAmount")),
			InputPortConfig<float>("PuddlesRippleAmount", 2.0f,                       _HELP("PuddlesRippleAmount")),
			InputPortConfig<float>("RainDropsAmount",     0.5f,                       _HELP("RainDropsAmount")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Rain Entity Properties");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		EFlowEvent eventType = event;

		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eI_Trigger))
				{
					pActInfo->pGraph->RequestFinalActivation(pActInfo->myID);
				}
				break;
			}

		case eFE_FinalActivate:
			{
				if (pActInfo->pEntity)
				{
					EntityId targetEntity = pActInfo->pEntity->GetId();

					if (targetEntity != INVALID_ENTITYID)
					{
						IEntity* pEntity = gEnv->pEntitySystem->GetEntity(targetEntity);
						if (pEntity)
						{
							SmartScriptTable props;
							IScriptTable* pScriptTable = pEntity->GetScriptTable();
							if (pScriptTable && pScriptTable->GetValue("Properties", props))
							{
								props->SetValue("fAmount", GetPortFloat(pActInfo, eI_Amount));
								props->SetValue("fPuddlesAmount", GetPortFloat(pActInfo, eI_PuddlesAmount));
								props->SetValue("fPuddlesRippleAmount", GetPortFloat(pActInfo, eI_PuddlesRippleAmount));
								props->SetValue("fRainDropsAmount", GetPortFloat(pActInfo, eI_RainDropsAmount));
								SEntityEvent e;
								e.event = ENTITY_EVENT_RESET;
								pEntity->SendEvent(e);
							}
						}
					}
				}
				break;
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_RainProperties(pActInfo);
	}
};

REGISTER_FLOW_NODE("Environment:RainProperties", CFlowNode_RainProperties)
