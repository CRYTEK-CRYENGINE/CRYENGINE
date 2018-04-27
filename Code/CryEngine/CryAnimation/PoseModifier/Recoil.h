// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAnimation/IAnimationPoseModifier.h>
#include <CryExtension/ClassWeaver.h>

namespace PoseModifier
{

class CRY_ALIGN(32) CRecoil: public IAnimationPoseModifier
{
public:
	struct State
	{
		f32    time;
		f32    duration;
		f32    strengh;
		f32    kickin;
		f32    displacerad;
		uint32 arms;

		State()
		{
			Reset();
		}

		void Reset()
		{
			time = 100.0f;
			duration = 0.0f;
			strengh = 0.0f;
			kickin = 0.8f;
			displacerad = 0.0f;
			arms = 3;
		}
	};

public:
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimationPoseModifier)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CRecoil, "AnimationPoseModifier_Recoil", "d7900cb9-e7be-4825-99e1-cc1211f9c561"_cry_guid)

	CRecoil();
	virtual ~CRecoil() {}

public:
	void SetState(const State& state) { m_state = state; m_bStateUpdate = true; }

private:
	f32 RecoilEffect(f32 t);

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
	bool m_bStateUpdate;
};

} // namespace PoseModifier
