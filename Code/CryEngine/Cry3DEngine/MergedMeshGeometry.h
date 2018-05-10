// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _PROCEDURALVEGETATION_GEOMETRY_
#define _PROCEDURALVEGETATION_GEOMETRY_
#include "MergedMeshRenderNode.h"
#include <Cry3DEngine/IIndexedMesh.h>

#define c_MergedMeshesExtent     (18.f)
#define c_MergedMeshChunkVersion (0xcafebab6)

#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
	#include <xmmintrin.h>
	#if CRY_PLATFORM_SSE3 || CRY_PLATFORM_SSE4 || CRY_PLATFORM_AVX
		#include <pmmintrin.h> // SSE3
	#endif
	#if CRY_PLATFORM_SSE4 || CRY_PLATFORM_AVX
		#include <smmintrin.h> // SSE4.1
		#include <nmmintrin.h> // SSE4.2
	#endif
	#if CRY_PLATFORM_AVX || CRY_PLATFORM_F16C
		#include <immintrin.h> // AVX, F16C

		#pragma warning(disable:4700)
		#if CRY_COMPILER_MSVC
			#include <intrin.h>
		#endif
	#endif
typedef      CRY_ALIGN (16) Vec3 aVec3;
typedef      CRY_ALIGN (16) Quat aQuat;
#else
typedef Vec3 aVec3;
typedef Quat aQuat;
#endif

enum MMRM_CULL_FLAGS
{
	MMRM_CULL_FRUSTUM             = 0x1
	, MMRM_CULL_DISTANCE          = 0x2
	, MMRM_CULL_LOD               = 0x4
	, MMRM_LOD_SHIFT              = 3
	, MMRM_SAMPLE_REDUCTION_SHIFT = 5
};

struct SMergedMeshInstanceCompressed
{
	uint16 pos_x;
	uint16 pos_y;
	uint16 pos_z;
	uint8  scale;
	int8   rot[4];
	AUTO_STRUCT_INFO_LOCAL;
};

struct SMergedMeshSectorChunk
{
	uint32 ver, i, j, k;
	uint32 m_StatInstGroupID;
	uint32 m_nSamples;
	AUTO_STRUCT_INFO_LOCAL;
};

struct Vec3fixed16
{
public:
	int16 x, y, z;

	Vec3fixed16() {}
	Vec3fixed16(const Vec3& src)
	{
		float _x(clamp_tpl(src.x, -127.f, 128.f) * (float)(1 << 8));
		float _y(clamp_tpl(src.y, -127.f, 128.f) * (float)(1 << 8));
		float _z(clamp_tpl(src.z, -127.f, 128.f) * (float)(1 << 8));
		x = static_cast<int16>(_x);
		y = static_cast<int16>(_y);
		z = static_cast<int16>(_z);
	}
	Vec3fixed16& operator=(const Vec3& src)
	{
		float _x(clamp_tpl(src.x, -127.f, 128.f) * (float)(1 << 8));
		float _y(clamp_tpl(src.y, -127.f, 128.f) * (float)(1 << 8));
		float _z(clamp_tpl(src.z, -127.f, 128.f) * (float)(1 << 8));
		x = static_cast<int16>(_x);
		y = static_cast<int16>(_y);
		z = static_cast<int16>(_z);
		return *this;
	}
	Vec3 ToVec3() const
	{
		int _x(static_cast<int>(x));
		int _y(static_cast<int>(y));
		int _z(static_cast<int>(z));
		Vec3 r;
		r.x = static_cast<int16>(_x / (1 << 8));
		r.y = static_cast<int16>(_y / (1 << 8));
		r.z = static_cast<int16>(_z / (1 << 8));
		return r;
	}
};

#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
typedef Vec3fixed16 Vec3half;
struct SMMRMBoneMapping
{
	CRY_ALIGN(16) float weights[4]; //weights for every bone (stored as floats to prevent lhs)
	ColorB boneIds;                 //boneIDs per render-batch

	SMMRMBoneMapping& operator=(const SMeshBoneMapping_uint8& mbm)
	{
		boneIds.r = mbm.boneIds[0];
		boneIds.g = mbm.boneIds[1];
		boneIds.b = mbm.boneIds[2];
		boneIds.a = mbm.boneIds[3];
		for (size_t i = 0; i < 4; ++i) weights[i] = static_cast<float>(mbm.weights[i]);
		for (size_t i = 0; i < 4; ++i) weights[i] *= 1.f / 255.f;
		return *this;
	}

	bool operator!=(const SMMRMBoneMapping& mbm) const
	{
		if (boneIds != mbm.boneIds) return true;
		for (size_t i = 0; i < 4; ++i)
			if (weights[i] != mbm.weights[i])
				return true;
		return false;
	}
};
#else
typedef Vec3fixed16            Vec3half;
typedef SMeshBoneMapping_uint8 SMMRMBoneMapping;
#endif

struct SProcVegSample
{
	Vec3   pos;
	Quat   q;
	uint32 InstGroupId;
	uint8  scale;
};

struct CRY_ALIGN(16) SSampleDensity
{
	union
	{
		uint8 density_u8[16];
	};
};

struct CRY_ALIGN(16) SMMRMDeformVertex
{
	Vec3 pos[2], vel;
};

struct SMMRMDeformConstraint
{
	enum
	{
		DC_POSITIONAL = 0,
		DC_EDGE,
		DC_BENDING,
	};
	// Strength of constraint (valid range 0..1, 0 meaning no enforcement, 1 full enforcement)
	// Values above or below the valid range are possible (and sometimes desirable) but make
	// no mathematical sense.
	float k;
	union
	{
		// limits the edge length between two vertices at index position[0/1]
		struct
		{
			float   edge_distance;
			vtx_idx edge[2];
		};
		// limits the bending of the bending triangle spanned between the three
		// bending vertices. the initial displacement is the height of the second
		// vertex in respect to the longest edge of the triangle.
		struct
		{
			float   displacement;
			vtx_idx bending[3];
		};
	};
	// Type depicting data stored in the union
	uint8 type;
};

struct CRY_ALIGN(16) SMMRMDeform
{
	SMMRMDeformConstraint* constraints;
	size_t nconstraints;
	float* invmass;
	Vec3* initial;
	vtx_idx* mapping;
	size_t nvertices;
	SMMRMDeform()
		: constraints()
		  , nconstraints()
		  , invmass()
		  , initial()
		  , mapping()
		  , nvertices()
	{
	}
	~SMMRMDeform()
	{
		if (constraints) CryModuleMemalignFree(constraints);
		if (invmass) CryModuleMemalignFree(invmass);
		if (initial) CryModuleMemalignFree(initial);
		if (mapping) CryModuleMemalignFree(mapping);
	}
};

// Note: This struct NEEDs to be 64 large and 64 byte aligned
// Do not change unless absolutely necessary
#define SMMRMSkinVertex_ALIGN 64
#define SMMRMSkinVertex_SIZE  64
struct CRY_ALIGN(SMMRMSkinVertex_ALIGN) SMMRMSkinVertex
{
	float weights[4];
	Vec3 pos;
	Vec3 normal;
	Vec2 uv;
	UCol colour;
	SPipQTangents qt;
	uint8 boneIds[4];

	void SetWeights(const float* pSrcWeights)
	{
		memcpy(weights, pSrcWeights, sizeof(weights));
	}

	void SetWeights(const uint8* srcWeights)
	{
		for (int i = 0; i < 4; ++i)
		{
			weights[i] = srcWeights[i] / 255.0f;
		}
	}

	void SetBoneIds(const uint8* srcBoneIds)
	{
		for (int i = 0; i < 4; ++i)
		{
			boneIds[i] = srcBoneIds[i];
		}
	}

	void SetBoneIds(const ColorB& srcBoneIds)
	{
		boneIds[0] = srcBoneIds.r;
		boneIds[1] = srcBoneIds.g;
		boneIds[2] = srcBoneIds.b;
		boneIds[3] = srcBoneIds.a;
	}

};
static_assert(sizeof(SMMRMSkinVertex) == SMMRMSkinVertex_SIZE, "Invalid type size!");

struct CRY_ALIGN(16) SMMRMChunk
{
	int matId;
	uint32 nvertices, nvertices_alloc;
	uint32 nindices, nindices_alloc;
	SVF_P3F_C4B_T2F* general;
	SPipQTangents* qtangents;
	SMMRMBoneMapping* weights;
	Vec3* normals;
	vtx_idx* indices;
	SMMRMSkinVertex* skin_vertices;

	SMMRMChunk(int _matId)
		: matId(_matId)
		  , nvertices()
		  , nvertices_alloc()
		  , nindices()
		  , nindices_alloc()
		  , general()
		  , qtangents()
		  , normals()
		  , weights()
		  , indices()
		  , skin_vertices()
	{
	}
	~SMMRMChunk()
	{
		if (general) CryModuleMemalignFree(general);
		if (qtangents) CryModuleMemalignFree(qtangents);
		if (weights) CryModuleMemalignFree(weights);
		if (normals) CryModuleMemalignFree(normals);
		if (indices) CryModuleMemalignFree(indices);
		if (skin_vertices) CryModuleMemalignFree(skin_vertices);
	}
	uint32 Size() const
	{
		uint32 size = 0u;
		size += sizeof(SVF_P3F_C4B_T2F) * nvertices_alloc;
		size += sizeof(SPipTangents) * nvertices_alloc;
		size += weights ? sizeof(SMMRMBoneMapping) * nvertices_alloc : 0;
		size += normals ? sizeof(Vec3) * nvertices_alloc : 0;
		size += sizeof(indices[0]) * nindices_alloc;
		size += (skin_vertices) ? sizeof(SMMRMSkinVertex) * nvertices_alloc : 0;
		return size;
	}
};

struct SMMRMSpineVtxBase
{
#if MMRM_SIMULATION_USES_FP32
	Vec3     pt;
	Vec3     vel;
#else
	Vec3half pt;
	Vec3half vel;
#endif
};

struct CRY_ALIGN(16) SMMRMSpineVtx
{
	Vec3 pt;
	float len, h;
};

struct CRY_ALIGN(16) SMMRMSpineInfo
{
	size_t nSpineVtx;
	float fSpineLen;
};

struct CRY_ALIGN(16) SMMRMGeometry
{
	enum State
	{
		CREATED = 0,
		PREPARED,
		NO_RENDERMESH,
		NO_STATINSTGROUP
	};

	AABB aabb;
	SMMRMChunk* pChunks[MAX_STATOBJ_LODS_NUM];
	size_t numChunks[MAX_STATOBJ_LODS_NUM];
	size_t numIdx, numVtx; // numIdx & numVtx reflect counts for first lod
	size_t numSpineVtx, numSpines, maxSpinesPerVtx;
	SMMRMSpineVtx* pSpineVtx;
	SMMRMSpineInfo* pSpineInfo;
	SMMRMDeform* deform;
	State state;
	union
	{
		uint32    srcGroupId;
		CStatObj* srcObj;
	};
	size_t refCount;
	JobManager::SJobState geomPrepareState;
	const bool is_obj : 1;

	SMMRMGeometry(CStatObj * obj)
		: aabb(AABB::RESET)
		  , numIdx()
		  , numVtx()
		  , numSpineVtx()
		  , numSpines()
		  , maxSpinesPerVtx()
		  , refCount()
		  , pSpineVtx()
		  , pSpineInfo()
		  , deform()
		  , state(CREATED)
		  , srcObj(obj)
		  , geomPrepareState()
		  , is_obj(true)
	{
		memset(pChunks, 0, sizeof(pChunks));
		memset(numChunks, 0, sizeof(numChunks));
		aabb.max = aabb.min = Vec3(0, 0, 0);
	};

	SMMRMGeometry(uint32 groupId)
		: srcGroupId(groupId)
		  , aabb(AABB::RESET)
		  , numIdx()
		  , numVtx()
		  , numSpineVtx()
		  , numSpines()
		  , maxSpinesPerVtx()
		  , refCount()
		  , pSpineVtx()
		  , pSpineInfo()
		  , deform()
		  , state(CREATED)
		  , geomPrepareState()
		  , is_obj(false)
	{
		memset(pChunks, 0, sizeof(pChunks));
		memset(numChunks, 0, sizeof(numChunks));
	}

	~SMMRMGeometry()
	{
		for (size_t i = 0; i < MAX_STATOBJ_LODS_NUM; ++i)
			if (pChunks[i])
			{
				for (size_t j = 0; j < numChunks[i]; ++j)
					pChunks[i][j].~SMMRMChunk();
				CryModuleMemalignFree(pChunks[i]);
			}
		if (pSpineVtx) CryModuleMemalignFree(pSpineVtx);
		if (pSpineInfo) CryModuleMemalignFree(pSpineInfo);
		delete deform;
	}

	uint32 Size() const
	{
		uint32 size = 0u;
		for (size_t i = 0; i < MAX_STATOBJ_LODS_NUM; ++i)
			for (size_t j = 0; j < numChunks[i]; ++j)
			{
				size += pChunks[i][j].Size();
				size += sizeof(SMMRMChunk);
			}
		for (size_t i = 0; i < numSpines; ++i)
			size += sizeof(SMMRMSpineVtx) * pSpineInfo[i].nSpineVtx;
		size += numSpines * sizeof(SMMRMSpineInfo);
		if (deform)
		{
			size += deform->nconstraints * sizeof(SMMRMDeformConstraint);
			size += deform->nvertices * sizeof(uint16);
		}
		return size;
	}
};

struct CRY_ALIGN(16) SMMRMInstance
{
	uint32 pos_x : 16;
	uint32 pos_y : 16;
	uint32 pos_z : 16;
	int32 qx : 8;
	int32 qy : 8;
	int32 qz : 8;
	int32 qw : 8;
	uint8 scale;
	int8 lastLod;
};

struct SMMRMVisibleChunk
{
	uint32 indices;
	uint32 vertices;
	int32  matId;
};

struct SMMRMProjectile
{
	Vec3  pos[2];
	Vec3  dir;
	float r;
};

struct SMMRMPhysConfig
{
	float kH;
	float fDamping;
	float variance;
	float airResistance;
	float airFrequency;
	float airModulation;
	int   max_iter;

	SMMRMPhysConfig()
		: kH(0.5f)
		, fDamping(1.f)
		, variance(0.1f)
		, airResistance(1.f)
		, airFrequency(0.f)
		, airModulation(0.f)
		, max_iter(3)
	{}

	void Update(const StatInstGroup* srcGroup, const float dampModifier = 1.f)
	{
		kH = srcGroup->fStiffness;
		fDamping = srcGroup->fDamping * dampModifier;
		variance = srcGroup->fVariance;
		airResistance = srcGroup->fAirResistance;
	}

	void Update(const SMMRMGeometry* geom)
	{
		if (NULL == geom->srcObj)
		{
			return;
		}
		const char* szprops = geom->srcObj->GetProperties(), * pval = NULL;
		if (!szprops)
			return;
		if (pval = strstr(szprops, "mergedmesh_deform_stiffness"))
		{
			for (pval += strlen("mergedmesh_deform_stiffness"); * pval && !isdigit((unsigned char)*pval); pval++);
			if (pval)
				kH = (float)atof(pval);
		}
		if (pval = strstr(szprops, "mergedmesh_deform_damping"))
		{
			for (pval += strlen("mergedmesh_deform_damping"); * pval && !isdigit((unsigned char)*pval); pval++);
			if (pval)
				fDamping = (float)atof(pval);
		}
		if (pval = strstr(szprops, "mergedmesh_deform_variance"))
		{
			for (pval += strlen("mergedmesh_deform_variance"); * pval && !isdigit((unsigned char)*pval); pval++);
			if (pval)
				variance = (float)atof(pval);
		}
		if (pval = strstr(szprops, "mergedmesh_deform_air_resistance"))
		{
			for (pval += strlen("mergedmesh_deform_air_resistance"); * pval && !isdigit((unsigned char)*pval); pval++);
			if (pval)
				airResistance = (float)atof(pval);
		}
		if (pval = strstr(szprops, "mergedmesh_deform_air_frequency"))
		{
			for (pval += strlen("mergedmesh_deform_air_frequency"); * pval && !isdigit((unsigned char)*pval); pval++);
			if (pval)
				airFrequency = (float)atof(pval);
		}
		if (pval = strstr(szprops, "mergedmesh_deform_air_modulation"))
		{
			for (pval += strlen("mergedmesh_deform_air_modulation"); * pval && !isdigit((unsigned char)*pval); pval++);
			if (pval)
				airModulation = (float)atof(pval);
		}
		if (pval = strstr(szprops, "mergedmesh_deform_max_iter"))
		{
			for (pval += strlen("mergedmesh_deform_max_iter"); * pval && !isdigit((unsigned char)*pval); pval++);
			if (pval)
				max_iter = atoi(pval);
			max_iter = min(max(max_iter, 1), 12);
		}
	}
};

struct CRY_ALIGN(128) SMMRMGroupHeader
{
	SMMRMInstance* instances;
	SMMRMSpineVtxBase* spines;
	SMMRMDeformVertex* deform_vertices;
	SMMRMGeometry* procGeom;
	SMMRMVisibleChunk* visibleChunks;
	CMergedMeshInstanceProxy** proxies;
	SMMRMPhysConfig physConfig;
	uint32 instGroupId;
	uint32 numSamples;
	uint32 numSamplesAlloc;
	uint32 numSamplesVisible;
	uint32 numVisbleChunks;
	float maxViewDistance;
	float lodRationNorm;
	bool specMismatch : 1;
	bool splitGroup : 1;
	bool is_dynamic : 1;
#if MMRM_USE_BOUNDS_CHECK
	volatile int debugLock;
#endif

	SMMRMGroupHeader()
		: instances()
		  , spines()
		  , deform_vertices()
		  , procGeom()
		  , visibleChunks()
		  , proxies()
		  , physConfig()
		  , instGroupId()
		  , numSamples()
		  , numSamplesAlloc()
		  , numSamplesVisible()
		  , numVisbleChunks()
		  , maxViewDistance()
		  , lodRationNorm()
		  , specMismatch()
		  , splitGroup()
		  , is_dynamic(false)
#if MMRM_USE_BOUNDS_CHECK
		  , debugLock()
#endif
	{
	}
	~SMMRMGroupHeader();

	void CullInstances(CCamera * cam, Vec3 * origin, Vec3 * rotationOrigin, float zRotation, int flags);

};

struct CRY_ALIGN(16) SMergedRMChunk
{
	uint32 ioff;
	uint32 voff;
	uint32 icnt;
	uint32 vcnt;
	uint32 matId;

	SMergedRMChunk()
		: ioff()
		  , voff()
		  , icnt()
		  , vcnt()
		  , matId()
	{
	}
	~SMergedRMChunk()
	{
		new(this)SMergedRMChunk;
	};
};

struct SMMRMUpdateContext
{
	SMMRMGroupHeader*           group;
	std::vector<SMergedRMChunk> chunks;
	SVF_P3S_C4B_T2S*            general;
	SPipTangents*               tangents;
	Vec3f16*                    normals;
	vtx_idx*                    idxBuf;
	volatile int*               updateFlag;
	primitives::sphere*         colliders;
	int                         ncolliders;
	SMMRMProjectile*            projectiles;
	int                         nprojectiles;
	int                         max_iter;
	float                       dt, dtscale, abstime;
	float                       zRotation;
	Vec3                        rotationOrigin;
	Vec3                        _max, _min;
	Vec3*                       wind;
	int                         use_spines;
	int                         frame_count;
#if MMRM_USE_BOUNDS_CHECK
	SVF_P3S_C4B_T2S*            general_end;
	SPipTangents*               tangents_end;
	vtx_idx*                    idx_end;
#endif

	SMMRMUpdateContext()
		: group()
		, chunks()
		, general()
		, tangents()
		, normals()
		, idxBuf()
		, updateFlag()
		, colliders()
		, ncolliders()
		, projectiles()
		, nprojectiles()
		, max_iter()
		, dt()
		, dtscale(1.f)
		, abstime()
		, zRotation()
		, rotationOrigin()
		, _max()
		, _min()
		, wind()
		, use_spines(1)
		, frame_count()
#if MMRM_USE_BOUNDS_CHECK
		, general_end()
		, tangents_end()
		, idx_end()
#endif
	{}
	~SMMRMUpdateContext()
	{
	}

	void MergeInstanceMeshesSpines(CCamera*, int);
	void MergeInstanceMeshesDeform(CCamera*, int);
};

struct SMMRM
{
	std::vector<_smart_ptr<IRenderMesh>> rms;
	IMaterial*                           mat;
	uint32                               vertices;
	uint32                               indices;
	uint32                               chunks;
	std::vector<SMMRMUpdateContext>      updates;
	bool hasNormals   : 1;
	bool hasTangents  : 1;
	bool hasQTangents : 1;

	SMMRM()
		: rms()
		, mat()
		, chunks()
		, hasNormals()
		, hasTangents()
		, hasQTangents()
	{}
};

inline void CompressQuat(const Quat& q, SMMRMInstance& i)
{
	float qx(q.v.x * (1 << 7)), qy(q.v.y * (1 << 7)), qz(q.v.z * (1 << 7)), qw(q.w * (1 << 7));
	i.qx = static_cast<int>(qx);
	i.qy = static_cast<int>(qy);
	i.qz = static_cast<int>(qz);
	i.qw = static_cast<int>(qw);
}

inline void DecompressQuat(Quat& q, const SMMRMInstance& i)
{
	int qx(i.qx), qy(i.qy), qz(i.qz), qw(i.qw);
	q.v.x = qx / (float)(1 << 7);
	q.v.y = qy / (float)(1 << 7);
	q.v.z = qz / (float)(1 << 7);
	q.w = qw / (float)(1 << 7);
	q.Normalize();
}

inline void ConvertInstanceAbsolute(Vec3& abs, const uint16 (&pos)[3], const Vec3& origin, const Vec3& rotationOrigin, float zRotation, const float fExtents)
{
	int ax(pos[0]), ay(pos[1]), az(pos[2]);
	abs.x = origin.x + (ax / (float)0xffffu) * fExtents;
	abs.y = origin.y + (ay / (float)0xffffu) * fExtents;
	abs.z = origin.z + (az / (float)0xffffu) * fExtents;

	if (zRotation != 0.0f)
	{
		// Change the position to be relative to the rotation origin
		abs -= rotationOrigin;

		// Apply the rotation and translate it back out to its world space position
		Matrix34 mat;
		mat.SetRotationZ(zRotation, rotationOrigin);
		abs = mat * abs;
	}
}

inline Vec3 ConvertInstanceAbsolute(const uint16 (&pos)[3], const Vec3& origin, const Vec3& rotationOrigin, float zRotation, const float fExtents)
{
	Vec3 abs;
	ConvertInstanceAbsolute(abs, pos, origin, rotationOrigin, zRotation, fExtents);
	return abs;
}

inline void ConvertInstanceAbsolute(Vec3& abs, const SMMRMInstance& i, const Vec3& origin, const Vec3& rotationOrigin, float zRotation, const float fExtents)
{
	int ax(i.pos_x), ay(i.pos_y), az(i.pos_z);
	abs.x = origin.x + (ax / (float)0xffffu) * fExtents;
	abs.y = origin.y + (ay / (float)0xffffu) * fExtents;
	abs.z = origin.z + (az / (float)0xffffu) * fExtents;

	if (zRotation != 0.0f)
	{
		// Change the position to be relative to the rotation origin
		abs -= rotationOrigin;

		// Apply the rotation and translate it back out to its world space position
		Matrix34 mat;
		mat.SetRotationZ(zRotation, rotationOrigin);
		abs = mat * abs;
	}
}

inline Vec3 ConvertInstanceAbsolute(const SMMRMInstance& i, const Vec3& origin, const Vec3& rotationOrigin, float zRotation, const float fExtents)
{
	Vec3 abs;
	ConvertInstanceAbsolute(abs, i, origin, rotationOrigin, zRotation, fExtents);
	return abs;
}

inline void ConvertInstanceRelative(SMMRMInstance& i, const Vec3& abs, const Vec3& origin, const float fExtentsRec)
{
	i.pos_x = (uint16)((abs.x - origin.x) * fExtentsRec * (float)0xffffu);
	i.pos_y = (uint16)((abs.y - origin.y) * fExtentsRec * (float)0xffffu);
	i.pos_z = (uint16)((abs.z - origin.z) * fExtentsRec * (float)0xffffu);
}

inline void DecompressInstance(const SMMRMInstance& i, const Vec3& origin, const Vec3& rotationOrigin, float zRotation, const float Extents, Vec3& pos)
{
	ConvertInstanceAbsolute(pos, i, origin, rotationOrigin, zRotation, Extents);
}

inline void DecompressInstance(const SMMRMInstance& i, const Vec3& origin, const Vec3& rotationOrigin, float zRotation, const float Extents, Vec3& pos, Quat& rot)
{
	ConvertInstanceAbsolute(pos, i, origin, rotationOrigin, zRotation, Extents);
	DecompressQuat(rot, i);
}

inline void DecompressInstance(const SMMRMInstance& i, const Vec3& origin, const Vec3& rotationOrigin, float zRotation, const float Extents, Vec3& pos, float& scale)
{
	int iscale(i.scale);
	ConvertInstanceAbsolute(pos, i, origin, rotationOrigin, zRotation, Extents);
	scale = (1.f / VEGETATION_CONV_FACTOR) * iscale;
}

inline void DecompressInstance(const SMMRMInstance& i, const aVec3& origin, const Vec3& rotationOrigin, float zRotation, const float Extents, aVec3& pos, aQuat& rot, float& scale)
{
#if 0 && MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
	__m128 vorigin = _mm_loadu_ps(&origin.x);
	__m128i instance = _mm_load_si128((const __m128i*)&i);
	__m128i ipos = _mm_shuffle_epi8(instance, _mm_set_epi32(0xffffffff, 0xffff0504, 0xffff0302, 0xffff0100));
	__m128 pcvt = _mm_set1_ps(Extents / (float)0xffff);
	__m128 rcvt = _mm_set1_ps(1.f / (float)(1 << 7));
	__m128 cpos = _mm_add_ps(_mm_shuffle_ps(vorigin, vorigin, _MM_SHUFFLE(2, 2, 1, 0)), _mm_mul_ps(pcvt, _mm_cvtepi32_ps(ipos)));
	__m128i irot = _mm_shuffle_epi8(instance, _mm_set_epi32(0x09ffffff, 0x08ffffff, 0x07ffffff, 0x06ffffff));
	__m128 crot = _mm_mul_ps(rcvt, _mm_cvtepi32_ps(_mm_srai_epi32(irot, 24)));
	__m128 len = _mm_rsqrt_ps(_mm_dp_ps(crot, crot, 0xff));
	__m128 cnrot = _mm_mul_ps(crot, len);

	_mm_store_ps(&pos.x, cpos);
	_mm_store_ps(&rot.v.x, cnrot);

	scale = (1.f / VEGETATION_CONV_FACTOR) * i.scale;
#else
	int iscale(i.scale);
	ConvertInstanceAbsolute(pos, i, origin, rotationOrigin, zRotation, Extents);
	DecompressQuat(rot, i);
	scale = (1.f / VEGETATION_CONV_FACTOR) * iscale;
#endif
}

inline Matrix34 CreateRotationQ(const Quat& q, const Vec3& t)
{
	Matrix34 r = Matrix34(q);
	r.SetTranslation(t);
	return r;
}

extern Vec3        SampleWind(const Vec3& pos, const Vec3 (&samples)[MMRM_WIND_DIM][MMRM_WIND_DIM][MMRM_WIND_DIM]);
extern inline Vec3 SampleWind(const Vec3& pos, const Vec3* samples);
#endif
