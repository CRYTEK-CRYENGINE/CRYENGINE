// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2012.
// -------------------------------------------------------------------------
//  File name:   GeomCacheEncoder.h
//  Created:     19. December 2012 by Axel Gneiting
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GeomCache.h"
#include "GeomCacheWriter.h"
#include "StealingThreadPool.h"

class GeomCacheEncoder;

struct GeomCacheEncoderFrameInfo
{
	GeomCacheEncoderFrameInfo(GeomCacheEncoder *pEncoder, const uint frameIndex, 
		const Alembic::Abc::chrono_t frameTime, const AABB &aabb, const bool bIsLastFrame) 
		: m_pEncoder(pEncoder), m_bWritten(false), m_encodeCountdown(0), m_frameIndex(frameIndex), 
		m_frameTime(frameTime), m_doneCountdown(0), m_frameAABB(aabb), m_bIsLastFrame(bIsLastFrame) {};

	// The encoder that created this structure
	GeomCacheEncoder *m_pEncoder;

	// The ID of this frame
	uint m_frameIndex;

	// Frame type
	GeomCacheFile::EFrameType m_frameType;

	// If frame is the last one
	bool m_bIsLastFrame;

	// The time of this frame
	Alembic::AbcCoreAbstract::chrono_t m_frameTime;

	// If frame was written already
	bool m_bWritten;

	// If this counter reaches 0 the frame is ready to be written
	long m_encodeCountdown;	

	// If this counter reaches 0 the frame can be discarded
	long m_doneCountdown;	

	// AABB of frame
	const AABB m_frameAABB;
};

class GeomCacheEncoder
{
public:
	GeomCacheEncoder(GeomCacheWriter &geomCacheWriter, GeomCache::Node &rootNode,
		const std::vector<GeomCache::Mesh*> &meshes, ThreadUtils::StealingThreadPool &threadPool,
		const bool bUseBFrames, const uint indexFrameDistance);

	void Init(); 
	void AddFrame(const Alembic::Abc::chrono_t frameTime, const AABB &aabb, const bool bIsLastFrame);
	void Flush();

	static bool OptimizeMeshForCompression(GeomCache::Mesh &mesh, const bool bUseMeshPrediction);

private:
	GeomCacheEncoderFrameInfo &GetInfoFromFrameIndex(const uint index);

	void CountNodesRec(GeomCache::Node &currentNode);

	void CreateFrameJobGroup(GeomCacheEncoderFrameInfo *pFrame);
	void CreateMeshJobs(GeomCacheEncoderFrameInfo *pFrame, ThreadUtils::JobGroup *pJobGroup);

	static void FrameJobGroupFinishedCallback(GeomCacheEncoderFrameInfo *pFrame);
	void FrameGroupFinished(GeomCacheEncoderFrameInfo *pFrame);

	static void EncodeNodesJob(GeomCacheEncoderFrameInfo *pFrame);
	void EncodeNodesRec(GeomCache::Node &currentNode, GeomCacheEncoderFrameInfo *pFrame);
	void EncodeNodeIFrame(const GeomCache::Node &currentNode, const GeomCache::NodeData &rawFrame, std::vector<uint8> &output);

	static void EncodeMeshJob(std::pair<GeomCache::Mesh*, GeomCacheEncoderFrameInfo*> *pData);
	void EncodeMesh(GeomCache::Mesh *pMesh, GeomCacheEncoderFrameInfo *pFrame);
	void EncodeMeshIFrame(GeomCache::Mesh &mesh, GeomCache::RawMeshFrame &rawMeshFrame, std::vector<uint8> &output);
	void EncodeMeshBFrame(GeomCache::Mesh &mesh,  GeomCache::RawMeshFrame &rawMeshFrame, GeomCache::RawMeshFrame *pPrevFrames[2],
		GeomCache::RawMeshFrame &floorIndexFrame, GeomCache::RawMeshFrame &ceilIndexFrame, std::vector<uint8> &output);

	GeomCacheWriter &m_geomCacheWriter;
	ThreadUtils::StealingThreadPool &m_threadPool;
	
	// Set to true if encoder should use bi-directional predicted frames
	bool m_bUseBFrames;
	uint m_indexFrameDistance;	

	// Number of animated nodes to compile
	unsigned int m_numNodes;

	// The job group status for the currently processed frame
	bool m_jobGroupDone;
	ThreadUtils::CriticalSection m_jobGroupDoneCS;
	ThreadUtils::ConditionVariable m_jobGroupDoneCV;

	// Frame data
	uint m_firstInfoFrameIndex;
	uint m_nextFrameIndex;
	ThreadUtils::CriticalSection m_framesCS;
	ThreadUtils::ConditionVariable m_frameRemovedCV;
	std::deque<std::unique_ptr<GeomCacheEncoderFrameInfo>> m_frames;	

	// Global scene structure handles from alembic compiler
	GeomCache::Node &m_rootNode;
	const std::vector<GeomCache::Mesh*> &m_meshes;
};
