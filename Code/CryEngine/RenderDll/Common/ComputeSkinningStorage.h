// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct CGpuBuffer;

namespace compute_skinning
{
struct SResourceHandle
{
	SResourceHandle() : key(0), refcount(0), buffer(NULL), type(eType_Invalid) {};
	uint64      key;
	EType       type;
	uint32      refcount;
	CGpuBuffer* buffer;
};

class CStorage
{
public:
	CStorage();
	virtual ~CStorage();

	virtual SResourceHandle CreateResource(const void* tag, const char* name, const EType gdType, const uint32 numElements, const void* data);
	virtual CGpuBuffer*     GetResource(const SResourceHandle& handle);
	virtual void            ReleaseResource(const EType gdType, SResourceHandle& handle);

	// debug
#ifndef _RELEASE
	virtual void DebugDraw();
#endif

private:

	typedef std::unordered_map<uint64, SResourceHandle> ResourceMapType;

	// resources streams
	ResourceMapType m_vertexInMap;
	ResourceMapType m_indicesMap;
	ResourceMapType m_triangleAdjacencyMap;
	ResourceMapType m_morphsDeltasMap;
	ResourceMapType m_morphsBitfieldMap;
	ResourceMapType m_skinningMap;
	ResourceMapType m_skinningMapMap;
	ResourceMapType m_vertexOutMap;
	ResourceMapType m_tangentsNTMap;

	uint32          m_cacheCapacity;
	uint32          m_cacheSize;

	// resource management
	void            RemoveResource(SResourceHandle& resource, ResourceMapType& container);
	void            Release(ResourceMapType& container);
	SResourceHandle GetOrCreateResource(ResourceMapType& container, const void* tag, const char* name, const EType type, const uint32 numElements, const uint32 elementSize, const void* data);
	uint32          GetSizeBytes(ResourceMapType& container, const uint32 elementSize);
	CGpuBuffer*     GetResourceType(const SResourceHandle& handle, const ResourceMapType& container);

	// hasher
	uint64 GetHash(const char* name, const void* tag, const uint32 numElements, const uint32 elementSize);
	void   HashCombine(uint64& seed, const uint64 val);
};
}
