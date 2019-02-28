// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAnimation/IAnimationPoseModifier.h>
#include <CryExtension/ClassWeaver.h>

struct SFeetData
{
	QuatT  m_WorldEndEffector;
	uint32 m_IsEndEffector;
	void   GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}
};

class CFeetPoseStore :
	public IAnimationPoseModifier
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimationPoseModifier)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CFeetPoseStore, "AnimationPoseModifier_FeetPoseStore", "4095cfb0-96b5-494f-864d-3c007b71d31d"_cry_guid)

	virtual ~CFeetPoseStore() {}

	// IAnimationPoseModifier
public:
	virtual bool Prepare(const SAnimationPoseModifierParams& params) override { return true; }
	virtual bool Execute(const SAnimationPoseModifierParams& params) override;
	virtual void Synchronize() override                                       {}

	void         GetMemoryUsage(ICrySizer* pSizer) const override
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(m_pFeetData);
	}
public://private:
	SFeetData* m_pFeetData;
};

class CFeetPoseRestore :
	public IAnimationPoseModifier
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimationPoseModifier)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CFeetPoseRestore, "AnimationPoseModifier_FeetPoseRestore", "90662f0e-d05a-4bf4-8fb6-9924b5da2872"_cry_guid)

	virtual ~CFeetPoseRestore() {}

	// IAnimationPoseModifier
public:
	virtual bool Prepare(const SAnimationPoseModifierParams& params) override { return true; }
	virtual bool Execute(const SAnimationPoseModifierParams& params) override;
	virtual void Synchronize() override                                       {}

	void         GetMemoryUsage(ICrySizer* pSizer) const override
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(m_pFeetData);
	}
public://private:
	SFeetData* m_pFeetData;
};

class CFeetLock
{
private:
	IAnimationPoseModifierPtr m_store;
	IAnimationPoseModifierPtr m_restore;

public:
	CFeetLock();

public:
	void                    Reset()   {}
	IAnimationPoseModifier* Store()   { return m_store.get(); }
	IAnimationPoseModifier* Restore() { return m_restore.get(); }

private:
	SFeetData m_FeetData[MAX_FEET_AMOUNT];
};
