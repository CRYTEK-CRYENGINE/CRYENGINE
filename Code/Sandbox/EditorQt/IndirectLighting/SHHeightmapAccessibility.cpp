// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SHHeightmapAccessibility.h"
#include "Quadtree/QuadtreeHelper.h"
#include "TerrainObjectMan.h"

const float CHemisphereSink_SH::scSampleHeightOffset = 1.f;
const float CHemisphereSink_SH::scRayLength = 15.f;
const float CHemisphereSink_SH::scObstructedColourMultiplier = 3.f;

const float CHemisphereSink_SH::cA1 = 2.094395f / 3.141593f;
const float CHemisphereSink_SH::cA2 = 0.785398f / 3.141593f;

CHemisphereSink_SH::CHemisphereSink_SH(const DWORD indwAngleSteps, const DWORD indwWidth, const DWORD indwHeight)
	: m_SHRotMatrix(9), m_pTerrainObjMan(NULL), m_AngleSteps((uint32)indwAngleSteps),
	m_SHSamples((size_t)indwAngleSteps), m_WaterLevel(0.f), m_Width((uint32)indwWidth)
{
	assert(indwWidth == indwHeight);
	Matrix33_tpl<float> rotMat;
	//get rotation matrix to transform into SH coord system
	//we need a rotation of -90 degree around z
	rotMat.SetIdentity();
	//SH_WORLD_EXCHANGE
	rotMat(0, 0) *= -1.f;
	//	rotMat(1,1) *= -1.f;
	m_SHRotMatrix.SetSHRotation(rotMat);
	//initialize samples
	NSH::CQuantizedSamplesHolder sampleHolder(m_AngleSteps);
	m_SampleCount = NSH::NFramework::QuantizeSamplesAzimuth(sampleHolder, false /*full hs*/);
	m_AngleDirections.resize(m_AngleSteps);

	m_DoMP = false;
#if defined(DO_MP)
	//init multiprocessing
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	m_CoreCount = std::max(systemInfo.dwNumberOfProcessors, (DWORD)1);
	m_DoMP = m_CoreCount > 1;
	m_SHSamplesMP.resize(m_AngleSteps);
	if (m_DoMP)
		m_MPData.resize(m_CoreCount);
#endif

	for (size_t i = 0; i < (size_t)m_AngleSteps; ++i)
	{
		const float cWedgeAngle = ((float)i + 0.5f) / (float)m_AngleSteps * (float)gf_PI2;
		m_AngleDirections[i] = Vec2(-sinf(cWedgeAngle), -cosf(cWedgeAngle));

		//compute index into quantized array, need to rotate it by -90 degree
		const size_t cQuantIndex = (i + m_AngleSteps / 4) % m_AngleSteps;
		TSingleWedgeSampleVec& rWedgeSHVec = m_SHSamples[i].second;

		SFloatCoeffs& rWedgeCoeffs = m_SHSamples[i].first;
		rWedgeCoeffs.Init();

		const size_t cStepSampleCount = sampleHolder.GetDiscretizationSampleCount(cQuantIndex);
		rWedgeSHVec.reserve(cStepSampleCount / 2 + 1);//only upper hs

#if defined(DO_MP)
		int curSampleNum = 0;
		TWedgeSampleVecMPSingle& rWedgeSHVecMP = m_SHSamplesMP[i];
		if (m_DoMP)
		{
			const uint32 cWedgeCountMP = cStepSampleCount / (2 + m_CoreCount) + 1;//only upper hs
			rWedgeSHVecMP.resize(m_CoreCount);
			for (int mp = 0; mp < m_CoreCount; ++mp)
				rWedgeSHVecMP[mp].reserve(cWedgeCountMP);
		}
#endif

		for (int s = 0; s < cStepSampleCount; ++s)
		{
			const NSH::TSample& crSample = sampleHolder.GetDiscretizationSample(s, cQuantIndex);
			SWedgeSHSamples wegdeSample;
#if defined(DO_TERRAIN_RAYCASTING)
			//0 means no value in z(theta is pi/2 there)
			wegdeSample.altitude = std::max(0.f, (float)(gf_PI / 2 - crSample.GetPolarPos().theta));
#endif
			for (int sa = 0; sa < 9; ++sa)
				wegdeSample.coeffs[sa] = crSample.Coeffs()[sa];
			const NSH::SCartesianCoord_tpl<double>& cCartCoord = crSample.GetCartesianPos();
			//bring ray direction into world space direction
			float phi = (crSample.GetPolarPos().phi + 1.5f * gf_PI);
			if (phi >= gf_PI2)
				phi -= gf_PI2;
			NSH::TPolarCoord p(crSample.GetPolarPos().theta, phi);
			NSH::TCartesianCoord c;
			NSH::ConvertToCartesian(c, p);
			if (cCartCoord.z < 0.f)
				rWedgeCoeffs += wegdeSample;
			else
			{
				wegdeSample.dirZ = cCartCoord.z;
				rWedgeSHVec.push_back(wegdeSample);
#if defined(DO_MP)
				if (m_DoMP)
				{
					//also put sample equally into according container
					rWedgeSHVecMP[curSampleNum % m_CoreCount].push_back(wegdeSample);
				}
				++curSampleNum;
#endif
			}
		}
	}
}

void CHemisphereSink_SH::Init
(
  CTerrainObjectMan* cpTerrainObjMan,
  float* pHeightMap,
  const float cWaterLevel,
  const float cHeightScale,
  const uint32 cWorldSpaceMul
)
{
	assert(cpTerrainObjMan);
	m_pTerrainObjMan = cpTerrainObjMan;
	m_pHeightMap = pHeightMap;
	m_WaterLevel = cWaterLevel;
	m_HeightScale = cHeightScale;
	m_WorldSpaceMul = cWorldSpaceMul;

	//init threads and event ids
#if defined(DO_MP)
	if (m_DoMP)
		for (int i = 0; i < m_CoreCount; ++i)
		{
			SMPData& rMPData = m_MPData[i];
			rMPData.pTerrainObjMan = m_pTerrainObjMan;
			rMPData.eventLoopID = CreateEvent(NULL, false, false, NULL);
			rMPData.eventID = CreateEvent(NULL, false, false, NULL);

			if (!gEnv->pThreadManager->SpawnThread(&m_MPData[i], "SHLighting_%u", i))
			{
				CryFatalError("Error spawning \"SHLighting__%u\" thread.", i);
			}
		}
#endif
}

void CHemisphereSink_SH::InitSample(SampleType& rInoutValue, const uint32 cX, const uint32 cY) const
{
	rInoutValue.pSHData = NULL;
	rInoutValue.InitFlags();
	rInoutValue.SetSampleLink();//set to no link if no argument is provided
	rInoutValue.colCount = 0;

	const Vec3 cPos(GridToWorldPos(Vec3(cX, cY, m_pHeightMap[cY * m_Width + cX] + scSampleHeightOffset /*offset a little bit above terrain*/)));

	/*offset z-pos a little bit over terrain surface*/
	rInoutValue.posZ = cPos.z;
	rInoutValue.posX = (uint16)cPos.x;
	rInoutValue.posY = (uint16)cPos.y;
	uint32 obstructionIndex = 0xFFFFFFFF;
	//WORLD_HMAP_COORDINATE_EXCHANGE
	float obstructionAmount = 1.f;
	const uint8 cObstruction = m_pTerrainObjMan->IsObstructed(cX, cY, m_Width, obstructionIndex, obstructionAmount, false);//obstruction map is in heightmap space
	if (cObstruction == CTerrainObjectMan::scObstructed)
		rInoutValue.SetObstructed(true);//OnBeforeProcessing will take care of coordinate translation
	if (rInoutValue.posZ <= m_WaterLevel)
		rInoutValue.SetToBeIgnored(true);
}

void CHemisphereSink_SH::AddWedgeArea(const float cCurWedgeAngle, const float cInfSlope, SampleType& rInoutValue, const float cSlopeDist)
{
	if (rInoutValue.IsToBeIgnored() && !rInoutValue.HasObjectLink())
		return;

	//cCurWedgeAngle is azimuth angle (0..2pi)
	float wedgeHorizonAngle = atanf(-cInfSlope);      // [0..PI/2[
	//lower slope if too close
	if (cSlopeDist < 2)
		wedgeHorizonAngle *= 0.5f;
	else
	{
		//attenuate slope according to distance
		//constant for weighting the slope visibility according to distance (do not let hills very far away influence vis too much)
		static const float scLowerSlopeDistValue = 50.f;    //attenuate terrain vis in 50 m
		static const float scUpperSlopeDistValue = 1000.f;  //in some distance terrain is fully visible again (foggy env will scatter enough light to compensate for this)
		const float cLerpFactor = std::min(scUpperSlopeDistValue - scLowerSlopeDistValue, std::max(cSlopeDist * m_WorldSpaceMul - scLowerSlopeDistValue, 0.f)) / (scUpperSlopeDistValue - scLowerSlopeDistValue);
		wedgeHorizonAngle *= (1.f - cLerpFactor);
	}

	const uint32 cAzimutIndex = (uint32)(cCurWedgeAngle * (float)m_AngleSteps / (float)(gf_PI2));
	assert((size_t)cAzimutIndex < m_SHSamples.size());
	const TSingleWedgeSampleVecPair& crWedgeSamples = m_SHSamples[cAzimutIndex];
	//query ray intersection objects from sample pos in cCurWedgeAngle
	const Vec2& crRayDir = m_AngleDirections[cAzimutIndex];
	static const float scNonObjectRadiusShift = 0.35f;//amount of origin movement for non object samples
	bool rayIntersectionDataValid = false;
	float xPos, yPos;

	if (!rInoutValue.IsToBeIgnored() && rInoutValue.colCount != 0xFFFF)// != 0xFFFF black sample inside large objects
	{
		const bool cDoRayCasting = rInoutValue.DoRayCasting();
		if (cDoRayCasting)
		{
			//move ray origin a bit into the xy direction to be more exact
			const Vec3 cRayOrigin = Vec3((float)rInoutValue.posX, (float)rInoutValue.posY, rInoutValue.posZ);
			Vec3 newOrigin(cRayOrigin + Vec3(crRayDir.x * scNonObjectRadiusShift, crRayDir.y * scNonObjectRadiusShift, 0.f));
			newOrigin.x = std::max(0.f, newOrigin.x);
			newOrigin.y = std::max(0.f, newOrigin.y);
			xPos = newOrigin.x;
			yPos = newOrigin.y;
			m_pTerrainObjMan->CalcRayIntersectionData
			(
			  newOrigin,
			  crRayDir,
			  scRayLength
			);
			rayIntersectionDataValid = true;
		}
		if (!cDoRayCasting || !m_DoMP)
			PerformRayCasting(crWedgeSamples, cDoRayCasting, rInoutValue, wedgeHorizonAngle);
		else
			PerformRayCastingMP(crWedgeSamples, m_SHSamplesMP[cAzimutIndex], rInoutValue, wedgeHorizonAngle);//call mp version
	}

	SampleType* pSample = &rInoutValue;
	while (pSample->HasObjectLink())
	{
		pSample = RetrieveSample(pSample->linkID);
		if (pSample->colCount != 0xFFFF)//object inside other large objects
		{
			const bool cDoRayCasting = pSample->DoRayCasting();
			if (cDoRayCasting)
			{
				const Vec3 cRayOrigin = Vec3(pSample->posXFloat, pSample->posYFloat, pSample->posZ);
				Vec3 newOrigin = cRayOrigin;
				//check if we need to fetch the quadtree contents
				bool getQuadTreeCont = !rayIntersectionDataValid;
				if (rayIntersectionDataValid)
				{
					const float cMaxDistSq = 1.f + 1.f;
					const float cDistSq = (newOrigin.x - xPos) * (newOrigin.x - xPos) + (newOrigin.y - yPos) * (newOrigin.y - yPos);
					if (cDistSq > cMaxDistSq)
						getQuadTreeCont = true;
				}
				if (getQuadTreeCont)
				{
					xPos = newOrigin.x;
					yPos = newOrigin.y;
					rayIntersectionDataValid = true;
				}
				m_pTerrainObjMan->CalcRayIntersectionData
				(
				  newOrigin,
				  crRayDir,
				  scRayLength,
				  getQuadTreeCont
				);
			}
			if (!cDoRayCasting || !m_DoMP)
				PerformRayCasting(crWedgeSamples, cDoRayCasting, *pSample, wedgeHorizonAngle);
			else
				PerformRayCastingMP(crWedgeSamples, m_SHSamplesMP[cAzimutIndex], *pSample, wedgeHorizonAngle);//call mp version
		}
	}
}

#if defined(DO_MP)
void CHemisphereSink_SH::SMPData::ThreadEntry()
{
	CHemisphereSink_SH::SMPData& rMPData = *this;
	CTerrainObjectMan& rTerrainObjMan = *rMPData.pTerrainObjMan;
	while (true)
	{
		WaitForSingleObject(rMPData.eventLoopID, INFINITE);
		assert(rMPData.pWedges != NULL);
		if ((intptr_t)rMPData.pWedges == -1)
		{
			CloseHandle(rMPData.eventLoopID);
			CloseHandle(rMPData.eventID);
			return;
		}
		ColorF col;
		//we have something to process
		const CHemisphereSink_SH::TSingleWedgeSampleVec& crWedgeSamples = *rMPData.pWedges;
		const CHemisphereSink_SH::TSingleWedgeSampleVec::const_iterator cEnd = crWedgeSamples.end();
		for (CHemisphereSink_SH::TSingleWedgeSampleVec::const_iterator iter = crWedgeSamples.begin(); iter != cEnd; ++iter)
		{
			//is above horizon
			const CHemisphereSink_SH::SWedgeSHSamples& crSample = *iter;
			//calc ray intersection colour
			if (rTerrainObjMan.RayIntersection(col, crSample.dirZ))//do not test below too much
			{
				++rMPData.colCount;
				if (col.a < 0.95f)//1 indicates opacity
				{
					const float cAlphaWeight = 1.f - col.a;
					for (int i = 0; i < 9; ++i)
						rMPData.shCoeffs.coeffs[i] += crSample.coeffs[i] * cAlphaWeight;
				}
			}
			else
			{
				//test against terrain
	#if defined(DO_TERRAIN_RAYCASTING)
				if (crSample.altitude < rMPData.wedgeHorizonAngle)
				{
					//add some basic visibility
					const float cBasicVis = 0.3f;
					for (int i = 0; i < 9; ++i)
						rMPData.shCoeffs.coeffs[i] += crSample.coeffs[i] * cBasicVis;

					++rMPData.colCount;
				}
				else
	#endif //DO_TERRAIN_RAYCASTING
				{
					//sky is visible
					for (int i = 0; i < 9; ++i)
						rMPData.shCoeffs.coeffs[i] += crSample.coeffs[i];
				}
			}
		}
		SetEvent(rMPData.eventID);
	}
}
#endif

void CHemisphereSink_SH::PerformRayCastingMP
(
  const TSingleWedgeSampleVecPair& crWedgeSamples,
  TWedgeSampleVecMPSingle& rWedgeSamplesMP,
  SampleType& rInoutValue,
  const float cWedgeHorizonAngle
)
{
#if defined(DO_MP)
	for (int i = 0; i < m_CoreCount; ++i)
	{
		SMPData& rMPData = m_MPData[i];
		rMPData.Reset();
		rMPData.pWedges = &rWedgeSamplesMP[i];
		rMPData.wedgeHorizonAngle = cWedgeHorizonAngle;
		SetEvent(rMPData.eventLoopID);    //make thread to resume
	}

	//iterate all altitudes and get the ray query with its hit colour
	//if it is above the cWedgeHorizonAngle and didnt hit any objects add a 1.0 to the sh coefficients
	//get value for below horizon
	float visBelow = 1.f;
	ColorF col;
	if (m_pTerrainObjMan->RayIntersection(col, -0.01f))
		visBelow = 1.f - col.a;

	SSHSampleOnDemand& rSHData = *rInoutValue.pSHData;
	const SFloatCoeffs& crLowerHSCoeffs = crWedgeSamples.first;
	//add sh coeffs for lower hemisphere
	for (int i = 0; i < 9; ++i)
		rSHData.shCoeffs[i] += crLowerHSCoeffs.coeffs[i] * visBelow;
	//join and gather results from threads
	for (int i = 0; i < m_CoreCount; ++i)
	{
		SMPData& rMPData = m_MPData[i];
		WaitForSingleObject(rMPData.eventID, INFINITE);
		//accumulate data
		for (int i = 0; i < 9; ++i)
			rSHData.shCoeffs[i] += rMPData.shCoeffs.coeffs[i];
		rInoutValue.colCount += rMPData.colCount;
	}
#endif
}

void CHemisphereSink_SH::PerformRayCasting
(
  const TSingleWedgeSampleVecPair& crWedgeSamples,
  const bool cDoRayCasting,
  SampleType& rInoutValue,
  const float cWedgeHorizonAngle
) const
{
	//iterate all altitudes and get the ray query with its hit colour
	//if it is above the cWedgeHorizonAngle and didnt hit any objects add a 1.0 to the sh coefficients
	//get value for below horizon
	float visBelow = 1.f;
	ColorF col;

#if defined(DO_OBJECT_RAYCASTING)
	if (cDoRayCasting && m_pTerrainObjMan->RayIntersection(col, -0.01f))
		visBelow = 1.f - col.a;
#endif

	SSHSampleOnDemand& rSHData = *rInoutValue.pSHData;
	const SFloatCoeffs& crLowerHSCoeffs = crWedgeSamples.first;
	//add sh coeffs for lower hemisphere
	for (int i = 0; i < 9; ++i)
		rSHData.shCoeffs[i] += crLowerHSCoeffs.coeffs[i] * visBelow;
	const TSingleWedgeSampleVec::const_iterator cEnd = crWedgeSamples.second.end();
	for (TSingleWedgeSampleVec::const_iterator iter = crWedgeSamples.second.begin(); iter != cEnd; ++iter)
	{
		//is above horizon
		const SWedgeSHSamples& crSample = *iter;
		//calc ray intersection colour
#if defined(DO_OBJECT_RAYCASTING)
		if (cDoRayCasting && m_pTerrainObjMan->RayIntersection(col, crSample.dirZ))//do not test below too much
		{
			++rInoutValue.colCount;
			if (col.a < 0.95f)//1 indicates opacity
			{
				const float cAlphaWeight = 1.f - col.a;
				for (int i = 0; i < 9; ++i)
					rSHData.shCoeffs[i] += crSample.coeffs[i] * cAlphaWeight;
			}
		}
		else
#endif //DO_OBJECT_RAYCASTING
		{
			//test against terrain
#if defined(DO_TERRAIN_RAYCASTING)
			if (crSample.altitude < cWedgeHorizonAngle)
			{
				//add some basic visibility
				const float cBasicVis = 0.3f;
				for (int i = 0; i < 9; ++i)
					rSHData.shCoeffs[i] += crSample.coeffs[i] * cBasicVis;

				++rInoutValue.colCount;
			}
			else
#endif //DO_TERRAIN_RAYCASTING
			{
				//sky is visible
				for (int i = 0; i < 9; ++i)
					rSHData.shCoeffs[i] += crSample.coeffs[i];
			}
		}
	}
}

void CHemisphereSink_SH::OnCalcEnd(SampleType& rInoutValue)
{
	if (!rInoutValue.IsToBeIgnored())
	{
		//post scale the numerical integration
		const double cSampleScale = 4. /*full hs*/ * NSH::g_cPi / (double)m_SampleCount;
		SSHSampleOnDemand& rSHData = *rInoutValue.pSHData;
		//average colour
		float upperHSAveVis = 1.f;
		if (rInoutValue.colCount == 0xFFFF)
		{
			//set SH data to no visibility (used for completely by large objects obstructed samples
			for (int i = 0; i < 9; ++i)
				rSHData.shCoeffs[i] = 0.f;
		}
		else
		{
			if (rInoutValue.colCount < scMinColCount)//not enough hits, assign default values
			{
				//default reflection colour is white, avoids any assignment of colours due to dark areas from the sh coeffs
				rSHData.shCoeffs[0] = gscDefaultVisCoeff0;
				for (int i = 1; i < 9; ++i)
					rSHData.shCoeffs[i] = 0.f;
			}
			else
			{
				for (size_t j = 0; j < 9; ++j)
				{
					//post-scale by inverse sample count
					rSHData.shCoeffs[j] *= (float)cSampleScale;
				}
				//perform transformation from SH coord system into our coord system
				//SH_WORLD_EXCHANGE
				TransformCoeffs(rSHData.shCoeffs);
			}
		}
	}
	if (rInoutValue.HasObjectLink())
	{
		SampleType& rNextSample = *RetrieveSample(rInoutValue.linkID);
		//adapt values in the unions
		const float cXPos = rNextSample.posXFloat;
		const float cYPos = rNextSample.posYFloat;
		const NQT::SVec2<uint16> cConvPos = (NQT::SVec2<uint16> )(NQT::SVec2<float>(cXPos, cYPos));
		rNextSample.posX = cConvPos.x;
		rNextSample.posY = cConvPos.y;
		OnCalcEnd(rNextSample);
	}
	//adjust colour of terrain reflection if sample is obstructed
	if (!rInoutValue.IsToBeIgnored())
	{
		uint32 obstructionIndex = 0xFFFFFFFF;
		float obstructionAmount = 1.f;
		//WORLD_HMAP_COORDINATE_EXCHANGE
		const uint8 cObstruction = m_pTerrainObjMan->IsObstructed
		                           (
		  (uint16)rInoutValue.posY,
		  (uint16)rInoutValue.posX,
		  m_Width * m_WorldSpaceMul,
		  obstructionIndex,
		  obstructionAmount,
		  true
		                           );//obstruction map is in heightmap space
	}
}

void CHemisphereSink_SH::AddSample
(
  SampleType& rSampleToLinkFrom,
  const Vec3& crPos,
  const bool cApplyRayCasting,
  const bool cIsFullyObstructed
)
{
	//always sort sample to be added by distance to original sample, this way we might minimize calls CalcRayIntersectionData
	//assumes sample to link from is already initialized
	SampleType addSample;
	addSample.InitFlags();
	addSample.SetSampleLink();//disable object link
	addSample.pSHData = NULL;

	float height = crPos.z;
	if (rSampleToLinkFrom.IsOffseted())
		height = std::max(height, rSampleToLinkFrom.posZ);//dont place below

	addSample.posZ = height;
	addSample.posXFloat = crPos.x;
	addSample.posYFloat = crPos.y;
	addSample.colCount = 0;

	//get xy distance for sorting
	const float cXDist = (float)rSampleToLinkFrom.posX - crPos.x;
	const float cYDist = (float)rSampleToLinkFrom.posY - crPos.y;
	const float cDistSq = cXDist * cXDist + cYDist * cYDist;

	{
		//first go through the list of attached samples to check if one very close already exists
		const float cDistThreshold = 0.5f;
		SampleType* pSample = &rSampleToLinkFrom;
		while (pSample->HasObjectLink())
		{
			pSample = RetrieveSample(pSample->linkID);
			if (fabs(pSample->posXFloat - crPos.x) < cDistThreshold &&
			    fabs(pSample->posYFloat - crPos.y) < cDistThreshold &&
			    fabs(pSample->posZ - height) < cDistThreshold)
				return;//a sample near this pos does already exists
		}
	}
	addSample.SetDoRayCasting(cApplyRayCasting);
	if (cIsFullyObstructed)
	{
		addSample.SetDoRayCasting(false);
		addSample.SetIsAlreadyDisplaced(true);//mark as translated to not do it twice
		addSample.colCount = 0xFFFF;//mask by setting colour to -1
	}

	SampleType* pSample = &rSampleToLinkFrom;
	SampleType* pLastSample = pSample;
	bool sampleLinked = false;
	while (pSample->HasObjectLink())
	{
		pSample = RetrieveSample(pSample->linkID);
		//get xy distance for sorting
		const float cXDistSample = (float)rSampleToLinkFrom.posX - pSample->posX;
		const float cYDistSample = (float)rSampleToLinkFrom.posY - pSample->posY;
		const float cDistSqSample = cXDistSample * cXDistSample + cYDistSample * cYDistSample;
		if (cDistSqSample > cDistSq + 0.01f)
		{
			//new sample must get inserted between linked samples
			addSample.SetSampleLink(pLastSample->linkID);
			pLastSample->SetSampleLink(m_AddSamples.size());
			sampleLinked = true;
			break;
		}
		pLastSample = pSample;
	}
	if (!sampleLinked)
		pSample->SetSampleLink(m_AddSamples.size());
	m_AddSamples.push_back(addSample);
}

