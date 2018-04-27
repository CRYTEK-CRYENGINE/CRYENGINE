// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef TransformationPinning_h
#define TransformationPinning_h

class CRY_ALIGN(32) CTransformationPinning :
	public ITransformationPinning
{
public:
	struct TransformationPinJoint
	{
		enum Type
		{
			Copy		= 'C',
			Feather		= 'F',
			Inherit		= 'I'
		};
	};

	CRYINTERFACE_BEGIN()
		CRYINTERFACE_ADD(IAnimationPoseModifier)
		CRYINTERFACE_ADD(ITransformationPinning)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CTransformationPinning, "AnimationPoseModifier_TransformationPin", "cc34ddea-972e-47da-93f9-cdcb98c28c8e"_cry_guid)

	CTransformationPinning();
	virtual ~CTransformationPinning();

public:
	virtual void SetBlendWeight(float factor) override;
	virtual void SetJoint(uint32 jntID) override;
	virtual void SetSource(ICharacterInstance* source) override;

	// IAnimationPoseModifier
public:
	virtual bool Prepare(const SAnimationPoseModifierParams& params) override { return true; }
	virtual bool Execute(const SAnimationPoseModifierParams& params) override;
	virtual void Synchronize() override {}

	void GetMemoryUsage( ICrySizer *pSizer ) const override
	{
		pSizer->AddObject(this, sizeof(*this));
	}
private:
	float m_factor;

	uint32 m_jointID;
	char *m_jointTypes;
	uint32 m_numJoints;
	ICharacterInstance* m_source;
	bool m_jointsInitialised;

	void Init(const SAnimationPoseModifierParams& params);

};

#endif // TransformationPinning_h
