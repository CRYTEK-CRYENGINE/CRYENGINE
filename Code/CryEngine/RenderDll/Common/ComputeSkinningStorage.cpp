#include "StdAfx.h"
#include "ComputeSkinningStorage.h"

namespace compute_skinning
{
#define CS_DEFAULT_CACHESIZE 100 * 1024 * 1024

CStorage::CStorage()
{
	m_cacheSize = 0;
	m_cacheCapacity = CS_DEFAULT_CACHESIZE;
}

CStorage::~CStorage()
{
	Release(m_vertexInMap);
	Release(m_indicesMap);
	Release(m_triangleAdjacencyMap);
	Release(m_morphsDeltasMap);
	Release(m_morphsBitfieldMap);
	Release(m_skinningMap);
	Release(m_skinningMapMap);
	Release(m_vertexOutMap);
	Release(m_tangentsNTMap);

	m_cacheSize = 0;
}

void CStorage::Release(ResourceMapType& container)
{
	for (auto it = container.begin(); it != container.end(); ++it)
	{
		if (it->second.buffer)
		{
			it->second.buffer->Release();
			delete it->second.buffer;
		}
	}
	container.clear();
}

void CStorage::HashCombine(uint64& seed, const uint64 val)
{
	const uint64 kMul = 0x9ddfea08eb382d69ULL;
	uint64 a = (val ^ seed) * kMul;
	a ^= (a >> 47);
	uint64 b = (seed ^ a) * kMul;
	b ^= (b >> 47);
	seed = b * kMul;
}

uint64 CStorage::GetHash(const char* name, const void* tag, const uint32 numElements, const uint32 elementSize)
{
	uint64 hash = 0;

	HashCombine(hash, CCrc32::ComputeLowercase(name));
	HashCombine(hash, numElements);
	HashCombine(hash, elementSize);

	if (tag)
		HashCombine(hash, (uintptr_t)tag);

	return hash;
}

CGpuBuffer* CStorage::GetResourceType(const SResourceHandle& handle, const ResourceMapType& container)
{
	auto it = container.find(handle.key);
	if (it != container.end())
	{
		return it->second.buffer;
	}

	return NULL;
}

CGpuBuffer* CStorage::GetResource(const SResourceHandle& handle)
{
	switch (handle.type)
	{
	case eType_VertexInSrv:
		return GetResourceType(handle, m_vertexInMap);
	case eType_IndicesSrv:
		return GetResourceType(handle, m_indicesMap);
	case eType_TriangleAdjacencyIndicesSrv:
		return GetResourceType(handle, m_triangleAdjacencyMap);
	case eType_MorphsDeltaSrv:
		return GetResourceType(handle, m_morphsDeltasMap);
	case eType_MorphsBitFieldSrv:
		return GetResourceType(handle, m_morphsBitfieldMap);
	case eType_SkinningSrv:
		return GetResourceType(handle, m_skinningMap);
	case eType_SkinningMapSrv:
		return GetResourceType(handle, m_skinningMapMap);
	case eType_VertexOutUav:
		return GetResourceType(handle, m_vertexOutMap);
	case eType_TriangleNtUav:
		return GetResourceType(handle, m_tangentsNTMap);
	}

	return NULL;
}

SResourceHandle CStorage::GetOrCreateResource(ResourceMapType& container, const void* tag, const char* name, const EType type, const uint32 numElements, const uint32 elementSize, const void* data)
{
	uint64 hashKey = GetHash(name, tag, numElements, elementSize);

	auto it = container.find(hashKey);
	if (it == container.end())
	{
		CGpuBuffer* buffer = new CGpuBuffer();
		if (buffer)
		{
			if (tag)
				buffer->Create(numElements, elementSize, DXGI_FORMAT_UNKNOWN, DX11BUF_STRUCTURED | DX11BUF_BIND_SRV | DX11BUF_BIND_UAV, NULL);
			else
				buffer->Create(numElements, elementSize, DXGI_FORMAT_UNKNOWN, DX11BUF_STRUCTURED | DX11BUF_BIND_SRV, data);

			SResourceHandle handle;
			handle.buffer = buffer;
			handle.key = hashKey;
			handle.type = type;
			handle.refcount++;
			container[hashKey] = handle;

			return handle;
		}
		else
		{
			return SResourceHandle();
		}
	}
	else
	{
		++it->second.refcount;
		return it->second;
	}
}

void CStorage::RemoveResource(SResourceHandle& handle, ResourceMapType& container)
{
	//  auto it = container.find(handle.key);
	//  if (it != container.end())
	//  {
	//    if (--it->second.refcount <= 0)
	//    {
	//      if (it->second.buffer)
	//      {
	//        it->second.buffer->Release();
	//        delete it->second.buffer;
	//        it->second.buffer = NULL;
	//
	//        CryLogAlways("Removed from cache %d", (uint32)handle.type);
	//
	//        container.erase(it);
	//      }
	//    }
	//  }
}

SResourceHandle CStorage::CreateResource(const void* tag, const char* name, const EType type, const uint32 numElements, const void* data)
{
	SResourceHandle handle;
	if (!numElements)
	{
		//		CryLogAlways("[ComputeSkinning] CreateResource(): Invalid number of elements for resource type: %d, (%s)", (uint32)gdType, name);
		return handle;
	}

	switch (type)
	{
	case eType_VertexInSrv:
		return GetOrCreateResource(m_vertexInMap, tag, name, type, numElements, sizeof(SSkinVertexIn), data);
	case eType_IndicesSrv:
		return GetOrCreateResource(m_indicesMap, tag, name, type, numElements, sizeof(SIndices), data);
	case eType_TriangleAdjacencyIndicesSrv:
		return GetOrCreateResource(m_triangleAdjacencyMap, tag, name, type, numElements, sizeof(SIndices), data);
	case eType_MorphsDeltaSrv:
		return GetOrCreateResource(m_morphsDeltasMap, tag, name, type, numElements, sizeof(SMorphsDeltas), data);
	case eType_MorphsBitFieldSrv:
		return GetOrCreateResource(m_morphsBitfieldMap, tag, name, type, numElements, sizeof(SMorphsBitField), data);
	case eType_SkinningSrv:
		return GetOrCreateResource(m_skinningMap, tag, name, type, numElements, sizeof(SSkinning), data);
	case eType_SkinningMapSrv:
		return GetOrCreateResource(m_skinningMapMap, tag, name, type, numElements, sizeof(SSkinningMap), data);
	case eType_VertexOutUav:
		return GetOrCreateResource(m_vertexOutMap, tag, name, type, numElements, sizeof(SComputeShaderSkinVertexOut), data);
	case eType_TriangleNtUav:
		return GetOrCreateResource(m_tangentsNTMap, tag, name, type, numElements, sizeof(SComputeShaderTriangleNT), data);
	default:
		CryLogAlways("[ComputeSkinning] CreateResource(): Invalid Resource Type: %d (%s)", (uint32)type, name);
		return handle;
	}

	return handle;
}

void CStorage::ReleaseResource(const EType type, SResourceHandle& handle)
{
	switch (type)
	{
	case eType_VertexInSrv:
		return RemoveResource(handle, m_vertexInMap);
	case eType_IndicesSrv:
		return RemoveResource(handle, m_indicesMap);
	case eType_TriangleAdjacencyIndicesSrv:
		return RemoveResource(handle, m_triangleAdjacencyMap);
	case eType_MorphsDeltaSrv:
		return RemoveResource(handle, m_morphsDeltasMap);
	case eType_MorphsBitFieldSrv:
		return RemoveResource(handle, m_morphsBitfieldMap);
	case eType_SkinningSrv:
		return RemoveResource(handle, m_skinningMap);
	case eType_SkinningMapSrv:
		return RemoveResource(handle, m_skinningMapMap);
	case eType_VertexOutUav:
		return RemoveResource(handle, m_vertexOutMap);
	case eType_TriangleNtUav:
		return RemoveResource(handle, m_tangentsNTMap);
	default:
		CryLogAlways("[ComputeSkinning] RemoveResource(): Invalid Resource Type: %d", (uint32)type);
	}
}

#ifndef _RELEASE

	#define KB(x) (1.0f * x) / 1024.f
	#define MB(x) KB(x) / 1024.f

const char* gdTypes[eType_AMOUNT] =
{
	"GD_INVALID",
	"GD_VERTEX_IN",
	"GD_VERTEX_OUT",
	"GD_TRIANGLE_NT",
	"GD_INDICES",
	"GD_SHARED_TRIANGLE_INDICES",
	"GD_MORPHS_DELTA",
	"GD_MORPHS_BITFIELD",
	"GD_SKINNING",
	"GD_SKINNING_MAP"
};

uint32 CStorage::GetSizeBytes(ResourceMapType& container, const uint32 elementSize)
{
	uint32 total = 0;
	for (auto it = container.begin(); it != container.end(); ++it)
	{
		total += it->second.buffer->m_numElements * elementSize;
	}

	return total;
}

void CStorage::DebugDraw()
{
	ColorF c0 = ColorF(0.0f, 1.0f, 1.0f, 1.0f);
	ColorF c1 = ColorF(0.0f, 0.8f, 0.8f, 1.0f);

	uint32 total = 0;
	int sizes[(uint32)eType_AMOUNT] = { 0 };

	total += (sizes[eType_VertexInSrv] = GetSizeBytes(m_vertexInMap, sizeof(SSkinVertexIn)));
	total += (sizes[eType_SkinningSrv] = GetSizeBytes(m_skinningMap, sizeof(SSkinning)));
	total += (sizes[eType_SkinningMapSrv] = GetSizeBytes(m_skinningMapMap, sizeof(SSkinningMap)));
	total += (sizes[eType_IndicesSrv] = GetSizeBytes(m_indicesMap, sizeof(SIndices)));
	total += (sizes[eType_TriangleAdjacencyIndicesSrv] = GetSizeBytes(m_triangleAdjacencyMap, sizeof(SIndices)));
	total += (sizes[eType_MorphsDeltaSrv] = GetSizeBytes(m_morphsDeltasMap, sizeof(SMorphsDeltas)));
	total += (sizes[eType_MorphsBitFieldSrv] = GetSizeBytes(m_morphsBitfieldMap, sizeof(SMorphsBitField)));
	total += (sizes[eType_VertexOutUav] = GetSizeBytes(m_vertexOutMap, sizeof(SComputeShaderSkinVertexOut)));
	total += (sizes[eType_TriangleNtUav] = GetSizeBytes(m_tangentsNTMap, sizeof(SComputeShaderTriangleNT)));

	gRenDev->Draw2dLabel(1, 20.0f, 2.0f, c1, false, "[Geometry Deformation]");
	gRenDev->Draw2dLabel(1, 40.0f, 1.5f, c1, false, " Total GPU memory used: (%.02fKB / %.02fMB)",
	                     KB(total),
	                     MB(total)
	                     );

	uint32 offset = 0;
	for (uint32 i = 1; i < (uint32)eType_AMOUNT; ++i)
	{
		gRenDev->Draw2dLabel(1, 60.0f + (offset * 20.0f), 1.5f, c0, false,
		                     "	[%s] - (%.02fKB / %.02fMB)",
		                     gdTypes[i],
		                     KB(sizes[i]),
		                     MB(sizes[i])
		                     );
		++offset;
	}
}

#endif
}
