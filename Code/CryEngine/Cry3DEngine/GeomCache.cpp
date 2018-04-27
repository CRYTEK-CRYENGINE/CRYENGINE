// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ------------------------------------------------------------------------
//  File name:   GeomCache.cpp
//  Created:     19/7/2012 by Axel Gneiting
//  Description: Manages geometry cache data
// -------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#if defined(USE_GEOM_CACHES)

	#include "GeomCache.h"
	#include "GeomCacheManager.h"
	#include "GeomCacheDecoder.h"
	#include <CryString/StringUtils.h>
	#include "MatMan.h"

CGeomCache::CGeomCache(const char* pFileName)
	: m_bValid(false)
	, m_bLoaded(false)
	, m_refCount(0)
	, m_fileName(pFileName)
	, m_lastDrawMainFrameId(0)
	, m_bPlaybackFromMemory(false)
	, m_numFrames(0)
	, m_compressedAnimationDataSize(0)
	, m_totalUncompressedAnimationSize(0)
	, m_numStreams(0)
	, m_staticMeshDataOffset(0)
	, m_aabb(0.0f)
{
	m_bUseStreaming = GetCVars()->e_StreamCgf > 0;
	m_staticDataHeader.m_compressedSize = 0;
	m_staticDataHeader.m_uncompressedSize = 0;

	string materialPath = PathUtil::ReplaceExtension(m_fileName, "mtl");
	m_pMaterial = GetMatMan()->LoadMaterial(materialPath);

	if (!LoadGeomCache())
	{
		FileWarning(0, m_fileName.c_str(), "Failed to load geometry cache: %s", m_lastError.c_str());
		stl::free_container(m_lastError);
	}
}

CGeomCache::~CGeomCache()
{
	Shutdown();
}

int CGeomCache::AddRef()
{
	++m_refCount;
	return m_refCount;
}

int CGeomCache::Release()
{
	assert(m_refCount >= 0);

	--m_refCount;
	int refCount = m_refCount;
	if (m_refCount <= 0)
	{
		GetGeomCacheManager()->DeleteGeomCache(this);
	}

	return refCount;
}

void CGeomCache::SetMaterial(IMaterial* pMaterial)
{
	m_pMaterial = pMaterial;
}

_smart_ptr<IMaterial> CGeomCache::GetMaterial()
{
	return m_pMaterial;
}

const _smart_ptr<IMaterial> CGeomCache::GetMaterial() const
{
	return m_pMaterial;
}

const char* CGeomCache::GetFilePath() const
{
	return m_fileName;
}

class CScopedFileHandle
{
public:
	CScopedFileHandle(const string& fileName, const char* mode)
	{
		m_pHandle = gEnv->pCryPak->FOpen(fileName, mode);
	}

	~CScopedFileHandle()
	{
		if (m_pHandle)
		{
			gEnv->pCryPak->FClose(m_pHandle);
		}
	}

	operator FILE*()
	{
		return m_pHandle;
	}

	bool IsValid() const
	{
		return m_pHandle != NULL;
	}

private:
	FILE* m_pHandle;
};

bool CGeomCache::LoadGeomCache()
{
	using namespace GeomCacheFile;

	FUNCTION_PROFILER_3DENGINE;

	CRY_DEFINE_ASSET_SCOPE("GeomCache", m_fileName);

	#if INCLUDE_MEMSTAT_CONTEXTS
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_GeomCache, EMemStatContextFlags::MSF_Instance, "%s", m_fileName.c_str());
	#endif

	CScopedFileHandle geomCacheFileHandle(m_fileName, "rb");
	if (!geomCacheFileHandle.IsValid())
	{
		return false;
	}

	size_t bytesRead = 0;

	// Read header and check signature
	SHeader header;
	bytesRead = gEnv->pCryPak->FReadRaw((void*)&header, 1, sizeof(SHeader), geomCacheFileHandle);
	if (bytesRead != sizeof(SHeader))
	{
		m_lastError = "Could not read header";
		return false;
	}

	if (header.m_signature != kFileSignature)
	{
		m_lastError = "Bad file signature";
		return false;
	}

	if (header.m_versionGuidHipart != kCurrentVersion.hipart
		|| header.m_versionGuidLopart != kCurrentVersion.lopart)
	{
		m_lastError = "Bad file version";
		return false;
	}

	const bool bFileHas32BitIndexFormat = (header.m_flags & eFileHeaderFlags_32BitIndices) != 0;
	if (((sizeof(vtx_idx) == sizeof(uint16)) && bFileHas32BitIndexFormat) || ((sizeof(vtx_idx) == sizeof(uint32)) && !bFileHas32BitIndexFormat))
	{
		m_lastError = "Index format mismatch";
		return false;
	}

	if (header.m_blockCompressionFormat != eBlockCompressionFormat_None &&
	    header.m_blockCompressionFormat != eBlockCompressionFormat_Deflate &&
	    header.m_blockCompressionFormat != eBlockCompressionFormat_LZ4HC)
	{
		m_lastError = "Bad block compression format";
		return false;
	}

	m_bPlaybackFromMemory = (header.m_flags & GeomCacheFile::eFileHeaderFlags_PlaybackFromMemory) != 0;
	m_blockCompressionFormat = static_cast<EBlockCompressionFormat>(header.m_blockCompressionFormat);
	m_totalUncompressedAnimationSize = header.m_totalUncompressedAnimationSize;
	m_numFrames = header.m_numFrames;
	m_aabb.min = Vec3(header.m_aabbMin[0], header.m_aabbMin[1], header.m_aabbMin[2]);
	m_aabb.max = Vec3(header.m_aabbMax[0], header.m_aabbMax[1], header.m_aabbMax[2]);

	// Read frame times and frame offsets.
	if (!ReadFrameInfos(geomCacheFileHandle, header.m_numFrames))
	{
		return false;
	}

	const int maxPlaybackFromMemorySize = std::max(0, GetCVars()->e_GeomCacheMaxPlaybackFromMemorySize);
	if (m_bPlaybackFromMemory && m_compressedAnimationDataSize > uint(maxPlaybackFromMemorySize * 1024 * 1024))
	{
		GetLog()->LogWarning("Animated data size of geometry cache '%s' is over memory playback limit "
		                     "of %d MiB. Reverting to stream playback.", m_fileName.c_str(), maxPlaybackFromMemorySize);
		m_bPlaybackFromMemory = false;
	}

	// Load static node data and physics geometries
	{
		std::vector<char> compressedData;
		std::vector<char> decompressedData;

		if (!ReadStaticBlock(geomCacheFileHandle, (EBlockCompressionFormat)(header.m_blockCompressionFormat), compressedData)
		    || !DecompressStaticBlock((EBlockCompressionFormat)(header.m_blockCompressionFormat), &compressedData[0], decompressedData))
		{
			if (m_lastError.empty())
			{
				m_lastError = "Could not read or decompress static block";
			}

			return false;
		}

		CGeomCacheStreamReader reader(&decompressedData[0], decompressedData.size());
		if (!ReadNodesStaticDataRec(reader))
		{
			if (m_lastError.empty())
			{
				m_lastError = "Could not read node static data";
			}

			return false;
		}
	}

	m_staticMeshDataOffset = gEnv->pCryPak->FTell(geomCacheFileHandle);

	if (!m_bUseStreaming)
	{
		std::vector<char> compressedData;
		std::vector<char> decompressedData;

		if (!ReadStaticBlock(geomCacheFileHandle, (EBlockCompressionFormat)(header.m_blockCompressionFormat), compressedData)
		    || !DecompressStaticBlock((EBlockCompressionFormat)(header.m_blockCompressionFormat), &compressedData[0], decompressedData))
		{
			if (m_lastError.empty())
			{
				m_lastError = "Could not read or decompress static block";
			}

			return false;
		}

		CGeomCacheStreamReader reader(&decompressedData[0], decompressedData.size());
		if (!ReadMeshesStaticData(reader, m_fileName.c_str()))
		{
			if (m_lastError.empty())
			{
				m_lastError = "Could not read mesh static data";
			}

			return false;
		}

		if (m_bPlaybackFromMemory && m_frameInfos.size() > 0)
		{
			std::vector<char> animationData;
			animationData.resize(static_cast<size_t>(m_compressedAnimationDataSize));
			bytesRead = gEnv->pCryPak->FReadRaw((void*)&animationData[0], 1, static_cast<uint>(m_compressedAnimationDataSize), geomCacheFileHandle);

			if (!LoadAnimatedData(&animationData[0], 0))
			{
				return false;
			}
		}

		m_bLoaded = true;
	}
	else
	{
		bytesRead = gEnv->pCryPak->FReadRaw((void*)&m_staticDataHeader, 1, sizeof(GeomCacheFile::SCompressedBlockHeader), geomCacheFileHandle);
		if (bytesRead != sizeof(GeomCacheFile::SCompressedBlockHeader))
		{
			m_lastError = "Bad data";
			return false;
		}
	}

	m_bValid = true;
	return true;
}

bool CGeomCache::ReadStaticBlock(FILE* pFile, GeomCacheFile::EBlockCompressionFormat compressionFormat, std::vector<char>& compressedData)
{
	compressedData.resize(sizeof(GeomCacheFile::SCompressedBlockHeader));

	size_t bytesRead = 0;
	bytesRead = gEnv->pCryPak->FReadRaw((void*)&compressedData[0], 1, sizeof(GeomCacheFile::SCompressedBlockHeader), pFile);
	if (bytesRead != sizeof(GeomCacheFile::SCompressedBlockHeader))
	{
		return false;
	}

	m_staticDataHeader = *reinterpret_cast<GeomCacheFile::SCompressedBlockHeader*>(&compressedData[0]);
	compressedData.resize(sizeof(GeomCacheFile::SCompressedBlockHeader) + m_staticDataHeader.m_compressedSize);

	bytesRead = gEnv->pCryPak->FReadRaw(&compressedData[sizeof(GeomCacheFile::SCompressedBlockHeader)], 1, m_staticDataHeader.m_compressedSize, pFile);
	if (bytesRead != m_staticDataHeader.m_compressedSize)
	{
		return false;
	}

	return true;
}

bool CGeomCache::DecompressStaticBlock(GeomCacheFile::EBlockCompressionFormat compressionFormat, const char* pCompressedData, std::vector<char>& decompressedData)
{
	const GeomCacheFile::SCompressedBlockHeader staticBlockHeader = *reinterpret_cast<const GeomCacheFile::SCompressedBlockHeader*>(pCompressedData);
	decompressedData.resize(staticBlockHeader.m_uncompressedSize);

	if (!GeomCacheDecoder::DecompressBlock(compressionFormat, &decompressedData[0], pCompressedData))
	{
		m_lastError = "Could not decompress static data";
		return false;
	}

	return true;
}

bool CGeomCache::ReadFrameInfos(FILE* pFile, const uint32 numFrames)
{
	FUNCTION_PROFILER_3DENGINE;

	size_t bytesRead;

	std::vector<GeomCacheFile::SFrameInfo> fileFrameInfos;

	fileFrameInfos.resize(numFrames);
	bytesRead = gEnv->pCryPak->FReadRaw((void*)&fileFrameInfos[0], 1, numFrames * sizeof(GeomCacheFile::SFrameInfo), pFile);
	if (bytesRead != (numFrames * sizeof(GeomCacheFile::SFrameInfo)))
	{
		return false;
	}

	m_frameInfos.resize(numFrames);
	for (uint i = 0; i < numFrames; ++i)
	{
		m_frameInfos[i].m_frameTime = fileFrameInfos[i].m_frameTime;
		m_frameInfos[i].m_frameType = fileFrameInfos[i].m_frameType;
		m_frameInfos[i].m_frameSize = fileFrameInfos[i].m_frameSize;
		m_frameInfos[i].m_frameOffset = fileFrameInfos[i].m_frameOffset;
		m_compressedAnimationDataSize += m_frameInfos[i].m_frameSize;
	}

	if (m_frameInfos.front().m_frameType != GeomCacheFile::eFrameType_IFrame
	    || m_frameInfos.back().m_frameType != GeomCacheFile::eFrameType_IFrame)
	{
		return false;
	}

	// Assign prev/next index frames and count total data size
	uint prevIFrame = 0;
	for (uint i = 0; i < numFrames; ++i)
	{
		m_frameInfos[i].m_prevIFrame = prevIFrame;

		if (m_frameInfos[i].m_frameType == GeomCacheFile::eFrameType_IFrame)
		{
			prevIFrame = i;
		}
	}

	uint nextIFrame = numFrames - 1;
	for (int i = numFrames - 1; i >= 0; --i)
	{
		m_frameInfos[i].m_nextIFrame = nextIFrame;

		if (m_frameInfos[i].m_frameType == GeomCacheFile::eFrameType_IFrame)
		{
			nextIFrame = i;
		}
	}

	return true;
}

bool CGeomCache::ReadMeshesStaticData(CGeomCacheStreamReader& reader, const char* pFileName)
{
	FUNCTION_PROFILER_3DENGINE;

	uint32 numMeshes;
	if (!reader.Read(&numMeshes))
	{
		return false;
	}

	std::vector<GeomCacheFile::SMeshInfo> meshInfos;
	meshInfos.reserve(numMeshes);

	for (uint32 i = 0; i < numMeshes; ++i)
	{
		GeomCacheFile::SMeshInfo meshInfo;
		m_staticMeshData.push_back(SGeomCacheStaticMeshData());
		SGeomCacheStaticMeshData& staticMeshData = m_staticMeshData.back();

		if (!reader.Read(&meshInfo))
		{
			return false;
		}

		staticMeshData.m_bUsePredictor = (meshInfo.m_flags & GeomCacheFile::eMeshIFrameFlags_UsePredictor) != 0;
		staticMeshData.m_positionPrecision[0] = meshInfo.m_positionPrecision[0];
		staticMeshData.m_positionPrecision[1] = meshInfo.m_positionPrecision[1];
		staticMeshData.m_positionPrecision[2] = meshInfo.m_positionPrecision[2];
		staticMeshData.m_constantStreams = static_cast<GeomCacheFile::EStreams>(meshInfo.m_constantStreams);
		staticMeshData.m_animatedStreams = static_cast<GeomCacheFile::EStreams>(meshInfo.m_animatedStreams);
		staticMeshData.m_numVertices = meshInfo.m_numVertices;
		staticMeshData.m_aabb.min = Vec3(meshInfo.m_aabbMin[0], meshInfo.m_aabbMin[1], meshInfo.m_aabbMin[2]);
		staticMeshData.m_aabb.max = Vec3(meshInfo.m_aabbMax[0], meshInfo.m_aabbMax[1], meshInfo.m_aabbMax[2]);
		staticMeshData.m_hash = meshInfo.m_hash;

		std::vector<char> tempName(meshInfo.m_nameLength);
		if (!reader.Read(&tempName[0], meshInfo.m_nameLength))
		{
			return false;
		}

		if (gEnv->IsEditor())
		{
			staticMeshData.m_name = &tempName[0];
		}

		// Read material IDs
		staticMeshData.m_materialIds.resize(meshInfo.m_numMaterials);

		if (!reader.Read(&staticMeshData.m_materialIds[0], meshInfo.m_numMaterials))
		{
			return false;
		}

		meshInfos.push_back(meshInfo);
	}

	m_staticMeshData.reserve(numMeshes);
	for (uint32 i = 0; i < numMeshes; ++i)
	{
		if (!ReadMeshStaticData(reader, meshInfos[i], m_staticMeshData[i], pFileName))
		{
			return false;
		}
	}

	return true;
}

bool CGeomCache::ReadMeshStaticData(CGeomCacheStreamReader& reader, const GeomCacheFile::SMeshInfo& meshInfo,
                                    SGeomCacheStaticMeshData& staticMeshData, const char* pFileName)
{
	CGeomCacheMeshManager& meshManager = GetGeomCacheManager()->GetMeshManager();

	if (meshInfo.m_animatedStreams == 0)
	{
		// If we don't need the static data to fill dynamic meshes, just construct a static mesh
		_smart_ptr<IRenderMesh> pRenderMesh = meshManager.ConstructStaticRenderMesh(reader, meshInfo, staticMeshData, pFileName);

		if (!pRenderMesh)
		{
			return false;
		}

		m_staticRenderMeshes.push_back(pRenderMesh);
	}
	else
	{
		// Otherwise we need the static data for filling the dynamic mesh later and read it to the vertex array in staticMeshData
		if (!meshManager.ReadMeshStaticData(reader, meshInfo, staticMeshData))
		{
			return false;
		}
	}

	return true;
}

bool CGeomCache::ReadNodesStaticDataRec(CGeomCacheStreamReader& reader)
{
	FUNCTION_PROFILER_3DENGINE;

	GeomCacheFile::SNodeInfo nodeInfo;
	if (!reader.Read(&nodeInfo))
	{
		return false;
	}

	SGeomCacheStaticNodeData staticNodeData;
	staticNodeData.m_meshOrGeometryIndex = nodeInfo.m_meshIndex;
	staticNodeData.m_numChildren = nodeInfo.m_numChildren;
	staticNodeData.m_type = static_cast<GeomCacheFile::ENodeType>(nodeInfo.m_type);
	staticNodeData.m_transformType = static_cast<GeomCacheFile::ETransformType>(nodeInfo.m_transformType);

	std::vector<char> tempName(nodeInfo.m_nameLength);
	if (!reader.Read(&tempName[0], nodeInfo.m_nameLength))
	{
		return false;
	}

	if (gEnv->IsEditor())
	{
		staticNodeData.m_name = &tempName[0];
	}

	staticNodeData.m_nameHash = CryStringUtils::HashString(&tempName[0]);

	if (!reader.Read(&staticNodeData.m_localTransform))
	{
		return false;
	}

	if (staticNodeData.m_type == GeomCacheFile::eNodeType_PhysicsGeometry)
	{
		uint32 geometrySize;
		if (!reader.Read(&geometrySize))
		{
			return false;
		}

		std::vector<char> geometryData(geometrySize);
		if (!reader.Read(&geometryData[0], geometrySize))
		{
			return false;
		}

		CMemStream memStream(&geometryData[0], geometrySize, false);
		phys_geometry* pGeometry = GetPhysicalWorld()->GetGeomManager()->LoadPhysGeometry(memStream, 0, 0, 0);
		m_physicsGeometries.push_back(pGeometry);

		staticNodeData.m_meshOrGeometryIndex = (uint32)(m_physicsGeometries.size() - 1);
	}

	m_staticNodeData.push_back(staticNodeData);

	for (uint32 i = 0; i < nodeInfo.m_numChildren; ++i)
	{
		if (!ReadNodesStaticDataRec(reader))
		{
			return false;
		}
	}

	return true;
}

float CGeomCache::GetDuration() const
{
	if (m_frameInfos.empty())
	{
		return 0.0f;
	}
	else
	{
		return m_frameInfos.back().m_frameTime - m_frameInfos.front().m_frameTime;
	}
}

IGeomCache::SStatistics CGeomCache::GetStatistics() const
{
	IGeomCache::SStatistics stats;
	memset(&stats, 0, sizeof(stats));

	std::set<uint16> materialIds;

	CGeomCacheMeshManager& meshManager = GetGeomCacheManager()->GetMeshManager();

	const uint numMeshes = m_staticMeshData.size();
	for (uint i = 0; i < numMeshes; ++i)
	{
		const SGeomCacheStaticMeshData& meshData = m_staticMeshData[i];
		materialIds.insert(meshData.m_materialIds.begin(), meshData.m_materialIds.end());

		stats.m_staticDataSize += static_cast<uint>(sizeof(SGeomCacheStaticMeshData));
		stats.m_staticDataSize += static_cast<uint>(sizeof(vtx_idx) * meshData.m_indices.size());
		stats.m_staticDataSize += static_cast<uint>(sizeof(uint32) * meshData.m_numIndices.size());
		stats.m_staticDataSize += static_cast<uint>(sizeof(Vec3) * meshData.m_positions.size());
		stats.m_staticDataSize += static_cast<uint>(sizeof(UCol) * meshData.m_colors.size());
		stats.m_staticDataSize += static_cast<uint>(sizeof(Vec2) * meshData.m_texcoords.size());
		stats.m_staticDataSize += static_cast<uint>(sizeof(SPipTangents) * meshData.m_tangents.size());
		stats.m_staticDataSize += static_cast<uint>(sizeof(uint16) * meshData.m_materialIds.size());
		stats.m_staticDataSize += static_cast<uint>(sizeof(uint16) * meshData.m_predictorData.size());

		uint numIndices = 0;
		const uint numMaterials = meshData.m_numIndices.size();
		for (uint j = 0; j < numMaterials; ++j)
		{
			numIndices += meshData.m_numIndices[j];
		}

		if (meshData.m_animatedStreams == 0)
		{
			++stats.m_numStaticMeshes;
			stats.m_numStaticVertices += meshData.m_numVertices;
			stats.m_numStaticTriangles += numIndices / 3;
		}
		else
		{
			++stats.m_numAnimatedMeshes;
			stats.m_numAnimatedVertices += meshData.m_numVertices;
			stats.m_numAnimatedTriangles += numIndices / 3;
		}
	}

	stats.m_staticDataSize += static_cast<uint>(m_staticNodeData.size() * sizeof(SGeomCacheStaticNodeData));
	stats.m_staticDataSize += static_cast<uint>(m_frameInfos.size() * sizeof(SFrameInfo));

	stats.m_bPlaybackFromMemory = m_bPlaybackFromMemory;
	stats.m_averageAnimationDataRate = (float(m_compressedAnimationDataSize) / 1024.0f / 1024.0f) / float(GetDuration());
	stats.m_numMaterials = static_cast<uint>(materialIds.size());
	stats.m_diskAnimationDataSize = static_cast<uint>(m_compressedAnimationDataSize);
	stats.m_memoryAnimationDataSize = static_cast<uint>(m_animationData.size());

	return stats;
}

void CGeomCache::Shutdown()
{
	uint numPhysicsGeometries = m_physicsGeometries.size();
	for (uint i = 0; i < numPhysicsGeometries; ++i)
	{
		phys_geometry* pGeometry = m_physicsGeometries[i];
		if (pGeometry)
		{
			GetPhysicalWorld()->GetGeomManager()->UnregisterGeometry(pGeometry);
		}
	}

	if (m_pStaticDataReadStream)
	{
		m_pStaticDataReadStream->Abort();
		m_pStaticDataReadStream = NULL;
	}

	GetObjManager()->UnregisterForStreaming(this);
	GetGeomCacheManager()->StopCacheStreamsAndWait(this);

	const uint numListeners = m_listeners.size();
	for (uint i = 0; i < numListeners; ++i)
	{
		m_listeners[i]->OnGeomCacheStaticDataUnloaded();
	}

	m_eStreamingStatus = ecss_NotLoaded;

	stl::free_container(m_frameInfos);
	stl::free_container(m_staticMeshData);
	stl::free_container(m_staticNodeData);
	stl::free_container(m_physicsGeometries);
	stl::free_container(m_animationData);
}

void CGeomCache::Reload()
{
	Shutdown();

	const bool bUseStreaming = m_bUseStreaming;
	m_bUseStreaming = false;
	m_bValid = false;
	m_bLoaded = false;
	LoadGeomCache();
	m_bUseStreaming = bUseStreaming;

	if (m_bLoaded)
	{
		const uint numListeners = m_listeners.size();
		for (uint i = 0; i < numListeners; ++i)
		{
			m_listeners[i]->OnGeomCacheStaticDataLoaded();
		}
	}
	else
	{
		FileWarning(0, m_fileName.c_str(), "Failed to load geometry cache: %s", m_lastError.c_str());
		stl::free_container(m_lastError);
	}
}

uint CGeomCache::GetNumFrames() const
{
	return m_frameInfos.size();
}

bool CGeomCache::PlaybackFromMemory() const
{
	return m_bPlaybackFromMemory;
}

uint64 CGeomCache::GetCompressedAnimationDataSize() const
{
	return m_compressedAnimationDataSize;
}

char* CGeomCache::GetFrameData(const uint frameIndex)
{
	assert(m_bPlaybackFromMemory);

	char* pAnimationData = &m_animationData[0];

	SGeomCacheFrameHeader* pFrameHeader = reinterpret_cast<SGeomCacheFrameHeader*>(
	  pAnimationData + (frameIndex * sizeof(SGeomCacheFrameHeader)));

	return pAnimationData + pFrameHeader->m_offset;
}

const char* CGeomCache::GetFrameData(const uint frameIndex) const
{
	assert(m_bPlaybackFromMemory);

	const char* pAnimationData = &m_animationData[0];

	const SGeomCacheFrameHeader* pFrameHeader = reinterpret_cast<const SGeomCacheFrameHeader*>(
	  pAnimationData + (frameIndex * sizeof(SGeomCacheFrameHeader)));

	return pAnimationData + pFrameHeader->m_offset;
}

AABB CGeomCache::GetAABB() const
{
	return m_aabb;
}

uint CGeomCache::GetFloorFrameIndex(const float time) const
{
	if (m_frameInfos.empty())
	{
		return 0;
	}

	const float duration = GetDuration();
	float timeInCycle = fmod(time, duration);
	uint numLoops = static_cast<uint>(floor(time / duration));

	// Make sure that exactly at wrap around we still refer to last frame
	if (timeInCycle == 0.0f && time > 0.0f)
	{
		timeInCycle = duration;
		numLoops -= 1;
	}

	const uint numFrames = m_frameInfos.size();
	const uint numPreviousCycleFrames = numLoops * numFrames;
	uint frameInCycle;

	// upper_bound = first value greater than time
	SFrameInfo value = { timeInCycle };
	std::vector<SFrameInfo>::const_iterator findIter =
	  std::upper_bound(m_frameInfos.begin(), m_frameInfos.end(), value, CompareFrameTimes);
	if (findIter == m_frameInfos.begin())
	{
		frameInCycle = 0;
	}
	else if (findIter == m_frameInfos.end())
	{
		frameInCycle = static_cast<uint>(m_frameInfos.size() - 1);
	}
	else
	{
		frameInCycle = static_cast<uint>(findIter - m_frameInfos.begin() - 1);
	}

	return frameInCycle + numPreviousCycleFrames;
}

uint CGeomCache::GetCeilFrameIndex(const float time) const
{
	if (m_frameInfos.empty())
	{
		return 0;
	}

	const float duration = GetDuration();
	float timeInCycle = fmod(time, duration);
	uint numLoops = static_cast<uint>(floor(time / duration));

	// Make sure that exactly at wrap around we still refer to last frame
	if (timeInCycle == 0.0f && time > 0.0f)
	{
		timeInCycle = duration;
		numLoops -= 1;
	}

	const uint numFrames = m_frameInfos.size();
	const uint numPreviousCycleFrames = numLoops * numFrames;
	uint frameInCycle;

	// lower_bound = first value greater than or equal to time
	SFrameInfo value = { timeInCycle };
	std::vector<SFrameInfo>::const_iterator findIter =
	  std::lower_bound(m_frameInfos.begin(), m_frameInfos.end(), value, CompareFrameTimes);
	if (findIter == m_frameInfos.end())
	{
		frameInCycle = static_cast<uint>(m_frameInfos.size() - 1);
	}
	else
	{
		frameInCycle = static_cast<uint>(findIter - m_frameInfos.begin());
	}

	return frameInCycle + numPreviousCycleFrames;
}

GeomCacheFile::EFrameType CGeomCache::GetFrameType(const uint frameIndex) const
{
	const uint numFrames = m_frameInfos.size();
	return (GeomCacheFile::EFrameType)m_frameInfos[frameIndex % numFrames].m_frameType;
}

uint64 CGeomCache::GetFrameOffset(const uint frameIndex) const
{
	const uint numFrames = m_frameInfos.size();
	return m_frameInfos[frameIndex % numFrames].m_frameOffset;
}

uint32 CGeomCache::GetFrameSize(const uint frameIndex) const
{
	const uint numFrames = m_frameInfos.size();
	return m_frameInfos[frameIndex % numFrames].m_frameSize;
}

float CGeomCache::GetFrameTime(const uint frameIndex) const
{
	const uint numFrames = m_frameInfos.size();
	const uint numLoops = frameIndex / numFrames;
	const float duration = GetDuration();
	return (duration * numLoops) + m_frameInfos[frameIndex % numFrames].m_frameTime;
}

uint CGeomCache::GetPrevIFrame(const uint frameIndex) const
{
	const uint numFrames = m_frameInfos.size();
	const uint numLoops = frameIndex / numFrames;
	return (numFrames * numLoops) + m_frameInfos[frameIndex % numFrames].m_prevIFrame;
}

uint CGeomCache::GetNextIFrame(const uint frameIndex) const
{
	const uint numFrames = m_frameInfos.size();
	const uint numLoops = frameIndex / numFrames;
	return (numFrames * numLoops) + m_frameInfos[frameIndex % numFrames].m_nextIFrame;
}

bool CGeomCache::NeedsPrevFrames(const uint frameIndex) const
{
	if (GetFrameType(frameIndex) == GeomCacheFile::eFrameType_IFrame
	    || GetFrameType(frameIndex - 1) == GeomCacheFile::eFrameType_IFrame)
	{
		return false;
	}

	return true;
}

void CGeomCache::ValidateReadRange(const uint start, uint& end) const
{
	const uint numFrames = m_frameInfos.size();
	uint startMod = start % numFrames;
	uint endMod = end % numFrames;

	if (endMod < startMod)
	{
		end = start + (numFrames - 1 - startMod);
	}
}

GeomCacheFile::EBlockCompressionFormat CGeomCache::GetBlockCompressionFormat() const
{
	return m_blockCompressionFormat;
}

void CGeomCache::UpdateStreamableComponents(float importance, const Matrix34A& objMatrix, IRenderNode* pRenderNode, bool bFullUpdate)
{
	if (!m_bUseStreaming)
	{
		return;
	}

	const int nRoundId = GetObjManager()->m_nUpdateStreamingPrioriryRoundId;

	if (UpdateStreamingPrioriryLowLevel(importance, nRoundId, bFullUpdate))
	{
		GetObjManager()->RegisterForStreaming(this);
	}
}

void CGeomCache::StartStreaming(bool bFinishNow, IReadStream_AutoPtr* ppStream)
{
	m_bValid = false;

	assert(m_eStreamingStatus == ecss_NotLoaded);
	if (m_eStreamingStatus != ecss_NotLoaded)
	{
		return;
	}

	if (m_bLoaded)
	{
		const uint numListeners = m_listeners.size();
		for (uint i = 0; i < numListeners; ++i)
		{
			m_listeners[i]->OnGeomCacheStaticDataLoaded();
		}

		m_eStreamingStatus = ecss_Ready;
		return;
	}

	// start streaming
	StreamReadParams params;
	params.dwUserData = 0;
	params.nOffset = static_cast<uint>(m_staticMeshDataOffset);
	params.nSize = sizeof(GeomCacheFile::SCompressedBlockHeader) + m_staticDataHeader.m_compressedSize;
	params.pBuffer = NULL;
	params.nLoadTime = 10000;
	params.nMaxLoadTime = 10000;

	if (m_bPlaybackFromMemory)
	{
		params.nSize += static_cast<uint>(m_compressedAnimationDataSize);
	}

	if (bFinishNow)
	{
		params.ePriority = estpUrgent;
	}

	if (m_fileName.empty())
	{
		m_eStreamingStatus = ecss_Ready;
		if (ppStream) *ppStream = NULL;
		return;
	}

	m_pStaticDataReadStream = GetSystem()->GetStreamEngine()->StartRead(eStreamTaskTypeGeometry, m_fileName, this, &params);

	if (ppStream)
	{
		(*ppStream) = m_pStaticDataReadStream;
	}

	if (!bFinishNow)
	{
		m_eStreamingStatus = ecss_InProgress;
	}
	else if (!ppStream)
	{
		m_pStaticDataReadStream->Wait();
	}
}

void CGeomCache::StreamOnComplete(IReadStream* pStream, unsigned nError)
{
	if (nError != 0 || !m_bValid)
	{
		return;
	}

	const uint numListeners = m_listeners.size();
	for (uint i = 0; i < numListeners; ++i)
	{
		m_listeners[i]->OnGeomCacheStaticDataLoaded();
	}

	m_eStreamingStatus = ecss_Ready;

	m_pStaticDataReadStream = NULL;
}

void CGeomCache::StreamAsyncOnComplete(IReadStream* pStream, unsigned nError)
{
	if (nError != 0)
	{
		return;
	}

	const char* pData = (char*)pStream->GetBuffer();

	std::vector<char> decompressedData;
	if (!DecompressStaticBlock((GeomCacheFile::EBlockCompressionFormat)(m_blockCompressionFormat), pData, decompressedData))
	{
		if (m_lastError.empty())
		{
			m_lastError = "Could not decompress static block";
			return;
		}
	}

	CGeomCacheStreamReader reader(&decompressedData[0], decompressedData.size());
	if (!ReadMeshesStaticData(reader, m_fileName.c_str()))
	{
		if (m_lastError.empty())
		{
			m_lastError = "Could not read mesh static data";
			return;
		}
	}

	if (m_bPlaybackFromMemory && m_frameInfos.size() > 0)
	{
		if (!LoadAnimatedData(pData, sizeof(GeomCacheFile::SCompressedBlockHeader) + m_staticDataHeader.m_compressedSize))
		{
			return;
		}
	}

	m_bValid = true;
	m_bLoaded = true;

	pStream->FreeTemporaryMemory();
}

bool CGeomCache::LoadAnimatedData(const char* pData, const size_t bufferOffset)
{
	const uint numFrames = m_frameInfos.size();

	// First get size necessary for decompressing animation data
	uint32 totalDecompressedAnimatedDataSize = GeomCacheDecoder::GetDecompressBufferSize(pData + bufferOffset, numFrames);

	// Allocate memory and decompress blocks into it
	m_animationData.resize(totalDecompressedAnimatedDataSize);
	if (!GeomCacheDecoder::DecompressBlocks(m_blockCompressionFormat, &m_animationData[0], pData + bufferOffset, 0, numFrames, numFrames))
	{
		m_lastError = "Could not decompress animation data";
		return false;
	}

	// Decode index frames
	for (uint i = 0; i < numFrames; ++i)
	{
		if (m_frameInfos[i].m_frameType == GeomCacheFile::eFrameType_IFrame)
		{
			GeomCacheDecoder::DecodeIFrame(this, GetFrameData(i));
		}
	}

	// Decode b-frames
	for (uint i = 0; i < numFrames; ++i)
	{
		if (m_frameInfos[i].m_frameType == GeomCacheFile::eFrameType_BFrame)
		{
			char* pFrameData = GetFrameData(i);
			char* pPrevFrameData[2] = { pFrameData, pFrameData };

			if (NeedsPrevFrames(i))
			{
				pPrevFrameData[0] = GetFrameData(i - 2);
				pPrevFrameData[1] = GetFrameData(i - 1);
			}

			char* pFloorIndexFrameData = GetFrameData(GetPrevIFrame(i));
			char* pCeilIndexFrameData = GetFrameData(GetNextIFrame(i));

			GeomCacheDecoder::DecodeBFrame(this, pFrameData, pPrevFrameData, pFloorIndexFrameData, pCeilIndexFrameData);
		}
	}

	return true;
}

int CGeomCache::GetStreamableContentMemoryUsage(bool bJustForDebug)
{
	return 0;
}

void CGeomCache::ReleaseStreamableContent()
{
	const uint numListeners = m_listeners.size();
	for (uint i = 0; i < numListeners; ++i)
	{
		m_listeners[i]->OnGeomCacheStaticDataUnloaded();
	}

	// Data cannot be unloaded right away, because stream could still be active,
	// so we need to wait for a UnloadData callback from the geom cache managers
	m_eStreamingStatus = ecss_NotLoaded;
}

void CGeomCache::GetStreamableName(string& sName)
{
	sName = m_fileName;
}

uint32 CGeomCache::GetLastDrawMainFrameId()
{
	return m_lastDrawMainFrameId;
}

bool CGeomCache::IsUnloadable() const
{
	return m_bUseStreaming;
}

void CGeomCache::AddListener(IGeomCacheListener* pListener)
{
	stl::push_back_unique(m_listeners, pListener);
}

void CGeomCache::RemoveListener(IGeomCacheListener* pListener)
{
	stl::find_and_erase(m_listeners, pListener);
}

void CGeomCache::UnloadData()
{
	if (m_eStreamingStatus == ecss_NotLoaded)
	{
		CGeomCacheMeshManager& meshManager = GetGeomCacheManager()->GetMeshManager();

		uint numStaticMesh = m_staticMeshData.size();
		for (uint i = 0; i < numStaticMesh; ++i)
		{
			if (m_staticMeshData[i].m_animatedStreams == 0)
			{
				meshManager.RemoveReference(m_staticMeshData[i]);
			}
		}

		stl::free_container(m_staticRenderMeshes);
		stl::free_container(m_staticMeshData);
		stl::free_container(m_animationData);
		m_bLoaded = false;
	}
}

#endif
