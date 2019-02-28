// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   statobj.h
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef STAT_OBJ_H
#define STAT_OBJ_H

#if CRY_PLATFORM_DESKTOP
	#define TRACE_CGF_LEAKS
#endif

class CIndexedMesh;
class CRenderObject;
class CContentCGF;
struct CNodeCGF;
struct CMaterialCGF;
struct phys_geometry;
struct IIndexedMesh;
struct IParticleEffect;

#include "../Cry3DEngine/Cry3DEngineBase.h"
#include <CryCore/Containers/CryArray.h>

#include <Cry3DEngine/IStatObj.h>
#include <CrySystem/IStreamEngine.h>
#include "RenderMeshUtils.h"
#include <CryMath/GeomQuery.h>

#define MAX_PHYS_GEOMS_TYPES 4

struct SDeformableMeshData
{
	IGeometry*    pInternalGeom;
	int*          pVtxMap;
	unsigned int* pUsedVtx;
	int*          pVtxTri;
	int*          pVtxTriBuf;
	float*        prVtxValency;
	Vec3*         pPrevVtx;
	float         kViscosity;
};

struct SSpine
{
	~SSpine() { delete[] pVtx; delete[] pVtxCur; delete[] pSegDim; }
	SSpine() { pVtx = 0; pVtxCur = 0; pSegDim = 0; bActive = false; nVtx = 0; len = 0; navg = Vec3(0, 0, 0); idmat = 0; iAttachSpine = 0; iAttachSeg = 0; }

	bool  bActive;
	Vec3* pVtx;
	Vec3* pVtxCur;
	Vec4* pSegDim;
	int   nVtx;
	float len;
	Vec3  navg;
	int   idmat;
	int   iAttachSpine;
	int   iAttachSeg;
};

class CStatObjFoliage : public IFoliage, public Cry3DEngineBase
{
public:
	CStatObjFoliage()
	{
		m_next = 0;
		m_prev = 0;
		m_lifeTime = 0;
		m_ppThis = 0;
		m_pStatObj = 0;
		m_pRopes = 0;
		m_pRopesActiveTime = 0;
		m_nRopes = 0;
		m_nRefCount = 1;
		m_timeIdle = 0;
		m_pVegInst = 0;
		m_pTrunk = 0;
		m_pSkinningTransformations[0] = 0;
		m_pSkinningTransformations[1] = 0;
		m_iActivationSource = 0;
		m_flags = 0;
		m_bGeomRemoved = 0;
		m_bEnabled = 1;
		m_timeInvisible = 0;
		m_bDelete = 0;
		m_pRenderObject = 0;
		m_minEnergy = 0.0f;
		m_stiffness = 0.0f;
		arrSkinningRendererData[0].pSkinningData = NULL;
		arrSkinningRendererData[0].nFrameID = 0;
		arrSkinningRendererData[1].pSkinningData = NULL;
		arrSkinningRendererData[1].nFrameID = 0;
		arrSkinningRendererData[2].pSkinningData = NULL;
		arrSkinningRendererData[2].nFrameID = 0;
	}
	~CStatObjFoliage();
	virtual void             AddRef()  { m_nRefCount++; }
	virtual void             Release() { if (--m_nRefCount <= 0) m_bDelete = 2; }

	virtual int              Serialize(TSerialize ser);
	virtual void             SetFlags(int flags);
	virtual int              GetFlags()                    { return m_flags; }
	virtual IRenderNode*     GetIRenderNode()              { return m_pVegInst; }
	virtual int              GetBranchCount()              { return m_nRopes; }
	virtual IPhysicalEntity* GetBranchPhysics(int iBranch) { return (unsigned int)iBranch < (unsigned int)m_nRopes ? m_pRopes[iBranch] : 0; }

	virtual SSkinningData*   GetSkinningData(const Matrix34& RenderMat34, const SRenderingPassInfo& passInfo);

	uint32                   ComputeSkinningTransformationsCount();
	void                     ComputeSkinningTransformations(uint32 nList);

	void                     OnHit(struct EventPhysCollision* pHit);
	void                     Update(float dt, const CCamera& rCamera);
	void                     BreakBranch(int idx);

	CStatObjFoliage*  m_next, * m_prev;
	int               m_nRefCount;
	int               m_flags;
	CStatObj*         m_pStatObj;
	IPhysicalEntity** m_pRopes;
	float*            m_pRopesActiveTime;
	IPhysicalEntity*  m_pTrunk;
	int16             m_nRopes;
	int16             m_bEnabled;
	float             m_timeIdle, m_lifeTime;
	IFoliage**        m_ppThis;
	QuatTS*           m_pSkinningTransformations[2];
	int               m_iActivationSource;
	int               m_bGeomRemoved;
	IRenderNode*      m_pVegInst;
	CRenderObject*    m_pRenderObject;
	float             m_timeInvisible;
	float             m_minEnergy;
	float             m_stiffness;
	int               m_bDelete;
	// history for skinning data, needed for motion blur
	struct { SSkinningData* pSkinningData; int nFrameID; } arrSkinningRendererData[3]; // tripple buffered for motion blur
};

struct SClothTangentVtx
{
	int  ivtxT;   // for each vertex, specifies the iThisVtx->ivtxT edge, which is the closest to the vertex's tangent vector
	Vec3 edge;    // that edge's projection on the vertex's normal basis
	int  sgnNorm; // sign of phys normal * normal from the basis
};

struct SSkinVtx
{
	int      bVolumetric;
	int      idx[4];
	float    w[4];
	Matrix33 M;
};

struct SDelayedSkinParams
{
	Matrix34   mtxSkelToMesh;
	IGeometry* pPhysSkel;
	float      r;
};

struct SPhysGeomThunk
{
	phys_geometry* pgeom;
	int            type;
	void           GetMemoryUsage(ICrySizer* pSizer) const
	{
		//		pSizer->AddObject(pgeom);
	}
};

struct SPhysGeomArray
{
	phys_geometry* operator[](int idx) const
	{
		if (idx < PHYS_GEOM_TYPE_DEFAULT)
			return idx < (int)m_array.size() ? m_array[idx].pgeom : 0;
		else
		{
			int i;
			for (i = m_array.size() - 1; i >= 0 && m_array[i].type != idx; i--);
			return i >= 0 ? m_array[i].pgeom : 0;
		}
	}
	void SetPhysGeom(phys_geometry* pgeom, int idx = PHYS_GEOM_TYPE_DEFAULT, int type = PHYS_GEOM_TYPE_DEFAULT)
	{
		int i;
		if (idx < PHYS_GEOM_TYPE_DEFAULT)
			i = idx, idx = type;
		else
			for (i = 0; i < (int)m_array.size() && m_array[i].type != idx; i++);
		if (pgeom)
		{
			if (i >= (int)m_array.size())
				m_array.resize(i + 1);
			m_array[i].pgeom = pgeom;
			m_array[i].type = idx;
		}
		else if (i < (int)m_array.size())
			m_array.erase(m_array.begin() + i);
	}
	int  GetGeomCount() const { return m_array.size(); }
	int  GetGeomType(int idx) { return idx >= PHYS_GEOM_TYPE_DEFAULT ? idx : m_array[idx].type; }
	std::vector<SPhysGeomThunk> m_array;
	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_array);
	}
};

struct SSyncToRenderMeshContext
{
	Vec3*                         vmin, * vmax;
	int                           iVtx0;
	int                           nVtx;
	strided_pointer<Vec3>         pVtx;
	int*                          pVtxMap;
	int                           mask;
	float                         rscale;
	SClothTangentVtx*             ctd;
	strided_pointer<Vec3>         pMeshVtx;
	strided_pointer<SPipTangents> pTangents;
	strided_pointer<Vec3>         pNormals; // TODO: change Vec3 to SPipNormal
	CStatObj*                     pObj;
	JobManager::SJobState         jobState;

	void                          Set(Vec3* _vmin, Vec3* _vmax, int _iVtx0, int _nVtx, strided_pointer<Vec3> _pVtx, int* _pVtxMap
	                                  , int _mask, float _rscale, SClothTangentVtx* _ctd, strided_pointer<Vec3> _pMeshVtx
	                                  , strided_pointer<SPipTangents> _pTangents, strided_pointer<Vec3> _pNormals, CStatObj* _pObj)
	{
		vmin = _vmin;
		vmax = _vmax;
		iVtx0 = _iVtx0;
		nVtx = _nVtx;
		pVtx = _pVtx;
		pVtxMap = _pVtxMap;
		mask = _mask;
		rscale = _rscale;
		ctd = _ctd;
		pMeshVtx = _pMeshVtx;
		pTangents = _pTangents;
		pNormals = _pNormals;
		pObj = _pObj;
	}
};

struct CRY_ALIGN(8) CStatObj: public IStatObj, public IStreamCallback, public stl::intrusive_linked_list_node<CStatObj>, public Cry3DEngineBase
{
	CStatObj();
	~CStatObj();

public:
	//////////////////////////////////////////////////////////////////////////
	// Variables.
	//////////////////////////////////////////////////////////////////////////
	volatile int m_nUsers; // reference counter

	uint32 m_nLastDrawMainFrameId;

	_smart_ptr<IRenderMesh> m_pRenderMesh;

	CryCriticalSection m_streamingMeshLock;
	_smart_ptr<IRenderMesh> m_pStreamedRenderMesh;
	_smart_ptr<IRenderMesh> m_pMergedRenderMesh;

	// Used by hierarchical breaking to hide sub-objects that initially must be hidden.
	hidemask m_nInitialSubObjHideMask;

	CIndexedMesh* m_pIndexedMesh;
	volatile int m_lockIdxMesh;

	string m_szFileName;
	string m_szGeomName;
	string m_szProperties;
	string m_szStreamingDependencyFilePath;

	int m_nLoadedTrisCount;
	int m_nLoadedVertexCount;
	int m_nRenderTrisCount;
	int m_nRenderMatIds;
	float m_fGeometricMeanFaceArea;
	float m_fLodDistance;
	Vec3 m_depthSortOffset;

	// Default material.
	_smart_ptr<IMaterial> m_pMaterial;

	// Billboard material and mesh
	_smart_ptr<IMaterial> m_pBillboardMaterial;

	float m_fRadiusHors;
	float m_fRadiusVert;

	AABB m_AABB;
	Vec3 m_vVegCenter;

	SPhysGeomArray m_arrPhysGeomInfo;
	ITetrLattice* m_pLattice;
	IStatObj* m_pLastBooleanOp;
	float m_lastBooleanOpScale;

	_smart_ptr<CStatObj>* m_pLODs;
	CStatObj* m_pLod0;                 // Level 0 stat object. (Pointer to the original object of the LOD)
	unsigned int m_nMinUsableLod0 : 8; // What is the minimal LOD that can be used as LOD0.
	unsigned int m_nMaxUsableLod0 : 8; // What is the maximal LOD that can be used as LOD0.
	unsigned int m_nMaxUsableLod  : 8; // What is the maximal LOD that can be used.
	unsigned int m_nLoadedLodsNum : 8; // How many lods loaded.

	string m_cgfNodeName;

	//////////////////////////////////////////////////////////////////////////
	// Externally set flags from enum EStaticObjectFlags.
	//////////////////////////////////////////////////////////////////////////
	int m_nFlags;

	//////////////////////////////////////////////////////////////////////////
	// Internal Flags.
	//////////////////////////////////////////////////////////////////////////
	unsigned int m_bCheckGarbage : 1;
	unsigned int m_bCanUnload : 1;
	unsigned int m_bLodsLoaded : 1;
	unsigned int m_bDefaultObject : 1;
	unsigned int m_bOpenEdgesTested : 1;
	unsigned int m_bSubObject : 1;          // This is sub object.
	unsigned int m_bVehicleOnlyPhysics : 1; // Object can be used for collisions with vehicles only
	unsigned int m_bBreakableByGame : 1;    // material is marked as breakable by game
	unsigned int m_bSharesChildren : 1;     // means its subobjects belong to another parent statobj
	unsigned int m_bHasDeformationMorphs : 1;
	unsigned int m_bTmpIndexedMesh : 1; // indexed mesh is temporary and can be deleted after MakeRenderMesh
	unsigned int m_bUnmergable : 1;     // Set if sub objects cannot be merged together to the single render merge.
	unsigned int m_bMerged : 1;         // Set if sub objects merged together to the single render merge.
	unsigned int m_bMergedLODs : 1;     // Set if m_pLODs were created while merging LODs
	unsigned int m_bLowSpecLod0Set : 1;
	unsigned int m_bHaveOcclusionProxy : 1; // If this stat object or its childs have occlusion proxy.
	unsigned int m_bLodsAreLoadedFromSeparateFile : 1;
	unsigned int m_bNoHitRefinement : 1;       // doesn't refine bullet hits against rendermesh
	unsigned int m_bDontOccludeExplosions : 1; // don't act as an explosion occluder in physics
	unsigned int m_hasClothTangentsData : 1;
	unsigned int m_hasSkinInfo : 1;
	unsigned int m_bMeshStrippedCGF : 1; // This CGF was loaded from the Mesh Stripped CGF, (in Level Cache)
	unsigned int m_isDeformable : 1;     // This cgf is deformable in the sense that it has a special renderpath
	unsigned int m_isProxyTooBig : 1;
	unsigned int m_bHasStreamOnlyCGF : 1;

	int m_idmatBreakable; // breakable id for the physics
	//////////////////////////////////////////////////////////////////////////

	// streaming
	int m_nRenderMeshMemoryUsage;
	int m_nMergedMemoryUsage;
	int m_arrRenderMeshesPotentialMemoryUsage[2];
	IReadStreamPtr m_pReadStream;
	uint32 m_nModificationId; // used to detect the cases when dependent permanent render objects have to be updated

#if !defined (_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)
	static float s_fStreamingTime;
	static int s_nBandwidth;
	float m_fStreamingStart;
#endif

#ifdef OBJMAN_STREAM_STATS
	int m_nStatoscopeState;
#endif

	//////////////////////////////////////////////////////////////////////////

	uint16* m_pMapFaceToFace0;
	union
	{
		SClothTangentVtx* m_pClothTangentsData;
		SSkinVtx*         m_pSkinInfo;
	};
	SDelayedSkinParams* m_pDelayedSkinParams;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Bendable Foliage.
	//////////////////////////////////////////////////////////////////////////
	SSpine* m_pSpines;
	int m_nSpines;
	struct SMeshBoneMapping_uint8* m_pBoneMapping;
	std::vector<uint16> m_chunkBoneIds;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// for debug purposes
	//////////////////////////////////////////////////////////////////////////
#ifdef TRACE_CGF_LEAKS
	string m_sLoadingCallstack;
#endif

private:

	// Returns a list of all CStatObj instances contained within this object (all sub-objects plus the parent object itself).
	std::vector<CStatObj*> GatherAllObjects();

	//////////////////////////////////////////////////////////////////////////
	// Sub objects.
	//////////////////////////////////////////////////////////////////////////
	std::vector<SSubObject> m_subObjects;
	CStatObj* m_pParentObject;       // Parent object (Must not be smart pointer).
	CStatObj* m_pClonedSourceObject; // If this is cloned object, pointer to original source object (Must not be smart pointer).
	int m_nSubObjectMeshCount;
	int m_nNodeCount;

	CGeomExtents m_Extents;           // Cached extents for random pos generation.

	//////////////////////////////////////////////////////////////////////////
	// Special AI/Physics parameters.
	//////////////////////////////////////////////////////////////////////////
	float m_aiVegetationRadius;
	float m_phys_mass;
	float m_phys_density;

	//////////////////////////////////////////////////////////////////////////
	// used only in the editor
	//////////////////////////////////////////////////////////////////////////

	SSyncToRenderMeshContext* m_pAsyncUpdateContext;

	//////////////////////////////////////////////////////////////////////////
	// METHODS.
	//////////////////////////////////////////////////////////////////////////
public:
	//////////////////////////////////////////////////////////////////////////
	// Fast non virtual access functions.
	ILINE IStatObj::SSubObject& SubObject(int nIndex)  { return m_subObjects[nIndex]; };
	ILINE int                   SubObjectCount() const { return m_subObjects.size(); };
	//////////////////////////////////////////////////////////////////////////

	virtual bool IsUnloadable() const final { return m_bCanUnload; }
	void DisableStreaming();

	virtual IIndexedMesh*    GetIndexedMesh(bool bCreatefNone = false) final;
	virtual IIndexedMesh*    CreateIndexedMesh() final;
	void ReleaseIndexedMesh(bool bRenderMeshUpdated = false);
	virtual ILINE const Vec3 GetVegCenter() final          { return m_vVegCenter; }

	virtual void             SetFlags(int nFlags) final		{ m_nFlags = nFlags; IncrementModificationId(); }
	virtual int              GetFlags() const final         { return m_nFlags; };

	virtual unsigned int     GetVehicleOnlyPhysics() final { return m_bVehicleOnlyPhysics; };
	virtual int              GetIDMatBreakable() final     { return m_idmatBreakable; };
	virtual unsigned int     GetBreakableByGame() final    { return m_bBreakableByGame; };

	virtual bool IsDeformable() final;

	// Loader
	bool LoadCGF(const char* filename, bool bLod, unsigned long nLoadingFlags, const void* pData, const int nDataSize);
	bool LoadCGF_Int(const char* filename, bool bLod, unsigned long nLoadingFlags, const void* pData, const int nDataSize);

	//////////////////////////////////////////////////////////////////////////
	virtual void SetMaterial(IMaterial * pMaterial) final;
	virtual IMaterial* GetMaterial() const final { return m_pMaterial; }
	//////////////////////////////////////////////////////////////////////////

	IMaterial * GetBillboardMaterial() { return m_pBillboardMaterial; }

	void RenderInternal(CRenderObject * pRenderObject, hidemask nSubObjectHideMask, const CLodValue &lodValue, const SRenderingPassInfo &passInfo);
	void RenderObjectInternal(CRenderObject * pRenderObject, int nLod, uint8 uLodDissolveRef, bool dissolveOut, const SRenderingPassInfo &passInfo);
	void RenderSubObject(CRenderObject * pRenderObject, int nLod,
	                     int nSubObjId, const Matrix34A &renderTM, const SRenderingPassInfo &passInfo);
	void RenderSubObjectInternal(CRenderObject * pRenderObject, int nLod, const SRenderingPassInfo &passInfo);
	virtual void Render(const SRendParams &rParams, const SRenderingPassInfo &passInfo) final;
	void RenderRenderMesh(CRenderObject * pObj, struct SInstancingInfo* pInstInfo, const SRenderingPassInfo &passInfo);
	virtual phys_geometry* GetPhysGeom(int nGeomType = PHYS_GEOM_TYPE_DEFAULT) const final { return m_arrPhysGeomInfo[nGeomType]; }
	virtual void           SetPhysGeom(phys_geometry* pPhysGeom, int nGeomType = PHYS_GEOM_TYPE_DEFAULT) final
	{
		if (m_arrPhysGeomInfo[nGeomType])
			GetPhysicalWorld()->GetGeomManager()->UnregisterGeometry(m_arrPhysGeomInfo[nGeomType]);
		m_arrPhysGeomInfo.SetPhysGeom(pPhysGeom, nGeomType);
	}
	virtual IPhysicalEntity* GetPhysEntity() const final               { return NULL; }
	virtual ITetrLattice*    GetTetrLattice() final                    { return m_pLattice; }

	virtual float            GetAIVegetationRadius() const final       { return m_aiVegetationRadius; }
	virtual void             SetAIVegetationRadius(float radius) final { m_aiVegetationRadius = radius; }

	//! Refresh object ( reload shaders or/and object geometry )
	virtual void Refresh(int nFlags) final;

	virtual IRenderMesh* GetRenderMesh() const final { return m_pRenderMesh; };
	void SetRenderMesh(IRenderMesh * pRM);

	virtual const char* GetFilePath() final                       { return (m_szFileName); }
	virtual void        SetFilePath(const char* szFileName) final { m_szFileName = szFileName; }
	virtual const char* GetGeoName() final                        { return (m_szGeomName); }
	virtual void        SetGeoName(const char* szGeoName) final   { m_szGeomName = szGeoName; }
	virtual bool IsSameObject(const char* szFileName, const char* szGeomName) final;

	//set object's min/max bbox
	virtual void SetBBoxMin(const Vec3& vBBoxMin) final { m_AABB.min = vBBoxMin; }
	virtual void SetBBoxMax(const Vec3& vBBoxMax) final { m_AABB.max = vBBoxMax; }
	virtual AABB GetAABB() const final                  { return m_AABB; }

	virtual float GetExtent(EGeomForm eForm) final;
	virtual void GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const final;

	virtual Vec3 GetHelperPos(const char* szHelperName) final;
	virtual const Matrix34& GetHelperTM(const char* szHelperName) final;

	virtual float           GetRadiusVert() const final { return m_fRadiusVert; }
	virtual float           GetRadiusHors() const final { return m_fRadiusHors; }

	virtual int AddRef() final;
	virtual int Release() final;
	virtual int  GetRefCount() const final { return m_nUsers; }

	virtual bool IsDefaultObject() final   { return (m_bDefaultObject); }

	int          GetLoadedTrisCount()         { return m_nLoadedTrisCount; }
	int          GetRenderTrisCount()         { return m_nRenderTrisCount; }

	// Load LODs
	virtual void SetLodObject(int nLod, IStatObj * pLod) final;
	bool LoadLowLODS_Prep(bool bUseStreaming, unsigned long nLoadingFlags);
	CStatObj* LoadLowLODS_Load(int nLodLevel, bool bUseStreaming, unsigned long nLoadingFlags, const void* pData, int nDataLen);
	void LoadLowLODS_Finalize(int nLoadedLods, CStatObj * loadedLods[MAX_STATOBJ_LODS_NUM]);
	void LoadLowLODs(bool bUseStreaming, unsigned long nLoadingFlags);
	// Free render resources for unused upper LODs.
	void CleanUnusedLods();

	virtual void FreeIndexedMesh() final;
	bool RenderDebugInfo(CRenderObject * pObj, const SRenderingPassInfo &passInfo);

	//! Release method.
	virtual void GetMemoryUsage(class ICrySizer* pSizer) const final;

	void ShutDown();
	void Init();

	//  void CheckLoaded();
	virtual IStatObj* GetLodObject(int nLodLevel, bool bReturnNearest = false) final;
	virtual IStatObj* GetLowestLod() final;

	virtual int FindNearestLoadedLOD(int nLodIn, bool bSearchUp = false) final;
	virtual int FindHighestLOD(int nBias) final;

	// interface IStreamCallback -----------------------------------------------------

	virtual void StreamAsyncOnComplete(IReadStream * pStream, unsigned nError) final;
	virtual void StreamOnComplete(IReadStream * pStream, unsigned nError) final;
	void IncrementModificationId();
	uint32 GetModificationId() { return m_nModificationId; }

	// -------------------------------------------------------------------------------

	virtual void StartStreaming(bool bFinishNow, IReadStream_AutoPtr * ppStream) final;
	void UpdateStreamingPrioriryInternal(const Matrix34A &objMatrix, float fDistance, bool bFullUpdate);

	void MakeCompiledFileName(char* szCompiledFileName, int nMaxLen);

	virtual bool IsPhysicsExist() const final;
	bool IsSphereOverlap(const Sphere &sSphere);
	virtual void Invalidate(bool bPhysics = false, float tolerance = 0.05f) final;

	void AnalyzeFoliage(IRenderMesh * pRenderMesh, CContentCGF * pCGF);
	void FreeFoliageData();
	virtual void CopyFoliageData(IStatObj * pObjDst, bool bMove = false, IFoliage * pSrcFoliage = 0, int* pVtxMap = 0, primitives::box * pMoveBoxes = 0, int nMovedBoxes = -1) final;
	virtual int PhysicalizeFoliage(IPhysicalEntity * pTrunk, const Matrix34 &mtxWorld, IFoliage * &pRes, float lifeTime = 0.0f, int iSource = 0) final;
	int SerializeFoliage(TSerialize ser, IFoliage * pFoliage);

	virtual IStatObj* UpdateVertices(strided_pointer<Vec3> pVtx, strided_pointer<Vec3> pNormals, int iVtx0, int nVtx, int* pVtxMap = 0, float rscale = 1.f) final;
	bool              HasSkinInfo(float skinRadius = -1.0f)                                                { return m_hasSkinInfo && m_pSkinInfo && (skinRadius < 0.0f || m_pSkinInfo[m_nLoadedVertexCount].w[0] == skinRadius); }
	void PrepareSkinData(const Matrix34 &mtxSkelToMesh, IGeometry * pPhysSkel, float r = 0.0f);
	virtual IStatObj* SkinVertices(strided_pointer<Vec3> pSkelVtx, const Matrix34& mtxSkelToMesh) final { return SkinVertices(pSkelVtx, mtxSkelToMesh, NULL);  }
	IStatObj*         SkinVertices(strided_pointer<Vec3> pSkelVtx, const Matrix34& mtxSkelToMesh, volatile int* ready);

	//////////////////////////////////////////////////////////////////////////
	// Sub objects.
	//////////////////////////////////////////////////////////////////////////
	virtual int                   GetSubObjectCount() const final { return m_subObjects.size(); }
	virtual void SetSubObjectCount(int nCount) final;
	virtual IStatObj::SSubObject* FindSubObject(const char* sNodeName) final;
	virtual IStatObj::SSubObject* FindSubObject_StrStr(const char* sNodeName) final;
	virtual IStatObj::SSubObject* FindSubObject_CGA(const char* sNodeName) final;
	virtual IStatObj::SSubObject* GetSubObject(int nIndex) final
	{
		if (nIndex >= 0 && nIndex < (int)m_subObjects.size())
			return &m_subObjects[nIndex];
		else
			return 0;
	}
	virtual bool RemoveSubObject(int nIndex) final;
	virtual IStatObj* GetParentObject() const final      { return m_pParentObject; }
	virtual IStatObj* GetCloneSourceObject() const final { return m_pClonedSourceObject; }
	virtual bool      IsSubObject() const final          { return m_bSubObject; };
	virtual bool CopySubObject(int nToIndex, IStatObj * pFromObj, int nFromIndex) final;
	virtual int PhysicalizeSubobjects(IPhysicalEntity * pent, const Matrix34 * pMtx, float mass, float density = 0.0f, int id0 = 0,
	                                  strided_pointer<int> pJointsIdMap = 0, const char* szPropsOverride = 0, int idbodyArtic = -1) final;
	virtual IStatObj::SSubObject& AddSubObject(IStatObj* pStatObj) final;
	virtual int Physicalize(IPhysicalEntity * pent, pe_geomparams * pgp, int id = -1, const char* szPropsOverride = 0) final;
	//////////////////////////////////////////////////////////////////////////

	virtual bool SaveToCGF(const char* sFilename, IChunkFile * *pOutChunkFile = NULL, bool bHavePhiscalProxy = false) final;

	//virtual IStatObj* Clone(bool bCloneChildren=true, bool nDynamic=false);
	virtual IStatObj* Clone(bool bCloneGeometry, bool bCloneChildren, bool bMeshesOnly) final;

	virtual int SetDeformationMorphTarget(IStatObj * pDeformed) final;
	virtual int SubobjHasDeformMorph(int iSubObj);
	virtual IStatObj* DeformMorph(const Vec3& pt, float r, float strength, IRenderMesh* pWeights = 0) final;

	virtual IStatObj* HideFoliage() final;

	virtual int Serialize(TSerialize ser) final;

	// Get object properties as loaded from CGF.
	virtual const char* GetProperties() final                  { return m_szProperties.c_str(); };
	virtual void        SetProperties(const char* props) final { m_szProperties = props; ParseProperties(); }

	virtual bool GetPhysicalProperties(float& mass, float& density) final;

	virtual IStatObj* GetLastBooleanOp(float& scale) final { scale = m_lastBooleanOpScale; return m_pLastBooleanOp; }

	// Intersect ray with static object.
	// Ray must be in object local space.
	virtual bool RayIntersection(SRayHitInfo & hitInfo, IMaterial * pCustomMtl = 0) final;
	virtual bool LineSegIntersection(const Lineseg &lineSeg, Vec3 & hitPos, int& surfaceTypeId) final;

	virtual void DebugDraw(const SGeometryDebugDrawInfo &info) final;
	virtual void GetStatistics(SStatistics & stats) final;
	//////////////////////////////////////////////////////////////////////////

	IParticleEffect* GetSurfaceBreakageEffect(const char* sType);

	virtual hidemask GetInitialHideMask() final { return m_nInitialSubObjHideMask; }

	virtual void     SetStreamingDependencyFilePath(const char* szFileName) final
	{
		const bool streamingDependencyLoop = CheckForStreamingDependencyLoop(szFileName);
		if (streamingDependencyLoop)
		{
			CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING, "StatObj '%s' cannot set '%s' as a streaming dependency as it would result in a looping dependency.", GetFilePath(), szFileName);
			return;
		}

		m_szStreamingDependencyFilePath = szFileName;
	}

	int GetMaxUsableLod();
	int GetMinUsableLod();
	void RenderStreamingDebugInfo(CRenderObject * pRenderObject, const SRenderingPassInfo& passInfo);
	void RenderCoverInfo(CRenderObject * pRenderObject, const SRenderingPassInfo& passInfo);
	int CountChildReferences();
	void ReleaseStreamableContent() final;
	int GetStreamableContentMemoryUsage(bool bJustForDebug = false) final;
	virtual SMeshLodInfo ComputeGeometricMean() const final;
	SMeshLodInfo ComputeAndStoreLodDistances();
	virtual float GetLodDistance() const final     { return m_fLodDistance; }
	virtual Vec3  GetDepthSortOffset() const final { return m_depthSortOffset; }
	virtual int ComputeLodFromScale(float fScale, float fLodRatioNormalized, float fEntDistance, bool bFoliage, bool bForPrecache) final;
	bool UpdateStreamableComponents(float fImportance, const Matrix34A &objMatrix, bool bFullUpdate, int nNewLod);
	void GetStreamableName(string& sName) final
	{
		sName = m_szFileName;
		if (m_szGeomName.length())
		{
			sName += " - ";
			sName += m_szGeomName;
		}
	};
	void GetStreamFilePath(stack_string & strOut);
	void FillRenderObject(const SRendParams &rParams, IRenderNode * pRenderNode, IMaterial * pMaterial,
	                      SInstancingInfo * pInstInfo, CRenderObject * &pObj, const SRenderingPassInfo &passInfo);
	virtual uint32 GetLastDrawMainFrameId() final { return m_nLastDrawMainFrameId; }

	// Allow pooled allocs
	static void* operator new(size_t size);
	static void  operator delete(void* pToFree);

	// Used in ObjMan.
	void TryMergeSubObjects(bool bFromStreaming);
	void SavePhysicalizeData(CNodeCGF * pNode);

protected:
	// Called by async stream callback.
	bool LoadStreamRenderMeshes(const char* filename, const void* pData, const int nDataSize, bool bLod);
	// Called by sync stream complete callback.
	void CommitStreamRenderMeshes();

	void MergeSubObjectsRenderMeshes(bool bFromStreaming, CStatObj * pLod0, int nLod);
	void UnMergeSubObjectsRenderMeshes();
	bool CanMergeSubObjects();
	bool IsMatIDReferencedByObj(uint16 matID);

	//	bool LoadCGF_Info( const char *filename );
	CStatObj* MakeStatObjFromCgfNode(CContentCGF* pCGF, CNodeCGF* pNode, bool bLod, int nLoadingFlags, AABB& commonBBox);
	void ParseProperties();

	void CalcRadiuses();
	void GetStatisticsNonRecursive(SStatistics & stats);

	void PhysicalizeCompiled(CNodeCGF * pNode, int bAppend = 0);
	bool PhysicalizeGeomType(int nGeomType, CMesh & mesh, float tolerance = 0.05f, int bAppend = 0);
	bool RegisterPhysicGeom(int nGeomType, phys_geometry * pPhysGeom);
	void AssignPhysGeom(int nGeomType, phys_geometry * pPhysGeom, int bAppend = 0, int bLoading = 0);

	// Creates static object contents from mesh.
	// Return true if successful.
	_smart_ptr<IRenderMesh> MakeRenderMesh(CMesh * pMesh, bool bDoRenderMesh);
	void MakeRenderMesh();

	const char* stristr(const char* szString, const char* szSubstring)
	{
		int nSuperstringLength = (int)strlen(szString);
		int nSubstringLength = (int)strlen(szSubstring);

		for (int nSubstringPos = 0; nSubstringPos <= nSuperstringLength - nSubstringLength; ++nSubstringPos)
		{
			if (strnicmp(szString + nSubstringPos, szSubstring, nSubstringLength) == 0)
				return szString + nSubstringPos;
		}
		return NULL;
	}

	bool CheckForStreamingDependencyLoop(const char* szFilenameDependancy) const;
	void CheckCreateBillboardMaterial();
	void CreateBillboardMesh(IMaterial* pMaterial);
};

//////////////////////////////////////////////////////////////////////////
inline void InitializeSubObject(IStatObj::SSubObject& so)
{
	so.localTM.SetIdentity();
	so.name = "";
	so.properties = "";
	so.nType = STATIC_SUB_OBJECT_MESH;
	so.pWeights = 0;
	so.pFoliage = 0;
	so.nParent = -1;
	so.tm.SetIdentity();
	so.bIdentityMatrix = true;
	so.bHidden = false;
	so.helperSize = Vec3(0, 0, 0);
	so.pStatObj = 0;
	so.bShadowProxy = 0;
}

#endif // STAT_OBJ_H
