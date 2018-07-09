// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ParametricSampler.h"

#include <numeric>

#include <CryRenderer/IRenderAuxGeom.h>
#include "CharacterInstance.h"
#include "CharacterManager.h"
#include <float.h>
#include <CryCore/Platform/IPlatformOS.h>
#include <CryGame/GameUtils.h>

const f32 fRenderPosBS1 = 7.0f;
const f32 fRenderPosBS2 = -21.0f;

f32 SParametricSamplerInternal::Parameterizer(const CAnimationSet* pAnimationSet, const CDefaultSkeleton* pDefaultSkeleton, const CAnimation& rCurAnim, f32 fFrameDeltaTime, f32 fPlaybackScale, bool AllowDebug)
{
	int32 nAnimID = rCurAnim.GetAnimationId();
	memset(m_fBlendWeight, 0, sizeof(m_fBlendWeight));

	//check if ParaGroup is invalid
	const ModelAnimationHeader* pAnim = pAnimationSet->GetModelAnimationHeader(nAnimID);
	assert(pAnim->m_nAssetType == LMG_File);
	GlobalAnimationHeaderLMG& rLMG = g_AnimationManager.m_arrGlobalLMG[pAnim->m_nGlobalAnimId];
	uint32 nError = rLMG.m_Status.size();
	if (nError == 0)
	{
		//---------------------------------------------------------------------
		//compute the Blend-Weights for a Parametric Group (using a VEG))
		//---------------------------------------------------------------------
		uint32 nDimensions = rLMG.m_Dimensions;
		assert(nDimensions > 0 && nDimensions < 5);
		uint32 numBlendSpaces = rLMG.m_arrCombinedBlendSpaces.size();
		if (numBlendSpaces == 0)
		{
			Vec3 off = Vec3(fRenderPosBS1, 0, 0.01f);
			//Blend-Spaces are special type of Blend-Trees.
			if (nDimensions == 1)
				BlendSpace1D(rLMG, pAnimationSet, pDefaultSkeleton, rCurAnim, fFrameDeltaTime, fPlaybackScale, off, AllowDebug, 0);
			if (nDimensions == 2)
				BlendSpace2D(rLMG, pAnimationSet, pDefaultSkeleton, rCurAnim, fFrameDeltaTime, fPlaybackScale, off, AllowDebug, 0);
			if (nDimensions == 3)
				BlendSpace3D(rLMG, pAnimationSet, pDefaultSkeleton, rCurAnim, fFrameDeltaTime, fPlaybackScale, off, AllowDebug, 0);
		}
		else
		{
			//into the future this might turn into a cascaded blend-tree
			CombinedBlendSpaces(rLMG, pAnimationSet, pDefaultSkeleton, rCurAnim, fFrameDeltaTime, fPlaybackScale, AllowDebug);
		}
	}
	else
	{
		//Usually we can't start invalid ParaGroups. So this must be the result of Hot-Loading
		gEnv->pLog->LogError("CryAnimation: %s: '%s'", rLMG.m_Status.c_str(), rLMG.GetFilePath());
	}
	//---------------------------------------------------------------------
	//check if Blend-Weights are valid
	//---------------------------------------------------------------------
	for (uint32 i = 0; i < m_numExamples; i++)
	{
		if (fabsf(m_fBlendWeight[i]) < 0.0001f)
			m_fBlendWeight[i] = 0.0f;  //remove all blend-weight without visual impact
	}
	f32 fTotalSum = 0.0f;
	for (uint32 i = 0; i < m_numExamples; i++)
		fTotalSum += m_fBlendWeight[i];

	if (fTotalSum == 0.0f) //probably an error occurred and the parameterizer was not executed at all
		m_fBlendWeight[0] = 1.0f, fTotalSum = 1.0f;
	for (uint32 i = 0; i < m_numExamples; i++)
		m_fBlendWeight[i] /= fTotalSum; //normalize weights

#if !defined(_RELEASE)
	{
		const float weightSum = std::accumulate(m_fBlendWeight, m_fBlendWeight + m_numExamples, 0.0f);
		if (std::abs(weightSum - 1.0f) > 0.005f)
		{
			CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, "Parametric sampler encountered an error while processing '%s': Blend weights do not sum up to 1.0f.", rLMG.GetFilePath());
		}
	}
#endif

	//----------------------------------------------------------------------------------------
	//Version 1
	//the original time-warping
	//-----------------------------------------------------------------------------------------
	if (0)
	{
		f32 m_fTWNDeltaTime = 0.0f; //time-warped and normalized delta-time
		for (uint32 i = 0; i < m_numExamples; i++)
		{
			int32 nAnimIDPS = m_nAnimID[i];
			if (nAnimIDPS < 0)
				CryFatalError("CryAnimation: negative");
			const ModelAnimationHeader* pAnimPS = pAnimationSet->GetModelAnimationHeader(nAnimIDPS);
			GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[pAnimPS->m_nGlobalAnimId];
			int32 segcount = m_nSegmentCounter[0][i];
			f32 fSegDuration = rCAF.GetSegmentDuration(segcount);
			fSegDuration = max(fSegDuration, 1.0f / ANIMATION_30Hz);

			f32 fNormalizedDeltaTime = fFrameDeltaTime / fSegDuration;     //per-asset normalized delta time.
			m_fTWNDeltaTime += m_fBlendWeight[i] * fNormalizedDeltaTime;   //time-warped and normalized delta-time
		}
		return m_fTWNDeltaTime;
	}

	//----------------------------------------------------------------------------------------
	//Version 2
	//compute the time-warped animation time for this Parametric Group
	//this version is using simple scaling of the duration and is causing a lot of foot-sliding
	//-----------------------------------------------------------------------------------------
	if (0)
	{
		f32 fTWDuration = 0.0f;
		for (uint32 i = 0; i < m_numExamples; i++)
		{
			if (m_fBlendWeight[i] == 0.0f)
				continue;
			int32 nAnimIDPS = m_nAnimID[i];
			if (nAnimIDPS < 0)
				CryFatalError("CryAnimation: negative");
			const ModelAnimationHeader* pAnimPS = pAnimationSet->GetModelAnimationHeader(nAnimIDPS);
			GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[pAnimPS->m_nGlobalAnimId];

			int32 segcount = m_nSegmentCounter[0][i];
			f32 fSegDuration = rCAF.GetSegmentDuration(segcount);
			fSegDuration = max(fSegDuration, 1.0f / ANIMATION_30Hz);

			fTWDuration += m_fBlendWeight[i] * fSegDuration; //time-warped and normalized delta-time
		}
		return fFrameDeltaTime / fTWDuration;
	}

	//------------------------------------------------------------------------------------------
	//Version 3
	//compute the time-warped animation time for this Parametric Group
	//this version is using the ratio of speed and distance. No foot-sliding at all
	//------------------------------------------------------------------------------------------
	if (1)
	{
		f32 fTWDuration = 0.0f;
		f32 fTWMoveSpeed = 0.0f;
		f32 fTWDistance = 0.0f;
		for (uint32 i = 0; i < m_numExamples; i++)
		{
			if (m_fBlendWeight[i] == 0.0f)
				continue;
			int32 nAnimIDPS = m_nAnimID[i];
			if (nAnimIDPS < 0)
				CryFatalError("CryAnimation: negative");
			const ModelAnimationHeader* pAnimPS = pAnimationSet->GetModelAnimationHeader(nAnimIDPS);

			/*if (AllowDebug)
			   {
			   const char* pAnimName = pAnimPS->GetAnimName();
			   float fColor[4] = {1,1,1,1};
			   g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.6f, fColor, false,"---fWeight: %20.16f %s",m_fBlendWeight[i],pAnimName );
			   g_YLine+=16.0f;
			   }*/

			GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[pAnimPS->m_nGlobalAnimId];
			const int32 segmentIndex = m_nSegmentCounter[0][i];
			const f32 t0 = rCAF.m_SegmentsTime[segmentIndex + 0];
			const f32 t1 = rCAF.m_SegmentsTime[segmentIndex + 1];
			const f32 segmentDuration = std::max(rCAF.GetSegmentDuration(segmentIndex), (1.0f / ANIMATION_30Hz));

			Vec3 root_keys[1024];
			f32 fMoveSpeed1 = 0.0f;
			//f32 fMoveSpeed2 = 0.0f;
			f32 fDistance = 0.0f;
			float fPoses = 0.0f;
			const CDefaultSkeleton::SJoint* pRootJoint = &pDefaultSkeleton->m_arrModelJoints[0];
			IController* pController = rCAF.GetControllerByJointCRC32(pRootJoint->m_nJointCRC32);
			if (pController)
			{
				uint32 numKeys = uint32(rCAF.m_fTotalDuration * ANIMATION_30Hz + 1);
				uint32 maxsize = (sizeof(root_keys) / sizeof(Vec3)) - 1;
				numKeys = (numKeys > maxsize) ? maxsize : numKeys;
				if (numKeys > 1)
				{
					//more then 1 keyframe.
					f32 fk = rCAF.m_fStartSec * ANIMATION_30Hz;
					for (uint32 k = 0; k < numKeys; k++, fk += 1.0f)
						pController->GetP(fk, root_keys[k]);

					float fNumKeys = (float)(numKeys);
					float fskey = t0 * (fNumKeys - 1.0f);
					float fekey = t1 * (fNumKeys - 1.0f);
					uint32 skey = uint32(fskey);
					uint32 ekey = uint32(fekey);
					for (uint32 k = skey; k < ekey; k++)
					{
						float fKeyLength = (root_keys[k + 0] - root_keys[k + 1]).GetLength();
						fDistance += fKeyLength;
						fMoveSpeed1 += fKeyLength * ANIMATION_30Hz;
						fPoses += 1.0f;
					}

					if (fPoses > 0.0f)
					{
						fMoveSpeed1 /= fPoses;
						//fMoveSpeed2 = fDistance/(fPoses/ANIMATION_30Hz);
					}
				}
			}
			//	float fColor[4] = {1,1,1,1};
			//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.6f, fColor, false,"fDistance: %f   fMoveSpeed1: %f  fMoveSpeed2: %f  fSegDuration: %f",fDistance,fMoveSpeed,  fDistance/(fSegDuration+1.0f/ANIMATION_30Hz), fSegDuration );
			//	g_YLine+=16.0f;
			//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.6f, fColor, false,"fDistance: %f   fMoveSpeed1: %f  fMoveSpeed2: %f  fSegDuration: %f",fDistance,fMoveSpeed1,fMoveSpeed2, fSegDuration );
			//	g_YLine+=16.0f;

			fTWDuration += m_fBlendWeight[i] * segmentDuration;
			fTWMoveSpeed += m_fBlendWeight[i] * fMoveSpeed1;
			fTWDistance += m_fBlendWeight[i] * fDistance;
		}

		//consider the Playbackspeed attached to the assets in the ParaGroup
		uint32 numParameters = rLMG.m_arrParameter.size();
		f32 fSumPlaybackScale = 0.0f;
		for (uint32 i = 0; i < m_numExamples; i++)
			fSumPlaybackScale += m_fPlaybackScale[i] * m_fBlendWeight[i];

		if (fTWDistance < 0.001f)
			return fSumPlaybackScale * (fFrameDeltaTime / fTWDuration); //most likely an idle animation, or a single pose.
		else
			return fSumPlaybackScale * (fFrameDeltaTime * (fTWMoveSpeed / fTWDistance));
	}
}

#ifdef BLENDSPACE_VISUALIZATION
void SParametricSamplerInternal::BlendSpace1DVisualize(GlobalAnimationHeaderLMG& rLMG, const CAnimation& rCurAnim, const CAnimationSet* pAnimationSet, float fPlaybackScale, int nInstanceOffset, unsigned int fDebugConfig, float fUniScale) const
{
	float fCol1111[4] = { 1, 1, 1, 1 };
	float fCol1011[4] = { 1, 0, 1, 1 };
	float fCol1001[4] = { 1, 0, 0, 1 };

	Vec2 vDesiredParameter = Vec2(m_vDesiredParameter.x, m_vDesiredParameter.y);
	const Diag33 scl(rLMG.m_DimPara[0].m_scale, 1.0f, 1.0f);
	Vec3 off(ZERO);

	int selFace = -1;
	int numExamples = rLMG.m_numExamples;
	//float* arrWeights = m_fBlendWeight;
	float arrWeights[256] = { 0 };
	{
		SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
		g_pAuxGeom->SetRenderFlags(renderFlags);

		selFace = GetWeights1D(vDesiredParameter.x, rLMG, arrWeights, scl, off);

		VirtualExample1D ve;
		ve.i0 = 0;
		ve.i1 = 0;
		ve.w0 = 0.0f;
		ve.w1 = 0.0f;
		uint32 c = 0;
		uint8* pI = &(ve.i0);
		float* pW = &(ve.w0);
		for (uint32 i = 0; i < numExamples; i++)
		{
			if (arrWeights[i])
			{
				if (c > 1)
					break;
				//CryFatalError("Invalid Weights");
				pI[c] = i;
				pW[c] = arrWeights[i];
				c++;
			}
		}
		if (fDebugConfig & 4)
		{
			float fColDebug6[4] = { 1, 0, 1, 1 };
			g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fColDebug6, false, "idx0: %d w0: %f", ve.i0, ve.w0);
			g_YLine += 21.0f;
			g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fColDebug6, false, "idx1: %d w1: %f", ve.i1, ve.w1);
			g_YLine += 21.0f;
		}
	}

	uint32 numVExamples = rLMG.m_VirtualExampleGrid1D.size();
	if (numVExamples)
	{
		SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
		g_pAuxGeom->SetRenderFlags(renderFlags);
		if (fDebugConfig & 2)
		{
			//---------------------------------------------------------------------------------------------
			//---                   visualize the virtual-examples                                  -------
			//---------------------------------------------------------------------------------------------
			static Ang3 angles = Ang3(0, 0, 0);
			angles += Ang3(0.01f, 0.02f, 0.03f);
			Matrix33 _m33 = Matrix33::CreateRotationXYZ(angles);
			AABB aabb1 = AABB(Vec3(-0.02f, -0.02f, -0.02f), Vec3(0.02f, 0.02f, 0.02f));
			OBB _obb1 = OBB::CreateOBBfromAABB(_m33, aabb1);
			for (uint32 n = 0; n < numVExamples; n++)
			{
				VirtualExample1D ve = rLMG.m_VirtualExampleGrid1D[n];
				Vec3 v0 = Vec3(rLMG.m_arrParameter[ve.i0].m_Para.x, 0, 0);
				Vec3 v1 = Vec3(rLMG.m_arrParameter[ve.i1].m_Para.x, 0, 0);
				Vec3 ip = (v0 * ve.w0 + v1 * ve.w1) * scl + off;
				g_pAuxGeom->DrawOBB(_obb1, ip, 1, RGBA8(0x00, 0x00, 0x7f, 0x00), eBBD_Extremes_Color_Encoded);
			}
		}
	}

	f32 wsum = 0.0f;
	for (uint32 i = 0; i < numExamples; i++)
		wsum += m_fBlendWeight[i];
	if (fabsf(wsum - 1.0f) > 0.05f)
		g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.6f, fCol1001, false, "Invalid Sum of Weights (%f) in BSpace: '%s'. Please verify annotations", wsum, rLMG.GetFilePath()), g_YLine += 16.0f;

	{
		SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
		g_pAuxGeom->SetRenderFlags(renderFlags);
		uint32 initD1 = m_MotionParameterFlags[0] & CA_Dim_Initialized;
		g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fCol1111, false, "%s: %f %d", rLMG.m_DimPara[0].m_strParaName.c_str(), vDesiredParameter.x, initD1);
		g_YLine += 20.0f;

		if (fDebugConfig & 1)
			g_pAuxGeom->DrawLine(Vec3(rLMG.m_DimPara[0].m_min, 0, 0.0f) * scl + off, RGBA8(0xff, 0x00, 0xff, 0x00), Vec3(rLMG.m_DimPara[0].m_max, 0, 0.0f) * scl + off, RGBA8(0xff, 0x00, 0xff, 0x00), 1.0f);

		static f32 fScaleSphere = 0.0f;
		g_pAuxGeom->DrawSphere(Vec3(vDesiredParameter.x, 0.0f, 0.0f) * scl + off, 0.05f + fScaleSphere * 0.07f, ColorB(0xff, 0, 0));
		fScaleSphere += 3.5f * g_AverageFrameTime;
		if (fScaleSphere > 1.0f) fScaleSphere -= 1.0f;
		VisualizeBlendSpace(pAnimationSet, rCurAnim, fPlaybackScale, nInstanceOffset, rLMG, off, selFace, fUniScale);
	}
}
#endif

//---------------------------------------------------------------------------------------------------------------------------

void SParametricSamplerInternal::BlendSpace1D(GlobalAnimationHeaderLMG& rLMG, const CAnimationSet* pAnimationSet, const CDefaultSkeleton* pDefaultSkeleton, const CAnimation& rCurAnim, f32 fFrameDeltaTime, f32 fPlaybackScale, Vec3 off, bool AllowDebug, uint32 nInstanceOffset)
{
	uint32 nDimensions = rLMG.m_Dimensions;
	uint32 numExamples = rLMG.m_numExamples;
	uint32 numParameter = rLMG.m_arrParameter.size();

	m_fBlendWeight[0] = 1.0f;

	//---------------------------------------------------

	f32 arrWeights[0xff];
	Vec3 vDesiredParameter = Vec3(0, 0, 0);

	//-------------------------------------------------------------
	//Dimension=1
	const char* pParaName = rLMG.m_DimPara[0].m_strParaName; //just for debugging
	uint32 nParaID = rLMG.m_DimPara[0].m_ParaID;
	if (nParaID > eMotionParamID_COUNT)
		CryFatalError("CryAnimation: nParaID not initialized");
	if (rLMG.m_DimPara[0].m_nInitialized == 0)
		rLMG.ParameterExtraction(pAnimationSet, pDefaultSkeleton, nParaID, 0);
	vDesiredParameter.x = clamp_tpl(m_MotionParameter[0], rLMG.m_DimPara[0].m_min, rLMG.m_DimPara[0].m_max);

	uint32 numParameters = rLMG.m_arrParameter.size();
	for (uint32 i = numExamples; i < numParameters; i++)
	{
		uint32 i0 = rLMG.m_arrParameter[i].i0;
		f32 w0 = rLMG.m_arrParameter[i].w0;
		uint32 i1 = rLMG.m_arrParameter[i].i1;
		f32 w1 = rLMG.m_arrParameter[i].w1;
		f32 sum = w0 + w1;
		assert(fabsf(1.0f - sum) < 0.00001f);
		rLMG.m_arrParameter[i].m_Para = rLMG.m_arrParameter[i0].m_Para * w0 + rLMG.m_arrParameter[i1].m_Para * w1;
	}

	const Diag33 scl(rLMG.m_DimPara[0].m_scale, 1.0f, 1.0f);
	f32 xstep = (rLMG.m_DimPara[0].m_max - rLMG.m_DimPara[0].m_min) / f32(rLMG.m_DimPara[0].m_cells - 1);
	uint32 initialized = rLMG.m_VirtualExampleGrid1D.size();
	if (initialized == 0)
	{
		rLMG.m_VirtualExampleGrid1D.resize(rLMG.m_DimPara[0].m_cells);
		for (uint32 c0 = 0; c0 < rLMG.m_DimPara[0].m_cells; c0++)
		{
			f32 x = f32(c0) * xstep + rLMG.m_DimPara[0].m_min;
			GetWeights1D(x, rLMG, arrWeights, scl, off);
			rLMG.m_VirtualExampleGrid1D[c0].i0 = 0;
			rLMG.m_VirtualExampleGrid1D[c0].i1 = 0;
			rLMG.m_VirtualExampleGrid1D[c0].w0 = 0.0f;
			rLMG.m_VirtualExampleGrid1D[c0].w1 = 0.0f;

			uint32 c = 0;
			uint8* pI = &(rLMG.m_VirtualExampleGrid1D[c0].i0);
			float* pW = &(rLMG.m_VirtualExampleGrid1D[c0].w0);
			for (uint32 i = 0; i < numExamples; i++)
			{
				f32 w = arrWeights[i];
				if (w)
				{
					if (c > 1)
						break;
					//CryFatalError("Invalid Weights");
					pI[c] = i;
					pW[c] = w;
					c++;
				}
			}

			f32 sum = 0.0f;
			sum += rLMG.m_VirtualExampleGrid1D[c0].w0;
			sum += rLMG.m_VirtualExampleGrid1D[c0].w1;
			if (fabsf(sum - 1.0f) > 0.001f)
			{
				g_pAuxGeom->Draw2dLabel(1, g_YLine, 3.0f, ColorF(1, 1, 1, 1), false, "invalid sum: %f", sum);
				g_YLine += 31.0f;
			}
		}
	}

	uint32 numVExamples = rLMG.m_VirtualExampleGrid1D.size();
	if (numVExamples)
	{
#if !defined(_RELEASE)
		if (AllowDebug && Console::GetInst().ca_SnapToVGrid)
		{
			f32 fClosestDist = 99999.0f;
			f32 fMotionParam = vDesiredParameter.x;
			for (uint32 c0 = 0; c0 < rLMG.m_DimPara[0].m_cells; c0++)
			{
				f32 x = f32(c0) * xstep + rLMG.m_DimPara[0].m_min;
				f32 cd = fabsf(fMotionParam - x);
				if (fClosestDist > cd)
					fClosestDist = cd, vDesiredParameter.x = x;
			}
			g_pAuxGeom->Draw2dLabel(1, g_YLine, 3.0f, ColorF(1, 1, 1, 1), false, "Snapped to VGrid: %f", vDesiredParameter.x);
			g_YLine += 31.0f;
		}
#endif

		//---------------------------------------------------------------------------------------------
		//---                   this is the 1D-parameterizer     --------------------------------------
		//---------------------------------------------------------------------------------------------
		VirtualExample1D ve;
		f32 cel0 = f32(rLMG.m_DimPara[0].m_cells - 1);
		f32 cel1 = f32(rLMG.m_DimPara[1].m_cells - 1);
		f32 fx = (vDesiredParameter.x - rLMG.m_DimPara[0].m_min) / ((rLMG.m_DimPara[0].m_max - rLMG.m_DimPara[0].m_min) / cel0);
		fx = clamp_tpl(fx, 0.0f, cel0 - 0.001f);
		int32 ix = int32(fx);
		f32 px = fx - f32(ix);
		//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 2.0f, fCol1111, false,"dpx:%f  ix:%d  px:%f",vDesiredParameter.x,ix,px);
		//	g_YLine+=21.0f;
		for (uint32 i = 0; i < numExamples; i++)
			arrWeights[i] = 0;
		ve = rLMG.m_VirtualExampleGrid1D[ix];
		arrWeights[ve.i0] += f32(ve.w0) * (1 - px);
		arrWeights[ve.i1] += f32(ve.w1) * (1 - px);
		ve = rLMG.m_VirtualExampleGrid1D[ix + 1];
		arrWeights[ve.i0] += f32(ve.w0) * px;
		arrWeights[ve.i1] += f32(ve.w1) * px;
	}

	for (uint32 i = 0; i < numExamples; i++)
	{
		PREFAST_SUPPRESS_WARNING(6001)
		m_fBlendWeight[i] = arrWeights[i];
	}

#ifdef BLENDSPACE_VISUALIZATION
	m_vDesiredParameter = Vec3(vDesiredParameter.x, vDesiredParameter.y, 0.0f);

	if (AllowDebug)
	{
		const uint32 fDebugConfig = uint32(floorf(Console::GetInst().ca_DrawVEGInfo));
		float fUniScale = Console::GetInst().ca_DrawVEGInfo - fDebugConfig;
		BlendSpace1DVisualize(rLMG, rCurAnim, pAnimationSet, fPlaybackScale, nInstanceOffset, fDebugConfig, fUniScale);
	}
#endif
}

//----------------------------------------------------------------------------------------------

#ifdef BLENDSPACE_VISUALIZATION
void SParametricSamplerInternal::BlendSpace2DVisualize(GlobalAnimationHeaderLMG& rLMG, const CAnimation& rCurAnim, const CAnimationSet* pAnimationSet, float fPlaybackScale, int nInstanceOffset, unsigned int fDebugConfig, float fUniScale) const
{
	float fCol1111[4] = { 1, 1, 1, 1 };
	float fCol1011[4] = { 1, 0, 1, 1 };
	float fCol1001[4] = { 1, 0, 0, 1 };

	Vec2 vDesiredParameter = Vec2(m_vDesiredParameter.x, m_vDesiredParameter.y);
	const Diag33 scl(rLMG.m_DimPara[0].m_scale, 1.0f, 1.0f);
	Vec3 off(ZERO);

	int selFace = -1;
	int numExamples = rLMG.m_numExamples;
	uint32 numVExamples = rLMG.m_VirtualExampleGrid2D.size();
	if (numExamples == 0 || numVExamples == 0)
		return;
	//float* arrWeights = m_fBlendWeight;
	float arrWeights[256] = { 0 };
	int selectedFace = -1;
	{

		selectedFace = GetWeights2D(vDesiredParameter, rLMG, arrWeights, scl, off);
		VirtualExample2D ve;
		ve.i0 = 0;
		ve.i1 = 0;
		ve.i2 = 0;
		ve.i3 = 0;
		ve.w0 = 0.0f;
		ve.w1 = 0.0f;
		ve.w2 = 0.0f;
		ve.w3 = 0.0f;
		uint32 c = 0;
		uint8* pI = &(ve.i0);
		float* pW = &(ve.w0);
		for (uint32 i = 0; i < numExamples; i++)
		{
			if (arrWeights[i])
			{
				pI[c] = i;
				pW[c] = arrWeights[i];
				c++;
			}
		}
		//	float fColDebug6[4]={1,0,1,1};
		//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 2.0f, fColDebug6, false,"weights: %f %f %f %f",ve.w0,ve.w1,ve.w2,ve.w3 );
		//	g_YLine+=21.0f;
	}
	{
		//---------------------------------------------------------------------------------------------
		//---                   visualize the virtual-examples                                  -------
		//---------------------------------------------------------------------------------------------
		if (fDebugConfig & 2)
		{
			static Ang3 angles = Ang3(0, 0, 0);
			angles += Ang3(0.01f, 0.02f, 0.03f);
			Matrix33 _m33 = Matrix33::CreateRotationXYZ(angles);
			AABB aabb1 = AABB(Vec3(-0.02f, -0.02f, -0.02f), Vec3(0.02f, 0.02f, 0.02f));
			OBB _obb1 = OBB::CreateOBBfromAABB(_m33, aabb1);
			for (uint32 n = 0; n < numVExamples; n++)
			{
				VirtualExample2D ve = rLMG.m_VirtualExampleGrid2D[n];
				Vec3 v0 = Vec3(rLMG.m_arrParameter[ve.i0].m_Para.x, rLMG.m_arrParameter[ve.i0].m_Para.y, 0);
				Vec3 v1 = Vec3(rLMG.m_arrParameter[ve.i1].m_Para.x, rLMG.m_arrParameter[ve.i1].m_Para.y, 0);
				Vec3 v2 = Vec3(rLMG.m_arrParameter[ve.i2].m_Para.x, rLMG.m_arrParameter[ve.i2].m_Para.y, 0);
				Vec3 v3 = Vec3(rLMG.m_arrParameter[ve.i3].m_Para.x, rLMG.m_arrParameter[ve.i3].m_Para.y, 0);
				Vec3 ip = (v0 * ve.w0 + v1 * ve.w1 + v2 * ve.w2 + v3 * ve.w3) * scl + off;
				g_pAuxGeom->DrawOBB(_obb1, ip, 1, RGBA8(0x00, 0x00, 0x7f, 0x00), eBBD_Extremes_Color_Encoded);
			}

			f32 cel0 = f32(rLMG.m_DimPara[0].m_cells - 1);
			f32 fx = (vDesiredParameter.x - rLMG.m_DimPara[0].m_min) / ((rLMG.m_DimPara[0].m_max - rLMG.m_DimPara[0].m_min) / cel0);
			fx = clamp_tpl(fx, 0.0f, cel0 - 0.001f);
			int32 ix = int32(fx);

			f32 cel1 = f32(rLMG.m_DimPara[1].m_cells - 1);
			f32 fy = (vDesiredParameter.y - rLMG.m_DimPara[1].m_min) / ((rLMG.m_DimPara[1].m_max - rLMG.m_DimPara[1].m_min) / cel1);
			fy = clamp_tpl(fy, 0.0f, cel1 - 0.001f);
			int32 iy = int32(fy);

			f32 CellSizeX = (rLMG.m_DimPara[0].m_max - rLMG.m_DimPara[0].m_min) / cel0;
			f32 CellSizeY = (rLMG.m_DimPara[1].m_max - rLMG.m_DimPara[1].m_min) / cel1;
			Vec3 p0 = Vec3((ix + 0) * CellSizeX + rLMG.m_DimPara[0].m_min, (iy + 0) * CellSizeY + rLMG.m_DimPara[1].m_min, 0.0f) * scl + off;
			Vec3 p1 = Vec3((ix + 1) * CellSizeX + rLMG.m_DimPara[0].m_min, (iy + 0) * CellSizeY + rLMG.m_DimPara[1].m_min, 0.0f) * scl + off;
			Vec3 p2 = Vec3((ix + 0) * CellSizeX + rLMG.m_DimPara[0].m_min, (iy + 1) * CellSizeY + rLMG.m_DimPara[1].m_min, 0.0f) * scl + off;
			Vec3 p3 = Vec3((ix + 1) * CellSizeX + rLMG.m_DimPara[0].m_min, (iy + 1) * CellSizeY + rLMG.m_DimPara[1].m_min, 0.0f) * scl + off;

			SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
			renderFlags.SetFillMode(e_FillModeSolid);
			renderFlags.SetAlphaBlendMode(e_AlphaAdditive);
			g_pAuxGeom->SetRenderFlags(renderFlags);
			g_pAuxGeom->DrawTriangle(p0, RGBA8(0x00, 0x7f, 0x00, 0x00), p1, RGBA8(0x00, 0x7f, 0x00, 0x00), p3, RGBA8(0x00, 0x7f, 0x00, 0x00));
			g_pAuxGeom->DrawTriangle(p0, RGBA8(0x00, 0x7f, 0x00, 0x00), p3, RGBA8(0x00, 0x7f, 0x00, 0x00), p2, RGBA8(0x00, 0x7f, 0x00, 0x00));

			uint32 mem = rLMG.m_VirtualExampleGrid2D.get_alloc_size();
			g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fCol1111, false, "mem: %d", mem);
			g_YLine += 21.0f;
		}
	}
	{
		SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
		g_pAuxGeom->SetRenderFlags(renderFlags);
		for (uint32 d = 0; d < rLMG.m_Dimensions; d++)
		{
			uint32 initD = m_MotionParameterFlags[d] & CA_Dim_Initialized;
			g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fCol1111, false, "%s: %f %d", rLMG.m_DimPara[d].m_strParaName.c_str(), vDesiredParameter[d], initD);
			g_YLine += 20.0f;
		}
		if (fDebugConfig & 1)
		{
			f32 xstep = (rLMG.m_DimPara[0].m_max - rLMG.m_DimPara[0].m_min) / f32(rLMG.m_DimPara[0].m_cells - 1);
			f32 ystep = (rLMG.m_DimPara[1].m_max - rLMG.m_DimPara[1].m_min) / f32(rLMG.m_DimPara[1].m_cells - 1);
			for (f32 x = rLMG.m_DimPara[0].m_min; x <= (rLMG.m_DimPara[0].m_max + 0.001f); x = x + xstep)
				g_pAuxGeom->DrawLine(Vec3(x, rLMG.m_DimPara[1].m_min, 0.00f) * scl + off, RGBA8(0xff, 0x00, 0xff, 0x00), Vec3(x, rLMG.m_DimPara[1].m_max, 0.0f) * scl + off, RGBA8(0xff, 0x00, 0xff, 0x00), 1.0f);
			for (f32 y = rLMG.m_DimPara[1].m_min; y <= (rLMG.m_DimPara[1].m_max + 0.001f); y = y + ystep)
				g_pAuxGeom->DrawLine(Vec3(rLMG.m_DimPara[0].m_min, y, 0.00f) * scl + off, RGBA8(0xff, 0x00, 0xff, 0x00), Vec3(rLMG.m_DimPara[0].m_max, y, 0.0f) * scl + off, RGBA8(0xff, 0x00, 0xff, 0x00), 1.0f);
		}
		static f32 fScaleSphere = 0.0f;
		g_pAuxGeom->DrawSphere(Vec3(vDesiredParameter.x, vDesiredParameter.y, 0.00f) * scl + off, 0.05f + fScaleSphere * 0.07f, ColorB(0xff, 0, 0));
		fScaleSphere += 3.5f * g_AverageFrameTime;
		if (fScaleSphere > 1.0f) fScaleSphere -= 1.0f;

		VisualizeBlendSpace(pAnimationSet, rCurAnim, fPlaybackScale, nInstanceOffset, rLMG, off, selectedFace, fUniScale);
	}
}
#endif

void SParametricSamplerInternal::BlendSpace2D(GlobalAnimationHeaderLMG& rLMG, const CAnimationSet* pAnimationSet, const CDefaultSkeleton* pDefaultSkeleton, const CAnimation& rCurAnim, f32 fFrameDeltaTime, f32 fPlaybackScale, Vec3 off, bool AllowDebug, uint32 nInstanceOffset)
{
	float fCol1111[4] = { 1, 1, 1, 1 };
	uint32 numExamples = rLMG.m_numExamples;//m_arrBSAnimations.size();

	//---------------------------------------------------

	f32 arrWeights[256];
	Vec2 vDesiredParameter = Vec2(0, 0);
	for (uint32 d = 0; d < rLMG.m_Dimensions; d++)
	{
		const char* pParaName = rLMG.m_DimPara[d].m_strParaName; //just for debugging
		uint32 nParaID = rLMG.m_DimPara[d].m_ParaID;
		if (nParaID > eMotionParamID_COUNT)
			CryFatalError("CryAnimation: nParaID not initialized");
		if (rLMG.m_DimPara[d].m_nInitialized == 0)
			rLMG.ParameterExtraction(pAnimationSet, pDefaultSkeleton, nParaID, d);
		vDesiredParameter[d] = clamp_tpl(m_MotionParameter[d], rLMG.m_DimPara[d].m_min, rLMG.m_DimPara[d].m_max);
	}

	uint32 numParameters = rLMG.m_arrParameter.size();
	for (uint32 i = numExamples; i < numParameters; i++)
	{
		uint32 i0 = rLMG.m_arrParameter[i].i0;
		f32 w0 = rLMG.m_arrParameter[i].w0;
		uint32 i1 = rLMG.m_arrParameter[i].i1;
		f32 w1 = rLMG.m_arrParameter[i].w1;
		f32 sum = w0 + w1;
		assert(fabsf(1.0f - sum) < 0.00001f);
		rLMG.m_arrParameter[i].m_Para = rLMG.m_arrParameter[i0].m_Para * w0 + rLMG.m_arrParameter[i1].m_Para * w1;
	}

	const Diag33 scl(rLMG.m_DimPara[0].m_scale, rLMG.m_DimPara[1].m_scale, 1.0f);
	f32 xstep = (rLMG.m_DimPara[0].m_max - rLMG.m_DimPara[0].m_min) / f32(rLMG.m_DimPara[0].m_cells - 1);
	f32 ystep = (rLMG.m_DimPara[1].m_max - rLMG.m_DimPara[1].m_min) / f32(rLMG.m_DimPara[1].m_cells - 1);
	uint32 initialized = rLMG.m_VirtualExampleGrid2D.size();
	if (initialized == 0)
	{
		rLMG.m_VirtualExampleGrid2D.resize(rLMG.m_DimPara[0].m_cells * rLMG.m_DimPara[1].m_cells);
		uint32 c1 = 0;
		for (f32 y = rLMG.m_DimPara[1].m_min; y <= (rLMG.m_DimPara[1].m_max + 0.001f); y = y + ystep, c1++)
		{
			uint32 c0 = 0;
			for (f32 x = rLMG.m_DimPara[0].m_min; x <= (rLMG.m_DimPara[0].m_max + 0.001f); x = x + xstep, c0++)
			{
				GetWeights2D(Vec2(x, y), rLMG, arrWeights, scl, off);
				uint32 cell = c1 * rLMG.m_DimPara[0].m_cells + c0;
				rLMG.m_VirtualExampleGrid2D[cell].i0 = 0;
				rLMG.m_VirtualExampleGrid2D[cell].i1 = 0;
				rLMG.m_VirtualExampleGrid2D[cell].i2 = 0;
				rLMG.m_VirtualExampleGrid2D[cell].i3 = 0;
				rLMG.m_VirtualExampleGrid2D[cell].w0 = 0.0f;
				rLMG.m_VirtualExampleGrid2D[cell].w1 = 0.0f;
				rLMG.m_VirtualExampleGrid2D[cell].w2 = 0.0f;
				rLMG.m_VirtualExampleGrid2D[cell].w3 = 0.0f;
				uint32 c = 0;
				uint8* pI = &(rLMG.m_VirtualExampleGrid2D[cell].i0);
				float* pW = &(rLMG.m_VirtualExampleGrid2D[cell].w0);
				for (uint32 i = 0; i < numExamples; i++)
				{
					f32 w = arrWeights[i];
					if (w)
					{
						if (c > 3)
							break;
						//CryFatalError("Invalid Weights");
						pI[c] = i;
						pW[c] = w;
						c++;
					}
				}
			}
		}
	}

	uint32 numVExamples = rLMG.m_VirtualExampleGrid2D.size();
	if (numVExamples)
	{

#if !defined(_RELEASE)
		if (AllowDebug && Console::GetInst().ca_SnapToVGrid)
		{
			f32 fClosestDist = 99999.0f;
			Vec2 vMotionParam = Vec2(vDesiredParameter);
			uint32 c1 = 0;
			for (f32 y = rLMG.m_DimPara[1].m_min; y <= (rLMG.m_DimPara[1].m_max + 0.001f); y = y + ystep, c1++)
			{
				uint32 c0 = 0;
				for (f32 x = rLMG.m_DimPara[0].m_min; x <= (rLMG.m_DimPara[0].m_max + 0.001f); x = x + xstep, c0++)
				{
					f32 cd = (vMotionParam - Vec2(x, y)).GetLength();
					if (fClosestDist > cd)
						fClosestDist = cd, vDesiredParameter.x = x, vDesiredParameter.y = y;
				}
			}
			g_pAuxGeom->Draw2dLabel(1, g_YLine, 3.0f, fCol1111, false, "Snapped to VGrid: %f  %f", vDesiredParameter.x, vDesiredParameter.y);
			g_YLine += 31.0f;
		}
#endif

		//---------------------------------------------------------------------------------------------
		//---                   this is the 2D-parameterizer     --------------------------------------
		//---------------------------------------------------------------------------------------------
		f32 cel0 = f32(rLMG.m_DimPara[0].m_cells - 1);
		f32 fx = (vDesiredParameter.x - rLMG.m_DimPara[0].m_min) / ((rLMG.m_DimPara[0].m_max - rLMG.m_DimPara[0].m_min) / cel0);
		fx = clamp_tpl(fx, 0.0f, cel0 - 0.001f);
		int32 ix = int32(fx);
		f32 px = fx - f32(ix);

		f32 cel1 = f32(rLMG.m_DimPara[1].m_cells - 1);
		f32 fy = (vDesiredParameter.y - rLMG.m_DimPara[1].m_min) / ((rLMG.m_DimPara[1].m_max - rLMG.m_DimPara[1].m_min) / cel1);
		fy = clamp_tpl(fy, 0.0f, cel1 - 0.001f);
		int32 iy = int32(fy);
		f32 py = fy - f32(iy);

		for (uint32 i = 0; i < numExamples; i++)
			arrWeights[i] = 0;

		VirtualExample2D ve;
		f32 ew0 = (1 - px) * (1 - py);
		int idx0 = (rLMG.m_DimPara[0].m_cells * (iy + 0)) + (ix + 0);
		f32 ew1 = px * (1 - py);
		int idx1 = (rLMG.m_DimPara[0].m_cells * (iy + 0)) + (ix + 1);

		f32 ew2 = (1 - px) * py;
		int idx2 = (rLMG.m_DimPara[0].m_cells * (iy + 1)) + (ix + 0);
		f32 ew3 = px * py;
		int idx3 = (rLMG.m_DimPara[0].m_cells * (iy + 1)) + (ix + 1);

		ve = rLMG.m_VirtualExampleGrid2D[idx0];
		arrWeights[ve.i0] += f32(ve.w0) * ew0;
		arrWeights[ve.i1] += f32(ve.w1) * ew0;
		arrWeights[ve.i2] += f32(ve.w2) * ew0;
		arrWeights[ve.i3] += f32(ve.w3) * ew0;

		ve = rLMG.m_VirtualExampleGrid2D[idx1];
		arrWeights[ve.i0] += f32(ve.w0) * ew1;
		arrWeights[ve.i1] += f32(ve.w1) * ew1;
		arrWeights[ve.i2] += f32(ve.w2) * ew1;
		arrWeights[ve.i3] += f32(ve.w3) * ew1;

		ve = rLMG.m_VirtualExampleGrid2D[idx2];
		arrWeights[ve.i0] += f32(ve.w0) * ew2;
		arrWeights[ve.i1] += f32(ve.w1) * ew2;
		arrWeights[ve.i2] += f32(ve.w2) * ew2;
		arrWeights[ve.i3] += f32(ve.w3) * ew2;

		ve = rLMG.m_VirtualExampleGrid2D[idx3];
		arrWeights[ve.i0] += f32(ve.w0) * ew3;
		arrWeights[ve.i1] += f32(ve.w1) * ew3;
		arrWeights[ve.i2] += f32(ve.w2) * ew3;
		arrWeights[ve.i3] += f32(ve.w3) * ew3;
	}

	for (uint32 i = 0; i < numExamples; i++)
	{
		PREFAST_SUPPRESS_WARNING(6001)
		m_fBlendWeight[i] = arrWeights[i];
	}

#ifdef BLENDSPACE_VISUALIZATION
	m_vDesiredParameter = Vec3(vDesiredParameter.x, vDesiredParameter.y, 0.0f);

	if (AllowDebug)
	{
		const uint32 fDebugConfig = uint32(floorf(Console::GetInst().ca_DrawVEGInfo));
		float fUniScale = Console::GetInst().ca_DrawVEGInfo - fDebugConfig;
		BlendSpace2DVisualize(rLMG, rCurAnim, pAnimationSet, fPlaybackScale, nInstanceOffset, fDebugConfig, fUniScale);
	}
#endif
} //function PARA

NO_INLINE void UpdateWeights(f32 ew, uint32 idx, f32* __restrict arrWeights, const GlobalAnimationHeaderLMG& rLMG)
{
	const VirtualExample3D& ve = rLMG.m_VirtualExampleGrid3D[idx];
	arrWeights[ve.i0] += f32(ve.w0) * ew;
	arrWeights[ve.i1] += f32(ve.w1) * ew;
	arrWeights[ve.i2] += f32(ve.w2) * ew;
	arrWeights[ve.i3] += f32(ve.w3) * ew;
	arrWeights[ve.i4] += f32(ve.w4) * ew;
	arrWeights[ve.i5] += f32(ve.w5) * ew;
	arrWeights[ve.i6] += f32(ve.w6) * ew;
	arrWeights[ve.i7] += f32(ve.w7) * ew;
}

#ifdef BLENDSPACE_VISUALIZATION
void SParametricSamplerInternal::BlendSpace3DVisualize(GlobalAnimationHeaderLMG& rLMG, const CAnimation& rCurAnim, const CAnimationSet* pAnimationSet, float fPlaybackScale, int nInstanceOffset, unsigned int fDebugConfig, float fUniScale) const
{
	float fCol1111[4] = { 1, 1, 1, 1 };
	float fCol1011[4] = { 1, 0, 1, 1 };
	float fCol1001[4] = { 1, 0, 0, 1 };

	Vec3 vDesiredParameter = m_vDesiredParameter;
	const Diag33 scl(rLMG.m_DimPara[0].m_scale, 1.0f, 1.0f);
	Vec3 off(ZERO);
	int selFace = -1;
	float arrWeights[256] = { 0 }; // = m_fBlendWeight;
	int numExamples = rLMG.m_numExamples;
	uint32 numVExamples = rLMG.m_VirtualExampleGrid3D.size();
	if (numExamples == 0 || numVExamples == 0)
		return;

	selFace = GetWeights3D(vDesiredParameter, rLMG, arrWeights, scl, off);
	VirtualExample3D ve;
	ve.i0 = 0;
	ve.i1 = 0;
	ve.i2 = 0;
	ve.i3 = 0;
	ve.i4 = 0;
	ve.i5 = 0;
	ve.i6 = 0;
	ve.i7 = 0;
	ve.w0 = 0.0f;
	ve.w1 = 0.0f;
	ve.w2 = 0.0f;
	ve.w3 = 0.0f;
	ve.w4 = 0.0f;
	ve.w5 = 0.0f;
	ve.w6 = 0.0f;
	ve.w7 = 0.0f;
	uint32 c = 0;
	uint8* pI = &(ve.i0);
	float* pW = &(ve.w0);
	for (uint32 i = 0; i < numExamples; i++)
	{
		if (arrWeights[i])
		{
			if (c > 7)
				break;
			//CryFatalError("Invalid Weights");
			pI[c] = i;
			pW[c] = arrWeights[i];
			c++;
		}
	}
	//	float fColDebug6[4]={1,0,1,1};
	//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 2.0f, fColDebug6, false,"weights: %f %f %f %f",ve.w0,ve.w1,ve.w2,ve.w3,ve.w3 );
	//	g_YLine+=21.0f;

	if (fDebugConfig & 2)
	{
		static Ang3 angles = Ang3(0, 0, 0);
		angles += Ang3(0.01f, 0.02f, 0.03f);
		Matrix33 _m33 = Matrix33::CreateRotationXYZ(angles);
		AABB aabb1 = AABB(Vec3(-0.02f, -0.02f, -0.02f), Vec3(0.02f, 0.02f, 0.02f));
		OBB _obb1 = OBB::CreateOBBfromAABB(_m33, aabb1);
		for (uint32 n = 0; n < numVExamples; n++)
		{
			VirtualExample3D ve = rLMG.m_VirtualExampleGrid3D[n];
			Vec3 v0 = Vec3(rLMG.m_arrParameter[ve.i0].m_Para.x, rLMG.m_arrParameter[ve.i0].m_Para.y, rLMG.m_arrParameter[ve.i0].m_Para.z);
			Vec3 v1 = Vec3(rLMG.m_arrParameter[ve.i1].m_Para.x, rLMG.m_arrParameter[ve.i1].m_Para.y, rLMG.m_arrParameter[ve.i1].m_Para.z);
			Vec3 v2 = Vec3(rLMG.m_arrParameter[ve.i2].m_Para.x, rLMG.m_arrParameter[ve.i2].m_Para.y, rLMG.m_arrParameter[ve.i2].m_Para.z);
			Vec3 v3 = Vec3(rLMG.m_arrParameter[ve.i3].m_Para.x, rLMG.m_arrParameter[ve.i3].m_Para.y, rLMG.m_arrParameter[ve.i3].m_Para.z);
			Vec3 v4 = Vec3(rLMG.m_arrParameter[ve.i4].m_Para.x, rLMG.m_arrParameter[ve.i4].m_Para.y, rLMG.m_arrParameter[ve.i4].m_Para.z);
			Vec3 v5 = Vec3(rLMG.m_arrParameter[ve.i5].m_Para.x, rLMG.m_arrParameter[ve.i5].m_Para.y, rLMG.m_arrParameter[ve.i5].m_Para.z);
			Vec3 v6 = Vec3(rLMG.m_arrParameter[ve.i6].m_Para.x, rLMG.m_arrParameter[ve.i6].m_Para.y, rLMG.m_arrParameter[ve.i6].m_Para.z);
			Vec3 v7 = Vec3(rLMG.m_arrParameter[ve.i7].m_Para.x, rLMG.m_arrParameter[ve.i7].m_Para.y, rLMG.m_arrParameter[ve.i7].m_Para.z);
			Vec3 ip = (v0 * ve.w0 + v1 * ve.w1 + v2 * ve.w2 + v3 * ve.w3 + v4 * ve.w4 + v5 * ve.w5 + v6 * ve.w6 + v7 * ve.w7) * scl + off;
			g_pAuxGeom->DrawOBB(_obb1, ip, 1, RGBA8(0x00, 0x00, 0x7f, 0x00), eBBD_Extremes_Color_Encoded);
		}

		f32 cel0 = f32(rLMG.m_DimPara[0].m_cells - 1);
		f32 fx = (vDesiredParameter.x - rLMG.m_DimPara[0].m_min) / ((rLMG.m_DimPara[0].m_max - rLMG.m_DimPara[0].m_min) / cel0);
		fx = clamp_tpl(fx, 0.0f, cel0 - 0.001f);
		int32 ix = int32(fx);
		//f32 px		=	fx-f32(ix);

		f32 cel1 = f32(rLMG.m_DimPara[1].m_cells - 1);
		f32 fy = (vDesiredParameter.y - rLMG.m_DimPara[1].m_min) / ((rLMG.m_DimPara[1].m_max - rLMG.m_DimPara[1].m_min) / cel1);
		fy = clamp_tpl(fy, 0.0f, cel1 - 0.001f);
		int32 iy = int32(fy);
		//f32 py		=	fy-f32(iy);

		f32 cel2 = f32(rLMG.m_DimPara[2].m_cells - 1);
		f32 fz = (vDesiredParameter.z - rLMG.m_DimPara[2].m_min) / ((rLMG.m_DimPara[2].m_max - rLMG.m_DimPara[2].m_min) / cel2);
		fz = clamp_tpl(fz, 0.0f, cel2 - 0.001f);
		int32 iz = int32(fz);
		//f32 pz		=	fz-f32(iz);

		f32 CellSizeX = (rLMG.m_DimPara[0].m_max - rLMG.m_DimPara[0].m_min) / cel0;
		f32 CellSizeY = (rLMG.m_DimPara[1].m_max - rLMG.m_DimPara[1].m_min) / cel1;
		f32 CellSizeZ = (rLMG.m_DimPara[2].m_max - rLMG.m_DimPara[2].m_min) / cel2;

		Vec3 p0 = Vec3((ix + 0) * CellSizeX + rLMG.m_DimPara[0].m_min, (iy + 0) * CellSizeY + rLMG.m_DimPara[1].m_min, (iz + 0) * CellSizeZ + rLMG.m_DimPara[2].m_min) * scl + off;
		Vec3 p1 = Vec3((ix + 1) * CellSizeX + rLMG.m_DimPara[0].m_min, (iy + 0) * CellSizeY + rLMG.m_DimPara[1].m_min, (iz + 0) * CellSizeZ + rLMG.m_DimPara[2].m_min) * scl + off;
		Vec3 p2 = Vec3((ix + 0) * CellSizeX + rLMG.m_DimPara[0].m_min, (iy + 1) * CellSizeY + rLMG.m_DimPara[1].m_min, (iz + 0) * CellSizeZ + rLMG.m_DimPara[2].m_min) * scl + off;
		Vec3 p3 = Vec3((ix + 1) * CellSizeX + rLMG.m_DimPara[0].m_min, (iy + 1) * CellSizeY + rLMG.m_DimPara[1].m_min, (iz + 0) * CellSizeZ + rLMG.m_DimPara[2].m_min) * scl + off;

		Vec3 p4 = Vec3((ix + 0) * CellSizeX + rLMG.m_DimPara[0].m_min, (iy + 0) * CellSizeY + rLMG.m_DimPara[1].m_min, (iz + 1) * CellSizeZ + rLMG.m_DimPara[2].m_min) * scl + off;
		Vec3 p5 = Vec3((ix + 1) * CellSizeX + rLMG.m_DimPara[0].m_min, (iy + 0) * CellSizeY + rLMG.m_DimPara[1].m_min, (iz + 1) * CellSizeZ + rLMG.m_DimPara[2].m_min) * scl + off;
		Vec3 p6 = Vec3((ix + 0) * CellSizeX + rLMG.m_DimPara[0].m_min, (iy + 1) * CellSizeY + rLMG.m_DimPara[1].m_min, (iz + 1) * CellSizeZ + rLMG.m_DimPara[2].m_min) * scl + off;
		Vec3 p7 = Vec3((ix + 1) * CellSizeX + rLMG.m_DimPara[0].m_min, (iy + 1) * CellSizeY + rLMG.m_DimPara[1].m_min, (iz + 1) * CellSizeZ + rLMG.m_DimPara[2].m_min) * scl + off;

		SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
		renderFlags.SetFillMode(e_FillModeSolid);
		renderFlags.SetAlphaBlendMode(e_AlphaAdditive);
		g_pAuxGeom->SetRenderFlags(renderFlags);
		g_pAuxGeom->DrawTriangle(p0, RGBA8(0x00, 0x7f, 0x00, 0x00), p3, RGBA8(0x00, 0x7f, 0x00, 0x00), p1, RGBA8(0x00, 0x7f, 0x00, 0x00));  //bottom
		g_pAuxGeom->DrawTriangle(p0, RGBA8(0x00, 0x7f, 0x00, 0x00), p2, RGBA8(0x00, 0x7f, 0x00, 0x00), p3, RGBA8(0x00, 0x7f, 0x00, 0x00));

		g_pAuxGeom->DrawTriangle(p4, RGBA8(0x00, 0x9f, 0x00, 0x00), p5, RGBA8(0x00, 0x9f, 0x00, 0x00), p7, RGBA8(0x00, 0x9f, 0x00, 0x00));  //top
		g_pAuxGeom->DrawTriangle(p4, RGBA8(0x00, 0x9f, 0x00, 0x00), p7, RGBA8(0x00, 0x9f, 0x00, 0x00), p6, RGBA8(0x00, 0x9f, 0x00, 0x00));

		g_pAuxGeom->DrawTriangle(p7, RGBA8(0x00, 0xaf, 0x00, 0x00), p5, RGBA8(0x00, 0xaf, 0x00, 0x00), p1, RGBA8(0x00, 0xaf, 0x00, 0x00));  //front
		g_pAuxGeom->DrawTriangle(p7, RGBA8(0x00, 0xaf, 0x00, 0x00), p1, RGBA8(0x00, 0xaf, 0x00, 0x00), p3, RGBA8(0x00, 0xaf, 0x00, 0x00));

		g_pAuxGeom->DrawTriangle(p6, RGBA8(0x00, 0x7f, 0x00, 0x00), p7, RGBA8(0x00, 0x7f, 0x00, 0x00), p3, RGBA8(0x00, 0x7f, 0x00, 0x00));  //side
		g_pAuxGeom->DrawTriangle(p6, RGBA8(0x00, 0x7f, 0x00, 0x00), p3, RGBA8(0x00, 0x7f, 0x00, 0x00), p2, RGBA8(0x00, 0x7f, 0x00, 0x00));

		g_pAuxGeom->DrawTriangle(p4, RGBA8(0x0f, 0x6f, 0x00, 0x00), p6, RGBA8(0x00, 0x6f, 0x00, 0x00), p2, RGBA8(0x00, 0x6f, 0x00, 0x00));  //back
		g_pAuxGeom->DrawTriangle(p4, RGBA8(0x0f, 0x6f, 0x00, 0x00), p2, RGBA8(0x00, 0x6f, 0x00, 0x00), p0, RGBA8(0x00, 0x6f, 0x00, 0x00));

		g_pAuxGeom->DrawTriangle(p5, RGBA8(0x0f, 0x8f, 0x00, 0x00), p4, RGBA8(0x00, 0x8f, 0x00, 0x00), p0, RGBA8(0x00, 0x8f, 0x00, 0x00));  //side
		g_pAuxGeom->DrawTriangle(p5, RGBA8(0x0f, 0x8f, 0x00, 0x00), p0, RGBA8(0x00, 0x8f, 0x00, 0x00), p1, RGBA8(0x00, 0x8f, 0x00, 0x00));

		uint32 mem = rLMG.m_VirtualExampleGrid3D.get_alloc_size();
		g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fCol1111, false, "mem: %d", mem);
		g_YLine += 21.0f;
	}

	SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
	g_pAuxGeom->SetRenderFlags(renderFlags);
	int nDimensions = rLMG.m_Dimensions;
	for (uint32 d = 0; d < nDimensions; d++)
	{
		uint32 initD = m_MotionParameterFlags[d] & CA_Dim_Initialized;
		g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fCol1111, false, "%s: %f %d", rLMG.m_DimPara[d].m_strParaName.c_str(), vDesiredParameter[d], initD);
		g_YLine += 20.0f;
	}
	if (fDebugConfig & 1)
	{
		f32 xstep = (rLMG.m_DimPara[0].m_max - rLMG.m_DimPara[0].m_min) / f32(rLMG.m_DimPara[0].m_cells - 1);
		f32 ystep = (rLMG.m_DimPara[1].m_max - rLMG.m_DimPara[1].m_min) / f32(rLMG.m_DimPara[1].m_cells - 1);
		f32 zstep = (rLMG.m_DimPara[2].m_max - rLMG.m_DimPara[2].m_min) / f32(rLMG.m_DimPara[2].m_cells - 1);
		for (f32 z = rLMG.m_DimPara[2].m_min; z <= (rLMG.m_DimPara[2].m_max + 0.001f); z = z + zstep)
		{
			for (f32 x = rLMG.m_DimPara[0].m_min; x <= (rLMG.m_DimPara[0].m_max + 0.001f); x = x + xstep)
				g_pAuxGeom->DrawLine(Vec3(x, rLMG.m_DimPara[1].m_min, z) * scl + off, RGBA8(0xff, 0x00, 0xff, 0x00), Vec3(x, rLMG.m_DimPara[1].m_max, z) * scl + off, RGBA8(0xff, 0x00, 0xff, 0x00), 1.0f);
			for (f32 y = rLMG.m_DimPara[1].m_min; y <= (rLMG.m_DimPara[1].m_max + 0.001f); y = y + ystep)
				g_pAuxGeom->DrawLine(Vec3(rLMG.m_DimPara[0].m_min, y, z) * scl + off, RGBA8(0xff, 0x00, 0xff, 0x00), Vec3(rLMG.m_DimPara[0].m_max, y, z) * scl + off, RGBA8(0xff, 0x00, 0xff, 0x00), 1.0f);
		}
	}

	static f32 fScaleSphere = 0.0f;
	g_pAuxGeom->DrawSphere(Vec3(vDesiredParameter.x, vDesiredParameter.y, vDesiredParameter.z) * scl + off, 0.05f + fScaleSphere * 0.07f, ColorB(0xff, 0, 0));
	fScaleSphere += 3.5f * g_AverageFrameTime;
	if (fScaleSphere > 1.0f) fScaleSphere -= 1.0f;

	VisualizeBlendSpace(pAnimationSet, rCurAnim, fPlaybackScale, nInstanceOffset, rLMG, off, selFace, fUniScale);
}
#endif

void SParametricSamplerInternal::BlendSpace3D(GlobalAnimationHeaderLMG& rLMG, const CAnimationSet* pAnimationSet, const CDefaultSkeleton* pDefaultSkeleton, const CAnimation& rCurAnim, f32 fFrameDeltaTime, f32 fPlaybackScale, Vec3 off, bool AllowDebug, uint32 nInstanceOffset)
{
	float fCol1111[4] = { 1, 1, 1, 1 };
	float fCol1011[4] = { 1, 0, 1, 1 };
	uint32 nDimensions = rLMG.m_Dimensions;
	uint32 numExamples = rLMG.m_numExamples;

	f32 arrWeights[0xff];
	Vec3 vDesiredParameter = Vec3(0, 0, 0);
	for (uint32 d = 0; d < nDimensions; d++)
	{
		uint32 nParaID = rLMG.m_DimPara[d].m_ParaID;
		if (nParaID > eMotionParamID_COUNT)
			CryFatalError("CryAnimation: nParaID not initialized");
		if (rLMG.m_DimPara[d].m_nInitialized == 0)
			rLMG.ParameterExtraction(pAnimationSet, pDefaultSkeleton, nParaID, d);
		vDesiredParameter[d] = clamp_tpl(m_MotionParameter[d], rLMG.m_DimPara[d].m_min, rLMG.m_DimPara[d].m_max);
	}

	uint32 numParameters = rLMG.m_arrParameter.size();
	for (uint32 i = numExamples; i < numParameters; i++)
	{
		uint32 i0 = rLMG.m_arrParameter[i].i0;
		f32 w0 = rLMG.m_arrParameter[i].w0;
		uint32 i1 = rLMG.m_arrParameter[i].i1;
		f32 w1 = rLMG.m_arrParameter[i].w1;
		f32 sum = w0 + w1;
		assert(fabsf(1.0f - sum) < 0.00001f);
		rLMG.m_arrParameter[i].m_Para = rLMG.m_arrParameter[i0].m_Para * w0 + rLMG.m_arrParameter[i1].m_Para * w1;
	}

	f32 fThreshold = rLMG.m_fThreshold;
	if (fThreshold > -100 && fThreshold < 100)
	{
		f32 tdy = fabsf(vDesiredParameter.y);
		if (tdy > fThreshold)
			vDesiredParameter.z = 0;
		else
			vDesiredParameter.z *= 1.0f - (tdy / fThreshold);
	}

	const Diag33 scl(rLMG.m_DimPara[0].m_scale, rLMG.m_DimPara[1].m_scale, rLMG.m_DimPara[2].m_scale);
	f32 xstep = (rLMG.m_DimPara[0].m_max - rLMG.m_DimPara[0].m_min) / f32(rLMG.m_DimPara[0].m_cells - 1);
	f32 ystep = (rLMG.m_DimPara[1].m_max - rLMG.m_DimPara[1].m_min) / f32(rLMG.m_DimPara[1].m_cells - 1);
	f32 zstep = (rLMG.m_DimPara[2].m_max - rLMG.m_DimPara[2].m_min) / f32(rLMG.m_DimPara[2].m_cells - 1);
	uint32 initialized = rLMG.m_VirtualExampleGrid3D.size();
	if (initialized == 0)
	{
		//precompute the Virtual Example Grid
		rLMG.m_VirtualExampleGrid3D.resize(rLMG.m_DimPara[0].m_cells * rLMG.m_DimPara[1].m_cells * rLMG.m_DimPara[2].m_cells);
		uint32 c2 = 0;
		for (f32 z = rLMG.m_DimPara[2].m_min; z <= (rLMG.m_DimPara[2].m_max + 0.001f); z = z + zstep, c2++)
		{
			uint32 c1 = 0;
			for (f32 y = rLMG.m_DimPara[1].m_min; y <= (rLMG.m_DimPara[1].m_max + 0.001f); y = y + ystep, c1++)
			{
				uint32 c0 = 0;
				for (f32 x = rLMG.m_DimPara[0].m_min; x <= (rLMG.m_DimPara[0].m_max + 0.001f); x = x + xstep, c0++)
				{
					GetWeights3D(Vec3(x, y, z), rLMG, arrWeights, scl, off);
#ifdef _DEBUG
					f32 sum = 0.0f;
					for (uint32 i = 0; i < numExamples; i++)
						sum += arrWeights[i];
					assert(fabsf(sum - 1.0f) < 0.005f);
#endif
					uint32 cell = c2 * rLMG.m_DimPara[1].m_cells * rLMG.m_DimPara[0].m_cells + c1 * rLMG.m_DimPara[0].m_cells + c0;
					rLMG.m_VirtualExampleGrid3D[cell].i0 = 0;
					rLMG.m_VirtualExampleGrid3D[cell].i1 = 0;
					rLMG.m_VirtualExampleGrid3D[cell].i2 = 0;
					rLMG.m_VirtualExampleGrid3D[cell].i3 = 0;
					rLMG.m_VirtualExampleGrid3D[cell].i4 = 0;
					rLMG.m_VirtualExampleGrid3D[cell].i5 = 0;
					rLMG.m_VirtualExampleGrid3D[cell].i6 = 0;
					rLMG.m_VirtualExampleGrid3D[cell].i7 = 0;

					rLMG.m_VirtualExampleGrid3D[cell].w0 = 0.0f;
					rLMG.m_VirtualExampleGrid3D[cell].w1 = 0.0f;
					rLMG.m_VirtualExampleGrid3D[cell].w2 = 0.0f;
					rLMG.m_VirtualExampleGrid3D[cell].w3 = 0.0f;
					rLMG.m_VirtualExampleGrid3D[cell].w4 = 0.0f;
					rLMG.m_VirtualExampleGrid3D[cell].w5 = 0.0f;
					rLMG.m_VirtualExampleGrid3D[cell].w6 = 0.0f;
					rLMG.m_VirtualExampleGrid3D[cell].w7 = 0.0f;

					uint32 c = 0;
					uint8* pI = &(rLMG.m_VirtualExampleGrid3D[cell].i0);
					float* pW = &(rLMG.m_VirtualExampleGrid3D[cell].w0);
					for (uint32 i = 0; i < numExamples; i++)
					{
						f32 w = arrWeights[i];
						if (w)
						{
							if (c > 7)
								break;
							//CryFatalError("Invalid Weights");
							pI[c] = i;
							pW[c] = w;
							c++;
						}
					}
				}
			}
		}
	}

	//---------------------------------------------------------------------------------------------
	//---                   visualize the virtual-examples                                  -------
	//---------------------------------------------------------------------------------------------
	uint32 numVExamples = rLMG.m_VirtualExampleGrid3D.size();
	if (numVExamples)
	{
#if !defined(_RELEASE)
		if (AllowDebug && Console::GetInst().ca_SnapToVGrid)
		{
			f32 fClosestDist = 99999.0f;
			Vec3 vMotionParam = vDesiredParameter;
			uint32 c2 = 0;
			for (f32 z = rLMG.m_DimPara[2].m_min; z <= (rLMG.m_DimPara[2].m_max + 0.001f); z = z + zstep, c2++)
			{
				uint32 c1 = 0;
				for (f32 y = rLMG.m_DimPara[1].m_min; y <= (rLMG.m_DimPara[1].m_max + 0.001f); y = y + ystep, c1++)
				{
					uint32 c0 = 0;
					for (f32 x = rLMG.m_DimPara[0].m_min; x <= (rLMG.m_DimPara[0].m_max + 0.001f); x = x + xstep, c0++)
					{
						f32 cd = (vMotionParam - Vec3(x, y, z)).GetLength();
						if (fClosestDist > cd)
							fClosestDist = cd, vDesiredParameter.x = x, vDesiredParameter.y = y, vDesiredParameter.z = z;
					}
				}
			}
			g_pAuxGeom->Draw2dLabel(1, g_YLine, 3.0f, fCol1111, false, "Snapped to VGrid: %f  %f  %f", vDesiredParameter.x, vDesiredParameter.y, vDesiredParameter.z);
			g_YLine += 31.0f;
		}
#endif

		//---------------------------------------------------------------------------------------------
		//---                   this is the 3D-parameterizer     --------------------------------------
		//---------------------------------------------------------------------------------------------
		f32 cel0 = f32(rLMG.m_DimPara[0].m_cells - 1);
		f32 fx = (vDesiredParameter.x - rLMG.m_DimPara[0].m_min) / ((rLMG.m_DimPara[0].m_max - rLMG.m_DimPara[0].m_min) / cel0);
		fx = clamp_tpl(fx, 0.0f, cel0 - 0.001f);
		int32 ix = int32(fx);
		f32 px = fx - f32(ix);

		f32 cel1 = f32(rLMG.m_DimPara[1].m_cells - 1);
		f32 fy = (vDesiredParameter.y - rLMG.m_DimPara[1].m_min) / ((rLMG.m_DimPara[1].m_max - rLMG.m_DimPara[1].m_min) / cel1);
		fy = clamp_tpl(fy, 0.0f, cel1 - 0.001f);
		int32 iy = int32(fy);
		f32 py = fy - f32(iy);

		f32 cel2 = f32(rLMG.m_DimPara[2].m_cells - 1);
		f32 fz = (vDesiredParameter.z - rLMG.m_DimPara[2].m_min) / ((rLMG.m_DimPara[2].m_max - rLMG.m_DimPara[2].m_min) / cel2);
		fz = clamp_tpl(fz, 0.0f, cel2 - 0.001f);
		int32 iz = int32(fz);
		f32 pz = fz - f32(iz);

		for (uint32 i = 0; i < numExamples; i++)
			arrWeights[i] = 0;

		int32 idx[8] =
		{
			(iz + 0) * rLMG.m_DimPara[1].m_cells * rLMG.m_DimPara[0].m_cells + (iy + 0) * rLMG.m_DimPara[0].m_cells + (ix + 0),
			(iz + 0) * rLMG.m_DimPara[1].m_cells * rLMG.m_DimPara[0].m_cells + (iy + 0) * rLMG.m_DimPara[0].m_cells + (ix + 1),
			(iz + 0) * rLMG.m_DimPara[1].m_cells * rLMG.m_DimPara[0].m_cells + (iy + 1) * rLMG.m_DimPara[0].m_cells + (ix + 0),
			(iz + 0) * rLMG.m_DimPara[1].m_cells * rLMG.m_DimPara[0].m_cells + (iy + 1) * rLMG.m_DimPara[0].m_cells + (ix + 1),
			(iz + 1) * rLMG.m_DimPara[1].m_cells * rLMG.m_DimPara[0].m_cells + (iy + 0) * rLMG.m_DimPara[0].m_cells + (ix + 0),
			(iz + 1) * rLMG.m_DimPara[1].m_cells * rLMG.m_DimPara[0].m_cells + (iy + 0) * rLMG.m_DimPara[0].m_cells + (ix + 1),
			(iz + 1) * rLMG.m_DimPara[1].m_cells * rLMG.m_DimPara[0].m_cells + (iy + 1) * rLMG.m_DimPara[0].m_cells + (ix + 0),
			(iz + 1) * rLMG.m_DimPara[1].m_cells * rLMG.m_DimPara[0].m_cells + (iy + 1) * rLMG.m_DimPara[0].m_cells + (ix + 1)
		};

		for (int i = 0; i < 8; i++)
		{
			PrefetchLine(&rLMG.m_VirtualExampleGrid3D[idx[i]], 0);
		}

		const f32 ew0 = (1.0f - pz) * (1 - px) * (1 - py);
		UpdateWeights(ew0, idx[0], arrWeights, rLMG);

		const f32 ew1 = (1.0f - pz) * px * (1 - py);
		UpdateWeights(ew1, idx[1], arrWeights, rLMG);

		const f32 ew2 = (1.0f - pz) * (1 - px) * py;
		UpdateWeights(ew2, idx[2], arrWeights, rLMG);

		const f32 ew3 = (1.0f - pz) * px * py;
		UpdateWeights(ew3, idx[3], arrWeights, rLMG);

		//---------------------------------------------------------

		const uint32 increment = 128 / sizeof(m_fBlendWeight[0]);
		for (uint32 i = 0; i < numExamples; i += increment)
			PrefetchLine(&m_fBlendWeight[i], 0);

		const f32 ew4 = pz * (1 - px) * (1 - py);
		UpdateWeights(ew4, idx[4], arrWeights, rLMG);

		const f32 ew5 = pz * px * (1 - py);
		UpdateWeights(ew5, idx[5], arrWeights, rLMG);

		const f32 ew6 = pz * (1 - px) * py;
		UpdateWeights(ew6, idx[6], arrWeights, rLMG);

		const f32 ew7 = pz * px * py;
		UpdateWeights(ew7, idx[7], arrWeights, rLMG);
	}

	for (uint32 i = 0; i < numExamples; i++)
	{
		PREFAST_SUPPRESS_WARNING(6001); // gets filled
		m_fBlendWeight[i] = arrWeights[i];
	}

#ifdef BLENDSPACE_VISUALIZATION
	m_vDesiredParameter = vDesiredParameter;

	if (AllowDebug)
	{
		const uint32 fDebugConfig = uint32(floorf(Console::GetInst().ca_DrawVEGInfo));
		float fUniScale = Console::GetInst().ca_DrawVEGInfo - fDebugConfig;
		BlendSpace3DVisualize(rLMG, rCurAnim, pAnimationSet, fPlaybackScale, nInstanceOffset, fDebugConfig, fUniScale);
	}
#endif

} //function PARA

int SParametricSamplerInternal::GetWeights1D(f32 fDesiredParameter, const GlobalAnimationHeaderLMG& rLMG, f32 arrWeights[], Diag33 scl, Vec3 off) const
{
	const uint32 fDebugConfig = uint32(floorf(Console::GetInst().ca_DrawVEGInfo));
	const ColorB faceColor = RGBA8(0xff, 0xff, 0xff, 0x40);

	uint32 numExamples = rLMG.m_numExamples;//m_arrBSAnimations.size();
	for (uint32 i = 0; i < numExamples; i++)
		arrWeights[i] = 0.0f;

	uint32 nInsideFace = 0;
	uint32 numLines = rLMG.m_arrBSAnnotations.size();
	for (uint32 f = 0; f < numLines; f++)
	{
		if (rLMG.m_arrBSAnnotations[f].num == 2)
		{
			int32 i0 = rLMG.m_arrBSAnnotations[f].idx[0];
			int32 i1 = rLMG.m_arrBSAnnotations[f].idx[1];
			f32 x0 = rLMG.m_arrParameter[i0].m_Para.x;
			f32 x1 = rLMG.m_arrParameter[i1].m_Para.x;

#if !defined(_RELEASE)
			if (x0 >= x1)
				g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, 0, "!CryAnimation: motion parameter must be sorted by size in the 1D-Blend-Space. Lowest parameter must come first: %s", rLMG.GetFilePath());
#endif

			if (x0 <= fDesiredParameter && x1 >= fDesiredParameter)
			{
#ifdef BLENDSPACE_VISUALIZATION
				if (fDebugConfig)
				{
					g_pAuxGeom->DrawLine(Vec3(x0, 0.00f, +0.04f) * scl + off, faceColor, Vec3(x1, 0.00f, +0.04f) * scl + off, faceColor, 1.0f);
					g_pAuxGeom->DrawLine(Vec3(x0, 0.00f, -0.04f) * scl + off, faceColor, Vec3(x1, 0.00f, -0.04f) * scl + off, faceColor, 1.0f);
					g_pAuxGeom->DrawLine(Vec3(x0, -0.04f, 0.00f) * scl + off, faceColor, Vec3(x1, -0.04f, 0.00f) * scl + off, faceColor, 1.0f);
					g_pAuxGeom->DrawLine(Vec3(x0, +0.04f, 0.00f) * scl + off, faceColor, Vec3(x1, +0.04f, 0.00f) * scl + off, faceColor, 1.0f);
				}
#endif

				f32 fDistance = x1 - x0;
				assert(fDistance);
				f32 d = fDesiredParameter - x0;
				f32 w0 = 1.0f - d / fDistance;
				f32 w1 = d / fDistance;

				uint32 ex00 = rLMG.m_arrParameter[i0].i0;
				arrWeights[ex00] += rLMG.m_arrParameter[i0].w0 * w0;
				uint32 ex01 = rLMG.m_arrParameter[i0].i1;
				arrWeights[ex01] += rLMG.m_arrParameter[i0].w1 * w0;

				uint32 ex10 = rLMG.m_arrParameter[i1].i0;
				arrWeights[ex10] += rLMG.m_arrParameter[i1].w0 * w1;
				uint32 ex11 = rLMG.m_arrParameter[i1].i1;
				arrWeights[ex11] += rLMG.m_arrParameter[i1].w1 * w1;

#ifdef BLENDSPACE_VISUALIZATION
				if (fDebugConfig & 4)
				{
					float fColDebug6[4] = { 1, 0, 1, 1 };
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fColDebug6, false, "ex00: %d w0: %f", ex00, rLMG.m_arrParameter[i0].w0);
					g_YLine += 21.0f;
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fColDebug6, false, "ex01: %d w1: %f", ex01, rLMG.m_arrParameter[i0].w1);
					g_YLine += 21.0f;
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fColDebug6, false, "ex10: %d w0: %f", ex10, rLMG.m_arrParameter[i1].w0);
					g_YLine += 21.0f;
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fColDebug6, false, "ex11: %d w1: %f", ex11, rLMG.m_arrParameter[i1].w1);
					g_YLine += 21.0f;
				}
#endif
				nInsideFace++;
				return f;
			}
		}
	} //loop over lines

	if (nInsideFace == 0)
	{
		uint32 nLineNo = -1;
		f32 ClosestWeight = 9999.0f;

		for (uint32 f = 0; f < numLines; f++)
		{
			if (rLMG.m_arrBSAnnotations[f].num == 2)
			{
				int32 i0 = rLMG.m_arrBSAnnotations[f].idx[0];
				int32 i1 = rLMG.m_arrBSAnnotations[f].idx[1];
				f32 x0 = rLMG.m_arrParameter[i0].m_Para.x;
				f32 x1 = rLMG.m_arrParameter[i1].m_Para.x;

				f32 fDistance = (x1 - x0);
				assert(fDistance);
				f32 d = fDesiredParameter - x0;
				f32 w0 = 1.0f - d / fDistance;
				f32 w1 = d / fDistance;
				if (w0 < 0.0f)
				{
					if (ClosestWeight > -w0)
						ClosestWeight = -w0, nLineNo = f;
				}
				if (w0 > 1.0f)
				{
					if (ClosestWeight > (w0 - 1.0f))
						ClosestWeight = w0 - 1.0f, nLineNo = f;
				}

				if (w1 < 0.0f)
				{
					if (ClosestWeight > -w1)
						ClosestWeight = -w1, nLineNo = f;
				}
				if (w1 > 1.0f)
				{
					if (ClosestWeight > (w1 - 1.0f))
						ClosestWeight = w1 - 1.0f, nLineNo = f;
				}

			}
		} //loop over lines

		if (nLineNo != -1)
		{
			int32 i0 = rLMG.m_arrBSAnnotations[nLineNo].idx[0];
			int32 i1 = rLMG.m_arrBSAnnotations[nLineNo].idx[1];
			f32 x0 = rLMG.m_arrParameter[i0].m_Para.x;
			f32 x1 = rLMG.m_arrParameter[i1].m_Para.x;
#ifdef BLENDSPACE_VISUALIZATION
			if (fDebugConfig)
			{
				g_pAuxGeom->DrawLine(Vec3(x0, 0.00f, +0.04f) * scl + off, RGBA8(0xff, 0x7f, 0x00, 0x00), Vec3(x1, 0.00f, +0.04f) * scl + off, RGBA8(0xff, 0x7f, 0x00, 0x00), 1.0f);
				g_pAuxGeom->DrawLine(Vec3(x0, 0.00f, -0.04f) * scl + off, RGBA8(0xff, 0x7f, 0x00, 0x00), Vec3(x1, 0.00f, -0.04f) * scl + off, RGBA8(0xff, 0x7f, 0x00, 0x00), 1.0f);
				g_pAuxGeom->DrawLine(Vec3(x0, -0.04f, 0.00f) * scl + off, RGBA8(0xff, 0x7f, 0x00, 0x00), Vec3(x1, -0.04f, 0.00f) * scl + off, RGBA8(0xff, 0x7f, 0x00, 0x00), 1.0f);
				g_pAuxGeom->DrawLine(Vec3(x0, +0.04f, 0.00f) * scl + off, RGBA8(0xff, 0x7f, 0x00, 0x00), Vec3(x1, +0.04f, 0.00f) * scl + off, RGBA8(0xff, 0x7f, 0x00, 0x00), 1.0f);
			}
#endif

			f32 fDistance = x1 - x0;
			assert(fDistance);
			f32 d = fDesiredParameter - x0;
			f32 w0 = 1.0f - d / fDistance;
			f32 w1 = d / fDistance;
			uint32 ex00 = rLMG.m_arrParameter[i0].i0;
			uint32 ex01 = rLMG.m_arrParameter[i0].i1;
			uint32 ex10 = rLMG.m_arrParameter[i1].i0;
			uint32 ex11 = rLMG.m_arrParameter[i1].i1;
			arrWeights[ex00] += rLMG.m_arrParameter[i0].w0 * w0;
			arrWeights[ex01] += rLMG.m_arrParameter[i0].w1 * w0;
			arrWeights[ex10] += rLMG.m_arrParameter[i1].w0 * w1;
			arrWeights[ex11] += rLMG.m_arrParameter[i1].w1 * w1;

#ifdef BLENDSPACE_VISUALIZATION
			if (fDebugConfig & 4)
			{
				float fColDebug6[4] = { 1, 0, 1, 1 };
				g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fColDebug6, false, "ex00: %d w0: %f", ex00, rLMG.m_arrParameter[i0].w0);
				g_YLine += 21.0f;
				g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fColDebug6, false, "ex01: %d w1: %f", ex01, rLMG.m_arrParameter[i0].w1);
				g_YLine += 21.0f;
				g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fColDebug6, false, "ex10: %d w0: %f", ex10, rLMG.m_arrParameter[i1].w0);
				g_YLine += 21.0f;
				g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.0f, fColDebug6, false, "ex11: %d w1: %f", ex11, rLMG.m_arrParameter[i1].w1);
				g_YLine += 21.0f;
			}
#endif
		}
	}

	return -1;
}

int SParametricSamplerInternal::GetWeights2D(const Vec2& vDesiredParameter, const GlobalAnimationHeaderLMG& rLMG, f32 arrWeights[], Diag33 scl, Vec3 off) const
{
	float fCol1111[4] = { 1, 1, 1, 1 };
	uint32 numExamples = rLMG.m_numExamples;//m_arrBSAnimations.size();
	for (uint32 i = 0; i < numExamples; i++)
		arrWeights[i] = 0.0f;

	uint8 i[4];
	Vec3 v[4];
	uint32 numBlocks = rLMG.m_arrBSAnnotations.size();
	for (f32 d = 0.0f; d < 2.35f; d = d + 0.05f)
	{
		for (uint32 b = 0; b < numBlocks; b++)
		{
			uint32 numPoints = rLMG.m_arrBSAnnotations[b].num;
			for (uint32 e = 0; e < numPoints; e++)
			{
				i[e] = rLMG.m_arrBSAnnotations[b].idx[e];
				v[e] = Vec3(rLMG.m_arrParameter[i[e]].m_Para.x, rLMG.m_arrParameter[i[e]].m_Para.y, rLMG.m_arrParameter[i[e]].m_Para.z);
			}

			BC4 bw = GetConvex4(numPoints, vDesiredParameter, rLMG, arrWeights, i, v, scl, off);
			bool InsideHull = true;
			f32* pbw = &bw.w0;
			for (uint32 e = 0; e < numPoints; e++)
			{
				if (pbw[e] < (0.0f - d))
					InsideHull = false;
				if (pbw[e] > (1.0f + d))
					InsideHull = false;
			}

			if (InsideHull)
			{
				if (Console::GetInst().ca_DrawVEGInfo)
				{
					//					g_pAuxGeom->Draw2dLabel( 1,g_YLine, 4.0f, fCol1111, false,"Inside face: %d",b );
					//					g_YLine+=40.0f;
				}
				return b;
			}
		}
	}

	return -1;
}

int SParametricSamplerInternal::GetWeights3D(const Vec3& vDesiredParameter, const GlobalAnimationHeaderLMG& rLMG, f32 arrWeights[], Diag33 scl, Vec3 off) const
{
	float fCol1111[4] = { 1, 1, 1, 1 };
	uint32 numExamples = rLMG.m_numExamples;//m_arrBSAnimations.size();
	for (uint32 i = 0; i < numExamples; i++)
		arrWeights[i] = 0.0f;

	uint8 i[8];
	Vec3 v[8];
	uint32 numBlocks = rLMG.m_arrBSAnnotations.size();
	for (f32 d = 0.0f; d < 2.35f; d = d + 0.05f)
	{
		for (uint32 b = 0; b < numBlocks; b++)
		{
			uint32 numPoints = rLMG.m_arrBSAnnotations[b].num;
			for (uint32 e = 0; e < numPoints; e++)
			{
				i[e] = rLMG.m_arrBSAnnotations[b].idx[e];
				v[e] = Vec3(rLMG.m_arrParameter[i[e]].m_Para.x, rLMG.m_arrParameter[i[e]].m_Para.y, rLMG.m_arrParameter[i[e]].m_Para.z);
			}

			BC8 bw = GetConvex8(numPoints, vDesiredParameter, rLMG, arrWeights, i, v, scl, off);
			bool InsideHull = true;
			f32* pbw = &bw.w0;
			for (uint32 e = 0; e < numPoints; e++)
			{
				if (pbw[e] < (0.0f - d))
					InsideHull = false;
				if (pbw[e] > (1.0f + d))
					InsideHull = false;
			}

			if (InsideHull)
			{
				if (Console::GetInst().ca_DrawVEGInfo)
				{
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 4.0f, fCol1111, false, "Inside Block1: %d", b);
					g_YLine += 40.0f;
				}
				return b;
			}
		}
	}

	return -1;
}

void SParametricSamplerInternal::ComputeWeightExtrapolate4(Vec4& Weight4, const Vec2& P, const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3) const
{
	Vec4 w;
	struct TW3
	{
		static ILINE void Weight3(const Vec2& p, const Vec2& v0, const Vec2& v1, const Vec2& v2, f32& w0, f32& w1, f32& w2, f32& w3)
		{
			w0 = 0;
			w1 = 0;
			w2 = 0;
			w3 = 0;
			Plane plane = Plane::CreatePlane(v0, v1, Vec3(v0.x, v0.y, 1));
			if ((plane | p) <= 0)
			{
				Vec2 e0 = v0 - v2;
				Vec2 e1 = v1 - v2;
				Vec2 p2 = p - v2;
				w0 = p2.x * e1.y - e1.x * p2.y;
				w1 = e0.x * p2.y - p2.x * e0.y;
				w2 = e0.x * e1.y - e1.x * e0.y - w0 - w1;
			}
		}
	};

	TW3::Weight3(P, p1, p3, p0, w.y, w.w, w.x, w.z);
	Weight4 = w;
	TW3::Weight3(P, p3, p1, p2, w.w, w.y, w.z, w.x);
	Weight4 += w;
	TW3::Weight3(P, p2, p0, p1, w.z, w.x, w.y, w.w);
	Weight4 += w;
	TW3::Weight3(P, p0, p2, p3, w.x, w.z, w.w, w.y);
	Weight4 += w;

	Weight4 /= (Weight4.x + Weight4.y + Weight4.z + Weight4.w);
}

BC4 SParametricSamplerInternal::GetConvex4(uint32 numPoints, const Vec2& vDesiredParameter, const GlobalAnimationHeaderLMG& rLMG, f32 arrWeights[], uint8* idx, Vec3* vtx, Diag33 scl, Vec3 off) const
{
	const ColorB faceColor = RGBA8(0xff, 0xff, 0xff, 0x40);
	float fCol1111[4] = { 1, 1, 1, 1 };
	uint32 numExamples = rLMG.m_numExamples;//m_arrBSAnimations.size();
	for (uint32 e = 0; e < numExamples; e++)
		arrWeights[e] = 0.0f;

	uint32 numParameter = rLMG.m_arrParameter.size();
	ColorB col[256];
	Vec3 examples[256];
	for (uint32 p = 0; p < numParameter; p++)
	{
		if (p < numExamples)
			col[p] = RGBA8(0x00, 0x3f, 0x00, 0x00);
		else
			col[p] = RGBA8(0x3f, 0x00, 0x00, 0x00);
	}

	BC4 bw4;
	bw4.w0 = 9999.0f;
	bw4.w1 = 9999.0f;
	bw4.w2 = 9999.0f;
	bw4.w3 = 9999.0f;

	if (numPoints == 3)
	{
		//Triangle
		uint8 i0 = idx[0];
		Vec3 v0 = vtx[0];
		uint8 i1 = idx[1];
		Vec3 v1 = vtx[1];
		uint8 i2 = idx[2];
		Vec3 v2 = vtx[2];

		//calculate the three weight-values using barycentric coordinates
		f32 px = vDesiredParameter.x - v2.x;
		f32 py = vDesiredParameter.y - v2.y;
		Vec3 z0 = v0 - v2;
		Vec3 z1 = v1 - v2;
		f32 u = px * z1.y - py * z1.x;
		f32 v = py * z0.x - px * z0.y;
		f32 w = z0.x * z1.y - z1.x * z0.y - u - v;
		f32 uvw = u + v + w;
		Vec3 bw = fabsf(uvw) > FLT_EPSILON ? Vec3(u, v, w) / uvw : Vec3(u, v, w);

		bw4.w0 = bw.x;
		bw4.w1 = bw.y;
		bw4.w2 = bw.z;
		bw4.w3 = 0;

		f32 wsum = bw4.w0 + bw4.w1 + bw4.w2 + bw4.w3;

		uint32 ex00 = rLMG.m_arrParameter[i0].i0;
		arrWeights[ex00] += rLMG.m_arrParameter[i0].w0 * bw4.w0;
		uint32 ex01 = rLMG.m_arrParameter[i0].i1;
		arrWeights[ex01] += rLMG.m_arrParameter[i0].w1 * bw4.w0;

		uint32 ex10 = rLMG.m_arrParameter[i1].i0;
		arrWeights[ex10] += rLMG.m_arrParameter[i1].w0 * bw4.w1;
		uint32 ex11 = rLMG.m_arrParameter[i1].i1;
		arrWeights[ex11] += rLMG.m_arrParameter[i1].w1 * bw4.w1;

		uint32 ex20 = rLMG.m_arrParameter[i2].i0;
		arrWeights[ex20] += rLMG.m_arrParameter[i2].w0 * bw4.w2;
		uint32 ex21 = rLMG.m_arrParameter[i2].i1;
		arrWeights[ex21] += rLMG.m_arrParameter[i2].w1 * bw4.w2;

		bool r0 = (bw4.w0 >= 0.0f && bw4.w0 <= 1.0f);
		bool r1 = (bw4.w1 >= 0.0f && bw4.w1 <= 1.0f);
		bool r2 = (bw4.w2 >= 0.0f && bw4.w2 <= 1.0f);
		if (r0 && r1 && r2 && Console::GetInst().ca_DrawVEGInfo)
		{
			g_pAuxGeom->DrawLine(v0 * scl + off, faceColor, v1 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v1 * scl + off, faceColor, v2 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v2 * scl + off, faceColor, v0 * scl + off, faceColor, 1.0f);
		}
	}

	if (numPoints == 4)
	{
		//quad
		uint8 i0 = idx[0];
		Vec3 v0 = vtx[0];
		uint8 i1 = idx[1];
		Vec3 v1 = vtx[1];
		uint8 i2 = idx[2];
		Vec3 v2 = vtx[2];
		uint8 i3 = idx[3];
		Vec3 v3 = vtx[3];

		Vec4 bw = Vec4(0, 0, 0, 0);
		ComputeWeightExtrapolate4(bw, vDesiredParameter, Vec2(v0), Vec2(v1), Vec2(v2), Vec2(v3));

		bw4.w0 = bw.x;
		bw4.w1 = bw.y;
		bw4.w2 = bw.z;
		bw4.w3 = bw.w;

		f32 wsum = bw4.w0 + bw4.w1 + bw4.w2 + bw4.w3;

		uint32 ex00 = rLMG.m_arrParameter[i0].i0;
		arrWeights[ex00] += rLMG.m_arrParameter[i0].w0 * bw4.w0;
		uint32 ex01 = rLMG.m_arrParameter[i0].i1;
		arrWeights[ex01] += rLMG.m_arrParameter[i0].w1 * bw4.w0;

		uint32 ex10 = rLMG.m_arrParameter[i1].i0;
		arrWeights[ex10] += rLMG.m_arrParameter[i1].w0 * bw4.w1;
		uint32 ex11 = rLMG.m_arrParameter[i1].i1;
		arrWeights[ex11] += rLMG.m_arrParameter[i1].w1 * bw4.w1;

		uint32 ex20 = rLMG.m_arrParameter[i2].i0;
		arrWeights[ex20] += rLMG.m_arrParameter[i2].w0 * bw4.w2;
		uint32 ex21 = rLMG.m_arrParameter[i2].i1;
		arrWeights[ex21] += rLMG.m_arrParameter[i2].w1 * bw4.w2;

		uint32 ex30 = rLMG.m_arrParameter[i3].i0;
		arrWeights[ex30] += rLMG.m_arrParameter[i3].w0 * bw4.w3;
		uint32 ex31 = rLMG.m_arrParameter[i3].i1;
		arrWeights[ex31] += rLMG.m_arrParameter[i3].w1 * bw4.w3;

		bool r0 = (bw4.w0 >= 0.0f && bw4.w0 <= 1.0f);
		bool r1 = (bw4.w1 >= 0.0f && bw4.w1 <= 1.0f);
		bool r2 = (bw4.w2 >= 0.0f && bw4.w2 <= 1.0f);
		bool r3 = (bw4.w3 >= 0.0f && bw4.w3 <= 1.0f);
		if (r0 && r1 && r2 && r3 && Console::GetInst().ca_DrawVEGInfo)
		{
			g_pAuxGeom->DrawLine(v0 * scl + off, faceColor, v1 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v1 * scl + off, faceColor, v2 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v2 * scl + off, faceColor, v3 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v3 * scl + off, faceColor, v0 * scl + off, faceColor, 1.0f);
		}
	}

	return bw4;
}

BC4 SParametricSamplerInternal::WeightTetrahedron(const Vec3& CP, const Vec3& t0, const Vec3& t1, const Vec3& t2, const Vec3& t3) const
{

	BC4 bc;
	Vec3 n = (t3 - t0) % (t2 - t0);
	f32 m = n * (t1 - t0);
	bc.w0 = (((t2 - t1) % (t3 - t1)) | (CP - t0)) / m + 1;
	bc.w1 = (((t0 - t2) % (t3 - t2)) | (CP - t1)) / m + 1;
	bc.w2 = (((t0 - t3) % (t1 - t3)) | (CP - t2)) / m + 1;
	bc.w3 = (((t2 - t0) % (t1 - t0)) | (CP - t3)) / m + 1;
	f32 sum = bc.w0 + bc.w1 + bc.w2 + bc.w3;
	assert(fabsf(sum - 1.0f) < 0.0001f);
	return bc;
}

BC5 SParametricSamplerInternal::WeightPyramid(const Vec3& ControlPoint, const Vec3& t0, const Vec3& t1, const Vec3& t2, const Vec3& t3, const Vec3& t4) const
{

	f32 w[5];
	w[0] = 0.0f;
	w[1] = 0.0f;
	w[2] = 0.0f;
	w[3] = 0.0f;
	w[4] = 0.0f;
	Vec3 t[5];
	t[0] = t0;
	t[1] = t1;
	t[2] = t2;
	t[3] = t3;
	t[4] = t4;
	for (uint32 e = 0; e < 4; e++)
	{
		int32 i0 = (e + 0) & 3;
		int32 i1 = (e + 1) & 3;
		int32 i2 = (e + 2) & 3;
		int32 i3 = 4;
		BC4 bc = WeightTetrahedron(ControlPoint, t[i0], t[i1], t[i2], t[i3]);
		if (bc.w1 >= 0)
			w[i0] += bc.w0, w[i1] += bc.w1, w[i2] += bc.w2, w[i3] += bc.w3;
	}

	f32 sum = w[0] + w[1] + w[2] + w[3] + w[4];
	if (sum)
		w[0] /= sum, w[1] /= sum, w[2] /= sum, w[3] /= sum, w[4] /= sum;

	BC5 bc5;
	bc5.w0 = w[0];
	bc5.w1 = w[1];
	bc5.w2 = w[2];
	bc5.w3 = w[3];
	bc5.w4 = w[4];
	return bc5;
}

BC6 SParametricSamplerInternal::WeightPrism(const Vec3& ControlPoint, const Vec3& t0, const Vec3& t1, const Vec3& t2, const Vec3& t3, const Vec3& t4, const Vec3& t5) const
{
	f32 w[6];
	w[0] = 0.0f;
	w[1] = 0.0f;
	w[2] = 0.0f;
	w[3] = 0.0f;
	w[4] = 0.0f;
	w[5] = 0.0f;

	Vec3 t[6];
	t[0] = t0;
	t[1] = t1;
	t[2] = t2;
	t[3] = t3;
	t[4] = t4;
	t[5] = t5;

	int32 i0 = 0;
	int32 i1 = 1;
	int32 i2 = 2;
	int32 i3 = 3;
	int32 i4 = 5;
	Plane plane2 = Plane::CreatePlane(t[i0], t[i1], t[i4]);
	if ((plane2 | ControlPoint) <= 0)
	{
		BC5 bc = WeightPyramid(ControlPoint, t[i0], t[i1], t[i2], t[i3], t[i4]);
		w[i0] += bc.w0;
		w[i1] += bc.w1;
		w[i2] += bc.w2;
		w[i3] += bc.w3;
		w[i4] += bc.w4;
	}

	i0 = 0;
	i1 = 1;
	i2 = 5;
	i3 = 4;
	Plane plane3 = Plane::CreatePlane(t[i1], t[i0], t[i2]);
	if ((plane3 | ControlPoint) <= 0)
	{
		BC4 bc = WeightTetrahedron(ControlPoint, t[i0], t[i1], t[i2], t[i3]);
		w[i0] += bc.w0;
		w[i1] += bc.w1;
		w[i2] += bc.w2;
		w[i3] += bc.w3;
	}

	f32 sum = w[0] + w[1] + w[2] + w[3] + w[4] + w[5];
	if (sum)
		w[0] /= sum, w[1] /= sum, w[2] /= sum, w[3] /= sum, w[4] /= sum, w[5] /= sum;

	BC6 bc6;
	bc6.w0 = w[0];
	bc6.w1 = w[1];
	bc6.w2 = w[2];
	bc6.w3 = w[3];
	bc6.w4 = w[4];
	bc6.w5 = w[5];
	return bc6;
}

BC8 SParametricSamplerInternal::GetConvex8(uint32 numPoints, const Vec3& vDesiredParameter, const GlobalAnimationHeaderLMG& rLMG, f32 arrWeights[], uint8* i, Vec3* v, Diag33 scl, Vec3 off) const
{
	const ColorB faceColor = RGBA8(0xff, 0xff, 0xff, 0xa0);
	float fCol1111[4] = { 1, 1, 1, 1 };
	uint32 numExamples = rLMG.m_numExamples;//m_arrBSAnimations.size();
	for (uint32 e = 0; e < numExamples; e++)
		arrWeights[e] = 0.0f;

	uint32 numParameter = rLMG.m_arrParameter.size();
	ColorB col[0xff];
	Vec3 examples[0xff];
	for (uint32 p = 0; p < numParameter; p++)
	{
		if (p < numExamples)
			col[p] = RGBA8(0x00, 0x3f, 0x00, 0x00);
		else
			col[p] = RGBA8(0x3f, 0x00, 0x00, 0x00);
	}

	BC8 bw;
	bw.w0 = 9999.0f;
	bw.w1 = 9999.0f;
	bw.w2 = 9999.0f;
	bw.w3 = 9999.0f;
	bw.w4 = 9999.0f;
	bw.w5 = 9999.0f;
	bw.w6 = 9999.0f;
	bw.w7 = 9999.0f;

	if (numPoints == 4)
	{
		//Tetrahedron
		uint8 i0 = i[0];
		Vec3 v0 = v[0];
		uint8 i1 = i[1];
		Vec3 v1 = v[1];
		uint8 i2 = i[2];
		Vec3 v2 = v[2];
		uint8 i3 = i[3];
		Vec3 v3 = v[3];

		uint32 pl0 = fabsf(v0.z) < 0.01f;
		uint32 pl1 = fabsf(v1.z) < 0.01f;
		uint32 pl2 = fabsf(v2.z) < 0.01f;
		uint32 pl3 = fabsf(v3.z) < 0.01f;
		if (pl0 && pl1 && pl2 && pl3)
			return bw;

		BC4 bw4 = WeightTetrahedron(vDesiredParameter, v0, v1, v2, v3);

		bw.w0 = bw4.w0;
		bw.w1 = bw4.w1;
		bw.w2 = bw4.w2;
		bw.w3 = bw4.w3;
		bw.w4 = 0;
		bw.w5 = 0;
		bw.w6 = 0;
		bw.w7 = 0;
		//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 2.0f, fCol1111, false,"3d-tetrahedron: %f %f %f %f",bw.w0,bw.w1,bw.w2,bw.w3 );
		//	g_YLine+=20.0f;

		f32 wsum = bw.w0 + bw.w1 + bw.w2 + bw.w3 + bw.w4 + bw.w5 + bw.w6 + bw.w7;

		uint32 ex00 = rLMG.m_arrParameter[i0].i0;
		arrWeights[ex00] += rLMG.m_arrParameter[i0].w0 * bw.w0;
		uint32 ex01 = rLMG.m_arrParameter[i0].i1;
		arrWeights[ex01] += rLMG.m_arrParameter[i0].w1 * bw.w0;

		uint32 ex10 = rLMG.m_arrParameter[i1].i0;
		arrWeights[ex10] += rLMG.m_arrParameter[i1].w0 * bw.w1;
		uint32 ex11 = rLMG.m_arrParameter[i1].i1;
		arrWeights[ex11] += rLMG.m_arrParameter[i1].w1 * bw.w1;

		uint32 ex20 = rLMG.m_arrParameter[i2].i0;
		arrWeights[ex20] += rLMG.m_arrParameter[i2].w0 * bw.w2;
		uint32 ex21 = rLMG.m_arrParameter[i2].i1;
		arrWeights[ex21] += rLMG.m_arrParameter[i2].w1 * bw.w2;

		uint32 ex30 = rLMG.m_arrParameter[i3].i0;
		arrWeights[ex30] += rLMG.m_arrParameter[i3].w0 * bw.w3;
		uint32 ex31 = rLMG.m_arrParameter[i3].i1;
		arrWeights[ex31] += rLMG.m_arrParameter[i3].w1 * bw.w3;

		bool r0 = (bw4.w0 >= 0.0f && bw4.w0 <= 1.0f);
		bool r1 = (bw4.w1 >= 0.0f && bw4.w1 <= 1.0f);
		bool r2 = (bw4.w2 >= 0.0f && bw4.w2 <= 1.0f);
		bool r3 = (bw4.w3 >= 0.0f && bw4.w3 <= 1.0f);
		if (r0 && r1 && r2 && r3 && Console::GetInst().ca_DrawVEGInfo)
		{
			g_pAuxGeom->DrawLine(v0 * scl + off, faceColor, v1 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v1 * scl + off, faceColor, v2 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v2 * scl + off, faceColor, v0 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v0 * scl + off, faceColor, v3 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v1 * scl + off, faceColor, v3 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v2 * scl + off, faceColor, v3 * scl + off, faceColor, 1.0f);
		}
	}

	if (numPoints == 5)
	{
		//Pyramid
		uint8 i0 = i[0];
		Vec3 v0 = v[0];
		uint8 i1 = i[1];
		Vec3 v1 = v[1];
		uint8 i2 = i[2];
		Vec3 v2 = v[2];
		uint8 i3 = i[3];
		Vec3 v3 = v[3];
		uint8 i4 = i[4];
		Vec3 v4 = v[4];

		uint32 pl0 = fabsf(v0.z) < 0.01f;
		uint32 pl1 = fabsf(v1.z) < 0.01f;
		uint32 pl2 = fabsf(v2.z) < 0.01f;
		uint32 pl3 = fabsf(v3.z) < 0.01f;
		uint32 pl4 = fabsf(v4.z) < 0.01f;
		if (pl0 && pl1 && pl2 && pl3 && pl4)
			return bw; //Something went wrong. This is a plane. we need a volume

		BC5 bw5 = WeightPyramid(vDesiredParameter, v0, v1, v2, v3, v4);

		bw.w0 = bw5.w0;
		bw.w1 = bw5.w1;
		bw.w2 = bw5.w2;
		bw.w3 = bw5.w3;
		bw.w4 = bw5.w4;
		bw.w5 = 0;
		bw.w6 = 0;
		bw.w7 = 0;
		//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 2.0f, fCol1111, false,"3d-pyramid: %f %f %f %f %f",bw.w0,bw.w1,bw.w2,bw.w3,bw.w4 );
		//	g_YLine+=20.0f;

		f32 wsum = bw.w0 + bw.w1 + bw.w2 + bw.w3 + bw.w4 + bw.w5 + bw.w6 + bw.w7;

		uint32 ex00 = rLMG.m_arrParameter[i0].i0;
		arrWeights[ex00] += rLMG.m_arrParameter[i0].w0 * bw.w0;
		uint32 ex01 = rLMG.m_arrParameter[i0].i1;
		arrWeights[ex01] += rLMG.m_arrParameter[i0].w1 * bw.w0;

		uint32 ex10 = rLMG.m_arrParameter[i1].i0;
		arrWeights[ex10] += rLMG.m_arrParameter[i1].w0 * bw.w1;
		uint32 ex11 = rLMG.m_arrParameter[i1].i1;
		arrWeights[ex11] += rLMG.m_arrParameter[i1].w1 * bw.w1;

		uint32 ex20 = rLMG.m_arrParameter[i2].i0;
		arrWeights[ex20] += rLMG.m_arrParameter[i2].w0 * bw.w2;
		uint32 ex21 = rLMG.m_arrParameter[i2].i1;
		arrWeights[ex21] += rLMG.m_arrParameter[i2].w1 * bw.w2;

		uint32 ex30 = rLMG.m_arrParameter[i3].i0;
		arrWeights[ex30] += rLMG.m_arrParameter[i3].w0 * bw.w3;
		uint32 ex31 = rLMG.m_arrParameter[i3].i1;
		arrWeights[ex31] += rLMG.m_arrParameter[i3].w1 * bw.w3;

		uint32 ex40 = rLMG.m_arrParameter[i4].i0;
		arrWeights[ex40] += rLMG.m_arrParameter[i4].w0 * bw.w4;
		uint32 ex41 = rLMG.m_arrParameter[i4].i1;
		arrWeights[ex41] += rLMG.m_arrParameter[i4].w1 * bw.w4;

		bool r0 = (bw5.w0 >= 0.0f && bw5.w0 <= 1.0f);
		bool r1 = (bw5.w1 >= 0.0f && bw5.w1 <= 1.0f);
		bool r2 = (bw5.w2 >= 0.0f && bw5.w2 <= 1.0f);
		bool r3 = (bw5.w3 >= 0.0f && bw5.w3 <= 1.0f);
		bool r4 = (bw5.w4 >= 0.0f && bw5.w4 <= 1.0f);
		if (r0 && r1 && r2 && r3 && r4 && Console::GetInst().ca_DrawVEGInfo)
		{
			g_pAuxGeom->DrawLine(v0 * scl + off, faceColor, v1 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v1 * scl + off, faceColor, v2 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v2 * scl + off, faceColor, v3 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v3 * scl + off, faceColor, v0 * scl + off, faceColor, 1.0f);

			g_pAuxGeom->DrawLine(v0 * scl + off, faceColor, v4 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v1 * scl + off, faceColor, v4 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v2 * scl + off, faceColor, v4 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v3 * scl + off, faceColor, v4 * scl + off, faceColor, 1.0f);
		}
	}

	if (numPoints == 6)
	{
		//Pyramid
		uint8 i0 = i[0];
		Vec3 v0 = v[0];
		uint8 i1 = i[1];
		Vec3 v1 = v[1];
		uint8 i2 = i[2];
		Vec3 v2 = v[2];
		uint8 i3 = i[3];
		Vec3 v3 = v[3];
		uint8 i4 = i[4];
		Vec3 v4 = v[4];
		uint8 i5 = i[5];
		Vec3 v5 = v[5];

		uint32 pl0 = fabsf(v0.z) < 0.01f;
		uint32 pl1 = fabsf(v1.z) < 0.01f;
		uint32 pl2 = fabsf(v2.z) < 0.01f;
		uint32 pl3 = fabsf(v3.z) < 0.01f;
		uint32 pl4 = fabsf(v4.z) < 0.01f;
		uint32 pl5 = fabsf(v5.z) < 0.01f;
		if (pl0 && pl1 && pl2 && pl3 && pl4)
			return bw;

		BC6 bw6 = WeightPrism(vDesiredParameter, v0, v1, v2, v3, v4, v5);

		bw.w0 = bw6.w0;
		bw.w1 = bw6.w1;
		bw.w2 = bw6.w2;
		bw.w3 = bw6.w3;
		bw.w4 = bw6.w4;
		bw.w5 = bw6.w5;
		bw.w6 = 0;
		bw.w7 = 0;
		//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 2.0f, fCol1111, false,"3d-pyramid: %f %f %f %f %f",bw.w0,bw.w1,bw.w2,bw.w3,bw.w4 );
		//	g_YLine+=20.0f;

		f32 wsum = bw.w0 + bw.w1 + bw.w2 + bw.w3 + bw.w4 + bw.w5 + bw.w6 + bw.w7;

		uint32 ex00 = rLMG.m_arrParameter[i0].i0;
		arrWeights[ex00] += rLMG.m_arrParameter[i0].w0 * bw.w0;
		uint32 ex01 = rLMG.m_arrParameter[i0].i1;
		arrWeights[ex01] += rLMG.m_arrParameter[i0].w1 * bw.w0;

		uint32 ex10 = rLMG.m_arrParameter[i1].i0;
		arrWeights[ex10] += rLMG.m_arrParameter[i1].w0 * bw.w1;
		uint32 ex11 = rLMG.m_arrParameter[i1].i1;
		arrWeights[ex11] += rLMG.m_arrParameter[i1].w1 * bw.w1;

		uint32 ex20 = rLMG.m_arrParameter[i2].i0;
		arrWeights[ex20] += rLMG.m_arrParameter[i2].w0 * bw.w2;
		uint32 ex21 = rLMG.m_arrParameter[i2].i1;
		arrWeights[ex21] += rLMG.m_arrParameter[i2].w1 * bw.w2;

		uint32 ex30 = rLMG.m_arrParameter[i3].i0;
		arrWeights[ex30] += rLMG.m_arrParameter[i3].w0 * bw.w3;
		uint32 ex31 = rLMG.m_arrParameter[i3].i1;
		arrWeights[ex31] += rLMG.m_arrParameter[i3].w1 * bw.w3;

		uint32 ex40 = rLMG.m_arrParameter[i4].i0;
		arrWeights[ex40] += rLMG.m_arrParameter[i4].w0 * bw.w4;
		uint32 ex41 = rLMG.m_arrParameter[i4].i1;
		arrWeights[ex41] += rLMG.m_arrParameter[i4].w1 * bw.w4;

		uint32 ex50 = rLMG.m_arrParameter[i5].i0;
		arrWeights[ex50] += rLMG.m_arrParameter[i5].w0 * bw.w5;
		uint32 ex51 = rLMG.m_arrParameter[i5].i1;
		arrWeights[ex51] += rLMG.m_arrParameter[i5].w1 * bw.w5;

		bool r0 = (bw6.w0 >= 0.0f && bw6.w0 <= 1.0f);
		bool r1 = (bw6.w1 >= 0.0f && bw6.w1 <= 1.0f);
		bool r2 = (bw6.w2 >= 0.0f && bw6.w2 <= 1.0f);
		bool r3 = (bw6.w3 >= 0.0f && bw6.w3 <= 1.0f);
		bool r4 = (bw6.w4 >= 0.0f && bw6.w4 <= 1.0f);
		bool r5 = (bw6.w5 >= 0.0f && bw6.w5 <= 1.0f);
		if (r0 && r1 && r2 && r3 && r4 && r5 && Console::GetInst().ca_DrawVEGInfo)
		{
			g_pAuxGeom->DrawLine(v0 * scl + off, faceColor, v1 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v1 * scl + off, faceColor, v2 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v2 * scl + off, faceColor, v3 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v3 * scl + off, faceColor, v0 * scl + off, faceColor, 1.0f);

			g_pAuxGeom->DrawLine(v0 * scl + off, faceColor, v4 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v1 * scl + off, faceColor, v4 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v2 * scl + off, faceColor, v5 * scl + off, faceColor, 1.0f);
			g_pAuxGeom->DrawLine(v3 * scl + off, faceColor, v5 * scl + off, faceColor, 1.0f);

			g_pAuxGeom->DrawLine(v4 * scl + off, faceColor, v5 * scl + off, faceColor, 1.0f);
		}
	}

	return bw;
}

void SParametricSamplerInternal::CombinedBlendSpaces(GlobalAnimationHeaderLMG& rMasterParaGroup, const CAnimationSet* pAnimationSet, const CDefaultSkeleton* pDefaultSkeleton, const CAnimation& rCurAnim, f32 fFrameDeltaTime, f32 fPlaybackScale, bool AllowDebug)
{
	float fCol1111[4] = { 1, 1, 1, 1 };
	PrefetchLine(m_nLMGAnimIdx, 0);
	Vec3 off[2];
	off[0] = Vec3(fRenderPosBS1, 0, 0.01f);
	off[1] = Vec3(fRenderPosBS2, 0, 0.01f);

	memset(m_fBlendWeight, 0, sizeof(m_fBlendWeight));

	//--- Filter the used bspace based on any ChooseBlendSpace flags,
	//--- In the case of multiple bspaces matching the criteria the first will be used
	//--- If none match then it will use the closest option
	uint32 numBlendSpaces = rMasterParaGroup.m_arrCombinedBlendSpaces.size();
	uint32 viableBSpaces = 0xffffffff;
	for (uint32 s = 0; s < rMasterParaGroup.m_Dimensions; s++)
	{
		if (rMasterParaGroup.m_DimPara[s].m_ChooseBlendSpace)
		{
			float bestDist = FLT_MAX;
			uint32 blendSpaceMask = 0;
			uint32 closestBSpaceMask = 0;
			uint32 bSpaceMask = 1;
			const bool isAngular = (m_MotionParameterID[s] == eMotionParamID_TravelAngle) || (m_MotionParameterID[s] == eMotionParamID_TurnAngle);
			for (uint32 bs = 0; bs < numBlendSpaces; bs++, bSpaceMask <<= 1)
			{
				int32 nGlobalID = rMasterParaGroup.m_arrCombinedBlendSpaces[bs].m_ParaGroupID;

				if (nGlobalID >= 0)
				{
					GlobalAnimationHeaderLMG& rLMG = g_AnimationManager.m_arrGlobalLMG[nGlobalID];
					assert(rLMG.IsAssetLMG());                                                                                   //--- probably data mismatch

					for (uint32 d = 0; (d < rLMG.m_Dimensions) && (bestDist > 0.0f); d++)
					{
						if (rLMG.m_DimPara[d].m_ParaID == m_MotionParameterID[s])
						{
							float distMin = rLMG.m_DimPara[d].m_min - m_MotionParameter[s];
							float distMax = m_MotionParameter[s] - rLMG.m_DimPara[d].m_max;
							if (isAngular)
							{
								distMin = AngleWrap_PI(distMin);
								distMax = AngleWrap_PI(distMax);
							}
							float dist = max(distMin, distMax);

							if (dist < bestDist)
							{
								closestBSpaceMask = bSpaceMask;
								bestDist = dist;
							}
						}
					}
				}
			}

			viableBSpaces &= closestBSpaceMask;
		}
	}

	uint32 nInstanceOffset = 0;
	uint32 exampleIdx = 0;
	for (uint32 bs = 0; bs < numBlendSpaces; bs++)
	{
		int32 nGlobalID = rMasterParaGroup.m_arrCombinedBlendSpaces[bs].m_ParaGroupID;
		if (nGlobalID >= 0)
		{
			GlobalAnimationHeaderLMG& rLMG = g_AnimationManager.m_arrGlobalLMG[nGlobalID];
			assert(rLMG.IsAssetLMG());                                                                                   //--- probably data mismatch

			PrefetchLine(&rLMG, offsetof(GlobalAnimationHeaderLMG, m_numExamples));

			if ((viableBSpaces & BIT(bs)) == 0)
			{
				exampleIdx += rLMG.m_numExamples;
				continue;
			}

			SParametricSamplerInternal psi;
			psi.m_numExamples = rLMG.m_numExamples;
			for (uint32 i = 0; i < psi.m_numExamples; i++)
			{
				const uint16 idx = m_nLMGAnimIdx[exampleIdx + i];
				psi.m_nAnimID[i] = m_nAnimID[idx];
				psi.m_nSegmentCounter[0][i] = m_nSegmentCounter[0][idx];
			}
			for (uint32 d = 0; d < rLMG.m_Dimensions; d++)
			{
				for (uint32 s = 0; s < rMasterParaGroup.m_Dimensions; s++)
				{
					if (rLMG.m_DimPara[d].m_ParaID == m_MotionParameterID[s])
					{
						psi.m_MotionParameterID[d] = m_MotionParameterID[s];
						psi.m_MotionParameter[d] = m_MotionParameter[s] * rMasterParaGroup.m_DimPara[s].m_ParaScale;
						psi.m_MotionParameterFlags[d] = m_MotionParameterFlags[s];
						break;
					}
				}
			}
			if (rLMG.m_Dimensions == 1)
				psi.BlendSpace1D(rLMG, pAnimationSet, pDefaultSkeleton, rCurAnim, fFrameDeltaTime, fPlaybackScale, off[bs], AllowDebug, nInstanceOffset);
			if (rLMG.m_Dimensions == 2)
				psi.BlendSpace2D(rLMG, pAnimationSet, pDefaultSkeleton, rCurAnim, fFrameDeltaTime, fPlaybackScale, off[bs], AllowDebug, nInstanceOffset);
			if (rLMG.m_Dimensions == 3)
				psi.BlendSpace3D(rLMG, pAnimationSet, pDefaultSkeleton, rCurAnim, fFrameDeltaTime, fPlaybackScale, off[bs], AllowDebug, nInstanceOffset);
			for (uint32 i = 0; i < psi.m_numExamples; i++)
				m_fBlendWeight[m_nLMGAnimIdx[exampleIdx + i]] += psi.m_fBlendWeight[i];

			exampleIdx += rLMG.m_numExamples;

			uint32 numParameter = rLMG.m_arrParameter.size();
			nInstanceOffset += numParameter;
		}
	}
}

void DrawPoly(Vec3* pts, uint32 numPts, bool wireframe, ColorB color, ColorB backcolor = ColorB(0, 0, 0, 0), bool drawBackface = false)
{
	if (wireframe)
	{
		for (uint32 i = 0; i < numPts - 1; i++)
		{
			g_pAuxGeom->DrawLine(pts[i], color, pts[i + 1], color);
		}
		g_pAuxGeom->DrawLine(pts[0], color, pts[numPts - 1], color);
	}
	else
	{
		const uint32 numTris = numPts - 2;
		for (uint32 t = 0; t < numTris; t++)
		{
			g_pAuxGeom->DrawTriangle(pts[0], color, pts[t + 1], color, pts[t + 2], color);
			if (drawBackface)
			{
				g_pAuxGeom->DrawTriangle(pts[0], backcolor, pts[t + 2], backcolor, pts[t + 1], backcolor);
			}
		}
	}
}

#if BLENDSPACE_VISUALIZATION
void SParametricSamplerInternal::VisualizeBlendSpace(const IAnimationSet* _pAnimationSet, const CAnimation& rCurAnim, f32 fPlaybackScale, uint32 nInstanceOffset, const GlobalAnimationHeaderLMG& rLMG, Vec3 off, int selectedFace, float fUniScale) const
{
	const CAnimationSet* pAnimationSet = (const CAnimationSet*)_pAnimationSet;
	uint32 numDebugInstances = g_pCharacterManager->m_arrCharacterBase.size();

	const f32 fAnimTime = rCurAnim.GetCurrentSegmentNormalizedTime();
	const int nEOC = rCurAnim.GetEndOfCycle();        //value from the last frame

	uint32 nDimensions = rLMG.m_Dimensions;
	if (nDimensions > 3)
		return;

	assert(nDimensions > 0 && nDimensions < 4);
	uint32 numExamples = rLMG.m_numExamples; //m_arrBSAnimations2.size();
	uint32 numParameter = rLMG.m_arrParameter.size();
	if (numDebugInstances < numParameter)
		return;

	Diag33 scl(1.0f);
	if (nDimensions == 1)
		scl.x = rLMG.m_DimPara[0].m_scale;
	if (nDimensions == 2)
		scl.x = rLMG.m_DimPara[0].m_scale, scl.y = rLMG.m_DimPara[1].m_scale;
	if (nDimensions == 3)
		scl.x = rLMG.m_DimPara[0].m_scale, scl.y = rLMG.m_DimPara[1].m_scale, scl.z = rLMG.m_DimPara[2].m_scale;

	//--- Loop through and draw the VEG faces
	SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
	renderFlags.SetAlphaBlendMode(e_AlphaBlended);
	renderFlags.SetDepthWriteFlag(e_DepthWriteOff);
	g_pAuxGeom->SetRenderFlags(renderFlags);
	static ColorB unselFaceColor = RGBA8(0x00, 0xff, 0x00, 0x30);
	static ColorB errorFaceColor = RGBA8(0xff, 0x00, 0x00, 0x40);
	static ColorB selFaceColor = RGBA8(0xff, 0xff, 0x00, 0x40);

	const uint32 numFaces = rLMG.m_arrBSAnnotations.size();
	Vec3 facePts[8];

	for (uint32 passes = 0; passes < 2; passes++)
	{
		for (uint32 f = 0; f < numFaces; f++)
		{
			const ColorB faceColor = (selectedFace == f) ? selFaceColor : unselFaceColor;

			const BSBlendable& face = rLMG.m_arrBSAnnotations[f];

			if (face.num < 3)
			{
				continue;
			}

			for (uint32 i = 0; i < face.num; i++)
			{
				const Vec3 pt(rLMG.m_arrParameter[face.idx[i]].m_Para.x, rLMG.m_arrParameter[face.idx[i]].m_Para.y, rLMG.m_arrParameter[face.idx[i]].m_Para.z);
				facePts[i] = pt * scl + off;
			}

			if (nDimensions > 2)
			{
				switch (face.num)
				{
				case 4:
					//--- Tetrahedron
					g_pAuxGeom->DrawTriangle(facePts[3], faceColor, facePts[0], faceColor, facePts[1], faceColor);
					g_pAuxGeom->DrawTriangle(facePts[3], faceColor, facePts[1], faceColor, facePts[2], faceColor);
					g_pAuxGeom->DrawTriangle(facePts[3], faceColor, facePts[2], faceColor, facePts[0], faceColor);
					g_pAuxGeom->DrawTriangle(facePts[0], faceColor, facePts[1], faceColor, facePts[2], faceColor);
					break;
				case 5:
					{
						Plane basePlane;
						basePlane.SetPlane(facePts[0], facePts[1], facePts[2]);
						bool isInvalid = (basePlane.DistFromPlane(facePts[4]) < 0.0f);
						const ColorB drawColor = isInvalid ? errorFaceColor : faceColor;

						//--- Pyramid
						g_pAuxGeom->DrawTriangle(facePts[4], drawColor, facePts[0], drawColor, facePts[1], drawColor);
						g_pAuxGeom->DrawTriangle(facePts[4], drawColor, facePts[1], drawColor, facePts[2], drawColor);
						g_pAuxGeom->DrawTriangle(facePts[4], drawColor, facePts[2], drawColor, facePts[3], drawColor);
						g_pAuxGeom->DrawTriangle(facePts[4], drawColor, facePts[3], drawColor, facePts[0], drawColor);
						DrawPoly(facePts, 4, (passes == 1), drawColor);
					}
					break;
				case 6:
					{
						Plane basePlane;
						basePlane.SetPlane(facePts[0], facePts[1], facePts[2]);
						bool isInvalid = (basePlane.DistFromPlane(facePts[4]) < 0.0f) || (basePlane.DistFromPlane(facePts[5]) < 0.0f);
						const ColorB drawColor = isInvalid ? errorFaceColor : faceColor;

						//--- Prism
						g_pAuxGeom->DrawTriangle(facePts[0], drawColor, facePts[1], drawColor, facePts[4], drawColor);
						g_pAuxGeom->DrawTriangle(facePts[2], drawColor, facePts[3], drawColor, facePts[5], drawColor);

						Vec3 quad[4] = { facePts[4], facePts[1], facePts[2], facePts[5] };
						Vec3 quad2[4] = { facePts[3], facePts[0], facePts[4], facePts[5] };
						DrawPoly(quad, 4, (passes == 1), drawColor);
						DrawPoly(quad2, 4, (passes == 1), drawColor);
						DrawPoly(facePts, 4, (passes == 1), drawColor);
					}
					break;
				default:
					DrawPoly(facePts, face.num, (passes == 1), errorFaceColor);
					break;
				}
			}
			else
			{
				DrawPoly(facePts, face.num, (passes == 1), faceColor, errorFaceColor, true);
			}
		}

		renderFlags.SetFillMode(e_FillModeWireframe);
		renderFlags.SetCullMode(e_CullModeNone);
		g_pAuxGeom->SetRenderFlags(renderFlags);
	}

	if (fUniScale == 0)
		return;

	static Ang3 angles = Ang3(0, 0, 0);
	angles += Ang3(0.01f, 0.02f, 0.03f);
	AABB aabb = AABB(Vec3(-0.05f, -0.05f, -0.05f), Vec3(0.05f, 0.05f, 0.05f));

	ColorB col[256];
	Matrix33 _m33 = Matrix33::CreateRotationXYZ(angles);
	OBB _obb = OBB::CreateOBBfromAABB(_m33, aabb);

	if (!g_pCharacterManager->HasAnyDebugInstancesCreated())
		return;

	for (uint32 p = 0; p < numParameter; p++)
	{
		SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
		renderFlags.SetAlphaBlendMode(e_AlphaAdditive);
		//renderFlags.SetAlphaBlendMode(e_AlphaBlended);
		g_pAuxGeom->SetRenderFlags(renderFlags);

		ICharacterInstance* pExampleInst = g_pCharacterManager->m_arrCharacterBase[p + nInstanceOffset].m_pInst;
		pExampleInst->SetPlaybackScale(fPlaybackScale);
		pExampleInst->GetISkeletonPose()->SetForceSkeletonUpdate(1);
		CSkeletonAnim* pSkeletonAnim = (CSkeletonAnim*)pExampleInst->GetISkeletonAnim();
		pSkeletonAnim->SetAnimationDrivenMotion(1);
		CryCharAnimationParams AParams;
		AParams.m_nFlags = CA_LOOP_ANIMATION | CA_MANUAL_UPDATE;
		AParams.m_fTransTime = 0.15f;
		AParams.m_fKeyTime = -1;    //use keytime of previous motion to start this motion

		f32 x = rLMG.m_arrParameter[p].m_Para.x;
		f32 y = rLMG.m_arrParameter[p].m_Para.y;
		f32 z = rLMG.m_arrParameter[p].m_Para.z;
		Vec3 pos = Vec3(x, y, z) * scl + off;
		Quat rot = g_pCharacterManager->m_arrCharacterBase[p + nInstanceOffset].m_GridLocation.q;

		/*
		    if (nDimensions==1)
		      g_pIRenderer->DrawLabel(pos, 2.5f, "  x=%4.2f",x);
		    if (nDimensions==2)
		      g_pIRenderer->DrawLabel(pos, 2.0f, "  x=%4.2f y=%4.2f",x,y);
		    if (nDimensions==3)
		      g_pIRenderer->DrawLabel(pos, 2.0f, "  x=%4.2f y=%4.2f z=%4.2f",x,y,z);
		 */

		SAnimationProcessParams params;
		params.locationAnimation = QuatTS(rot, pos, fUniScale);
		Vec3 vCharCol(ZERO);
		if (p < numExamples)
		{
			//draw the interpolated examples
			if (rLMG.m_arrParameter[p].m_fPlaybackScale == 1.0f)
				vCharCol.x = 1.0f, vCharCol.y = 1.0f, vCharCol.z = 1.0f, col[p] = RGBA8(0xbf, 0xbf, 0xbf, 0x00); //normal asset
			else
				vCharCol.x = 0.0f, vCharCol.y = 1.0f, vCharCol.z = 0.0f, col[p] = RGBA8(0x00, 0xff, 0x00, 0x00); //time-scaled asset

			g_pAuxGeom->DrawOBB(_obb, pos, 1, col[p], eBBD_Extremes_Color_Encoded);
			IRenderAuxText::DrawLabelF(pos, 1.5f, "%d", p);

			const char* pAnimName = rLMG.m_arrParameter[p].m_animName.GetName_DEBUG();
			pSkeletonAnim->StartAnimation(pAnimName, AParams);

			int32 nAnimIDPS = m_nAnimID[p];
			const ModelAnimationHeader* pAnimPS = pAnimationSet->GetModelAnimationHeader(nAnimIDPS);
			GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[pAnimPS->m_nGlobalAnimId];
			uint8 segcount = m_nSegmentCounter[0][p];
			pSkeletonAnim->SetLayerNormalizedTimeAndSegment(fAnimTime, nEOC, 0, 0, segcount, 0);
			pExampleInst->StartAnimationProcessing(params);
			QuatT relmove = pSkeletonAnim->GetRelMovement();
			g_pCharacterManager->m_arrCharacterBase[p + nInstanceOffset].m_GridLocation.q *= relmove.q;
			pExampleInst->FinishAnimationComputations();

			g_pCharacterManager->m_arrCharacterBase[p + nInstanceOffset].m_AmbientColor.r = vCharCol.x;
			g_pCharacterManager->m_arrCharacterBase[p + nInstanceOffset].m_AmbientColor.g = vCharCol.y;
			g_pCharacterManager->m_arrCharacterBase[p + nInstanceOffset].m_AmbientColor.b = vCharCol.z;
			g_pCharacterManager->m_arrCharacterBase[p + nInstanceOffset].m_AmbientColor.a = 1.0f;
		}
		else
		{
			//draw the extrapolated examples
			vCharCol.x = 1.0f;
			vCharCol.y = 0.0f;
			vCharCol.z = 0.0f;
			col[p] = RGBA8(0xff, 0x00, 0x00, 0x00);
			g_pAuxGeom->DrawOBB(_obb, pos, 1, col[p], eBBD_Extremes_Color_Encoded);
			IRenderAuxText::DrawLabelF(pos, 1.5f, "%d", p);

			int32 i0 = rLMG.m_arrParameter[p].i0;
			f32 w0 = rLMG.m_arrParameter[p].w0;
			int32 i1 = rLMG.m_arrParameter[p].i1;
			f32 w1 = rLMG.m_arrParameter[p].w1;
			GlobalAnimationHeaderLMG& rInternalPara1D = g_AnimationManager.m_arrGlobalLMG[0];
			if (rInternalPara1D.IsAssetInternalType() == 0)
				CryFatalError("Not an Internal Blendspace");

			const char* pAnimName0 = rLMG.m_arrParameter[i0].m_animName.GetName_DEBUG();
			rInternalPara1D.m_arrParameter[0].m_animName.SetName(pAnimName0);
			const char* pAnimName1 = rLMG.m_arrParameter[i1].m_animName.GetName_DEBUG();
			rInternalPara1D.m_arrParameter[1].m_animName.SetName(pAnimName1);

			pSkeletonAnim->StartAnimation("InternalPara1D", AParams);
			pSkeletonAnim->SetDesiredMotionParam(eMotionParamID_BlendWeight, w1, 0.0f);

			int32 nAnimIDPS0 = m_nAnimID[i0];
			int32 nAnimIDPS1 = m_nAnimID[i1];
			const ModelAnimationHeader* pAnimPS = pAnimationSet->GetModelAnimationHeader(nAnimIDPS1);
			GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[pAnimPS->m_nGlobalAnimId];

			int32 segcount0 = m_nSegmentCounter[0][i0];
			int32 segcount1 = m_nSegmentCounter[0][i1];
			pSkeletonAnim->SetLayerNormalizedTimeAndSegment(fAnimTime, nEOC, nAnimIDPS0, nAnimIDPS1, segcount0, segcount1);

			/*float fCol1111[4] = {1,1,1,1};
			   g_pAuxGeom->Draw2dLabel( 1,g_YLine, 2.0f, fCol1111, false,"id: %d  w: %f seg: %d  name: %s",i0,w0,segcount0, pAnimName0);
			   g_YLine+=20.0f;
			   g_pAuxGeom->Draw2dLabel( 1,g_YLine, 2.0f, fCol1111, false,"id: %d  w: %f seg: %d  name: %s",i1,w1,segcount1, pAnimName1);
			   g_YLine+=20.0f;*/

			pExampleInst->StartAnimationProcessing(params);
			QuatT relmove = pSkeletonAnim->GetRelMovement();
			g_pCharacterManager->m_arrCharacterBase[p + nInstanceOffset].m_GridLocation.q *= relmove.q;
			pExampleInst->FinishAnimationComputations();

			g_pCharacterManager->m_arrCharacterBase[p + nInstanceOffset].m_AmbientColor.r = vCharCol.x;
			g_pCharacterManager->m_arrCharacterBase[p + nInstanceOffset].m_AmbientColor.g = vCharCol.y;
			g_pCharacterManager->m_arrCharacterBase[p + nInstanceOffset].m_AmbientColor.b = vCharCol.z;
			g_pCharacterManager->m_arrCharacterBase[p + nInstanceOffset].m_AmbientColor.a = 1.0f;
		}

		g_pCharacterManager->m_arrCharacterBase[p + nInstanceOffset].m_GridLocation.t = params.locationAnimation.t;
		g_pCharacterManager->m_arrCharacterBase[p + nInstanceOffset].m_GridLocation.s = params.locationAnimation.s;
	}
}
#endif
