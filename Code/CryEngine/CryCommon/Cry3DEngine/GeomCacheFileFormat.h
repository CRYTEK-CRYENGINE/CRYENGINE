// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GeomCacheFileFormat.h
//  Created:     26/8/2012 by Axel Gneiting
//
////////////////////////////////////////////////////////////////////////////

#ifndef __GeomCacheFileFormat_h__
#define __GeomCacheFileFormat_h__
#pragma once

#include "CryExtension/CryGUID.h"

#if !(CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)
	#pragma pack(push)
	#pragma pack(1)
	#define PACK_GCC
#else
	#define PACK_GCC __attribute__ ((packed))
#endif

namespace GeomCacheFile
{
// Important: The enums are serialized, don't change the values
// without increasing the file version, conversion code etc!

typedef Vec3_tpl<uint16> Position;
typedef Vec2_tpl<int16>  Texcoords;
typedef Vec4_tpl<int16>  QTangent;
typedef uint8            Color;

//! ASCII "CAXCACHE".
const uint64 kFileSignature = 0x4548434143584143ull;

//! Value at which texcoords will be clamped.
const uint kTexcoordRange = 64;

//! Bit Precision of tangents quaternions.
const uint kTangentQuatPrecision = 10;

//! Current file version GUID. Files with other GUIDs will not be loaded by the engine.
constexpr CryGUID kCurrentVersion = "1641defe-440a-f501-7ec5-e9164c8c2d1c"_cry_guid;

//! Mesh prediction look back array size.
const uint kMeshPredictorLookBackMaxDist = 4096;

//! Number of frames between index frames. Needs to be <= g_kMaxBufferedFrames.
const uint kMaxIFrameDistance = 30;

enum EFileHeaderFlags
{
	eFileHeaderFlags_PlaybackFromMemory = BIT(0),
	eFileHeaderFlags_32BitIndices       = BIT(1)
};

enum EBlockCompressionFormat
{
	eBlockCompressionFormat_None    = 0,
	eBlockCompressionFormat_Deflate = 1,     //!< Zlib.
	eBlockCompressionFormat_LZ4HC   = 2      //!< LZ4 HC.
};

enum EStreams
{
	eStream_Indices   = BIT(0),
	eStream_Positions = BIT(1),
	eStream_Texcoords = BIT(2),
	eStream_QTangents = BIT(3),
	eStream_Colors    = BIT(4)
};

enum ETransformType
{
	eTransformType_Constant,
	eTransformType_Animated
};

enum ENodeType
{
	eNodeType_Transform       = 0, //!< Transforms all sub nodes.
	eNodeType_Mesh            = 1,
	eNodeType_PhysicsGeometry = 2,
};

//! Common frame.
enum EFrameType
{
	eFrameType_IFrame = 0,
	eFrameType_BFrame = 1
};

//! Common frame flags.
enum EFrameFlags
{
	eFrameFlags_Hidden = BIT(0)
};

//! Flags for mesh index frames.
enum EMeshIFrameFlags
{
	eMeshIFrameFlags_UsePredictor = BIT(1)
};

struct SHeader
{
	SHeader()
		: m_signature(0)
		, m_versionGuidHipart(kCurrentVersion.hipart)
		, m_versionGuidLopart(kCurrentVersion.lopart)
		, m_blockCompressionFormat(0)
		, m_flags(0)
		, m_numFrames(0) {}

	uint64  m_signature;
	uint64 m_versionGuidHipart;
	uint64 m_versionGuidLopart;
	uint16  m_blockCompressionFormat;
	uint32  m_flags;
	uint32  m_numFrames;
	uint64  m_totalUncompressedAnimationSize;
	float   m_aabbMin[3];
	float   m_aabbMax[3];
} PACK_GCC;

struct SFrameInfo
{
	uint32 m_frameType;
	uint32 m_frameSize;
	uint64 m_frameOffset;
	float  m_frameTime;
} PACK_GCC;

struct SCompressedBlockHeader
{
	uint32 m_uncompressedSize;
	uint32 m_compressedSize;
} PACK_GCC;

struct SFrameHeader
{
	uint32 m_nodeDataOffset;
	float  m_frameAABBMin[3];
	float  m_frameAABBMax[3];
	uint32 m_padding;
} PACK_GCC;

struct STemporalPredictorControl
{
	uint8 m_acceleration;
	uint8 m_indexFrameLerpFactor;
	uint8 m_combineFactor;
	uint8 m_padding;
} PACK_GCC;

struct SMeshFrameHeader
{
	uint32                    m_flags;
	STemporalPredictorControl m_positionStreamPredictorControl;
	STemporalPredictorControl m_texcoordStreamPredictorControl;
	STemporalPredictorControl m_qTangentStreamPredictorControl;
	STemporalPredictorControl m_colorStreamPredictorControl[4];
} PACK_GCC;

struct SMeshInfo
{
	uint8  m_constantStreams;
	uint8  m_animatedStreams;
	uint8  m_positionPrecision[3];
	uint8  m_padding;
	uint16 m_numMaterials;
	uint32 m_numVertices;
	uint32 m_flags;
	float  m_aabbMin[3];
	float  m_aabbMax[3];
	uint32 m_nameLength;
	uint64 m_hash;
} PACK_GCC;

struct SNodeInfo
{
	uint8  m_type;
	uint8  m_bVisible;
	uint16 m_transformType;
	uint32 m_meshIndex;
	uint32 m_numChildren;
	uint32 m_nameLength;
} PACK_GCC;
}

#undef PACK_GCC

#if !(CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)
	#pragma pack(pop)
#endif

#endif
