// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AttachmentBase.h"
#include "Vertex/VertexData.h"
#include "Vertex/VertexAnimation.h"
#include "ModelSkin.h"
#include "SkeletonPose.h"
#include "AttachmentVClothPreProcess.h"

class CCharInstance;
class CClothProxies;
class CAttachmentManager;
struct SVertexAnimationJob;
class CClothPhysics;
struct SBuffers;

typedef Vec3     Vector4;
typedef Matrix33 Matrix3;
typedef Quat     Quaternion;

inline Vec3 SseToVec3(const Vector4& v)
{
	return v;
}

inline Quat SseToQuat(const Quaternion& q)
{
	return q;
}

enum ECollisionPrimitiveType
{
	eCPT_Invalid,
	eCPT_Capsule,
	eCPT_Sphere,
};

struct SPrimitive
{
	ECollisionPrimitiveType type;
	SPrimitive() : type(eCPT_Invalid) {}
	virtual ~SPrimitive() {}
};

struct SSphere : SPrimitive
{
	Vector4 center;
	float   r;
	SSphere()
		: center(ZERO)
		, r(0.0f)
	{
		type = eCPT_Sphere;
	}
};

struct SCapsule : SPrimitive
{
	Vector4 axis, center;
	float   hh, r;
	SCapsule()
		: axis(ZERO)
		, center(ZERO)
		, hh(0.0f)
		, r(0.0f)
	{
		type = eCPT_Capsule;
	}
};

enum EPhysicsLayer
{
	PHYS_LAYER_INVALID,
	PHYS_LAYER_ARTICULATED,
	PHYS_LAYER_CLOTH,
};

struct SPhysicsGeom
{
	SPrimitive*   primitive;
	EPhysicsLayer layer;

	SPhysicsGeom()
		: primitive(nullptr)
		, layer(PHYS_LAYER_INVALID)
	{}
};

struct SBonePhysics
{
	quaternionf q;
	Vec3        t;
	f32         cr, ca;

	SBonePhysics()
	{
		cr = 0;
		ca = 0;
		t = Vec3(ZERO);
		q.SetIdentity();
	}
};

struct STangents
{
	Vector4 t, n;
	STangents()
	{
		t.zero();
		n.zero();
	}
};

struct SBuffers
{
	// temporary buffer for simulated cloth vertices
	DynArray<Vector4>    m_tmpClothVtx;
	std::vector<Vector4> m_normals;

	// output buffers for sw-skinning
	DynArray<Vec3>         m_arrDstPositions;
	DynArray<SPipTangents> m_arrDstTangents; // TODO: static

	std::vector<STangents> m_tangents;
};

struct SSkinMapEntry
{
	int   iMap; // vertex index in the sim mesh
	int   iTri; // triangle index in the sim mesh
	float s, t; // barycentric coordinates in the adjacent triangle
	float h;    // distance from triangle
};

struct STangentData
{
	float t1, t2, r;
};

struct SClothGeometry
{
	enum
	{
		MAX_LODS = 2,
	};

	// the number of vertices in the sim mesh
	int            nVtx;
	// registered physical geometry constructed from sim mesh
	phys_geometry* pPhysGeom;
	// mapping between original mesh and welded one
	vtx_idx*       weldMap;
	// number of non-welded vertices in the sim mesh
	int            nUniqueVtx;
	// blending weights between skinning and sim (coming from vertex colors)
	float*         weights;
	// mapping between render mesh and sim mesh vertices
	SSkinMapEntry* skinMap[MAX_LODS];
	// render mesh indices
	uint           numIndices[MAX_LODS];
	vtx_idx*       pIndices[MAX_LODS];
	// tangent frame uv data
	STangentData*  tangentData[MAX_LODS];
	// maximum number for vertices (for LOD0)
	int            maxVertices;

	// memory pool. keeping it as a list for now to avoid reallocation
	// because the jobs are using pointers inside it
	std::list<SBuffers> pool;
	std::vector<int>    freePoolSlots;
	volatile int        poolLock;

	SClothGeometry()
		: nVtx(0)
		, pPhysGeom(NULL)
		, weldMap(NULL)
		, nUniqueVtx(0)
		, weights(NULL)
		, poolLock(0)
		, maxVertices(0)
	{
		for (int i = 0; i < MAX_LODS; i++)
		{
			skinMap[i] = NULL;
			pIndices[i] = NULL;
			tangentData[i] = NULL;
		}
	}

	void      Cleanup();
	int       GetBuffers();
	void      ReleaseBuffers(int idx);
	SBuffers* GetBufferPtr(int idx);
	void      AllocateBuffer();
};

struct SClothInfo
{
	uint64 key;
	float  distance;
	bool   visible;
	int    frame;

	SClothInfo() {}
	SClothInfo(uint64 k, float d, bool v, int f) : key(k), distance(d), visible(v), frame(f) {}
};

struct SPrimitive;

struct SParticleCold
{
	Vector4 prevPos;
	Vector4 oldPos;
	Vector4 skinnedPos;
	int     permIdx;
	int     bAttached;

	SParticleCold()
	{
		prevPos.zero();
		bAttached = 0;
		skinnedPos.zero();
		permIdx = -1;
		oldPos.zero();
	}
};

struct SParticleHot
{
	Vector4 pos;

	float   alpha;          //!< for blending with animation; == 0 if unconstrained .. == 1.0 if attached
	float   factorAttached; //!< == 0 if attached .. == 1.0 if unconstrained; i.e. 1.0f-alpha
	int     timer;

	bool    collisionExist;  //!< set to true, if a collision occurs
	Vector4 collisionNormal; //!< stores collision normal for damping in tangential direction

	// Nearest Neighbor Distance Constraints
	int   nndcIdx;        //!< index of closest constraint / attached vtx
	float nndcDist;       //!< distance to closest constraint
	int   nndcNextParent; //!< index of next parent on path to closest constraint

	SParticleHot() : pos(ZERO), alpha(0), factorAttached(0), timer(0), collisionExist(false), collisionNormal(ZERO), nndcIdx(-1), nndcDist(0), nndcNextParent(-1)
	{
	}
};

struct SCollidable
{
	Matrix3 R, oldR;
	QuatT   qLerp; //!< interpolation [R,oldR & translation] for substeps

	Vector4 offset, oldOffset;
	f32     cr, cx, cy, cz; //!< lozenge definition

	SCollidable()
		: R(IDENTITY)
		, oldR(IDENTITY)
		, offset(ZERO)
		, oldOffset(ZERO)
		, cr(0.0f)
		, cx(0.0f)
		, cy(0.0f)
		, cz(0.0f)
	{
	}
};

class CClothSimulator
{
public:

	CClothSimulator()
		: m_pAttachmentManager(nullptr)
		, m_nVtx(0)
		, m_nEdges(0)
		, m_particlesCold(nullptr)
		, m_particlesHot(nullptr)
		, m_links(nullptr)
		, m_gravity(0, 0, -9.8f)
		, m_time(0.0f)
		, m_timeInterval(0.0f)
		, m_dt(0.0f)
		, m_dtPrev(-1)
		, m_dtNormalize(100.0f) // i.e. normalize substep to 1/dt with dt = 0.01
		, m_steps(0)
		, m_normalizedTimePrev(0)
		, m_doSkinningForNSteps(0)
		, m_forceSkinningAfterNFramesCounter(0)
		, m_isFramerateBelowFpsThresh(false)
		, m_fadeInOutPhysicsDirection(0)
		, m_fadeTimeActual(0) // physical fade time
		, m_bIsInitialized(false)
		, m_bIsGpuSkinning(false)
		, m_debugCollidableSubsteppingId(0)
	{
	}

	~CClothSimulator()
	{
		SAFE_DELETE_ARRAY(m_particlesHot);
		SAFE_DELETE_ARRAY(m_particlesCold);
		SAFE_DELETE_ARRAY(m_links);
	}

	/**
	 * Disable simulation totally, e.g., if camera is far away.
	 */
	void                 EnableSimulation(bool enable = true)   { m_config.disableSimulation = !enable; }
	bool                 IsSimulationEnabled() const            { return !m_config.disableSimulation; }

	bool				 IsGpuSkinning()						{ return m_bIsGpuSkinning; }
	void                 SetGpuSkinning(bool bGpuSkinning)		{ m_bIsGpuSkinning=bGpuSkinning; }

	bool                 IsVisible();

	const SVClothParams& GetParams() const                      { return m_config; };
	void                 SetParams(const SVClothParams& params) { m_config = params; };
	int                  SetParams(const SVClothParams& params, float* weights);

	void                 StartStep(float time_interval, const QuatT& location);
	int                  Step();

	bool                 AddGeometry(phys_geometry* pgeom);
	void                 SetSkinnedPositions(const Vector4* points);
	void                 GetVertices(Vector4* pWorldCoords) const;
	void                 GetVerticesFaded(Vector4* pWorldCoords);
	bool                 IsParticleAttached(unsigned int idx) const { assert(idx < m_nVtx); return m_particlesCold[idx].bAttached != 0; }

	/**
	 * Laplace-filter for input-positions, using the default mesh-edges.
	 */
	void LaplaceFilterPositions(Vector4* positions, float intensity);

	/**
	 * Enable skinning/physics according to camera distance.
	 */
	void HandleCameraDistance();
	void SetInitialized(bool initialized = true) { m_bIsInitialized = initialized; }
	bool IsInitialized() const                   { return m_bIsInitialized; }

	// fading/blending between skinning and simulation
	bool IsFading() const { return (m_fadeTimeActual > 0.0f) || (m_fadeInOutPhysicsDirection != 0); }
	void DecreaseFadeInOutTimer(float dt);

	void DrawHelperInformation();

	void SetAttachmentManager(const CAttachmentManager* am) { m_pAttachmentManager = am; }

	/**
	 * Get metadata from skin.
	 */
	bool GetMetaData(mesh_data* pMesh, CSkin* pSimSkin);

	/**
	 * Generate metadata on the fly.
	 */
	void GenerateMetaData(mesh_data* pMesh, CSkin* pSimSkin, float* weights);

private:

	void PrepareEdge(SLink& link);
	void SolveEdge(const SLink& link, float stretch);
	void DampEdge(const SLink& link, f32 damping);
	void DampTangential();
	void DampPositionBasedDynamics();
	void PositionsIntegrate();
	void PositionsPullToSkinnedPositions();

	/**
	 * Set skinning positions; e.g. to reduce strong simulation forces when simulation is enabled/faded in.
	 */
	void PositionsSetToSkinned(bool projectToProxySurface = true, bool setPosOld = true);
	void PositionsSetAttachedToSkinnedInterpolated(float t01);

	/**
	 * Moves particle-positions 'm_particlesHot[i].pos' lying inside proxy to proxies surface; using time-interpolation if t01 is set.
	 */
	void PositionsProjectToProxySurface(f32 t01 = 1.0f);
	void BendByTriangleAngleDetermineNormals();
	void BendByTriangleAngleSolve(float kBend);

	/**
	 * Determine actual, lerped quaternions for colliders
	 */
	void UpdateCollidablesLerp(f32 t01 = 1.0f);
	void NearestNeighborDistanceConstraintsSolve();

	/**
	 * Check distance to camera.
	 * @return True, if distance to camera is les than value; false, otherwise.
	 */
	bool CheckCameraDistanceLessThan(float dist) const;

	/**
	 * Check screen space size of characters bounding box in x- or y-direction against viewport-size [using provided percentage-threshold]. 
	 * @return True, if x- or y-dimension of bounding box in screen space is larger than provided threshold.
	 */
	bool CheckSSRatioLargerThan(float ssAxisSizePercThresh) const;

	/**
	 * Check framerate.
	 * @return True, if framerate is less than m_config.forceSkinningFpsThreshold; false otherwise.
	 */
	bool CheckForceSkinningByFpsThreshold();
	bool CheckForceSkinning();

	/**
	 * Detect rewind from end to start of animation.
	 */
	bool CheckAnimationRewind();
	void DoForceSkinning();
	void DoAnimationRewind();

	// fading
	bool IsFadingOut() const { return (m_fadeTimeActual > 0.0f) && (m_fadeInOutPhysicsDirection == -1); }
	bool IsFadingIn()  const { return (m_fadeTimeActual > 0.0f) && (m_fadeInOutPhysicsDirection == 1); }
	void InitFadeInOutPhysics();
	void EnableFadeOutPhysics();
	void EnableFadeInPhysics();

	void DebugOutput(float stepTime01);

private:

	bool                      m_bIsInitialized;
	ColorB                    m_color;

	SVClothParams             m_config;
	const CAttachmentManager* m_pAttachmentManager;

	int                       m_nVtx;          //!< the number of particles
	int                       m_nEdges;        //!< the number of links between particles
	SParticleCold*            m_particlesCold; //!< the simulation particles
	SParticleHot*             m_particlesHot;  //!< the simulation particles

	SLink*                    m_links;        //!< the structural links between particles / stretch links
	std::vector<SLink>        m_shearLinks;   //!< shear links
	std::vector<SLink>        m_bendLinks;    //!< bend links
	Vector4                   m_gravity;      //!< the gravity vector
	float                     m_time;         //!< time accumulator over frames (used to determine the number of sub-steps)
	float                     m_timeInterval; //!< time interval to be simulated for last frame
	float                     m_dt;           //!< dt of actual substep
	float                     m_dtPrev;       //!< dt of previous substep - needed to determine the velocity by posPrev within position based approach
	float                     m_dtNormalize;  //!< normalization factor for dt to convert constants in suitable range
	float                     m_normalizedTimePrev;
	int                       m_steps;

	std::vector<SCollidable>  m_permCollidables; //!< list of collision proxies (no collision with the world)

	float                     m_fadeTimeActual;                   //!< actual fade time
	int                       m_fadeInOutPhysicsDirection;        //!< -1 fade out, 1 fade in
	int                       m_doSkinningForNSteps;              //!< use skinning if any position change has occured, to keep simulation stable
	int                       m_forceSkinningAfterNFramesCounter; //!< safety mechanism, i.e. local counter: if framerate falls below threshold for n-frames, skinning is forced to avoid performance issues
	bool                      m_isFramerateBelowFpsThresh;        //!< true, if framerate is below user provided framerate-threshold, false otherwise

	Vec3                      m_externalDeltaTranslation;  //!< delta translation of locator per timestep; is used to determine external influence according to velocity
	Vec3                      m_permCollidables0Old;       //!< to determine above m_externalDeltaTranslation per step
	QuatT                     m_location;                  //!< global location / not used in the moment

	bool					  m_bIsGpuSkinning;            //!< true, if simulation is not needed (e.g., due to distance threshold), thus, the cloth can be skinned in total

	// Nearest Neighbor Distance Constraints
	std::vector<int> m_nndcNotAttachedOrderedIdx; //!< not attached particles: ordered by distance to constraints

	// Bending by triangle angles, not springs
	std::vector<SBendTrianglePair> m_listBendTrianglePairs; //!< triangle pairs sharing an edge
	std::vector<SBendTriangle>     m_listBendTriangles;     //!< triangles which are used for bending

	// Laplace container
	std::vector<Vec3>  m_listLaplacePosSum;
	std::vector<float> m_listLaplaceN;

	// DebugDraw collidables substep interpolation
	int                m_debugCollidableSubsteppingId;
	std::vector<QuatT> m_debugCollidableSubsteppingQuatT; //!< for debug rendering of interpolated proxies
};

ILINE void CClothSimulator::PrepareEdge(SLink& link)
{
	int n1 = !(m_particlesCold[link.i1].bAttached);
	int n2 = !(m_particlesCold[link.i2].bAttached);
	link.skip = n1 + n2 == 0;
	if (link.skip)
		return;
	float m1Inv = n1 ? 1.f : 0.f;
	float m2Inv = n2 ? 1.f : 0.f;
	float mu = 1.0f / (m1Inv + m2Inv);
	float stretch = 1.f;
	link.weight1 = m1Inv * mu * stretch;
	link.weight2 = m2Inv * mu * stretch;
}

ILINE void CClothSimulator::SolveEdge(const SLink& link, float stretch)
{
	if (link.skip) return;
	Vector4& v1 = m_particlesHot[link.i1].pos;
	Vector4& v2 = m_particlesHot[link.i2].pos;
	Vector4 delta = v1 - v2;

	const float lenSqr = delta.len2();
	delta *= stretch * (lenSqr - link.lenSqr) / (lenSqr + link.lenSqr);

	if (!m_particlesCold[link.i1].bAttached) v1 -= link.weight1 * delta * m_dt;
	if (!m_particlesCold[link.i2].bAttached) v2 += link.weight2 * delta * m_dt;

}

ILINE void CClothSimulator::DampEdge(const SLink& link, f32 damping)
{
	if (link.skip) return;

	Vector4& p1 = m_particlesHot[link.i1].pos;
	Vector4& p2 = m_particlesHot[link.i2].pos;
	Vector4 delta = p2 - p1;

	Vector4 vel1 = m_particlesHot[link.i1].pos - m_particlesCold[link.i1].prevPos;
	Vector4 vel2 = m_particlesHot[link.i2].pos - m_particlesCold[link.i2].prevPos;
	Vector4 dv = (vel2 - vel1) / m_dtPrev; // relative velocity

	// fast determination of dvn for damping (without sqrt), see 'Physically Based Deformable Models in Computer Graphics', Nealen et al
	Vector4 dvn = (dv.dot(delta) / delta.dot(delta)) * delta;

	// damp velocity by changing prevPos
	if (!m_particlesCold[link.i1].bAttached) m_particlesCold[link.i1].prevPos -= damping * dvn * m_dt;
	if (!m_particlesCold[link.i2].bAttached) m_particlesCold[link.i2].prevPos += damping * dvn * m_dt;
	// possible variation: change posHot, since prevPos is not used within collision/stiffness loop
	//if (!m_particlesCold[link.i1].bAttached) m_particlesHot[link.i1].pos += damping * dvn;
	//if (!m_particlesCold[link.i2].bAttached) m_particlesHot[link.i2].pos -= damping * dvn;
}

class CClothPiece
{
private:
	friend struct VertexCommandClothSkin;

	enum
	{
		HIDE_INTERVAL = 50
	};

public:
	CClothPiece()
	{
		m_pCharInstance = NULL;
		m_pVClothAttachment = NULL;
		m_bHidden = false;
		m_numLods = 0;
		m_clothGeom = NULL;
		//	m_bSingleThreaded = false;
		m_lastVisible = false;
		m_bAlwaysVisible = false;
		m_currentLod = 0;
		m_buffers = NULL;
		m_poolIdx = -1;
		m_initialized = false;
	}

	// initializes the object given a skin and a stat obj
	bool                 Initialize(const CAttachmentVCLOTH* pVClothAttachment);

	void                 Dettach();

	int                  GetNumLods()      { return m_numLods; }
	//	bool IsSingleThreaded() { return m_bSingleThreaded; }
	bool                 IsAlwaysVisible() { return m_bAlwaysVisible; }

	bool                 PrepareCloth(CSkeletonPose& skeletonPose, const Matrix34& worldMat, bool visible, int lod);
	bool                 CompileCommand(SVertexSkinData& skinData, CVertexCommandBuffer& commandBuffer);

	void                 SetBlendWeight(float weight);

	bool                 NeedsDebugDraw() const { return Console::GetInst().ca_DrawCloth & 2 ? true : false; }
	void                 DrawDebug(const SVertexAnimationJob* pVertexAnimation);

	void                 SetClothParams(const SVClothParams& params);
	const SVClothParams& GetClothParams();
	CClothSimulator&     GetSimulator() { return m_simulator; }

private:

	bool    PrepareRenderMesh(int lod);
	Vector4 SkinByTriangle(int i, strided_pointer<Vec3>& pVtx, int lod);

	void    UpdateSimulation(const DualQuat* pTransformations, const uint transformationCount);
	template<bool PREVIOUS_POSITIONS>
	void    SkinSimulationToRenderMesh(int lod, CVertexData& vertexData, const strided_pointer<const Vec3>& pVertexPositionsPrevious);
	void    SetRenderPositionsFromSkinnedPositions(bool setAllPositions);

	void    WaitForJob(bool bPrev);

private:

	CAttachmentVCLOTH* m_pVClothAttachment;
	CClothSimulator    m_simulator;

	// structure containing all sim mesh related data and working buffers
	SClothGeometry* m_clothGeom;
	SBuffers*       m_buffers;
	int             m_poolIdx;

	// used for hiding the cloth (mainly in the editor)
	bool m_bHidden;

	// the number of loaded LODs
	int m_numLods;

	// enables single threaded simulation updated
	//	bool m_bSingleThreaded;

	// the current rotation of the character
	QuatT m_charLocation;

	// for easing in and out
	bool m_lastVisible;

	// flags that the cloth is always simulated
	bool m_bAlwaysVisible;

	int  m_currentLod;

	// used to get the correct time from the character
	CCharInstance* m_pCharInstance;

	bool           m_initialized;
};

ILINE Vector4 CClothPiece::SkinByTriangle(int i, strided_pointer<Vec3>& pVtx, int lod)
{
	DynArray<Vector4>& tmpClothVtx = m_buffers->m_tmpClothVtx;
	std::vector<Vector4>& normals = m_buffers->m_normals;
	const SSkinMapEntry& skinMap = m_clothGeom->skinMap[lod][i];
	int tri = skinMap.iTri;
	if (tri >= 0)
	{
		mesh_data* md = (mesh_data*)m_clothGeom->pPhysGeom->pGeom->GetData();
		const int i2 = skinMap.iMap;
		const int base = tri * 3;
		const int idx = md->pIndices[base + i2];
		const int idx0 = md->pIndices[base + inc_mod3[i2]];
		const int idx1 = md->pIndices[base + dec_mod3[i2]];
		const Vector4 u = tmpClothVtx[idx0] - tmpClothVtx[idx];
		const Vector4 v = tmpClothVtx[idx1] - tmpClothVtx[idx];
		const Vector4 n = (1.f - skinMap.s - skinMap.t) * normals[idx] + skinMap.s * normals[idx0] + skinMap.t * normals[idx1];
		return tmpClothVtx[idx] + skinMap.s * u + skinMap.t * v + skinMap.h * n;
	}
	else
		return tmpClothVtx[skinMap.iMap];
}

ILINE void CClothPiece::SetBlendWeight(float weight)
{
	weight = max(0.f, min(1.f, weight));
}

class CAttachmentVCLOTH : public IAttachmentSkin, public SAttachmentBase
{
public:

	CAttachmentVCLOTH()
	{
		m_clothCacheKey = -1;
		for (uint32 j = 0; j < 2; ++j) m_pRenderMeshsSW[j] = NULL;
		memset(m_arrSkinningRendererData, 0, sizeof(m_arrSkinningRendererData));
		m_pRenderSkin = 0;
	};

	virtual ~CAttachmentVCLOTH();

	virtual void AddRef() override
	{
		++m_nRefCounter;
	}

	virtual void Release() override
	{
		if (--m_nRefCounter == 0)
			delete this;
	}

	virtual uint32             GetType() const override                               { return CA_VCLOTH; }
	virtual uint32             SetJointName(const char* szJointName) override         { return 0; }

	virtual const char*        GetName() const override                               { return m_strSocketName; };
	virtual uint32             GetNameCRC() const override                            { return m_nSocketCRC32; }
	virtual uint32             ReName(const char* strSocketName, uint32 crc) override { m_strSocketName.clear(); m_strSocketName = strSocketName; m_nSocketCRC32 = crc; return 1; };

	virtual uint32             GetFlags() const override                              { return m_AttFlags | FLAGS_ATTACH_MERGED_FOR_SHADOWS; } // disable merging for vcloth shadows
	virtual void               SetFlags(uint32 flags) override                        { m_AttFlags = flags; }

	void                       ReleaseRenderRemapTablePair();
	void                       ReleaseSimRemapTablePair();
	void                       ReleaseSoftwareRenderMeshes();

	virtual uint32             Immediate_AddBinding(IAttachmentObject* pModel, ISkin* pISkinRender = 0, uint32 nLoadingFlags = 0) override;
	virtual void               Immediate_ClearBinding(uint32 nLoadingFlags = 0) override;
	virtual uint32             Immediate_SwapBinding(IAttachment* pNewAttachment) override;

	uint32                     AddSimBinding(const ISkin& pISkinRender, uint32 nLoadingFlags = 0);
	virtual IAttachmentObject* GetIAttachmentObject() const override { return m_pIAttachmentObject; }
	virtual IAttachmentSkin*   GetIAttachmentSkin() override         { return this; }

	virtual void               HideAttachment(uint32 x) override;
	virtual uint32             IsAttachmentHidden() const override             { return m_AttFlags & FLAGS_ATTACH_HIDE_MAIN_PASS; }
	virtual void               HideInRecursion(uint32 x) override;
	virtual uint32             IsAttachmentHiddenInRecursion() const override  { return m_AttFlags & FLAGS_ATTACH_HIDE_RECURSION; }
	virtual void               HideInShadow(uint32 x) override;
	virtual uint32             IsAttachmentHiddenInShadow() const override     { return m_AttFlags & FLAGS_ATTACH_HIDE_SHADOW_PASS; }

	virtual void               SetAttAbsoluteDefault(const QuatT& qt) override {};
	virtual void               SetAttRelativeDefault(const QuatT& qt) override {};
	virtual const QuatT& GetAttAbsoluteDefault() const override          { return g_IdentityQuatT; };
	virtual const QuatT& GetAttRelativeDefault() const override          { return g_IdentityQuatT; };

	virtual const QuatT& GetAttModelRelative() const override            { return g_IdentityQuatT; };//this is relative to the animated bone
	virtual const QuatTS GetAttWorldAbsolute() const override;
	virtual const QuatT& GetAdditionalTransformation() const override    { return g_IdentityQuatT; }
	virtual void         UpdateAttModelRelative() override;

	virtual uint32       GetJointID() const override                                                                      { return -1; };
	virtual void         AlignJointAttachment() override                                                                  {};

	virtual void         SetVClothParams(const SVClothParams& params) override                                            { AddClothParams(params); }
	virtual void         PostUpdateSimulationParams(bool bAttachmentSortingRequired, const char* pJointName = 0) override {};

	virtual void         Serialize(TSerialize ser) override;
	virtual size_t       SizeOfThis() const override;
	virtual void         GetMemoryUsage(ICrySizer* pSizer) const override;
	virtual void         TriggerMeshStreaming(uint32 nDesiredRenderLOD, const SRenderingPassInfo& passInfo);

	void                 DrawAttachment(SRendParams& rParams, const SRenderingPassInfo& passInfo, const Matrix34& rWorldMat34, f32 fZoomFactor = 1);
	void                 RecreateDefaultSkeleton(CCharInstance* pInstanceSkel, uint32 nLoadingFlags);
	void                 UpdateRemapTable();
	bool                 EnsureRemapTableIsValid();

	void                 ComputeClothCacheKey();
	uint64               GetClothCacheKey() const { return m_clothCacheKey; };
	void                 AddClothParams(const SVClothParams& clothParams);
	bool                 InitializeCloth();
	const SVClothParams& GetClothParams();

	// Vertex Transformation
public:
	SSkinningData*          GetVertexTransformationData(const bool bVertexAnimation, uint8 nRenderLOD, const SRenderingPassInfo& passInfo);
	_smart_ptr<IRenderMesh> CreateVertexAnimationRenderMesh(uint lod, uint id);

#ifdef EDITOR_PCDEBUGCODE
	void DrawWireframeStatic(const Matrix34& m34, int nLOD, uint32 color);
#endif

	virtual IVertexAnimation* GetIVertexAnimation() override { return &m_vertexAnimation; }
	virtual ISkin*            GetISkin() override            { return m_pRenderSkin; };
	virtual float             GetExtent(EGeomForm eForm) override;
	virtual void              GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const override;
	virtual SMeshLodInfo      ComputeGeometricMean() const override;

	int                       GetGuid() const;

	DynArray<JointIdType>   m_arrRemapTable;    //!< maps skin's bone indices to skeleton's bone indices
	DynArray<JointIdType>   m_arrSimRemapTable; //!< maps skin's bone indices to skeleton's bone indices
	_smart_ptr<CSkin>       m_pRenderSkin;
	_smart_ptr<CSkin>       m_pSimSkin;
	_smart_ptr<IRenderMesh> m_pRenderMeshsSW[2];
	string                  m_sSoftwareMeshName;
	CVertexData             m_vertexData;
	CVertexAnimation        m_vertexAnimation;
	CClothPiece             m_clothPiece;
	uint64                  m_clothCacheKey;

	// history for skinning data, needed for motion blur
	struct { SSkinningData* pSkinningData; int nFrameID; } m_arrSkinningRendererData[3]; // triple buffered for motion blur

private:
	// functions to keep in sync ref counts on skins and cleanup of remap tables
	void ReleaseRenderSkin();
	void ReleaseSimSkin();
};
