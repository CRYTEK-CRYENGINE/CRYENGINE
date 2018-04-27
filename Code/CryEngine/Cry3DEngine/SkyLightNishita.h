// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 09:05:2005   11:08 : Created by Carsten Wenzel

*************************************************************************/

#ifndef _SKY_LIGHT_NISHITA_H_
#define _SKY_LIGHT_NISHITA_H_

#pragma once

#include <vector>

class CSkyLightManager;

class CSkyLightNishita : public Cry3DEngineBase
{
private:

	// size of lookup tables
	static const uint32 cOLUT_HeightSteps = 32;
	static const uint32 cOLUT_AngularSteps = 256;

	// definition of optical depth LUT for mie/rayleigh scattering
	struct CRY_ALIGN(8) SOpticalDepthLUTEntry
	{
		f32 mie;
		f32 rayleigh;

		void GetMemoryUsage(ICrySizer* pSizer) const {}

		AUTO_STRUCT_INFO;
	};

	// definition of optical scale LUT for mie/rayleigh scattering
	struct SOpticalScaleLUTEntry
	{
		f32  atmosphereLayerHeight;
		f32  mie;
		f32  rayleigh;

		void GetMemoryUsage(ICrySizer* pSizer) const {}

		AUTO_STRUCT_INFO;
	};

	// definition lookup table entry for phase function
	struct CRY_ALIGN(8) SPhaseLUTEntry
	{
		f32 mie;
		f32 rayleigh;
	};

	// definition of lookup tables
	typedef std::vector<SOpticalDepthLUTEntry> OpticalDepthLUT;
	typedef std::vector<SOpticalScaleLUTEntry> OpticalScaleLUT;

	static const uint32 cPLUT_AngularSteps = 256;
	//replace std::vector by a custom class with static storage
	class PhaseLUT
	{
		CRY_ALIGN(128) SPhaseLUTEntry m_LUT[cPLUT_AngularSteps];
		size_t m_Size;
	public:
		PhaseLUT() : m_Size(0) {}
		SPhaseLUTEntry&       operator[](size_t index)         { return m_LUT[index]; }
		const SPhaseLUTEntry& operator[](size_t index) const   { return m_LUT[index]; }
		void                  resize(size_t size)              { m_Size = size; };
		void                  reserve(size_t)                  {}
		void                  push_back(SPhaseLUTEntry& entry) { m_LUT[m_Size++] = entry; }
		size_t                size() const                     { return m_Size; }
	};

public:
	CSkyLightNishita();
	~CSkyLightNishita();

	// set sky dome conditions
	void SetAtmosphericConditions(const Vec3& sunIntensity, const f32 Km, const f32 Kr, const f32 g);
	void SetRGBWaveLengths(const Vec3& rgbWaveLengths);
	void SetSunDirection(const Vec3& sunDir);

	// compute sky colors
	void  ComputeSkyColor(const Vec3& skyDir, Vec3* pInScattering, Vec3* pInScatteringMieNoPremul, Vec3* pInScatteringRayleighNoPremul, Vec3* pInScatteringRayleigh) const;

	void  SetInScatteringIntegralStepSize(int32 stepSize);
	int32 GetInScatteringIntegralStepSize() const;

	// constants for final pixel shader processing, if "no pre-multiplied in-scattering " colors are to be processed in a pixel shader
	Vec4 GetPartialMieInScatteringConst() const;
	Vec4 GetPartialRayleighInScatteringConst() const;
	Vec3 GetSunDirection() const;
	Vec4 GetPhaseFunctionConsts() const;

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(m_opticalDepthLUT);
		pSizer->AddObject(m_opticalScaleLUT);
	}
private:
	// mapping helpers
	f64  MapIndexToHeight(uint32 index) const;
	f64  MapIndexToCosVertAngle(uint32 index) const;
	f32  MapIndexToCosPhaseAngle(uint32 index) const;

	void MapCosPhaseAngleToIndex(const f32 cosPhaseAngle, uint32& index, f32& indexFrc) const;

	// optical lookup table access helpers
	uint32 OpticalLUTIndex(uint32 heightIndex, uint32 cosVertAngleIndex) const;

	// computes lookup tables for optical depth, etc.
	void ComputeOpticalLUTs();

	// computes lookup table for phase function
	void ComputePhaseLUT();

	// computes optical depth (helpers for ComputeOpticalLUTs())
	f64 IntegrateOpticalDepth(const Vec3d& start, const Vec3d& end,
	                          const f64& avgDensityHeightInv, const f64& error) const;

	bool ComputeOpticalDepth(const Vec3d& cameraLookDir, const f64& cameraHeight, const f64& avgDensityHeightInv, float& depth) const;

	// does a bilinearily filtered lookup into the optical depth LUT
	// SOpticalDepthLUTEntry* is passed to save address resolve operations
	ILINE SOpticalDepthLUTEntry LookupBilerpedOpticalDepthLUTEntry(const SOpticalDepthLUTEntry* const __restrict cpOptDepthLUT,
	                                                               uint32 heightIndex, const f32 cosVertAngle) const;

	// does a bilinearily filtered lookup into the phase LUT
	SPhaseLUTEntry LookupBilerpedPhaseLUTEntry(const f32 cosPhaseAngle) const;

	// computes in-scattering
	void SamplePartialInScatteringAtHeight(const SOpticalScaleLUTEntry& osAtHeight,
	                                       const f32 outScatteringConstMie, const Vec3& outScatteringConstRayleigh, const SOpticalDepthLUTEntry& odAtHeightSky,
	                                       const SOpticalDepthLUTEntry& odAtViewerSky, const SOpticalDepthLUTEntry& odAtHeightSun,
	                                       Vec3& partialInScatteringMie, Vec3& partialInScatteringRayleigh) const;

	void ComputeInScatteringNoPremul(const f32 outScatteringConstMie, const Vec3& outScatteringConstRayleigh, const Vec3& skyDir,
	                                 Vec3& inScatteringMieNoPremul, Vec3& inScatteringRayleighNoPremul) const;

	// serialization of optical LUTs
	bool                   LoadOpticalLUTs();
	void                   SaveOpticalLUTs() const;

	const OpticalScaleLUT& GetOpticalScaleLUT() const;

private:
	// lookup tables
	OpticalDepthLUT m_opticalDepthLUT;
	OpticalScaleLUT m_opticalScaleLUT;
	PhaseLUT        m_phaseLUT;

	// mie scattering constant
	f32 m_Km;

	// rayleigh scattering constant
	f32 m_Kr;

	// sun intensity
	Vec3 m_sunIntensity;

	// mie scattering asymmetry factor (g is always 0.0 for rayleigh scattering)
	f32 m_g;

	// wavelengths for r, g, and b to the -4th used for mie rayleigh scattering
	Vec3 m_invRGBWaveLength4;

	// direction towards the sun
	Vec3 m_sunDir;

	// step size for solving in-scattering integral
	int32 m_inScatteringStepSize;

	friend class CSkyLightManager;
};

ILINE void CSkyLightNishita::SetRGBWaveLengths(const Vec3& rgbWaveLengths)
{
	assert(380.0f <= rgbWaveLengths.x && 780.0f >= rgbWaveLengths.x);
	assert(380.0f <= rgbWaveLengths.y && 780.0f >= rgbWaveLengths.y);
	assert(380.0f <= rgbWaveLengths.z && 780.0f >= rgbWaveLengths.z);

	m_invRGBWaveLength4.x = powf(rgbWaveLengths.x * 1e-3f, -4.0f);
	m_invRGBWaveLength4.y = powf(rgbWaveLengths.y * 1e-3f, -4.0f);
	m_invRGBWaveLength4.z = powf(rgbWaveLengths.z * 1e-3f, -4.0f);
}

ILINE void CSkyLightNishita::SetSunDirection(const Vec3& sunDir)
{
	assert(sunDir.GetLengthSquared() > 0.0f);
	m_sunDir = sunDir;
	m_sunDir.Normalize();
}

ILINE void CSkyLightNishita::SetAtmosphericConditions(const Vec3& sunIntensity,
                                                      const f32 Km, const f32 Kr, const f32 g)
{
	m_sunIntensity = sunIntensity;
	m_Km = Km;
	m_Kr = Kr;

	// update g only if it changed as phase lut needs to be rebuilt
	float newg(clamp_tpl(g, -0.9995f, 0.9995f));
	if (fabsf(m_g - newg) > 1e-6f)
	{
		m_g = newg;
		ComputePhaseLUT();
	}
}

ILINE const CSkyLightNishita::OpticalScaleLUT& CSkyLightNishita::GetOpticalScaleLUT() const
{
	return m_opticalScaleLUT;
}

#endif // #ifndef _SKY_LIGHT_NISHITA_H_
