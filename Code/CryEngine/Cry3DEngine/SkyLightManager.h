// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 09:05:2005   14:32 : Created by Carsten Wenzel

*************************************************************************/

#ifndef _SKY_LIGHT_MANAGER_H_
#define _SKY_LIGHT_MANAGER_H_

#pragma once

#include <memory>
#include <vector>

#include <CryCore/optional.h>

class CSkyLightNishita;
struct ITimer;

class CRY_ALIGN(128) CSkyLightManager: public Cry3DEngineBase
{
public:
	struct CRY_ALIGN(16) SSkyDomeCondition
	{
		SSkyDomeCondition()
			: m_sunIntensity(20.0f, 20.0f, 20.0f)
			  , m_Km(0.001f)
			  , m_Kr(0.00025f)
			  , m_g(-0.99f)
			  , m_rgbWaveLengths(650.0f, 570.0f, 475.0f)
			  , m_sunDirection(0.0f, 0.707106f, 0.707106f)
		{
		}

		Vec3 m_sunIntensity;
		float m_Km;
		float m_Kr;
		float m_g;
		Vec3 m_rgbWaveLengths;
		Vec3 m_sunDirection;
	};

public:
	CSkyLightManager();
	~CSkyLightManager();

	// sky dome condition
	void SetSkyDomeCondition(const SSkyDomeCondition &skyDomeCondition);
	void GetCurSkyDomeCondition(SSkyDomeCondition & skyCond) const;

	// controls updates
	void FullUpdate();
	void IncrementalUpdate(f32 updateRatioPerFrame, const SRenderingPassInfo &passInfo);
	void SetQuality(int32 quality);

	// rendering params
	const SSkyLightRenderParams* GetRenderParams() const;

	void GetMemoryUsage(ICrySizer * pSizer) const;

	void InitSkyDomeMesh();
	void ReleaseSkyDomeMesh();

	void UpdateInternal(int32 newFrameID, int32 numUpdates, int callerIsFullUpdate = 0);
private:
	typedef std::vector<CryHalf4> SkyDomeTextureData;

private:
	bool IsSkyDomeUpdateFinished() const;

	int GetFrontBuffer() const;
	int GetBackBuffer() const;
	void ToggleBuffer();
	void UpdateRenderParams();
	void PushUpdateParams();

private:
	SSkyDomeCondition m_curSkyDomeCondition;      //current sky dome conditions
	SSkyDomeCondition m_reqSkyDomeCondition[2];   //requested sky dome conditions, double buffered(engine writes async)
	SSkyDomeCondition m_updatingSkyDomeCondition; //sky dome conditions the update is currently processed with
	int m_updateRequested[2];                     //true if an update is requested, double buffered(engine writes async)
	CSkyLightNishita* m_pSkyLightNishita;

	SkyDomeTextureData m_skyDomeTextureDataMie[2];
	SkyDomeTextureData m_skyDomeTextureDataRayleigh[2];
	int32 m_skyDomeTextureTimeStamp[2];

	bool m_bFlushFullUpdate;

	_smart_ptr<IRenderMesh> m_pSkyDomeMesh;

	int32 m_numSkyDomeColorsComputed;
	int32 m_curBackBuffer;

	stl::optional<int32> m_lastFrameID;
	int32 m_needRenderParamUpdate;

	Vec3 m_curSkyHemiColor[5];
	Vec3 m_curHazeColor;
	Vec3 m_curHazeColorMieNoPremul;
	Vec3 m_curHazeColorRayleighNoPremul;

	Vec3 m_skyHemiColorAccum[5];
	Vec3 m_hazeColorAccum;
	Vec3 m_hazeColorMieNoPremulAccum;
	Vec3 m_hazeColorRayleighNoPremulAccum;

	CRY_ALIGN(16) SSkyLightRenderParams m_renderParams;
};

#endif // #ifndef _SKY_LIGHT_MANAGER_H_
