// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ------------------------------------------------------------------------
//  File name:   GeomCache.h
//  Created:     19/7/2012 by Axel Gneiting
//  Description: Manages geometry cache data
// -------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////

#ifndef _GEOMCACHE_
#define _GEOMCACHE_

#pragma once

#if defined(USE_GEOM_CACHES)

	#include <Cry3DEngine/IGeomCache.h>
	#include <Cry3DEngine/GeomCacheFileFormat.h>

struct SGeomCacheStaticMeshData
{
	bool                                  m_bUsePredictor;
	uint8                                 m_positionPrecision[3];
	uint32                                m_numVertices;
	GeomCacheFile::EStreams               m_constantStreams;
	GeomCacheFile::EStreams               m_animatedStreams;
	uint64                                m_hash;
	AABB                                  m_aabb;
	string                                m_name;

	std::vector<vtx_idx>                  m_indices;
	std::vector<uint32>                   m_numIndices;
	stl::aligned_vector<Vec3, 16>         m_positions;
	stl::aligned_vector<UCol, 16>         m_colors;
	stl::aligned_vector<Vec2, 16>         m_texcoords;
	stl::aligned_vector<SPipTangents, 16> m_tangents;
	std::vector<uint16>                   m_materialIds;
	std::vector<uint16>                   m_predictorData;
};

struct SGeomCacheStaticNodeData
{
	uint32                        m_meshOrGeometryIndex;
	uint32                        m_numChildren;
	GeomCacheFile::ENodeType      m_type;
	GeomCacheFile::ETransformType m_transformType;
	QuatTNS                       m_localTransform;
	uint32                        m_nameHash;
	string                        m_name;
};

class CGeomCacheStreamReader
{
public:
	CGeomCacheStreamReader(const char* pData, const size_t length)
		: m_pData(pData), m_length(length), m_position(0) {}

	template<class T>
	bool Read(T* pDest, size_t numElements)
	{
		const size_t numBytes = sizeof(T) * numElements;

		if (m_position + numBytes > m_length)
		{
			return false;
		}

		memcpy(pDest, &m_pData[m_position], numBytes);
		m_position += numBytes;

		return true;
	}

	template<class T>
	bool Read(T* pDest)
	{
		const size_t numBytes = sizeof(T);

		if (m_position + numBytes > m_length)
		{
			return false;
		}

		memcpy(pDest, &m_pData[m_position], numBytes);
		m_position += numBytes;

		return true;
	}

private:
	const char*  m_pData;
	const size_t m_length;
	size_t       m_position;
};

struct IGeomCacheListener
{
public:
	virtual ~IGeomCacheListener() {}

	virtual void OnGeomCacheStaticDataLoaded() = 0;
	virtual void OnGeomCacheStaticDataUnloaded() = 0;
};

class CGeomCache : public IGeomCache, public IStreamCallback, public Cry3DEngineBase
{
	friend class CGeomCacheManager;

public:
	CGeomCache(const char* pFileName);
	~CGeomCache();

	// Gets number of frames
	uint GetNumFrames() const;

	// Returns true if cache plays back from memory
	bool        PlaybackFromMemory() const;

	const char* GetFrameData(const uint frameIndex) const;
	uint64      GetCompressedAnimationDataSize() const;

	// Gets the max extend of the geom cache through the entire animation
	AABB GetAABB() const;

	// Returns frame for specific time. Rounds to ceil or floor
	uint GetFloorFrameIndex(const float time) const;
	uint GetCeilFrameIndex(const float time) const;

	// Frame infos
	GeomCacheFile::EFrameType GetFrameType(const uint frameIndex) const;
	uint64                    GetFrameOffset(const uint frameIndex) const;
	uint32                    GetFrameSize(const uint frameIndex) const;
	float                     GetFrameTime(const uint frameIndex) const;
	uint                      GetPrevIFrame(const uint frameIndex) const;
	uint                      GetNextIFrame(const uint frameIndex) const;

	// Returns true if this frame uses motion prediction and needs the last two frames
	bool NeedsPrevFrames(const uint frameIndex) const;

	// Validates a frame range for reading from disk
	void ValidateReadRange(const uint start, uint& end) const;

	// Get block compression format
	GeomCacheFile::EBlockCompressionFormat GetBlockCompressionFormat() const;

	// Access to the mesh and node lists
	const std::vector<SGeomCacheStaticMeshData>& GetStaticMeshData() const    { return m_staticMeshData; }
	const std::vector<SGeomCacheStaticNodeData>& GetStaticNodeData() const    { return m_staticNodeData; }
	const std::vector<phys_geometry*>&           GetPhysicsGeometries() const { return m_physicsGeometries; }

	// Listener interface for async loading
	void AddListener(IGeomCacheListener* pListener);
	void RemoveListener(IGeomCacheListener* pListener);

	bool IsLoaded() const { return m_bLoaded; }
	void UnloadData();

	// Ref count for streams
	uint GetNumStreams() const { return m_numStreams; }
	void IncreaseNumStreams()  { ++m_numStreams; }
	void DecreaseNumStreams()  { --m_numStreams; }

	// IGeomCache
	virtual int                         AddRef();
	virtual int                         Release();

	virtual bool                        IsValid() const { return m_bValid; }

	virtual void                        SetMaterial(IMaterial* pMaterial);
	virtual _smart_ptr<IMaterial>       GetMaterial();
	virtual const _smart_ptr<IMaterial> GetMaterial() const;

	virtual const char*                 GetFilePath() const;

	virtual float                       GetDuration() const;

	virtual IGeomCache::SStatistics     GetStatistics() const;

	virtual void                        Reload();

	// Static data streaming
	void UpdateStreamableComponents(float importance, const Matrix34A& objMatrix, IRenderNode* pRenderNode, bool bFullUpdate);
	void SetLastDrawMainFrameId(const uint32 id) { m_lastDrawMainFrameId = id; }

	// IStreamable
	virtual void   StartStreaming(bool bFinishNow, IReadStream_AutoPtr* ppStream);
	virtual int    GetStreamableContentMemoryUsage(bool bJustForDebug);
	virtual void   ReleaseStreamableContent();
	virtual void   GetStreamableName(string& sName);
	virtual uint32 GetLastDrawMainFrameId();
	virtual bool   IsUnloadable() const;

	// IStreamCallback
	virtual void StreamOnComplete(IReadStream* pStream, unsigned nError);
	virtual void StreamAsyncOnComplete(IReadStream* pStream, unsigned nError);

private:
	struct SFrameInfo
	{
		float  m_frameTime;
		uint32 m_frameType;
		uint32 m_frameSize;
		uint32 m_prevIFrame;
		uint32 m_nextIFrame;
		uint64 m_frameOffset;
	};

	void Shutdown();

	bool LoadGeomCache();

	bool ReadFrameInfos(FILE* pFile, const uint32 numFrames);

	bool ReadStaticBlock(FILE* pFile, GeomCacheFile::EBlockCompressionFormat compressionFormat, std::vector<char>& compressedData);
	bool DecompressStaticBlock(GeomCacheFile::EBlockCompressionFormat compressionFormat, const char* pCompressedData, std::vector<char>& decompressedData);

	bool ReadMeshesStaticData(CGeomCacheStreamReader& reader, const char* pFileName);
	bool ReadMeshStaticData(CGeomCacheStreamReader& reader, const GeomCacheFile::SMeshInfo& meshInfo,
	                        SGeomCacheStaticMeshData& mesh, const char* pFileName);

	bool        LoadAnimatedData(const char* pData, const size_t bufferOffset);

	bool        ReadNodesStaticDataRec(CGeomCacheStreamReader& reader);

	static bool CompareFrameTimes(const SFrameInfo& a, const SFrameInfo& b)
	{
		return a.m_frameTime < b.m_frameTime;
	}

	char* GetFrameData(const uint frameIndex);

	bool                  m_bValid;
	bool                  m_bLoaded;

	int                   m_refCount;
	_smart_ptr<IMaterial> m_pMaterial;
	string                m_fileName;
	string                m_lastError;

	// Static data streaming state
	bool           m_bUseStreaming;
	uint32         m_lastDrawMainFrameId;
	IReadStreamPtr m_pStaticDataReadStream;

	// Cache block compression format
	GeomCacheFile::EBlockCompressionFormat m_blockCompressionFormat;

	// Playback from memory flag
	bool m_bPlaybackFromMemory;

	// Number of frames
	uint m_numFrames;

	// Number of streams reading from this cache
	uint m_numStreams;

	// Offset of static mesh data
	uint64 m_staticMeshDataOffset;

	// Total size of animated data
	uint64 m_compressedAnimationDataSize;

	// Total size of uncompressed animation data
	uint64 m_totalUncompressedAnimationSize;

	// AABB of entire animation
	AABB m_aabb;

	// Static data size;
	GeomCacheFile::SCompressedBlockHeader m_staticDataHeader;

	// Frame infos
	std::vector<SFrameInfo>               m_frameInfos;

	std::vector<SGeomCacheStaticMeshData> m_staticMeshData;
	std::vector<SGeomCacheStaticNodeData> m_staticNodeData;

	// Physics
	std::vector<phys_geometry*> m_physicsGeometries;

	// Holds references of static render meshes until cache object dies
	std::vector<_smart_ptr<IRenderMesh>> m_staticRenderMeshes;

	// Listeners
	std::vector<IGeomCacheListener*> m_listeners;

	// Animation data (memory playback)
	std::vector<char> m_animationData;
};

#endif
#endif
