// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PoseModifier.h"

#include "PoseModifierHelper.h"
#include "../Skeleton.h"
#include <CrySerialization/Forward.h>
#include "PoseModifierDesc.h"
#include "../Serialization/SerializationCommon.h"

namespace PoseModifier
{

Quat RotationVectorToVector(const Vec3& vector0, const Vec3& vector1)
{
	float cosine = vector0 * vector1 + 1.0f;
	if (cosine > 0.00001f)
	{
		Vec3 vector = vector0.Cross(vector1);
		return Quat(cosine, vector).GetNormalized();
	}
	return Quat(0.0f, vector0.GetOrthogonal().GetNormalized());
}

/*
   CConstraintPoint
 */

class CConstraintPoint :
	public IAnimationPoseModifier,
	public IAnimationSerializable
{
public:
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimationPoseModifier)
	CRYINTERFACE_ADD(IAnimationSerializable)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CConstraintPoint, "AnimationPoseModifier_ConstraintPoint", "705fd8b7-906f-42a1-b7d6-d5dee73d3b54"_cry_guid)

	CConstraintPoint();
	virtual ~CConstraintPoint() {}

private:
	void Draw(const Vec3& point1, const Vec3& target) const;

	// IAnimationPoseModifier
public:
	virtual bool Prepare(const SAnimationPoseModifierParams& params) override;
	virtual bool Execute(const SAnimationPoseModifierParams& params) override;
	virtual void Synchronize() override                           {}

	void         GetMemoryUsage(ICrySizer* pSizer) const override {}

	// IAnimationSerializable
public:
	virtual void Serialize(Serialization::IArchive& ar) override;

private:
	SConstraintPointDesc m_desc;
	uint                 m_nodeIndex;
	uint                 m_pointNodeIndex;
	uint                 m_weightNodeIndex;
	bool                 m_bInitialized;

	bool                 m_bDraw;
};

CRYREGISTER_CLASS(CConstraintPoint)

//

CConstraintPoint::CConstraintPoint() :
	m_nodeIndex(-1),
	m_pointNodeIndex(-1),
	m_weightNodeIndex(-1),
	m_bInitialized(false),
	m_bDraw(false)
{
}

//

void CConstraintPoint::Draw(const Vec3& point, const Vec3& target) const
{
	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	if (!pAuxGeom)
		return;

	SAuxGeomRenderFlags flags = pAuxGeom->GetRenderFlags();
	flags.SetDepthTestFlag(e_DepthTestOff);
	flags.SetAlphaBlendMode(e_AlphaBlended);
	pAuxGeom->SetRenderFlags(flags);

	pAuxGeom->DrawSphere(point, 0.01f, ColorB(0xff, 0x80, 0x80, 0x80));
	IRenderAuxText::DrawLabel(point, 1.0f, "Point");

	pAuxGeom->DrawSphere(target, 0.01f, ColorB(0xff, 0xff, 0xff, 0x80));
}

// IAnimationPoseModifier

bool CConstraintPoint::Prepare(const SAnimationPoseModifierParams& params)
{
	if (m_bInitialized)
		return true;

	m_nodeIndex = m_desc.node.ResolveJointIndex(PoseModifierHelper::GetDefaultSkeleton(params));
	m_pointNodeIndex = m_desc.point.node.ResolveJointIndex(PoseModifierHelper::GetDefaultSkeleton(params));
	m_weightNodeIndex = m_desc.weightNode.ResolveJointIndex(PoseModifierHelper::GetDefaultSkeleton(params));

	m_bInitialized = true;
	return true;
}

bool CConstraintPoint::Execute(const SAnimationPoseModifierParams& params)
{
	Skeleton::CPoseData* pPoseData = Skeleton::CPoseData::GetPoseData(params.pPoseData);
	if (!pPoseData)
		return false;

	const CDefaultSkeleton& defaultSkeleton = PoseModifierHelper::GetDefaultSkeleton(params);

	if (m_nodeIndex >= pPoseData->GetJointCount() ||
	    m_pointNodeIndex >= pPoseData->GetJointCount())
	{
		return false;
	}

	float weight = m_desc.weight;
	if (m_weightNodeIndex != uint(-1))
	{
		weight *= clamp_tpl(pPoseData->GetJointRelative(m_weightNodeIndex).t.x, 0.0f, 1.0f);
	}

	const QuatT& nodeAbsolute = pPoseData->GetJointAbsolute(m_nodeIndex);

	const QuatT& pointAbsolute = pPoseData->GetJointAbsolute(m_pointNodeIndex);
	const Vec3 point = pointAbsolute.t + pointAbsolute.q * m_desc.point.localOffset + m_desc.point.worldOffset;
	const Vec3 nodeTargetPoint = Vec3::CreateLerp(nodeAbsolute.t, point, weight);

	PoseModifierHelper::SetJointAbsolutePosition(defaultSkeleton, *pPoseData, m_nodeIndex, nodeTargetPoint);

	if (m_bDraw)
		Draw(params.location * point, params.location * nodeTargetPoint);
	return true;
}

// IAnimationSerializable

void CConstraintPoint::Serialize(Serialization::IArchive& ar)
{
	ar(m_bDraw, "draw", "^Draw");

	PoseModifier::Serialize(ar, m_desc);

	if (ar.isInput())
		m_bInitialized = false;
}

/*
   CConstraintLine
 */

class CConstraintLine :
	public IAnimationPoseModifier,
	public IAnimationSerializable
{
public:
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimationPoseModifier)
	CRYINTERFACE_ADD(IAnimationSerializable)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CConstraintLine, "AnimationPoseModifier_ConstraintLine", "705fd8b7-906f-42d2-b7d6-d5dee73d3c64"_cry_guid)

	CConstraintLine();
	virtual ~CConstraintLine() {}

private:
	void Draw(const Vec3& point1, const Vec3& point2, const Vec3& target) const;

	// IAnimationPoseModifier
public:
	virtual bool Prepare(const SAnimationPoseModifierParams& params) override;
	virtual bool Execute(const SAnimationPoseModifierParams& params) override;
	virtual void Synchronize() override                           {}

	void         GetMemoryUsage(ICrySizer* pSizer) const override {}

	// IAnimationSerializable
public:
	virtual void Serialize(Serialization::IArchive& ar) override;

private:
	SConstraintLineDesc m_desc;
	uint                m_nodeIndex;
	uint                m_startPointNodeIndex;
	uint                m_endPointNodeIndex;
	uint                m_weightNodeIndex;
	bool                m_bInitialized;

	bool                m_bDraw;
};

CRYREGISTER_CLASS(CConstraintLine)

//

CConstraintLine::CConstraintLine() :
	m_nodeIndex(-1),
	m_startPointNodeIndex(-1),
	m_endPointNodeIndex(-1),
	m_weightNodeIndex(-1),
	m_bInitialized(false),
	m_bDraw(false)
{
}

//

void CConstraintLine::Draw(const Vec3& startPoint, const Vec3& endPoint, const Vec3& target) const
{
	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	if (!pAuxGeom)
		return;

	SAuxGeomRenderFlags flags = pAuxGeom->GetRenderFlags();
	flags.SetDepthTestFlag(e_DepthTestOff);
	flags.SetAlphaBlendMode(e_AlphaBlended);
	pAuxGeom->SetRenderFlags(flags);

	pAuxGeom->DrawSphere(startPoint, 0.01f, ColorB(0xff, 0x80, 0x80, 0x80));
	IRenderAuxText::DrawLabel(startPoint, 1.0f, "Start");

	pAuxGeom->DrawSphere(endPoint, 0.01f, ColorB(0x80, 0xff, 0x80, 0x80));
	IRenderAuxText::DrawLabel(endPoint, 1.0f, "End");

	pAuxGeom->DrawLine(
	  startPoint, ColorB(0xff, 0x80, 0x80, 0xff),
	  endPoint, ColorB(0x80, 0xff, 0x80, 0xff));

	pAuxGeom->DrawSphere(target, 0.01f, ColorB(0xff, 0xff, 0xff, 0x80));
}

// IAnimationPoseModifier

bool CConstraintLine::Prepare(const SAnimationPoseModifierParams& params)
{
	if (m_bInitialized)
		return true;

	m_nodeIndex = m_desc.node.ResolveJointIndex(PoseModifierHelper::GetDefaultSkeleton(params));
	m_startPointNodeIndex = m_desc.startPoint.node.ResolveJointIndex(PoseModifierHelper::GetDefaultSkeleton(params));
	m_endPointNodeIndex = m_desc.endPoint.node.ResolveJointIndex(PoseModifierHelper::GetDefaultSkeleton(params));
	m_weightNodeIndex = m_desc.weightNode.ResolveJointIndex(PoseModifierHelper::GetDefaultSkeleton(params));

	m_bInitialized = true;
	return true;
}

bool CConstraintLine::Execute(const SAnimationPoseModifierParams& params)
{
	Skeleton::CPoseData* pPoseData = Skeleton::CPoseData::GetPoseData(params.pPoseData);
	if (!pPoseData)
		return false;

	const CDefaultSkeleton& defaultSkeleton = PoseModifierHelper::GetDefaultSkeleton(params);

	if (m_nodeIndex >= pPoseData->GetJointCount() ||
	    m_startPointNodeIndex >= pPoseData->GetJointCount() ||
	    m_endPointNodeIndex >= pPoseData->GetJointCount())
	{
		return false;
	}

	float weight = m_desc.weight;
	if (m_weightNodeIndex != uint(-1))
	{
		weight *= clamp_tpl(pPoseData->GetJointRelative(m_weightNodeIndex).t.x, 0.0f, 1.0f);
	}

	const QuatT& nodeAbsolute = pPoseData->GetJointAbsolute(m_nodeIndex);

	const QuatT& startPointAbsolute = pPoseData->GetJointAbsolute(m_startPointNodeIndex);
	const Vec3 startPoint = startPointAbsolute.t + startPointAbsolute.q * m_desc.startPoint.localOffset + m_desc.startPoint.worldOffset;
	const QuatT& endPointAbsolute = pPoseData->GetJointAbsolute(m_endPointNodeIndex);
	const Vec3 endPoint = endPointAbsolute.t + endPointAbsolute.q * m_desc.endPoint.localOffset + m_desc.endPoint.worldOffset;

	const Vec3 nodeTargetPoint = Vec3::CreateLerp(startPoint, endPoint, weight);

	PoseModifierHelper::SetJointAbsolutePosition(defaultSkeleton, *pPoseData, m_nodeIndex, nodeTargetPoint);

	if (m_bDraw)
		Draw(params.location * startPoint, params.location * endPoint, params.location * nodeTargetPoint);
	return true;
}

// IAnimationSerializable

void CConstraintLine::Serialize(Serialization::IArchive& ar)
{
	ar(m_bDraw, "draw", "^Draw");

	PoseModifier::Serialize(ar, m_desc);

	if (ar.isInput())
		m_bInitialized = false;
}

/*
   CConstraintAim
 */

class CConstraintAim :
	public IAnimationPoseModifier,
	public IAnimationSerializable
{
public:
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimationPoseModifier)
	CRYINTERFACE_ADD(IAnimationSerializable)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CConstraintAim, "AnimationPoseModifier_ConstraintAim", "9d07deeb-5408-413d-ad47-1fabc571f964"_cry_guid)

	CConstraintAim();
	virtual ~CConstraintAim() {}

private:
	void Draw(const Vec3& origin, const Vec3& target, const Vec3& up);

	// IAnimationPoseModifier
public:
	virtual bool Prepare(const SAnimationPoseModifierParams& params) override;
	virtual bool Execute(const SAnimationPoseModifierParams& params) override;
	virtual void Synchronize() override                           {}

	void         GetMemoryUsage(ICrySizer* pSizer) const override {}

	// IAnimationSerializable
public:
	virtual void Serialize(Serialization::IArchive& ar) override;

private:
	SConstraintAimDesc m_desc;
	uint               m_nodeIndex;
	uint               m_targetNodeIndex;
	uint               m_upNodeIndex;
	uint               m_weightNodeIndex;
	Quat               m_frame;
	bool               m_bInitialized;

	bool               m_bDraw;
};

CRYREGISTER_CLASS(CConstraintAim)

//

CConstraintAim::CConstraintAim() :
	m_nodeIndex(-1),
	m_targetNodeIndex(-1),
	m_upNodeIndex(-1),
	m_weightNodeIndex(-1),
	m_frame(IDENTITY),
	m_bInitialized(false),
	m_bDraw(false)
{
}

//

void CConstraintAim::Draw(const Vec3& origin, const Vec3& target, const Vec3& up)
{
	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	if (!pAuxGeom)
		return;

	SAuxGeomRenderFlags flags = pAuxGeom->GetRenderFlags();
	flags.SetDepthTestFlag(e_DepthTestOff);
	flags.SetAlphaBlendMode(e_AlphaBlended);
	pAuxGeom->SetRenderFlags(flags);

	pAuxGeom->DrawSphere(origin, 0.01f, ColorB(0xff, 0xff, 0xff, 0x80));

	pAuxGeom->DrawSphere(target, 0.01f, ColorB(0xff, 0x80, 0x80, 0x80));
	IRenderAuxText::DrawLabel(target, 1.0f, "Target");

	pAuxGeom->DrawSphere(up, 0.01f, ColorB(0x80, 0xff, 0x80, 0x80));
	IRenderAuxText::DrawLabel(up, 1.0f, "Up");

	pAuxGeom->DrawLine(
	  origin, ColorB(0xff, 0x00, 0x00, 0xff),
	  target, ColorB(0xff, 0x80, 0x80, 0xff));

	pAuxGeom->DrawLine(
	  origin, ColorB(0x00, 0xff, 0x00, 0xff),
	  up, ColorB(0x80, 0xff, 0x80, 0xff));
}

// IAnimationPoseModifier

bool CConstraintAim::Prepare(const SAnimationPoseModifierParams& params)
{
	if (m_bInitialized)
		return true;

	m_nodeIndex = m_desc.node.ResolveJointIndex(PoseModifierHelper::GetDefaultSkeleton(params));
	m_targetNodeIndex = m_desc.targetNode.ResolveJointIndex(PoseModifierHelper::GetDefaultSkeleton(params));
	m_upNodeIndex = m_desc.upNode.ResolveJointIndex(PoseModifierHelper::GetDefaultSkeleton(params));
	m_weightNodeIndex = m_desc.weightNode.ResolveJointIndex(PoseModifierHelper::GetDefaultSkeleton(params));

	m_bInitialized = true;
	return true;
}

bool CConstraintAim::Execute(const SAnimationPoseModifierParams& params)
{
	if (!m_bInitialized)
		return false;

	Skeleton::CPoseData* pPoseData = Skeleton::CPoseData::GetPoseData(params.pPoseData);
	if (!pPoseData)
		return false;

	const CDefaultSkeleton& defaultSkeleton = PoseModifierHelper::GetDefaultSkeleton(params);

	if (m_nodeIndex >= pPoseData->GetJointCount() ||
	    m_targetNodeIndex >= pPoseData->GetJointCount() ||
	    m_upNodeIndex >= pPoseData->GetJointCount())
	{
		return false;
	}

	const QuatT& originAbsolute = pPoseData->GetJointAbsolute(m_nodeIndex);
	const QuatT& targetAbsolute = pPoseData->GetJointAbsolute(m_targetNodeIndex);
	const QuatT& upAbsolute = pPoseData->GetJointAbsolute(m_upNodeIndex);

	const Vec3 origin = originAbsolute.t;
	const Vec3 target = targetAbsolute.t + targetAbsolute.q * m_desc.targetOffset;
	const Vec3 up = upAbsolute.t + upAbsolute.q * m_desc.upOffset;

	const Vec3 xVector = (target - origin).GetNormalizedSafe(Vec3(1.0f, 0.0f, 0.0f));
	Vec3 yVector = (up - origin).GetNormalizedSafe(Vec3(0.0f, 1.0f, 0.0f));
	if (fabsf(xVector * yVector) > 0.9999f)
		yVector = yVector.GetOrthogonal().normalize();
	Vec3 zVector = xVector.Cross(yVector).normalize();
	yVector = zVector.Cross(xVector).normalize();
	Quat aimOrientation = Quat(Matrix33(xVector, yVector, zVector));
	Quat orientation = aimOrientation * !m_frame;

	float weight = m_desc.weight;
	if (m_weightNodeIndex != uint(-1))
	{
		weight *= clamp_tpl(pPoseData->GetJointRelative(m_weightNodeIndex).t.x, 0.0f, 1.0f);
	}

	orientation = Quat::CreateNlerp(originAbsolute.q, orientation, weight);
	PoseModifierHelper::SetJointAbsoluteOrientation(defaultSkeleton, *pPoseData, m_nodeIndex, orientation);

	if (m_bDraw)
		Draw(params.location * origin, params.location * target, params.location * up);
	return true;
}

// IAnimationSerializable

void CConstraintAim::Serialize(Serialization::IArchive& ar)
{
	//	if (ar.isEdit())
	{
		ar(m_bDraw, "draw", "^Draw");
	}

	PoseModifier::Serialize(ar, m_desc);

	if (ar.isInput())
	{
		m_bInitialized = false;

		const Vec3 aimVector = m_desc.aimVector.GetNormalizedSafe(Vec3(1.0f, 0.0f, 0.0f));
		Vec3 upVector = m_desc.upVector.GetNormalizedSafe(Vec3(0.0f, 1.0f, 0.0f));
		if (fabsf(aimVector * upVector) > 0.9999f)
			upVector = aimVector.GetOrthogonal().normalize();
		Vec3 sideVector = aimVector.Cross(upVector).normalize();
		upVector = sideVector.Cross(aimVector).normalize();
		m_frame = Quat(Matrix33(aimVector, upVector, sideVector));
	}
}

/*
   CDrivenTwist
 */

class CDrivenTwist :
	public IAnimationPoseModifier,
	public IAnimationSerializable
{
public:
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimationPoseModifier)
	CRYINTERFACE_ADD(IAnimationSerializable)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CDrivenTwist, "AnimationPoseModifier_DrivenTwist", "4d9ef006-1e06-4b8d-b1a6-d24fd84c599b"_cry_guid)

	CDrivenTwist();
	virtual ~CDrivenTwist() {}

	// IAnimationPoseModifier
public:
	virtual bool Prepare(const SAnimationPoseModifierParams& params) override;
	virtual bool Execute(const SAnimationPoseModifierParams& params) override;
	virtual void Synchronize() override                           {}

	void         GetMemoryUsage(ICrySizer* pSizer) const override {}

	// IAnimationSerializable
public:
	virtual void Serialize(Serialization::IArchive& ar) override;

private:
	SDrivenTwistDesc m_desc;
	uint             m_sourceNodeIndex;
	uint             m_targetNodeIndex;
	bool             m_bInitialized;
};

CRYREGISTER_CLASS(CDrivenTwist)

//

CDrivenTwist::CDrivenTwist() :
	m_sourceNodeIndex(-1),
	m_targetNodeIndex(-1),
	m_bInitialized(false)
{
}

// IAnimationPoseModifier

bool CDrivenTwist::Prepare(const SAnimationPoseModifierParams& params)
{
	if (m_bInitialized)
		return true;

	m_sourceNodeIndex = m_desc.sourceNode.ResolveJointIndex(PoseModifierHelper::GetDefaultSkeleton(params));
	m_targetNodeIndex = m_desc.targetNode.ResolveJointIndex(PoseModifierHelper::GetDefaultSkeleton(params));

	m_bInitialized = true;
	return true;
}

bool CDrivenTwist::Execute(const SAnimationPoseModifierParams& params)
{
	Skeleton::CPoseData* pPoseData = Skeleton::CPoseData::GetPoseData(params.pPoseData);
	if (!pPoseData)
		return false;

	const CDefaultSkeleton& defaultSkeleton = PoseModifierHelper::GetDefaultSkeleton(params);
	const Skeleton::CPoseData& defaultPoseData = defaultSkeleton.GetPoseData();

	if (m_sourceNodeIndex >= pPoseData->GetJointCount() ||
	    m_targetNodeIndex >= pPoseData->GetJointCount())
		return false;

	const Quat& sourceAbsolute = pPoseData->GetJointAbsolute(m_sourceNodeIndex).q;
	const Quat& sourceAbsoluteDefault = defaultPoseData.GetJointAbsolute(m_sourceNodeIndex).q;

	const Quat& targetRelativeDefault = defaultPoseData.GetJointRelative(m_targetNodeIndex).q;
	const Quat& targetAbsoluteDefault = defaultPoseData.GetJointAbsolute(m_targetNodeIndex).q;
	const Quat& targetParentAbsolute = PoseModifierHelper::GetJointParentAbsoluteSafe(defaultSkeleton, *pPoseData, m_targetNodeIndex).q;

	const Quat& targetAbsoluteDefaultInCurrentParent = targetParentAbsolute * targetRelativeDefault;
	const Quat& targetInSourceRelativeDefault = !sourceAbsoluteDefault * targetAbsoluteDefault;
	const Quat& targetInSourceAbsolute = sourceAbsolute * targetInSourceRelativeDefault;

	const Quat aim = RotationVectorToVector(targetInSourceAbsolute * m_desc.targetVector, targetAbsoluteDefaultInCurrentParent * m_desc.targetVector);
	const Quat result = Quat::CreateNlerp(targetAbsoluteDefaultInCurrentParent, aim * targetInSourceAbsolute, m_desc.weight);

	PoseModifierHelper::SetJointAbsoluteOrientation(defaultSkeleton, *pPoseData, m_targetNodeIndex, result);
	return true;
}

// IAnimationSerializable

void CDrivenTwist::Serialize(Serialization::IArchive& ar)
{
	PoseModifier::Serialize(ar, m_desc);

	if (ar.isInput())
		m_bInitialized = false;
}

/*
   CIk2Segments
 */

class CIk2Segments :
	public IAnimationPoseModifier,
	public IAnimationSerializable
{
public:
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimationPoseModifier)
	CRYINTERFACE_ADD(IAnimationSerializable)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CIk2Segments, "AnimationPoseModifier_Ik2Segments", "6a078d00-c194-41eb-b891-9c52d094076d"_cry_guid);

	CIk2Segments();
	virtual ~CIk2Segments() {}

private:
	void Draw(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& target0, const Vec3& target1, const float targetWeight);

	// IAnimationPoseModifier
public:
	virtual bool Prepare(const SAnimationPoseModifierParams& params) override;
	virtual bool Execute(const SAnimationPoseModifierParams& params) override;
	virtual void Synchronize() override                           {}

	void         GetMemoryUsage(ICrySizer* pSizer) const override {}

	// IAnimationSerializable
public:
	virtual void Serialize(Serialization::IArchive& ar) override;

private:
	SIk2SegmentsDesc m_desc;
	uint             m_rootNodeIndex;
	uint             m_linkNodeIndex;
	uint             m_endNodeIndex;
	uint             m_targetNodeIndex;
	uint             m_targetWeightNodeIndex;
	bool             m_bInitialized;

	bool             m_bDraw;
};

CRYREGISTER_CLASS(CIk2Segments)

//

CIk2Segments::CIk2Segments() :
	m_rootNodeIndex(-1),
	m_linkNodeIndex(-1),
	m_endNodeIndex(-1),
	m_targetNodeIndex(-1),
	m_targetWeightNodeIndex(-1),
	m_bInitialized(false),
	m_bDraw(false)
{
}

//

void CIk2Segments::Draw(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& target0, const Vec3& target1, const float targetWeight)
{
	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	if (!pAuxGeom)
		return;

	SAuxGeomRenderFlags flags = pAuxGeom->GetRenderFlags();
	flags.SetDepthTestFlag(e_DepthTestOff);
	flags.SetAlphaBlendMode(e_AlphaBlended);
	pAuxGeom->SetRenderFlags(flags);

	pAuxGeom->DrawSphere(p0, 0.01f, ColorB(0xff, 0xff, 0xff, 0x80));
	IRenderAuxText::DrawLabel(p0, 1.0f, "p0");

	pAuxGeom->DrawSphere(p1, 0.01f, ColorB(0xff, 0xff, 0xff, 0x80));
	IRenderAuxText::DrawLabel(p1, 1.0f, "p1");

	pAuxGeom->DrawSphere(p2, 0.01f, ColorB(0xff, 0xff, 0xff, 0x80));
	IRenderAuxText::DrawLabel(p2, 1.0f, "p2");

	pAuxGeom->DrawLine(
	  p0, ColorB(0xff, 0xff, 0xff, 0xff),
	  p1, ColorB(0xff, 0xff, 0xff, 0xff));
	pAuxGeom->DrawLine(
	  p1, ColorB(0xff, 0xff, 0xff, 0xff),
	  p2, ColorB(0xff, 0xff, 0xff, 0xff));
	pAuxGeom->DrawLine(
	  p2, ColorB(0xff, 0xff, 0xff, 0xff),
	  p0, ColorB(0xff, 0xff, 0xff, 0xff));

	pAuxGeom->DrawTriangle(
	  p0, ColorB(0xff, 0xff, 0xff, 0x80),
	  p1, ColorB(0xff, 0xff, 0xff, 0x80),
	  p2, ColorB(0xff, 0xff, 0xff, 0x80));
	pAuxGeom->DrawTriangle(
	  p2, ColorB(0xff, 0xff, 0xff, 0x80),
	  p1, ColorB(0xff, 0xff, 0xff, 0x80),
	  p0, ColorB(0xff, 0xff, 0xff, 0x80));

	pAuxGeom->DrawLine(
	  target0, ColorB(0x80, 0xff, 0x80, 0xff),
	  target1, ColorB(0xff, 0x80, 0x80, 0xff));

	pAuxGeom->DrawSphere(target0, 0.01f, ColorB(0x80, 0xff, 0x80, 0x80));
	IRenderAuxText::DrawLabel(target0, 1.0f, "target0");
	pAuxGeom->DrawSphere(target1, 0.01f, ColorB(0xff, 0x80, 0x80, 0x80));
	IRenderAuxText::DrawLabel(target1, 1.0f, "target1");

	Vec3 target = Vec3::CreateLerp(target0, target1, targetWeight);
	pAuxGeom->DrawSphere(target, 0.01f, ColorB(0xff, 0x80, 0x80, 0x80));
	IRenderAuxText::DrawLabel(target, 1.0f, "target");
}

// IAnimationPoseModifier

bool CIk2Segments::Prepare(const SAnimationPoseModifierParams& params)
{
	if (m_bInitialized)
		return true;

	m_rootNodeIndex = m_desc.rootNode.ResolveJointIndex(PoseModifierHelper::GetDefaultSkeleton(params));
	m_linkNodeIndex = m_desc.linkNode.ResolveJointIndex(PoseModifierHelper::GetDefaultSkeleton(params));
	m_endNodeIndex = m_desc.endNode.ResolveJointIndex(PoseModifierHelper::GetDefaultSkeleton(params));
	m_targetNodeIndex = uint(-1);
	if (m_desc.targetNode.IsSet())
		m_targetNodeIndex = m_desc.targetNode.ResolveJointIndex(PoseModifierHelper::GetDefaultSkeleton(params));
	m_targetWeightNodeIndex = uint(-1);
	if (m_desc.targetWeightNode.IsSet())
		m_targetWeightNodeIndex = m_desc.targetWeightNode.ResolveJointIndex(PoseModifierHelper::GetDefaultSkeleton(params));

	m_bInitialized = true;
	return true;
}

bool CIk2Segments::Execute(const SAnimationPoseModifierParams& params)
{
	if (!m_bInitialized)
		return false;

	Skeleton::CPoseData* pPoseData = Skeleton::CPoseData::GetPoseData(params.pPoseData);
	if (!pPoseData)
		return false;

	const CDefaultSkeleton& defaultSkeleton = PoseModifierHelper::GetDefaultSkeleton(params);

	if (m_rootNodeIndex >= pPoseData->GetJointCount() ||
	    m_linkNodeIndex >= pPoseData->GetJointCount() ||
	    m_endNodeIndex >= pPoseData->GetJointCount())
		return false;

	const QuatT& rootAbsolute = pPoseData->GetJointAbsolute(m_rootNodeIndex);
	const QuatT& linkAbsolute = pPoseData->GetJointAbsolute(m_linkNodeIndex);
	const QuatT& endAbsolute = pPoseData->GetJointAbsolute(m_endNodeIndex);
	const QuatT& endAbsoluteParent = pPoseData->GetJointAbsolute(m_endNodeIndex);

	const Vec3& p0 = rootAbsolute.t;
	const Vec3& p1 = linkAbsolute.t;
	const Vec3 p2 = endAbsolute.t + endAbsoluteParent.q * m_desc.endOffset;

	Vec3 targetOriginal = m_desc.targetOffset;
	if (m_targetNodeIndex != uint(-1))
	{
		const QuatT& targetAbsoluteParent = pPoseData->GetJointAbsolute(m_targetNodeIndex);
		targetOriginal = pPoseData->GetJointAbsolute(m_targetNodeIndex).t + targetAbsoluteParent.q * m_desc.targetOffset;
	}

	float targetWeight = m_desc.targetWeight;
	if (m_targetWeightNodeIndex != uint(-1))
	{
		targetWeight *= clamp_tpl(pPoseData->GetJointRelative(m_targetWeightNodeIndex).t.x, 0.0f, 1.0f);
	}

	const Vec3 target = Vec3::CreateLerp(p2, targetOriginal, targetWeight);

	Quat rotationToDistance, rotationToDirection;
	if (PoseModifierHelper::ComputeRotationsForIk2Segments(
	      p0, p1, p2, target, rotationToDistance, rotationToDirection))
	{
		PoseModifierHelper::SetJointAbsoluteOrientation(defaultSkeleton, *pPoseData, m_linkNodeIndex, rotationToDistance * linkAbsolute.q);
		PoseModifierHelper::SetJointAbsoluteOrientation(defaultSkeleton, *pPoseData, m_rootNodeIndex, rotationToDirection * rootAbsolute.q);
	}

	if (m_bDraw)
	{
		Draw(
		  params.location * rootAbsolute.t,
		  params.location * linkAbsolute.t,
		  params.location * (endAbsolute.t + endAbsoluteParent.q * m_desc.endOffset),
		  params.location * p2,
		  params.location * targetOriginal,
		  targetWeight);
	}
	return true;
}

// IAnimationSerializable

void CIk2Segments::Serialize(Serialization::IArchive& ar)
{
	//	if (ar.isEdit())
	{
		ar(m_bDraw, "draw", "^Draw");
	}

	PoseModifier::Serialize(ar, m_desc);

	if (ar.isInput())
		m_bInitialized = false;
}

/*
   CIkCCD
 */

class CIkCCD :
	public IAnimationPoseModifier,
	public IAnimationSerializable
{
public:
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimationPoseModifier)
	CRYINTERFACE_ADD(IAnimationSerializable)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CIkCCD, "AnimationPoseModifier_IkCcd", "6a078d00-c194-41e2-b891-9c52d094076d"_cry_guid);

	CIkCCD();
	virtual ~CIkCCD() {}

private:
	void Draw(const QuatT& location, const QuatT& startAbsolute, const QuatT& targetAbsolute, const float weight);

	// IAnimationPoseModifier
public:
	virtual bool Prepare(const SAnimationPoseModifierParams& params) override;
	virtual bool Execute(const SAnimationPoseModifierParams& params) override;
	virtual void Synchronize() override                           {}

	void         GetMemoryUsage(ICrySizer* pSizer) const override {}

	// IAnimationSerializable
public:
	virtual void Serialize(Serialization::IArchive& ar) override;

private:
	SIkCcdDesc       m_desc;
	uint             m_rootNodeIndex;
	uint             m_endNodeIndex;
	uint             m_targetNodeIndex;
	uint             m_weightNodeIndex;
	uint             m_nIterations;
	float            m_fStepSize;
	float            m_fThreshold;

	DynArray<uint32> m_arrJointChain;

	bool             m_bInitialized;
	bool             m_bDraw;
};

CRYREGISTER_CLASS(CIkCCD)

//

CIkCCD::CIkCCD() :
	m_rootNodeIndex(-1),
	m_endNodeIndex(-1),
	m_targetNodeIndex(-1),
	m_weightNodeIndex(-1),
	m_bInitialized(false),
	m_bDraw(false)
{
}

void CIkCCD::Draw(const QuatT& location, const QuatT& startAbsolute, const QuatT& targetAbsolute, const float weight)
{
	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	if (!pAuxGeom)
		return;

	SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags | e_DepthTestOff | e_DrawInFrontOn);
	pAuxGeom->SetRenderFlags(renderFlags);

	OBB obb;
	obb.m33 = Matrix33(IDENTITY);
	obb.c = Vec3(0);
	obb.h = Vec3(0.01f);

	pAuxGeom->DrawOBB(obb, Matrix34(location * startAbsolute), false, ColorB(0x80, 0x80, 0xff, 0x85), eBBD_Faceted);
	pAuxGeom->DrawOBB(obb, Matrix34(location * targetAbsolute), false, ColorB(0xff, 0x80, 0x80, 0x85), eBBD_Faceted);
	ColorF pCol(0, 0.7f, 0);
	Ang3 angles = Ang3(targetAbsolute.q);
	pAuxGeom->Draw2dLabel(10, g_YLine+=12, 1.2f, pCol, false, "IkCCD %s -> %s:\t%d   Pos: %f  %f  %f   Rot: %f  %f  %f", m_desc.rootNode.name.c_str(), m_desc.endNode.name.c_str(), (uint8)(weight*100.0f), targetAbsolute.t.x, targetAbsolute.t.y, targetAbsolute.t.z, RAD2DEG(angles.x), RAD2DEG(angles.y), RAD2DEG(angles.z));
	pAuxGeom->Submit();
}

// IAnimationPoseModifier

bool CIkCCD::Prepare(const SAnimationPoseModifierParams& params)
{
	if (m_bInitialized)
		return true;

	const CDefaultSkeleton& defaultSkeleton = PoseModifierHelper::GetDefaultSkeleton(params);

	m_rootNodeIndex = m_desc.rootNode.ResolveJointIndex(defaultSkeleton);
	m_endNodeIndex = m_desc.endNode.ResolveJointIndex(defaultSkeleton);
	m_targetNodeIndex = uint(-1);
	if (m_desc.targetNode.IsSet())
		m_targetNodeIndex = m_desc.targetNode.ResolveJointIndex(defaultSkeleton);
	m_weightNodeIndex = uint(-1);
	if (m_desc.weightNode.IsSet())
		m_weightNodeIndex = m_desc.weightNode.ResolveJointIndex(defaultSkeleton);

	m_nIterations = m_desc.iterations;
	m_fStepSize = m_desc.stepSize;
	m_fThreshold = m_desc.threshold;
	m_arrJointChain.clear();

	if (m_rootNodeIndex != uint(-1) && m_endNodeIndex != uint(-1))
	{
		m_arrJointChain.push_back(m_endNodeIndex);
		uint32 joint = m_endNodeIndex;
		bool finishedInit = false;
		while (joint > 0 && !finishedInit)
		{
			uint32 parent = defaultSkeleton.GetJointParentIDByID(joint);
			m_arrJointChain.push_back(parent);

			if (joint == m_rootNodeIndex)
				finishedInit = true;

			joint = parent;
		}

		if (!finishedInit)
			m_arrJointChain.clear();
		else
		{
			if (uint32 size = m_arrJointChain.size())
			{
				std::reverse(m_arrJointChain.begin(), m_arrJointChain.end());
				m_bInitialized = true;
			}
		}
	}

	return true;
}

bool CIkCCD::Execute(const SAnimationPoseModifierParams& params)
{
	if (!m_bInitialized)
		return false;

	Skeleton::CPoseData* pPoseData = Skeleton::CPoseData::GetPoseData(params.pPoseData);
	if (!pPoseData)
		return false;

	const CDefaultSkeleton& defaultSkeleton = PoseModifierHelper::GetDefaultSkeleton(params);
	uint32 numLinks = m_arrJointChain.size();
	uint32 numJoints = pPoseData->GetJointCount();

	if (m_rootNodeIndex == uint(-1) || m_rootNodeIndex >= numJoints ||
	    m_endNodeIndex == uint(-1) || m_endNodeIndex >= numJoints ||
	    !numLinks)
		return false;

	QuatT* const __restrict pRelPose = pPoseData->GetJointsRelative();
	QuatT* const __restrict pAbsPose = pPoseData->GetJointsAbsolute();

	f32 fTransCompensation = 0;
	for (uint32 i = 2; i < numLinks; i++)
	{
		int32 p = m_arrJointChain[i - 1];
		int32 c = m_arrJointChain[i];
		fTransCompensation += (pAbsPose[c].t - pAbsPose[p].t).GetLengthFast();
	}

	Vec3 vTarget = m_desc.targetOffset;
	if (m_targetNodeIndex < pPoseData->GetJointCount())
		vTarget = pAbsPose[m_targetNodeIndex].t + pAbsPose[m_targetNodeIndex].q * m_desc.targetOffset;

	float weight = m_desc.weight;
	if (m_weightNodeIndex < pPoseData->GetJointCount())
		weight *= clamp_tpl(pPoseData->GetJointRelative(m_weightNodeIndex).t.x, 0.0f, 1.0f);

	f32 inumLinks = 1.0f / f32(numLinks);
	int32 nRootIdx = m_arrJointChain[1];              //Root
	int32 nEndEffIdx = m_arrJointChain[numLinks - 1]; //EndEffector
	ANIM_ASSET_ASSERT(nRootIdx < nEndEffIdx);
	int32 iJointIterator = 1;

	// Cyclic Coordinate Descent
	for (uint32 i = 0; i < m_nIterations; i++)
	{
		Vec3 vecEnd = pAbsPose[nEndEffIdx].t; // Position of end effector
		int32 parentLinkIdx = m_arrJointChain[iJointIterator - 1];
		ANIM_ASSET_ASSERT(parentLinkIdx >= 0);
		int32 LinkIdx = m_arrJointChain[iJointIterator];
		Vec3 vecLink = pAbsPose[LinkIdx].t;                       // Position of current node
		Vec3 vecLinkTarget = (vTarget - vecLink).GetNormalized(); // Vector current link -> target
		Vec3 vecLinkEnd = (vecEnd - vecLink).GetNormalized();     // Vector current link -> current effector position

		Quat qrel;
		qrel.SetRotationV0V1(vecLinkEnd, vecLinkTarget);
		ANIM_ASSET_ASSERT((fabs_tpl(1 - (qrel | qrel))) < 0.001); //check if unit-quaternion
		if (qrel.w < 0)
			qrel.w = -qrel.w;

		f32 ji = iJointIterator * inumLinks + 0.3f;
		f32 t = min(m_fStepSize * ji, 0.4f);
		qrel.w *= t + 1.0f - t;
		qrel.v *= t;
		qrel.SetNlerp(Quat(IDENTITY), qrel, weight);
		qrel.NormalizeSafe();

		//calculate new relative IK-orientation
		pRelPose[LinkIdx].q = !pAbsPose[parentLinkIdx].q * qrel * pAbsPose[LinkIdx].q;
		pRelPose[LinkIdx].q.NormalizeSafe();

		//calculate new absolute IK-orientation
		for (uint32 j = iJointIterator; j < numLinks; j++)
		{
			int32 c = m_arrJointChain[j];
			int32 p = m_arrJointChain[j - 1];
			assert(p >= 0);
			ANIM_ASSET_ASSERT(pRelPose[c].q.IsUnit());
			ANIM_ASSET_ASSERT(pAbsPose[p].q.IsUnit());
			pAbsPose[c] = pAbsPose[p] * pRelPose[c];
			ANIM_ASSET_ASSERT(pAbsPose[c].q.IsUnit());
		}

		f32 fError = (pAbsPose[nEndEffIdx].t - vTarget).GetLength();

		if (fError < m_fThreshold)
			break;
		iJointIterator++; //Next link
		if (iJointIterator > (int)(numLinks) - 3)
			iJointIterator = 1;
	}

	//-----------------------------------------------------------------------------
	//--do a cheap translation compensation to fix the error
	//-----------------------------------------------------------------------------
	Vec3 absEndEffector = pAbsPose[nEndEffIdx].t; // Position of end effector
	Vec3 vDistance = (vTarget - absEndEffector);
	f32 fDistance = vDistance.GetLengthFast();
	if (fDistance > fTransCompensation)
		vDistance *= fTransCompensation / fDistance;
	Vec3 bPartDistance = vDistance / f32(numLinks - 0);
	Vec3 vAddDistance = bPartDistance;

	for (uint32 i = 1; i < numLinks; i++)
	{
		int c = m_arrJointChain[i];
		int p = m_arrJointChain[i - 1];
		assert(p >= 0);
		pAbsPose[c].t += vAddDistance;
		vAddDistance += bPartDistance;
		ANIM_ASSET_ASSERT(pAbsPose[c].q.IsUnit());
		ANIM_ASSET_ASSERT(pAbsPose[p].q.IsUnit());
		pRelPose[c] = pAbsPose[p].GetInverted() * pAbsPose[c];
		pRelPose[c].q.NormalizeSafe();
	}

	// calculate relative pose from changed joints

	for (uint32 i = m_rootNodeIndex; i < numJoints; i++)
	{
		if (uint32 p = defaultSkeleton.GetJointParentIDByID(i))
		{
			pAbsPose[i] = pAbsPose[p] * pRelPose[i];
			pAbsPose[i].q.NormalizeSafe();
		}
	}

	if (m_bDraw)
		Draw((QuatT)params.location, pAbsPose[m_rootNodeIndex], pAbsPose[m_endNodeIndex], weight);

	return true;
}

// IAnimationSerializable

void CIkCCD::Serialize(Serialization::IArchive& ar)
{
	ar(m_bDraw, "draw", "^Draw");
	PoseModifier::Serialize(ar, m_desc);

	if (ar.isInput())
		m_bInitialized = false;
}

/*
   CDynamicsSpring
 */

class CDynamicsSpring :
	public IAnimationPoseModifier,
	public IAnimationSerializable
{
public:
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimationPoseModifier)
	CRYINTERFACE_ADD(IAnimationSerializable)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CDynamicsSpring, "AnimationPoseModifier_DynamicsSpring", "92e070d5-701b-4f8a-a761-42e967579948"_cry_guid)

	CDynamicsSpring();
	virtual ~CDynamicsSpring() {}

private:
	void Draw(const Vec3& position);

	// IAnimationPoseModifier
public:
	virtual bool Prepare(const SAnimationPoseModifierParams& params) override;
	virtual bool Execute(const SAnimationPoseModifierParams& params) override;
	virtual void Synchronize() override                           {}

	void         GetMemoryUsage(ICrySizer* pSizer) const override {}

	// IAnimationSerializable
public:
	virtual void Serialize(Serialization::IArchive& ar) override;

private:
	SDynamicsSpringDesc m_desc;
	uint                m_nodeIndex;

	Vec3                m_position;
	Vec3                m_velocity;

	bool                m_bInitialized;

	bool                m_bDraw;
};

CRYREGISTER_CLASS(CDynamicsSpring)

//

CDynamicsSpring::CDynamicsSpring() :
	m_nodeIndex(-1),
	m_position(ZERO),
	m_velocity(ZERO),
	m_bInitialized(false),
	m_bDraw(false)
{
}

//

void CDynamicsSpring::Draw(const Vec3& position)
{
	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	if (!pAuxGeom)
		return;

	const float length = max(m_desc.length, 0.1f);
	const Vec3 weightDirection = Vec3(0.0f, length, 0.0f).normalize();

	SAuxGeomRenderFlags flags = gEnv->pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
	flags.SetDepthTestFlag(e_DepthTestOff);
	flags.SetAlphaBlendMode(e_AlphaBlended);
	pAuxGeom->SetRenderFlags(flags);

	pAuxGeom->DrawSphere(position, m_desc.length, ColorB(0xff, 0x80, 0x80, 0x85));
	pAuxGeom->DrawSphere(m_position, m_desc.length < 0.025f * 2.0f ? m_desc.length / 4.0f : 0.025f, ColorB(0xff, 0xff, 0xff, 0xff));
}

// IAnimationPoseModifier

bool CDynamicsSpring::Prepare(const SAnimationPoseModifierParams& params)
{
	if (m_bInitialized)
		return true;

	m_nodeIndex = m_desc.node.ResolveJointIndex(PoseModifierHelper::GetDefaultSkeleton(params));

	m_bInitialized = true;
	return true;
}

bool CDynamicsSpring::Execute(const SAnimationPoseModifierParams& params)
{
	if (!m_bInitialized)
		return false;

	Skeleton::CPoseData* pPoseData = Skeleton::CPoseData::GetPoseData(params.pPoseData);
	if (!pPoseData)
		return false;

	const CDefaultSkeleton& defaultSkeleton = PoseModifierHelper::GetDefaultSkeleton(params);

	if (m_nodeIndex >= pPoseData->GetJointCount())
		return false;

	const float _30hz = 0.0333f;
	const float _1000hz = 0.001f;
	const float timeDelta = clamp_tpl(params.timeDelta, _1000hz, _30hz);

	const QuatT& targetAbsolute = pPoseData->GetJointAbsolute(m_nodeIndex);
	const QuatT& parentAbsolute = PoseModifierHelper::GetJointParentAbsoluteSafe(defaultSkeleton, *pPoseData, m_nodeIndex);
	const Vec3 targetPosition = params.location * (targetAbsolute.t + parentAbsolute.q * m_desc.positionOffset);

	// gravity
	Vec3 acceleration = m_desc.gravity * timeDelta;

	// spring
	acceleration += (targetPosition - m_position) * m_desc.stiffness;
	acceleration -= m_velocity * m_desc.damping;
	m_velocity += acceleration * timeDelta;
	m_position += m_velocity * timeDelta;

	// limit by length
	m_position = m_position - targetPosition;

#if 0
	float length = m_position.len();
	m_position /= length > 0.000001f ? length : 1.0f;
	length = min(length, m_desc.length);
#else
	float lengthPrevious = m_position.GetLength();
	m_position /= lengthPrevious > 0.000001f ? lengthPrevious : 1.0f;
	float length = min(lengthPrevious, m_desc.length);
	m_velocity *= lengthPrevious > length ? (length / lengthPrevious) : 1.0f;
#endif
	m_position = targetPosition + m_position * length;

	// limit by planes

	Quat limitSpace = params.location.q * targetAbsolute.q;
	Vec3 direction = m_position - targetPosition;

	const Vec3 xp = limitSpace * Vec3(1.0f, 0.0f, 0.0f);
	const float xpd = xp * direction;
	const float xpw = xpd > 0.0f ? 0.0f : (m_desc.flags & SDynamicsSpringDesc::eFlag_LimitPlaneXPositive ? 1.0f : 0.0f);
	m_position -= xp * xpd * xpw;
	m_velocity -= xp * (m_velocity * xp) * xpw;

	const Vec3 xn = -xp;
	const float xnd = xn * direction;
	const float xnw = xnd > 0.0f ? 0.0f : (m_desc.flags & SDynamicsSpringDesc::eFlag_LimitPlaneXNegative ? 1.0f : 0.0f);
	m_position -= xn * xnd * xnw;
	m_velocity -= xn * (m_velocity * xn) * xnw;

	const Vec3 yp = limitSpace * Vec3(0.0f, 1.0f, 0.0f);
	const float ypd = yp * direction;
	const float ypw = ypd > 0.0f ? 0.0f : (m_desc.flags & SDynamicsSpringDesc::eFlag_LimitPlaneYPositive ? 1.0f : 0.0f);
	m_position -= yp * ypd * ypw;
	m_velocity -= yp * (m_velocity * yp) * ypw;

	const Vec3 yn = -yp;
	const float ynd = yn * direction;
	const float ynw = ynd > 0.0f ? 0.0f : (m_desc.flags & SDynamicsSpringDesc::eFlag_LimitPlaneYNegative ? 1.0f : 0.0f);
	m_position -= yn * ynd * ynw;
	m_velocity -= yn * (m_velocity * yn) * ynw;

	const Vec3 zp = limitSpace * Vec3(0.0f, 0.0f, 1.0f);
	const float zpd = zp * direction;
	const float zpw = zpd > 0.0f ? 0.0f : (m_desc.flags & SDynamicsSpringDesc::eFlag_LimitPlaneZPositive ? 1.0f : 0.0f);
	m_position -= zp * zpd * zpw;
	m_velocity -= zp * (m_velocity * zp) * zpw;

	const Vec3 zn = -zp;
	const float znd = zn * direction;
	const float znw = znd > 0.0f ? 0.0f : (m_desc.flags & SDynamicsSpringDesc::eFlag_LimitPlaneZNegative ? 1.0f : 0.0f);
	m_position -= zn * znd * znw;
	m_velocity -= zn * (m_velocity * zn) * znw;

	//

	Vec3 p = params.location.GetInverted() * m_position;
	PoseModifierHelper::SetJointAbsolutePosition(defaultSkeleton, *pPoseData, m_nodeIndex, p);

	if (m_bDraw)
		Draw(targetPosition);
	return true;
}

// IAnimationSerializable

void CDynamicsSpring::Serialize(Serialization::IArchive& ar)
{
	//	if (ar.isEdit())
	{
		ar(m_bDraw, "draw", "^Draw");
	}

	PoseModifier::Serialize(ar, m_desc);

	if (ar.isInput())
		m_bInitialized = false;
}

/*
   CDynamicsPendulum
 */

class CDynamicsPendulum :
	public IAnimationPoseModifier,
	public IAnimationSerializable
{
private:
	struct SState
	{
		Vec3 positionAbsolute;
		Vec3 positionGlobal;
		Quat orientationAbsolute;
		Vec3 velocityAbsolute;

		SState() :
			positionAbsolute(ZERO),
			positionGlobal(ZERO),
			orientationAbsolute(IDENTITY),
			velocityAbsolute(ZERO)
		{
		}
	};

	struct SRuntimeDesc
	{
		uint  nodeIndex;
		Vec3  sideVector;
		float limitVectorHalfAngleCos;
		float limitVectorHalfAngleSin;
		Vec3  limitPlaneNormal;
		Quat  limitRotation;

		bool  bInitialized;

		SRuntimeDesc() :
			nodeIndex(-1),
			sideVector(0.0f, 0.0f, 1.0f),
			limitVectorHalfAngleCos(0.0f),
			limitVectorHalfAngleSin(1.0f),
			limitPlaneNormal(1.0f, 0.0f, 0.0f),
			limitRotation(IDENTITY),
			bInitialized(false)
		{
		}

		bool Initialize(const SDynamicsPendulumDesc& desc, const CDefaultSkeleton& skeleton);
	};

public:
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimationPoseModifier)
	CRYINTERFACE_ADD(IAnimationSerializable)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CDynamicsPendulum, "AnimationPoseModifier_DynamicsPendulum", "f6c1b4da-5caf-4b9e-bb97-c28f9f17b003"_cry_guid)

	CDynamicsPendulum();
	virtual ~CDynamicsPendulum() {}

private:
	void ApplyLimit1(const Quat& limitOrientation, const Vec3& targetDirection, Vec3& direction, Vec3& velocity);
	void ApplyLimit2(const Quat& limitOrientation, const Quat& orientation, const Vec3& targetDirection, Vec3& directionNew, Vec3& velocity);

	void DrawCircle(
	  const Vec3& position,
	  const Vec3& direction,
	  const Vec3& up,
	  const float length,
	  const ColorB& color0, const ColorB& color1);
	void DrawCone(
	  const Vec3& position,
	  const Vec3& direction,
	  const float length,
	  const float angle,
	  const ColorB& color0, const ColorB& color1);
	void Draw(const Vec3& position, const Quat& worldOrientation, const Quat& nodeOrientation, const Quat& limitOrientation);

	// IAnimationPoseModifier
public:
	virtual bool Prepare(const SAnimationPoseModifierParams& params) override;
	virtual bool Execute(const SAnimationPoseModifierParams& params) override;
	virtual void Synchronize() override                           {}

	void         GetMemoryUsage(ICrySizer* pSizer) const override {}

	// IAnimationSerializable
public:
	virtual void Serialize(Serialization::IArchive& ar) override;

private:
	SDynamicsPendulumDesc m_desc;
	SRuntimeDesc          m_runtimeDesc;
	SState                m_state;
	bool                  m_bDraw;
};

CRYREGISTER_CLASS(CDynamicsPendulum)

//

bool CDynamicsPendulum::SRuntimeDesc::Initialize(const SDynamicsPendulumDesc& desc, const CDefaultSkeleton& skeleton)
{
	sideVector = desc.aimVector.Cross(desc.upVector);
	sideVector.GetNormalizedSafe(Vec3(0.0f, 0.0f, 1.0f));
	limitVectorHalfAngleCos = fabsf(cosf(DEG2RAD(desc.limitAngle) * 0.5f));
	limitVectorHalfAngleSin = sqrt(fabsf(1.0f - limitVectorHalfAngleCos * limitVectorHalfAngleCos));
	limitPlaneNormal = desc.aimVector.GetOrthogonal();
	limitRotation =
	  Quat::CreateRotationAA(DEG2RAD(desc.limitRotationAngles.x), Vec3(1.0f, 0.0f, 0.0f)) *
	  Quat::CreateRotationAA(DEG2RAD(desc.limitRotationAngles.y), Vec3(0.0f, 1.0f, 0.0f)) *
	  Quat::CreateRotationAA(DEG2RAD(desc.limitRotationAngles.z), Vec3(0.0f, 0.0f, 1.0f));

	nodeIndex = desc.node.ResolveJointIndex(skeleton);

	bInitialized = true;
	return true;
}

//

CDynamicsPendulum::CDynamicsPendulum() :
	m_bDraw(false)
{
}

//

void CDynamicsPendulum::DrawCircle(
  const Vec3& position,
  const Vec3& direction,
  const Vec3& up,
  const float length,
  const ColorB& color0, const ColorB& color1)
{
	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	if (!pAuxGeom)
		return;

	const float angleStart = -DEG2RAD(m_desc.limitAngle);
	const float angle1 = DEG2RAD(m_desc.limitAngle) * 2.0f;

	const uint segments = 64;
	const float angleStep = angle1 / float(segments);

	float angleCurrent = angleStart;
	Vec3 point = position + Quat::CreateRotationAA(angleCurrent, up) * direction * length;
	for (uint i = 0; i < segments; ++i)
	{
		angleCurrent += angleStep;
		Vec3 p = position + Quat::CreateRotationAA(angleCurrent, up) * direction * length;
		pAuxGeom->DrawLine(point, color0, p, color0);
		pAuxGeom->DrawTriangle(position, color0, point, color1, p, color1);
		pAuxGeom->DrawTriangle(position, color0, p, color1, point, color1);
		point = p;
	}
}

void CDynamicsPendulum::DrawCone(
  const Vec3& position,
  const Vec3& direction,
  const float length,
  const float angle,
  const ColorB& color0, const ColorB& color1)
{
	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	if (!pAuxGeom)
		return;

	const float angleStart = -DEG2RAD(0.0f);
	const float angle1 = gf_PI * 2.0f;

	const uint segments = 64;
	const float angleStep = angle1 / float(segments);

	const Vec3 vector = Quat::CreateRotationAA(angle, direction.GetOrthogonal().normalize()) * direction * length;

	float angleCurrent = angleStart;
	Vec3 point = position + Quat::CreateRotationAA(angleCurrent, direction) * vector;
	for (uint i = 0; i < segments; ++i)
	{
		angleCurrent += angleStep;
		Vec3 p = position + Quat::CreateRotationAA(angleCurrent, direction) * vector;
		pAuxGeom->DrawLine(point, color0, p, color0);
		point = p;
	}

	angleCurrent = angleStart;
	point = position + Quat::CreateRotationAA(angleCurrent, direction) * vector;
	for (uint i = 0; i < segments; ++i)
	{
		angleCurrent += angleStep;
		Vec3 p = position + Quat::CreateRotationAA(angleCurrent, direction) * vector;
		pAuxGeom->DrawTriangle(position, color0, p, color1, point, color1);
		point = p;
	}

	angleCurrent = angleStart;
	point = position + Quat::CreateRotationAA(angleCurrent, direction) * vector;
	for (uint i = 0; i < segments; ++i)
	{
		angleCurrent += angleStep;
		Vec3 p = position + Quat::CreateRotationAA(angleCurrent, direction) * vector;
		pAuxGeom->DrawTriangle(position, color0, point, color1, p, color1);
		point = p;
	}
}

void CDynamicsPendulum::Draw(const Vec3& position, const Quat& worldOrientation, const Quat& nodeOrientation, const Quat& limitRotation)
{
	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	if (!pAuxGeom)
		return;

	SAuxGeomRenderFlags flags = pAuxGeom->GetRenderFlags();
	flags.SetDepthTestFlag(e_DepthTestOff);
	flags.SetAlphaBlendMode(e_AlphaBlended);
	pAuxGeom->SetRenderFlags(flags);

	const Vec3 targetPosition = position + worldOrientation * nodeOrientation * m_desc.aimVector * m_desc.length;

	pAuxGeom->DrawLine(
	  position, ColorB(0xff, 0x80, 0x80, 0x80),
	  targetPosition, ColorB(0xff, 0x80, 0x80, 0xff));
	pAuxGeom->DrawSphere(targetPosition, 0.025f, ColorB(0xff, 0x80, 0x80, 0x80));

	pAuxGeom->DrawLine(
	  position, ColorB(0xff, 0xff, 0xff, 0xff),
	  m_state.positionAbsolute, ColorB(0xff, 0xff, 0xff, 0xff));
	pAuxGeom->DrawSphere(m_state.positionAbsolute, 0.025f, ColorB(0xff, 0xff, 0xff, 0xff));

	if (m_desc.bLimitPlane0 | m_desc.bLimitPlane1)
	{
		Quat frame = limitRotation * nodeOrientation;
		DrawCircle(position, worldOrientation * frame * m_desc.aimVector, worldOrientation * frame * m_runtimeDesc.limitPlaneNormal, 0.25f,
		           ColorB(0xff, 0x80, 0x80, 0x80), ColorB(0xff, 0x80, 0x40, 0x40));
	}

	Vec3 limitDirection = worldOrientation * limitRotation * (nodeOrientation * m_desc.aimVector);
	DrawCone(position, limitDirection, 0.25f, DEG2RAD(m_desc.limitAngle),
	         ColorB(0xff, 0x80, 0x80, 0x40), ColorB(0xff, 0x80, 0x40, 0x20));
}

// IAnimationPoseModifier

bool CDynamicsPendulum::Prepare(const SAnimationPoseModifierParams& params)
{
	if (!m_runtimeDesc.bInitialized)
		m_runtimeDesc.Initialize(m_desc, PoseModifierHelper::GetDefaultSkeleton(params));

	return true;
}

void CDynamicsPendulum::ApplyLimit1(const Quat& limitRotation, const Vec3& targetDirection, Vec3& direction, Vec3& velocity)
{
	Vec3 limitDirection = !limitRotation * direction;
	const float motionHalfAngleCos = RotationVectorToVector(targetDirection, limitDirection).w;
	if (motionHalfAngleCos < m_runtimeDesc.limitVectorHalfAngleCos)
	{
		velocity -= targetDirection * max(0.0f, velocity * targetDirection);
		const Vec3 v = (targetDirection % limitDirection).normalize();
		direction = limitRotation * Quat::CreateRotationAA(m_runtimeDesc.limitVectorHalfAngleCos, m_runtimeDesc.limitVectorHalfAngleSin, v) * targetDirection;
	}
}

void CDynamicsPendulum::ApplyLimit2(const Quat& limitRotation, const Quat& orientation, const Vec3& targetDirection, Vec3& directionNew, Vec3& velocity)
{
	Quat frame = limitRotation * orientation;
	Vec3 frameDirection = directionNew * frame;
	Vec3 frameVelocity = velocity * frame;

	{
		const float limit = m_desc.bLimitPlane0 ? 1.0f : 0.0f;
		const Vec3 v = m_runtimeDesc.limitPlaneNormal;
		const float d = frameDirection * v;
		const float w = d > 0.0f ? 0.0f : 1.0f;
		frameDirection -= v * d * w * limit;
		frameVelocity -= v * (frameVelocity * v) * w * limit;
	}

	{
		const float limit = m_desc.bLimitPlane1 ? 1.0f : 0.0f;
		const Vec3 v = -m_runtimeDesc.limitPlaneNormal;
		const float d = frameDirection * v;
		const float w = d > 0.0f ? 0.0f : 1.0f;
		frameDirection -= v * d * w * limit;
		frameVelocity -= v * (frameVelocity * v) * w * limit;
	}

	directionNew = frame * frameDirection;
	velocity = frame * frameVelocity;
}

bool CDynamicsPendulum::Execute(const SAnimationPoseModifierParams& params)
{
	if (!m_runtimeDesc.bInitialized)
		return false;

	Skeleton::CPoseData* pPoseData = Skeleton::CPoseData::GetPoseData(params.pPoseData);
	if (!pPoseData)
		return false;

	const CDefaultSkeleton& defaultSkeleton = PoseModifierHelper::GetDefaultSkeleton(params);

	if (m_runtimeDesc.nodeIndex >= pPoseData->GetJointCount())
		return false;

	const Skeleton::CPoseData& poseDataDefault = defaultSkeleton.m_poseDefaultData;

	const float _30hz = 0.0333f;
	const float _1000hz = 0.001f;
	const float timeDelta = clamp_tpl(params.timeDelta, _1000hz, _30hz);
	float timeElapsed = timeDelta;

	const QuatT& nodeRelative = pPoseData->GetJointRelative(m_runtimeDesc.nodeIndex);
	const QuatT nodeAbsolute = pPoseData->GetJointAbsolute(m_runtimeDesc.nodeIndex);
	const QuatT& nodeAbsoluteParent = PoseModifierHelper::GetJointParentAbsoluteSafe(defaultSkeleton, *pPoseData, m_runtimeDesc.nodeIndex);

	const Quat limitRotationRelative = nodeRelative.q * m_runtimeDesc.limitRotation;
	const Quat limitRotationAbsolute = nodeAbsoluteParent.q * limitRotationRelative;
	const Quat limitRotation = nodeAbsolute.q * !limitRotationAbsolute;

	const float length = max(m_desc.length, 0.1f);
	const Vec3 targetPosition = m_desc.aimVector * length;

	const Vec3 targetDirection = (nodeAbsolute.q * targetPosition).normalize();

	const Vec3 forceLocal = m_state.positionAbsolute - nodeAbsolute.t;
	const Vec3 forceMovement = params.location.GetInverted() * m_state.positionGlobal - m_state.positionAbsolute;
	const Vec3 forceDirection = forceLocal + forceMovement.CompMul(m_desc.forceMovementMultiplier);

	// compute counter force
	//		const Float force = M::Dot(forceDirection, velocity) / M::Dot(forceDirection, forceDirection);
	//		velocity -= forceDirection * force;

	// compute stiffness
	const Vec3 forceDirectionNormalized = forceDirection.normalized();
	const Vec3 moveDirection = (targetDirection % forceDirectionNormalized) % forceDirection;
	const float tension = 1.0f - (targetDirection * forceDirectionNormalized + 1.0f) * 0.5f;
	m_state.velocityAbsolute -= moveDirection *
	                            1.0f / sqrtf(moveDirection * moveDirection + FLT_MIN) *
	                            m_desc.stiffness * m_desc.stiffness * tension * timeElapsed;

	// gravity
	//		velocity += desc.gravity * timeDelta;

	Vec3 directionNew = forceDirection + m_state.velocityAbsolute * timeElapsed;
	ApplyLimit2(limitRotation, nodeAbsolute.q, targetDirection, directionNew, m_state.velocityAbsolute);
	directionNew.normalize();

	float d = targetDirection * directionNew + 1.0f;
	if (d > 0.00001f) // only needed if limit1 had been applied
	{
		ApplyLimit1(limitRotation, targetDirection, directionNew, m_state.velocityAbsolute);
		Quat rotation = RotationVectorToVector(targetDirection, directionNew);

		m_state.velocityAbsolute = (rotation * targetDirection * length - forceDirection) / timeElapsed;
		m_state.positionAbsolute = nodeAbsolute.t + rotation * targetDirection * length;
		m_state.positionGlobal = params.location * m_state.positionAbsolute;
		m_state.orientationAbsolute = rotation * nodeAbsolute.q;
	}
	else
	{
		m_state.velocityAbsolute = (directionNew * length - forceDirection) / timeElapsed;
		m_state.positionAbsolute = nodeAbsolute.t + directionNew * length;
		m_state.positionGlobal = params.location * m_state.positionAbsolute;
	}

	m_state.velocityAbsolute += !params.location.q * m_desc.gravity * timeElapsed;

	// damping
	m_state.velocityAbsolute *= max(0.0f, 1.0f - m_desc.damping * timeElapsed);

	PoseModifierHelper::SetJointAbsoluteOrientation(defaultSkeleton, *pPoseData, m_runtimeDesc.nodeIndex, m_state.orientationAbsolute);

	if (m_bDraw)
		Draw(params.location * nodeAbsolute.t, params.location.q, nodeAbsolute.q, limitRotation);
	return true;
}

// IAnimationSerializable

void CDynamicsPendulum::Serialize(Serialization::IArchive& ar)
{
	//	if (ar.isEdit())
	{
		ar(m_bDraw, "draw", "^Draw");
	}

	PoseModifier::Serialize(ar, m_desc);

	if (ar.isInput())
		m_runtimeDesc.bInitialized = false;
}

/*
   CTransformBlender
 */

class CTransformBlender :
	public IAnimationPoseModifier,
	public IAnimationSerializable
{
private:
	struct STransformBlenderTarget
	{
		uint  nodeIndex;
		uint  weightNodeIndex;

		QuatT transform;

		float defaultWeight;
		float weight;

		STransformBlenderTarget() :
			nodeIndex(-1),
			weightNodeIndex(-1),
			transform(IDENTITY),
			defaultWeight(1.0f),
			weight(1.0f)
		{
		}
	};

public:
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimationPoseModifier)
	CRYINTERFACE_ADD(IAnimationSerializable)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CTransformBlender, "AnimationPoseModifier_TransformBlender", "92e070d5-601b-4f9a-a761-43e967579958"_cry_guid)

	CTransformBlender();
	virtual ~CTransformBlender() {}

private:
	void Draw(const QuatT& targetAbsolute, const QuatT& defaultAbsolute);

	// IAnimationPoseModifier
public:
	virtual bool Prepare(const SAnimationPoseModifierParams& params) override;
	virtual bool Execute(const SAnimationPoseModifierParams& params) override;
	virtual void Synchronize() override                           {}

	void         GetMemoryUsage(ICrySizer* pSizer) const override {}

	// IAnimationSerializable
public:
	virtual void Serialize(Serialization::IArchive& ar) override;

private:
	STransformBlenderDesc                m_desc;

	uint                                 m_nodeIndex;
	uint                                 m_defaultTargetIndex;

	std::vector<STransformBlenderTarget> m_targets;

	bool                                 m_bInitialized;
	bool                                 m_bDraw;
};

CRYREGISTER_CLASS(CTransformBlender)

//

CTransformBlender::CTransformBlender() :
	m_nodeIndex(-1),
	m_defaultTargetIndex(-1),
	m_bInitialized(false),
	m_bDraw(false)
{
}

//

void CTransformBlender::Draw(const QuatT& targetAbsolute, const QuatT& defaultAbsolute)
{
	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	if (!pAuxGeom)
		return;

	SAuxGeomRenderFlags flags = gEnv->pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
	pAuxGeom->SetRenderFlags(e_Mode3D | e_AlphaBlended | e_DrawInFrontOn | e_FillModeSolid | e_CullModeNone | e_DepthWriteOff | e_DepthTestOff);

	OBB obb;
	obb.m33 = Matrix33(IDENTITY);
	obb.c = Vec3(0);
	obb.h = Vec3(0.01f);

	pAuxGeom->DrawOBB(obb, Matrix34(targetAbsolute), false, ColorB(0x80, 0x80, 0xff, 0x85), eBBD_Faceted);
	pAuxGeom->DrawLine(defaultAbsolute.t, ColorB(0x80, 0x80, 0xff, 0x85), targetAbsolute.t, ColorB(0xff, 0x80, 0x80, 0x85));

	for (int i = 0; i < m_targets.size(); i++)
	{
		uint8 alpha = (uint8)(200 * m_targets[i].weight);
		pAuxGeom->DrawOBB(obb, Matrix34(m_targets[i].transform), false, ColorB(0x80, 0x80, 0xff, alpha), eBBD_Faceted);
		pAuxGeom->DrawLine(m_targets[i].transform.t, ColorB(0xff, 0xff, 0xff, alpha), targetAbsolute.t, ColorB(0xff, 0x80, 0x80, alpha));
	}
}

// IAnimationPoseModifier

bool CTransformBlender::Prepare(const SAnimationPoseModifierParams& params)
{
	if (m_bInitialized)
		return true;

	const CDefaultSkeleton& pSkeleton = PoseModifierHelper::GetDefaultSkeleton(params);

	m_nodeIndex = uint(-1);
	if (m_desc.node.IsSet())
		m_nodeIndex = m_desc.node.ResolveJointIndex(pSkeleton);

	m_defaultTargetIndex = uint(-1);
	if (m_desc.defaultTarget.IsSet())
		m_defaultTargetIndex = m_desc.defaultTarget.ResolveJointIndex(pSkeleton);

	STransformBlenderTarget target;
	SNodeWeightDesc targetDesc;
	m_targets.clear();

	if (m_desc.weight > 0.0f)
	{
		for (int i = 0; i < m_desc.targets.size(); i++)
		{
			targetDesc = m_desc.targets[i];
			if (targetDesc.enabled)
			{
				if (targetDesc.targetNode.IsSet())
				{
					uint targetNodeIndex = targetDesc.targetNode.ResolveJointIndex(pSkeleton);
					if (targetNodeIndex != uint(-1))
					{
						target = STransformBlenderTarget();
						target.nodeIndex = targetNodeIndex;
						target.weightNodeIndex = uint(-1);
						if (targetDesc.weightNode.IsSet())
							target.weightNodeIndex = targetDesc.weightNode.ResolveJointIndex(pSkeleton);

						target.defaultWeight = targetDesc.weight;

						m_targets.push_back(target);
					}
				}
			}
		}
	}

	m_bInitialized = true;
	return true;
}

bool CTransformBlender::Execute(const SAnimationPoseModifierParams& params)
{
	if (!m_bInitialized)
		return false;

	int targetSize = m_targets.size();
	if (targetSize == 0)
		return false;

	Skeleton::CPoseData* pPoseData = Skeleton::CPoseData::GetPoseData(params.pPoseData);
	if (!pPoseData)
		return false;

	const CDefaultSkeleton& defaultSkeleton = PoseModifierHelper::GetDefaultSkeleton(params);

	if (m_nodeIndex == uint(-1) || m_nodeIndex >= pPoseData->GetJointCount())
		return false;

	const QuatT& drivenAbsolute = pPoseData->GetJointAbsolute(m_nodeIndex);

	QuatT defaultAbsolute = drivenAbsolute;
	if (m_defaultTargetIndex != uint(-1) && m_defaultTargetIndex < pPoseData->GetJointCount())
		defaultAbsolute = pPoseData->GetJointAbsolute(m_defaultTargetIndex);

	QuatT targetAbsolute(ZERO);

	if (m_desc.ordered)
	{
		targetAbsolute = defaultAbsolute;

		for (int i = 0; i < targetSize; i++)
		{
			STransformBlenderTarget& target = m_targets[i];

			target.weight = target.defaultWeight;
			if (target.weightNodeIndex != uint(-1))
				target.weight *= clamp_tpl(pPoseData->GetJointRelative(target.weightNodeIndex).t.x, 0.0f, 1.0f);

			if (target.weight > 0.0f)
			{
				target.transform = pPoseData->GetJointAbsolute(target.nodeIndex);
				targetAbsolute = QuatT::CreateNLerp(targetAbsolute, target.transform, target.weight);
			}
		}
	}
	else
	{
		float sumWeight = 0.0f;

		for (int i = 0; i < targetSize; i++)
		{
			STransformBlenderTarget& target = m_targets[i];

			target.weight = target.defaultWeight;
			if (target.weightNodeIndex != uint(-1))
				target.weight *= clamp_tpl(pPoseData->GetJointRelative(target.weightNodeIndex).t.x, 0.0f, 1.0f);

			if (target.weight > 0.0f)
			{
				target.transform = pPoseData->GetJointAbsolute(target.nodeIndex);
				targetAbsolute.t += target.transform.t * target.weight;
				targetAbsolute.q += target.transform.q * target.weight * fsgnnz(target.transform.q | defaultAbsolute.q);

				sumWeight += target.weight;
			}
		}

		if (sumWeight > 0.0f)
		{
			targetAbsolute.t /= sumWeight;
			targetAbsolute.q.Normalize();
		}

		targetAbsolute = QuatT::CreateNLerp(defaultAbsolute, targetAbsolute, clamp_tpl(sumWeight, 0.0f, 1.0f));
	}

	targetAbsolute = QuatT::CreateNLerp(drivenAbsolute, targetAbsolute, m_desc.weight);
	PoseModifierHelper::SetJointAbsoluteLocation(defaultSkeleton, *pPoseData, m_nodeIndex, targetAbsolute);

	if (m_bDraw)
		Draw(targetAbsolute, defaultAbsolute);

	return true;
}

// IAnimationSerializable

void CTransformBlender::Serialize(Serialization::IArchive& ar)
{
	ar(m_bDraw, "draw", "^Draw");
	PoseModifier::Serialize(ar, m_desc);

	if (ar.isInput())
		m_bInitialized = false;
}

} // namespace PoseModifier

/*
   CPoseModifierStack
 */

CRYREGISTER_CLASS(CPoseModifierStack)

//

CPoseModifierStack::CPoseModifierStack()
{
	m_modifiers.reserve(16);
}

//

bool CPoseModifierStack::Push(IAnimationPoseModifierPtr instance)
{
	if (!instance)
		return false;

	m_modifiers.push_back(instance);
	return true;
}

// IAnimationPoseModifier

bool CPoseModifierStack::Prepare(const SAnimationPoseModifierParams& params)
{
	const uint count = uint(m_modifiers.size());
	for (uint i = 0; i < count; ++i)
		m_modifiers[i]->Prepare(params);
	return true;
}

bool CPoseModifierStack::Execute(const SAnimationPoseModifierParams& params)
{
	DEFINE_PROFILER_FUNCTION();

	const uint count = uint(m_modifiers.size());
	for (uint i = 0; i < count; ++i)
		m_modifiers[i]->Execute(params);
	return true;
}

void CPoseModifierStack::Synchronize()
{
	const uint count = uint(m_modifiers.size());
	for (uint i = 0; i < count; ++i)
		m_modifiers[i]->Synchronize();
}

/*
   PoseModifierSetup::Entry
 */

bool Serialize(Serialization::IArchive& ar, IAnimationPoseModifierPtr& pointer, const char* name, const char* label)
{
	Serialization::CryExtensionPointer<IAnimationPoseModifier, IAnimationSerializable> serializer(pointer);
	return ar(serializer, name, label);
}

void CPoseModifierSetup::Entry::Serialize(Serialization::IArchive& ar)
{
	ar(enabled, "enabled", "^");
	if (!ar(instance, "instance", "^"))
	{
		// load old GUID-based name
		CryClassID classId = CryClassID::Null();
		ar(classId.hipart, "guidHiPart");
		ar(classId.lopart, "guidLoPart");
		if (classId.hipart != 0 || classId.lopart != 0)
		{
			ICryFactoryRegistry* factoryRegistry = gEnv->pSystem->GetCryFactoryRegistry();
			if (ICryFactory* factory = factoryRegistry->GetFactory(classId))
				instance = std::static_pointer_cast<IAnimationPoseModifier>(factory->CreateClassInstance());
			if (IAnimationSerializable* ser = cryinterface_cast<IAnimationSerializable>(instance.get()))
				ser->Serialize(ar);
			else
				instance.reset();
		}
	}
}

/*
   PoseModifierSetup
 */

CRYREGISTER_CLASS(CPoseModifierSetup)

bool CPoseModifierSetup::Create(CPoseModifierSetup& setup)
{
	return Serialization::CloneBinary(setup, *this);
}

bool CPoseModifierSetup::CreateStack()
{
	if (!m_pPoseModifierStack)
		m_pPoseModifierStack = CPoseModifierStack::CreateClassInstance();
	if (!m_pPoseModifierStack)
		return false;

	m_pPoseModifierStack->Clear();
	const uint count = uint(m_modifiers.size());
	for (uint i = 0; i < count; ++i)
	{
		if (m_modifiers[i].enabled)
			m_pPoseModifierStack->Push(m_modifiers[i].instance);
	}

	return true;
}

//

void CPoseModifierSetup::Serialize(Serialization::IArchive& ar)
{
	ar(m_modifiers, "Modifiers", "Modifiers (Experimental)");
	ar.doc("List of modifiers (IK chains, constraints, ...) that can modify the pose after animations & physics are applied.");
	CreateStack();
}
IAnimationPoseModifier* CPoseModifierSetup::GetEntry(int index)
{
	if (index >= 0 && index < m_modifiers.size())
	{
		return m_modifiers[index].instance.get();
	}
	return nullptr;
}

int CPoseModifierSetup::GetEntryCount()
{
	return m_modifiers.size();
}

