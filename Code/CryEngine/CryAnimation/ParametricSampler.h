// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// Compute blending weights for LMG.
struct BC4
{
	f32 w0, w1, w2, w3;
};
struct BC5
{
	f32 w0, w1, w2, w3, w4;
};
struct BC6
{
	f32 w0, w1, w2, w3, w4, w5;
};
struct BC8
{
	f32 w0, w1, w2, w3, w4, w5, w6, w7;
};

#define MAX_LMG_ANIMS    (40)
#define MAX_LMG_EXAMPLES (64)

struct SParametricSamplerInternal : public SParametricSampler
{
	uint32 m_numExamples;                 //number of used animations per Parametric Group
	int16  m_nAnimID[MAX_LMG_ANIMS];
	uint8  m_nLMGAnimIdx[MAX_LMG_EXAMPLES];
	uint8  m_nSegmentCounterPrev[2][MAX_LMG_ANIMS];
	uint8  m_nSegmentCounter[2][MAX_LMG_ANIMS];
	f32    m_fBlendWeight[MAX_LMG_ANIMS];         //percentage blend-value for this motion
	f32    m_fPlaybackScale[MAX_LMG_ANIMS];       //percentage blend-value for this motion

	SParametricSamplerInternal()
	{
		m_numDimensions = 0;       //max 4 dimensions
		m_nParametricType = 0xff;  //invalid type by default
		m_MotionParameter[0] = 0.0f;
		m_MotionParameter[1] = 0.0f;
		m_MotionParameter[2] = 0.0f;
		m_MotionParameter[3] = 0.0f;
		m_MotionParameterForNextIteration[0] = 0.0f;
		m_MotionParameterForNextIteration[1] = 0.0f;
		m_MotionParameterForNextIteration[2] = 0.0f;
		m_MotionParameterForNextIteration[3] = 0.0f;
		m_MotionParameterID[0] = 0xff;
		m_MotionParameterID[1] = 0xff;
		m_MotionParameterID[2] = 0xff;
		m_MotionParameterID[3] = 0xff;
		m_MotionParameterFlags[0] = 0;
		m_MotionParameterFlags[1] = 0;
		m_MotionParameterFlags[2] = 0;
		m_MotionParameterFlags[3] = 0;
		m_numExamples = 0;
		for (uint32 i = 0; i < MAX_LMG_ANIMS; i++)
		{
			m_nAnimID[i] = -7;
			m_nSegmentCounterPrev[0][i] = 0;
			m_nSegmentCounterPrev[1][i] = 0;
			m_nSegmentCounter[0][i] = 0;
			m_nSegmentCounter[1][i] = 0;
			m_fBlendWeight[i] = 0.0f;
			m_fPlaybackScale[i] = 1.0f;
		}
	}

	void Serialize(TSerialize ser)
	{
		if (ser.GetSerializationTarget() != eST_Network)
		{
			ser.BeginGroup("SParametricSampler");
			ser.Value("m_numDimensions", m_numDimensions);
			ser.Value("m_nParametricType", m_nParametricType);
			ser.Value("MotionParameter0", m_MotionParameter[0]);
			ser.Value("MotionParameter1", m_MotionParameter[1]);
			ser.Value("MotionParameter2", m_MotionParameter[2]);
			ser.Value("MotionParameter3", m_MotionParameter[3]);
			ser.Value("MotionParameterForNextIteration0", m_MotionParameterForNextIteration[0]);
			ser.Value("MotionParameterForNextIteration1", m_MotionParameterForNextIteration[1]);
			ser.Value("MotionParameterForNextIteration2", m_MotionParameterForNextIteration[2]);
			ser.Value("MotionParameterForNextIteration3", m_MotionParameterForNextIteration[3]);
			ser.Value("MotionParameterID0", m_MotionParameterID[0]);
			ser.Value("MotionParameterID1", m_MotionParameterID[1]);
			ser.Value("MotionParameterID2", m_MotionParameterID[2]);
			ser.Value("MotionParameterID3", m_MotionParameterID[3]);
			ser.Value("m_MotionParameterFlags0", m_MotionParameterFlags[0]);
			ser.Value("m_MotionParameterFlags1", m_MotionParameterFlags[1]);
			ser.Value("m_MotionParameterFlags2", m_MotionParameterFlags[2]);
			ser.Value("m_MotionParameterFlags3", m_MotionParameterFlags[3]);
			ser.Value("m_numExamples", m_numExamples);
			for (uint32 i = 0; i < m_numExamples; i++)
			{
				ser.BeginGroup("ID");
				ser.Value("nAnimIDs", m_nAnimID[i]);
				ser.Value("nSegmentCounter0", m_nSegmentCounter[0][i]);
				ser.EndGroup();
			}
			ser.EndGroup();
		}
	};

	f32           Parameterizer(const CAnimationSet* pAnimationSet, const CDefaultSkeleton* pDefaultSkeleton, const CAnimation& rCurAnim, f32 fFrameDeltaTime, f32 fPlaybackScale, bool AllowDebug);

	void          BlendSpace1D(GlobalAnimationHeaderLMG& rLMG, const CAnimationSet* pAnimationSet, const CDefaultSkeleton* pDefaultSkeleton, const CAnimation& rCurAnim, f32 fFrameDeltaTime, f32 fPlaybackScale, Vec3 off, bool AllowDebug, uint32 nInstanceOffset);
	void          BlendSpace2D(GlobalAnimationHeaderLMG& rLMG, const CAnimationSet* pAnimationSet, const CDefaultSkeleton* pDefaultSkeleton, const CAnimation& rCurAnim, f32 fFrameDeltaTime, f32 fPlaybackScale, Vec3 off, bool AllowDebug, uint32 nInstanceOffset);
	void          BlendSpace3D(GlobalAnimationHeaderLMG& rLMG, const CAnimationSet* pAnimationSet, const CDefaultSkeleton* pDefaultSkeleton, const CAnimation& rCurAnim, f32 fFrameDeltaTime, f32 fPlaybackScale, Vec3 off, bool AllowDebug, uint32 nInstanceOffset);

	void          CombinedBlendSpaces(GlobalAnimationHeaderLMG& rLMG, const CAnimationSet* pAnimationSet, const CDefaultSkeleton* pDefaultSkeleton, const CAnimation& rCurAnim, f32 fFrameDeltaTime, f32 fPlaybackScale, bool AllowDebug);

	int           GetWeights1D(f32 fDesiredParameter, const GlobalAnimationHeaderLMG& rLMG, f32 arrWeights[], Diag33 scl, Vec3 off) const;
	int           GetWeights2D(const Vec2& vDesiredParameter, const GlobalAnimationHeaderLMG& rLMG, f32 arrWeights[], Diag33 scl, Vec3 off) const;
	int           GetWeights3D(const Vec3& vDesiredParameter, const GlobalAnimationHeaderLMG& rLMG, f32 arrWeights[], Diag33 scl, Vec3 off) const;

	BC4           GetConvex4(uint32 numPoints, const Vec2& vDesiredParameter, const GlobalAnimationHeaderLMG& rLMG, f32 arrWeights[], uint8* idx, Vec3* vtx, Diag33 scl, Vec3 off) const;
	void          ComputeWeightExtrapolate4(Vec4& Weight4, const Vec2& P, const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3) const;

	BC8           GetConvex8(uint32 numPoints, const Vec3& vDesiredParameter, const GlobalAnimationHeaderLMG& rLMG, f32 arrWeights[], uint8* i, Vec3* v, Diag33 scl, Vec3 off) const;
	BC4           WeightTetrahedron(const Vec3& CP, const Vec3& t0, const Vec3& t1, const Vec3& t2, const Vec3& t3) const;
	BC5           WeightPyramid(const Vec3& ControlPoint, const Vec3& t0, const Vec3& t1, const Vec3& t2, const Vec3& t3, const Vec3& t4) const;
	BC6           WeightPrism(const Vec3& ControlPoint, const Vec3& t0, const Vec3& t1, const Vec3& t2, const Vec3& t3, const Vec3& t4, const Vec3& t5) const;

	virtual uint8 GetCurrentSegmentIndexBSpace() const
	{
		//@Sven, we need a meaningful implementation for this.
		return m_nSegmentCounter[0][0];  //just for backward compatibility with the old implementation
	}

#if BLENDSPACE_VISUALIZATION
	void BlendSpace1DVisualize(GlobalAnimationHeaderLMG& rLMG, const CAnimation& rCurAnim, const CAnimationSet* pAnimationSet, float fPlaybackScale, int nInstanceOffset, unsigned int fDebugConfig, float fUniScale) const;
	void BlendSpace2DVisualize(GlobalAnimationHeaderLMG& rLMG, const CAnimation& rCurAnim, const CAnimationSet* pAnimationSet, float fPlaybackScale, int nInstanceOffset, unsigned int fDebugConfig, float fUniScale) const;
	void BlendSpace3DVisualize(GlobalAnimationHeaderLMG& rLMG, const CAnimation& rCurAnim, const CAnimationSet* pAnimationSet, float fPlaybackScale, int nInstanceOffset, unsigned int fDebugConfig, float fUniScale) const;
	void VisualizeBlendSpace(const IAnimationSet* pAnimationSet, const CAnimation& rCurAnim, f32 fPlaybackScale, uint32 nInstanceOffset, const GlobalAnimationHeaderLMG& rLMG, Vec3 off, int selFace, float fUniScale) const;
	Vec3 m_vDesiredParameter;
#endif
};
