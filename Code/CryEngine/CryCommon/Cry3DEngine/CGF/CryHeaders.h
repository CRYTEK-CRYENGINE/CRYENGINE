// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include <CryCore/BaseTypes.h>
#include <CryMath/Cry_Math.h>

#define MAX_STATOBJ_LODS_NUM 6
#define MAX_SUB_MATERIALS    128

#include <CryCore/CryEndian.h>

//! Chunk type must fit into uint16.
enum ChunkTypes
{
	ChunkType_ANY  = 0,

	ChunkType_Mesh = 0x1000,  //!< Was 0xCCCC0000 in chunk files with versions <= 0x745.
	ChunkType_Helper,
	ChunkType_VertAnim,
	ChunkType_BoneAnim,
	ChunkType_GeomNameList, //!< obsolete
	ChunkType_BoneNameList,
	ChunkType_MtlList,    //!< Obsolete.
	ChunkType_MRM,        //!< Obsolete.
	ChunkType_SceneProps, //!< Obsolete.
	ChunkType_Light,      //!< Obsolete.
	ChunkType_PatchMesh,  //!< Not implemented.
	ChunkType_Node,
	ChunkType_Mtl,          //!< Obsolete.
	ChunkType_Controller,
	ChunkType_Timing,
	ChunkType_BoneMesh,
	ChunkType_BoneLightBinding,                    //!< Obsolete. describes the lights binded to bones.
	ChunkType_MeshMorphTarget,                     //!< Describes a morph target of a mesh chunk.
	ChunkType_BoneInitialPos,                      //!< Describes the initial position (4x3 matrix) of each bone; just an array of 4x3 matrices.
	ChunkType_SourceInfo,                          //!< Describes the source from which the cgf was exported: source max file, machine and user.
	ChunkType_MtlName,                             //!< Material name.
	ChunkType_ExportFlags,                         //!< Special export flags.
	ChunkType_DataStream,                          //!< Stream data.
	ChunkType_MeshSubsets,                         //!< Array of mesh subsets.
	ChunkType_MeshPhysicsData,                     //!< Physicalized mesh data.
	ChunkType_ImportSettings,                      //!< Settings used to import the asset, in JSON format.
	ChunkType_VCloth,                              //!< Describes per-mesh data for VCloth animation system.
	ChunkType_AssetMetadata,                       //!< Asset Manager metadata.

	// These are the new compiled chunks for characters.
	ChunkType_CompiledBones = 0x2000,  //!< Was 0xACDC0000 in chunk files with versions <= 0x745.
	ChunkType_CompiledPhysicalBones,
	ChunkType_CompiledMorphTargets,
	ChunkType_CompiledPhysicalProxies,
	ChunkType_CompiledIntFaces,
	ChunkType_CompiledIntSkinVertices,
	ChunkType_CompiledExt2IntMap,

	ChunkType_BreakablePhysics = 0x3000, //!< Was 0xAAFC0000 in chunk files with versions <= 0x745.
	ChunkType_FaceMap,                   //!< Obsolete.
	ChunkType_MotionParameters,
	ChunkType_FootPlantInfo,              //!< Obsolete.
	ChunkType_BonesBoxes,
	ChunkType_FoliageInfo,
	ChunkType_Timestamp,
	ChunkType_GlobalAnimationHeaderCAF,
	ChunkType_GlobalAnimationHeaderAIM,
	ChunkType_BspTreeData
};

enum ECgfStreamType
{
	CGF_STREAM_POSITIONS,
	CGF_STREAM_NORMALS,
	CGF_STREAM_TEXCOORDS,
	CGF_STREAM_COLORS,
	CGF_STREAM_COLORS2,
	CGF_STREAM_INDICES,
	CGF_STREAM_TANGENTS,
	CGF_STREAM_DUMMY0_,  //!< Used to be CGF_STREAM_SHCOEFFS, dummy is needed to keep existing assets loadable.
	CGF_STREAM_DUMMY1_,  //!< Used to be CGF_STREAM_SHAPEDEFORMATION, dummy is needed to keep existing assets loadable.
	CGF_STREAM_BONEMAPPING,
	CGF_STREAM_FACEMAP,
	CGF_STREAM_VERT_MATS,
	CGF_STREAM_QTANGENTS,
	CGF_STREAM_SKINDATA,
	CGF_STREAM_DUMMY2_,  //!< Used to be CGF_STREAM_PS3EDGEDATA, dummy is needed to keep existing assets loadable.
	CGF_STREAM_P3S_C4B_T2S,
};

//////////////////////////////////////////////////////////////////////////
enum EPhysicsGeomType
{
	PHYS_GEOM_TYPE_NONE          = -1,
	PHYS_GEOM_TYPE_DEFAULT       = 0x1000 + 0,
	PHYS_GEOM_TYPE_NO_COLLIDE    = 0x1000 + 1,
	PHYS_GEOM_TYPE_OBSTRUCT      = 0x1000 + 2,

	PHYS_GEOM_TYPE_DEFAULT_PROXY = 0x1000 + 0x100, //!< Default physicalization, but only proxy (NoDraw geometry).
};

struct CryVertex
{
	Vec3 p;     //!< Position vector.
	Vec3 n;     //!< Normal vector.

	AUTO_STRUCT_INFO;
};

struct CryFace
{
	int v0, v1, v2;   //!< Vertex indices.
	int MatID;        //!< Material ID.

	int& operator[](int i)       { return (&v0)[i]; }
	int  operator[](int i) const { return (&v0)[i]; }
	bool isDegenerate() const    { return v0 == v1 || v1 == v2 || v2 == v0; }

	AUTO_STRUCT_INFO;
};

struct CryUV
{
	float u, v;        //!< Texture coordinates.
	AUTO_STRUCT_INFO;
};

struct CrySkinVtx
{
	int      bVolumetric;
	int      idx[4];
	float    w[4];
	Matrix33 M;

	AUTO_STRUCT_INFO;
};

//////////////////////////////////////////////////////////////////////////
struct CryLink
{
	int   BoneID;
	Vec3  offset;
	float Blending;

	AUTO_STRUCT_INFO;
};

struct CryIRGB
{
	unsigned char r, g, b;
	AUTO_STRUCT_INFO;
};

struct NAME_ENTITY
{
	char name[64];
};

struct phys_geometry;

struct CryBonePhysics
{
	phys_geometry* pPhysGeom; //!< Id of a separate mesh for this bone, must NOT be in File Structures!
	// Additional joint parameters.
	int            flags;
	float          min[3], max[3];
	float          spring_angle[3];
	float          spring_tension[3];
	float          damping[3];
	float          framemtx[3][3];
};

//! The compatible between 32- and 64-bits structure.
struct CryBonePhysics_Comp
{
	int   nPhysGeom; //!< Id of a separate mesh for this bone.
	// additional joint parameters.
	int   flags;
	float min[3], max[3];
	float spring_angle[3];
	float spring_tension[3];
	float damping[3];
	float framemtx[3][3];

	AUTO_STRUCT_INFO;
};

#define __copy3(MEMBER) left.MEMBER[0] = right.MEMBER[0]; left.MEMBER[1] = right.MEMBER[1]; left.MEMBER[2] = right.MEMBER[2];
inline void CopyPhysInfo(CryBonePhysics& left, const CryBonePhysics_Comp& right)
{
	left.pPhysGeom = (phys_geometry*)(INT_PTR)right.nPhysGeom;
	left.flags = right.flags;
	__copy3(min);
	__copy3(max);
	__copy3(spring_angle);
	__copy3(spring_tension);
	__copy3(damping);
	__copy3(framemtx[0]);
	__copy3(framemtx[1]);
	__copy3(framemtx[2]);
}
inline void CopyPhysInfo(CryBonePhysics_Comp& left, const CryBonePhysics& right)
{
	left.nPhysGeom = (int)(INT_PTR)right.pPhysGeom;
	left.flags = right.flags;
	__copy3(min);
	__copy3(max);
	__copy3(spring_angle);
	__copy3(spring_tension);
	__copy3(damping);
	__copy3(framemtx[0]);
	__copy3(framemtx[1]);
	__copy3(framemtx[2]);
}
#undef __copy3

struct CryBoneDescData
{
	unsigned int m_nControllerID; //!< Unique id of bone (generated from bone name in the max).

	//! Physics info for different LODs.
	//! LOD 0 is the physics of alive body, LOD 1 is the physics of a dead body.
	CryBonePhysics m_PhysInfo[2];
	float          m_fMass;

	Matrix34f      m_DefaultW2B; //!< Intitalpose matrix World2Bone.
	Matrix34f      m_DefaultB2W; //!< Intitalpose matrix Bone2World.

	enum { kBoneNameMaxSize = 256, };
	char m_arrBoneName[CryBoneDescData::kBoneNameMaxSize];

	int  m_nLimbId; //!< Set by model state class.

	//! This bone parent is this[m_nOffsetParent], 0 if the bone is root. Normally this is <= 0.
	int m_nOffsetParent;

	//! The whole hierarchy of bones is kept in one big array that belongs to the ModelState.
	//! Each bone that has children has its own range of bone objects in that array,
	//! and this points to the beginning of that range and defines the number of bones.
	unsigned m_numChildren;

	//! Beginning of the subarray of children is at this[m_nOffsetChildren].
	//! This is 0 if there are no children.
	int m_nOffsetChildren;
};

struct CryBoneDescData_Comp
{
	unsigned int m_nControllerID; //!< Unique id of bone (generated from bone name).

	//! Physics info for different LOD.
	//! LOD 0 is the physics of alive body, LOD 1 is the physics of a dead body.
	CryBonePhysics_Comp m_PhysInfo[2];
	float               m_fMass;

	Matrix34f           m_DefaultW2B; //!< Intitalpose matrix World2Bone.
	Matrix34f           m_DefaultB2W; //!< Intitalpose matrix Bone2World.

	char                m_arrBoneName[256];

	int                 m_nLimbId; //!< Set by model state class.

	//! This bone parent is this[m_nOffsetParent], 0 if the bone is root. Normally this is <= 0.
	int m_nOffsetParent;

	//! The whole hierarchy of bones is kept in one big array that belongs to the ModelState.
	//! Each bone that has children has its own range of bone objects in that array,
	//! and this points to the beginning of that range and defines the number of bones.
	unsigned m_numChildren;

	//! Beginning of the subarray of children is at this[m_nOffsetChildren].
	//! This is 0 if there are no children.
	int m_nOffsetChildren;

	AUTO_STRUCT_INFO;
};

inline void CopyBoneDescData(CryBoneDescData_Comp& left, const CryBoneDescData& right)
{
	left.m_nControllerID = right.m_nControllerID;

	CopyPhysInfo(left.m_PhysInfo[0], right.m_PhysInfo[0]);
	CopyPhysInfo(left.m_PhysInfo[1], right.m_PhysInfo[1]);

	left.m_fMass = right.m_fMass;
	left.m_DefaultW2B = right.m_DefaultW2B;
	left.m_DefaultB2W = right.m_DefaultB2W;
	memcpy(left.m_arrBoneName, right.m_arrBoneName, sizeof(left.m_arrBoneName));
	left.m_nLimbId = right.m_nLimbId;
	left.m_nOffsetParent = right.m_nOffsetParent;
	left.m_numChildren = right.m_numChildren;
	left.m_nOffsetChildren = right.m_nOffsetChildren;
}

struct BONE_ENTITY
{
	int BoneID;
	int ParentID;
	int nChildren;

	//! Id of controller (CRC32 From name of bone).
	unsigned int        ControllerID;

	char                prop[32];
	CryBonePhysics_Comp phys;

	AUTO_STRUCT_INFO;
};

struct KEY_HEADER
{
	int KeyTime;    //!< Number of ticks.
	AUTO_STRUCT_INFO;
};

struct RANGE_ENTITY
{
	char name[32];
	int  start;
	int  end;
	AUTO_STRUCT_INFO;
};

//! Timing Chunk Header.
struct TIMING_CHUNK_DESC_0918
{
	enum { VERSION = 0x0918 };

	f32          _obsolete_SecsPerTick;   //!< Always assumed to be 1/4800.
	int32        _obsolete_TicksPerFrame; //!< Always assumed to be 160.

	RANGE_ENTITY global_range;     //!< Specifies time range of the animation, expressed in 'ticks' (1/4800th of a second).

	int32        _obsolete_SubRanges;

	AUTO_STRUCT_INFO;
};

//! Describes timing attributes of evenly sampled motion data. Used in .i_caf files.
struct TIMING_CHUNK_DESC_0919
{
	enum { VERSION = 0x0919 };

	int32 numberOfSamples;     //!< Shall be >= 1.
	float samplesPerSecond;    //!< Shall be > 0.
	float startTimeInSeconds;

	AUTO_STRUCT_INFO;
};

struct SPEED_CHUNK_DESC_2
{
	enum { VERSION = 0x0922 };

	float  Speed;
	float  Distance;
	float  Slope;
	uint32 AnimFlags;
	f32    MoveDir[3];
	QuatT  StartPosition;
	AUTO_STRUCT_INFO;
};

struct MotionParams905
{
	uint32 m_nAssetFlags;
	uint32 m_nCompression;

	int32  m_nTicksPerFrame;  //!< Unused (always assumed to be 1).
	f32    m_fSecsPerTick;    //!< Unused (always assumed to be 1/30).
	int32  m_nStart;
	int32  m_nEnd;

	f32    m_fMoveSpeed;
	f32    m_fTurnSpeed;
	f32    m_fAssetTurn;
	f32    m_fDistance;
	f32    m_fSlope;

	QuatT  m_StartLocation;
	QuatT  m_EndLocation;

	f32    m_LHeelStart, m_LHeelEnd;
	f32    m_LToe0Start, m_LToe0End;
	f32    m_RHeelStart, m_RHeelEnd;
	f32    m_RToe0Start, m_RToe0End;

	MotionParams905()
	{
		m_nAssetFlags = 0;
		m_nCompression = -1;
		m_nTicksPerFrame = 0;
		m_fSecsPerTick = 0;
		m_nStart = 0;
		m_nEnd = 0;

		m_fMoveSpeed = -1;
		m_fTurnSpeed = -1;
		m_fAssetTurn = -1;
		m_fDistance = -1;
		m_fSlope = -1;

		m_LHeelStart = -1;
		m_LHeelEnd = -1;
		m_LToe0Start = -1;
		m_LToe0End = -1;
		m_RHeelStart = -1;
		m_RHeelEnd = -1;
		m_RToe0Start = -1;
		m_RToe0End = -1;

		m_StartLocation.SetIdentity();
		m_EndLocation.SetIdentity();
	}
};

struct CHUNK_MOTION_PARAMETERS
{
	enum { VERSION = 0x0925 };

	MotionParams905 mp;
};

struct CHUNK_GAHCAF_INFO
{
	enum { VERSION = 0x0971 };
	enum { FILEPATH_SIZE = 256 };

	uint32 m_Flags;
	char   m_FilePath[FILEPATH_SIZE];
	uint32 m_FilePathCRC32;
	uint32 m_FilePathDBACRC32;

	f32    m_LHeelStart, m_LHeelEnd;
	f32    m_LToe0Start, m_LToe0End;
	f32    m_RHeelStart, m_RHeelEnd;
	f32    m_RToe0Start, m_RToe0End;

	f32    m_fStartSec;         //!< Asset-feature: Start time in seconds.
	f32    m_fEndSec;           //!< Asset-feature: End time in seconds.
	f32    m_fTotalDuration;    //!< Asset-feature: asset-feature: total duration in seconds.
	uint32 m_nControllers;

	QuatT  m_StartLocation;     //!< Locator information.
	QuatT  m_LastLocatorKey;    //!< Locator information.

	Vec3   m_vVelocity;         //!< Asset-feature: the velocity vector for this asset.
	f32    m_fDistance;         //!< Asset-feature: the absolute distance this objects is moving.
	f32    m_fSpeed;            //!< Asset-feature: speed (meters in second).
	f32    m_fSlope;            //!< Asset-feature: uphill-downhill measured in degrees.
	f32    m_fTurnSpeed;        //!< Asset-feature: turning speed per second.
	f32    m_fAssetTurn;        //!< Asset-feature: radiant between first and last frame.
};

struct CHUNK_GAHAIM_INFO
{
	struct VirtualExampleInit2
	{
		Vec2  polar;
		uint8 i0, i1, i2, i3;
		f32   w0, w1, w2, w3;
	};
	struct VirtualExample
	{
		uint8 i0, i1, i2, i3;
		int16 v0, v1, v2, v3;
	};

	enum { VERSION = 0x0970 };
	enum { XGRID = 17 };
	enum { YGRID = 9 };
	enum { FILEPATH_SIZE = 256 };

	uint32         m_Flags;
	char           m_FilePath[FILEPATH_SIZE];
	uint32         m_FilePathCRC32;

	f32            m_fStartSec;      //!< Asset-feature: Start time in seconds.
	f32            m_fEndSec;        //!< Asset-feature: End time in seconds.
	f32            m_fTotalDuration; //!< Asset-feature: total duration in seconds.

	uint32         m_AnimTokenCRC32;

	uint64         m_nExist;
	Quat           m_MiddleAimPoseRot;
	Quat           m_MiddleAimPose;
	VirtualExample m_PolarGrid[XGRID * YGRID];
	uint32         m_numAimPoses;
};

//========================================
//Material Chunk Header
//========================================

#define MTL_NAME_CHUNK_DESC_0800_MAX_SUB_MATERIALS (32)
struct MTL_NAME_CHUNK_DESC_0800
{
	enum { VERSION = 0x0800 };
	enum EFlags
	{
		FLAG_MULTI_MATERIAL = 0x0001, //!< Have sub materials info.
		FLAG_SUB_MATERIAL   = 0x0002, //!< This is sub material.
		FLAG_SH_COEFFS      = 0x0004, //!< This material should get spherical harmonics coefficients computed.
		FLAG_SH_2SIDED      = 0x0008, //!< This material will be used as 2 sided in the sh precomputation.
		FLAG_SH_AMBIENT     = 0x0010, //!< This material will get an ambient sh term(to not shadow it entirely).
	};

	int   nFlags; //!< See EFlags.
	int   nFlags2;
	char  name[128]; //!< Material/shader name.
	int   nPhysicalizeType;
	int   nSubMaterials;
	int   nSubMatChunkId[MTL_NAME_CHUNK_DESC_0800_MAX_SUB_MATERIALS];
	int   nAdvancedDataChunkId;
	float sh_opacity;
	int   reserve[32];

	AUTO_STRUCT_INFO;
};

struct MTL_NAME_CHUNK_DESC_0802
{
	enum { VERSION = 0x0802 };

	char name[128];  //!< Material/shader name.
	int  nSubMaterials;

	// Data continues from here.
	// 1) if nSubMaterials is 0, this is a single-material: we store physicalization type of the material (int32).
	// 2) if nSubMaterials is not 0, this is a multi-material: we store nSubMaterials physicalization types (int32
	//    value for each sub-material). After the physicalization types we store chain of ASCIIZ names of sub-materials.

	AUTO_STRUCT_INFO;
};

//========================================
//Mesh Chunk Header
//========================================

struct MESH_CHUNK_DESC_0745
{
	//! Versions 0x0744 and 0x0745 are *exactly* the same.
	//! Version number was increased from 0x0744 to 0x0745 just because
	//! it was the only way to inform *old* (existing) executables that
	//! NODE_CHUNK_DESC(!) chunk format was changed and cannot be read
	//! by them (old CLoaderCGF::LoadNodeChunk() didn't check.
	//! NODE_CHUNK_DESC's version number).
	enum { VERSION = 0x0745 };
	enum { COMPATIBLE_OLD_VERSION = 0x0744 };

	enum EFlags1
	{
		FLAG1_BONE_INFO = 0x01,
	};
	enum EFlags2
	{
		FLAG2_HAS_VERTEX_COLOR = 0x01,
		FLAG2_HAS_VERTEX_ALPHA = 0x02,
		FLAG2_HAS_TOPOLOGY_IDS = 0x04,
	};
	unsigned char flags1;
	unsigned char flags2;
	int           nVerts;
	int           nTVerts; //!< Number of texture vertices (0 or nVerts).
	int           nFaces;
	int           VertAnimID; //!< Id of the related vertAnim chunk if present. otherwise it is -1.

	AUTO_STRUCT_INFO;
};

//! Compiled Mesh chunk.
struct MESH_CHUNK_DESC_0801
{
	//! Versions 0x0800 and 0x0801 are *exactly* the same.
	//! Version number was increased from 0x0800 to 0x0801 just because
	//! it was the only way to inform *old* (existing) executables that
	//! NODE_CHUNK_DESC(!) chunk format was changed and cannot be read
	//! by them (old CLoaderCGF::LoadNodeChunk() didn't check
	//! NODE_CHUNK_DESC's version number).
	enum { VERSION = 0x0801 };
	enum { COMPATIBLE_OLD_VERSION = 0x0800 };

	enum EFlags
	{
		MESH_IS_EMPTY           = 0x0001, //!< Empty mesh (no streams are saved).
		HAS_TEX_MAPPING_DENSITY = 0x0002, //!< texMappingDensity contains a valid value.
		HAS_EXTRA_WEIGHTS       = 0x0004, //!< The weight stream will have weights for influences 5-8.
		HAS_FACE_AREA           = 0x0008, //!< geometricMeanFaceArea contains a valid value.
	};

	int nFlags;  //!< @see EFlags
	int nFlags2;

	// Just for info.
	int nVerts;       //!< Number of vertices.
	int nIndices;     //!< Number of indices.
	int nSubsets;     //!< Number of mesh subsets.

	int nSubsetsChunkId; //!< Chunk id of subsets. (Must be ChunkType_MeshSubsets)
	int nVertAnimID;     //!< id of the related vertAnim chunk if present. otherwise it is -1

	//! ChunkIDs of data streams (Must be ChunkType_DataStream).
	//! Index is one of ECgfStreamType values.
	int nStreamChunkID[16];

	//! Chunk IDs of physical mesh data. (Must be ChunkType_MeshPhysicsData).
	int nPhysicsDataChunkId[4];

	//! Bounding box of the mesh.
	Vec3  bboxMin;
	Vec3  bboxMax;

	float texMappingDensity;
	float geometricMeanFaceArea;
	int   reserved[31];
	AUTO_STRUCT_INFO;
};

//! Stream chunk contains data about a mesh data stream (positions, normals, etc...).
struct STREAM_DATA_CHUNK_DESC_0800
{
	enum { VERSION = 0x0800 };

	enum EFlags {};  //!< Not implemented.

	int nFlags;
	int nStreamType;  //!< Stream type one of ECgfStreamType.
	int nCount;       //!< Number of elements.
	int nElementSize; //!< Element Size.
	int reserved[2];

	//! Data starts here at the end of the chunk.
	//! char streamData[nCount*nElementSize];

	AUTO_STRUCT_INFO;
};

//! Contains array of mesh subsets.
//! Each subset holds an info about material id, indices ranges etc.
struct MESH_SUBSETS_CHUNK_DESC_0800
{
	enum { VERSION = 0x0800 };

	enum EFlags
	{
		SH_HAS_DECOMPR_MAT       = 0x0001, //!< Obsolete.
		BONEINDICES              = 0x0002,
		HAS_SUBSET_TEXEL_DENSITY = 0x0004,
	};

	int nFlags;
	int nCount;       //!< Number of elements.
	int reserved[2];

	struct MeshSubset
	{
		int   nFirstIndexId;
		int   nNumIndices;
		int   nFirstVertId;
		int   nNumVerts;
		int   nMatID; //!< Material sub-object Id.
		float fRadius;
		Vec3  vCenter;

		AUTO_STRUCT_INFO;
	};

	struct MeshBoneIDs
	{
		uint32 numBoneIDs;
		uint16 arrBoneIDs[0x80];

		AUTO_STRUCT_INFO;
	};

	struct MeshSubsetTexelDensity
	{
		float texelDensity;

		AUTO_STRUCT_INFO;
	};

	// Data starts here at the end of the chunk.
	//Subset streamData[nCount];

	MESH_SUBSETS_CHUNK_DESC_0800()
		: nFlags(0)
		, nCount(0)
	{
	}

	AUTO_STRUCT_INFO;
};

//! Contain array of mesh subsets.
//! Each subset holds an info about material id, indices ranges etc...
struct MESH_PHYSICS_DATA_CHUNK_DESC_0800
{
	enum { VERSION = 0x0800 };

	int nDataSize;  //!< Size of physical data at the end of the chunk.
	int nFlags;
	int nTetrahedraDataSize;
	int nTetrahedraChunkId; //!< Chunk of physics Tetrahedra data.
	int reserved[2];

	// Data starts here at the end of the chunk.
	//char physicsData[nDataSize];
	//char tetrahedraData[nTetrahedraDataSize];

	AUTO_STRUCT_INFO;
};

struct VERTANIM_CHUNK_DESC_0744
{
	enum { VERSION = 0x0744 };

	int GeomID;       //!< ID of the related mesh chunk.
	int nKeys;        //!< Number of keys.
	int nVerts;       //!< Number of vertices this object has.
	int nFaces;       //!< Number of faces this object has (for double check purpose).

	AUTO_STRUCT_INFO;
};

typedef VERTANIM_CHUNK_DESC_0744 VERTANIM_CHUNK_DESC;
#define VERTANIM_CHUNK_DESC_VERSION VERTANIM_CHUNK_DESC_0744::VERSION

//! Bone Anim Chunk Header.
struct BONEANIM_CHUNK_DESC_0290
{
	enum { VERSION = 0x0290 };

	int nBones;

	AUTO_STRUCT_INFO;
};

//! Bonelist Chunk Header.
//! This structure describes the bone names.
//! It's followed by numEntities packed \0-terminated strings, the list terminated by double-\0.
struct BONENAMELIST_CHUNK_DESC_0745
{
	enum { VERSION = 0x0745 };

	int numEntities;
	AUTO_STRUCT_INFO;
};

struct COMPILED_BONE_CHUNK_DESC_0800
{
	enum { VERSION = 0x0800 };

	char reserved[32];
	AUTO_STRUCT_INFO;
};

struct COMPILED_PHYSICALBONE_CHUNK_DESC_0800
{
	enum { VERSION = 0x0800 };

	char reserved[32];
	AUTO_STRUCT_INFO;
};

struct COMPILED_PHYSICALPROXY_CHUNK_DESC_0800
{
	enum { VERSION = 0x0800 };

	uint32 numPhysicalProxies;
	AUTO_STRUCT_INFO;
};

struct COMPILED_MORPHTARGETS_CHUNK_DESC_0800
{
	enum { VERSION = 0x0800, VERSION1 = 0x801 };

	uint32 numMorphTargets;
	AUTO_STRUCT_INFO;
};

struct COMPILED_INTFACES_CHUNK_DESC_0800
{
	enum { VERSION = 0x0800 };

	AUTO_STRUCT_INFO;
};

struct COMPILED_INTSKINVERTICES_CHUNK_DESC_0800
{
	enum { VERSION = 0x0800 };

	char reserved[32];
	AUTO_STRUCT_INFO;
};

struct COMPILED_EXT2INTMAP_CHUNK_DESC_0800
{
	enum { VERSION = 0x0800 };

	AUTO_STRUCT_INFO;
};

struct COMPILED_BONEBOXES_CHUNK_DESC_0800
{
	enum { VERSION = 0x0800, VERSION1 = 0x801 };

	AUTO_STRUCT_INFO;
};

//! Keyframe and Timing Primitives.
struct BaseKey
{
	int time;
	AUTO_STRUCT_INFO;
};

struct BaseTCB
{
	float t, c, b;
	float ein, eout;
	AUTO_STRUCT_INFO;
};

struct BaseKey1 : BaseKey
{
	float val;
	AUTO_STRUCT_INFO;
};
struct BaseKey3 : BaseKey
{
	Vec3 val;
	AUTO_STRUCT_INFO;
};
struct BaseKeyQ : BaseKey
{
	CryQuat val;
	AUTO_STRUCT_INFO;
};

struct CryLin1Key : BaseKey1
{
	AUTO_STRUCT_INFO;
};
struct CryLin3Key : BaseKey3
{
	AUTO_STRUCT_INFO;
};
struct CryLinQKey : BaseKeyQ
{
	AUTO_STRUCT_INFO;
};
struct CryTCB1Key : BaseKey1, BaseTCB
{
	AUTO_STRUCT_INFO;
};
struct CryTCB3Key : BaseKey3, BaseTCB
{
	AUTO_STRUCT_INFO;
};
struct CryTCBQKey : BaseKeyQ, BaseTCB
{
	AUTO_STRUCT_INFO;
};
struct CryBez1Key : BaseKey1
{
	float intan, outtan;
	AUTO_STRUCT_INFO;
};
struct CryBez3Key : BaseKey3
{
	Vec3 intan, outtan;
	AUTO_STRUCT_INFO;
};
struct CryBezQKey : BaseKeyQ
{
	AUTO_STRUCT_INFO;
};

//! Single animation frame used in CONTROLLER_CHUNK_DESC_0827 controller chunks. Used in .i_caf files.
struct CryKeyPQLog
{
	int  nTime;
	Vec3 vPos;
	Vec3 vRotLog; //!< Logarithm of the rotation.

	AUTO_STRUCT_INFO;
};

//! Single animation frame used in CONTROLLER_CHUNK_DESC_0833 controller chunks. Used in .i_caf files.
struct CryKeyPQS
{
	Vec3          position;
	Quat_tpl<f32> rotation; //!< We cannot use Quat here, because 3ds Max API already defines its own Quat which causes conflicts in the CryMaxExport projects.
	Diag33        scale;

	AUTO_STRUCT_INFO;
};

//========================================
//Controller Chunk Header
//========================================

//! TCB controller types.
enum CtrlTypes
{
	CTRL_NONE,
	CTRL_CRYBONE,
	CTRL_LINEER1, CTRL_LINEER3, CTRL_LINEERQ,
	CTRL_BEZIER1, CTRL_BEZIER3, CTRL_BEZIERQ,
	CTRL_TCB1, CTRL_TCB3, CTRL_TCBQ,
	CTRL_BSPLINE_2O, //!< 2-byte fixed values, open.
	CTRL_BSPLINE_1O, //!< 1-byte fixed values, open.
	CTRL_BSPLINE_2C, //!< 2-byte fixed values, closed.
	CTRL_BSPLINE_1C, //!< 1-byte fixed values, closed.
	CTRL_CONST       //!< constant position&rotation.
};

//! TCB controller flags.
enum CtrlFlags
{
	CTRL_ORT_CYCLE = 0x01,
	CTRL_ORT_LOOP  = 0x02
};

//! TCB animation controller chunk. Used in .anm and .cga files.
struct CONTROLLER_CHUNK_DESC_0826
{
	enum { VERSION = 0x0826 };

	CtrlTypes type;          //!< Specifies the type of the controller.
	int32     nKeys;         //!< Number of keys stored within this chunk; total number of knots (positional and orientational) in the case of a B-Spline.
	uint32    nFlags;        //!< Controller flags (see: CtrlFlags).
	uint32    nControllerId; //!< Unique controller ID (crc32 of the bone name).

	AUTO_STRUCT_INFO;
};

//! Uncompressed animation controller chunk. Used in .i_caf files.
//! The chunk payload consists of numKeys of CryKeyPQLog structures that represent sampled animation frames.
struct CONTROLLER_CHUNK_DESC_0827
{
	enum { VERSION = 0x0827 };
	uint32 numKeys;        //!< Number of keys stored within this chunk.
	uint32 nControllerId;  //!< Unique controller ID (crc32 of the bone name).

	AUTO_STRUCT_INFO;
};

//! Unused.
struct CONTROLLER_CHUNK_DESC_0828
{
	enum
	{
		VERSION = 0x0828
	};
};

//! Compressed PQ animation controller chunk (position + scale). Used in .caf files.
//! The chunk payload has the following structure:
//! - rotation value keys.
//! - rotation time keys.
//! - position value keys.
//! - position time keys (optional).
//! The number of keys and their binary representations are specified by this descriptor.
struct CONTROLLER_CHUNK_DESC_0829
{
	enum { VERSION = 0x0829 };

	enum { eKeyTimeRotation = 0, eKeyTimePosition = 1 };

	uint32 nControllerId;      //!< Unique controller ID (crc32 of the bone name).

	uint16 numRotationKeys;    //!< Number of rotation keys stored within this chunk.
	uint16 numPositionKeys;    //!< Number of position keys stored within this chunk.
	uint8  RotationFormat;     //!< Format identifier of the rotation value keys (see: ECompressionInformation enumeration type).
	uint8  RotationTimeFormat; //!< Format identifier of the rotation time keys (see: EKeyTimesFormat enumeration type).
	uint8  PositionFormat;     //!< Format identifier of the position value keys (see: ECompressionInformation enumeration type).
	uint8  PositionKeysInfo;   //!< Specifies which time key array is used by the position component (eKeyTimeRotation, eKeyTimePosition).
	uint8  PositionTimeFormat; //!< Format of identifier the position time keys (see: EKeyTimesFormat enumeration type). May not be used depending on the value of PositionKeysInfo.
	uint8  TracksAligned;      //!< If set to nonzero, signals that all key arrays are padded to a 4-byte boundary.

	AUTO_STRUCT_INFO;
};

//! Unused (were supposed to use wavelet compression).
struct CONTROLLER_CHUNK_DESC_0830
{
	enum
	{
		VERSION = 0x830
	};
};
struct CONTROLLER_CHUNK_DESC_0831
{
	enum
	{
		VERSION = 0x831
	};
};

//! Compressed PQS animation controller chunk (position + rotation + scale). Used in .caf files.
//! The chunk layout has the following structure:
//! - rotation value keys.
//! - rotation time keys.
//! - position value keys.
//! - position time keys (optional).
//! - scale value keys.
//! - scale time keys (optional).
//! The number of keys and their binary representations are specified by this descriptor.
//! All of the key arrays and the chunk descriptor itself are always padded to a 4-byte boundary.
struct CONTROLLER_CHUNK_DESC_0832
{
	enum { VERSION = 0x0832 };

	enum { eKeyTimeRotation = 0, eKeyTimePosition = 1, eKeyTimeScale = 2 };

	uint32 nControllerId;      //!< Unique controller ID (crc32 of the bone name).

	uint16 numRotationKeys;    //!< Number of rotation keys stored within this chunk.
	uint16 numPositionKeys;    //!< Number of position keys stored within this chunk.
	uint16 numScaleKeys;       //!< Number of scale keys stored within this chunk.

	uint8  rotationFormat;     //!< Format identifier of the rotation value keys (see: ECompressionInformation enumeration type).
	uint8  rotationTimeFormat; //!< Format identifier of the rotation time keys (see: EKeyTimesFormat enumeration type).

	uint8  positionFormat;     //!< Format identifier of the position value keys (see: ECompressionInformation enumeration type).
	uint8  positionKeysInfo;   //!< Specifies which time key array is used by the position component (eKeyTimeRotation, eKeyTimePosition).
	uint8  positionTimeFormat; //!< Format of identifier the position time keys (see: EKeyTimesFormat enumeration type). May not be used depending on the value of positionKeysInfo.

	uint8  scaleFormat;        //!< Format of identifier the scale value keys (see: ECompressionInformation enumeration type).
	uint8  scaleKeysInfo;      //!< Specifies which time key array is used by the scale component (eKeyTimeRotation, eKeyTimePosition, eKeyTimeScale).
	uint8  scaleTimeFormat;    //!< Format of identifier the scale time keys (see: EKeyTimesFormat enumeration type). May not be used depending on the value of scaleKeysInfo.

	char   padding[2];

	AUTO_STRUCT_INFO;
};

#if !defined(SWIG)
static_assert(sizeof(CONTROLLER_CHUNK_DESC_0832) % 4 == 0, "CONTROLLER_CHUNK_DESC_0832 should be padded to a 4-byte boundary!");
#endif

//! Uncompressed animation controller chunk. Used in .i_caf files.
//! The chunk payload consists of numKeys of CryKeyPQS structures that represent sampled animation frames.
struct CONTROLLER_CHUNK_DESC_0833
{
	enum { VERSION = 0x0833 };
	uint32 numKeys;       //!< Number of keys stored within this chunk.
	uint32 controllerId;  //!< Unique controller ID (crc32 of the bone name).

	AUTO_STRUCT_INFO;
};

struct CONTROLLER_CHUNK_DESC_0905
{
	enum { VERSION = 0x0905 };

	uint32 numKeyPos;
	uint32 numKeyRot;
	uint32 numKeyTime;
	uint32 numAnims;
	AUTO_STRUCT_INFO;
};

//========================================
//Node Chunk Header
//========================================
struct NODE_CHUNK_DESC_0824
{
	//! Versions 0x0823 and 0x0824 have exactly same layout.
	//! The only difference between 0x0823 and 0x0824 is that some members.
	//! are now named _obsoleteXXX_ and are not filled/used in 0x0824.
	enum { VERSION = 0x0824 };
	enum { COMPATIBLE_OLD_VERSION = 0x0823 };

	char  name[64];

	int   ObjectID;          //!< ID of this node's object chunk (if present).
	int   ParentID;          //!< Chunk ID of the parent node's chunk.
	int   nChildren;         //!< Number of child nodes.
	int   MatID;             //!< Material chunk number.

	uint8 _obsoleteA_[4];    //!< uint8 IsGroupHead; uint8 IsGroupMember; uint8 _padding_[2]. not used anymore.

	float tm[4][4];          //!< Transformation matrix.

	float _obsoleteB_[3];    //!< Position component of the matrix, stored as Vec3. not used anymore.
	float _obsoleteC_[4];    //!< Rotation component of the matrix, stored as CryQuat. not used anymore.
	float _obsoleteD_[3];    //!< Scale component of the matrix, stored as Vec3. not used anymore.

	int   pos_cont_id;       //!< Position controller chunk id.
	int   rot_cont_id;       //!< Rotation controller chunk id.
	int   scl_cont_id;       //!< Scale controller chunk id.

	int   PropStrLen;        //!< Length of the property string.

	AUTO_STRUCT_INFO;
};

//========================================
//Helper Chunk Header
//========================================
enum HelperTypes
{
	HP_POINT    = 0,
	HP_DUMMY    = 1,
	HP_XREF     = 2,
	HP_CAMERA   = 3,
	HP_GEOMETRY = 4
};

struct HELPER_CHUNK_DESC_0744
{
	enum { VERSION = 0x0744 };

	HelperTypes type; //!< One of the HelperTypes values.
	Vec3        size; //!< Size in local x,y,z axises (for dummy only).

	AUTO_STRUCT_INFO;
};

typedef HELPER_CHUNK_DESC_0744 HELPER_CHUNK_DESC;
#define HELPER_CHUNK_DESC_VERSION HELPER_CHUNK_DESC::VERSION

//! ChunkType_MeshMorphTarget  - morph target of a mesh chunk.
//! This chunk contains only the information about the vertices that are changed in the mesh.
//! This chunk is followed by an array of numMorphVertices structures SMeshMorphTargetVertex,
//! immediately followed by the name (null-terminated, variable-length string) of the morph target.
//! The string is after the array because of future alignment considerations; it may be padded with 0s.
struct MESHMORPHTARGET_CHUNK_DESC_0001
{
	enum { VERSION = 0x0001 };
	uint32 nChunkIdMesh;     //!< The chunk id of the mesh chunk (ChunkType_Mesh) for which this morph target is.
	uint32 numMorphVertices; //!< Number of MORPHED vertices.

	AUTO_STRUCT_INFO;
};

//! An array of these structures follows the MESHMORPHTARGET_CHUNK_DESC_0001.
//! There are numMorphVertices of them.
struct SMeshMorphTargetVertex
{
	uint32 nVertexId;  //!< Vertex index in the original (mesh) array of vertices.
	Vec3   ptVertex;   //!< The target point of the morph target.
	void   GetMemoryUsage(ICrySizer* pSizer) const {}
	AUTO_STRUCT_INFO;
};

struct SMeshMorphTargetHeader
{
	uint32 MeshID;
	uint32 NameLength;     //!< Size of the name string.
	uint32 numIntVertices; //!< Type SMeshMorphTargetVertex.
	uint32 numExtVertices; //!< Type SMeshMorphTargetVertex.

	AUTO_STRUCT_INFO;
};

struct SMeshPhysicalProxyHeader
{
	uint32 ChunkID;
	uint32 numPoints;
	uint32 numIndices;
	uint32 numMaterials;

	AUTO_STRUCT_INFO;
};

//! ChunkType_BoneInitialPos   - describes the initial position (4x3 matrix) of each bone; just an array of 4x3 matrices.
//! This structure is followed by.
struct BONEINITIALPOS_CHUNK_DESC_0001
{
	enum { VERSION = 0x0001 };
	//! The chunk id of the mesh chunk (ChunkType_Mesh) with bone info for which these bone initial positions are applicable.
	//! There might be some unused bones here as well. There must be the same number of bones as in the other chunks - they're placed
	//! in BoneId order.
	unsigned nChunkIdMesh;

	//! This is the number of bone initial pose matrices here.
	unsigned numBones;

	AUTO_STRUCT_INFO;
};

//! An array of these matrices follows the  BONEINITIALPOS_CHUNK_DESC_0001 header there are numBones of them.
//! To be replaced with Matrix43.
struct SBoneInitPosMatrix
{
	float mx[4][3];
	float*       operator[](int i)       { return mx[i]; }
	const float* operator[](int i) const { return mx[i]; }
	const Vec3&  getOrt(int nOrt) const  { return *(const Vec3*)(mx[nOrt]); }

	AUTO_STRUCT_INFO;
};

//////////////////////////////////////////////////////////////////////////
// Custom Attributes chunk description.
//////////////////////////////////////////////////////////////////////////
struct EXPORT_FLAGS_CHUNK_DESC
{
	enum { VERSION = 0x0001 };
	enum EFlags
	{
		MERGE_ALL_NODES          = 0x0001,
		HAVE_AUTO_LODS           = 0x0002,
		USE_CUSTOM_NORMALS       = 0x0004,
		WANT_F32_VERTICES        = 0x0008,
		EIGHT_WEIGHTS_PER_VERTEX = 0x0010,
		MAKE_VCLOTH              = 0x0020
	};
	enum ESrcFlags
	{
		FROM_MAX_EXPORTER = 0x0000,
		FROM_COLLADA_XSI  = 0x1001,
		FROM_COLLADA_MAX  = 0x1002,
		FROM_COLLADA_MAYA = 0x1003,
	};

	unsigned int flags;                 //!< @see EFlags
	unsigned int rc_version[4];         //!< Resource compiler version.
	char         rc_version_string[16]; //!< Version as a string.
	unsigned int assetAuthorTool;
	unsigned int authorToolVersion;
	unsigned int reserved[30];

	AUTO_STRUCT_INFO;
};

struct BREAKABLE_PHYSICS_CHUNK_DESC
{
	enum { VERSION = 0x0001 };

	unsigned int granularity;
	int          nMode;
	int          nRetVtx;
	int          nRetTets;
	int          nReserved[10];

	AUTO_STRUCT_INFO;
};

struct FOLIAGE_INFO_CHUNK_DESC
{
	enum { VERSION = 0x0001 };

	int nSpines;
	int nSpineVtx;
	int nSkinnedVtx;
	int nBoneIds;

	AUTO_STRUCT_INFO;
};

struct FOLIAGE_SPINE_SUB_CHUNK
{
	unsigned char nVtx;
	char          _paddingA_[3];
	float         len;
	Vec3          navg;
	unsigned char iAttachSpine;
	unsigned char iAttachSeg;
	char          _paddingB_[2];

	AUTO_STRUCT_INFO;
};

// Header of a VCloth chunk.
struct VCLOTH_CHUNK
{
	enum { VERSION = 0x0001 };

	uint32 vertexCount;
	uint32 bendTrianglePairCount;
	uint32 bendTriangleCount;
	uint32 nndcNotAttachedOrderedIdxCount;
	uint32 linkCount[3];

	AUTO_STRUCT_INFO;

	// The payload data follows immediately after this header and consists of the following arrays (in this order):
	//
	// SVClothChunkVertex      vertices[vertexCount];
	// SVClothBendTriangle     triangles[bendTriangleCount];
	// SVClothBendTrianglePair trianglePairs[bendTrianglePairCount];
	// SVClothNndcNotAttachedOrderedIdx m_nndcNotAttachedOrderedIdx[nndcNotAttachedOrderedIdxCount];
	// SVClothLink             m_links [linkCount];

};

//! \endcond