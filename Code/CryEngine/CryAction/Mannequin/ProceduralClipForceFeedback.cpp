// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "ICryMannequin.h"
#include "IForceFeedbackSystem.h"

#include <Mannequin/Serialization.h>

struct SForceFeedbackClipParams : public IProceduralParams
{
	SForceFeedbackClipParams()
		: scale(1.f)
		, delay(0)
		, onlyLocal(true)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ar(Serialization::Decorators::ForceFeedbackIdName<TProcClipString>(forceFeedbackId), "ForceFeedbackID", "ForceFeedbackID");
		ar(scale, "Scale", "Scale");
		ar(delay, "Delay", "Delay");
		ar(onlyLocal, "OnlyLocal", "OnlyLocal");
	}

	virtual void GetExtraDebugInfo(StringWrapper& extraInfoOut) const override
	{
		extraInfoOut = forceFeedbackId.c_str();
	}

	TProcClipString forceFeedbackId;
	float           scale;
	CTimeValue      delay;
	bool            onlyLocal;
};

class CProceduralClipForceFeedback : public TProceduralClip<SForceFeedbackClipParams>
{
public:
	CProceduralClipForceFeedback()
	{
	}

	virtual void OnEnter(const CTimeValue& blendTime, const CTimeValue& duration, const SForceFeedbackClipParams& params)
	{
		IGameFramework* pGameFrameWork = gEnv->pGameFramework;

		if ((gEnv->IsEditor() && pGameFrameWork->GetMannequinInterface().IsSilentPlaybackMode())
		    || (params.onlyLocal && pGameFrameWork->GetClientActorId() != m_entity->GetId()))
		{
			return;
		}

		IForceFeedbackSystem* pForceFeedback = CCryAction::GetCryAction()->GetIForceFeedbackSystem();
		CRY_ASSERT(pForceFeedback);
		ForceFeedbackFxId fxId = pForceFeedback->GetEffectIdByName(params.forceFeedbackId.c_str());

		if (fxId != InvalidForceFeedbackFxId)
		{
			float actionScale = 0.f;
			float ffScale = params.scale;

			if (GetParam("ffScale", actionScale))
			{
				ffScale *= actionScale;
			}

			SForceFeedbackRuntimeParams runtimeParams(ffScale, params.delay);
			pForceFeedback->PlayForceFeedbackEffect(fxId, runtimeParams);
		}
	}

	virtual void OnExit(const CTimeValue& blendTime)  {}

	virtual void Update(const CTimeValue& timePassed) {}

};

REGISTER_PROCEDURAL_CLIP(CProceduralClipForceFeedback, "ForceFeedback");
