// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CAttachmentPROW;
class CAttachmentBONE;

namespace Command
{

class CEvaluationContext;
class CState;

enum
{
	eClearPoseBuffer = 0,
	eAddPoseBuffer,     //reads content from m_SourceBuffer, multiplies the pose by a blend weight, and adds the result to the m_TargetBuffer

	eSampleAddAnimFull,
	eScaleUniformFull,
	eNormalizeFull,

	eSampleAddAnimPart, // Layer-Sampling for Override and Additive. This command adds a sample partial-body animation and its per-joint Blend-Weights to a destination buffer.
	ePerJointBlending,  // Layer-Blending for Override and Additive. This command is using Blend-Weigths per joint, which can be different for positions and orientations

	eJointMask,
	ePoseModifier,

	//just for debugging
	eSampleAddPoseFull, //used to playback the frames in a CAF-file which stored is in GlobalAnimationHeaderAIM
	eVerifyFull,

	eUpdateRedirectedJoint,
	eUpdatePendulumRow,
	ePrepareAllRedirectedTransformations,
	eGenerateProxyModelRelativeTransformations,

	eComputeAbsolutePose,
	eProcessAnimationDrivenIk,
	ePhysicsSync,
};

//this command deletes all previous entries in a pose-buffer (no matter if Temp or Target-Buffer)
struct ClearPoseBuffer
{
	enum { ID = eClearPoseBuffer };

	uint8 m_nCommand;
	uint8 m_TargetBuffer;
	uint8 m_nJointStatus;
	uint8 m_nPoseInit;

	void  Execute(const CState& state, CEvaluationContext& context) const;
};

struct AddPoseBuffer
{
	enum { ID = eAddPoseBuffer };

	uint8 m_nCommand;
	uint8 m_SourceBuffer;
	uint8 m_TargetBuffer;
	uint8 m_IsEmpty; //temporary
	f32   m_fWeight;

	void  Execute(const CState& state, CEvaluationContext& context) const;
};

struct SampleAddAnimFull
{
	enum { ID = eSampleAddAnimFull };
	enum
	{
		Flag_ADMotion  = 1,
		Flag_TmpBuffer = 8,
	};

	uint8 m_nCommand;
	uint8 m_flags;
	int16 m_nEAnimID;
	f32   m_fETimeNew; //this is a percentage value between 0-1
	f32   m_fWeight;

	void  Execute(const CState& state, CEvaluationContext& context) const;
};

struct ScaleUniformFull
{
	enum { ID = eScaleUniformFull };

	uint8 m_nCommand;
	uint8 m_TargetBuffer;
	uint8 _PADDING0;
	uint8 _PADDING1;
	f32   m_fScale;

	void  Execute(const CState& state, CEvaluationContext& context) const;
};

struct NormalizeFull
{
	enum { ID = eNormalizeFull };

	uint8 m_nCommand;
	uint8 m_TargetBuffer;
	uint8 _PADDING0;
	uint8 _PADDING1;

	void  Execute(const CState& state, CEvaluationContext& context) const;
};

struct SampleAddAnimPart
{
	enum { ID = eSampleAddAnimPart };

	uint8 m_nCommand;
	uint8 m_TargetBuffer;
	uint8 m_SourceBuffer;
	uint8 _PADDING1;

	int32 m_nEAnimID;

	f32   m_fAnimTime; //this is a percentage value between 0-1
	f32   m_fWeight;

#if defined(USE_PROTOTYPE_ABS_BLENDING)
	strided_pointer<const int>   m_maskJointIDs;
	strided_pointer<const float> m_maskJointWeights;
	int                          m_maskNumJoints;
#endif //!defined(USE_PROTOTYPE_ABS_BLENDING)

	void Execute(const CState& state, CEvaluationContext& context) const;
};

struct PerJointBlending
{
	enum { ID = ePerJointBlending };

	uint8 m_nCommand;
	uint8 m_SourceBuffer;
	uint8 m_TargetBuffer;
	uint8 m_BlendMode; //0=Override / 1=Additive

	void  Execute(const CState& state, CEvaluationContext& context) const;
};

//this is only for debugging of uncompiled aim-pose CAFs
struct SampleAddPoseFull
{
	enum { ID = eSampleAddPoseFull };
	uint8 m_nCommand;
	uint8 m_flags;
	int16 m_nEAnimID;
	f32   m_fETimeNew; //this is a percentage value between 0-1
	f32   m_fWeight;
	void  Execute(const CState& state, CEvaluationContext& context) const;
};

struct PoseModifier
{
	enum { ID = ePoseModifier };

	uint8                   m_nCommand;
	uint8                   m_TargetBuffer;
	uint8                   _PADDING0;
	uint8                   _PADDING1;
	IAnimationPoseModifier* m_pPoseModifier;

	void                    Execute(const CState& state, CEvaluationContext& context) const;
};

struct JointMask
{
	enum { ID = eJointMask };

	uint8         m_nCommand;
	uint8         m_count;
	uint8         _PADDING0;
	uint8         _PADDING1;
	const uint32* m_pMask;

	void          Execute(const CState& state, CEvaluationContext& context) const;
};

struct VerifyFull
{
	enum { ID = eVerifyFull };

	uint8 m_nCommand;
	uint8 m_TargetBuffer;
	uint8 _PADDING0;
	uint8 _PADDING1;

	void  Execute(const CState& state, CEvaluationContext& context) const;
};

struct UpdateRedirectedJoint
{
	enum { ID = eUpdateRedirectedJoint };
	uint8            m_nCommand;
	uint8            _PADDING[3];
	CAttachmentBONE* m_attachmentBone;

	void             Execute(const CState& state, CEvaluationContext& context) const;
};

struct UpdatePendulumRow
{
	enum { ID = eUpdatePendulumRow };
	uint8            m_nCommand;
	uint8            _PADDING[3];
	CAttachmentPROW* m_attachmentPendulumRow;

	void             Execute(const CState& state, CEvaluationContext& context) const;
};

struct PrepareAllRedirectedTransformations
{
	enum { ID = ePrepareAllRedirectedTransformations };
	uint8 m_nCommand;
	uint8 _PADDING[3];

	void  Execute(const CState& state, CEvaluationContext& context) const;
};

struct GenerateProxyModelRelativeTransformations
{
	enum { ID = eGenerateProxyModelRelativeTransformations };
	uint8 m_nCommand;
	uint8 _PADDING[3];

	void  Execute(const CState& state, CEvaluationContext& context) const;
};

struct ComputeAbsolutePose
{
	enum { ID = eComputeAbsolutePose };
	uint8 m_nCommand;
	uint8 _PADDING[3];

	void  Execute(const CState& state, CEvaluationContext& context) const;
};

struct ProcessAnimationDrivenIk
{
	enum { ID = eProcessAnimationDrivenIk };
	uint8 m_nCommand;
	uint8 _PADDING[3];

	void  Execute(const CState& state, CEvaluationContext& context) const;
};

struct PhysicsSync
{
	enum { ID = ePhysicsSync };
	uint8 m_nCommand;
	uint8 _PADDING[3];

	void  Execute(const CState& state, CEvaluationContext& context) const;
};

} // namespace Command
