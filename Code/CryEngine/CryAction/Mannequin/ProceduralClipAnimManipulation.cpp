// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "ICryMannequin.h"
#include <Mannequin/Serialization.h>
#include <CryExtension/ClassWeaver.h>
#include <CryExtension/CryCreateClassInstance.h>

struct SProceduralClipLayerWeightParams : public IProceduralParams
{
	SProceduralClipLayerWeightParams()
		: layer(0)
		, invert(false)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ar(paramName, "LayerWeightParam", "Layer Weight Param");
		ar(Serialization::Decorators::Range<uint32>(layer, 0, 15), "ScopeLayer", "Scope Layer");
		ar(invert, "Invert", "Invert");
	}

	virtual void GetExtraDebugInfo(StringWrapper& extraInfoOut) const override
	{
		extraInfoOut = paramName.c_str();
	}

	TProcClipString paramName;
	uint32          layer;
	bool            invert;
};

class CProceduralClipLayerWeight : public TProceduralClip<SProceduralClipLayerWeightParams>
{
public:
	CProceduralClipLayerWeight()
		: m_dataStringCRC(0)
		, m_animLayer(0)
		, m_invert(false)
	{
	}

	virtual void OnEnter(float blendTime, float duration, const SProceduralClipLayerWeightParams& params)
	{
		m_dataStringCRC = CCrc32::ComputeLowercase(params.paramName.c_str());
		m_invert = params.invert;
		uint32 layer = params.layer;

		CRY_ASSERT_MESSAGE(layer >= 0 && layer < m_scope->GetTotalLayers(), string().Format("CProceduralClipLayerWeight::OnEnter -> Invalid layer passed in: '%d'", layer));

		m_animLayer = m_scope->GetBaseLayer() + layer;

		UpdateLayerWeight();
	}

	virtual void OnExit(float blendTime)
	{
		m_charInstance->GetISkeletonAnim()->SetLayerBlendWeight(m_animLayer, 1.f);
	}

	virtual void Update(float timePassed)
	{
		UpdateLayerWeight();
	}

private:

	void UpdateLayerWeight()
	{
		float paramValue = 0.f;

		if (GetParam(m_dataStringCRC, paramValue))
		{
			paramValue = m_invert ? 1.0f - paramValue : paramValue;
			m_charInstance->GetISkeletonAnim()->SetLayerBlendWeight(m_animLayer, paramValue);
		}
	}

	uint32 m_dataStringCRC;
	uint8  m_animLayer;
	bool   m_invert;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipLayerWeight, "LayerWeight");

struct SProceduralClipIKControlledLayerWeightParams : public IProceduralParams
{
	SProceduralClipIKControlledLayerWeightParams()
		: layer(0)
		, invert(false)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ar(jointName, "JointName", "Joint Name");
		ar(Serialization::Decorators::Range<uint32>(layer, 0, 15), "ScopeLayer", "Scope Layer");
		ar(invert, "Invert", "Invert");
	}

	virtual void GetExtraDebugInfo(StringWrapper& extraInfoOut) const override
	{
		extraInfoOut = jointName.c_str();
	}

	TProcClipString jointName;
	uint32          layer;
	bool            invert;
};

class CProceduralClipIKControlledLayerWeight : public TProceduralClip<SProceduralClipIKControlledLayerWeightParams>
{
public:
	CProceduralClipIKControlledLayerWeight()
		: m_jointID(-1)
		, m_animLayer(0)
		, m_invert(false)
	{
	}

	virtual void OnEnter(float blendTime, float duration, const SProceduralClipIKControlledLayerWeightParams& params)
	{
		IDefaultSkeleton& defaultSkeleton = m_charInstance->GetIDefaultSkeleton();
		m_jointID = defaultSkeleton.GetJointIDByName(params.jointName.c_str());

		m_invert = params.invert;
		uint32 layer = params.layer;

		CRY_ASSERT_MESSAGE(layer >= 0 && layer < m_scope->GetTotalLayers(), string().Format("CProceduralClipIKControlledLayerWeight::OnEnter -> Invalid layer passed in: '%d'", layer));

		m_animLayer = m_scope->GetBaseLayer() + layer;

		UpdateLayerWeight();
	}

	virtual void OnExit(float blendTime)
	{
		m_charInstance->GetISkeletonAnim()->SetLayerBlendWeight(m_animLayer, 1.f);
	}

	virtual void Update(float timePassed)
	{
		UpdateLayerWeight();
	}

private:

	void UpdateLayerWeight()
	{
		if (m_jointID >= 0)
		{
			ISkeletonPose& skeletonPose = *m_charInstance->GetISkeletonPose();

			QuatT joint = skeletonPose.GetRelJointByID(m_jointID);

			float paramValue = clamp_tpl(joint.t.x, 0.f, 1.f);

			if (m_invert)
			{
				paramValue = 1.0f - paramValue;
			}

			m_charInstance->GetISkeletonAnim()->SetLayerBlendWeight(m_animLayer, paramValue);

		}
	}

	int16 m_jointID;
	uint8 m_animLayer;
	bool  m_invert;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipIKControlledLayerWeight, "IKLayerWeight");

struct SProceduralClipLayerManualUpdateParams
	: public IProceduralParams
{
	SProceduralClipLayerManualUpdateParams()
		: layer(0)
		, invert(false)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ar(paramName, "ParamName", "Param Name");
		ar(Serialization::Decorators::Range<uint32>(layer, 0, 15), "ScopeLayer", "Scope Layer");
		ar(invert, "Invert", "Invert");
	}

	virtual void GetExtraDebugInfo(StringWrapper& extraInfoOut) const override
	{
		extraInfoOut = paramName.c_str();
	}

	TProcClipString paramName;
	uint32          layer;
	bool            invert;
};

class CProceduralClipLayerManualUpdate : public TProceduralClip<SProceduralClipLayerManualUpdateParams>
{
public:
	CProceduralClipLayerManualUpdate()
		: m_dataStringCRC(0)
	{
	}

	virtual void OnEnter(float blendTime, float duration, const SProceduralClipLayerManualUpdateParams& params)
	{
		m_dataStringCRC = CCrc32::ComputeLowercase(params.paramName.c_str());
		uint32 layer = params.layer;

		CRY_ASSERT_MESSAGE(layer >= 0 && layer < m_scope->GetTotalLayers(), string().Format("CProceduralClipLayerManualUpdate::OnEnter -> Invalid layer passed in: '%d'", layer));

		CAnimation* pAnimation = NULL;
		ISkeletonAnim* pSkeletonAnim = NULL;

		if (GetSkeletonAndAnimation(pSkeletonAnim, pAnimation))
		{
			pAnimation->SetStaticFlag(CA_MANUAL_UPDATE);

			float paramValue = 0.f;

			if (GetParam(m_dataStringCRC, paramValue))
			{
				pSkeletonAnim->SetAnimationNormalizedTime(pAnimation, paramValue);
			}
		}
	}

	virtual void OnExit(float blendTime)
	{
		CAnimation* pAnimation = NULL;
		ISkeletonAnim* pSkeletonAnim = NULL;

		if (GetSkeletonAndAnimation(pSkeletonAnim, pAnimation))
		{
			pAnimation->ClearStaticFlag(CA_MANUAL_UPDATE);
		}
	}

	virtual void Update(float timePassed)
	{
		float paramValue = 0.f;

		if (GetParam(m_dataStringCRC, paramValue))
		{
			CAnimation* pAnimation = NULL;
			ISkeletonAnim* pSkeletonAnim = NULL;

			if (GetSkeletonAndAnimation(pSkeletonAnim, pAnimation))
			{
				pSkeletonAnim->SetAnimationNormalizedTime(pAnimation, paramValue);
			}
		}
	}

private:

	bool GetSkeletonAndAnimation(ISkeletonAnim*& pSkeletonAnim, CAnimation*& pAnimation)
	{
		pSkeletonAnim = m_charInstance->GetISkeletonAnim();
		pAnimation = m_scope->GetTopAnim(GetParams().layer);

		return (pAnimation != NULL);
	}

	uint32 m_dataStringCRC;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipLayerManualUpdate, "LayerManualUpdate");

struct SProceduralClipWeightedListParams : public IProceduralParams
{
	SProceduralClipWeightedListParams()
		: layer(0)
		, invert(false)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ar(paramName, "ParamName", "Param Name");
		ar(Serialization::Decorators::Range<uint32>(layer, 0, 15), "ScopeLayer", "Scope Layer");
		ar(invert, "Invert", "Invert");
	}

	virtual void GetExtraDebugInfo(StringWrapper& extraInfoOut) const override
	{
		extraInfoOut = paramName.c_str();
	}

	SProcDataCRC paramName;
	uint32       layer;
	bool         invert;
};

class CProceduralClipWeightedList : public TProceduralClip<SProceduralClipWeightedListParams>
{
public:
	CProceduralClipWeightedList()
		: m_baseLayer(0)
	{
	}

	virtual void OnEnter(float blendTime, float duration, const SProceduralClipWeightedListParams& params)
	{
		m_baseLayer = params.layer;
		UpdateLayerWeight();
	}

	virtual void OnExit(float blendTime)
	{
	}

	virtual void Update(float timePassed)
	{
		UpdateLayerWeight();
	}

private:

	void UpdateLayerWeight()
	{
		SWeightData data;
		if (GetParam(GetParams().paramName.crc, data))
		{
			const uint32 numLayers = min(m_scope->GetTotalLayers() - m_baseLayer, static_cast<uint32>(SWeightData::MAX_WEIGHTS));

			uint32 layer = m_baseLayer;
			for (uint32 i = 0; i < numLayers; i++, layer++)
			{
				float factor = clamp_tpl(data.weights[i], 0.f, 1.f);

				m_scope->ApplyAnimWeight(layer, factor);
			}
		}
	}

	uint32 m_baseLayer;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipWeightedList, "WeightedList");

struct SProceduralClipManualUpdateListParams : public IProceduralParams
{
	SProceduralClipManualUpdateListParams()
		: layer(0)
		, invert(false)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(paramName, "ParamName", "Param Name");
		ar(Serialization::Decorators::Range<uint32>(layer, 0, 15), "ScopeLayer", "Scope Layer");
		ar(invert, "Invert", "Invert");
	}

	SProcDataCRC paramName;
	uint32       layer;
	bool         invert;
};

class CProceduralClipManualUpdateList : public TProceduralClip<SProceduralClipManualUpdateListParams>
{
public:
	CProceduralClipManualUpdateList()
		: m_baseLayer(0)
	{
	}

	virtual void OnEnter(float blendTime, float duration, const SProceduralClipManualUpdateListParams& params)
	{
		m_baseLayer = params.layer;
		UpdateLayerTimes();
	}

	virtual void OnExit(float blendTime)
	{
	}

	virtual void Update(float timePassed)
	{
		UpdateLayerTimes();
	}

private:

	void UpdateLayerTimes()
	{
		SWeightData data;
		if (GetParam(GetParams().paramName.crc, data))
		{
			const uint32 numLayers = min(m_scope->GetTotalLayers() - m_baseLayer, static_cast<uint32>(SWeightData::MAX_WEIGHTS) * 2);

			uint32 layer = m_scope->GetBaseLayer() + m_baseLayer;
			for (uint32 i = 0; i < numLayers; i++, layer++)
			{
				uint32 factorID = i >> 1;
				float factor = data.weights[factorID];

				const bool bIsNegative = (i % 2) == 0;
				if (bIsNegative)
				{
					factor = -factor;
				}

				ISkeletonAnim& skeletonAnimation = *m_charInstance->GetISkeletonAnim();
				int numAnims = skeletonAnimation.GetNumAnimsInFIFO(layer);
				if (numAnims > 0)
				{
					CAnimation& animation = skeletonAnimation.GetAnimFromFIFO(layer, numAnims - 1);
					animation.SetStaticFlag(CA_MANUAL_UPDATE);
					animation.SetCurrentSegmentNormalizedTime(max(0.0f, factor));
				}
			}
		}
	}

	uint32 m_baseLayer;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipManualUpdateList, "ManualUpdateList");

struct SJointAdjustParams : public IProceduralParams
{
	SProcDataCRC jointName;
	uint32       layer;
	bool         additive;
	bool         relative;
	Vec3         position;

	SJointAdjustParams()
		: layer(0)
		, additive(true)
		, relative(true)
		, position(ZERO)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(Serialization::Decorators::JointName<SProcDataCRC>(jointName), "JointName", "Joint Name");
		ar(Serialization::Decorators::Range<uint32>(layer, 0, 15), "ScopeLayer", "Scope Layer");
		ar(additive, "Additive", "Additive");
		ar(relative, "Relative", "Relative To Parent");
		ar(position, "Position", "Position");
	}
};

class CProceduralClipPostProcessAdjust : public TProceduralClip<SJointAdjustParams>
{
public:
	CProceduralClipPostProcessAdjust()
		: m_operation(IAnimationOperatorQueue::eOp_AdditiveRelative)
		, m_jointID(-1)
		, m_layer(0)
		, m_position(ZERO)
	{
	}

	virtual void OnEnter(float blendTime, float duration, const SJointAdjustParams& params)
	{
		m_layer = params.layer;
		m_position = params.position;

		if (params.additive)
		{
			m_operation = (params.relative) ? IAnimationOperatorQueue::eOp_OverrideRelative : IAnimationOperatorQueue::eOp_Override;
		}
		else
		{
			m_operation = (params.relative) ? IAnimationOperatorQueue::eOp_AdditiveRelative : IAnimationOperatorQueue::eOp_Additive;
		}

		IDefaultSkeleton& defaultSkeleton = m_charInstance->GetIDefaultSkeleton();
		int16 jointID = defaultSkeleton.GetJointIDByCRC32(params.jointName.crc);

		if (jointID >= 0)
		{
			m_jointID = jointID;
			CryCreateClassInstanceForInterface(cryiidof<IAnimationOperatorQueue>(), m_pPoseModifier);
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "CProceduralClipPostProcessAdjust::OnEnter joint '%s' doesn't exist in skeleton '%s'", params.jointName.c_str(), m_charInstance->GetFilePath());
		}
	}

	virtual void OnExit(float blendTime)
	{
	}

	virtual void Update(float timePassed)
	{
		if (m_jointID >= 0)
		{
			if (ICharacterInstance* pCharacter = m_scope->GetCharInst())
			{
				pCharacter->GetISkeletonAnim()->PushPoseModifier(m_scope->GetBaseLayer() + m_layer, m_pPoseModifier, "ProceduralClipPostProcessAdjust");

				m_pPoseModifier->Clear();
				m_pPoseModifier->PushPosition(m_jointID, m_operation, m_position);
			}
		}
	}

private:

	IAnimationOperatorQueue::EOp m_operation;
	int16                        m_jointID;
	uint32                       m_layer;
	Vec3                         m_position;
	IAnimationOperatorQueuePtr   m_pPoseModifier;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipPostProcessAdjust, "JointAdjust");

struct SProceduralClipLayerAnimSpeedParams : public IProceduralParams
{
	SProceduralClipLayerAnimSpeedParams()
		: layer(0)
		, invert(false)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ar(paramName, "ParamName", "Param Name");
		ar(Serialization::Decorators::Range<uint32>(layer, 0, 15), "ScopeLayer", "Scope Layer");
		ar(invert, "Invert", "Invert");
	}

	virtual void GetExtraDebugInfo(StringWrapper& extraInfoOut) const override
	{
		extraInfoOut = paramName.c_str();
	}

	SProcDataCRC paramName;
	uint32       layer;
	bool         invert;
};

class CProceduralClipLayerAnimSpeed : public TProceduralClip<SProceduralClipLayerAnimSpeedParams>
{
public:
	CProceduralClipLayerAnimSpeed()
		: m_dataStringCRC(0)
		, m_animLayer(0)
		, m_invert(false)
	{
	}

	virtual void OnEnter(float blendTime, float duration, const SProceduralClipLayerAnimSpeedParams& params)
	{
		m_dataStringCRC = params.paramName.crc;
		m_invert = params.invert;

		CRY_ASSERT_MESSAGE(params.layer >= 0 && params.layer < m_scope->GetTotalLayers(), string().Format("CProceduralClipLayerAnimSpeed::OnEnter -> Invalid layer passed in: '%d'", params.layer));

		m_animLayer = m_scope->GetBaseLayer() + params.layer;

		UpdateLayerAnimSpeed();
	}

	virtual void OnExit(float blendTime)
	{
		m_charInstance->GetISkeletonAnim()->SetLayerPlaybackScale(m_animLayer, 1.0f);
	}

	virtual void Update(float timePassed)
	{
		UpdateLayerAnimSpeed();
	}

private:

	void UpdateLayerAnimSpeed()
	{
		float paramValue = 0.f;

		if (GetParam(m_dataStringCRC, paramValue))
		{
			paramValue = m_invert ? 1.0f - paramValue : paramValue;
			m_charInstance->GetISkeletonAnim()->SetLayerPlaybackScale(m_animLayer, paramValue);
		}
	}

	uint32 m_dataStringCRC;
	uint8  m_animLayer;
	bool   m_invert;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipLayerAnimSpeed, "LayerAnimSpeed");
