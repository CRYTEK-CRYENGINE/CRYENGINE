/*
	transfer implementations
*/
#include "stdafx.h"

#if defined(OFFLINE_COMPUTATION)

#include "Transfer.h"
#include "SampleRetrieval.h"
#include <PRT/TransferParameters.h>
#include <PRT/ITransferConfigurator.h>
#include "ISampleGenerator.h"
#include <PRT/SimpleIndexedMesh.h>
#include <PRT/ISHMaterial.h>
#include <CryMath/Cry_GeoIntersect.h>
#include <PRT/SHRotate.h>
#include "RayCaster.h"
#include <limits>

static const double gs_cSampleCosThreshold = 0.000001;
const float NSH::NTransfer::CInterreflectionTransfer::scLowerHSPercentage = (float)(30. / 90.)/*30 degree*/;

//!< helper function which retrieves a mesh index from a vector of mesh pointers
static const size_t GetIndexFromMesh(const NSH::TGeomVec& crMeshes,	const CSimpleIndexedMesh* pMesh)
{
	size_t index = (size_t)-1;
	const NSH::TGeomVec::const_iterator cEnd = crMeshes.end();
	for(NSH::TGeomVec::const_iterator iter = crMeshes.begin(); iter != cEnd; ++iter)
	{
		++index;
		if(*iter == pMesh)
			return index;//found hit
	}
	return (size_t)-1;
}

/**********************************************************************************************************************************************/

void NSH::NTransfer::CInterreflectionTransfer::PrepareVectors
(
	const TGeomVec& crMeshes,		
	const STransferParameters& crParameters,
	const SDescriptor& crSHDescriptor
)
{
	if(m_VerticesToSHProcessPerMesh.size() != crMeshes.size())
		m_VerticesToSHProcessPerMesh.resize(crMeshes.size());
	if(m_DirectCoefficients.size() != crMeshes.size())
		m_DirectCoefficients.resize(crMeshes.size());
	for(size_t mesh=0; mesh<crMeshes.size(); ++mesh)
	{
		const CSimpleIndexedMesh& crMesh = *(crMeshes[mesh]);
		m_VerticesToSHProcessPerMesh[mesh] = crMesh.GetVertexCount();
		m_DirectCoefficients[mesh].resize(crMesh.GetVertexCount());//prepare coefficients for later
		if(crMesh.GetVertexCount() > 0 && m_DirectCoefficients[mesh][0].size() != crSHDescriptor.Coefficients)
		{
			//initialize coeffs to proper number of coeffs(to avoid allocations during the transfer process
			for(uint32 i=0; i<crMesh.GetVertexCount(); ++i)
				m_DirectCoefficients[mesh][i].ReSize(crSHDescriptor.Coefficients);
		}
	}

	//initialize and reserve some space for each mesh
	m_IntersectionInfo.resize(crMeshes.size());
	m_HSVertexCounter.resize(crMeshes.size());
	for(size_t i=0; i<crMeshes.size(); ++i)
	{
		//initialize and reserve some space for each vertex
		m_IntersectionInfo[i].resize(crMeshes[i]->GetVertexCount());
		m_HSVertexCounter[i].resize(crMeshes[i]->GetVertexCount());
	}
}

void NSH::NTransfer::CInterreflectionTransfer::FreeMemory()
{
	TIntersectionsInfo intersectionsInfo;
	THemisphereCountInfo countInfo;
	m_IntersectionInfo.swap(intersectionsInfo);
	m_HSVertexCounter.swap(countInfo);
}

void NSH::NTransfer::CInterreflectionTransfer::InitializeVISCache(const SDescriptor& crSHDescriptor)
{
	//build the BorderColours, remember it is now in tangent space
	//look up slidely above the border to the lower hemisphere(which is located at theta = PI/2)
	m_VISCache.resize(0);
	const double scThetaLookupAngle	= (NSH::g_cPi * 0.5) * 0.95;
	const double scPhiStep = (double)(2. * NSH::g_cPi / (double)scBorderColours);

	m_VISCache.resize(scBorderColours);

	for(uint32 i=0; i<scBorderColours; ++i)
		ConvertToCartesian(m_VISCache[i].tangentSpaceCartCoord, NSH::SPolarCoord_tpl<float>((float)scThetaLookupAngle, (float)scPhiStep*(float)i));
}

//pay attention to have the ray casting prepared to work on all meshes from the crMeshes vector
const bool NSH::NTransfer::CInterreflectionTransfer::Process				
(
	const NSampleGen::ISampleGenerator& crSampleGen,		//!< samples generator where to get samples from
	const TGeomVec& crMeshes,														//!< vector of meshes to work on
	const SDescriptor& crSHDescriptor,									//!< descriptor for spherical harmonic setup
	const STransferParameters& crParameters,						//!< parameters to control the computation
	const ITransferConfigurator& crConfigurator					//!< configurator for transfer process	
)
{
	if(!crParameters.CheckForConsistency())
		return false;//some parameters have wrong values

	CRayCaster rayCaster;
	if(!rayCaster.SetupGeometry(crMeshes, crParameters))
		return false;

	//reset processing status
	m_State.Reset();
	m_State.totalMeshCount		= (uint32)crMeshes.size();
	m_State.totalVertexCount	= (m_State.totalMeshCount>0)?crMeshes[0]->GetVertexCount() : 0;
	m_State.totalRCThreads		= crParameters.rayCastingThreads;

	if(m_VISCache.empty())
		InitializeVISCache(crSHDescriptor);

	PrepareVectors(crMeshes, crParameters, crSHDescriptor);

	//iterate each mesh and vertex
	//count only faces which have a material subject for sh coefficient processing
	TVertexIndexSet vertexIndexSet;
	for(size_t mesh=0; mesh<crMeshes.size(); ++mesh)
	{
		const CSimpleIndexedMesh& crMesh = *(crMeshes[mesh]);
		uint32 verticesToSHProcess = 0;
		for(uint32 i=0; i<crMesh.GetFaceCount(); ++i)
		{
			if(!crMesh.ComputeSHCoeffs(i))	//compute sh coeffs for this material?
				continue;
			const CObjFace& rObjFace = crMesh.GetObjFace(i);
			for(int v = 0; v<3; ++v)//for each face index
			{
				const uint32 cVertexIndex = rObjFace.v[v];
				if((vertexIndexSet.insert(cVertexIndex)).second)
				{
					m_State.overallVertexCount++;			
					verticesToSHProcess++;
				}
			}
		}
		m_VerticesToSHProcessPerMesh[mesh] = verticesToSHProcess;
	}
	m_State.totalPassCount		= 1;

	//rTempIllumOnePassCoeffs[0] is storage location for non cos lobed coefficients, rTempIllumOnePassCoeffs[1] for cos lobed for later interreflection passes
	if(crParameters.rayCastingThreads > 1)
		ComputeDirectPassMultipleThreading(crSampleGen, crMeshes, crSHDescriptor,	rayCaster, crParameters, crConfigurator);
	else
		ComputeDirectPass(crSampleGen, crMeshes, crSHDescriptor,	rayCaster, crParameters, crConfigurator);
	FreeMemory();
	return true;
}

static void RetrieveTangentSpaceMatrix(const CSimpleIndexedMesh& crMesh, const uint32 cVertexIndex, Matrix33_tpl<float>& rTangentSpaceMatrix)
{
	assert(cVertexIndex < (uint32)crMesh.GetNormalCount());
	assert(crMesh.GetNormalCount() == crMesh.GetVertexCount());
	const Vec3& crNormal				= crMesh.GetNormal(cVertexIndex);
	const Vec3& crBiNormal			= crMesh.GetBiNormal(cVertexIndex);
	const Vec3& crTangentNormal = crMesh.GetTNormal(cVertexIndex);
	rTangentSpaceMatrix.SetRow(0, crBiNormal);
	rTangentSpaceMatrix.SetRow(1, crTangentNormal);
	rTangentSpaceMatrix.SetRow(2, crNormal);
}

void NSH::NTransfer::CInterreflectionTransfer::AdjustDirectCoefficients
(
	const TSampleVec& crSamples,
	const uint32 cSampleCount,
	const CSimpleIndexedMesh& crMesh, 
	const Vec2& crTexCoord,
	NSH::CRayCaster& rRayCaster,					
	const float cRayLength,
	const NSH::NTransfer::STransferParameters& crParameters,
	const uint32 cNormalIndex,
	NSH::SCoeffList_tpl<TScalarCoeff>& rCoeffListDirect, 
	const double cBaseSampleScale,
	const NSH::NMaterial::ISHMaterial& crMat,	
	TRayResultVec& rRayResults,			
	const SDescriptor& crSHDescriptor,
	const ITransferConfigurator& crConfigurator,
	const bool cHasTransparentMats,
	const bool cOnlyHemisphere
)
{
	const double cDirectScale = crParameters.bumpGranularity? NSH::g_cPi * cBaseSampleScale : cBaseSampleScale;//used as lookup -> no convolution and divide by PI
	static NSH::SCoeffList_tpl<TScalarCoeff> dest(crSHDescriptor); //save allocations

	if(!crParameters.bumpGranularity || !cOnlyHemisphere)
	{
		//post-scale by inverse sample count
		rCoeffListDirect *= (TScalarCoeff::TComponentType)cDirectScale;
		//now rotate coefficients from world into object space
		if(!crMesh.HasIdentityWSOSRotation())
		{
			const CSimpleIndexedMesh::TSHRotationMatrix& crSHRotMat = crMesh.GetSHWSOSRotation();
			NSH::SCoeffList_tpl<TScalarCoeff> coeff(rCoeffListDirect);//copy coeff
			crSHRotMat.Transform(coeff, rCoeffListDirect);
			rCoeffListDirect = coeff;
		}
	}
	else
	{
		//for bump granularity, we have to give the lower hemisphere some useful values, lookup the borders and add these values to the whole hemisphere
		//discretize the lower hemisphere into additional samples using the visibility looked up at some near border angle
		//build an array of scalars around the hemisphere border to be used to lower one according to polar angles
		const Vec3 cNormal	= crMesh.GetWSNormal(cNormalIndex);
		const Vec3 cPos			= crMesh.GetVertex(cNormalIndex);	const TVec cPosD(cPos);
		const TRGBCoeffType cNeutralIntensity(1.,1.,1.);


		float BorderColours[scBorderColours];//contains for each element 1 if visible, 0 if not(for transparent surfaces, in between values are possible)
		//rotate coefficients into tangent space as a hemisphere above the vertex normal(rotate into sphere origin 0,0,1)
		Matrix33_tpl<float> intoTSMat = crMesh.GetWSOSRotation(), tempRot;
		RetrieveTangentSpaceMatrix(crMesh, cNormalIndex, tempRot);
		//apply first rotation from world into object space
		intoTSMat *= tempRot;
		//now retrieve inverse tangent space matrix
		Matrix33_tpl<float> fromTSMat = intoTSMat;
		fromTSMat.Transpose();
		//build the BorderColours, remember it is now in tangent space
		//look up slidely above the border to the lower hemisphere(which is located at theta = PI/2)
		rRayCaster.ResetCache();
		for(uint32 i=0; i<scBorderColours; ++i)
		{ 
			SVISCache& cVISCache = m_VISCache[i];
			//rotate back to world space and look up visibility
			SCartesianCoord_tpl<float> worldSpaceDir(cVISCache.tangentSpaceCartCoord);
			worldSpaceDir = fromTSMat * worldSpaceDir;
			worldSpaceDir.Normalize();//need it because tangent space might not be orthogonal
			SCartesianCoord_tpl<double> worldSpaceDirD(worldSpaceDir);
			//now look up the visibility for each element
			//bump mapped border lookups(remember: up to a certain degree the lower hemisphere is encoded with the visibility of the border)
			//computes the visibility (0.f = blocked, 1.f = visible)
			SRayResult rayRes;
			float visibility = 0.f;
			if(cHasTransparentMats && crParameters.supportTransparency)//not for backlighting, 360 degrees are used
			{
				TRGBCoeffType incidentIntensity(1.,1.,1.);
				if(rRayCaster.CastRay(rRayResults, cPos, worldSpaceDir, cRayLength, cNormal, crParameters.rayTracingBias))
				{
					bool completelyBlocked;
					bool temp;
					SRayResultData rayResultData(cHasTransparentMats, worldSpaceDirD, false/*lower hs*/, rRayResults, incidentIntensity, completelyBlocked, temp);
					crConfigurator.ProcessRayCastingResult(rayResultData);
					if(!rayResultData.rContinueLoop)
					{
						//process transparent blocks
						//    vis = component_sum(incidentIntensity*vertex_mat) / component_sum(vertex_mat)
						const TRGBCoeffType rgbCoeffVertex = //fetch material property at vertex pos
							crMat.DiffuseIntensity
							(
    						cPosD, 
		  					(const TVec&)worldSpaceDirD, 
			  				crTexCoord, 
				  			TRGBCoeffD(1.,1.,1.), 
					  		worldSpaceDirD,
								false,//no cos
								true,//material application
  							false //no abs cos
							);
						const double cVertexMatComponentSum = rgbCoeffVertex.x + rgbCoeffVertex.y + rgbCoeffVertex.z;
						const double cIncidentVertexComponentSum = rgbCoeffVertex.x * incidentIntensity.x + rgbCoeffVertex.y * incidentIntensity.y + rgbCoeffVertex.z * incidentIntensity.z;
						visibility = (float)(cIncidentVertexComponentSum / cVertexMatComponentSum);
					}
				}
				else
					visibility = 1.f;
			}
			else
			{
				SRayResult rayRes;
				visibility = (rRayCaster.CastRay(rayRes, cPos, worldSpaceDir, cRayLength, cNormal, crParameters.rayTracingBias))? 0.f : 1.f;
			}
			//adjust and store vis values
			if(crParameters.groundPlaneBlockValue != 1.0f && worldSpaceDir.z < 0)//add ground plane block value
			{
				//ray fired toward ground, treat partly as blocked
				visibility *= crParameters.groundPlaneBlockValue;
			}
			BorderColours[i] = std::max(crParameters.minDirectBumpCoeffVisibility, visibility);//make sure minimum visibility is set
		}

		const double scMaxLHSThetaAngleAdd			= NSH::g_cPi * 0.5 * scLowerHSPercentage;
		const double scMaxLHSThetaAngleMax			= NSH::g_cPi * 0.5 + scMaxLHSThetaAngleAdd;
		const double scPhiStep									= (double)(2. * NSH::g_cPi / (double)scBorderColours);
		const TSampleVec::const_iterator cEnd = crSamples.end();
		for(TSampleVec::const_iterator sampleIter = crSamples.begin(); sampleIter != cEnd; ++sampleIter)
		{
			const CSample_tpl<SCoeffList_tpl<TScalarCoeff> >& crSample = *sampleIter;
			//rotate into tangent space
			SCartesianCoord_tpl<double> test = crSample.GetCartesianPos();
			if(test * cNormal > 0)
				continue;//only process lower HS
			//transform into tangent space
			test = intoTSMat * test;
			test.Normalize();
			SPolarCoord_tpl<double> polarCoord;
			ConvertUnitToPolar(polarCoord, test);
			if(polarCoord.theta < scMaxLHSThetaAngleMax)
			{
				//get right index and add it
				const uint16 cIndex = (uint16)(polarCoord.phi / scPhiStep);
				if(BorderColours[cIndex] > 0.f)
				{
					//visible, add world space coeffs(later converted into object space)
					crConfigurator.AddDirectScalarCoefficientValue(rCoeffListDirect, BorderColours[cIndex], crSample);
				}
			}
		}
		//integrated area is upper hemisphere plus lower hemisphere area percentage
		//compute covered area of lower hemisphere (area computed by formular: area sphere zone * sphere height * radius(=1) / 2)
		const double cNewScale = 4. * NSH::g_cPi / (double)crSamples.size();
		rCoeffListDirect *= (TScalarCoeff::TComponentType)cNewScale;//apply new scale
		//now rotate coefficients from world into object space
		if(!crMesh.HasIdentityWSOSRotation())
		{
			const CSimpleIndexedMesh::TSHRotationMatrix& crSHRotMat = crMesh.GetSHWSOSRotation();
			NSH::SCoeffList_tpl<TScalarCoeff> coeff(rCoeffListDirect);//copy coeff
			crSHRotMat.Transform(coeff, rCoeffListDirect);
			rCoeffListDirect = coeff;
		}
	}
}

const float NSH::NTransfer::CInterreflectionTransfer::DetermineRayLength(const TGeomVec& crMeshes)
{
	Vec3 minExt(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()), 
		maxExt(std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min());
	// get bbox
	for(size_t mesh=0; mesh<crMeshes.size(); ++mesh)
	{
		const CSimpleIndexedMesh& crMesh = *(crMeshes[mesh]);
		minExt.x = (minExt.x > crMesh.GetMinExt().x)? crMesh.GetMinExt().x : minExt.x;
		minExt.y = (minExt.y > crMesh.GetMinExt().y)? crMesh.GetMinExt().y : minExt.y;
		minExt.z = (minExt.z > crMesh.GetMinExt().z)? crMesh.GetMinExt().z : minExt.z;

		maxExt.x = (maxExt.x < crMesh.GetMaxExt().x)? crMesh.GetMaxExt().x : maxExt.x;
		maxExt.y = (maxExt.y < crMesh.GetMaxExt().y)? crMesh.GetMaxExt().y : maxExt.y;
		maxExt.z = (maxExt.z < crMesh.GetMaxExt().z)? crMesh.GetMaxExt().z : maxExt.z;
	}
	return (float)(TVec(maxExt.x, maxExt.y, maxExt.z) - TVec(minExt.x, minExt.y, minExt.z)).GetLength();
}

const bool NSH::NTransfer::CInterreflectionTransfer::HasTransparentMaterials(const TGeomVec& crMeshes)
{
	//determine whether there are any transparent materials or not(in that case ray casting needs to be handled in another way)
	bool hasTransparentMats = false;
	const TGeomVec::const_iterator cEnd = crMeshes.end();
	for(TGeomVec::const_iterator iter = crMeshes.begin(); iter != cEnd; ++iter)
	{
		if((*iter)->HasTransparentMaterials())
			hasTransparentMats = true;
	}
	return hasTransparentMats;
}

void NSH::NTransfer::CInterreflectionTransfer::ComputeDirectPass
(
	const NSampleGen::ISampleGenerator& crSampleGen,
	const TGeomVec& crMeshes,
	const SDescriptor& crSHDescriptor,											
	CRayCaster& rRayCaster,															
	const STransferParameters& crParameters,
	const ITransferConfigurator& crConfigurator					
)
{
	const TRGBCoeffType cNeutralIntensity(1.,1.,1.);
	SCoeffList_tpl<TRGBCoeffType> rgbCoeffDummy; //need reference to something if not interreflection is processed
	
	bool applyTransparency = false;	//reseted value for each sample, indicates that the ray went through a transparent surface
	TRGBCoeffType incidentIntensity;		//reseted value for each sample, is incident intensity from sample direction
	
	//determine whether there are any transparent materials or not(in that case ray casting needs to be handled in another way)
	const bool cHasTransparentMats = HasTransparentMaterials(crMeshes);

	//iterate each mesh and vertex
	//obtain samples
	uint32 sampleCount;
	TSampleVec samples;
	{
		TScalarVecVec sampleVector;
		crSampleGen.GetSamples(sampleVector);
		NSampleGen::CSampleRetrieval_tpl<TScalarCoeff> sampleRetrieval(sampleVector, crParameters.sampleCountPerVertex);
		sampleCount = sampleRetrieval.SampleCount();
		sampleRetrieval.ResetForNextVertex();
		samples.resize(sampleCount);
		const TSampleVec::iterator cEnd = samples.end();
		for(TSampleVec::iterator iter = samples.begin(); iter != cEnd; ++iter)
			*iter = sampleRetrieval.Next();
	}
	//determine ray length from bounding box extensions of all participating meshes	
	const float cRayLen = DetermineRayLength(crMeshes);
	const bool cUseMinVisibility = (crParameters.bumpGranularity && (crParameters.minDirectBumpCoeffVisibility > 0.f));

	TRayCacheVec tempRayCacheVec(sampleCount);//temp vector to not allocate memory too often, count inserted entries itself
  
	for(size_t mesh=0; mesh<crMeshes.size(); ++mesh)
	{
		const CSimpleIndexedMesh& crMesh = *(crMeshes[mesh]);
		TScalarCoeffVec& rCoeffsDirect		= m_DirectCoefficients[mesh];
		THemispherePerMeshCountInfo& rMeshVertexCounter = m_HSVertexCounter[mesh];
		TRayCacheVecVec& rIntersectionInfo = m_IntersectionInfo[mesh];

		GetSHLog().LogTime();
		GetSHLog().Log("computing direct pass: ");
		crMesh.Log();//write some info
		GetSHLog().Log("	%d vertices requiring SH coeffs\n", m_VerticesToSHProcessPerMesh[mesh]);
		m_State.totalVertexCount = crMesh.GetVertexCount();
		TVertexIndexSet vertexIndexSet;

		rRayCaster.ResetMeshStats();

		uint32 visQueries = 0;	//mesh queries for visibility (for log statistics)
		float vis = 0.f;				//counts the resulted visibility (for log statistics)

		for(uint32 i=0; i<crMesh.GetFaceCount(); ++i)
		{
			const bool cCalcSHCoeffs = crMesh.ComputeSHCoeffs(i);	//compute sh coeffs for this material?
			const NSH::NMaterial::ISHMaterial& crMat = crMesh.GetMaterialByFaceID(i);	//cache material reference
			//only hemisphere lighting if not to handle flat backlighting materials(bump mapped)
			const bool cOnlyHemisphere = crConfigurator.ProcessOnlyUpperHemisphere(crMat.Type());
			const CObjFace& rObjFace = crMesh.GetObjFace(i);
			for(int v = 0; v<3; ++v)//for each face index
			{
				const uint32 cVertexIndex = rObjFace.v[v];
				assert(cVertexIndex < (const uint32)crMesh.GetVertexCount());
				if(!(vertexIndexSet.insert(cVertexIndex)).second)
					continue;//already processed vertex
				if(!cCalcSHCoeffs)
					continue;
				const uint32 cNormalIndex = rObjFace.n[v];
				assert(cNormalIndex < (const uint32)crMesh.GetNormalCount());
				const uint32 cTexCoordIndex = rObjFace.t[v];
				assert(cTexCoordIndex < (const uint32)crMesh.GetTexCoordCount());
				Notify();//update observers
				//obtain vertex pos and normal
				const TVec cPos(crMesh.GetVertex(cVertexIndex));
				const TVec cNormal(crMesh.GetWSNormal(cNormalIndex));
				const Vec2 cTexCoord((crMesh.GetTexCoordCount() > 0)?crMesh.GetTexCoord(cTexCoordIndex).s : 0.f, (crMesh.GetTexCoordCount() > 0)?crMesh.GetTexCoord(cTexCoordIndex).t : 0.f);
				//remember that hemispherical retrieval only covers 2 g_cPi of the sphere
				int hemisphereSampleCount = 0;
				rRayCaster.ResetCache(cPos, cRayLen, cNormal, crParameters.rayTracingBias, cOnlyHemisphere, true/*use full vis cache*/);
				SCoeffList_tpl<TScalarCoeff>& rCoeffListDirect			= rCoeffsDirect[cVertexIndex];

				uint32 insertedRayCacheEntries = 0;//accessor for rRayInterreflectionCache
				const TSampleVec::const_iterator cEnd = samples.end();
				for(TSampleVec::const_iterator sampleIter = samples.begin(); sampleIter != cEnd; ++sampleIter)
				{
					const CSample_tpl<SCoeffList_tpl<TScalarCoeff> >& crSample = *sampleIter;
					bool isOnUpperHemisphere = true;//indicates where sample lies
					const TCartesianCoord& rSampleCartCoord = crSample.GetCartesianPos();
					if(rSampleCartCoord * cNormal < gs_cSampleCosThreshold)
					{
						isOnUpperHemisphere = false;
						if(cOnlyHemisphere)
							continue;//consider only hemisphere
					}
					//reset incident intensity
					if(isOnUpperHemisphere)
						hemisphereSampleCount++;//count for sample weighting(also important for indirect passes)

					visQueries++;
					bool continueLoop;	
					bool incrementCache;
					//shoot ray from vertex into direction of sample
					if(PerformRayCasting
					(
						rRayCaster,
						cHasTransparentMats,
						crSample,
						cRayLen,
						cPos,
						cNormal,
						crParameters,	
						incidentIntensity,
						continueLoop,
						applyTransparency,
						incrementCache,
						tempRayCacheVec[insertedRayCacheEntries],
						crMeshes,
						isOnUpperHemisphere,
						crConfigurator
					))
					{
						if(continueLoop)
						{
							if(incrementCache)
								insertedRayCacheEntries++;
							//add minimum visibility value if requested
							if(cUseMinVisibility)
								crConfigurator.AddDirectScalarCoefficientValue(rCoeffListDirect, crParameters.minDirectBumpCoeffVisibility, crSample);
							continue;
						}
					}
					if(incrementCache)
						insertedRayCacheEntries++;

					SRayProcessedData rayProcessData
					(
						crMat, crSample, crParameters, cPos, cNormal, cTexCoord, applyTransparency, isOnUpperHemisphere, false/*no interrefl*/,
						incidentIntensity, rCoeffListDirect, vis
					);
					crConfigurator.TransformRayCastingResult(rayProcessData);
				}//sample loop
				
				//copy recorded intersections (effort made due to saving of allocations)
				const double cBaseSampleScale = (hemisphereSampleCount==0)?0. : 2. / (double)hemisphereSampleCount;//divide by PI because we need exitance radiance here
				const double cDirectScale = cOnlyHemisphere?cBaseSampleScale : 4. / (double)sampleCount; //full sphere if required

				assert(hemisphereSampleCount < ((2 << (sizeof(short)*8-1))) - 1);
				rMeshVertexCounter[cVertexIndex] = (uint16)hemisphereSampleCount; 

				AdjustDirectCoefficients
				(
					samples, sampleCount, crMesh, cTexCoord, rRayCaster, cRayLen, crParameters, cNormalIndex, rCoeffListDirect, 
					cDirectScale, crMat, m_Results, crSHDescriptor, crConfigurator, cHasTransparentMats, cOnlyHemisphere
				);

				m_State.vertexIndex++;
				m_State.overallVertexIndex++;
			}
		}
		Notify();//update observers
		m_State.meshIndex++;//next mesh

		rRayCaster.LogMeshStats();
		GetSHLog().Log("	average visibility per vertex: %.2f percent\n", (visQueries > 0)?(vis / (float)visQueries)*100.f : 0);

		assert(rCoeffsDirect.size() == (size_t)crMesh.GetVertexCount());
	}
}

#pragma warning (disable : 4127) 
void NSH::NTransfer::CInterreflectionTransfer::ComputeDirectPassMultipleThreading
(
	const NSampleGen::ISampleGenerator& crSampleGen,
	const TGeomVec& crMeshes,
	const SDescriptor& crSHDescriptor,											
	CRayCaster& rRayCaster,															
	const STransferParameters& crParameters,
	const ITransferConfigurator& crConfigurator					
)
{
	//determine whether there are any transparent materials or not(in that case ray casting needs to be handled in another way)
	const bool cHasTransparentMats = HasTransparentMaterials(crMeshes);
	SCoeffList_tpl<TRGBCoeffType> rgbCoeffDummy; //need pointer to something if not interreflection is processed

	//iterate each mesh and vertex
	//obtain samples
	uint32 sampleCount;
	TSampleVec samples;
	{
		TScalarVecVec sampleVector;
		crSampleGen.GetSamples(sampleVector);
		NSampleGen::CSampleRetrieval_tpl<TScalarCoeff> sampleRetrieval(sampleVector, crParameters.sampleCountPerVertex);
		sampleCount = sampleRetrieval.SampleCount();
		sampleRetrieval.ResetForNextVertex();
		samples.resize(sampleCount);
		const TSampleVec::iterator cEnd = samples.end();
		for(TSampleVec::iterator iter = samples.begin(); iter != cEnd; ++iter)
			*iter = sampleRetrieval.Next();
	}
	
	//determine ray length from bounding box extensions of all participating meshes	
	const float cRayLen = DetermineRayLength(crMeshes);

	//clone ray caster with all its data for each thread
	TRayCasterVec clonedRayCasters(crParameters.rayCastingThreads - 1);
	const TRayCasterVec::iterator cEnd = clonedRayCasters.end();
	for(TRayCasterVec::iterator iter = clonedRayCasters.begin(); iter != cEnd; ++iter)
		*iter = rRayCaster.CreateClone();

	//works as follows: split each mesh into crParameters.rayCastingThreads vertices
	//built ray casting queries and process within each thread by trying to send to the other processor
	//let main thread go to sleep and check from time to time if some thread has been finished, then process with low priority 
	std::vector<TThreadRayVertexQueryVec, CSHAllocator<TThreadRayVertexQueryVec> > threadQueries(crParameters.rayCastingThreads);	//for each thread build a vector of queries
	std::vector<TRayResultVec, CSHAllocator<TRayResultVec> > threadRayResults(crParameters.rayCastingThreads);
	const std::vector<TRayResultVec, CSHAllocator<TRayResultVec> >::iterator cThreadEnd = threadRayResults.end();
	for(std::vector<TRayResultVec, CSHAllocator<TRayResultVec> >::iterator rayIter = threadRayResults.begin(); rayIter != cThreadEnd; ++rayIter)
		(*rayIter).reserve(50);//reserve for 50 hits for each thread
	//clone configurators for processing ray casting results
	std::vector<ITransferConfiguratorPtr, CSHAllocator<ITransferConfiguratorPtr> > threadConfigurators(crParameters.rayCastingThreads);	
	for(int i=0; i<crParameters.rayCastingThreads; ++i)
		threadConfigurators[i] = crConfigurator.Clone();

	for(size_t mesh=0; mesh<crMeshes.size(); ++mesh)
	{
		const CSimpleIndexedMesh& crMesh	= *(crMeshes[mesh]);
		TScalarCoeffVec& rCoeffsDirect		= m_DirectCoefficients[mesh];
		THemispherePerMeshCountInfo& rMeshVertexCounter = m_HSVertexCounter[mesh];
		TRayCacheVecVec& rIntersectionInfo = m_IntersectionInfo[mesh];

		GetSHLog().LogTime();
		GetSHLog().Log("computing direct pass: ");
		crMesh.Log();//write some info
		
		m_State.totalVertexCount = crMesh.GetVertexCount();
		m_State.rcThreadsRunning = 0;
		
		TVertexIndexSet vertexIndexSet;
		rRayCaster.ResetMeshStats();

		uint32 visQueries = 0;	//mesh queries for visibility (for log statistics)
		float vis = 0.f;				//counts the resulted visibility (for log statistics)
	
		//build ray cast threading queries
		//vertices to process per thread, pay attention that last one gets the rest(may be more, may be less)
		//main thread gets less since it is started as last one and processed as first one (save one thread)
		const uint32 cVerticesPerThread = (uint32)((float)m_VerticesToSHProcessPerMesh[mesh] / (float)(crParameters.rayCastingThreads - 1 + 0.8f/*constant should better be estimated due to number of threads*/)); 
		uint32 currentThreadIndex = 0;
		std::vector<std::pair<HANDLE, uint32>, CSHAllocator<std::pair<HANDLE, uint32> > > threadIDs(crParameters.rayCastingThreads - 1);//pair of handle and thread ids (-1 because main thread is processing too)
		TBoolVec threadStates(crParameters.rayCastingThreads - 1);//states of the threads
		//initialize them
		const TBoolVec::iterator cEnd = threadStates.end();
		for(TBoolVec::iterator iter = threadStates.begin(); iter != cEnd; ++iter)
			*iter = true; //indicate as running

		Notify(); 
		//iterate all faces and unique vertices
		for(uint32 i=0; i<crMesh.GetFaceCount(); ++i)
		{
			const bool cCalcSHCoeffs = crMesh.ComputeSHCoeffs(i);	//compute sh coeffs for this material?
			const NSH::NMaterial::ISHMaterial& crMat = crMesh.GetMaterialByFaceID(i);	//cache material reference
			threadQueries[currentThreadIndex].reserve(cVerticesPerThread);//save memory alloc calls
			const CObjFace& rObjFace = crMesh.GetObjFace(i);
			for(int v = 0; v<3; ++v)//for each face index
			{
				const uint32 cVertexIndex = rObjFace.v[v];
				assert(cVertexIndex < (const uint32)crMesh.GetVertexCount());
				if(!(vertexIndexSet.insert(cVertexIndex)).second)
					continue;//already processed vertex
				if(!cCalcSHCoeffs)
					continue;
				const uint32 cNormalIndex = rObjFace.n[v];
				assert(cNormalIndex < (const uint32)crMesh.GetNormalCount());
				const uint32 cTexCoordIndex = rObjFace.t[v];
				assert(cTexCoordIndex < (const uint32)crMesh.GetTexCoordCount());
				//obtain vertex pos and normal
				const TVec cPos(crMesh.GetVertex(cVertexIndex));
				const TVec cNormal(crMesh.GetWSNormal(cNormalIndex));
				const Vec2 cTexCoord((crMesh.GetTexCoordCount() > 0)?crMesh.GetTexCoord(cTexCoordIndex).s : 0.f, (crMesh.GetTexCoordCount() > 0)?crMesh.GetTexCoord(cTexCoordIndex).t : 0.f);
				
				//build new query for this vertex
				SThreadRayVertexQuery query;
				query.pCoeffListDirect		= &rCoeffsDirect[cVertexIndex];
				query.pos = cPos; 
				query.normal = cNormal;
				query.texCoord = cTexCoord;
				query.cpMat = &crMat;
				query.vertexIndex = cVertexIndex;
				query.normalIndex = cNormalIndex;
				threadQueries[currentThreadIndex].push_back(query);	//add query for this vertex

				if(currentThreadIndex != crParameters.rayCastingThreads-1) //if not last thread
				{
					if(threadQueries[currentThreadIndex].size() == cVerticesPerThread)
					{
						//start thread whilst building the other
						SThreadParams param((currentThreadIndex == 0)?rRayCaster : *clonedRayCasters[currentThreadIndex-1], samples, crParameters, threadQueries[currentThreadIndex], rIntersectionInfo, crMeshes, cRayLen, cHasTransparentMats, threadRayResults[currentThreadIndex], *threadConfigurators[currentThreadIndex]);
						threadIDs[currentThreadIndex].first = CreateThread(NULL, 1024000, RayCastingThread, &param, 0, (LPDWORD)&(threadIDs[currentThreadIndex].second));	
						SetThreadPriority(threadIDs[currentThreadIndex].first, THREAD_PRIORITY_HIGHEST);
						m_State.rcThreadsRunning++;
						currentThreadIndex++;//go to next thread
						Notify();
					}
				}
			}//face vertex
		}//face

		//process one ray casting in the main thread 
		SThreadParams param(*clonedRayCasters[currentThreadIndex-1], samples, crParameters, threadQueries[currentThreadIndex], rIntersectionInfo, crMeshes, cRayLen, cHasTransparentMats, threadRayResults[currentThreadIndex], *threadConfigurators[currentThreadIndex]);
		m_State.rcThreadsRunning++;
		Notify();
		RayCastingThread(&param);	
		m_State.rcThreadsRunning--;

		//ray cast query struct is now built
		//loop til all threads are done (check and put asleep)
		uint32 threadsProcessed = 0;
		bool wasProcessing = false;
		bool processMainThread = true;
		while(true) 
		{
			int32 mostRecentThreadFinished = -1;
			if(!processMainThread)
			{
				if(m_State.rcThreadsRunning == 0 && threadsProcessed == crParameters.rayCastingThreads-1)
					break;//all threads have been finished and have been processed
				//now check that one thread has been finished, if not, go asleep
				for(uint32 i=0; i<crParameters.rayCastingThreads-1; ++i)
				{
					if(threadStates[i])//still running
					{
						if(WaitForSingleObject(threadIDs[i].first, 1) != WAIT_TIMEOUT)
						{
							//has just been finished
							threadStates[i] = false;
							m_State.rcThreadsRunning--;
							Notify();
							mostRecentThreadFinished = i;
							break;//even if another thread has finished too, process this one first before continue checking
						}
					}
				}
				if(mostRecentThreadFinished == -1)
				{
					if(wasProcessing)
					{
						wasProcessing = false;
						SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_LOWEST);//reset main thread priority
					}
					SleepEx(1000, false);//put main thread asleep for some time
					continue;
				}
				if(!wasProcessing)
				{
					SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_HIGHEST);//reset main thread priority
					wasProcessing = true;	
				}
				threadsProcessed++;
			}
			const uint32 cThreadIndex = processMainThread? (crParameters.rayCastingThreads-1) : mostRecentThreadFinished;
			TThreadRayVertexQueryVec& rRayQueries = threadQueries[cThreadIndex];
			processMainThread = false;
			//iterate all vertices using the ray query results
			const TThreadRayVertexQueryVec::iterator cEnd = rRayQueries.end();
			for(TThreadRayVertexQueryVec::iterator vertexIter = rRayQueries.begin(); vertexIter != cEnd; ++vertexIter)
			{
				SThreadRayVertexQuery& rQuery = *vertexIter;
				const Vec2& cTexCoord = rQuery.texCoord;
				const uint32 cNormalIndex = rQuery.normalIndex;

				Notify();//update observers

				SCoeffList_tpl<TScalarCoeff>& rCoeffListDirect			= *rQuery.pCoeffListDirect;
				//remember that hemispherical retrieval only covers 2 g_cPi of the sphere
				const int cHemisphereSampleCount = rQuery.hemisphereSampleCount;
				const bool cOnlyHemisphere = threadConfigurators[cThreadIndex]->ProcessOnlyUpperHemisphere(rQuery.cpMat->Type());
				const double cBaseSampleScale = (cHemisphereSampleCount == 0)?0. : 2. / (double)cHemisphereSampleCount; //divide by PI because we need exitance radiance here
				const double cDirectScale = cOnlyHemisphere?cBaseSampleScale : 4. / (double)sampleCount; //full sphere if required
				const uint32 cVertexIndex = rQuery.vertexIndex;
				const NSH::NMaterial::ISHMaterial& crMat = *rQuery.cpMat;	//cache material reference
				visQueries += rQuery.visQueries;
				vis += rQuery.vis;

				assert(cHemisphereSampleCount < ((2 << (sizeof(short)*8-1))) - 1);
				rMeshVertexCounter[cVertexIndex] = (uint16)cHemisphereSampleCount; 

				AdjustDirectCoefficients
				(
					samples, sampleCount, crMesh, cTexCoord, ((cThreadIndex == 0)?rRayCaster : *clonedRayCasters[cThreadIndex-1]), cRayLen, crParameters, cNormalIndex, rCoeffListDirect, 
					cDirectScale, crMat, threadRayResults[cThreadIndex], crSHDescriptor, *threadConfigurators[cThreadIndex], cHasTransparentMats, cOnlyHemisphere
				);
				//post-scale by inverse sample count
				m_State.vertexIndex++;
				m_State.overallVertexIndex++;
			}//vertices
			//free some memory
			TThreadRayVertexQueryVec rayVertexQueryVec;
			rRayQueries.swap(rayVertexQueryVec);
		}//threads
		Notify();//update observers 
		m_State.meshIndex++;//next mesh

		uint32 fullVisQueries = rRayCaster.GetFullVisQueries();
		uint32 fullVisSuccesses = rRayCaster.GetFullVisSuccesses();
		const TRayCasterVec::const_iterator cClonedEnd = clonedRayCasters.end();
		for(TRayCasterVec::const_iterator rayCasterIter = clonedRayCasters.begin(); rayCasterIter != cClonedEnd; ++rayCasterIter)	
		{
			fullVisQueries += (*rayCasterIter)->GetFullVisQueries();
			fullVisSuccesses += (*rayCasterIter)->GetFullVisSuccesses();
		}
		GetSHLog().Log("	average full visibility: %.2f percent\n", (fullVisQueries > 0)?((float)fullVisSuccesses / (float)fullVisQueries)*100.f : 0);			
		GetSHLog().Log("	average visibility per vertex: %.2f percent\n", (visQueries > 0)?(vis / (float)visQueries)*100.f : 0);

		assert(rCoeffsDirect.size() == (size_t)crMesh.GetVertexCount());
	}
}
#pragma warning (default : 4127) 

DWORD WINAPI NSH::NTransfer::CInterreflectionTransfer::RayCastingThread(LPVOID pThreadParameter)
{
	assert(pThreadParameter);
	SThreadParams* pThreadParam = (SThreadParams*)pThreadParameter;
	//cache params
	CRayCaster& rRayCaster											= pThreadParam->rRayCaster;
	const float cRayLen													= pThreadParam->cRayLen;
	const STransferParameters& crParameters			= pThreadParam->crParameters;
	TThreadRayVertexQueryVec& rQueries					= pThreadParam->rQueries;
	const TGeomVec& crMeshes										= pThreadParam->crMeshes;
	TRayResultVec& rRayResultVec								= pThreadParam->rRayResultVec;
	const ITransferConfigurator& crConfigurator = pThreadParam->crConfigurator;

	const bool cHasTransparentMats		= pThreadParam->cHasTransparentMats;
	const bool cUseMinVisibility			= (crParameters.bumpGranularity && (crParameters.minDirectBumpCoeffVisibility > 0.f));
	const TRGBCoeffType cNeutralIntensity(1.,1.,1.);

	TRayCacheVecVec& rIntersectionInfo = pThreadParam->rIntersectionInfo;
	TRayCacheVec tempRayCacheVec(pThreadParam->crSamples.size());//temp vector to not allocate memory too often, count inserted entries itself

	const bool cAddGroundPlaneBehaviour = (crParameters.groundPlaneBlockValue != 1.0f);
	//iterate each vertex and query
	const TThreadRayVertexQueryVec::iterator cEnd = rQueries.end();
	for(TThreadRayVertexQueryVec::iterator vertexIter = rQueries.begin(); vertexIter != cEnd; ++vertexIter)
	{
		SThreadRayVertexQuery& rQuery = *vertexIter;
		TRayCacheVec& rRayInterreflectionCache = rIntersectionInfo[rQuery.vertexIndex];
		const bool cOnlyHemisphere = crConfigurator.ProcessOnlyUpperHemisphere(rQuery.cpMat->Type());
		rRayCaster.ResetCache(rQuery.pos, cRayLen, rQuery.normal, crParameters.rayTracingBias, cOnlyHemisphere, true/*use full vis cache*/);

		const TVec& crPos					= rQuery.pos;
		const TVec& crNormal			= rQuery.normal;
		const Vec2& crTexCoord		= rQuery.texCoord;
		SCoeffList_tpl<TScalarCoeff>& rCoeffListDirect				= *rQuery.pCoeffListDirect;
		const NSH::NMaterial::ISHMaterial& crMat = *rQuery.cpMat;	//cache material reference
		int& hemisphereSampleCount	= rQuery.hemisphereSampleCount;
		uint32& visQueries					= rQuery.visQueries;
		float& vis									= rQuery.vis;

		uint32 insertedRayCacheEntries = 0;//accessor for rRayInterreflectionCache
		//iterate samples
		const TSampleVec::const_iterator cEnd = pThreadParam->crSamples.end();
		for(TSampleVec::const_iterator sampleIter = pThreadParam->crSamples.begin(); sampleIter != cEnd; ++sampleIter)
		{
			SThreadRayResult result;

			const CSample_tpl<SCoeffList_tpl<TScalarCoeff> >& crSample = *sampleIter;
			const TCartesianCoord& rSampleCartCoord = crSample.GetCartesianPos();
 
			bool isOnUpperHemisphere = true;//indicates where sample lies
			if(rSampleCartCoord * rQuery.normal < gs_cSampleCosThreshold)
			{
				if(cOnlyHemisphere)
					continue;//consider only hemisphere
				isOnUpperHemisphere = false;			
			}

			//cache result values
			TRGBCoeffType& rIncidentIntensity = result.incidentIntensity;
			bool& rContinueLoop = result.continueLoop;
			bool& rApplyTransparency = result.applyTransparency;

			//reset intensity, loop indicator and transparency indicator
			rIncidentIntensity = TRGBCoeffType(1.,1.,1.);
			rContinueLoop = true;
			rApplyTransparency = false;

			bool firstElement = true;
			if(cHasTransparentMats && crParameters.supportTransparency)
			{
				const TCartesianCoord& crSampleDir = crSample.GetCartesianPos();
				if(rRayCaster.CastRay(rRayResultVec, rQuery.pos, crSampleDir, cRayLen, rQuery.normal, crParameters.rayTracingBias))
				{
					SRayResultData rayResultData(cHasTransparentMats, crSampleDir, isOnUpperHemisphere, rRayResultVec,	rIncidentIntensity, rContinueLoop, rApplyTransparency);
					crConfigurator.ProcessRayCastingResult(rayResultData);
					if(rayResultData.pRayRes)
					{ //record hit for interreflection and cache
						const SRayResult& rRayRes = *rayResultData.pRayRes;
						SRayCache& rRayCache		= tempRayCacheVec[insertedRayCacheEntries++];
						rRayCache.hSampleHandle	= crSample.GetHandle();
						assert(rRayRes.faceID < (2 << (sizeof(rRayCache.faceIndex) * 8 - 1)));
						rRayCache.faceIndex			= (uint16)rRayRes.faceID;
						rRayCache.meshIndex			= (uint16)GetIndexFromMesh(crMeshes, rRayRes.pMesh);
						assert((uint32)crMeshes.size() < (uint32)(2 << (sizeof(rRayCache.faceIndex) * 8 - 1)));
					}
					result.returnValue = true;
				}
				else
					result.returnValue = false;
			}
			else
			{
				SRayResult rayRes;
				if(rRayCaster.CastRay(rayRes, rQuery.pos, crSample.GetCartesianPos(), cRayLen, rQuery.normal, crParameters.rayTracingBias))
				{
					//record hit, used for subsequent interreflection passes
					result.returnValue = true;
				}
				else
					result.returnValue = false;
			}
			//now process result
			TRGBCoeffType incidentIntensity = result.incidentIntensity;
			if(isOnUpperHemisphere)
				hemisphereSampleCount++;//count for sample weighting(also important for indirect passes)
			visQueries++;
			if(result.returnValue)
			{
				if(result.continueLoop) 
				{
					if(cUseMinVisibility)
						crConfigurator.AddDirectScalarCoefficientValue(rCoeffListDirect, crParameters.minDirectBumpCoeffVisibility, crSample);
					continue;
				}
			}
			//ray fired toward ground, treat partly as blocked
			SRayProcessedData rayProcessData
			(
				crMat, crSample, crParameters, crPos, crNormal, crTexCoord, result.applyTransparency, isOnUpperHemisphere, false/*no interrefl*/,
				incidentIntensity, rCoeffListDirect, vis
			);
			crConfigurator.TransformRayCastingResult(rayProcessData);
		}//sample loop for vertex query
		rRayInterreflectionCache.resize(insertedRayCacheEntries);
		for(uint32 i=0; i<insertedRayCacheEntries; ++i)
			rRayInterreflectionCache[i] = tempRayCacheVec[i];
	}//vertex query
	return 0;
}

const bool NSH::NTransfer::CInterreflectionTransfer::PerformRayCasting
(
	CRayCaster& rRayCaster,
	const bool cHasTransparentMats,
	const CSample_tpl<SCoeffList_tpl<TScalarCoeff> >& crSample,
	const float cRayLen,						
	const TVec cPos,								
	const TVec cNormal,							
	const STransferParameters& crParameters,
	TRGBCoeffType& rIncidentIntensity,
	bool& rContinueLoop,					
	bool& rApplyTransparency,			
	bool& rCacheRecorded,					
	SRayCache& rRayInterreflectionCache,
	const TGeomVec& crMeshes, 
	const bool cIsOnUpperHemisphere,	
	const ITransferConfigurator& crConfigurator
)
{
	//reset intensity, loop indicator and transparency indicator
	rIncidentIntensity = TRGBCoeffType(1.,1.,1.);
	rContinueLoop = true;
	rApplyTransparency = false;
	rCacheRecorded = false;

	if(cHasTransparentMats && crParameters.supportTransparency)
	{
		const TCartesianCoord& crSampleDir = crSample.GetCartesianPos();
		if(rRayCaster.CastRay(m_Results, cPos, crSampleDir, cRayLen, cNormal, crParameters.rayTracingBias))
		{
			SRayResultData rayResultData(cHasTransparentMats, crSampleDir, cIsOnUpperHemisphere, m_Results,	rIncidentIntensity, rContinueLoop, rApplyTransparency);
			crConfigurator.ProcessRayCastingResult(rayResultData);
			if(rayResultData.pRayRes)
			{ //record hit for interreflection and cache
				const SRayResult& rRayRes = *rayResultData.pRayRes;
				rRayInterreflectionCache.hSampleHandle	= crSample.GetHandle();
				assert(rRayRes.faceID < (2 << (sizeof(rRayInterreflectionCache.faceIndex) * 8 - 1)));
				rRayInterreflectionCache.faceIndex			= (uint16)rRayRes.faceID;
				rRayInterreflectionCache.meshIndex			= (uint16)GetIndexFromMesh(crMeshes, rRayRes.pMesh);
				assert((uint32)crMeshes.size() < (uint32)(2 << (sizeof(rRayInterreflectionCache.faceIndex) * 8 - 1)));
				rCacheRecorded = true;
			}
			return true;
		}
		else
			return false;
	}
	else
	{
		SRayResult rayRes;
		//nothing to treat different here, ray is blocked or not
		if(rRayCaster.CastRay(rayRes, cPos, crSample.GetCartesianPos(), cRayLen, cNormal, crParameters.rayTracingBias))
		{
			//record hit, used for subsequent interreflection passes
			return true;
		}
		else
			return false;
	}
}

#endif