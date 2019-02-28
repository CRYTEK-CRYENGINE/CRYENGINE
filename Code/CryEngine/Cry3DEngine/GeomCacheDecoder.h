// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ------------------------------------------------------------------------
//  File name:   GeomCacheDecoder.h
//  Created:     23/8/2012 by Axel Gneiting
//  Description: Decodes geom cache data
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef _GEOMCACHE_DECODER_
#define _GEOMCACHE_DECODER_

#pragma once

#if defined(USE_GEOM_CACHES)

	#include <Cry3DEngine/GeomCacheFileFormat.h>

class CGeomCache;
struct SGeomCacheRenderMeshUpdateContext;
struct SGeomCacheStaticMeshData;

struct SGeomCacheFrameHeader
{
	enum EFrameHeaderState
	{
		eFHS_Uninitialized = 0,
		eFHS_Undecoded     = 1,
		eFHS_Decoded       = 2
	};

	EFrameHeaderState m_state;
	uint32            m_offset;
};

namespace GeomCacheDecoder
{
// Decodes an index frame
void DecodeIFrame(const CGeomCache* pGeomCache, char* pData);

// Decodes a bi-directional predicted frame
void DecodeBFrame(const CGeomCache* pGeomCache, char* pData, char* pPrevFramesData[2],
                  char* pFloorIndexFrameData, char* pCeilIndexFrameData);

bool PrepareFillMeshData(SGeomCacheRenderMeshUpdateContext& updateContext, const SGeomCacheStaticMeshData& staticMeshData,
                         const char*& pFloorFrameMeshData, const char*& pCeilFrameMeshData, size_t& offsetToNextMesh, float& lerpFactor);

void FillMeshDataFromDecodedFrame(const bool bMotionBlur, SGeomCacheRenderMeshUpdateContext& updateContext,
                                  const SGeomCacheStaticMeshData& staticMeshData, const char* pFloorFrameMeshData,
                                  const char* pCeilFrameMeshData, float lerpFactor);

// Gets total needed space for uncompressing successive blocks
uint32 GetDecompressBufferSize(const char* const pStartBlock, const uint numFrames);

// Decompresses one block of compressed data with header for input
bool DecompressBlock(const GeomCacheFile::EBlockCompressionFormat compressionFormat, char* const pDest, const char* const pSource);

// Decompresses blocks of compressed data with headers for input and output
bool DecompressBlocks(const GeomCacheFile::EBlockCompressionFormat compressionFormat, char* const pDest,
                      const char* const pSource, const uint blockOffset, const uint numBlocks, const uint numHandleFrames);

Vec3 DecodePosition(const Vec3& aabbMin, const Vec3& aabbSize, const GeomCacheFile::Position& inPosition, const Vec3& convertFactor);
Vec2 DecodeTexcoord(const GeomCacheFile::Texcoords& inTexcoords);
Quat DecodeQTangent(const GeomCacheFile::QTangent& inQTangent);

void TransformAndConvertToTangentAndBitangent(const Quat& rotation, const Quat& inQTangent, SPipTangents& outTangents);
void ConvertToTangentAndBitangent(const Quat& inQTangent, SPipTangents& outTangents);
};

#endif
#endif
