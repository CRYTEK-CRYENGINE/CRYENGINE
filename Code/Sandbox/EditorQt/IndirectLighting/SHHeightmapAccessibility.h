// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <PRT/SHFramework.h>
#include <CryThreading/IThreadManager.h>

class CTerrainObjectMan;
class CImageEx32;
class CHemisphereSink_SH;

//defines for debugging to enable features
#define DO_OBJECT_RAYCASTING  //enables object ray casting
#define DO_TERRAIN_RAYCASTING //enables terrain ray casting
#if defined(DO_OBJECT_RAYCASTING)
	#define DO_MP               //enables multiprocessing, does not make sense without per object rc
#endif

inline const Vec3 Vec3FromColour(const uint32 cCol)
{
	return Vec3((float)(cCol & 0x000000FF) / 255.f, (float)((cCol & 0x0000FF00) >> 8) / 255.f, (float)((cCol & 0x00FF0000) >> 16) / 255.f);
}

static const float gscDefaultVisCoeff0 = 3.54490509f;

//put this into a struct to save memory
//cannot use indexing since it not clear at init time which one is to be used but later on some data or already deallocated
struct SSHSampleOnDemand
{
	float shCoeffs[9];  //9 float coeffs for scalar spherical harmonics
};

struct SSHSample
{
	//managed from outside in one contiguous block, bad design but i do not want to pollute other users of HeightmapAccessibility classes
	SSHSampleOnDemand* pSHData;
	float              posXFloat; //x float position for object sample (needs to be more correct than just integer grid)
	float              posZ;//world space z position
	union
	{
		struct
		{
			uint16 posX;
			uint16 posY;
		};
		float posYFloat;  //y float position for object sample (needs to be more correct than just integer grid)
	};
	uint32 linkID;//link into object sample vector
	uint16 colCount;//number of calls to aveCol, max is wedges * wedges, 0xFFFF means set vis to 0

	void   SetSampleLink(const uint32 cIndex = 0xFFFFFFFF)
	{
		if (cIndex != 0xFFFFFFFF)
			flags |= scHasLinkedObject;
		else
			flags &= ~scHasLinkedObject;
		linkID = cIndex;
	}

	const bool HasObjectLink() const
	{
		return (linkID != 0xFFFFFFFF && ((flags & scHasLinkedObject) != 0));
	}

	const bool IsToBeIgnored() const
	{
		return (flags & scToBeIgnored) != 0;
	}

	void SetToBeIgnored(const bool cSet = true)
	{
		if (cSet)
			flags |= scToBeIgnored;
		else
			flags &= ~scToBeIgnored;
	}

	const bool IsObstructed() const
	{
		return (flags & scObstructed) != 0;
	}

	void SetObstructed(const bool cSet = true)
	{
		if (cSet)
			flags |= scObstructed;
		else
			flags &= ~scObstructed;
	}

	const bool DoRayCasting() const
	{
		return (flags & scNoRayCasting) == 0;
	}

	void SetDoRayCasting(const bool cSet = false)
	{
		flags &= ~scNoRayCasting;
		flags |= cSet ? 0 : scNoRayCasting;
	}

	void SetMapped(const bool cSet = true)
	{
		flags = cSet ? (flags | scMapped) : (flags & ~scMapped);
	}

	const bool IsMapped() const
	{
		return ((flags & scMapped) != 0);
	}

	void SetIsAlreadyDisplaced(const bool cSet = true)
	{
		flags = cSet ? (flags | scAlreadyDisplaced) : (flags & ~scAlreadyDisplaced);
	}

	const bool IsAlreadyDisplaced() const
	{
		return ((flags & scAlreadyDisplaced) != 0);
	}

	void SetIsOffseted(const bool cSet = true)
	{
		flags = cSet ? (flags | scOffseted) : (flags & ~scOffseted);
	}

	const bool IsOffseted() const
	{
		return ((flags & scOffseted) != 0);
	}

	void InitFlags()
	{
		flags = 0;
	}

private:
	uint8              flags;//flags of ray casting
	static const uint8 scObstructed = 0x1;
	static const uint8 scNoRayCasting = 0x2;
	static const uint8 scToBeIgnored = 0x4;
	static const uint8 scHasLinkedObject = 0x8;
	static const uint8 scMapped = 0x10;
	static const uint8 scAlreadyDisplaced = 0x40;
	static const uint8 scOffseted = 0x80;
};

//converts heightmap accessibility into a scalar spherical harmonic
class CHemisphereSink_SH
{
public:

	static const float  cA1;
	static const float  cA2;
	static const uint32 scMinColCount = 32;

	typedef SSHSample SampleType;//scalar float SH as sample type

	//contains all, sh coefficient samples for this wedge angle
	struct SWedgeSHSamples
	{
		float coeffs[9];  //9 float scalar spherical harmonics coeffs for this polar angle
		float dirZ;       //unit sample direction z
#if defined(DO_TERRAIN_RAYCASTING)
		float altitude; //altitude this corresponds to
#endif
	};

	struct SFloatCoeffs
	{
		float coeffs[9];  //9 float scalar spherical harmonics coeffs for this polar angle

		void  Init()
		{
			for (int i = 0; i < 9; ++i)
				coeffs[i] = 0.f;
		}

		void operator+=(const SWedgeSHSamples& crWedgeSample)
		{
			for (int i = 0; i < 9; ++i)
				coeffs[i] += crWedgeSample.coeffs[i];
		}
	};

	//one wedge vector contains all samples above horizon quantized according to polar angle
	typedef std::vector<SWedgeSHSamples>                   TSingleWedgeSampleVec;
	typedef std::pair<SFloatCoeffs, TSingleWedgeSampleVec> TSingleWedgeSampleVecPair;
	//vector containing all quantized wedges plus coeffs of lower hemisphere
	typedef std::vector<TSingleWedgeSampleVecPair>         TWedgeSampleVec;
	typedef std::vector<TSingleWedgeSampleVec>             TWedgeSampleVecMPSingle;
	typedef std::vector<TWedgeSampleVecMPSingle>           TWedgeSampleVecMP;

#if defined(DO_MP)
	//containes data required for each ray casting thread
	struct SMPData : public IThread
	{
		//core routine for MP
		virtual void ThreadEntry();

		TSingleWedgeSampleVec* pWedges;        //all wedges to process for thread
		CTerrainObjectMan*     pTerrainObjMan; //ptr to terrain man

		SFloatCoeffs           shCoeffs;    //all accumulated coeffs
		uint32                 colCount;    //number of added colours

		float                  wedgeHorizonAngle; //const wedge horizon at this sample

		HANDLE                 threadID;    //thread handle
		HANDLE                 eventID;     //event handle (to signal completion to main thread)
		HANDLE                 eventLoopID; //event handle (to control flow of thread, will suspend itself)

		SMPData() : pWedges(NULL), pTerrainObjMan(NULL)       //ctor
		{
			Reset();
		}
		void Reset()
		{
			shCoeffs.Init();
			colCount = 0;
		}   //reset coeffs
	};
	typedef std::vector<SMPData> TMPDataVec;
#endif

	static const float scRayLength;                  //length of ray
	static const float scObstructedColourMultiplier; //colour multiplier for reflection colour with respect to ground (terrain) colour

	// ---------------------------------------------------------------------------------

	//! constructor
	//! \param indwAngleSteps [1..[
	CHemisphereSink_SH(const DWORD indwAngleSteps, const DWORD indwWidth, const DWORD indwHeight);

	void Init(CTerrainObjectMan* cpTerrainObjMan, float* pHeightMap, const float cWaterLevel, const float cHeightScale, const uint32 cWorldSpaceMul);

	void InitSample(SampleType& rInoutValue, const uint32 cX, const uint32 cY) const;

	//! \param infSlope [0..[
	//! \param rInoutValue result is added to this value
	void       AddWedgeArea(const float cCurWedgeAngle, const float infSlope, SampleType& rInoutValue, const float cSlopeDist);

	const bool NeedsPostProcessing() const
	{
		return true;
	}

	//called when calculation has been finished to post process data if required
	void OnCalcEnd(SampleType& rInoutValue);

	static const float scSampleHeightOffset;//offset for each sample in z

	std::vector<SampleType>::iterator GetAddSampleIterBegin()
	{
		return m_AddSamples.begin();
	}

	std::vector<SampleType>::const_iterator GetAddSampleIterBegin() const
	{
		return m_AddSamples.begin();
	}

	const size_t GetAddSampleCount() const
	{
		return m_AddSamples.size();
	}

	const std::vector<SampleType>::const_iterator GetAddSampleIterEnd() const
	{
		return m_AddSamples.end();
	}

	SampleType* RetrieveSample(const size_t cIndex)
	{
		assert(cIndex < m_AddSamples.size());
		return &m_AddSamples[cIndex];
	}

	void AddSample
	(
	  SampleType& rSampleToLinkFrom,
	  const Vec3& crPos,
	  const bool cApplyRayCasting = true,
	  const bool cIsFullyObstructed = false
	);

	void SignalCalcEnd()
	{
#if defined(MP)
		if (m_DoMP)//stop threads
			for (int i = 0; i < m_CoreCount; ++i)
			{
				SMPData& rMPData = m_MPData[i];
				(intptr_t)rMPData.pWedges = -1;//will make thread to abort
				SetEvent(rMPData.eventLoopID);
			}
#endif
	}

	void FreeWedgeSampleVec()
	{
		m_SHSamples.swap(TWedgeSampleVec());
	}

	//WORLD_HMAP_COORDINATE_EXCHANGE
	const Vec3 GridToWorldPos(Vec3& crPos) const
	{
		return Vec3(crPos.y * (float)m_WorldSpaceMul, crPos.x * (float)m_WorldSpaceMul, crPos.z);
	}

	//WORLD_HMAP_COORDINATE_EXCHANGE
	void WorldToGridPos(uint16& rX, uint16& rY, const float cX, const float cY) const
	{
		rX = cY / (float)m_WorldSpaceMul;
		rY = cX / (float)m_WorldSpaceMul;
	}

	const uint32 GetGridResolution() const
	{
		return m_Width;
	}

	const uint32 GetWorldSampleDist() const
	{
		return m_WorldSpaceMul;
	}

	const uint32 GetAngleStepCount() const
	{
		return m_AngleSteps;
	}

	const uint32 GetSampleCount() const
	{
		return m_SampleCount;
	}

	const uint32 GetThreadCount() const
	{
#if defined(DO_MP)
		return m_CoreCount;
#else
		return 1;
#endif
	}

	void TransformCoeffs(float* pCoeffs) const
	{
		assert(pCoeffs);
		NSH::SCoeffList_tpl<NSH::TScalarCoeff> coeffs;
		NSH::SCoeffList_tpl<NSH::TScalarCoeff> coeffsDest;
		for (int i = 0; i < 9; ++i)
			coeffs[i] = pCoeffs[i];
		m_SHRotMatrix.Transform(coeffs, coeffsDest);
		for (int i = 0; i < 9; ++i)
			pCoeffs[i] = coeffsDest[i];
	}

protected: // ---------------------------------------------------------------------------------

	NSH::CRotateMatrix_tpl<float, false> m_SHRotMatrix;//needs for post processing rotation
	TWedgeSampleVec                      m_SHSamples;//all sample directions and precomputed SH coefficients, 1 entry for each wedge
	std::vector<SampleType>              m_AddSamples;//samples for the objects, linked from the global sample vector

	bool              m_DoMP;        //true if to do multiprocessing, common to non MP code to save preproc. stuff
	TWedgeSampleVecMP m_SHSamplesMP; //sample vec for each multicore (if m_CoreCount > 1)
#if defined(DO_MP)
	uint32            m_CoreCount;    //number of multi processing cores
	TMPDataVec        m_MPData;       //mp data vec
#endif

	CTerrainObjectMan* m_pTerrainObjMan;  //terrain object manager
	float*             m_pHeightMap;//heightmap
	float              m_WaterLevel;//cached water level
	float              m_HeightScale;//heightscale
	uint32             m_WorldSpaceMul;//granularity of grid points, m_Width * m_WorldSpaceMul yields to world space terrain extension

	uint32             m_AngleSteps; //number of wedge angle steps
	uint32             m_Width;//size of grid texture, every pixel represents one grid point
	uint32             m_SampleCount;//sample count for upper hemisphere

	std::vector<Vec2>  m_AngleDirections; //precalc the directions according to angle index, m_AngleSteps entries

	//inner ray casting loop non mp
	void PerformRayCasting
	(
	  const TSingleWedgeSampleVecPair& crWedgeSamples,
	  const bool cDoRayCasting,
	  SampleType& rInoutValue,
	  const float cWedgeHorizonAngle
	) const;

	//inner ray casting loop for mp
	void PerformRayCastingMP
	(
	  const TSingleWedgeSampleVecPair& crWedgeSamples,
	  TWedgeSampleVecMPSingle& rWedgeSamplesMP,
	  SampleType& rInoutValue,
	  const float cWedgeHorizonAngle
	);
}; // CHemisphereSink_SH

