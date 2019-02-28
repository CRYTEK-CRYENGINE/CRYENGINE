// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef LookAtSimple_h
#define LookAtSimple_h

#include <CryExtension/ClassWeaver.h>

namespace AnimPoseModifier {

class CRY_ALIGN(32) CLookAtSimple:
	public IAnimationPoseModifier
{
private:
	struct State
	{
		int32 jointId;
		Vec3  jointOffsetRelative;
		Vec3  targetGlobal;
		f32   weight;
	};

public:
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimationPoseModifier)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CLookAtSimple, "AnimationPoseModifier_LookAtSimple", "ba7e2a80-9970-435f-b667-9c08df616d74"_cry_guid);

	CLookAtSimple();
	virtual ~CLookAtSimple() {}

public:
	void SetJointId(uint32 id)                      { m_state.jointId = id; }
	void SetJointOffsetRelative(const Vec3& offset) { m_state.jointOffsetRelative = offset; }

	void SetTargetGlobal(const Vec3& target)        { m_state.targetGlobal = target; }

	void SetWeight(f32 weight)                      { m_state.weight = weight; }

private:
	bool ValidateJointId(IDefaultSkeleton & pModelSkeleton);

	// IAnimationPoseModifier
public:
	virtual bool Prepare(const SAnimationPoseModifierParams &params) override;
	virtual bool Execute(const SAnimationPoseModifierParams &params) override;
	virtual void Synchronize() override;

	void GetMemoryUsage(ICrySizer* pSizer) const override
	{
		pSizer->AddObject(this, sizeof(*this));
	}

private:
	State m_state;
	State m_stateExecute;
};

} // namespace AnimPoseModifier

#endif // LookAtSimple_h
