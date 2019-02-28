// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2012.
// -------------------------------------------------------------------------
//  File name:   AlembicCompiler.h
//  Created:     20. June 2012 by Axel Gneiting
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IConverter.h"
#include "GeomCache.h"
#include "ThreadUtils.h"
#include "StealingThreadPool.h"
#include "../ResourceCompilerPC/PhysWorld.h"

class ICryXML;
class GeomCacheEncoder;
class IXMLSerializer;

// Used for detecting identical meshes. For two identical meshes all digests must match.
class AlembicMeshDigest
{
	friend struct std::hash<AlembicMeshDigest>;
public:
	explicit AlembicMeshDigest(Alembic::AbcGeom::IPolyMeshSchema &meshSchema);

	bool operator==(const AlembicMeshDigest &digest) const;

private:
	bool m_bHasNormals;
	bool m_bHasTexcoords;
	bool m_bHasColors;

	Alembic::AbcGeom::ArraySampleKey m_positionDigest;
	Alembic::AbcGeom::ArraySampleKey m_positionIndexDigest;	
	Alembic::AbcGeom::ArraySampleKey m_normalsDigest;	
	Alembic::AbcGeom::ArraySampleKey m_texcoordDigest;
	Alembic::AbcGeom::ArraySampleKey m_colorsDigest;
};

// Unoptimized vertex used in the compiler
struct AlembicCompilerVertex
{	
	Vec3 m_position;
	Vec3 m_normal;
	Vec2 m_texcoords;
	Vec4 m_rgba;
};

template<class T> class AlembicCompilerHash;

template<> class AlembicCompilerHash<float>
{
public:
	uint64 operator()(const float value)
	{
		uint32 bits = alias_cast<uint32>(value);
		bits = (bits == 0x80000000) ? 0 : bits; // -0 == 0

		// Magic taken from CityHash64
		const uint64 u = bits;
		const uint64 kMul = 0x9ddfea08eb382d69ULL;
		uint64 a = u * kMul;
		a ^= (a >> 47);
		a *= kMul;
		return a;
	}
};

template<> class AlembicCompilerHash<uint64>
{
public:
	uint64 operator()(const uint64 value)
	{
		return value;
	}
};

// Helper function to combine hashes
template<class T>
inline void AlembicCompilerHashCombine(uint64& seed, const T& v)
{	
	AlembicCompilerHash<T> hasher;	
	
	// Magic taken from CityHash64
	const uint64 kMul = 0x9ddfea08eb382d69ULL;
	uint64 a = (hasher(v) ^ seed) * kMul;
	a ^= (a >> 47);
	uint64 b = (seed ^ a) * kMul;
	b ^= (b >> 47);
	seed = b * kMul;
}

template<> class AlembicCompilerHash<AlembicCompilerVertex> 
{
public:		
	uint64 operator()(const AlembicCompilerVertex &vertex) const 
	{
		uint64 hash = 0;

		AlembicCompilerHashCombine(hash, vertex.m_position[0]);
		AlembicCompilerHashCombine(hash, vertex.m_position[1]);
		AlembicCompilerHashCombine(hash, vertex.m_position[2]);
		AlembicCompilerHashCombine(hash, vertex.m_normal[0]);
		AlembicCompilerHashCombine(hash, vertex.m_normal[1]);
		AlembicCompilerHashCombine(hash, vertex.m_normal[2]);
		AlembicCompilerHashCombine(hash, vertex.m_texcoords[0]);
		AlembicCompilerHashCombine(hash, vertex.m_texcoords[1]);
		AlembicCompilerHashCombine(hash, vertex.m_rgba[0]);
		AlembicCompilerHashCombine(hash, vertex.m_rgba[1]);
		AlembicCompilerHashCombine(hash, vertex.m_rgba[2]);
		AlembicCompilerHashCombine(hash, vertex.m_rgba[3]);

		return hash;
	}
};

// std::hash<> specializations for std::unordered_map
namespace std
{
	template<> class hash<AlembicMeshDigest> 
	{
	public:		
		size_t operator()(const AlembicMeshDigest &digest) const 
		{
			// Just return the position digest. It's very likely that
			// if positions match everything else is matching as well
			return Alembic::AbcGeom::StdHash(digest.m_positionDigest);
		}
	};
}

namespace
{	
	struct CompileMeshJobData;
}

class GeomCacheEncoder;

class AlembicCompiler : public ICompiler
{
	friend struct CompileMeshJobData;

	struct FrameJobGroupData
	{
		FrameJobGroupData()
			: m_bFinished(false)
			, m_bWritten(false)
			, m_errorCount(0)
			, m_pAlembicCompiler(nullptr)
		{
		}
		
		volatile bool m_bFinished;
		volatile bool m_bWritten;
		uint m_jobIndex;
		uint m_frameIndex;		
		volatile uint m_errorCount;
		Alembic::Abc::chrono_t m_frameTime;
		AlembicCompiler *m_pAlembicCompiler;		
		AABB m_frameAABB;		
	};

public:
	explicit AlembicCompiler(ICryXML *pXMLParser);
	virtual ~AlembicCompiler() {}

	// ICompiler methods.
	virtual void Release();
	virtual void BeginProcessing(const IConfig* config) {}
	virtual void EndProcessing() {}
	virtual IConvertContext* GetConvertContext() { return &m_CC; }
	virtual bool Process();

private:
	XmlNodeRef ReadConfig(const string &configPath, IXMLSerializer *pXMLSerializer);

	string GetOutputFileNameOnly() const;
	string GetOutputPath() const;

	bool CheckTimeSampling(Alembic::Abc::IArchive &archive);
	void OutputTimeSamplingType(const Alembic::Abc::TimeSamplingType &timeSamplingType);

	void CheckTimeSamplingRec(Alembic::Abc::IObject &currentObject);
	void CheckTimeSamplingRec(Alembic::Abc::ICompoundProperty &currentProperty);

	// Setup static data for header
	bool CompileStaticData(Alembic::Abc::IArchive &archive, ThreadUtils::StealingThreadPool &threadPool);
	bool CompileStaticDataRec(GeomCache::Node *pParentNode, Alembic::Abc::IObject &currentObject, const QuatTNS &localTransform, 
		std::vector<Alembic::AbcGeom::IXform> abcXformStack, ThreadUtils::StealingThreadPool &threadPool,
		const bool bParentRemoved, const GeomCacheFile::ETransformType parentTransform);
	bool CompileStaticMeshData(GeomCache::Node &node, Alembic::AbcGeom::IPolyMesh &mesh, ThreadUtils::StealingThreadPool &threadPool);
	bool CompilePhysicsGeometry(GeomCache::Node &node, Alembic::AbcGeom::IPolyMesh &mesh, ThreadUtils::StealingThreadPool &threadPool);
	void CheckMeshForColors(Alembic::AbcGeom::IPolyMeshSchema & meshSchema, GeomCache::Mesh &mesh) const;

	// Prints the node tree
	void PrintNodeTreeRec(GeomCache::Node &node, string padding);

	// Compile stream of frames and  send them to the cache writer
	bool CompileAnimationData(Alembic::Abc::IArchive &archive, GeomCacheEncoder &geomCacheEncoder, ThreadUtils::StealingThreadPool &threadPool);

	// Pushes the completed frames in order to the encoder
	void PushCompletedFrames(GeomCacheEncoder &geomCacheEncoder, const uint numJobGroups);
	
	// Update transforms
	static void UpdateTransformsJob(FrameJobGroupData *pData);
	typedef std::unordered_map<std::string, Alembic::AbcGeom::M44d> TMatrixMap;
	typedef std::unordered_map<std::string, Alembic::AbcGeom::ObjectVisibility> TVisibilityMap;
	void UpdateTransformsRec(GeomCache::Node &node, const uint bufferIndex, const Alembic::Abc::chrono_t frameTime, 
		AABB &frameAABB, QuatTNS currentTransform, TMatrixMap &matrixMap, TVisibilityMap &visibilityMap, std::string &currentObjectPath);

	// Compiles a mesh or update its vertices in a job	
	static void UpdateVertexDataJob(CompileMeshJobData *pData);

	// Called when frame group has finished
	static void FrameGroupFinished(FrameJobGroupData *pData);

	// Gets mapping from alembic face Id to material ids
	std::unordered_map<uint32, uint16> GetMeshMaterialMap(const Alembic::AbcGeom::IPolyMesh& mesh, const Alembic::Abc::chrono_t frameTime);

	// Generates a hash for each vertex (all frames are taken into account)
	bool ComputeVertexHashes(std::vector<uint64> &abcVertexHashes, const size_t currentFrame, const size_t numAbcIndices, GeomCache::Mesh &mesh, 
		Alembic::Abc::TimeSampling &meshTimeSampling, size_t numMeshSamples, Alembic::AbcGeom::IPolyMeshSchema &meshSchema, const bool bHasNormals, 
		const bool bHasTexcoords, const bool bHasColors, const size_t numAbcNormalIndices, const size_t numAbcTexcoordsIndices, const size_t numAbcFaces);

	// This is used for the first frame compilation of constant/homogeneous meshes and for each frame for heterogeneous meshes.	
	bool CompileFullMesh(GeomCache::Mesh &mesh, const size_t currentFrame, const QuatTNS &transform);

	// Update vertex data according to the mapping table produced by CompileFullMesh at frameTime. Used for homogeneous meshes.
	bool UpdateVertexData(GeomCache::Mesh &mesh, const uint bufferIndex, const size_t currentFrame);
	
	// Calculate smoothed normals
	bool CalculateSmoothNormals(std::vector<AlembicCompilerVertex> &vertices, GeomCache::Mesh &mesh, 
		const Alembic::Abc::Int32ArraySample &faceCounts, const Alembic::Abc::Int32ArraySample &faceIndices, 
		const Alembic::Abc::P3fArraySample &positions);

	// Computes tangent space, quantizes vertex positions and fills MeshData
	bool CompileVertices(std::vector<AlembicCompilerVertex> &vertices, GeomCache::Mesh &mesh, GeomCache::MeshData &meshData, const bool bUpdate);	

	// Goes through the node tree and update the transform frame dequeues
	void AppendTransformFrameDataRec(GeomCache::Node &node, const uint bufferIndex) const;

	// Cleans up data structures
	void Cleanup();

	// The RC XML parser instance
	ICryXML *m_pXMLParser;

	// Cache root node
	GeomCache::Node m_rootNode;

	// Context
	ConvertContext m_CC;
	
	// Ref count
	int m_refCount;

	// Flag for 32 bit index format
	bool m_b32BitIndices;

	// Config flags
	bool m_bConvertYUpToZUp;
	bool m_bMeshPrediction;
	bool m_bUseBFrames;
	bool m_bPlaybackFromMemory;
	uint m_indexFrameDistance;
	GeomCacheFile::EBlockCompressionFormat m_blockCompressionFormat;
	double m_positionPrecision;

	// Time
	std::vector<Alembic::Abc::TimeSampling> m_timeSamplings;	
	Alembic::Abc::chrono_t m_minTime;
	Alembic::Abc::chrono_t m_maxTime;
	std::vector<Alembic::Abc::chrono_t> m_frameTimes;

	// Stats
	LONG m_numExportedMeshes;
	LONG m_numVertexSplits;
	LONG m_numSharedMeshNodes;

	// For error handling
	std::string m_currentObjectPath;

	// List of unique meshes
	std::vector<GeomCache::Mesh*> m_meshes;
	uint m_numAnimatedMeshes;

	// For detecting cloned meshes	
	std::unordered_map<AlembicMeshDigest, std::shared_ptr<GeomCache::Mesh>> m_digestToMeshMap;

	// The next job group hat will be used
	uint m_nextJobGroupIndex;

	// Index of frame that needs to be written next
	size_t m_nextFrameToWrite;

	// Number of frame job groups running
	uint m_numJobGroupsRunning;	
	volatile uint m_numJobsFinished;
	ThreadUtils::CriticalSection m_jobFinishedCS;
	ThreadUtils::ConditionVariable m_jobFinishedCV;

	// Data for each frame job group
	std::vector<FrameJobGroupData> m_jobGroupData;	

	// Transform update lock (ABC library is sometimes not thread safe)
	ThreadUtils::CriticalSection m_abcLock;

	// Error count
	volatile uint m_errorCount;

	CPhysWorldLoader m_physLoader;
};
