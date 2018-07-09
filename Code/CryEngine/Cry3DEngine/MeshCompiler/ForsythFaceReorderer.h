// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#ifndef __ForsythFaceReorderer_h__
#define __ForsythFaceReorderer_h__

//
// Note: This implementation, in contrast to many other implementations
// of the Forsyth's algorithm, does not crash when the input faces contain
// duplicate indices, for example (8,3,8) or (1,1,9).
//
class ForsythFaceReorderer
{
public:
	static const size_t sk_maxCacheSize = 50; // you can change it. note: making it higher will increase sizeof(*this)
	static const size_t sk_minVerticesPerFace = 3;
	static const size_t sk_maxVerticesPerFace = 4;

	static_assert(sk_minVerticesPerFace >= 3, "Bad min # of vertices per face");
	static_assert(sk_minVerticesPerFace <= sk_maxVerticesPerFace, "Bad # of vertices per face");
	static_assert(sk_maxVerticesPerFace <= sk_maxCacheSize, "Bad max cache size");

private:
	typedef uint16 valency_type;
	static const valency_type sk_maxValency = 0xFFFF;

	typedef int8 cachepos_type;
	static const cachepos_type sk_maxCachePos = 127;

	struct Vertex
	{
		uint32*       m_pFaceList;
		valency_type  m_aliveFaceCount;
		cachepos_type m_posInCache;
		float         m_score;
	};

	static const size_t sk_valencyTableSize = 32;                  // note: size_t is used instead of valency_type because valency_type overflows if sk_valencyTableSize == 1 + sk_maxValency
	static_assert(sk_valencyTableSize - 1 <= sk_maxValency, "Bad valency table size");

	static_assert(sk_maxCacheSize <= 1 + (size_t)sk_maxCachePos, "Max cache size is too big");

	std::vector<Vertex> m_vertices;
	std::vector<uint8>  m_deadFacesBitArray;
	std::vector<float>  m_faceScores;        // score of every face
	std::vector<uint32> m_vertexFaceLists;   // lists with indices of faces (each vertex has own list)

	int                 m_cacheSize;
	int                 m_cacheUsedSize;
	uint32              m_cache[sk_maxCacheSize + sk_maxVerticesPerFace]; // +sk_maxVerticesPerFace is temporary storage for vertices of the incoming face

	float               m_scoreTable_valency[sk_valencyTableSize];
	float               m_scoreTable_cachePosition[sk_maxCacheSize];

public:
	ForsythFaceReorderer();

	// notes:
	// 1) it's not allowed to pass same array for inVertexIndices and outVertexIndices
	// 2) outFaceToOldFace is optional (pass 0 if you don't need this array filled)
	bool reorderFaces(
	  const size_t cacheSize,
	  const uint verticesPerFace,
	  const size_t indexCount,
	  const uint32* const inVertexIndices,
	  uint32* const outVertexIndices,
	  uint32* const outFaceToOldFace);

private:
	void   clear();
	void   computeCacheScoreTable(const int verticesPerFace);
	void   computeValencyScoreTable();
	void   computeVertexScore(Vertex& v);
	void   moveVertexToCacheTop(const uint32 vertexIndex);
	void   removeFaceFromVertex(const uint32 vertexIndex, const uint32 faceIndex);
	uint32 findBestFaceToAdd(uint32& faceSearchCursor) const;
};

#endif
