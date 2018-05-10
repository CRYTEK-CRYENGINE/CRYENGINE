// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "ICryMannequin.h"
#include <CryExtension/ClassWeaver.h>
#include <CryExtension/CryCreateClassInstance.h>
#include <Mannequin/Serialization.h>

struct SSetParamParams : public IProceduralParams
{
	SProcDataCRC paramName;
	float        target;
	float        exitTarget;

	SSetParamParams()
		: target(0.f)
		, exitTarget(1.f)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(paramName, "ParamName", "Param Name");
		ar(target, "Target", "Target");
		ar(exitTarget, "ExitTarget", "Exit Target");
	}
};

struct SParamTarget
{
	SParamTarget(float _target, float _start, float _blend)
		: targetValue(_target)
		, startValue(_start)
		, blendRate(_blend)
		, currentFraction(0.f)
	{}
	float targetValue;
	float startValue;
	float blendRate;
	float currentFraction;
};

class CProceduralContext_SetParam : public IProceduralContext
{
private:
	typedef IProceduralContext BaseClass;

	virtual ~CProceduralContext_SetParam() {}

public:
	PROCEDURAL_CONTEXT(CProceduralContext_SetParam, "SetParamContext", "c6c08712-5781-4854-adc5-6ab4252834bd"_cry_guid);

	virtual void Update(float timePassed) override
	{
		VectorMap<uint32, SParamTarget>::iterator it = m_paramTargets.begin();

		while (it != m_paramTargets.end())
		{
			float currentFraction = min(1.f, it->second.currentFraction + (it->second.blendRate * timePassed));
			float newValue = LERP(it->second.startValue, it->second.targetValue, currentFraction);

			m_actionController->SetParam(it->first, newValue);

			if (currentFraction >= 1.f)
			{
				it = m_paramTargets.erase(it);
			}
			else
			{
				it->second.currentFraction = currentFraction;
				++it;
			}
		}
	}

	void SetParamTarget(uint32 paramCRC, float paramTarget, float blendRate)
	{
		VectorMap<uint32, SParamTarget>::iterator it = m_paramTargets.find(paramCRC);

		if (blendRate < FLT_MAX)
		{
			if (it != m_paramTargets.end())
			{
				it->second.targetValue = paramTarget;
				it->second.blendRate = blendRate;
				it->second.currentFraction = 0.f;

				m_actionController->GetParam(paramCRC, it->second.startValue);
			}
			else
			{
				SParamTarget newParamTarget(paramTarget, 0.f, blendRate);
				m_actionController->GetParam(paramCRC, newParamTarget.startValue);

				m_paramTargets.insert(VectorMap<uint32, SParamTarget>::value_type(paramCRC, newParamTarget));
			}
		}
		else
		{
			if (it != m_paramTargets.end())
			{
				m_paramTargets.erase(it);
			}

			m_actionController->SetParam(paramCRC, paramTarget);
		}
	}

private:
	VectorMap<uint32, SParamTarget> m_paramTargets;
};

CRYREGISTER_CLASS(CProceduralContext_SetParam);

class CProceduralClipSetParam : public TProceduralContextualClip<CProceduralContext_SetParam, SSetParamParams>
{
public:
	CProceduralClipSetParam();

	virtual void OnEnter(float blendTime, float duration, const SSetParamParams& params)
	{
		float blendRate = blendTime > 0.f ? __fres(blendTime) : FLT_MAX;

		m_context->SetParamTarget(params.paramName.crc, params.target, blendRate);

		m_paramCRC = params.paramName.crc;
		m_exitTarget = params.exitTarget;
	}

	virtual void OnExit(float blendTime)
	{
		float blendRate = blendTime > 0.f ? __fres(blendTime) : FLT_MAX;
		m_context->SetParamTarget(m_paramCRC, m_exitTarget, blendRate);
	}

	virtual void Update(float timePassed)
	{
	}

protected:
	uint32 m_paramCRC;
	float  m_exitTarget;
};

CProceduralClipSetParam::CProceduralClipSetParam() : m_paramCRC(0), m_exitTarget(0.f)
{
}

REGISTER_PROCEDURAL_CLIP(CProceduralClipSetParam, "SetParam");
