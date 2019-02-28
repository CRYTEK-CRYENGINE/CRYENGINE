// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __terraingigen_h__
#define __terraingigen_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "Quadtree/Quadtree.h"
#include <PRT/SHFramework.h>
#include <Cry3DEngine/I3DEngine.h>
#include "Raster.h"
#include "SHHeightmapAccessibility.h"
#include "TerrainObjectMan.h"
#include "RasterMPMan.h"

// forward declaration.
class CTerrainObjectMan;
struct STempBBoxData;
class CTerrainGrid;
template<class THemisphereSink>
class CHeightmapAccessibility;
struct SSHSampleOnDemand;
struct SSHSample;

template<class T>
struct SIndirectGridPoint;

struct SHeightmapTriangleValidator;

/** Class that generates indirect lighting information
 */
class CTerrainGIGen
{
public:
	// Description:
	//		contains all quality properties for indirect lighting computation
	struct SConfigProp
	{
		float colourThreshold;      //threshold at which sum the grid point gets removed in the optimization stage
		float shThreshold;          //threshold at which percentage of diff a sh sample gets removed
		uint8 baseGranularity;      //base granularity for samples in meters
		uint8 wedges;               //wedges the sphere is subdivided into
		uint8 brushResMultiplier;   //resolution multiplier for brushes (4 means 4x resolution of samples around brushes), must be power of 2
		uint8 vegResMultiplier;     //resolution multiplier for vegetation, must be power of 2
		SConfigProp()
			: colourThreshold(0.2f), shThreshold(0.25f), baseGranularity(16), wedges(32), brushResMultiplier(4), vegResMultiplier(4)
		{}
	};

	// -----------------------------------------------------------------------
	// Description:
	//		default constructor
	CTerrainGIGen();
	// Description:
	//		destructor
	~CTerrainGIGen();
	// Description:
	//		generates the quadtree
	const bool Generate
	(
	  CPakFile& rLevelPakFile,
	  uint8*& rpTerrainAccMap,
	  uint32& rTerrainAccMapRes,
	  const uint32 cApplySS,
	  const SConfigProp& crConfigProps = SConfigProp()
	);//cGranularity is grid res in meters and must be power of 2

	typedef NSH::SScalarCoeff_tpl<float>          TSingleCoeffType;
	typedef NSH::SCoeffList_tpl<TSingleCoeffType> TCoeffType;

	static const float  scMinBBoxBrushHeight;        //minimum brush height to consider for ray casting
	static const float  scMinBBoxVegHeight;          //minimum vegetation height to be considered at all
	static const float  scMaxDistToGroundRC;         //maximum dist to ground to perform ray casting for object sample
	static const float  scMaxObjectSampleExtension;  //restrict axis extension for object samples
	static const float  scMinLargeEntityXYExtension; //min xy-axis extension to consider entity as occluder
	static const float  scMaxLargeEntityXYExtension; //max xy-axis extension to consider entity as occluder
	static const float  scMaxDistToGroundNoRayCast;  //maximum dist to ground to consider at all
	static const float  scMaxObjExt;                 //maximum obj ext to consider (alien ship f.i. too large)

	static const uint32 scValid = 0x1;              //flags validity of grid point (not optimized away or something)
	static const uint32 scOptimizeable = 0x2;       //flags that grid point can be optimized
	static const uint32 scUnUsed = 0x4;             //force to set it as UnUsed
	static const uint32 scLowResShift = 5;          //32x32 units for each low res texel

private: // ---------------------------------------------------------------------

	// Description:
	//		editor representation of a grid point
	template<class TPosType>
	struct SEditorGridPoint
	{
		TCoeffType  shCoeffs;   // original non compressed scalar sh coeffs

		const uint8 GetFlags() const            { return flags; }
		void        SetFlag(const uint32 cFlag) { flags = cFlag; }
		void        UnSetFlag(const uint32 cFlag)
		{
			flags &= ~cFlag;
		}

		SEditorGridPoint() : x(0), y(0), shCoeffs(0), flags(0){}

		TPosType x, y;           // x/y position of grid point
		uint32   flags;
	};

	typedef uint32                                                                                                                                          TIndexType; //32 bit sample index (we will have more than 32767)
	typedef NQT::CQuadTree<uint32 /*index into SH table*/, 8 /*4*//*max 4 elems per leaf cell*/, uint16 /*pos type*/, TIndexType /*big index type*/, false> TQuadTree;
	typedef SEditorGridPoint<int16>                                                                                                                         TEditorGridPoint;
	typedef std::vector<TEditorGridPoint>                                                                                                                   TGridVec;
	typedef TQuadTree::TVec2F                                                                                                                               TVec2F;
	typedef TQuadTree::TVec2                                                                                                                                TVec2;

	// Description:
	//		contains the bounding box data and center height for object the ray casting
	//		used for sample refinement placement
	struct SObjectBBoxes
	{
		TVec2F              minInner;         //min extension of the bounding boxes on the ground
		TVec2F              maxInner;         //max extension of the bounding boxes on the ground
		TVec2F              minOuter;         //min extension of the bounding box for the whole object
		TVec2F              maxOuter;         //max extension of the bounding box for the whole object
		Vec3                pos;              //object pos, z is taken from the center rather from the object translation
		float               centerZ;          //translation z (z of the object translation, used to compute average vis across object)
		float               distToGround;     //distance to ground
		float               maxAxisExtension; //max axis extension
		float               zExt;             //z axis extension
		float               maxZ;             //upper z (on top of object, used to compute average vis across object)
		uint32              terrainObjManId;  //corresponding object id in terrain object manager
		uint32              flags;            //flags

		static const uint32 scForceDefaultVis = 0x1;      //force default vis and no vis progression
		static const uint32 scNoObjectSample = 0x2;       //add no object sample for this object
		static const uint32 scForceDefaultVisTop = 0x4;   //set vis progression sample to be fully vis on top

		SObjectBBoxes() :
			minInner(-1, -1), maxInner(-1, -1), minOuter(-1, -1), maxOuter(-1, -1),
			pos(-1.f, -1.f, -1.f), centerZ(-1.f), distToGround(-1.f), maxAxisExtension(-1.f), maxZ(-1.f), terrainObjManId(0xFFFFFFFF),
			flags(0), zExt(0.f)
		{}
	};

	//enum describing type of objects
	enum EObjectType
	{
		eOT_Brush  = 0,
		eOT_Entity = 1,
		eOT_Veg    = 2,
		eOT_Voxel  = 3
	};

	typedef std::vector<SObjectBBoxes>                  TObjectVec; //typedef for object bounding boxes in xy
	typedef CHeightmapAccessibility<CHemisphereSink_SH> THeightmapAccess;//typedef for heightmap accessibility and ray cast manager

	//---------------------------data elements---------------------------

	TGridVec                        m_GridPoints;   //grid points, boolean indicates validity (removal sets it to false)
	uint32                          m_GridMaxCount; //max count for grid points
	TQuadTree                       m_QuadTree;     //whole terrain covering quad tree (later per 1x1 km cell), is in heightmap space
	SConfigProp                     m_Config;       //configuration
	CHeightmap*                     m_pHeightMap;   //pointer to editors heightmap
	float                           m_UnitSize;     //m_pHeightMap->GetUnitSize()

	CRasterMPMan                    m_RasterMPMan;  //bboxraster mp manager instance

	std::vector<TVec2>              m_AdditionalSamples;    //sample positions around objects
	std::vector<SSHSampleOnDemand*> m_ContiguousSampleData; //vector of memory for sample data for heightmap calc

	//these object vectors are built in order
	TObjectVec m_ObjectBBoxesXYBrushes;             //object vector for brushes
	TObjectVec m_ObjectBBoxesXYLargeEntities;       //object vector for large entities (which are no vehicles)
	TObjectVec m_ObjectBBoxesXYVegetation;          //object vector for vegetation
	TObjectVec m_ObjectBBoxesXYVoxels;              //object vector for voxels to add samples around

	uint8*     m_pTempBlurArea;                     //allocated memory for terrain blur passes, to be preallocated to not run out of memory at the end
	uint8*     m_pSmallHeightMapAccMap;             //allocated memory for terrain acc.map down sampling

	//1 byte vector containing the information if a texel needs ray casting for vis calculation (heightmap space)
	std::vector<uint8> m_TerrainMapMarks;
	unsigned int       m_TerrainMapRes;             //corresponding resolution to m_TerrainMapMarks

	//sector data
	int32                     m_SectorRowNum;       //number of sectors per row
	static const uint8        scSectorUnused = 0;
	static const uint8        scSectorUsed = 1;
	static const uint8        scSectorUsedByVeg = 2;
	static const uint8        scSectorUsedByNonVeg = 4;
	std::vector<uint8>        m_SectorTable;        //sector table, 1 if to be used, 0 if not

	std::vector<CBBoxRaster*> m_BBoxRasterPtrs;     //tracks the raster allocations to delete them

	//data for parallelization of bbox rasterization
	std::vector<std::vector<SMatChunk>> m_ChunkVecs[2];
	uint32                              m_ActiveChunk; //active index

	bool                                m_FirstTime; //true if it runs for the first time

	//---------------------------private member functions---------------------------

	//cWidth is a size of a texture where every pixel represents one grid point
	const bool PrepareHeightmap(const uint32 cWidth, CFloatImage& rFloatImage, float& rHeightScale) const;  //retrieves scaled heightmap with scale for accessibility
	// Description:
	//		adds outdoor brushes and vegetation objects to the object manager for the ray casting
	void PrepareObjects
	(
	  const float cWaterLevel,
	  CTerrainObjectMan& rTerrainMan,
	  const float* cpHeightMap,
	  const uint32 cWidthUnits,
	  const uint32 cHeigthMapRes
	);
	// Description:
	//		prepares brush, called from PrepareObjects
	//		returns true if it was processed
	const bool PrepareBrush
	(
	  CBaseObject* pObj,
	  const float cWaterLevel,
	  CTerrainObjectMan& rTerrainMan,
	  const float* cpHeightMap,
	  const uint32 cWidthUnits,
	  const uint32 cHeigthMapRes,
	  const float cWorldHMapScale,
	  AABB& rBoxObject
	);

	// Description:
	//		prepares entity, called from PrepareObjects
	//		returns true if it was processed
	const bool PrepareEntity
	(
	  CBaseObject* pObj,
	  const float cWaterLevel,
	  CTerrainObjectMan& rTerrainMan,
	  const float* cpHeightMap,
	  const uint32 cWidthUnits,
	  const uint32 cHeigthMapRes,
	  const float cWorldHMapScale,
	  const bool cIsPrefab
	);

	// Description:
	//		adds more samples around bounding boxes of the objects/vegetation
	//		m_Config controls the resolution in which this is done
	//		cIsBrush if true then object size need to be tested for adding to object samples (basically all vegetation is added whereas brushes not)
	void AddSamplesAroundObjects
	(
	  CTerrainObjectMan& rTerrainObjMan,
	  THeightmapAccess& rHMapCalc,
	  TObjectVec& rObjectVec,
	  const uint8 cResMultiplier,
	  const float* cpHeightMap,
	  const uint8 cMaxMultiplier,
	  const EObjectType cObjectType = eOT_Veg
	);
	// Description:
	//		retrieves the interpolated value from up to 4 quadtree nodes (which are the grid points)
	//		returns true if at least one value was provided for interpolation (from rNodes)
	const bool InterpolateGridPoint
	(
	  int pClosestNodes[2],
	  const uint32 cLeafCount,
	  const TQuadTree::TInitialInterpolRetrievalType cpNodes,
	  TCoeffType& rInterpolatedCoeffs
	) const;
	// Description:
	//		generates a material vector for each static (sub)object
	//		returns true if a sh flag has been found somewhere
	void GenMaterialVec
	(
	  IStatObj* pStatObj,
	  IRenderNode* pRenderNode,
	  std::vector<std::vector<int>>& rMats, //each sub object contains sub materials which are indices
	  std::vector<ColorF>& rMaterials,      //the real colours
	  const ColorF cDefMat = ColorF(0.5f, 0.5f, 0.5f, 1.f)
	);
	// Description:
	//		returns the color for an object, multiplies average texture color and diffuse color together
	//		also fills a vector of the average alpha value for this material (needed for bounding box adaption)
	//		cFillVec needed for internal control of setting up rMatAlphas
	void GetMaterialColor(IMaterial* pMat, std::vector<ColorF>& rSubMats, ColorF& rLastCol, const bool cFillVec) const;
	// Description:
	//		sets the grid values on the large grid from the heightmap calculator
	void SetCoarseGridValues(const uint32 cWidthUnits, const uint32 cHighResWidth, const THeightmapAccess& crCalc);
	// Description:
	//		adds the coarse grid points to the quadtree
	void AddCoarseGridPoints
	(
	  const float cWaterLevel,
	  const uint32 cGridWidth,
	  const uint32 cHighResWidth,
	  const float* const cpHeightMap,
	  const CTerrainObjectMan& crTerrainMan
	);
	// Description:
	//		processes the mesh
	//		returns true if object is on ground
	const bool ProcessMesh
	(
	  IStatObj* pStatObj,
	  const AABB& crBBox,
	  const Matrix34& crWorldMat,
	  const std::vector<std::vector<int>>& crMatIndices,
	  const std::vector<ColorF>& crMatColours,
	  std::vector<STempBBoxData>& rBBoxVec,
	  const bool cIsVegetation,
	  const float* cpHeightMap,
	  const float cWorldHMapScale,
	  const uint32 cHeigthMapRes,
	  SObjectBBoxes& rObjectBox,
	  const bool cCheckAgainstHeightMap,
	  AABB& rMinMaxCalculated,
	  IRenderMesh* pReplacementMesh = NULL
	);
	// Description:
	//		returns object center distance to ground
	const float GetDistToGround(const float* cpHeightMap, const AABB& crBBox, const float cWorldHMapScale, const uint32 cHeigthMapRes) const;
	// Description:
	//		marks the high res samples required for computation, optimization to not calculate unnecessary samples
	//		marks samples being outside any object range (to save ray casting) and samples not needed in high res
	void MarkSamples
	(
	  CTerrainObjectMan& rTerrainObjMan,
	  const uint32 cRes,
	  THeightmapAccess& rCalc,
	  const uint8 cMaxMultiplier,
	  const float* cpHeightMap,
	  const uint32 cWidthUnits
	);
	// Description:
	//		sets the data for the samples around the objects and due to colour diffs, uses m_AdditionalSamples
	void SetDataForAdditionalSamples(const THeightmapAccess& crHMapCalc);
	// Description:
	//		generates the samples for the additional samples (samples around objects, per object samples)
	void GenerateAddSampleVec
	(
	  const CTerrainObjectMan& crTerrainObjMan,
	  const CHemisphereSink_SH& crCalcSink
	);
	// Description:
	//		calculates the normal at the heightmap pos
	const Vec3 ComputeHeightmapNormal
	(
	  const uint16 cHPosX, const uint16 cHPosY,
	  const float* const cpHeightMap,
	  const uint32 cHighResWidth
	) const;
	// Description:
	//		goes through all samples and allocates data for sh coeffs for the samples we really need
	//		also calculates better positions for obstructed samples
	void OnBeforeProcessing(const uint32 cFullWidth, CTerrainObjectMan& rTerrainObjMan, const uint32 cHighResWidth, THeightmapAccess& rCalc);
	// Description:
	//		deallocates the contiguous sample data block
	void DeAllocateContiguousSampleData();
	// Description:
	//		retrieves the average alpha of all materials
	const float GetAverageMaterialAlpha(const std::vector<ColorF>& crSubMat) const;
	// Description:
	//		adapts samples which are obstructed to a different position
	void AdaptObstructedSamplePos
	(
	  const uint32 cFullWidth,
	  const CTerrainObjectMan& crTerrainObjMan,
	  const THeightmapAccess& crCalc,
	  SSHSample& rSample,
	  SSHSample& rSampleLeft,
	  SSHSample& rSampleRight,
	  SSHSample& rSampleUpper,
	  SSHSample& rSampleLower
	);
	// Description:
	//		adds terrain voxels as shadow caster
	void AddTerrainVoxels
	(
	  const float cWaterLevel,
	  CTerrainObjectMan& rTerrainMan,
	  const float* cpHeightMap,
	  const uint32 cHeigthMapRes,
	  const float cWorldHMapScale
	);
	// Description:
	//		builds the sector table which indicates which sectors are of any interests
	void BuildSectorTable(const uint32 cWidthUnits, const CTerrainObjectMan& crTerrainObjMan);
	// Description:
	//		retrieves if sample is a sector to be used
	const bool IsSectorUsed(const uint16 cX, const uint16 cY) const;
	// Description:
	//		retrieves if pos is in a sector together with non vegetation (entities or brushes)
	const bool IsSectorUsedByNonVeg(const uint16 cX, const uint16 cY) const;
	// Description:
	//		sets the information what kind of object is present
	void SetSectorUsed(const uint16 cX, const uint16 cY, const uint8 cVal);
	// Description:
	//		calculates the sky accessibility for the terrain in terms of objects
	void CalcTerrainOcclusion
	(
	  CPakFile& rLevelPakFile,
	  const uint32 cRGBImageRes,
	  const uint64 cWidthUnits,
	  CTerrainObjectMan& rTerrainMan,
	  const float* const cpHeightMap,
	  const CHemisphereSink_SH& crCalcSink,
	  uint8*& rpTerrainAccMap,
	  uint32& rTerrainAccMapRes,
	  const uint32 cApplySuperSampling,
	  uint8* pHeightMapAccMap
	);
	// Description:
	//		blurs the terrain access. map
	void BlurTerrainAccMap(uint8* pAccMap, const uint32 cWidth, const uint8 cMinValue = 0);
	// Description:
	//		retrieves interpolated height
	const float GetFilteredHeight
	(
	  const uint16 cXPos,
	  const uint16 cYPos,
	  const uint32 cRGBImageRes,
	  const uint32 cXYRes,
	  const float* const cpHeightMap,
	  const uint32 cRGBToXYResShift
	) const;
	// Description:
	//		post processes a sample (weighting, lookup...)
	void PostProcessSample
	(
	  const uint8 cScale,
	  const TCoeffType& crSHCoeffs,
	  uint8* pHeightMapAccMap,
	  const uint32 cWidth,
	  const uint32 cX,
	  const uint32 cY,
	  const NSH::TSample& crLookupVec
	);
	// Description:
	//		retrieves height at a specific position from the full resolution heightmap
	const float GetHeightFromWorldPosFullRes(const Vec3& crWorldPos) const;
	// Description:
	//		returns true if at least one endpoint of the triangle (in world space coords) is above the terrain (with some threshold)
	const bool IsTriangleAboveTerrain(const Vec3& crWorldA, const Vec3& crWorldB, const Vec3& crWorldC) const;

	friend struct SHeightmapTriangleValidator;
};

inline const bool CTerrainGIGen::IsSectorUsed(const uint16 cX, const uint16 cY) const
{
	size_t nSectorIndex(
	  (cX >> CObstructionAccessManager::scSectorSizeShift) * m_SectorRowNum +
	  (cY >> CObstructionAccessManager::scSectorSizeShift)
	  );

	if (nSectorIndex < m_SectorTable.size())
	{
		return (m_SectorTable[nSectorIndex] != scSectorUnused);
	}
	else
	{
		return false;
	}
}

inline const bool CTerrainGIGen::IsSectorUsedByNonVeg(const uint16 cX, const uint16 cY) const
{
	size_t nSectorIndex(
	  (cX >> CObstructionAccessManager::scSectorSizeShift) * m_SectorRowNum +
	  (cY >> CObstructionAccessManager::scSectorSizeShift)
	  );

	if (nSectorIndex < m_SectorTable.size())
	{
		return ((m_SectorTable[nSectorIndex] & scSectorUsedByNonVeg) != 0);
	}
	else
	{
		return false;
	}
}

inline void CTerrainGIGen::SetSectorUsed(const uint16 cX, const uint16 cY, const uint8 cVal)
{
	size_t nSectorIndex(
	  (cX >> CObstructionAccessManager::scSectorSizeShift) * m_SectorRowNum +
	  (cY >> CObstructionAccessManager::scSectorSizeShift)
	  );
	if (nSectorIndex < m_SectorTable.size())
	{
		m_SectorTable[nSectorIndex] |= cVal;
	}
}

inline void CTerrainGIGen::DeAllocateContiguousSampleData()
{
	for (int i = 0; i < m_ContiguousSampleData.size(); ++i)
		delete[] m_ContiguousSampleData[i];
	m_ContiguousSampleData.clear();
}

inline CTerrainGIGen::CTerrainGIGen() :
	m_FirstTime(true), m_pSmallHeightMapAccMap(NULL), m_pTempBlurArea(NULL),
	m_QuadTree(4 * 1024 * 1024, 16 * 1024 * 1024), m_ActiveChunk(0)
{}

//////////////////////////////////////////////////////////////////////////
inline CTerrainGIGen::~CTerrainGIGen()
{}

inline const bool CTerrainGIGen::IsTriangleAboveTerrain(const Vec3& crWorldA, const Vec3& crWorldB, const Vec3& crWorldC) const
{
	static const float scHeightThreshold = 0.5f;

	//do only consider if above heightmap (voxels replace terrain in its sector)
	if (crWorldA.z <= scHeightThreshold + GetHeightFromWorldPosFullRes(crWorldA) &&
	    crWorldB.z <= scHeightThreshold + GetHeightFromWorldPosFullRes(crWorldB) &&
	    crWorldC.z <= scHeightThreshold + GetHeightFromWorldPosFullRes(crWorldC))
		return false;
	return true;
}

struct SHeightmapTriangleValidator : public ITriangleValidator
{
	const CTerrainGIGen& m_crTerrainGen;

	SHeightmapTriangleValidator(const CTerrainGIGen& crTerrainGen) : m_crTerrainGen(crTerrainGen){}

	virtual const bool ValidateTriangle(const Vec3 crA, const Vec3 crB, const Vec3 crC) const
	{
		return m_crTerrainGen.IsTriangleAboveTerrain(crA, crB, crC);
	}
};

#endif // __terraingigen_h__

