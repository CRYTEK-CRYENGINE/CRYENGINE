// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Includes
#include "StdAfx.h"
#include <CryRenderer/RenderElements/CREBreakableGlass.h>

#include "Utils/PolygonMath2D.h"
#include "Utils/SpatialHashGrid.h"

#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/IRenderNode.h>
#include <CryParticleSystem/IParticles.h>
#include <CryRenderer/IRenderAuxGeom.h>

// Statics
CCryNameR CREBreakableGlass::s_ImpactDecalParamName("");
const SBreakableGlassCVars* CREBreakableGlass::s_pCVars = NULL;
float CREBreakableGlass::s_loosenTimer = 0.0f;
float CREBreakableGlass::s_impactTimer = 0.0f;

// Forward declare constant "CBreakableGlassBuffer::NoBuffer"
#define NO_BUFFER 0

// Profiling
#ifndef GLASS_FUNC_PROFILER
// #ifndef RELEASE
//  #define GLASS_FUNC_PROFILER			CRY_PROFILE_FUNCTION(PROFILE_GLASS)
//  #define GLASS_PROFILE_ENABLED
//  #define GLASS_PROFILE_AUTO_BREAK
// #else
	#define GLASS_FUNC_PROFILER
//#endif
#endif

// Debug drawing
#ifdef GLASS_DEBUG_MODE
	#define IMPACT_SIZE              0.01f
	#define IMPACT_HALF_SIZE         (IMPACT_SIZE * 0.5f)
	#define IMPACT_NUM_IMPULSE_SIDES 10
#endif

// Error logging
#ifndef RELEASE
	#define LOG_GLASS_ERROR(...) CryLogAlways("[BreakGlassSystem Error]: " __VA_ARGS__)
#else
	#define LOG_GLASS_ERROR(...)
#endif

// RGBA vertex layout
enum EVertRGBAIndex
{
	EColIdx_R = 2,
	EColIdx_G = 1,
	EColIdx_B = 0,
	EColIdx_A = 3,
};

//--------------------------------------------------------------------------------------------------
// Name: CREBreakableGlass
// Desc: Constructor
//--------------------------------------------------------------------------------------------------
CREBreakableGlass::CREBreakableGlass()
	: m_invMaxGlassSize(0.0f)
	, m_impactEnergy(0.0f)
	, m_geomBufferId(NO_BUFFER)
	, m_seed(0)
	, m_fragsActive(0)
	, m_fragsLoose(0)
	, m_fragsFree(0)
	, m_lastFragBit(0)
	, m_numDecalImpacts(0)
	, m_numDecalPSConsts(0)
	, m_pointArrayCount(0)
	, m_lastPhysFrag(0)
	, m_totalNumFrags(0)
	, m_lastFragIndex(0)
	, m_numLooseFrags(0)
	, m_shattered(false)
	, m_geomDirty(false)
	, m_geomRebuilt(false)
	, m_geomBufferLost(false)
	, m_dirtyGeomBufferCount(0)
	, m_geomUpdateFrame(0)
#if GLASSCFG_USE_HASH_GRID
	, m_pHashGrid(NULL)
#endif
{
#ifndef RELEASE
	// Data size assumption/optimization validations
	TGlassDefFragmentArray sizeTestArray1;
	TGlassFragmentIndexArray sizeTestArray2;

	static_assert(GLASSCFG_FRAGMENT_ARRAY_SIZE <= 32, "Invalid array size!");
	CRY_ASSERT(POLY_ARRAY_SIZE == sizeTestArray1.max_size());
	CRY_ASSERT(POLY_ARRAY_SIZE == sizeTestArray2.max_size());
#endif

	// Default data
	memset(m_pointArray, 0, sizeof(Vec2) * GLASSCFG_MAX_NUM_CRACK_POINTS);

	for (uint i = 0; i < GLASSCFG_MAX_NUM_PHYS_FRAGMENTS; ++i)
	{
		m_fragGeomRebuilt[i] = false;
	}

	SetHashGridSize(Vec2Constants<float>::fVec2_One);

	// Initially all frag indices are free
	uint32 fragBit = 1;
	for (uint i = 0; i < GLASSCFG_FRAGMENT_ARRAY_SIZE; ++i, fragBit <<= 1)
	{
		m_frags.push_back(SGlassFragment());
		m_freeFragIndices.push_back(i);
		m_fragsFree |= fragBit;
	}

	// Mark fragment geom buffers with invalid Ids
	for (uint i = 0; i < GLASSCFG_MAX_NUM_PHYS_FRAGMENTS; ++i)
	{
		m_fragGeomBufferIds[i].m_fragId = GLASSCFG_FRAGMENT_ARRAY_SIZE;
		m_fragGeomBufferIds[i].m_geomBufferId = NO_BUFFER;
	}

	// Set invisible/invalid data for decal shader constants
	const float invalidUVPosition = -100.0f;
	const float invalidUVScale = 0.01f;

	for (uint i = 0; i < GLASSCFG_MAX_NUM_IMPACT_DECALS; ++i)
	{
		m_decalPSConsts[i].decal.x = m_decalPSConsts[i].decal.y = invalidUVPosition;
		m_decalPSConsts[i].decal.z = m_decalPSConsts[i].decal.w = invalidUVScale;
	}

	// Ensure shader param name is set
	if (s_ImpactDecalParamName.length() == 0)
	{
		s_ImpactDecalParamName = "gImpactDecals";
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ~CREBreakableGlass
// Desc: Destructor (Called from RT)
//--------------------------------------------------------------------------------------------------
CREBreakableGlass::~CREBreakableGlass()
{
#if GLASSCFG_USE_HASH_GRID
	SAFE_DELETE(m_pHashGrid);
#endif
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: InitialiseRenderElement
// Desc: Initializes render element
//--------------------------------------------------------------------------------------------------
bool CREBreakableGlass::InitialiseRenderElement(const SBreakableGlassInitParams& params)
{
#if GLASSCFG_USE_HASH_GRID
	if (m_pHashGrid)
	{
		CRY_ASSERT_MESSAGE(0, "Glass render element initialised twice.");
		LOG_GLASS_ERROR("Element initialised twice.");
	}
#endif

	// Copy data
	memcpy(&m_glassParams, &params, sizeof(SBreakableGlassInitParams));

	// Assert minimum size data
	const float minGlassSize = 0.05f;
	m_glassParams.size.x = max(m_glassParams.size.x, minGlassSize);
	m_glassParams.size.y = max(m_glassParams.size.y, minGlassSize);

	m_invMaxGlassSize = 1.0f / max(m_glassParams.size.x, m_glassParams.size.y);

	// Calculate uv coord range
	Vec2 uvPt0 = m_glassParams.uvOrigin;
	Vec2 uvPt1 = m_glassParams.uvOrigin + m_glassParams.uvXAxis * m_glassParams.size.x;
	Vec2 uvPt2 = m_glassParams.uvOrigin + m_glassParams.uvYAxis * m_glassParams.size.y;
	Vec2 uvPt3 = m_glassParams.uvOrigin + m_glassParams.uvXAxis * m_glassParams.size.x + m_glassParams.uvYAxis * m_glassParams.size.y;

	Vec2 uvMin, uvMax;
	uvMin.x = min(min(uvPt0.x, uvPt1.x), min(uvPt2.x, uvPt3.x));
	uvMin.y = min(min(uvPt0.y, uvPt1.y), min(uvPt2.y, uvPt3.y));
	uvMax.x = max(max(uvPt0.x, uvPt1.x), max(uvPt2.x, uvPt3.x));
	uvMax.y = max(max(uvPt0.y, uvPt1.y), max(uvPt2.y, uvPt3.y));

	m_invUVRange = uvMax - uvMin;
	m_invUVRange.x = 1.0f / max(m_invUVRange.x, 0.01f);
	m_invUVRange.y = 1.0f / max(m_invUVRange.y, 0.01f);

	// Pre-factor (uv-space) glass size into range
	Vec2 uvXAxis = m_glassParams.uvXAxis.GetNormalized();
	Vec2 uvYAxis = m_glassParams.uvYAxis.GetNormalized();

	Vec2 uvGlassSize;
	uvGlassSize.x = m_glassParams.size.x * uvXAxis.x + m_glassParams.size.y * uvYAxis.x;
	uvGlassSize.y = m_glassParams.size.x * uvXAxis.y + m_glassParams.size.y * uvYAxis.y;

	m_invUVRange.x *= uvGlassSize.x;
	m_invUVRange.y *= uvGlassSize.y;

	// Create hash grid
#if GLASSCFG_USE_HASH_GRID
	if (!m_pHashGrid)
	{
		m_pHashGrid = new TGlassHashGrid(params.size.x, params.size.y);
		m_pHashGrid->ClearGrid();
	}
#endif

	// Re-calculate hash cell sizes
	SetHashGridSize(m_glassParams.size);

	// Reset glass state
	m_shattered = false;
	m_geomDirty = false;
	m_geomRebuilt = false;
	m_dirtyGeomBufferCount = 0;
	m_fragsActive = 0;
	m_fragsLoose = 0;

	for (uint i = 0; i < GLASSCFG_MAX_NUM_PHYS_FRAGMENTS; ++i)
	{
		m_fragGeomRebuilt[i] = false;
	}

	// Create initial mesh
	if (!GenerateGeomFromFragment(params.pInitialFrag, params.numInitialFragPts))
	{
		GenerateDefaultPlaneGeom();
	}

	return true;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ReleaseRenderElement
// Desc: Initializes render element
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::ReleaseRenderElement()
{
#if GLASSCFG_USE_HASH_GRID
	// Hash grid and element data
	SAFE_DELETE(m_pHashGrid);
#endif

	// "False" here allows a delayed delete, avoiding any issues of it being mid draw-call etc.
	Release(false);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Update
// Desc: Propels loose pieces away from plane to the ground
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::Update(SBreakableGlassUpdateParams& params)
{
	// Periodically loosen weakly-linked fragments
	if (s_loosenTimer > 1.0f + GetRandF() && s_impactTimer > 1.0f && s_impactTimer < 2.0f)
	{
		const uint32 activeState = m_fragsActive & ~m_fragsFree;
		uint32 fragBit = 1;

		for (uint i = 0; i < GLASSCFG_FRAGMENT_ARRAY_SIZE; ++i, fragBit <<= 1)
		{
			const SGlassFragment& frag = m_frags[i];

			if (activeState & fragBit && IsFragmentWeaklyLinked(frag))
			{
				// Force update as we will loosen this fragment
				m_geomDirty = true;
				break;
			}
		}
	}

	s_loosenTimer += params.m_frametime;
	s_impactTimer += params.m_frametime;

	// Fill return flags before they are cleared
	params.m_geomChanged = m_geomDirty;

#ifdef GLASS_PROFILE_AUTO_BREAK
	static int autoBreak = 0;
	uint32 fragBit = 1;
	SGlassImpactParams impact;

	switch (autoBreak)
	{
	case 3:
		ResetTempCrackData();

		impact = m_impactParams.back();
		impact.seed = 0;

		m_impactParams.clear();
		m_frags.clear();
		m_freeFragIndices.clear();
		m_fragsFree = 0;

		for (uint i = 0; i < GLASSCFG_FRAGMENT_ARRAY_SIZE; ++i, fragBit <<= 1)
		{
			m_frags.push_back(SGlassFragment());
			m_freeFragIndices.push_back(i);
			m_fragsFree |= fragBit;
		}

		m_fragsActive = 0;
		m_fragsLoose = 0;
		m_lastFragBit = 0;
		m_lastPhysFrag = 0;
		m_totalNumFrags = 0;
		m_lastFragIndex = 0;
		m_numLooseFrags = 0;
		m_numDecalImpacts = 0;
		m_numDecalPSConsts = 0;
		m_shattered = false;
		m_geomDirty = false;
		m_geomRebuilt = false;
		m_geomBufferLost = false;
		m_dirtyGeomBufferCount = 0;

		m_geomBufferId = NO_BUFFER;

		for (uint i = 0; i < GLASSCFG_MAX_NUM_PHYS_FRAGMENTS; ++i)
		{
			m_fragGeomBufferIds[i].m_fragId = GLASSCFG_FRAGMENT_ARRAY_SIZE;
			m_fragGeomBufferIds[i].m_geomBufferId = NO_BUFFER;
			m_fragGeomRebuilt[i] = false;
		}

		for (uint i = 0; i < GLASSCFG_MAX_NUM_IMPACT_DECALS; ++i)
		{
			m_decalPSConsts[i].decal.x = m_decalPSConsts[i].decal.y = -100.0f;
			m_decalPSConsts[i].decal.z = m_decalPSConsts[i].decal.w = 0.01f;
		}

	#ifdef GLASS_DEBUG_MODE
		m_impactLineList.clear();
	#endif

		GenerateDefaultPlaneGeom();
		break;

	case 6:
		impact.velocity = Vec3(-930.58624, -442.86945, 384.53485);
		impact.x = 0.29331410;
		impact.y = 1.3911150;
		impact.impulse = 308.00000;
		impact.speed = 1099.9960;
		impact.radius = 0.0099999998;
		impact.seed = 3527700152;
		ApplyImpactToGlass(impact);
		break;

	case 9:
		impact.velocity = Vec3(-1085.3186, 78.216599, 161.12334);
		impact.x = 2.3538542;
		impact.y = 0.37980682;
		impact.impulse = 308.00000;
		impact.speed = 1099.9978;
		impact.radius = 0.0099999998;
		impact.seed = 2126611280;
		ApplyImpactToGlass(impact);
		break;

	case 12:
		impact.velocity = Vec3(-1047.5664, -277.38022, 188.83971);
		impact.x = 0.96540481;
		impact.y = 0.51500261;
		impact.impulse = 308.00000;
		impact.speed = 1099.9979;
		impact.radius = 0.0099999998;
		impact.seed = 1035809870;
		ApplyImpactToGlass(impact);
		break;

	case 15:
		impact.velocity = Vec3(-986.25885, 144.72720, 465.11740);
		impact.x = 2.5825634;
		impact.y = 1.5812253;
		impact.impulse = 308.00000;
		impact.speed = 1099.9939;
		impact.radius = 0.0099999998;
		impact.seed = 381451321;
		ApplyImpactToGlass(impact);
		break;

	case 18:
		impact.velocity = Vec3(-910.84277, -478.89627, 388.60526);
		impact.x = 0.11862427;
		impact.y = 1.4369123;
		impact.impulse = 308.00000;
		impact.speed = 1099.9956;
		impact.radius = 0.0099999998;
		impact.seed = 3410067940;
		ApplyImpactToGlass(impact);
		break;

	case 21:
		impact.velocity = Vec3(-1001.4235, -436.36160, 129.36899);
		impact.x = 0.21810441;
		impact.y = 0.29683763;
		impact.impulse = 308.00000;
		impact.speed = 1099.9985;
		impact.radius = 0.0099999998;
		impact.seed = 2357971410;
		ApplyImpactToGlass(impact);
		break;

	case 24:
		impact.velocity = Vec3(-1079.0485, -41.873333, 209.51102);
		impact.x = 1.8947206;
		impact.y = 0.57215804;
		impact.impulse = 308.00000;
		impact.speed = 1099.9972;
		impact.radius = 0.0099999998;
		impact.seed = 2609387782;
		ApplyImpactToGlass(impact);
		break;

	case 27:
		impact.velocity = Vec3(-1072.4220, 144.37529, 197.63870);
		impact.x = 2.6052594;
		impact.y = 0.53135908;
		impact.impulse = 308.00000;
		impact.speed = 1099.9974;
		impact.radius = 0.0099999998;
		impact.seed = 3082654461;
		ApplyImpactToGlass(impact);
		break;
	}

	static int updateAuto = 0;
	autoBreak += (updateAuto == 0 || (autoBreak % 3) == 0) ? 1 : 0;
	updateAuto = (updateAuto + 1) % 7;
	if (autoBreak > 35) { autoBreak = 0; }
#endif // GLASS_PROFILE_AUTO_BREAK

	// Shatter glass if/when we lose our geom buffer
	if (m_geomBufferLost)
	{
		ShatterGlass();
	}

	// Process any physicalized fragments that were destroyed
	if (params.m_pDeadPhysFrags && !params.m_pDeadPhysFrags->empty())
	{
		const uint numDeadFrags = params.m_pDeadPhysFrags->size();
		for (uint i = 0; i < numDeadFrags; ++i)
		{
			const uint16 fragData = params.m_pDeadPhysFrags->at(i);
			const uint8 buffId = fragData & 0xFF;
			const uint8 fragId = (fragData >> 8) & 0xFF;

			if (buffId < GLASSCFG_MAX_NUM_PHYS_FRAGMENTS && m_fragGeomBufferIds[buffId].m_fragId == fragId)
			{
				m_fragGeomBufferIds[buffId].m_fragId = GLASSCFG_FRAGMENT_ARRAY_SIZE;
				m_fragGeomBufferIds[buffId].m_geomBufferId = NO_BUFFER;

				// Not happy here as threading scariness, but we should really deal with
				// the case where we get stuck waiting for an update that's never coming
				//        if (!m_geomRebuilt && m_fragGeomRebuilt[fragId])
				//        {
				//          m_fragGeomRebuilt[fragId] = false;
				//          --m_dirtyGeomBufferCount;
				//        }
			}
		}
	}

	// Regenerate geometry if dirty
	if (m_geomDirty)
	{
		FindLooseFragments(params.m_pPhysFrags, params.m_pPhysFragsInitData);
		RebuildAllFragmentGeom();
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GetGlassState
// Desc: Returns the current state of the glass
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::GetGlassState(SBreakableGlassState& state)
{
	state.m_numImpacts = m_impactParams.size();
	state.m_numLooseFrags = m_numLooseFrags;
	state.m_hasShattered = m_shattered;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetCVars
// Desc: Updates the cvar pointer
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::SetCVars(const SBreakableGlassCVars* pCVars)
{
	s_pCVars = pCVars;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ResetTempCrackData
// Desc: Clears all temporary geom and point data used during crack generation
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::ResetTempCrackData()
{
	memset(m_pointArray, 0, sizeof(Vec2) * GLASSCFG_MAX_NUM_CRACK_POINTS);
	m_pointArrayCount = 0;

	for (uint i = 0; i < GLASSCFG_NUM_RADIAL_CRACKS; ++i)
	{
		m_radialCrackTrees[i].Clear();
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SeedRand
// Desc: Regenerates the internal random values
//--------------------------------------------------------------------------------------------------
static const uint s_gNumRandValues = 32;
static struct
{
	uint32 currValue;
	float  values[s_gNumRandValues];
} s_gRandData;

void CREBreakableGlass::SeedRand()
{
	CRndGen randGen;
	randGen.Seed(m_seed);

	for (uint i = 0; i < s_gNumRandValues; ++i)
	{
		s_gRandData.values[i] = randGen.GenerateFloat();
	}

	s_gRandData.currValue = 0;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GetRandF
// Desc: Retrieves the next random float value
//--------------------------------------------------------------------------------------------------
float CREBreakableGlass::GetRandF()
{
	++s_gRandData.currValue;
	return s_gRandData.values[s_gRandData.currValue & 31]; // Optimized "value % 32"
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GetRandF
// Desc: Retrieves a specific random float value
//--------------------------------------------------------------------------------------------------
float CREBreakableGlass::GetRandF(const uint index)
{
	return s_gRandData.values[index & 31]; // Optimized "index % 32"
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GetClosestImpactDistance
// Desc: Returns the distance to the closest impact point
//--------------------------------------------------------------------------------------------------
float CREBreakableGlass::GetClosestImpactDistance(const Vec2& pt)
{
	GLASS_FUNC_PROFILER

	const uint numImpacts = m_impactParams.size();
	const float radialScale = 0.5f;
	float centerDist = FLT_MAX;

	for (uint i = 0; i < numImpacts; ++i)
	{
		float radius = GetDecalRadius(i) * radialScale;
		Vec2 center = Vec2(m_impactParams[i].x, m_impactParams[i].y);

		float dist = max((center - pt).GetLength() - radius, 0.0f);
		centerDist = min(centerDist, dist);
	}

	return centerDist;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: HandleAdditionalImpact
// Desc: Allows the glass to fully shatter after too many impacts
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::HandleAdditionalImpact(const SGlassImpactParams& params)
{
	// Only impact if there is still stable glass
	const uint numImpacts = m_impactParams.size();

	if (!m_shattered && numImpacts == GLASSCFG_MAX_NUM_IMPACTS)
	{
		ShatterGlass();
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ShatterGlass
// Desc: Instantly knocks all pieces from the glass
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::ShatterGlass()
{
	if (!m_shattered)
	{
		// On shatter, all fragments still active become loose
		m_fragsLoose |= m_fragsActive;

#if GLASSCFG_USE_HASH_GRID
		// No stable fragments means no hashed data
		SAFE_DELETE(m_pHashGrid);
#endif

		// Update state
		m_shattered = true;
		m_geomDirty = true;
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: PlayShatterEffect
// Desc: Spawns/plays glass shatter effects
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::PlayShatterEffect(const uint8 fragIndex)
{
	if (!s_pCVars || s_pCVars->m_particleFXEnable > 0)
	{
		if (m_glassParams.pShatterEffect && !m_impactParams.empty())
		{
			// Get fragment data
			const SGlassFragment& frag = m_frags[fragIndex];
			const Vec2* pFragPts = frag.m_outlinePts.begin();
			const int numFragPts = frag.m_outlinePts.Count();

			if (numFragPts >= 3)
			{
				// Calculate particle spawn parameters
				const Vec2& center = frag.m_center;
				Vec3 worldPos = m_params.matrix.TransformPoint(center);

				Vec3 dir;

				// Velocity from impact or just "up" relative to surface
				if (!m_impactParams.empty())
				{
					dir = m_impactParams.back().velocity.GetNormalizedFast();
				}
				else
				{
					dir = m_params.matrix.GetColumn2();
					LOG_GLASS_ERROR("Creating particle effect but no valid impacts found");
				}

				Vec2 bounds = CalculatePolygonBounds2D(pFragPts, numFragPts);
				float size = (bounds.x + bounds.y) * 0.5f;

				float effectScale = s_pCVars ? s_pCVars->m_particleFXScale : 0.25f;

				// Spawn effect emitter
				SpawnParams spawnParams;
				spawnParams.fCountScale = effectScale * size;
				spawnParams.fSizeScale = size;

				m_glassParams.pShatterEffect->Spawn(ParticleLoc(worldPos, dir), &spawnParams);
			}
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: CalculateBreakThreshold
// Desc: Randomly adjusts the break threshold (biased towards center)
//--------------------------------------------------------------------------------------------------
float CREBreakableGlass::CalculateBreakThreshold()
{
	const float thresholdCenterStr = 0.5f;
	const SGlassImpactParams& currImpact = m_impactParams.back();

	// Adjust threshold to give weaker center
	float planeMaxX = m_glassParams.size.x;
	float planeMaxY = m_glassParams.size.y;

	float xDistFromEdge = planeMaxX - fabs_tpl(currImpact.x - (planeMaxX * 0.5f)) * 2.0f;
	float yDistFromEdge = planeMaxY - fabs_tpl(currImpact.y - (planeMaxY * 0.5f)) * 2.0f;
	float distFromEdge = min(xDistFromEdge, yDistFromEdge);

	float threshold = LERP(1.0f, thresholdCenterStr, distFromEdge * m_invMaxGlassSize);

	// Severely weaken glass if its already been broken
	if (m_impactParams.size() > 1)
	{
		threshold *= 0.0f;
	}

	return threshold;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: CheckForGlassBreak
// Desc: Determines if the impact speed is sufficient to break the glass,
//			 and calculates the excess impact energy
//--------------------------------------------------------------------------------------------------
bool CREBreakableGlass::CheckForGlassBreak(const float breakThreshold, float& excessEnergy)
{
	const float breakSpeedMult = 1.0f / GLASSCFG_PLANE_AVG_SPEED_TO_BREAK;

	const SGlassImpactParams& currImpact = m_impactParams.back();
	float thresholdSpeed = currImpact.speed * breakSpeedMult;

	// Excess energy is speed left over past break threshold
	excessEnergy = max(thresholdSpeed - breakThreshold, 0.0f);

	return (breakThreshold < thresholdSpeed);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: CalculateImpactEnergies
// Desc: Partitions input energy towards forming radial or circular cracks
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::CalculateImpactEnergies(const float totalEnergy, float& radialEnergy, float& circularEnergy)
{
	// General idea is lower impact collisions cause large radial cracks. As total energy increases,
	// radial cracks become shorter and more concentrated circular cracks begin to appear. Happens
	// as faster object spends less time in contact with plane (impulse=f*t), causing glass to dissipate
	// less energy bending (causing radial cracks), instead breaking at a concentrated point
	const float circularFadeIn = 1.0f;
	const float radialFadeOut = 3.5f;
	const float minRadialEnergy = 0.2f;

	// Energy can be fully radial, fully circular or a blend
	if (totalEnergy < circularFadeIn)
	{
		radialEnergy = totalEnergy;
		circularEnergy = 0.0f;
	}
	else if (totalEnergy > radialFadeOut)
	{
		radialEnergy = minRadialEnergy;
		circularEnergy = totalEnergy;
	}
	else
	{
		float blend = (totalEnergy - circularFadeIn) / (radialFadeOut - circularFadeIn);
		circularEnergy = totalEnergy * blend;
		radialEnergy = max(minRadialEnergy, totalEnergy - circularEnergy);
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ApplyImpactToGlass
// Desc: Splits and updates the impacted glass fragments
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::ApplyImpactToGlass(const SGlassImpactParams& params)
{
	GLASS_FUNC_PROFILER

	// Store param data even if this impact is about to cause a full shatter
	if (!m_shattered)
	{
		m_impactParams.push_back(params);
	}

	// Potentially shatter on new impact
	HandleAdditionalImpact(params);

	if (!m_shattered)
	{
		// Pre-calculate randomised params
		SGlassImpactParams& currImpact = m_impactParams.back();

		if (m_impactParams.size() == 1)
		{
			m_seed = currImpact.seed;
		}

		SeedRand();

		// Check if impact actually breaks the glass
		float breakThreshold = CalculateBreakThreshold();
		float excessEnergy = 0.0f;
		bool impactHit = false;

		if (CheckForGlassBreak(breakThreshold, excessEnergy))
		{
			m_impactEnergy = excessEnergy;

			// Force all very fast objects to make a hole
			if (currImpact.speed > GLASSCFG_MIN_BULLET_SPEED)
			{
				currImpact.impulse = max(currImpact.impulse, GLASSCFG_PLANE_SPLIT_IMPULSE * GLASSCFG_MIN_BULLET_HOLE_SCALE);
			}

			// Apply impact
			if (ApplyImpactToFragments(currImpact))
			{
#ifdef GLASS_DEBUG_MODE
				GenerateImpactGeom(currImpact);
#endif
				impactHit = true;
				s_impactTimer = 0.0f;
				s_loosenTimer = 0.0f;

				// Check if we still have any stable geometry left
				const uint32 stableFrags = m_fragsActive & ~(m_fragsLoose | m_fragsFree);

				if (stableFrags == 0)
				{
					m_shattered = true;
				}

				// Rebuild geometry if changed
				m_geomDirty = true;
			}
		}

		// Undo impact if it didn't actually collide
		if (!impactHit)
		{
			m_impactParams.pop_back();
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ApplyExplosionToGlass
// Desc: Knocks out all fragments in the explosion, and splits those clipping
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::ApplyExplosionToGlass(const SGlassImpactParams& params)
{
	GLASS_FUNC_PROFILER

	// Store param data even if this impact is about to cause a full shatter
	if (!m_shattered)
	{
		m_impactParams.push_back(params);
	}

	// Potentially shatter on new impact
	HandleAdditionalImpact(params);

	if (!m_shattered)
	{
#if GLASSCFG_SHATTER_ON_EXPLOSION
		// Auto-shatter
		ShatterGlass();
		bool impactHit = true;

#else
		// Calculate explosion details
		bool impactHit = false;

		const Vec2 center(params.x, params.y);

		const float radiusVariation = 0.3f;
		const float radius = GetImpactRadius(params) * (1.0f + GetRandF() * radiusVariation);

	#if GLASSCFG_SPLIT_ON_EXPLOSION
		// Pre-calculate randomised params
		SGlassImpactParams& currImpact = m_impactParams.back();
		SeedRand();

		// Firstly split as with regular impact, as this leads to nicer fragment patterns
		if (ApplyImpactToFragments(currImpact))
		{
			impactHit = true;
		}
	#endif

		// Pre-evaluate fragment state bits
		const uint32 validState = m_fragsActive & ~(m_fragsLoose | m_fragsFree);
		uint32 fragBit = 1;

		// Check all active fragments against explosion
		// Note: Would be nicer to simply walk connections from impact fragment,
		// which should avoid lots of redundant checks. May find with explosion
		// being so large however, that almost all fragments get clipped anyway!
		TPolygonArray tempFragShape;
		TPolygonArray splitFragShape;

		for (uint i = 0; i < GLASSCFG_FRAGMENT_ARRAY_SIZE; ++i, fragBit <<= 1)
		{
			if (validState & fragBit)
			{
				// Clip fragment against explosion radius
				EPolygonInCircle2D state = PolygonInCircle2D(center, radius, m_frags[i].m_outlinePts.begin(), m_frags[i].m_outlinePts.Count());

				if (state == EPolygonInCircle2D_Inside)
				{
					// Instantly remove
					impactHit = true;
					m_fragsLoose |= fragBit;
				}
				else if (state == EPolygonInCircle2D_Overlap)
				{
					impactHit = true;

					// Remove hash grid references now, as shape will change
					const bool remove = true;
					HashFragment(i, remove);

					// Apply split and create new polygon if necessary
					splitFragShape.clear();
					SplitPolygonAroundPoint(m_frags[i].m_outlinePts, center, radius, splitFragShape);

					const int noParent = 0;
					const bool forceLoose = true;
					CreateFragmentFromOutline(splitFragShape.begin(), splitFragShape.size(), noParent, forceLoose);

					// Rebuild remaining fragment section
					RebuildChangedFragment(i);
				}
			}
		}
#endif // GLASSCFG_SHATTER_ON_EXPLOSION

		// Need to rebuild geometry if hit
		if (impactHit)
		{
#ifdef GLASS_DEBUG_MODE
			GenerateImpactGeom(params);
#endif
			m_geomDirty = true;
		}
		// Undo impact if it didn't actually collide
		else
		{
			m_impactParams.pop_back();
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GenerateCracksFromImpact
// Desc: Generates temporary crack data from the current impact points
//--------------------------------------------------------------------------------------------------
uint CREBreakableGlass::GenerateCracksFromImpact(const uint8 parentFragIndex, const SGlassImpactParams& impact)
{
	GLASS_FUNC_PROFILER

	// Clear old data
	  ResetTempCrackData();

	// Determine crack generation depth
	const uint impactDepth = m_frags[parentFragIndex].m_depth;
	const float fragArea = m_frags[parentFragIndex].m_area;

	// Generate crack data from impact point
	m_pointArray[0].x = impact.x;
	m_pointArray[0].y = impact.y;
	m_pointArrayCount = 1;

	const uint numCracks = GenerateRadialCracks(m_impactEnergy, impactDepth, fragArea);
	return numCracks;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: PropagateRadialCrack
// Desc: Adds a new radial crack to the end of the list. Returns energy consumed
//--------------------------------------------------------------------------------------------------
float CREBreakableGlass::PropagateRadialCrack(const uint startIndex, const uint currIndex, const uint localIndex, const float angle, const float energyPerStep)
{
	GLASS_FUNC_PROFILER

	const float energyStepVariance = 0.015f;
	const float globalEnergyMultipler = 4.0f;

	// Randomise energy used for this step slightly
	float energyStep = 1.0f + (GetRandF() * energyStepVariance) * 0.5f + 0.5f;
	energyStep *= energyPerStep;

	float energyUsed = energyStep * (globalEnergyMultipler + localIndex * localIndex) * GLASSCFG_SIMPLIFY_CRACKS;

	// Get points
	Vec2& startPt = m_pointArray[startIndex];
	Vec2& currPt = m_pointArray[currIndex];

	// Calculate position data
	float sinA, cosA;
	sincos_tpl(angle, &sinA, &cosA);

	currPt.x = startPt.x + cosA * energyUsed;
	currPt.y = startPt.y + sinA * energyUsed;

	// For now, we always want cracks to continue until they clip
	energyStep = 0.0f;

	// Get plane bounds
	const float planeMin = 0.0f;
	const float planeMaxX = m_glassParams.size.x;
	const float planeMaxY = m_glassParams.size.y;

	// Check plane bounds, and end crack if hit
	//	-Note: Points are always different, so will not get divby0 error
	const float useAllEnergy = 1000.0f;

	if (currPt.x < planeMin || currPt.x > planeMaxX)
	{
		float bound = currPt.x < planeMin ? planeMin : planeMaxX;
		float excessDelta = (currPt.x - bound) / (currPt.x - startPt.x);
		currPt.y -= (currPt.y - startPt.y) * excessDelta;

		currPt.x = clamp_tpl<float>(currPt.x, planeMin, planeMaxX);
		energyStep = useAllEnergy;
	}

	if (currPt.y < planeMin || currPt.y > planeMaxY)
	{
		float bound = currPt.y < planeMin ? planeMin : planeMaxY;
		float excessDelta = (currPt.y - bound) / (currPt.y - startPt.y);
		currPt.x -= (currPt.x - startPt.x) * excessDelta;

		currPt.y = clamp_tpl<float>(currPt.y, planeMin, planeMaxY);
		energyStep = useAllEnergy;
	}

	// Additional point generated
	++m_pointArrayCount;

	// Return energy consumed
	return energyStep;
}

//--------------------------------------------------------------------------------------------------
// Name: GenerateRadialCracks
// Desc: Generates radial point data formed by cracks perpendicular to the impact point
//--------------------------------------------------------------------------------------------------
uint CREBreakableGlass::GenerateRadialCracks(const float totalEnergy, const uint impactDepth, const float fragArea)
{
	GLASS_FUNC_PROFILER

	const uint minCracksToBreak = 3;
	const float minEnergyPerStep = 0.005f;
	const float maxEnergyPerStep = 0.1f;
	const float energyStepRelaxRate = 2.5f;
	const float randAngle = gf_PI * 0.8f;
	const float angleStepAccel = 0.15f;

	// The higher the energy, the easier it is for cracks to appear as they become smaller
	float energyStepRelaxation = clamp_tpl<float>(totalEnergy * energyStepRelaxRate, 0.0f, 1.0f);
	float energyPerStep = LERP(maxEnergyPerStep, minEnergyPerStep, energyStepRelaxation);

	// Already decide to break, so clamp to bounds regardless of energy
	const uint numCracks = GLASSCFG_NUM_RADIAL_CRACKS;

	//if (numCracks >= minCracksToBreak)
	{
		// Divide energy amongst cracks
		float fNumCracks = (float)numCracks;
		float currentEnergyPerCrack = totalEnergy / fNumCracks;

		// Calculate per-crack angle increment
		const float angleInc = gf_PI2 / fNumCracks;
		const float halfCosAngleInc = 2.0f / fNumCracks;
		const float randAnglePerInc = randAngle / fNumCracks;
		const float halfRandAnglePerInc = randAnglePerInc * 0.5f;

		float crackAngle = GetRandF() * gf_PI2;

		// All radial cracks begin at central impact point
		Vec2* pCenterPt = &m_pointArray[0];
		uint ptIndex = m_pointArrayCount;

		// Generate initial cracks
		for (uint i = 0; i < numCracks; ++i)
		{
			// Add center point to tree lists
			m_radialCrackTrees[i].AddNew() = pCenterPt;

			// Increment angle, then randomize local offset
			crackAngle += angleInc;
			float angle = crackAngle + (GetRandF() * randAnglePerInc - halfRandAnglePerInc);

			// Reset for new crack
			float crackEnergy = max(currentEnergyPerCrack, energyPerStep);
			float localAngle = angle;

			Vec2* pStartPt = pCenterPt;
			Vec2 initialDir;
			uint startPtIndex = 0;
			uint crackStepIndex = 0;

			while (crackEnergy >= energyPerStep)
			{
				float totalEnergyStep = 0.0f;

				// Randomise angle for step, further steps getting more aggressive
				float localAngleStep = GetRandF() * randAnglePerInc - halfRandAnglePerInc;
				localAngleStep *= 1.0f + crackStepIndex * angleStepAccel;
				localAngle += localAngleStep;

				// If getting too far away from local sector, need to return to avoid intersections
				if (crackStepIndex > 1)
				{
					float sinA, cosA;
					sincos_tpl(angle, &sinA, &cosA);

					const Vec2 nextPt(pStartPt->x + cosA, pStartPt->y + sinA);
					const Vec2 nextDir = (nextPt - *pCenterPt).GetNormalized();
					const float cosAngle = nextDir.Dot(initialDir);

					if (cosAngle < halfCosAngleInc)
					{
						localAngle -= localAngleStep * 2.0f;
					}
				}

				// Create new point
				totalEnergyStep = PropagateRadialCrack(startPtIndex, ptIndex, crackStepIndex, localAngle, energyPerStep);

				// Dissipate energy
				crackEnergy -= totalEnergyStep;

				// Push new point into radial tree
				m_radialCrackTrees[i].AddNew() = &m_pointArray[ptIndex];

				// Prepare for next step
				pStartPt = &m_pointArray[ptIndex];
				startPtIndex = ptIndex;
				++ptIndex;
				++crackStepIndex;

				// Record initial direction
				if (crackStepIndex == 1)
				{
					initialDir = (*pStartPt - *pCenterPt).GetNormalized();
				}

				// Assert boundaries
				if (ptIndex >= GLASSCFG_MAX_NUM_CRACK_POINTS)
				{
					LOG_GLASS_ERROR("Allocated too many crack points, need to expand array!");
					return 0;
				}
			}
		}
	}

	// Circular cracks need to be aware of shape
	return numCracks;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GetImpactRadius
// Desc: Calculates the radius of an impact
//--------------------------------------------------------------------------------------------------
float CREBreakableGlass::GetImpactRadius(const SGlassImpactParams& impact)
{
	// Now using fixed minimum impulse, rather than phys event approximation
	const float minImpulse = GLASSCFG_PLANE_SPLIT_IMPULSE * GLASSCFG_MIN_BULLET_HOLE_SCALE;

	// Calculate minimum radius
	float minSplitRadius = (minImpulse /*impact.impulse*/ - GLASSCFG_PLANE_SPLIT_IMPULSE) * GLASSCFG_PLANE_SPLIT_IMPULSE_STRENGTH;
	minSplitRadius = (minSplitRadius + minSplitRadius * minSplitRadius) * 0.5f; // Square here to change behaviour, not to get sq radius

	return max(minSplitRadius, impact.radius);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GetDecalRadius
// Desc: Calculates the radius of an impact decal
//--------------------------------------------------------------------------------------------------
float CREBreakableGlass::GetDecalRadius(const uint8 impactIndex)
{
	// Get impact size multipliers
	float minImpactSpeed = s_pCVars ? s_pCVars->m_minImpactSpeed : GLASSCFG_MIN_BULLET_SPEED;
	float minImpactRadius = s_pCVars ? s_pCVars->m_decalMinRadius : 0.25f;
	float maxImpactRadius = s_pCVars ? s_pCVars->m_decalMaxRadius : 1.25f;
	float randDecalChance = s_pCVars ? s_pCVars->m_decalRandChance : 0.67f;
	float randDecalScale = s_pCVars ? s_pCVars->m_decalRandScale : 1.6f;
	float impactScale = s_pCVars ? max(s_pCVars->m_decalScale, 0.01f) : 2.5f;

	// Calculate scale
	const SGlassImpactParams& impact = m_impactParams[impactIndex];
	float decalRadius = GetImpactRadius(impact) * impactScale;
	decalRadius = clamp_tpl<float>(decalRadius, minImpactRadius, maxImpactRadius);
	decalRadius *= impactScale * 0.5f;

	bool isBullet = (impact.speed >= minImpactSpeed);
	bool isLargeDecal = (isBullet && GetRandF(impactIndex) >= randDecalChance);
	decalRadius *= isLargeDecal ? randDecalScale : 1.0f;

	return decalRadius;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ApplyImpactToFragments
// Desc: Allows multiple impacts to remove triangles from the mesh
//--------------------------------------------------------------------------------------------------
bool CREBreakableGlass::ApplyImpactToFragments(SGlassImpactParams& impact)
{
	GLASS_FUNC_PROFILER

	// Find the impact fragment (if any)
	uint8 impactFrag = 0;
	bool foundFrag = FindImpactFragment(impact, impactFrag);

	if (foundFrag)
	{
		uint32 fragBit = (uint32)1 << impactFrag;
		const uint impactIndex = m_impactParams.size() - 1;

		// Shift impact slightly towards center of fragment to avoid
		// incredibly rare case where point is *exactly* on fragment edge
		SGlassFragment& frag = m_frags[impactFrag];
		const Vec2& impactFragCenter = frag.m_center;
		const Vec2 impactCenter = Vec2(impact.x, impact.y);

		Vec2 impactToFragDir = impactFragCenter - impactCenter;
		impactToFragDir.Normalize();

		const float impactPtShift = 0.0001f;
		impact.x += impactToFragDir.x * impactPtShift;
		impact.y += impactToFragDir.y * impactPtShift;

		// With larger impacts, randomly knock out a few surrounding fragments
		const float extraImpactRadius = 0.2f;
		if (impact.radius >= extraImpactRadius)
		{
			// Randomly rotated impact points
#ifdef GLASSCFG_HIGH_QUALITY_MODE
			const uint numExtraImpacts = 5;
			const float angleInc = 1.0f / 5.0f;
#else
			const uint numExtraImpacts = 3;
			const float angleInc = 1.0f / 3.0f;
#endif
			const float offset = GetRandF();

			// Fixed maximum range for extra impacts
			const float maxExtraImpactRadius = 0.4f;
			SGlassImpactParams extraImpact = impact;
			extraImpact.radius = min(impact.radius, maxExtraImpactRadius);

			for (uint i = 0; i < numExtraImpacts; ++i)
			{
				// Jitter impact position
				float sinA, cosA;
				sincos_tpl((angleInc * i + offset) * gf_PI2, &sinA, &cosA);

				extraImpact.x = impact.x + sinA * extraImpact.radius;
				extraImpact.y = impact.y + cosA * extraImpact.radius;

				// Knock out hit fragment, if it's not the one we're splitting
				uint8 extraImpactFrag = GLASSCFG_MAX_NUM_FRAGMENTS;
				if (FindImpactFragment(extraImpact, extraImpactFrag) && extraImpactFrag != impactFrag)
				{
					fragBit = (uint32)1 << extraImpactFrag;
					m_fragsLoose |= fragBit;

					RemoveFragmentConnections(extraImpactFrag, m_frags[extraImpactFrag].m_outConnections);
					RemoveFragmentConnections(extraImpactFrag, m_frags[extraImpactFrag].m_inConnections);

#ifdef GLASS_DEBUG_MODE
					// Add debug impact geom
					extraImpact.radius = 0.025f;
					GenerateImpactGeom(extraImpact);
#endif
				}
			}
		}

		// If possible, split the fragment
		if (impactFrag < GLASSCFG_MAX_NUM_FRAGMENTS)
		{
			if (m_totalNumFrags + GLASSCFG_NUM_RADIAL_CRACKS < GLASSCFG_MAX_NUM_FRAGMENTS)
			{
				// Split the fragment into it's new sub-fragments
				GenerateSubFragments(impactFrag, impact);

				// Need to safely dispose of this fragment
				RemoveFragment(impactFrag);
			}
			else
			{
				// Simply knock out this fragment
				m_fragsLoose |= fragBit;
				RemoveFragmentConnections(impactFrag, m_frags[impactFrag].m_outConnections);
				RemoveFragmentConnections(impactFrag, m_frags[impactFrag].m_inConnections);
			}
		}
	}

	// Report success
	return foundFrag;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: FindImpactFragment
// Desc: Tests the impact point against hashed fragment triangles
//--------------------------------------------------------------------------------------------------
bool CREBreakableGlass::FindImpactFragment(const SGlassImpactParams& impact, uint8& fragIndex)
{
	GLASS_FUNC_PROFILER

	bool foundFrag = false;
	const Vec2 impactPt = Vec2(impact.x, impact.y);

#if GLASSCFG_USE_HASH_GRID
	if (m_pHashGrid)
	{
		// Hash impact to a grid cell of fragment triangles
		const uint32 cellIndex = m_pHashGrid->HashPosition(impact.x, impact.y);
		const TGlassHashBucket* const impactFrags = m_pHashGrid->GetBucket(cellIndex);

		// Test impact point against fragment polygons
		if (impactFrags && !impactFrags->empty())
		{
			const uint numImpactFrags = impactFrags->size();
			const uint8* pImpactFrags = impactFrags->begin();

			for (uint i = 0; i < numImpactFrags; ++i)
			{
				// Get hashed data
				const uint8 ownerFrag = pImpactFrags[i];
				CRY_ASSERT_MESSAGE(ownerFrag < GLASSCFG_FRAGMENT_ARRAY_SIZE, "Invalid frag index");

				// Check if this is the impact fragment
				const PodArray<Vec2>& fragOutline = m_frags[ownerFrag].m_outlinePts;

				if (PointInPolygon2D(impactPt, fragOutline.begin(), fragOutline.Count()))
				{
					fragIndex = ownerFrag;
					foundFrag = true;
					break;
				}
			}
		}
	}
#else
	// Perform a brute force loop through all active fragments
	const uint32 validState = m_fragsActive & ~(m_fragsLoose | m_fragsFree);
	const SGlassFragment* pFrags = m_frags.begin();

	uint32 fragBit = 1;
	for (uint i = 0; i < GLASSCFG_FRAGMENT_ARRAY_SIZE; ++i, fragBit <<= 1)
	{
		if (validState & fragBit)
		{
			// Check if this is the impact fragment
			const PodArray<Vec2>& fragOutline = m_frags[i].m_outlinePts;

			if (PointInPolygon2D(impactPt, fragOutline.begin(), fragOutline.Count()))
			{
				fragIndex = i;
				foundFrag = true;
				break;
			}
		}
	}
#endif // GLASSCFG_USE_HASH_GRID

	// If we didn't find a fragment, the impact passed through a hole
	return foundFrag;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: AddFragment
// Desc: Claims and re-initialises a fragment container
//--------------------------------------------------------------------------------------------------
SGlassFragment* CREBreakableGlass::AddFragment()
{
	CRY_ASSERT_MESSAGE(!m_freeFragIndices.empty(), "Tried to add glass fragment, but none free.");
	SGlassFragment* pFrag = NULL;

	if (!m_freeFragIndices.empty())
	{
		// Claim next free fragment
		m_lastFragIndex = m_freeFragIndices.back();
		m_freeFragIndices.pop_back();

		// Re-initialise it
		m_lastFragBit = (uint32)1 << m_lastFragIndex;

		m_frags[m_lastFragIndex].Reset();
		++m_totalNumFrags;

		m_fragsActive |= m_lastFragBit;
		m_fragsLoose &= ~m_lastFragBit;
		m_fragsFree &= ~m_lastFragBit;

		pFrag = &m_frags[m_lastFragIndex];
	}
	//  else
	//  {
	//    // Want to log this, but shotguns spam the log
	//    //LOG_GLASS_ERROR("Tried to add fragment, but none free.");
	//  }

	return pFrag;
}

//--------------------------------------------------------------------------------------------------
// Name: RemoveFragment
// Desc: Removes a fragment from the hash grid, and clears all data
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::RemoveFragment(const uint8 fragIndex)
{
	GLASS_FUNC_PROFILER

	const uint32 fragBit = (uint32)1 << fragIndex;
	const uint32 validState = m_fragsActive & ~m_fragsFree;

	if (fragIndex < GLASSCFG_FRAGMENT_ARRAY_SIZE)
	{
		if (validState & fragBit)
		{
			SGlassFragment& frag = m_frags[fragIndex];

			// Remove inter-fragment connections
			RemoveFragmentConnections(fragIndex, frag.m_outConnections);
			RemoveFragmentConnections(fragIndex, frag.m_inConnections);

			// Remove all hash grid references to this fragment
			const bool remove = true;
			HashFragment(fragIndex, remove);

			// Free all fragment data as now unused
			frag.m_outConnections.Free();
			frag.m_inConnections.Free();

			frag.m_outlinePts.Free();
			frag.m_triInds.Free();

			// Flag fragment as inactive
			DeactivateFragment(fragIndex, fragBit);
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: RebuildChangedFragment
// Desc: Re-connects, triangulates and hashes a changed fragment
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::RebuildChangedFragment(const uint8 fragIndex)
{
	SGlassFragment& frag = m_frags[fragIndex];
	const uint32 fragBit = (uint32)1 << fragIndex;

	// Rebuild if valid data available
	if (frag.m_outlinePts.Count() >= 3)
	{
		// Re-connect fragment
		const int fragCount = 1;
		ReplaceFragmentConnections(fragIndex, &fragIndex, fragCount);

		// Re-triangulate fragment
		frag.m_triInds.Clear();
		TriangulatePolygon2D(frag.m_outlinePts.begin(), frag.m_outlinePts.Count(), NULL, &frag.m_triInds);

		// Hash new fragment triangles
		HashFragment(fragIndex);
	}
	// Just discard
	else
	{
		m_fragsLoose |= fragBit;
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GenerateSubFragments
// Desc: Generates stable piece triangles within a specified polygon
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::GenerateSubFragments(const uint8 parentFragIndex, const SGlassImpactParams& impact)
{
	GLASS_FUNC_PROFILER

	// Begin by generating internal crack data (clipped within parent)
	TRadialCrackArray fragIntersectPts;
	GenerateSubFragmentCracks(parentFragIndex, impact, fragIntersectPts);

	// Find how many trees were made
	int numUsedCrackTrees = fragIntersectPts.size();
	CRY_ASSERT_MESSAGE(numUsedCrackTrees > 0, "Should always generate something, as it's forced");

	// When we hit our limit, all new fragments are loose
	// - Leads to fragments still being split when knocked out
	const bool hitStableLimit = (m_totalNumFrags + numUsedCrackTrees >= GLASSCFG_MAX_NUM_STABLE_FRAGMENTS);

	// Impact split variables
	const float impactSplitMinRadius = s_pCVars ? s_pCVars->m_impactSplitMinRadius : 0.05f;
	const float impactSplitRandMin = s_pCVars ? s_pCVars->m_impactSplitRandMin : 0.5f;
	const float impactSplitRandMax = s_pCVars ? s_pCVars->m_impactSplitRandMax : 1.5f;

	// Walk trees and parent to generate sub-fragment outlines
	TPolygonArray subFragShape;
	TPolygonArray splitFragShape;
	TSubFragArray stableSubFrags;

	for (int i = 0; i < numUsedCrackTrees; ++i)
	{
		// Resizing the vector, so need to re-grab parent fragment pointer
		SGlassFragment& parentFrag = m_frags[parentFragIndex];
		int parentOutlineSize = parentFrag.m_outlinePts.Count();

		// Calculate number of parent outline sections to add
		int nextTreeIndex = i + 1 < numUsedCrackTrees ? i + 1 : 0;
		int outlineCount = fragIntersectPts[nextTreeIndex] - fragIntersectPts[i];

		if (fragIntersectPts[nextTreeIndex] < fragIntersectPts[i])
		{
			outlineCount += parentOutlineSize;
		}

		// Pre-size the container
		const int firstTreeSize = m_radialCrackTrees[i].Count();
		const int secondTreeSize = m_radialCrackTrees[nextTreeIndex].Count() - 1;

		const int fullFragSize = firstTreeSize + outlineCount + secondTreeSize;

		if (fullFragSize >= 3 && fullFragSize <= POLY_ARRAY_SIZE)
		{
			subFragShape.resize(fullFragSize);

			// Add entire first tree
			Vec2* pSubFragPts = subFragShape.begin();

			for (int j = 0; j < firstTreeSize; ++j)
			{
				memcpy(&pSubFragPts[j], m_radialCrackTrees[i][j], sizeof(Vec2));
			}

			// Add parent outline between tree intersections
			if (outlineCount > 0)
			{
				for (int j = 0, k = fragIntersectPts[i] + 1; j < outlineCount; ++j, ++k)
				{
					// Cyclic outlines, so loop index when necessary
					if (k >= parentOutlineSize)
					{
						k -= parentOutlineSize;
					}

					memcpy(&pSubFragPts[firstTreeSize + j], &parentFrag.m_outlinePts[k], sizeof(Vec2));
				}
			}

			// Add opposite tree, excluding center impact pt
			const int secondTreeOffset = firstTreeSize + outlineCount;

			for (int j = 0, k = secondTreeSize; j < secondTreeSize; ++j, --k)
			{
				memcpy(&pSubFragPts[secondTreeOffset + j], m_radialCrackTrees[nextTreeIndex][k], sizeof(Vec2));
			}

			// Create new sub-fragment if there's enough valid data
			if (subFragShape.size() >= 3)
			{
				// In case of high-impulse collision, radially split the new fragment
				if ((impact.radius > 0.02f || impact.impulse > GLASSCFG_PLANE_SPLIT_IMPULSE) && numUsedCrackTrees > 2)
				{
					const float impactRadius = max(GetImpactRadius(impact), impactSplitMinRadius);
					const float splitVariation = clamp_tpl<float>(impactRadius, impactSplitRandMin, impactSplitRandMax);
					const Vec2 splitPt(impact.x, impact.y);
					float splitRadius = impactRadius * (1.0f + GetRandF() * splitVariation);

					SplitPolygonAroundPoint(subFragShape, splitPt, splitRadius, splitFragShape);

					const bool forceLoose = true;
					CreateFragmentFromOutline(splitFragShape.begin(), splitFragShape.size(), parentFragIndex, forceLoose);
				}

				// Create main sub-fragment (if still valid)
				if (CreateFragmentFromOutline(subFragShape.begin(), subFragShape.size(), parentFragIndex, hitStableLimit))
				{
					// Store fragment index if stable
					if (!(m_fragsLoose & m_lastFragBit))
					{
						stableSubFrags.push_back(m_lastFragIndex);
					}
				}
			}
		}
		else
		{
			LOG_GLASS_ERROR("Intersections form invalid sized fragment, had to discard.");
		}

		// Done with this sub-fragment
		subFragShape.clear();
		splitFragShape.clear();
	}

	// Need to connect new sub-fragments to each other,
	// and then to parent's old neighbours
	const uint8* pStableSubFrags = stableSubFrags.begin();
	const int numStableSubFrags = stableSubFrags.size();

	if (numStableSubFrags > 0)
	{
		ConnectSubFragments(pStableSubFrags, numStableSubFrags);
		ReplaceFragmentConnections(parentFragIndex, pStableSubFrags, numStableSubFrags);
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GenerateSubFragmentCracks
// Desc: Generates radial crack triangles within a specified fragment
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::GenerateSubFragmentCracks(const uint8 parentFragIndex, const SGlassImpactParams& impact, TRadialCrackArray& fragIntersectPts)
{
	GLASS_FUNC_PROFILER

	// Get parent fragment data
	SGlassFragment& parentFrag = m_frags[parentFragIndex];
	const PodArray<Vec2>& fragOutline = parentFrag.m_outlinePts;

	const Vec2* pFragPts = fragOutline.begin();
	const int numFragPts = fragOutline.Count();

	// Generate cracks from current impact pt
	fragIntersectPts.clear();
	const int numRadialCracks = GenerateCracksFromImpact(parentFragIndex, impact);

	const float planeMin = 0.0f;
	const float planeMaxX = m_glassParams.size.x;
	const float planeMaxY = m_glassParams.size.y;

	// Clip crack trees to fragment bounds
	for (int i = 0; i < numRadialCracks; ++i)
	{
		if (m_radialCrackTrees[i].IsEmpty())
		{
			break;
		}

		const int treeSize = m_radialCrackTrees[i].Count();
		Vec2** pRadialPts = &m_radialCrackTrees[i][0];

		for (int j = 0; j < treeSize - 1; ++j)
		{
			const Vec2& nextPt = (*pRadialPts[j + 1]);

			bool ptOnBounds = nextPt.x == planeMin
			                  || nextPt.x == planeMaxX
			                  || nextPt.y == planeMin
			                  || nextPt.y == planeMaxY;

			// Test to see if tree has left fragment
			if (!ptOnBounds && PointInPolygon2D(nextPt, pFragPts, numFragPts))
			{
				continue;
			}

			// Create last tree line
			Lineseg crackLine;
			crackLine.start = (*pRadialPts[j]);
			crackLine.end = nextPt;

			// Find intersection point with fragment outline
			Lineseg outline;
			float outA = 0.0f, outB = 0.0f;
			bool found = false;

			for (int k = 0; k < numFragPts; ++k)
			{
				const int nextIndex = k + 1 < numFragPts ? k + 1 : 0;

				outline.start = fragOutline[k];
				outline.end = fragOutline[nextIndex];

				if (Intersect::Lineseg_Lineseg2D(crackLine, outline, outA, outB))
				{
					found = true;
					fragIntersectPts.push_back(k);
					break;
				}
			}

			CRY_ASSERT_MESSAGE(found, "Crack line leaves fragment but did not find intersection point.");

			// Move end point to intersection
			pRadialPts[j + 1]->x = crackLine.start.x * (1.0f - outA) + crackLine.end.x * outA;
			pRadialPts[j + 1]->y = crackLine.start.y * (1.0f - outA) + crackLine.end.y * outA;

			// Discard remaining sections
			const int numLeft = j + 2;
			m_radialCrackTrees[i].resize(numLeft);

			// Break because the const loop reference value has changed
			break;
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: CreateFragmentFromOutline
// Desc: Creates and hashes a new sub fragment with the input data
//--------------------------------------------------------------------------------------------------
bool CREBreakableGlass::CreateFragmentFromOutline(const Vec2* pOutline, const int outlineSize, const uint8 parentFragIndex, const bool forceLoose)
{
	GLASS_FUNC_PROFILER
	bool success = false;

	if (pOutline && outlineSize >= 3)
	{
		// Check against fixed working array size
		if (outlineSize <= POLY_ARRAY_SIZE)
		{
			// Create new fragment
			SGlassFragment* pSubFrag = AddFragment();

			if (pSubFrag)
			{
				const uint8 fragIndex = m_lastFragIndex;
				const uint32 fragBitIndex = m_lastFragBit;

				// Store the outline points
				pSubFrag->m_outlinePts.resize(outlineSize);
				memcpy(pSubFrag->m_outlinePts.begin(), pOutline, sizeof(Vec2) * outlineSize);

				Vec2* pPts = pSubFrag->m_outlinePts.begin();
				const int numPts = pSubFrag->m_outlinePts.Count();

				// Calculate modified fragment area - Smaller fragments give a simpler sim overall
				pSubFrag->m_area = CalculatePolygonArea2D(pPts, numPts);
				pSubFrag->m_area *= (1.0f / GLASSCFG_SIMPLIFY_AREA);

				// Minimum size for actual creation
				if (pSubFrag->m_area > GLASSCFG_MIN_VALID_FRAG_AREA)
				{
					// Triangulate fragment outline
					TriangulatePolygon2D(pPts, numPts, NULL, &pSubFrag->m_triInds);

					// Calculate fragment center position
					pSubFrag->m_center = CalculatePolygonCenter2D(pPts, numPts);

					// Determine if fragment is loose
					const uint8 numFrags = (uint8)m_totalNumFrags;
					pSubFrag->m_depth = parentFragIndex < numFrags ? m_frags[parentFragIndex].m_depth + 1 : 0;

					if (forceLoose || IsFragmentLoose(*pSubFrag))
					{
						m_fragsLoose |= fragBitIndex;
					}
					else
					{
						// Hash the new triangles if stable
						HashFragment(fragIndex);
					}

					// Done with fragment
					success = true;
				}
				else
				{
					RemoveFragment(fragIndex);
				}
			}
		}
		else
		{
			LOG_GLASS_ERROR("Attempted to create invalid sized fragment.");
		}
	}

	//  if (!success)
	//  {
	//    // It could be worth throwing a particle effect here
	//    // to cover the pop when the new fragment disappears
	//  }

	// Invalid data, so failed to create
	return success;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: IsFragmentLoose
// Desc: Determines if a new fragment is loose
//--------------------------------------------------------------------------------------------------
bool CREBreakableGlass::IsFragmentLoose(const SGlassFragment& frag)
{
	const int MinLooseFragDepth = 3;
	const int LooseFragDepthRange = 3; // Range is 3 -> 6
	const float EdgeStableFragSizeMul = 1.0f;

	bool isLoose = false;
	const int fragDepth = frag.m_depth;

	// Test depth test against randomised range
	if (fragDepth >= MinLooseFragDepth)
	{
		const int looseFragDepth = cry_random(0, LooseFragDepthRange - 1) + MinLooseFragDepth;
		isLoose = (fragDepth >= looseFragDepth);
	}

	// Test area against fixed value, weighted to allow more stable pieces
	// nearer to edges. Means we get lots of nice little shards sticking around
	if (!isLoose)
	{
		const float planeMaxX = m_glassParams.size.x;
		const float planeMaxY = m_glassParams.size.y;

		const float xDistFromEdge = planeMaxX - fabs_tpl(frag.m_center.x - (planeMaxX * 0.5f)) * 2.0f;
		const float yDistFromEdge = planeMaxY - fabs_tpl(frag.m_center.y - (planeMaxY * 0.5f)) * 2.0f;
		const float minDistFromEdge = min(xDistFromEdge, yDistFromEdge);

		float edgeSizeMul = 1.0f - (minDistFromEdge * m_invMaxGlassSize);
		edgeSizeMul = 1.0f + EdgeStableFragSizeMul * edgeSizeMul * edgeSizeMul;

		isLoose = (frag.m_area < GLASSCFG_MIN_STABLE_FRAG_AREA * edgeSizeMul);
	}

	return isLoose;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: IsFragmentWeaklyLinked
// Desc: Determines if an existing fragment is "weakly linked"
//--------------------------------------------------------------------------------------------------
bool CREBreakableGlass::IsFragmentWeaklyLinked(const SGlassFragment& frag)
{
	// Weakly linked means large fragment with little inward support, but giving outward support
	return (frag.m_area > GLASSCFG_MIN_STABLE_FRAG_AREA * 2.0f) &&
	       (frag.m_inConnections.size() == 1) &&
	       (frag.m_outConnections.size() >= 1);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: RemoveFragmentConnections
// Desc: Removes all registered connections with the fragment
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::RemoveFragmentConnections(const uint8 fragIndex, PodArray<uint8>& connections)
{
	GLASS_FUNC_PROFILER

	while (!connections.IsEmpty())
	{
		// Remove last connection
		const uint8 neighbour = connections.back();
		connections.DeleteLast();

		// Delete from neighbour's outgoing connection list
		//	- Manually doing delete search code here as uint8
		//		template gives ambiguous function signatures
		PodArray<uint8>& neighbourOutConns = m_frags[neighbour].m_outConnections;
		const uint8* pNeighbourOutConns = neighbourOutConns.begin();
		const int numOutConns = neighbourOutConns.Count();

		for (int i = 0; i < numOutConns; ++i)
		{
			if (pNeighbourOutConns[i] == fragIndex)
			{
				const int numElems = 1;
				neighbourOutConns.Delete(i, numElems);

				break;
			}
		}

		// Delete from neighbour's incoming connection list
		PodArray<uint8>& neighbourInConns = m_frags[neighbour].m_inConnections;
		const uint8* pNeighbourInConns = neighbourInConns.begin();
		const int numInConns = neighbourInConns.Count();

		for (int i = 0; i < numInConns; ++i)
		{
			if (pNeighbourInConns[i] == fragIndex)
			{
				const int numElems = 1;
				neighbourInConns.Delete(i, numElems);

				break;
			}
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ConnectSubFragments
// Desc: Forms connections for a circular list of new sub-fragments
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::ConnectSubFragments(const uint8* pSubFrags, const uint8 numSubFrags)
{
	if (pSubFrags && numSubFrags > 1)
	{
		// All solid fragments just created may be connected, always in a circular fashion
		for (int i = 0, j = 1; i < numSubFrags; ++i, ++j)
		{
			j = (j == numSubFrags) ? 0 : j;
			ConnectFragmentPair(pSubFrags[i], pSubFrags[j]);
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ReplaceFragmentConnections
// Desc: When a fragment is sub-divided, tells neighbours to reconnect to the child fragments
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::ReplaceFragmentConnections(const uint8 parentFragIndex, const uint8* pSubFrags, const uint8 numSubFrags)
{
	// Check for potential new connections between parent neighbours and new sub-fragments
	if (pSubFrags && numSubFrags > 0)
	{
		PodArray<uint8>& neighbours = m_frags[parentFragIndex].m_inConnections;
		const uint8* pNeighbours = neighbours.begin();
		const int numNeighbours = neighbours.Count();

		for (int i = 0; i < numNeighbours; ++i)
		{
			for (int j = 0; j < numSubFrags; ++j)
			{
				ConnectFragmentPair(pNeighbours[i], pSubFrags[j]);
			}
		}
	}

	// Remove all old connections
	RemoveFragmentConnections(parentFragIndex, m_frags[parentFragIndex].m_outConnections);
	RemoveFragmentConnections(parentFragIndex, m_frags[parentFragIndex].m_inConnections);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ConnectFragmentPair
// Desc: Forms directional connections between a pair of fragments if strong enough
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::ConnectFragmentPair(const uint8 fragAIndex, const uint8 fragBIndex)
{
	GLASS_FUNC_PROFILER

	const float minFragConnection = 0.2f;

	// Validate fragment state
	if (fragAIndex != fragBIndex)
	{
		SGlassFragment& fragA = m_frags[fragAIndex];
		SGlassFragment& fragB = m_frags[fragBIndex];

		float connectionA, connectionB;
		if (CalculatePolygonSharedPerimeter2D(fragA.m_outlinePts.begin(), fragA.m_outlinePts.Count(),
		                                      fragB.m_outlinePts.begin(), fragB.m_outlinePts.Count(), connectionA, connectionB))
		{
			/*
			   --------			A is ~10% connected
			 |      |__		B is ~30% connected
			 |  A   |B |
			 |      |--		In this case, only one connection formed from A to B. This
			   --------			is because we can then traverse from A to B, but not other
			   way around. Can be thought of as "A is supporting B".
			 */

			// Connect the fragments where stable connection, to control
			// which fragments are strong enough to support others
			if (connectionB > minFragConnection)
			{
				fragA.m_outConnections.push_back(fragBIndex);
				fragB.m_inConnections.push_back(fragAIndex);
			}

			if (connectionA > minFragConnection)
			{
				fragB.m_outConnections.push_back(fragAIndex);
				fragA.m_inConnections.push_back(fragBIndex);
			}
		}
	}
	else
	{
		LOG_GLASS_ERROR("Tried to connect invalid fragments, ignoring.");
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: FindLooseFragments
// Desc: Creates a tree of stable fragments, flagging any loose ones not caught in the traversal
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::FindLooseFragments(TGlassPhysFragmentArray* pPhysFrags, TGlassPhysFragmentInitArray* pPhysFragsInitData)
{
	GLASS_FUNC_PROFILER

	TFragIndexPodArray stableFrags;
	bool stableFragsFound = false;

	// Pre-evaluate fragments bit states
	const uint32 stableState = m_fragsActive & ~(m_fragsLoose | m_fragsFree);

	// Find all anchored fragments
	if (!m_shattered)
	{
		const uint numAnchors = m_glassParams.numAnchors;
		const Vec2* pAnchors = &m_glassParams.anchors[0];
		TFragIndexPodArray anchorFrags;

		for (uint i = 0; i < numAnchors; ++i)
		{
			SGlassImpactParams anchorParams;
			anchorParams.x = pAnchors[i].x;
			anchorParams.y = pAnchors[i].y;

			uint8 anchorFrag = 0;
			if (FindImpactFragment(anchorParams, anchorFrag))
			{
				// Only store result if found an active fragment
				const uint32 anchorFragBit = (uint32)1 << anchorFrag;

				if (stableState & anchorFragBit)
				{
					anchorFrags.push_back(anchorFrag);
				}
			}
		}

		// Traverse all valid connections, and log all stable fragments
		const uint numAnchorFrags = anchorFrags.size();
		stableFragsFound = (numAnchorFrags > 0);

		for (uint i = 0; i < numAnchorFrags; ++i)
		{
			TraverseStableConnection(anchorFrags[i], stableFrags);
		}
	}

	// Mark all active fragments not in list as loose
	const uint32 activeState = m_fragsActive & ~m_fragsFree;
	uint32 fragBit = 1;

	for (uint i = 0; i < GLASSCFG_FRAGMENT_ARRAY_SIZE; ++i, fragBit <<= 1)
	{
		if (activeState & fragBit)
		{
			bool dampenVelocity = false;
			bool noVelocity = false;

			// Unconnected fragments should just fall
			if (!(m_fragsLoose & fragBit) && (!stableFragsFound || stableFrags.find(i) < 0))
			{
				m_fragsLoose |= fragBit;
				dampenVelocity = true;
			}
			// Periodically loosen weakly-connected fragments
			else if (s_loosenTimer > 1.0f + GetRandF() && s_impactTimer > 1.0f && s_impactTimer < 2.0f)
			{
				const SGlassFragment& frag = m_frags[i];
				if (IsFragmentWeaklyLinked(frag))
				{
					m_fragsLoose |= fragBit;
					noVelocity = true;
					s_loosenTimer = 0.0f - GetRandF();
				}
			}

			// Resolve any loose fragments
			if (m_fragsLoose & fragBit)
			{
				PrePhysicalizeLooseFragment(pPhysFrags, pPhysFragsInitData, i, dampenVelocity, noVelocity);
			}
		}
	}

	// Reset loosen timer
	s_loosenTimer = 0.0f;

	// No stable fragments means we should shatter
	if (!stableFragsFound)
	{
		ShatterGlass();
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: TraverseStableConnection
// Desc: Recursively adds this fragment and all connected fragments to the input list
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::TraverseStableConnection(const uint8 fragIndex, TFragIndexPodArray& stableFrags)
{
	GLASS_FUNC_PROFILER

	// NOTE: This method should actually take into account a ratio of fragment
	// area and connected edge, as we can currently have a small piece holding
	// together two large pieces. If mass was taken into account, we could
	// instead have the hanging large fragment pull out the smaller one

	// Getting here means this is a stable fragment. Can be a cyclic
	// graph though, so only continue if not already visited
	if (stableFrags.find(fragIndex) < 0)
	{
		stableFrags.push_back(fragIndex);

		// Traverse through all valid out-going connections
		PodArray<uint8>& connections = m_frags[fragIndex].m_outConnections;
		uint8* pConnections = connections.begin();
		const int numConnections = connections.Count();

		for (int i = 0; i < numConnections; ++i)
		{
			TraverseStableConnection(pConnections[i], stableFrags);
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: PrePhysicalizeLooseFragment
// Desc: Creates data for a fragment to become physicalized and removes it from the plane
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::PrePhysicalizeLooseFragment(TGlassPhysFragmentArray* pPhysFrags, TGlassPhysFragmentInitArray* pPhysFragsInitData, const uint8 fragIndex, const bool dampenVel, const bool noVel)
{
	GLASS_FUNC_PROFILER

	  assert(pPhysFragsInitData);
	PREFAST_ASSUME(pPhysFragsInitData);

	SGlassFragment& frag = m_frags[fragIndex];
	const uint32 fragBit = (uint32)1 << fragIndex;

	Vec2* pPts = frag.m_outlinePts.begin();
	const uint numPts = frag.m_outlinePts.size();

	// Remove inter-fragment connections
	RemoveFragmentConnections(fragIndex, frag.m_outConnections);
	RemoveFragmentConnections(fragIndex, frag.m_inConnections);

	frag.m_outConnections.Free();
	frag.m_inConnections.Free();

	// Remove all hash grid references to this fragment (if it was ever stable)
	if (frag.m_area >= GLASSCFG_MIN_STABLE_FRAG_AREA)
	{
		const bool remove = true;
		HashFragment(fragIndex, remove);
	}

	// Only physicalize if fragment is large enough
	const float physSizeScale = 0.7f;
	const float minPhysFragSize = 0.065f;

	const Vec2 bounds = CalculatePolygonBounds2D(pPts, numPts);
	const float physFragSize = max(bounds.x, bounds.y) * physSizeScale;

	if (physFragSize >= minPhysFragSize)
	{
		// Can run out of space, in which case just shatter to particles
		const bool outOfSpace = (!pPhysFrags || pPhysFrags->size() == GLASSCFG_MAX_NUM_PHYS_FRAGMENTS);

#ifndef RELEASE
		// Arrays should always be in sync
		const int numPhysFrags = pPhysFrags ? pPhysFrags->size() : 0;
		const int numPhysFragsInitData = pPhysFragsInitData ? pPhysFragsInitData->size() : 0;
		CRY_ASSERT(numPhysFrags == numPhysFragsInitData);
#endif

		if (outOfSpace)
		{
			PlayShatterEffect(fragIndex);
		}
		else
		{
			SGlassPhysFragment physFrag;
			SGlassPhysFragmentInitData physFragsInitData;

			// Prepare to be physicalized
			physFrag.m_fragIndex = fragIndex;
			physFrag.m_bufferIndex = m_lastPhysFrag;
			physFrag.m_matrix = m_params.matrix;
			physFrag.m_size = physFragSize;

			physFragsInitData.m_center = frag.m_center;

			// Re-center fragment mesh at origin
			for (uint i = 0; i < numPts; ++i)
			{
				pPts[i] -= physFragsInitData.m_center;
			}

			// Get cvar constants
			float fragImpulseScale = 5.0f;
			float fragAngImpulseScale = 0.1f;
			float fragImpulseDampen = 0.3f;
			float fragAngImpulseDampen = 0.4f;
			float fragSpread = 1.5f;
			float fragMaxSpeed = 4.0f;
			float fragRandSpread = 0.5f;

			if (s_pCVars)
			{
				fragImpulseScale = s_pCVars->m_fragImpulseScale;
				fragAngImpulseScale = s_pCVars->m_fragAngImpulseScale;
				fragImpulseDampen = s_pCVars->m_fragImpulseDampen;
				fragAngImpulseDampen = s_pCVars->m_fragAngImpulseDampen;
				fragSpread = s_pCVars->m_fragSpread;
				fragMaxSpeed = s_pCVars->m_fragMaxSpeed;
			}

			// Initial impulse based on generation impact
			float angularImpulseScale = fragAngImpulseScale * physFrag.m_size;

			if (dampenVel)
			{
				fragMaxSpeed *= fragImpulseDampen;
				angularImpulseScale *= fragAngImpulseDampen;
			}

			if (noVel)
			{
				fragMaxSpeed = 0.0f;
				angularImpulseScale *= 0.5f;
			}

			if (!m_impactParams.empty())
			{
				// Impact driven physicalization
				SGlassImpactParams& impact = m_impactParams.back();
				const Vec2 impactPt = Vec2(impact.x, impact.y);
				const Vec2 toImpactPt = (impactPt - physFragsInitData.m_center).GetNormalized();

				// Max speed clamp
				Vec3 velocity = impact.velocity;
				float speedSq = impact.velocity.GetLengthSquared();

				if (speedSq > fragMaxSpeed * fragMaxSpeed)
				{
					velocity = velocity.GetNormalizedFast() * fragMaxSpeed;
				}

				// Adjust velocity to apply artificial spread
				float randSpread = (1.0f - fragRandSpread) + fragRandSpread * GetRandF();
				Vec3 worldFromImpact = physFrag.m_matrix.TransformVector(-toImpactPt);
				velocity += worldFromImpact * GetImpactRadius(impact) * fragSpread * randSpread;

				// Calculate impulse (w/ point between center and edge)
				physFragsInitData.m_impulse = velocity * fragImpulseScale;
				physFragsInitData.m_impulsePt = physFragsInitData.m_center + toImpactPt * angularImpulseScale;
			}
			else
			{
				// Simple piece drop
				physFragsInitData.m_impulse = Vec3Constants<float>::fVec3_Zero;
				physFragsInitData.m_impulsePt = physFragsInitData.m_center;
			}

			physFragsInitData.m_impulsePt = physFrag.m_matrix.TransformPoint(physFragsInitData.m_impulsePt);

			// Scale impulse to match fragment size
			const float impulseSizeScale = 1.0f + physFrag.m_size;
			physFragsInitData.m_impulse *= impulseSizeScale * impulseSizeScale;

			// Push into lists
			pPhysFrags->push_back(physFrag);
			pPhysFragsInitData->push_back(physFragsInitData);

			// Reset fragment geometry buffer id
			m_fragGeomBufferIds[m_lastPhysFrag].m_geomBufferId = NO_BUFFER;
			m_fragGeomBufferIds[m_lastPhysFrag].m_fragId = fragIndex;
			m_lastPhysFrag = m_lastPhysFrag + 1 < GLASSCFG_MAX_NUM_PHYS_FRAGMENTS ? m_lastPhysFrag + 1 : 0;
		}
	}

	// Flag fragment as inactive
	DeactivateFragment(fragIndex, fragBit);
	++m_numLooseFrags;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetHashGridSize
// Desc: Re-calculates the hash grid sizes
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::SetHashGridSize(const Vec2& size)
{
#if GLASSCFG_USE_HASH_GRID
	// Resize hash grid to fit new dimensions
	if (m_pHashGrid)
	{
		m_pHashGrid->Resize(size.x, size.y);
		m_pHashGrid->ClearGrid();
	}
#endif

	// Pre-calculate a few constants
	m_hashCellSize = size * (1.0f / (float)GLASSCFG_HASH_GRID_SIZE);
	m_hashCellInvSize.set(1.0f / m_hashCellSize.x, 1.0f / m_hashCellSize.y);
	m_hashCellHalfSize = m_hashCellSize * 0.5f;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: HashFragment
// Desc: Hashes a fragment into the grid
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::HashFragment(const uint8 fragIndex, const bool remove)
{
#if GLASSCFG_USE_HASH_GRID
	// Get fragment
	const SGlassFragment& frag = m_frags[fragIndex];

	// Hash polygon
	const int numTriIndices = frag.m_triInds.Count();
	const uint8* pIndices = frag.m_triInds.begin();

	for (int j = 0; j < numTriIndices; j += 3)
	{
		const uint index0 = pIndices[j];
		const uint index1 = pIndices[j + 1];
		const uint index2 = pIndices[j + 2];

		HashFragmentTriangle(frag.m_outlinePts[index0], frag.m_outlinePts[index1], frag.m_outlinePts[index2], fragIndex, remove);
	}
#endif
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: STriEdge/STriSpan
// Desc: Triangle hashing helper classes
//--------------------------------------------------------------------------------------------------
struct STriEdge
{
	STriEdge(const Vec2& a, const Vec2& b)
	{
		// Lowest point first
		const float abMin = b.y - a.y;
		const float abMax = a.y - b.y;
		pt[0].x = (float)__fsel(abMin, a.x, b.x);
		pt[0].y = (float)__fsel(abMin, a.y, b.y);
		pt[1].x = (float)__fsel(abMax, a.x, b.x);
		pt[1].y = (float)__fsel(abMax, a.y, b.y);

		// Edge height
		height = pt[1].y - pt[0].y;
	}

	Vec2  pt[2];
	float height;
};

struct STriSpan
{
	STriSpan(const float x1, const float x2)
	{
		// Left-most point first
		pt[0] = min(x1, x2);
		pt[1] = max(x1, x2);

		// Span length (with direction)
		width = x2 - x1;
	}

	float pt[2];
	float width;
};

//--------------------------------------------------------------------------------------------------
// Name: HashFragmentTriangle
// Desc: Hashes a fragment triangle into the grid
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::HashFragmentTriangle(const Vec2& a, const Vec2& b, const Vec2& c,
                                             const uint8& triFrag, const bool removeTri)
{
#if GLASSCFG_USE_HASH_GRID
	GLASS_FUNC_PROFILER

	if (m_pHashGrid)
	{
		// Create triangle edges
		STriEdge edges[3] =
		{
			STriEdge(a, b),
			STriEdge(a, c),
			STriEdge(b, c)
		};

		// Find tallest edge
		uint tallEdge = edges[0].height > edges[1].height ? 0 : 1;
		tallEdge = edges[2].height > edges[tallEdge].height ? 2 : tallEdge;

		uint shortEdge1 = inc_mod3[tallEdge];
		uint shortEdge2 = dec_mod3[tallEdge];

		// Need to essentially rasterise triangle to hash all contained cells into grid
		const float epsilon = 0.05f;

		if (fabs_tpl(edges[shortEdge1].height) >= m_hashCellSize.y * epsilon)
		{
			HashFragmentTriangleSpan(edges[tallEdge], edges[shortEdge1], triFrag, removeTri);
		}

		if (fabs_tpl(edges[shortEdge2].height) >= m_hashCellSize.y * epsilon)
		{
			HashFragmentTriangleSpan(edges[tallEdge], edges[shortEdge2], triFrag, removeTri);
		}
	}
#endif // GLASSCFG_USE_HASH_GRID
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: HashFragmentTriangleSpan
// Desc: Hashes common spans of triangle edges into the grid
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::HashFragmentTriangleSpan(const STriEdge& a, const STriEdge& b,
                                                 const uint8& triFrag, const bool removeTri)
{
#if GLASSCFG_USE_HASH_GRID
	GLASS_FUNC_PROFILER

	// Hash spans along height of smaller edge
	STriSpan spanA(a.pt[0].x, a.pt[1].x);
	STriSpan spanB(b.pt[0].x, b.pt[1].x);

	float spanALerp = (b.pt[0].y - a.pt[0].y) / a.height;
	float spanBLerp = 0.0f;

	float spanAStep = m_hashCellSize.y / a.height;
	float spanBStep = m_hashCellSize.y / b.height;

	uint numYSteps = static_cast<int>(fabs_tpl(b.height - m_hashCellHalfSize.y) * m_hashCellInvSize.y) + 1;
	float yOffset = b.pt[0].y;

	for (uint y = 0; y <= numYSteps; ++y, yOffset += m_hashCellSize.y)
	{
		// Generate span info
		STriSpan span(a.pt[0].x + spanA.width * spanALerp, b.pt[0].x + spanB.width * spanBLerp);
		uint numXSteps = static_cast<uint>(fabs_tpl(span.width - m_hashCellHalfSize.x) * m_hashCellInvSize.x) + 1;

		// Pre-hash first cell
		uint32 cellOffset = m_pHashGrid->HashPosition(span.pt[0], yOffset);

		// Check valid cell
		if (cellOffset < GLASSCFG_HASH_GRID_SIZE * GLASSCFG_HASH_GRID_SIZE)
		{
			// Simple addition step through span as we will touch
			// contiguous set of tiles when moving horizontally
			if (removeTri)
			{
				for (uint x = 0; x < numXSteps; ++x, ++cellOffset)
				{
					m_pHashGrid->RemoveElementFromGrid(cellOffset, triFrag);
				}
			}
			else
			{
				for (uint x = 0; x < numXSteps; ++x, ++cellOffset)
				{
					m_pHashGrid->AddElementToGrid(cellOffset, triFrag);
				}
			}
		}

		// Step along edges
		spanALerp += spanAStep;
		spanBLerp += spanBStep;
	}
#endif // GLASSCFG_USE_HASH_GRID
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: DeactivateFragment
// Desc: Deactivates and recycles a fragment index
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::DeactivateFragment(const uint8 index, const uint32 bit)
{
	// Mark as inactive
	m_fragsActive &= ~bit;

	// Recycle index
	m_fragsFree |= bit;
	m_freeFragIndices.push_back(index);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: RebuildAllFragmentGeom
// Desc: Creates simple plane geometry
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::RebuildAllFragmentGeom()
{
	GLASS_FUNC_PROFILER

	if (m_geomDirty && !m_geomRebuilt && m_dirtyGeomBufferCount == 0)
	{
		// Clear old data
		m_planeGeom.Clear();
		m_crackGeom.Clear();

		for (uint i = 0; i < GLASSCFG_MAX_NUM_PHYS_FRAGMENTS; ++i)
		{
			m_fragFullGeom[i].Clear();
		}

		// Pool fragments into single lists for vertex creation
		ProcessFragmentGeomData();

		// Update the impact count so the shader constants can update too
		m_numDecalImpacts = (uint8)m_impactParams.size();

		// Rebuilt, so ready for buffer copy/update
		MemoryBarrier();
		m_geomDirty = false;
		uint8 numDirtyBuffers = 1;

		// Toggle flag for all active loose fragments too
		for (uint i = 0; i < GLASSCFG_MAX_NUM_PHYS_FRAGMENTS; ++i)
		{
			if (m_fragGeomBufferIds[i].m_fragId < GLASSCFG_FRAGMENT_ARRAY_SIZE)
			{
				m_fragGeomRebuilt[i] = true;
				++numDirtyBuffers;
			}
		}

		m_dirtyGeomBufferCount = numDirtyBuffers;
		m_geomUpdateFrame = 0; // PC doesn't suffer same delay issues
		m_geomRebuilt = true;
		MemoryBarrier();
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: RehashAllFragmentData
// Desc: Clears the grid and re-hashes all fragments
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::RehashAllFragmentData()
{
#if GLASSCFG_USE_HASH_GRID
	GLASS_FUNC_PROFILER

	if (m_pHashGrid)
	{
		// Clear old data
		m_pHashGrid->ClearGrid();

		// Pre-evaluate fragment state bits
		const uint32 validState = m_fragsActive & ~(m_fragsLoose | m_fragsFree);
		uint32 fragBit = 1;

		// Hash all active fragments
		for (uint i = 0; i < GLASSCFG_FRAGMENT_ARRAY_SIZE; ++i, fragBit <<= 1)
		{
			if (validState & fragBit)
			{
				HashFragment(i);
			}
		}
	}
#endif // GLASSCFG_USE_HASH_GRID
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ProcessFragmentGeomData
// Desc: Generates the geometry data for all fragments
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::ProcessFragmentGeomData()
{
	GLASS_FUNC_PROFILER

	// Pre-evaluate fragment state bits
	const uint32 validState = m_fragsActive & ~(m_fragsLoose | m_fragsFree);
	uint32 fragBit = 1;

	// Pre-size geom buffers for active fragments
	const SGlassFragment* pFrags = m_frags.begin();

	const int crackVertStep = 2;
	const int crackIndStep = 6;

	uint vertCount = 0;
	uint indCount = 0;

	for (uint i = 0; i < GLASSCFG_FRAGMENT_ARRAY_SIZE; ++i, fragBit <<= 1)
	{
		if (validState & fragBit)
		{
			vertCount += pFrags[i].m_outlinePts.Count();
			indCount += pFrags[i].m_triInds.Count();
		}
	}

	m_planeGeom.m_verts.resize(vertCount);
	m_planeGeom.m_inds.resize(indCount);
	m_planeGeom.m_tans.resize(vertCount);

	m_crackGeom.m_verts.resize(vertCount * crackVertStep);
	m_crackGeom.m_inds.resize(indCount * crackIndStep);
	m_crackGeom.m_tans.resize(vertCount * crackVertStep);

	// Create geom in arrays
	uint planeVOffs = 0, planeIOffs = 0;
	uint crackVOffs = 0, crackIOffs = 0;
	fragBit = 1;

	for (uint i = 0; i < GLASSCFG_FRAGMENT_ARRAY_SIZE; ++i, fragBit <<= 1)
	{
		if (validState & fragBit)
		{
			BuildFragmentTriangleData(i, planeVOffs, planeIOffs);
			BuildFragmentOutlineData(i, crackVOffs, crackIOffs);
		}
	}

	// Create geom for all loose fragments
	for (uint i = 0; i < GLASSCFG_MAX_NUM_PHYS_FRAGMENTS; ++i)
	{
		const uint8 fragId = m_fragGeomBufferIds[i].m_fragId;

		// Validate fragment
		if (fragId < GLASSCFG_FRAGMENT_ARRAY_SIZE)
		{
			// Pre-size buffer to hold entire geom
			const uint numVerts = pFrags[fragId].m_outlinePts.Count();
			const uint numInds = pFrags[fragId].m_triInds.Count();

			vertCount = numVerts + numVerts * crackVertStep;
			indCount = numInds + numVerts * crackIndStep;

			m_fragFullGeom[i].m_verts.resize(vertCount);
			m_fragFullGeom[i].m_inds.resize(indCount);
			m_fragFullGeom[i].m_tans.resize(vertCount);

			// Reset offsets
			planeVOffs = planeIOffs = 0;

			// Build phys fragments
			BuildFragmentOutlineData(fragId, planeVOffs, planeIOffs, i);
			BuildFragmentTriangleData(fragId, planeVOffs, planeIOffs, EGlassSurfaceSide_Front, i);
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: BuildFragmentTriangleData
// Desc: Generates the stable triangle data for all fragments
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::BuildFragmentTriangleData(const uint8 fragIndex, uint& vertOffs, uint& indOffs, const EGlassSurfaceSide side, const int subFragID)
{
	GLASS_FUNC_PROFILER

	// Get fragment data
	const PodArray<Vec2>& vertices = m_frags[fragIndex].m_outlinePts;
	const PodArray<uint8>& indices = m_frags[fragIndex].m_triInds;

	const int vertCount = vertices.Count();
	const int indCount = indices.Count();

	// Get offset into output arrays
	const bool isPhysFrag = (subFragID >= 0);
	SGlassGeom& buffer = isPhysFrag ? m_fragFullGeom[subFragID] : m_planeGeom;

	const int vertOffset = vertOffs;
	const int indOffset = indOffs;

	// Increment external counters
	vertOffs += vertCount;
	indOffs += indCount;

	// Slightly non-flat surface tangent (Fixed per fragment for consistency)
	const float randTangentScale = 0.015f;
	const float randTangents[8] = { 0.0f, 0.7f, 0.5f, 0.1f, 1.0f, 0.6f, 0.2f, 0.8f };
	const uint8 randIndex = fragIndex & 7; // Optimized "index % 8"

	const Vec3 tanOffset(randTangents[randIndex] * randTangentScale);

	Vec3 tangent = (side == EGlassSurfaceSide_Back) ? Vec3(0.0f, 1.0f, 0.0f) : Vec3(0.0f, -1.0f, 0.0f);
	Vec3 bitangent = (side == EGlassSurfaceSide_Back) ? Vec3(-1.0f, 0.0f, 0.0f) : Vec3(1.0f, 0.0f, 0.0f);

	tangent = tangent + tanOffset;
	bitangent = bitangent - tanOffset;

	SPipTangents planeTangent;
	PackTriangleTangent(tangent, bitangent, planeTangent);

	// Physicalized fragments use offset positions, so need UV coords adjusting
	const Vec2& uvOffset = isPhysFrag ? m_frags[fragIndex].m_center : Vec2Constants<float>::fVec2_Zero;

	// Surface side determines Z offset
	float surfaceZOffset = 0.0f;

	switch (side)
	{
	case EGlassSurfaceSide_Front:
		surfaceZOffset = m_glassParams.thickness;
		break;

	case EGlassSurfaceSide_Back:
		surfaceZOffset = 0.0f;
		break;

	default: // EGlassSurfaceSide_Center
		surfaceZOffset = m_glassParams.thickness * 0.5f;
	}
	;

	// Create vertex data
	for (int i = 0; i < vertCount; ++i)
	{
		const int offset = vertOffset + i;

		GenerateVertFromPoint(vertices[i], uvOffset, buffer.m_verts[offset]);
		buffer.m_verts[offset].xyz.z += surfaceZOffset;
		buffer.m_tans[offset] = planeTangent;
	}

	// Copy indices with correct offset into vertices
	for (int i = 0; i < indCount; ++i)
	{
		buffer.m_inds[indOffset + i] = indices[i] + vertOffset;
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: BuildFragmentOutlineData
// Desc: Generates the outline triangle data for all fragments
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::BuildFragmentOutlineData(const uint8 fragIndex, uint& vertOffs, uint& indOffs, const int subFragID)
{
	GLASS_FUNC_PROFILER

	Vec3 pt0, pt1, pt2;
	const float thickness = m_glassParams.thickness;
	const float edgeUVOffset = 0.003f;
	const bool impactDistInAlpha = true;

	// Get fragment data
	const PodArray<Vec2>& pts = m_frags[fragIndex].m_outlinePts;
	const Vec2* pPts = pts.begin();
	const int numPts = pts.Count();

	const int vertStep = 2;
	const int indStep = 6;

	const int vertCount = numPts * vertStep;
	const int indCount = numPts * indStep;

	// Get offset into output arrays
	const bool isPhysFrag = (subFragID >= 0);
	SGlassGeom& buffer = isPhysFrag ? m_fragFullGeom[subFragID] : m_crackGeom;

	const int vertOffset = vertOffs;
	const int indOffset = indOffs;

	// Increment external counters
	vertOffs += vertCount;
	indOffs += indCount;

	// Log if we've somehow managed to get out of sync
	if (vertOffs > (uint)buffer.m_verts.Count() || indOffs > (uint)buffer.m_inds.Count() || vertOffs > (uint)buffer.m_tans.Count())
	{
		LOG_GLASS_ERROR("Fragment geometry buffers out of sync. "
		                "vCnt(%i), vOffs(%u), iCnt(%i), iOffs(%u), "
		                "vbCnt(%i), ibCnt(%i), tbCnt(%i), "
		                "frIdx(%u), sbFrId(%i)",
		                vertCount, vertOffs, indCount, indOffs,
		                buffer.m_verts.Count(), buffer.m_inds.Count(), buffer.m_tans.Count(),
		                fragIndex, subFragID);
	}

	// Create a pair of vertices at each point to show plane thickness
	for (int i = 0, j = 0; i < vertCount; i += vertStep, ++j)
	{
		// Expand (and jitter) back point
		pt0 = pt1 = pPts[j];
		pt1.z += thickness;

		// Next point along, used for normal generation
		pt2 = pPts[(j + 1) % numPts];

		// Physicalized fragments use offset positions, so need UV coords adjusting
		const Vec2& uvOffset = isPhysFrag ? m_frags[fragIndex].m_center : Vec2Constants<float>::fVec2_Zero;

		// Create vertex data
		const int offset = vertOffset + i;
		GenerateVertFromPoint(pt0, uvOffset, buffer.m_verts[offset], impactDistInAlpha);
		GenerateVertFromPoint(pt1, uvOffset, buffer.m_verts[offset + 1]);

		buffer.m_verts[offset + 1].color = buffer.m_verts[offset].color;

		// Apply a slight uv offset to help avoid stretching
		//	- Would be nice to map/mirror away from fragment center
		buffer.m_verts[offset].st.y -= edgeUVOffset;
		buffer.m_verts[offset + 1].st.y += edgeUVOffset;

		// Calculate triangle tangent
		GenerateTriangleTangent(pt0, pt1, pt2, buffer.m_tans[offset]);
		buffer.m_tans[offset + 1] = buffer.m_tans[offset];
	}

	// Create quad indices with correct offset into vertices
	for (int i = 0, j = 0; i < indCount; i += indStep, j += vertStep)
	{
		// Get cyclic vert indices
		const int currIndex = vertOffset + j;
		const int nextIndex = vertOffset + (j + vertStep >= vertCount ? 0 : j + vertStep);

		// Create index data
		const int offset = indOffset + i;

		buffer.m_inds[offset] = currIndex;
		buffer.m_inds[offset + 1] = currIndex + 1;
		buffer.m_inds[offset + 2] = nextIndex + 1;

		buffer.m_inds[offset + 3] = currIndex;
		buffer.m_inds[offset + 4] = nextIndex + 1;
		buffer.m_inds[offset + 5] = nextIndex;
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GenerateDefaultPlaneGeom
// Desc: Creates simple plane geometry
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::GenerateDefaultPlaneGeom()
{
	// Create plane fragment shape
	TPolygonArray outline;
	outline.resize(4);

	const float planeMin = 0.0f;
	const float planeMaxX = m_glassParams.size.x;
	const float planeMaxY = m_glassParams.size.y;

	outline[0].set(planeMaxX, planeMin);
	outline[1].set(planeMaxX, planeMaxY);

	outline[2].set(planeMin, planeMaxY);
	outline[3].set(planeMin, planeMin);

	// Initialise fragment
	const uint8 noParent = (uint8) - 1; // Forcing a value we *know* is invalid
	if (CreateFragmentFromOutline(outline.begin(), outline.size(), noParent))
	{
		// Force geometry rebuild
		m_geomDirty = true;
	}
	else
	{
		LOG_GLASS_ERROR("Failed to create default plane, possible corrupt glass dimensions.");
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GenerateGeomFromFragment
// Desc: Creates initial fragment from input
//--------------------------------------------------------------------------------------------------
bool CREBreakableGlass::GenerateGeomFromFragment(const Vec2* pFragPts, const uint numPts)
{
	bool success = false;
	if (pFragPts && numPts >= 3)
	{
		// Initialise fragment
		const int noParent = -1;
		if (CreateFragmentFromOutline(pFragPts, numPts, noParent))
		{
			// Force geometry rebuild
			m_geomDirty = true;
			success = true;
		}
		else
		{
			LOG_GLASS_ERROR("Failed to create initial fragment, possible corrupt geometry extraction.");
		}
	}

	return success;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GenerateVertFromPoint
// Desc: Generates a single vertex from input point
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::GenerateVertFromPoint(const Vec3& pt, const Vec2& uvOffset, SVF_P3F_C4B_T2F& vert, const bool impactDistInAlpha)
{
	GLASS_FUNC_PROFILER

	// Position
	vert.xyz = pt;

	// UV coordinates
	const Vec2 uvPt(pt.x + uvOffset.x, pt.y + uvOffset.y);

	vert.st = m_glassParams.uvOrigin;
	vert.st += m_glassParams.uvXAxis * uvPt.x;
	vert.st += m_glassParams.uvYAxis * uvPt.y;

	// White colour
	vert.color.dcolor = 0xFFFFFFFF;

	// Distance to closest impact point
	if (impactDistInAlpha)
	{
		float alphaDistScale = 255.0f;
		alphaDistScale *= s_pCVars ? s_pCVars->m_impactEdgeFadeScale : 2.0f;

		float centerDist = GetClosestImpactDistance(Vec2(pt.x, pt.y));
		centerDist = clamp_tpl<float>(centerDist * alphaDistScale, 0.0f, 255.0f);

		vert.color.bcolor[EColIdx_A] = 0xFF - (byte)centerDist;

		// Add a subtle dark blue-green tint
		vert.color.bcolor[EColIdx_B] = 0xDF;
		vert.color.bcolor[EColIdx_G] = 0xDF;
		vert.color.bcolor[EColIdx_R] = 0xBF;
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GenerateTriangleTangent
// Desc: Generates triangle tangent/bitangent data
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::GenerateTriangleTangent(const Vec3& triPt0, const Vec3& triPt1, const Vec3& triPt2, SPipTangents& tangent)
{
	const Vec3 forward = (triPt1 - triPt0).GetNormalizedSafe();
	const Vec3 right = (triPt2 - triPt0).GetNormalizedSafe();

	PackTriangleTangent(forward, right, tangent);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: PackTriangleTangent
// Desc: Packs triangle tangent/bitangent data into SPipTangent structure
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::PackTriangleTangent(const Vec3& tangent, const Vec3& bitangent, SPipTangents& tanBitan)
{
	tanBitan = SPipTangents(tangent, bitangent, 1);
}//-------------------------------------------------------------------------------------------------

#ifdef GLASS_DEBUG_MODE
//--------------------------------------------------------------------------------------------------
// Name: GenerateImpactGeom
// Desc: Creates simple impact quad line geometry
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::GenerateImpactGeom(const SGlassImpactParams& impact)
{
	#ifndef GLASS_PROFILER_ENABLED
	// Revert existing transformation
	if (!m_impactLineList.IsEmpty())
	{
		const bool inverse = true;
		TransformPointList(m_impactLineList, inverse);
	}

	// Add new quad
	int offset = m_impactLineList.Count();
	m_impactLineList.resize(offset + 8);

	m_impactLineList[offset + 0].Set(impact.x - IMPACT_HALF_SIZE, impact.y + IMPACT_HALF_SIZE, 0.0f);
	m_impactLineList[offset + 1].Set(impact.x + IMPACT_HALF_SIZE, impact.y + IMPACT_HALF_SIZE, 0.0f);

	m_impactLineList[offset + 2].Set(impact.x - IMPACT_HALF_SIZE, impact.y - IMPACT_HALF_SIZE, 0.0f);
	m_impactLineList[offset + 3].Set(impact.x + IMPACT_HALF_SIZE, impact.y - IMPACT_HALF_SIZE, 0.0f);

	m_impactLineList[offset + 4].Set(impact.x + IMPACT_HALF_SIZE, impact.y - IMPACT_HALF_SIZE, 0.0f);
	m_impactLineList[offset + 5].Set(impact.x + IMPACT_HALF_SIZE, impact.y + IMPACT_HALF_SIZE, 0.0f);

	m_impactLineList[offset + 6].Set(impact.x - IMPACT_HALF_SIZE, impact.y - IMPACT_HALF_SIZE, 0.0f);
	m_impactLineList[offset + 7].Set(impact.x - IMPACT_HALF_SIZE, impact.y + IMPACT_HALF_SIZE, 0.0f);

	// Add circle showing impact radius
	const Vec3 center(impact.x, impact.y, 0.0f);
	const float radius = GetImpactRadius(impact);

	if (radius > 0.0f)
	{
		float angle = 0.0f;
		const float angleInc = gf_PI2 / (float)IMPACT_NUM_IMPULSE_SIDES;

		offset = m_impactLineList.Count();

		const int doubleNumSides = IMPACT_NUM_IMPULSE_SIDES * 2;
		m_impactLineList.resize(offset + doubleNumSides);

		// Pre-calculate first sin/cos
		float sinA, cosA, sinB, cosB;
		sincos_tpl(angle, &sinA, &cosA);
		angle += angleInc;

		// Generate circle shape
		for (int i = 0; i < doubleNumSides; i += 2, angle += angleInc)
		{
			sincos_tpl(angle, &sinB, &cosB);

			const Vec3 a(sinA, cosA, 0.0f);
			const Vec3 b(sinB, cosB, 0.0f);

			m_impactLineList[offset + i] = a * radius + center;
			m_impactLineList[offset + i + 1] = b * radius + center;

			sinA = sinB;
			cosA = cosB;
		}
	}

	// Transform for rendering
	TransformPointList(m_impactLineList);
	#endif // !GLASS_PROFILER_ENABLED
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: TransformPointList
// Desc: Transforms a list of points to/from world-space
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::TransformPointList(PodArray<Vec3>& ptList, const bool inverse)
{
	#ifndef GLASS_PROFILER_ENABLED
	// Get transformation
	Matrix34 transMat = m_params.matrix;

	if (inverse)
	{
		transMat.Invert();
	}

	// Transform points
	const int numPts = ptList.Count();
	Vec3* pPts = ptList.begin();

	for (int i = 0; i < numPts; ++i)
	{
		pPts[i] = transMat.TransformPoint(pPts[i]);
	}
	#endif // !GLASS_PROFILER_ENABLED
}//-------------------------------------------------------------------------------------------------
#endif // GLASS_DEBUG_MODE
