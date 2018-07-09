// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 09:05:2005   14:32 : Created by Carsten Wenzel

*************************************************************************/

#include "StdAfx.h"
#include "SkyLightManager.h"
#include "SkyLightNishita.h"
#include <CryRenderer/RenderElements/CRESky.h>
#include <CryThreading/IJobManager_JobDelegator.h>

DECLARE_JOB("SkyUpdate", TSkyJob, CSkyLightManager::UpdateInternal);
static JobManager::SJobState g_JobState;

CSkyLightManager::CSkyLightManager()
	: m_pSkyLightNishita(CryAlignedNew<CSkyLightNishita>())
	, m_pSkyDomeMesh(0)
	, m_curSkyDomeCondition()
	, m_updatingSkyDomeCondition()
	, m_numSkyDomeColorsComputed(SSkyLightRenderParams::skyDomeTextureSize)
	, m_curBackBuffer(0)
	, m_curSkyHemiColor()
	, m_curHazeColor(0.0f, 0.0f, 0.0f)
	, m_curHazeColorMieNoPremul(0.0f, 0.0f, 0.0f)
	, m_curHazeColorRayleighNoPremul(0.0f, 0.0f, 0.0f)
	, m_skyHemiColorAccum()
	, m_hazeColorAccum(0.0f, 0.0f, 0.0f)
	, m_hazeColorMieNoPremulAccum(0.0f, 0.0f, 0.0f)
	, m_hazeColorRayleighNoPremulAccum(0.0f, 0.0f, 0.0f)
	, m_bFlushFullUpdate(false)
	, m_renderParams()
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "SkyLightManager");

	if (GetRenderer())
	{
		InitSkyDomeMesh();
	}

	m_updateRequested[0] = m_updateRequested[1] = 0;

	// init textures with default data
	m_skyDomeTextureDataMie[0].resize(SSkyLightRenderParams::skyDomeTextureSize, CryHalf4{ .0f,.0f,.0f,.0f });
	m_skyDomeTextureDataMie[1].resize(SSkyLightRenderParams::skyDomeTextureSize, CryHalf4{ .0f,.0f,.0f,.0f });
	m_skyDomeTextureDataRayleigh[0].resize(SSkyLightRenderParams::skyDomeTextureSize, CryHalf4{ .0f,.0f,.0f,.0f });
	m_skyDomeTextureDataRayleigh[1].resize(SSkyLightRenderParams::skyDomeTextureSize, CryHalf4{ .0f,.0f,.0f,.0f });

	// init time stamps
	m_skyDomeTextureTimeStamp[0] = gEnv->nMainFrameID;
	m_skyDomeTextureTimeStamp[1] = gEnv->nMainFrameID;

	// init sky hemisphere colors and accumulators
	memset(m_curSkyHemiColor, 0, sizeof(m_curSkyHemiColor));
	memset(m_skyHemiColorAccum, 0, sizeof(m_skyHemiColorAccum));

	// set default render parameters
	UpdateRenderParams();
}

CSkyLightManager::~CSkyLightManager()
{
	CryAlignedDelete(m_pSkyLightNishita);
}

inline static void Sync()
{
	gEnv->GetJobManager()->WaitForJob(g_JobState);
}

void CSkyLightManager::PushUpdateParams()
{
	//pushes the update parameters, explicite call since engine requests asynchronously
	memcpy(&m_reqSkyDomeCondition[0], &m_reqSkyDomeCondition[1], sizeof(SSkyDomeCondition));
	m_updateRequested[0] = m_updateRequested[1];
	m_updateRequested[1] = 0;
}

void CSkyLightManager::SetSkyDomeCondition(const SSkyDomeCondition& skyDomeCondition)
{
	m_reqSkyDomeCondition[1] = skyDomeCondition;
	m_updateRequested[1] = 1;
}

void CSkyLightManager::FullUpdate()
{
	Sync();
	PushUpdateParams();
	TSkyJob job(GetRenderer()->GetFrameID(false), SSkyLightRenderParams::skyDomeTextureSize, (int)1);
	job.SetClassInstance(this);
	job.RegisterJobState(&g_JobState);
	job.Run();
	m_needRenderParamUpdate = true;
	m_bFlushFullUpdate = true;
}

void CSkyLightManager::IncrementalUpdate(f32 updateRatioPerFrame, const SRenderingPassInfo& passInfo)
{
	Sync();

	FUNCTION_PROFILER_3DENGINE;

	if (!m_lastFrameID)
	{
		FullUpdate();
		return;
	}

	// get current ID of "main" frame (no recursive rendering),
	// incremental update should only be processed once per frame
	if (m_lastFrameID.value() != passInfo.GetMainFrameID())
	{
		int32 numUpdate((int32) ((f32) SSkyLightRenderParams::skyDomeTextureSize * updateRatioPerFrame / 100.0f + 0.5f));
		numUpdate = clamp_tpl(numUpdate, 1, SSkyLightRenderParams::skyDomeTextureSize);
		if (m_needRenderParamUpdate)
			UpdateRenderParams();     // update render params

		PushUpdateParams();

		TSkyJob job(passInfo.GetMainFrameID(), numUpdate, (int)0);
		job.SetClassInstance(this);
		job.RegisterJobState(&g_JobState);
		job.Run();
	}
}

void CSkyLightManager::UpdateInternal(int32 newFrameID, int32 numUpdates, int callerIsFullUpdate)
{
	FUNCTION_PROFILER_3DENGINE;

	// update sky dome if requested -- requires last update request to be fully processed!
	int procUpdate = callerIsFullUpdate;
	procUpdate |= (int)IsSkyDomeUpdateFinished();
	procUpdate &= m_updateRequested[0];
	if (procUpdate)
	{
		// set sky dome settings
		memcpy(&m_updatingSkyDomeCondition, &m_reqSkyDomeCondition[0], sizeof(SSkyDomeCondition));
		m_pSkyLightNishita->SetSunDirection(m_updatingSkyDomeCondition.m_sunDirection);
		m_pSkyLightNishita->SetRGBWaveLengths(m_updatingSkyDomeCondition.m_rgbWaveLengths);
		m_pSkyLightNishita->SetAtmosphericConditions(m_updatingSkyDomeCondition.m_sunIntensity,
		                                             1e-4f * m_updatingSkyDomeCondition.m_Km, 1e-4f * m_updatingSkyDomeCondition.m_Kr, m_updatingSkyDomeCondition.m_g); // scale mie and rayleigh scattering for more convenient editing in time of day dialog

		// update request has been accepted
		m_updateRequested[0] = 0;
		m_numSkyDomeColorsComputed = 0;

		// reset sky & haze color accumulator
		m_hazeColorAccum = Vec3(0.0f, 0.0f, 0.0f);
		m_hazeColorMieNoPremulAccum = Vec3(0.0f, 0.0f, 0.0f);
		m_hazeColorRayleighNoPremulAccum = Vec3(0.0f, 0.0f, 0.0f);
		memset(m_skyHemiColorAccum, 0, sizeof(m_skyHemiColorAccum));
	}

	// any work to do?
	if (false == IsSkyDomeUpdateFinished())
	{
		if (numUpdates <= 0)
		{
			// do a full update
			numUpdates = SSkyLightRenderParams::skyDomeTextureSize;
		}

		// find minimally required work load for this incremental update
		numUpdates = min(SSkyLightRenderParams::skyDomeTextureSize - m_numSkyDomeColorsComputed, numUpdates);

		// perform color computations
		SkyDomeTextureData& skyDomeTextureDataMie(m_skyDomeTextureDataMie[GetBackBuffer()]);
		SkyDomeTextureData& skyDomeTextureDataRayleigh(m_skyDomeTextureDataRayleigh[GetBackBuffer()]);

		int32 numSkyDomeColorsComputed(m_numSkyDomeColorsComputed);
		for (; numUpdates > 0; --numUpdates, ++numSkyDomeColorsComputed)
		{
			// calc latitude/longitude
			int lon(numSkyDomeColorsComputed / SSkyLightRenderParams::skyDomeTextureWidth);
			int lat(numSkyDomeColorsComputed % SSkyLightRenderParams::skyDomeTextureWidth);

			float lonArc(DEG2RAD((float) lon * 90.0f / (float) SSkyLightRenderParams::skyDomeTextureHeight));
			float latArc(DEG2RAD((float) lat * 360.0f / (float) SSkyLightRenderParams::skyDomeTextureWidth));

			float sinLon(0);
			float cosLon(0);
			sincos_tpl(lonArc, &sinLon, &cosLon);
			float sinLat(0);
			float cosLat(0);
			sincos_tpl(latArc, &sinLat, &cosLat);

			// calc sky direction for given update latitude/longitude (hemisphere)
			Vec3 skyDir(sinLon * cosLat, sinLon * sinLat, cosLon);

			// compute color
			//Vec3 skyColAtDir( 0.0, 0.0, 0.0 );
			Vec3 skyColAtDirMieNoPremul(0.0, 0.0, 0.0);
			Vec3 skyColAtDirRayleighNoPremul(0.0, 0.0, 0.0);
			Vec3 skyColAtDirRayleigh(0.0, 0.0, 0.0);

			m_pSkyLightNishita->ComputeSkyColor(skyDir, 0, &skyColAtDirMieNoPremul, &skyColAtDirRayleighNoPremul, &skyColAtDirRayleigh);

			// store color in texture
			skyDomeTextureDataMie[numSkyDomeColorsComputed] = CryHalf4(skyColAtDirMieNoPremul.x, skyColAtDirMieNoPremul.y, skyColAtDirMieNoPremul.z, 1.0f);
			skyDomeTextureDataRayleigh[numSkyDomeColorsComputed] = CryHalf4(skyColAtDirRayleighNoPremul.x, skyColAtDirRayleighNoPremul.y, skyColAtDirRayleighNoPremul.z, 1.0f);

			// update haze color accum (accumulate second last sample row)
			if (lon == SSkyLightRenderParams::skyDomeTextureHeight - 2)
			{
				m_hazeColorAccum += skyColAtDirRayleigh;
				m_hazeColorMieNoPremulAccum += skyColAtDirMieNoPremul;
				m_hazeColorRayleighNoPremulAccum += skyColAtDirRayleighNoPremul;
			}

			// update sky hemisphere color accumulator
			int y(lon >> SSkyLightRenderParams::skyDomeTextureHeightBy2Log);
			int x(((lat + SSkyLightRenderParams::skyDomeTextureWidthBy8) & (SSkyLightRenderParams::skyDomeTextureWidth - 1)) >> SSkyLightRenderParams::skyDomeTextureWidthBy4Log);
			int skyHemiColAccumIdx(x * y + y);
			assert(((unsigned int)skyHemiColAccumIdx) < 5);
			m_skyHemiColorAccum[skyHemiColAccumIdx] += skyColAtDirRayleigh;
		}

		m_numSkyDomeColorsComputed = numSkyDomeColorsComputed;

		// sky dome update finished?
		if (false != IsSkyDomeUpdateFinished())
		{
			// update time stamp
			m_skyDomeTextureTimeStamp[GetBackBuffer()] = newFrameID;

			// get new haze color
			const float c_invNumHazeSamples(1.0f / (float) SSkyLightRenderParams::skyDomeTextureWidth);
			m_curHazeColor = m_hazeColorAccum * c_invNumHazeSamples;
			m_curHazeColorMieNoPremul = m_hazeColorMieNoPremulAccum * c_invNumHazeSamples;
			m_curHazeColorRayleighNoPremul = m_hazeColorRayleighNoPremulAccum * c_invNumHazeSamples;

			// get new sky hemisphere colors
			const float c_scaleHemiTop(2.0f / (SSkyLightRenderParams::skyDomeTextureWidth * SSkyLightRenderParams::skyDomeTextureHeight));
			const float c_scaleHemiSide(8.0f / (SSkyLightRenderParams::skyDomeTextureWidth * SSkyLightRenderParams::skyDomeTextureHeight));
			m_curSkyHemiColor[0] = m_skyHemiColorAccum[0] * c_scaleHemiTop;
			m_curSkyHemiColor[1] = m_skyHemiColorAccum[1] * c_scaleHemiSide;
			m_curSkyHemiColor[2] = m_skyHemiColorAccum[2] * c_scaleHemiSide;
			m_curSkyHemiColor[3] = m_skyHemiColorAccum[3] * c_scaleHemiSide;
			m_curSkyHemiColor[4] = m_skyHemiColorAccum[4] * c_scaleHemiSide;

			// toggle sky light buffers
			ToggleBuffer();
		}
	}

	// update frame ID
	m_lastFrameID = newFrameID;
}

void CSkyLightManager::SetQuality(int32 quality)
{
	if (quality != m_pSkyLightNishita->GetInScatteringIntegralStepSize())
	{
		Sync();
		// when setting new quality we need to start sky dome update from scratch...
		// ... to avoid "artifacts" in the resulting texture
		m_numSkyDomeColorsComputed = 0;
		m_pSkyLightNishita->SetInScatteringIntegralStepSize(quality);
	}
}

const SSkyLightRenderParams* CSkyLightManager::GetRenderParams() const
{
	return &m_renderParams;
}

void CSkyLightManager::UpdateRenderParams()
{
	// sky dome mesh data
	m_renderParams.m_pSkyDomeMesh = m_pSkyDomeMesh;

	// sky dome texture access
	m_renderParams.m_skyDomeTextureTimeStamp = m_skyDomeTextureTimeStamp[GetFrontBuffer()];
	m_renderParams.m_pSkyDomeTextureDataMie = (const void*) &m_skyDomeTextureDataMie[GetFrontBuffer()][0];
	m_renderParams.m_pSkyDomeTextureDataRayleigh = (const void*) &m_skyDomeTextureDataRayleigh[GetFrontBuffer()][0];
	m_renderParams.m_skyDomeTexturePitch = SSkyLightRenderParams::skyDomeTextureWidth * sizeof(CryHalf4);

	// shader constants for final per-pixel phase computation
	m_renderParams.m_partialMieInScatteringConst = m_pSkyLightNishita->GetPartialMieInScatteringConst();
	m_renderParams.m_partialRayleighInScatteringConst = m_pSkyLightNishita->GetPartialRayleighInScatteringConst();
	Vec3 sunDir(m_pSkyLightNishita->GetSunDirection());
	m_renderParams.m_sunDirection = Vec4(sunDir.x, sunDir.y, sunDir.z, 0.0f);
	m_renderParams.m_phaseFunctionConsts = m_pSkyLightNishita->GetPhaseFunctionConsts();
	m_renderParams.m_hazeColor = Vec4(m_curHazeColor.x, m_curHazeColor.y, m_curHazeColor.z, 0);
	m_renderParams.m_hazeColorMieNoPremul = Vec4(m_curHazeColorMieNoPremul.x, m_curHazeColorMieNoPremul.y, m_curHazeColorMieNoPremul.z, 0);
	m_renderParams.m_hazeColorRayleighNoPremul = Vec4(m_curHazeColorRayleighNoPremul.x, m_curHazeColorRayleighNoPremul.y, m_curHazeColorRayleighNoPremul.z, 0);

	// set sky hemisphere colors
	m_renderParams.m_skyColorTop = m_curSkyHemiColor[0];
	m_renderParams.m_skyColorNorth = m_curSkyHemiColor[3];
	m_renderParams.m_skyColorWest = m_curSkyHemiColor[4];
	m_renderParams.m_skyColorSouth = m_curSkyHemiColor[1];
	m_renderParams.m_skyColorEast = m_curSkyHemiColor[2];

	// copy sky dome condition params
	m_curSkyDomeCondition = m_updatingSkyDomeCondition;

	m_needRenderParamUpdate = 0;
}

void CSkyLightManager::GetCurSkyDomeCondition(SSkyDomeCondition& skyCond) const
{
	skyCond = m_curSkyDomeCondition;
}

bool CSkyLightManager::IsSkyDomeUpdateFinished() const
{
	return(SSkyLightRenderParams::skyDomeTextureSize == m_numSkyDomeColorsComputed);
}

void CSkyLightManager::InitSkyDomeMesh()
{
	ReleaseSkyDomeMesh();

	if (!GetRenderer())
		return;

#if CRY_PLATFORM_MOBILE
	const uint32 c_numRings(10);
	const uint32 c_numSections(10);
#else
	const uint32 c_numRings(20);
	const uint32 c_numSections(20);
#endif
	const uint32 c_numSkyDomeVertices((c_numRings + 1) * (c_numSections + 1));
	const uint32 c_numSkyDomeTriangles(2 * c_numRings * c_numSections);
	const uint32 c_numSkyDomeIndices(c_numSkyDomeTriangles * 3);

	std::vector<vtx_idx> skyDomeIndices;
	std::vector<SVF_P3F_C4B_T2F> skyDomeVertices;

	// setup buffers with source data
	skyDomeVertices.reserve(c_numSkyDomeVertices);
	skyDomeIndices.reserve(c_numSkyDomeIndices);

	// calculate vertices
	float sectionSlice(DEG2RAD(360.0f / (float) c_numSections));
	float ringSlice(DEG2RAD(180.0f / (float) c_numRings));
	for (uint32 a(0); a <= c_numRings; ++a)
	{
		float w(sinf(a * ringSlice));
		float z(cosf(a * ringSlice));

		for (uint32 i(0); i <= c_numSections; ++i)
		{
			SVF_P3F_C4B_T2F v;

			float ii(i - a * 0.5f);   // Gives better tessellation, requires texture address mode to be "wrap"
			                          // for u when rendering (see v.st[ 0 ] below). Otherwise set ii = i;
			v.xyz = Vec3(cosf(ii * sectionSlice) * w, sinf(ii * sectionSlice) * w, z);
			assert(fabs(v.xyz.GetLengthSquared() - 1.0) < 1e-2 /*1e-4*/);       // because of FP-16 precision
			v.st = Vec2(ii / (float) c_numSections, 2.0f * (float) a / (float) c_numRings);
			skyDomeVertices.push_back(v);
		}
	}

	// build faces
	for (uint32 a(0); a < c_numRings; ++a)
	{
		for (uint32 i(0); i < c_numSections; ++i)
		{
			skyDomeIndices.push_back((vtx_idx) (a * (c_numSections + 1) + i + 1));
			skyDomeIndices.push_back((vtx_idx) (a * (c_numSections + 1) + i));
			skyDomeIndices.push_back((vtx_idx) ((a + 1) * (c_numSections + 1) + i + 1));

			skyDomeIndices.push_back((vtx_idx) ((a + 1) * (c_numSections + 1) + i));
			skyDomeIndices.push_back((vtx_idx) ((a + 1) * (c_numSections + 1) + i + 1));
			skyDomeIndices.push_back((vtx_idx) (a * (c_numSections + 1) + i));
		}
	}

	// sanity checks
	assert(skyDomeVertices.size() == c_numSkyDomeVertices);
	assert(skyDomeIndices.size() == c_numSkyDomeIndices);

	// create static buffers in renderer
	m_pSkyDomeMesh = gEnv->pRenderer->CreateRenderMeshInitialized(&skyDomeVertices[0], c_numSkyDomeVertices, EDefaultInputLayouts::P3F_C4B_T2F,
	                                                              &skyDomeIndices[0], c_numSkyDomeIndices, prtTriangleList, "SkyHDR", "SkyHDR");

	m_lastFrameID = stl::nullopt;
	m_needRenderParamUpdate = true;
}

void CSkyLightManager::ReleaseSkyDomeMesh()
{
	m_renderParams.m_pSkyDomeMesh = NULL;
	m_pSkyDomeMesh = NULL;
}

int CSkyLightManager::GetFrontBuffer() const
{
	assert(m_curBackBuffer >= 0 && m_curBackBuffer <= 1);
	return((m_curBackBuffer + 1) & 1);
}

int CSkyLightManager::GetBackBuffer() const
{
	assert(m_curBackBuffer >= 0 && m_curBackBuffer <= 1);
	return(m_curBackBuffer);
}

void CSkyLightManager::ToggleBuffer()
{
	assert(m_curBackBuffer >= 0 && m_curBackBuffer <= 1);
	//better enforce cache flushing then making PPU wait til job has been finished
	m_curBackBuffer = (m_curBackBuffer + 1) & 1;
	m_needRenderParamUpdate = 1;
}

void CSkyLightManager::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_pSkyLightNishita);
	pSizer->AddObject(m_skyDomeTextureDataMie[0]);
	pSizer->AddObject(m_skyDomeTextureDataMie[1]);
	pSizer->AddObject(m_skyDomeTextureDataRayleigh[0]);
	pSizer->AddObject(m_skyDomeTextureDataRayleigh[1]);
}
