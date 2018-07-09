// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 09:05:2005   11:08 : Created by Carsten Wenzel

*************************************************************************/

#include "StdAfx.h"

#define IGNORE_ASSERTS
#if defined(_DEBUG) && defined(IGNORE_ASSERTS)
	#undef assert
	#define assert(cond) ((void)0)
#endif

#include "SkyLightNishita.h"

#include <math.h>

// constant definitions (all heights & radii given in km or km^-1 )
const f64 c_maxAtmosphereHeight(100.0);
const f64 c_earthRadius(6368.0);
const f32 c_earthRadiusf(6368.0f);
const f64 c_avgDensityHeightMieInv(1.0 / 1.2);
const f64 c_avgDensityHeightRayleighInv(1.0 / 7.994);

const f64 c_opticalDepthWhenHittingEarth(1e10);

const f64 c_pi(3.1415926535897932384626433832795);
const f32 c_pif(3.1415926535897932384626433832795f);

// constants for optical LUT serialization
const uint32 c_lutFileTag(0x4C594B53);          // "SKYL"
const uint32 c_lutFileVersion(0x00010002);
const char c_lutFileName[] = "%ENGINE%/engineassets/sky/optical.lut";

static inline f64 MapSaveExpArg(f64 arg)
{
	const f64 c_saveExpArgRange((f64)650.0);   // -650.0 to 650 range is safe not to introduce fp over-/underflows
	return((arg < -c_saveExpArgRange) ? -c_saveExpArgRange : (arg > c_saveExpArgRange) ? c_saveExpArgRange : arg);
}

static inline f64 exp_precise(f64 arg)
{
	return(exp(MapSaveExpArg((f64) arg)));
}

namespace
{
union eco
{
	f64 d;
	struct
	{
		int32 i, j;
	} n;
};
}

static inline f64 exp_fast(f64 arg)
{
	const f64 eco_m(1048576L / 0.693147180559945309417232121458177);
	const f64 eco_a(1072693248L - 60801L);

#if CRY_PLATFORM_X86 || CRY_PLATFORM_X64 || CRY_PLATFORM_ARM // for little endian (tested on Win32 / Win64)
	eco e;
	#ifdef _DEBUG
	e.d = 1.0;
	assert(e.n.j - 1072693248L || e.n.i == 0);     // check IEEE-754 conformance
	#endif
	e.n.j = (int32) (eco_m * MapSaveExpArg(arg) + eco_a);
	return((f64)e.d);
#else // fall back to default exp_sky() implementation for untested/unsupported target platforms
	#pragma message( "Optimized exp_fast() not available for this platform!" )
	#pragma message(	"If your target CPU is IEEE-754 conformant then please specify it in either the little or big endian branch (see SkyLightNishita.cpp::exp_fast())." )
	return(exp(arg));
#endif
}

static inline f64 OpticalScaleFunction(const f64& height, const f64& avgDensityHeightInv)
{
	assert(height >= 0.0);
	assert(avgDensityHeightInv > 0.0 && avgDensityHeightInv <= 1.0);
	return(exp_precise(-height * avgDensityHeightInv));
}

static inline f64 IntegrateOpticalDepthInternal(const Vec3d& start, const f64& startScale,
                                                const Vec3d& end, const f64& endScale, const f64& avgDensityHeightInv, const f64& error)
{
	assert(_finite(startScale) && _finite(endScale));

	Vec3d mid(0.5 * (start + end));
	f64 midScale(OpticalScaleFunction(mid.GetLength() - c_earthRadius, avgDensityHeightInv));

	if (fabs(startScale - midScale) <= error && fabs(midScale - endScale) <= error)
	{
		// integrate section this via simpson rule and stop recursing
		const f64 c_oneSixth(1.0 / 6.0);
		return((startScale + 4.0 * midScale + endScale) * c_oneSixth * (end - start).GetLength());
	}
	else
	{
		// refine section via recursing down left and right branch
		return(IntegrateOpticalDepthInternal(start, startScale, mid, midScale, avgDensityHeightInv, error) +
		       IntegrateOpticalDepthInternal(mid, midScale, end, endScale, avgDensityHeightInv, error));
	}
}

CSkyLightNishita::CSkyLightNishita()
	: m_opticalDepthLUT()
	, m_opticalScaleLUT()
	, m_phaseLUT()
	, m_Km(0.0f)
	, m_Kr(0.0f)
	, m_sunIntensity(20.0f, 20.0f, 20.0f)
	, m_g(0.0f)
	, m_invRGBWaveLength4(1.0f, 1.0f, 1.0f)
	, m_sunDir(0.0f, 0.707106f, 0.707106f)
	, m_inScatteringStepSize(1)
{
	SetRGBWaveLengths(Vec3(650.0f, 570.0f, 475.0f));
	SetSunDirection(Vec3(0.0f, 0.707106f, 0.707106f));
	SetAtmosphericConditions(Vec3(20.0f, 20.0f, 20.0f), 0.001f, 0.00025f, -0.99f);
	ILog* pLog(C3DEngine::GetLog());
	if (false == LoadOpticalLUTs())
	{
		if (0 != pLog)
		{
			PrintMessage("Sky light: Optical lookup tables couldn't be loaded off disc. Recomputation needed!");
		}
		ComputeOpticalLUTs();
	}
	else
	{
		if (0 != pLog)
		{
			PrintMessage("Sky light: Optical lookup tables loaded off disc.");
		}
	}
}

CSkyLightNishita::~CSkyLightNishita()
{
}

CSkyLightNishita::SOpticalDepthLUTEntry CSkyLightNishita::LookupBilerpedOpticalDepthLUTEntry(
  const SOpticalDepthLUTEntry* const __restrict cpOptDepthLUT,
  uint32 heightIndex, const f32 cosVertAngle) const
{
	uint32 vertAngleIndex;
	f32 vertAngleIndexFrc;
	f32 saveCosVertAngle(clamp_tpl(cosVertAngle, -1.0f, 1.0f));
	f32 _index((f32) (cOLUT_AngularSteps - 1) * (-saveCosVertAngle * 0.5f + 0.5f));
	vertAngleIndex = (uint32) _index;
	vertAngleIndexFrc = _index - floorf(_index);

	if (vertAngleIndex >= cOLUT_AngularSteps - 1)
	{
		return(cpOptDepthLUT[OpticalLUTIndex(heightIndex, vertAngleIndex)]);
	}
	else
	{
		uint32 index(OpticalLUTIndex(heightIndex, vertAngleIndex));
		const SOpticalDepthLUTEntry& a(cpOptDepthLUT[index]);
		const SOpticalDepthLUTEntry& b(cpOptDepthLUT[index + 1]);

		SOpticalDepthLUTEntry res;
		res.mie = a.mie + vertAngleIndexFrc * (b.mie - a.mie);
		res.rayleigh = a.rayleigh + vertAngleIndexFrc * (b.rayleigh - a.rayleigh);
		return(res);
	}
}

CSkyLightNishita::SPhaseLUTEntry CSkyLightNishita::LookupBilerpedPhaseLUTEntry(const f32 cosPhaseAngle) const
{
	uint32 index;
	f32 indexFrc;
	MapCosPhaseAngleToIndex(cosPhaseAngle, index, indexFrc);

	if (index >= cPLUT_AngularSteps - 1)
	{
		return(m_phaseLUT[cPLUT_AngularSteps - 1]);
	}
	else
	{
		const SPhaseLUTEntry& a(m_phaseLUT[index + 0]);
		const SPhaseLUTEntry& b(m_phaseLUT[index + 1]);

		SPhaseLUTEntry res;
		res.mie = a.mie + indexFrc * (b.mie - a.mie);
		res.rayleigh = a.rayleigh + indexFrc * (b.rayleigh - a.rayleigh);
		return(res);
	}
}

void CSkyLightNishita::SamplePartialInScatteringAtHeight(const SOpticalScaleLUTEntry& osAtHeight,
                                                         const f32 outScatteringConstMie, const Vec3& outScatteringConstRayleigh, const SOpticalDepthLUTEntry& odAtHeightSky,
                                                         const SOpticalDepthLUTEntry& odAtViewerSky, const SOpticalDepthLUTEntry& odAtHeightSun,
                                                         Vec3& partialInScatteringMie, Vec3& partialInScatteringRayleigh) const
{
	assert(odAtHeightSky.mie >= 0.0 && odAtHeightSky.mie <= odAtViewerSky.mie);
	assert(odAtHeightSun.mie >= 0.0);
	assert(odAtHeightSky.rayleigh >= 0.0 && odAtHeightSky.rayleigh <= odAtViewerSky.rayleigh);
	assert(odAtHeightSun.rayleigh >= 0.0);

	// mie out-scattering
	f32 sampleExpArgMie(outScatteringConstMie * (-odAtHeightSun.mie - (odAtViewerSky.mie - odAtHeightSky.mie)));

	// rayleigh out-scattering
	Vec3 sampleExpArgRayleigh(outScatteringConstRayleigh * (-odAtHeightSun.rayleigh - (odAtViewerSky.rayleigh - odAtHeightSky.rayleigh)));

	// partial in-scattering sampling result
	Vec3 sampleExpArg(Vec3(sampleExpArgMie, sampleExpArgMie, sampleExpArgMie) + sampleExpArgRayleigh);
	Vec3 sampleRes((float)exp_fast(sampleExpArg.x), (float)exp_fast(sampleExpArg.y), (float)exp_fast(sampleExpArg.z));

	partialInScatteringMie = osAtHeight.mie * sampleRes;
	partialInScatteringRayleigh = osAtHeight.rayleigh * sampleRes;
}

void CSkyLightNishita::ComputeInScatteringNoPremul(const f32 outScatteringConstMie, const Vec3& outScatteringConstRayleigh, const Vec3& skyDir,
                                                   Vec3& inScatteringMieNoPremul, Vec3& inScatteringRayleighNoPremul) const
{
	// start integration along the "skyDir" from the viewer's point of view
	const Vec3 c_up(0.0f, 0.0f, 1.0f);
	const Vec3 viewer(c_up * c_earthRadiusf);
	Vec3 curRayPos(viewer);

	// to be reused by ray-sphere intersection code in loop below
	f32 B(2.0f * viewer.Dot(skyDir));
	f32 Bsq(B * B);

	// calculate optical depth at viewer
	const SOpticalDepthLUTEntry* const __restrict cpOptDepthLUT = &m_opticalDepthLUT[0];

	const Vec3& cSunDir(m_sunDir);

	SOpticalDepthLUTEntry odAtViewerSky(LookupBilerpedOpticalDepthLUTEntry(cpOptDepthLUT, 0, skyDir.Dot(c_up)));
	SOpticalDepthLUTEntry odAtViewerSun(LookupBilerpedOpticalDepthLUTEntry(cpOptDepthLUT, 0, cSunDir.Dot(c_up)));

	// sample partial in-scattering term at viewer
	Vec3 curSampleMie, curSampleRayleigh;

	const SOpticalScaleLUTEntry* const __restrict cpOptScaleLUT = &m_opticalScaleLUT[0];

	SamplePartialInScatteringAtHeight(cpOptScaleLUT[0], outScatteringConstMie, outScatteringConstRayleigh,
	                                  odAtViewerSky, odAtViewerSky, odAtViewerSun, curSampleMie, curSampleRayleigh);

	// integrate along "skyDir" over all height segments we've precalculated in the optical lookup table
	inScatteringMieNoPremul = Vec3(0.0f, 0.0f, 0.0f);
	inScatteringRayleighNoPremul = Vec3(0.0f, 0.0f, 0.0f);
	const int32 cInScatteringStepSize(m_inScatteringStepSize);
	for (int a(1); a < cOLUT_HeightSteps; a += cInScatteringStepSize)
	{
		// calculate intersection with current "atmosphere shell"
		const SOpticalScaleLUTEntry& crOpticalScaleLUTEntry = cpOptScaleLUT[a];
		SOpticalScaleLUTEntry osAtHeight(crOpticalScaleLUTEntry);

		f32 C = -osAtHeight.atmosphereLayerHeight * (osAtHeight.atmosphereLayerHeight + 2.0f * c_earthRadiusf);
		f32 det(Bsq - 4.0f * C);
		assert(det >= 0.0f && (0.5f * (-B - sqrtf(det)) <= 0.0f) && (0.5f * (-B + sqrtf(det)) >= 0.0f));

		f32 t(0.5f * (-B + sqrtf(det)));

		Vec3 newRayPos(viewer + t * skyDir);

		// calculate optical depth at new position
		// since atmosphere bends we need to determine a new up vector to properly index the optical LUT
		Vec3 newUp(newRayPos.GetNormalized());
		SOpticalDepthLUTEntry odAtHeightSky(LookupBilerpedOpticalDepthLUTEntry(cpOptDepthLUT, a, skyDir.Dot(newUp)));
		SOpticalDepthLUTEntry odAtHeightSun(LookupBilerpedOpticalDepthLUTEntry(cpOptDepthLUT, a, cSunDir.Dot(newUp)));

		// sample partial in-scattering term at new position
		Vec3 newSampleMie, newSampleRayleigh;
		SamplePartialInScatteringAtHeight(osAtHeight, outScatteringConstMie, outScatteringConstRayleigh,
		                                  odAtHeightSky, odAtViewerSky, odAtHeightSun, newSampleMie, newSampleRayleigh);

		// integrate via trapezoid rule
		f32 weight((newRayPos - curRayPos).GetLength() * 0.5f);
		inScatteringMieNoPremul += (curSampleMie + newSampleMie) * weight;
		inScatteringRayleighNoPremul += (curSampleRayleigh + newSampleRayleigh) * weight;

		// update sampling data
		curRayPos = newRayPos;
		curSampleMie = newSampleMie;
		curSampleRayleigh = newSampleRayleigh;
	}
}

void CSkyLightNishita::ComputeSkyColor(const Vec3& skyDir, Vec3* pInScattering, Vec3* pInScatteringMieNoPremul,
                                       Vec3* pInScatteringRayleighNoPremul, Vec3* pInScatteringRayleigh) const
{
	//// get high precision normalized sky direction
	//Vec3 _skyDir( skyDir );
	//assert( _skyDir.GetLengthSquared() > 0.0 );
	//_skyDir.Normalize();

	assert(fabsf(skyDir.GetLengthSquared() - 1.0f) < 1e-4f);

	SPhaseLUTEntry phaseLUTEntry(LookupBilerpedPhaseLUTEntry(-skyDir.Dot(m_sunDir)));

	// initialize constants for mie scattering
	f32 phaseForPhiGMie(phaseLUTEntry.mie);
	f32 outScatteringConstMie(4.0f * c_pif * m_Km);
	Vec3 inScatteringConstMie(m_sunIntensity * m_Km * phaseForPhiGMie);

	// initialize constants for rayleigh scattering
	f32 phaseForPhiGRayleigh(phaseLUTEntry.rayleigh);
	Vec3 outScatteringConstRayleigh(4.0f * (float)c_pi * m_Kr * m_invRGBWaveLength4);
	Vec3 inScatteringConstRayleigh((m_sunIntensity * m_Kr * phaseForPhiGRayleigh).CompMul(m_invRGBWaveLength4));

	// compute in-scattering
	Vec3 inScatteringMieNoPremul, inScatteringRayleighNoPremul;

	ComputeInScatteringNoPremul(outScatteringConstMie, outScatteringConstRayleigh, skyDir, inScatteringMieNoPremul, inScatteringRayleighNoPremul);

	assert(inScatteringMieNoPremul.x >= 0.0f && inScatteringMieNoPremul.y >= 0.0f && inScatteringMieNoPremul.z >= 0.0f);
	assert(inScatteringRayleighNoPremul.x >= 0.0f && inScatteringRayleighNoPremul.y >= 0.0f && inScatteringRayleighNoPremul.z >= 0.0f);

	// return color
	if (pInScattering)
		*pInScattering = Vec3(inScatteringMieNoPremul.CompMul(inScatteringConstMie) + inScatteringRayleighNoPremul.CompMul(inScatteringConstRayleigh));

	if (pInScatteringMieNoPremul)
		*pInScatteringMieNoPremul = Vec3(inScatteringMieNoPremul);

	if (pInScatteringRayleighNoPremul)
		*pInScatteringRayleighNoPremul = Vec3(inScatteringRayleighNoPremul);

	if (pInScatteringRayleigh)
		*pInScatteringRayleigh = Vec3(inScatteringRayleighNoPremul.CompMul(inScatteringConstRayleigh));
}

void CSkyLightNishita::SetInScatteringIntegralStepSize(int32 stepSize)
{
	stepSize = stepSize < 1 ? 1 : stepSize > 2 ? 2 : stepSize;
	m_inScatteringStepSize = stepSize;
}

int32 CSkyLightNishita::GetInScatteringIntegralStepSize() const
{
	return(m_inScatteringStepSize);
}

Vec4 CSkyLightNishita::GetPartialMieInScatteringConst() const
{
	Vec3 res(m_sunIntensity * m_Km);
	return(Vec4(res.x, res.y, res.z, 0.0f));
}

Vec4 CSkyLightNishita::GetPartialRayleighInScatteringConst() const
{
	Vec3 res((m_sunIntensity * m_Kr).CompMul(m_invRGBWaveLength4));
	return(Vec4(res.x, res.y, res.z, 0.0f));
}

Vec3 CSkyLightNishita::GetSunDirection() const
{
	return(Vec3(m_sunDir.x, m_sunDir.y, m_sunDir.z));
}

Vec4 CSkyLightNishita::GetPhaseFunctionConsts() const
{
	//f32 g2( m_g * m_g );
	//f32 miePart( 1.5f * ( 1.0f - g2 ) / ( 2.0f + g2 ) );
	//return( Vec4( m_g, m_g * m_g, miePart, 0.0f ) );

	f32 g2(m_g * m_g);
	f32 miePart(1.5f * (1.0f - g2) / (2.0f + g2));
	f32 miePartPow(powf(miePart, -2.0f / 3.0f));
	return(Vec4(miePartPow * -2.0f * m_g, miePartPow * (1.0f + g2), 0.0f, 0.0f));
}

f64 CSkyLightNishita::IntegrateOpticalDepth(const Vec3d& start, const Vec3d& end, const f64& avgDensityHeightInv, const f64& error) const
{
	f64 startScale(OpticalScaleFunction(start.GetLength() - c_earthRadius, avgDensityHeightInv));
	f64 endScale(OpticalScaleFunction(end.GetLength() - c_earthRadius, avgDensityHeightInv));
	return(IntegrateOpticalDepthInternal(start, startScale, end, endScale, avgDensityHeightInv, error));
}

bool CSkyLightNishita::ComputeOpticalDepth(const Vec3d& cameraLookDir, const f64& cameraHeight, const f64& avgDensityHeightInv, float& depth) const
{
	// init camera position
	Vec3d cameraPos(0.0, cameraHeight + c_earthRadius, 0.0);

	// check if ray hits earth
	// compute B, and C of quadratic function (A=1, as looking direction is normalized)
	f64 B(2.0 * cameraPos.Dot(cameraLookDir));
	f64 Bsq(B * B);
	f64 Cpart(cameraPos.Dot(cameraPos));
	f64 C(Cpart - c_earthRadius * c_earthRadius);
	f64 det(Bsq - 4.0 * C);

	bool hitsEarth(det >= 0.0 && ((0.5 * (-B - sqrt(det)) > 1e-4) || (0.5 * (-B + sqrt(det)) > 1e-4)));
	if (false != hitsEarth)
	{
		depth = (float)c_opticalDepthWhenHittingEarth;
		return(false);
	}

	// find intersection with atmosphere top
	C = Cpart - (c_maxAtmosphereHeight + c_earthRadius) * (c_maxAtmosphereHeight + c_earthRadius);
	det = Bsq - 4.0 * C;
	assert(det >= 0.0);   // ray defined outside the atmosphere
	f64 t(0.5 * (-B + sqrt(det)));
	assert(t >= -1e-4);
	if (t < 0.0) t = 0.0;

	// integrate depth along ray from camera to atmosphere top
	f64 _depth(0.0);

	int numInitialSamples((int) t);
	numInitialSamples = (numInitialSamples < 2) ? 2 : numInitialSamples;

	Vec3d lastCameraPos(cameraPos);
	for (int i(1); i < numInitialSamples; ++i)
	{
		Vec3d curCameraPos(cameraPos + cameraLookDir * (t * ((float) i / (float) numInitialSamples)));
		_depth += IntegrateOpticalDepth(lastCameraPos, curCameraPos, avgDensityHeightInv, 1e-1);
		lastCameraPos = curCameraPos;
	}

	assert(_depth >= 0.0 && _depth < 1e25);
	assert(0 != _finite(_depth));

	depth = (float) _depth;
	return(true);
}

void CSkyLightNishita::ComputeOpticalLUTs()
{
	LOADING_TIME_PROFILE_SECTION(GetISystem());

	ILog* pLog(C3DEngine::GetLog());
	if (0 != pLog)
	{
		PrintMessage("Sky light: Computing optical lookup tables (this might take a while)... ");
	}

	// reset tables
	m_opticalDepthLUT.resize(0);
	m_opticalDepthLUT.reserve(cOLUT_HeightSteps * cOLUT_AngularSteps);

	m_opticalScaleLUT.resize(0);
	m_opticalScaleLUT.reserve(cOLUT_HeightSteps);

	// compute LUTs
	for (int a(0); a < cOLUT_HeightSteps; ++a)
	{
		f64 height(MapIndexToHeight(a));

		// compute optical depth
		for (int i(0); i < cOLUT_AngularSteps; ++i)
		{
			// init looking direction of camera
			f64 cosVertAngle(MapIndexToCosVertAngle(i));
			Vec3d cameraLookDir(sqrt(1.0 - cosVertAngle * cosVertAngle), cosVertAngle, 0.0);

			// compute optical depth
			SOpticalDepthLUTEntry e;
			bool b0(ComputeOpticalDepth(cameraLookDir, height, c_avgDensityHeightMieInv, e.mie));
			bool b1(ComputeOpticalDepth(cameraLookDir, height, c_avgDensityHeightRayleighInv, e.rayleigh));
			assert(b0 == b1);

			// blend out previous values once camera ray hits earth
			if (false == b0 && false == b1 && i > 0)
			{
				e = m_opticalDepthLUT.back();
				e.mie = (f32) ((f32)0.5 * (e.mie + c_opticalDepthWhenHittingEarth));
				e.rayleigh = (f32) ((f32)0.5 * (e.rayleigh + c_opticalDepthWhenHittingEarth));
			}

			// store result
			m_opticalDepthLUT.push_back(e);
		}

		{
			// compute optical scale
			SOpticalScaleLUTEntry e;
			e.atmosphereLayerHeight = (f32) height;
			e.mie = (f32) OpticalScaleFunction(height, c_avgDensityHeightMieInv);
			e.rayleigh = (f32) OpticalScaleFunction(height, c_avgDensityHeightRayleighInv);
			m_opticalScaleLUT.push_back(e);
		}
	}

	// save LUTs for next time
	SaveOpticalLUTs();
	if (0 != pLog)
	{
		PrintMessage(" ... done.\n");
	}
}

void CSkyLightNishita::ComputePhaseLUT()
{
	//ILog* pLog( C3DEngine::GetLog() );
	//if( 0 != pLog )
	//	PrintMessage( "Sky light: Computing phase lookup table... " );

	// reset tables
	m_phaseLUT.resize(0);
	m_phaseLUT.reserve(cPLUT_AngularSteps);

	// compute coefficients
	f32 g(m_g);
	f32 g2(g * g);
	f32 miePart(1.5f * (1.0f - g2) / (2.0f + g2));

	// calculate entries
	for (int i(0); i < cPLUT_AngularSteps; ++i)
	{
		f32 cosine(MapIndexToCosPhaseAngle(i));
		f32 cosine2(cosine * cosine);

		//f32 t = 1.0f + g2 - 2.0f * g * cosine;
		//if (fabsf(t) < 1e-5f)
		//{
		//	PrintMessage( "Sky light: g = %.10f", g );
		//	PrintMessage( "Sky light: g2 = %.10f", g2 );
		//	PrintMessage( "Sky light: cosine = %.10f", cosine );
		//	PrintMessage( "Sky light: cosine2 = %.10f", cosine2 );
		//	PrintMessage( "Sky light: t = %.10f", t );
		//}

		f32 miePhase(miePart * (1.0f + cosine2) / powf(1.0f + g2 - 2.0f * g * cosine, 1.5f));
		f32 rayleighPhase(0.75f * (1.0f + cosine2));

		SPhaseLUTEntry e;
		e.mie = (float) miePhase;
		e.rayleigh = (float) rayleighPhase;
		m_phaseLUT.push_back(e);
	}
	//if( 0 != pLog )
	//	PrintMessage( " ... done.\n" );
}

f64 CSkyLightNishita::MapIndexToHeight(uint32 index) const
{
	// a function that maps well to mie and rayleigh at the same time
	// that is, a lot of indices will map below the average density height for mie & rayleigh scattering
	assert(index < cOLUT_HeightSteps);
	f64 x((f64)index / (cOLUT_HeightSteps - 1));
	return(c_maxAtmosphereHeight * exp_precise(10.0 * (x - 1.0)) * x);
}

f64 CSkyLightNishita::MapIndexToCosVertAngle(uint32 index) const
{
	assert(index < cOLUT_AngularSteps);
	return(1.0 - 2.0 * ((f64)index / (cOLUT_AngularSteps - 1)));
}

f32 CSkyLightNishita::MapIndexToCosPhaseAngle(uint32 index) const
{
	assert(index < cPLUT_AngularSteps);
	return(1.0f - 2.0f * ((f32)index / ((f32)cPLUT_AngularSteps - 1)));
}

void CSkyLightNishita::MapCosPhaseAngleToIndex(const f32 cosPhaseAngle, uint32& index, f32& indexFrc) const
{
	//assert( -1 <= cosPhaseAngle && 1 >= cosPhaseAngle );
	f32 saveCosPhaseAngle(clamp_tpl(cosPhaseAngle, -1.0f, 1.0f));
	f32 _index((f32) (cPLUT_AngularSteps - 1) * (-saveCosPhaseAngle * 0.5f + 0.5f));
	index = (uint32) _index;
	indexFrc = _index - floorf(_index);
}

uint32 CSkyLightNishita::OpticalLUTIndex(uint32 heightIndex, uint32 cosVertAngleIndex) const
{
	assert(heightIndex < cOLUT_HeightSteps && cosVertAngleIndex < cOLUT_AngularSteps);
	return(heightIndex * cOLUT_AngularSteps + cosVertAngleIndex);
}

bool CSkyLightNishita::LoadOpticalLUTs()
{
	ICryPak* pPak(C3DEngine::GetPak());
	if (0 != pPak)
	{
		FILE* f(pPak->FOpen(c_lutFileName, "rb"));
		if (0 != f)
		{
			size_t itemsRead(0);

			// read in file tag
			uint32 fileTag(0);
			itemsRead = pPak->FRead(&fileTag, 1, f);
			if (itemsRead != 1 || fileTag != c_lutFileTag)
			{
				// file tag mismatch
				pPak->FClose(f);
				return(false);
			}

			// read in file format version
			uint32 fileVersion(0);
			itemsRead = pPak->FRead(&fileVersion, 1, f);
			if (itemsRead != 1 || fileVersion != c_lutFileVersion)
			{
				// file version mismatch
				pPak->FClose(f);
				return(false);
			}

			// read in optical depth LUT
			m_opticalDepthLUT.resize(cOLUT_HeightSteps * cOLUT_AngularSteps);
			itemsRead = pPak->FRead(&m_opticalDepthLUT[0], m_opticalDepthLUT.size(), f);
			if (itemsRead != m_opticalDepthLUT.size())
			{
				pPak->FClose(f);
				return(false);
			}

			// read in optical scale LUT
			m_opticalScaleLUT.resize(cOLUT_HeightSteps);
			itemsRead = pPak->FRead(&m_opticalScaleLUT[0], m_opticalScaleLUT.size(), f);
			if (itemsRead != m_opticalScaleLUT.size())
			{
				pPak->FClose(f);
				return(false);
			}

			// check if we read entire file
			long curPos(pPak->FTell(f));
			pPak->FSeek(f, 0, SEEK_END);
			long endPos(pPak->FTell(f));
			if (curPos != endPos)
			{
				pPak->FClose(f);
				return(false);
			}

			// LUT successfully read
			pPak->FClose(f);
			return(true);
		}
	}

	return(false);
}

void CSkyLightNishita::SaveOpticalLUTs() const
{
	// only save on little endian PCs so the load function can do proper endian swapping
#if CRY_PLATFORM_WINDOWS
	ICryPak* pPak(C3DEngine::GetPak());
	if (0 != pPak)
	{
		FILE* f(pPak->FOpen(c_lutFileName, "wb"));
		if (0 != f)
		{
			// write out file tag
			pPak->FWrite(&c_lutFileTag, 1, sizeof(c_lutFileTag), f);

			// write out file format version
			pPak->FWrite(&c_lutFileVersion, 1, sizeof(c_lutFileVersion), f);

			// write out optical depth LUT
			assert(m_opticalDepthLUT.size() == cOLUT_HeightSteps * cOLUT_AngularSteps);
			pPak->FWrite(&m_opticalDepthLUT[0], 1, sizeof(SOpticalDepthLUTEntry) * m_opticalDepthLUT.size(), f);

			// write out optical scale LUT
			assert(m_opticalScaleLUT.size() == cOLUT_HeightSteps);
			pPak->FWrite(&m_opticalScaleLUT[0], 1, sizeof(SOpticalScaleLUTEntry) * m_opticalScaleLUT.size(), f);

			// close file
			pPak->FClose(f);
		}
	}
#endif
}
