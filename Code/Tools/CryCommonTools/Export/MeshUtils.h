// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MESHUTILS_H__
#define __MESHUTILS_H__

#include <CryCore/BaseTypes.h>     // uint8
#include <CryMath/Cry_Vector3.h>   // Vec3

namespace MeshUtils
{

struct Face
{
	int vertexIndex[3];
};

struct Color
{
	uint8 r;
	uint8 g;
	uint8 b;
};

// Stores linking of a vertex to bone(s)
class VertexLinks
{
public:
	struct Link
	{
		int boneId;
		float weight;
		Vec3 offset;

		Link()
			: boneId(-1)
			, weight(-1.0f)
			, offset(0.0f, 0.0f, 0.0f)
		{
		}

		Link(int boneId, float weight)
			: boneId(boneId)
			, weight(weight)
			, offset(0.0f, 0.0f, 0.0f)
		{
		}
	};

	enum ESort
	{
		eSort_ByWeight,
		eSort_ByBoneId,
	};

public:
	std::vector<Link> links;

public:
	// minWeightToDelete: links with weights <= minWeightToDelete will be deleted
	const char* Normalize(ESort eSort, const float minWeightToDelete, const int maxLinkCount)
	{
		if (minWeightToDelete < 0 || minWeightToDelete >= 1)
		{
			return "Bad minWeightToDelete passed";
		}
		if (maxLinkCount <= 0)
		{
			return "Bad maxLinkCount passed";
		}

		// Merging links with matching bone ids
		{
			DeleteByWeight(0.0f);

			if (links.empty())
			{
				return "All bone links of a vertex have zero weight";
			}

			std::sort(links.begin(), links.end(), CompareLinksByBoneId);

			size_t dst = 0;
			for (size_t i = 1; i < links.size(); ++i)
			{
				if (links[i].boneId == links[dst].boneId)
				{
					const float w0 = links[dst].weight;
					const float w1 = links[i].weight;
					const float a =  w0 / (w0 + w1);
					links[dst].offset = links[dst].offset * a + links[i].offset * (1 - a);
					links[dst].weight = w0 + w1;
				}
				else
				{
					links[++dst] = links[i];
				}
			}

			links.resize(dst + 1);
		}

		// Deleting links, normalizing link weights.
		//
		// Note: we produce meaningful results even in cases like this:
		// input weights are { 0.03, 0.01 }, minWeightTodelete is 0.2.
		// Output weights produced are { 0.75, 0.25 }.
		{
			std::sort(links.begin(), links.end(), CompareLinksByWeight);

			if (links.size() > maxLinkCount)
			{
				links.resize(maxLinkCount);
			}

			NormalizeWeights();

			const size_t oldSize = links.size();

			DeleteByWeight(minWeightToDelete);

			if (links.empty())
			{
				return "All bone links of a vertex are deleted (minWeightToDelete is too big)";
			}

			if (links.size() != oldSize)
			{
				NormalizeWeights();
			}
		}

		switch (eSort)
		{
		case eSort_ByWeight:
			// Do nothing because we already sorted links by weight (see above) 
			break;
		case eSort_ByBoneId:
			std::sort(links.begin(), links.end(), CompareLinksByBoneId);
			break;
		default:
			assert(0);
			break;
		}

		return nullptr;
	}

private:
	void DeleteByWeight(float minWeightToDelete)
	{
		for (size_t i = 0; i < links.size(); ++i)
		{
			if (links[i].weight <= minWeightToDelete)
			{
				if (i < links.size() - 1)
				{
					links[i] = links[links.size() - 1];
				}
				links.resize(links.size() - 1);
				--i;
			}
		}
	}

	void NormalizeWeights()
	{
		assert(!links.empty() && links[0].weight > 0);

		float w = 0;

		for (size_t i = 0; i < links.size(); ++i)
		{
			w += links[i].weight;
		}

		w = 1 / w;

		for (size_t i = 0; i < links.size(); ++i)
		{
			links[i].weight *= w;
		}
	}

	static bool CompareLinksByBoneId(const Link& left, const Link& right)
	{
		if (left.boneId != right.boneId)
		{
			return left.boneId < right.boneId;
		}
		if (left.weight != right.weight)
		{
			return left.weight < right.weight;
		}
		return memcmp(&left.offset, &right.offset, sizeof(left.offset)) < 0;
	}

	static bool CompareLinksByWeight(const Link& left, const Link& right)
	{
		if (left.weight != right.weight)
		{
			return left.weight > right.weight;
		}
		if (left.boneId != right.boneId)
		{
			return left.boneId < right.boneId;
		}
		return memcmp(&left.offset, &right.offset, sizeof(left.offset)) < 0;
	}
};

struct SMorphTarget
{
	string name;
	std::vector<Vec3> positions;
};

class Mesh
{
public:
	// Vertex data
	std::vector<Vec3> m_positions;
	std::vector<int> m_topologyIds;
	std::vector<Vec3> m_normals;
	std::vector<Vec2> m_texCoords;
	std::vector<Color> m_colors;
	std::vector<uint8> m_alphas;
	std::vector<VertexLinks> m_links;
	std::vector<int> m_vertexMatIds;
	size_t m_auxSizeof;
	std::vector<uint8> m_aux;

	std::vector<SMorphTarget> m_morphTargets;

	// Face data
	std::vector<Face> m_faces;
	std::vector<int> m_faceMatIds;

	// Mappings computed by ComputeVertexRemapping()
	std::vector<int> m_vertexOldToNew;
	std::vector<int> m_vertexNewToOld;

public:
	Mesh()
		: m_auxSizeof(0)
	{
	}

	int GetVertexCount() const
	{
		return m_positions.size();
	}

	int GetFaceCount() const
	{
		return m_faces.size();
	}

	//////////////////////////////////////////////////////////////////////////
	// Setters

	void Clear()
	{
		m_positions.clear();
		m_topologyIds.clear();
		m_normals.clear();
		m_texCoords.clear();
		m_colors.clear();
		m_alphas.clear();
		m_links.clear();
		m_vertexMatIds.clear();
		m_aux.clear();

		m_faces.clear();
		m_faceMatIds.clear();

		m_morphTargets.clear();

		m_vertexOldToNew.clear();
		m_vertexNewToOld.clear();
	}

	const char* SetPositions(const float* pVec3, int count, int stride, const float scale)
	{
		if (count <= 0)
		{
			return "bad position count";
		}
		if (stride < 0 || (stride > 0 && stride < sizeof(*pVec3) * 3))
		{
			return "bad position stride";
		}

		m_positions.resize(count);

		for (int i = 0; i < count; ++i)
		{
			const float* const p = (const float*)(((const char*)pVec3) + ((size_t)i * stride));
			if (!_finite(p[0]) || !_finite(p[1]) || !_finite(p[2]))
			{
				m_positions.clear();
				return "Illegal (NAN) vertex position. Fix the 3d Model.";
			}
			m_positions[i].x = p[0] * scale;
			m_positions[i].y = p[1] * scale;
			m_positions[i].z = p[2] * scale;
		}

		return nullptr;
	}

	const char* SetTopologyIds(const int* pTopo, int count, int stride)
	{
		if (count <= 0)
		{
			return "bad topologyId count";
		}
		if (stride < 0 || (stride > 0 && stride < sizeof(*pTopo)))
		{
			return "bad topologyId stride";
		}

		m_topologyIds.resize(count);

		for (int i = 0; i < count; ++i)
		{
			const int* const p = (const int*)(((const char*)pTopo) + ((size_t)i * stride));
			m_topologyIds[i] = p[0];
		}

		return nullptr;
	}

	const char* SetNormals(const float* pVec3, int count, int stride)
	{
		if (count <= 0)
		{
			return "bad normal count";
		}
		if (stride < 0 || (stride > 0 && stride < sizeof(*pVec3) * 3))
		{
			return "bad normal stride";
		}

		m_normals.resize(count);

		for (int i = 0; i < count; ++i)
		{
			const float* const p = (const float*)(((const char*)pVec3) + ((size_t)i * stride));
			if (!_finite(p[0]) || !_finite(p[1]) || !_finite(p[2]))
			{
				m_normals.clear();
				return "Illegal (NAN) vertex normal. Fix the 3d Model.";
			}
			m_normals[i].x = p[0];
			m_normals[i].y = p[1];
			m_normals[i].z = p[2];
			m_normals[i] = m_normals[i].GetNormalizedSafe(Vec3Constants<float>::fVec3_OneZ);
		}

		return nullptr;
	}

	const char* SetTexCoords(const float* pVec2, int count, int stride, bool bFlipT)
	{
		if (count <= 0)
		{
			return "bad texCoord count";
		}
		if (stride < 0 || (stride > 0 && stride < sizeof(*pVec2) * 2))
		{
			return "bad texCoord stride";
		}

		m_texCoords.resize(count);

		for (int i = 0; i < count; ++i)
		{
			const float* const p = (const float*)(((const char*)pVec2) + ((size_t)i * stride));
			if (!_finite(p[0]) || !_finite(p[1]))
			{
				m_texCoords.clear();
				return "Illegal (NAN) texture coordinate. Fix the 3d Model.";
			}
			m_texCoords[i].x = p[0];
			m_texCoords[i].y = bFlipT ? 1 - p[1] : p[1];
		}

		return nullptr;
	}

	const char* SetColors(const uint8* pRgb, int count, int stride)
	{
		if (count <= 0)
		{
			return "bad color count";
		}
		if (stride < 0 || (stride > 0 && stride < sizeof(*pRgb) * 3))
		{
			return "bad color stride";
		}

		m_colors.resize(count);

		for (int i = 0; i < count; ++i)
		{
			const uint8* const p = (((const uint8*)pRgb) + ((size_t)i * stride));
			m_colors[i].r = p[0];
			m_colors[i].g = p[1];
			m_colors[i].b = p[2];
		}

		return nullptr;
	}

	const char* SetAlphas(const uint8* pAlpha, int count, int stride)
	{
		if (count <= 0)
		{
			return "bad alpha count";
		}
		if (stride < 0 || (stride > 0 && stride < sizeof(*pAlpha)))
		{
			return "bad alpha stride";
		}

		m_alphas.resize(count);

		for (int i = 0; i < count; ++i)
		{
			const uint8* const p = (((const uint8*)pAlpha) + ((size_t)i * stride));
			m_alphas[i] = p[0];
		}

		return nullptr;
	}

	const char* SetFaces(const int* pVertIdx3, int count, int stride)
	{
		if (count <= 0)
		{
			return "bad face count";
		}
		if (stride < 0 || (stride > 0 && stride < sizeof(*pVertIdx3) * 3))
		{
			return "bad face stride";
		}

		m_faces.resize(count);

		for (int i = 0; i < count; ++i)
		{
			const int* const p = (const int *)(((const char*)pVertIdx3) + ((size_t)i * stride));
			for (int j = 0; j < 3; ++j)
			{
				if (p[j] < 0 || p[j] >= m_positions.size())
				{
					return "bad vertex index found in a face";
				}
				m_faces[i].vertexIndex[j] = p[j];
			}
		}

		return nullptr;
	}

	const char* SetFaceMatIds(const int* pMatIds, int count, int stride, int maxMaterialId)
	{
		if (count <= 0)
		{
			return "bad face materialId count";
		}
		if (stride < 0 || (stride > 0 && stride < sizeof(*pMatIds)))
		{
			return "bad face materialIdstride";
		}

		m_faceMatIds.resize(count);

		for (int i = 0; i < count; ++i)
		{
			const int* const p = (const int *)(((const char*)pMatIds) + ((size_t)i * stride));
			if (p[0] < 0)
			{
				return "negative material ID found in a face";
			}
			if (p[0] >= maxMaterialId)
			{
				return "material ID found in a face is outside of allowed ranges";
			}
			m_faceMatIds[i] = p[0];
		}

		return nullptr;
	}

	const char* SetAux(size_t auxSizeof, const void* pData, int count, int stride)
	{
		if (auxSizeof <= 0)
		{
			return "bad aux sizeof";
		}
		if (count <= 0)
		{
			return "bad aux count";
		}
		if (stride < 0 || (stride > 0 && stride < auxSizeof))
		{
			return "bad aux stride";
		}

		m_auxSizeof = auxSizeof;

		m_aux.resize(count * m_auxSizeof);

		for (int i = 0; i < count; ++i)
		{
			const uint8* const p = (((const uint8*)pData) + ((size_t)i * stride));
			memcpy(&m_aux[i * m_auxSizeof], p, m_auxSizeof);
		}

		return nullptr;
	}

	// Note: Remapping arrays (m_vertexOldToNew and m_vertexNewToOld) are not preserved
	bool Append(const Mesh& other)
	{
		const int dstVertexCount = GetVertexCount();
		const int dstFaceCount = GetFaceCount();

		const int srcVertexCount = other.GetVertexCount();
		const int srcFaceCount = other.GetFaceCount();

		if (srcVertexCount == 0 || srcFaceCount == 0)
		{
			return true;
		}

		if (!m_aux.empty() || !other.m_aux.empty())
		{
			return false;
		}

		if (m_morphTargets.size() != other.m_morphTargets.size())
		{
			if (!m_morphTargets.empty())
			{
				return false;
			}

			m_morphTargets.resize(other.m_morphTargets.size());

			for (size_t i = 0; i < other.m_morphTargets.size(); ++i)
			{ 
				m_morphTargets[i].name = other.m_morphTargets[i].name;
			}
		}
		else
		{
			for (size_t i = 0; i < m_morphTargets.size(); ++i)
			{
				if (m_morphTargets[i].name != other.m_morphTargets[i].name)
				{
					return false;
				}
			}
		}

		m_vertexOldToNew.clear();
		m_vertexNewToOld.clear();

		bool bOk  = true;

		const Color color = { 0xFF, 0xFF, 0xFF };
		bOk = bOk && AppendArray(dstVertexCount, m_positions,    srcVertexCount, other.m_positions,    Vec3(ZERO));
		bOk = bOk && AppendArray(dstVertexCount, m_topologyIds,  srcVertexCount, other.m_topologyIds,  0);
		bOk = bOk && AppendArray(dstVertexCount, m_normals,      srcVertexCount, other.m_normals,      Vec3(ZERO));
		bOk = bOk && AppendArray(dstVertexCount, m_texCoords,    srcVertexCount, other.m_texCoords,    Vec2(ZERO));
		bOk = bOk && AppendArray(dstVertexCount, m_colors,       srcVertexCount, other.m_colors,       color);
		bOk = bOk && AppendArray(dstVertexCount, m_alphas,       srcVertexCount, other.m_alphas,       (uint8)0xFF);
		bOk = bOk && AppendArray(dstVertexCount, m_links,        srcVertexCount, other.m_links,        VertexLinks());
		bOk = bOk && AppendArray(dstVertexCount, m_vertexMatIds, srcVertexCount, other.m_vertexMatIds, 0);

		for (size_t i = 0; i < m_morphTargets.size(); ++i)
		{
			assert(i < other.m_morphTargets.size() && m_morphTargets[i].name == other.m_morphTargets[i].name);
			bOk = bOk && AppendArray(dstVertexCount, m_morphTargets[i].positions, srcVertexCount, other.m_morphTargets[i].positions, Vec3(ZERO));
		}

		const Face face = { 0, 0, 0 };
		bOk = bOk && AppendArray(dstFaceCount, m_faces,      srcFaceCount, other.m_faces,      face);
		bOk = bOk && AppendArray(dstFaceCount, m_faceMatIds, srcFaceCount, other.m_faceMatIds, 0);

		if (!bOk)
		{
			return false;
		}

		for (size_t i = 0; i < srcFaceCount; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				m_faces[dstFaceCount + i].vertexIndex[j] += dstVertexCount;
			}
		}

		// Recompute topology ids
		if (!m_topologyIds.empty())
		{
			std::vector<int> tmp;
			if (dstVertexCount == 0)
			{
				const int lastId = CondenseIds(tmp, &m_topologyIds[0], srcVertexCount, 0);
				assert(lastId > 0 && lastId < (std::numeric_limits<int>::max)());
			}
			else
			{
				const int lastId0 = CondenseIds(tmp, &m_topologyIds[0], dstVertexCount, 0);
				assert(lastId0 > 0 && lastId0 < (std::numeric_limits<int>::max)());
				const int lastId1 = CondenseIds(tmp, &m_topologyIds[dstVertexCount], srcVertexCount, lastId0 + 1);
				assert(lastId1 > 0 && lastId1 < (std::numeric_limits<int>::max)());
			}
		}

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	// Validation

	// Returns nullptr if ok, or pointer to the error text
	const char* Validate() const
	{
		const size_t nVerts = m_positions.size();
		const size_t nFaces = m_faces.size();

		if (nVerts == 0)
		{
			return "No vertices";
		}

		if (nFaces == 0)
		{
			return "No faces";
		}

		if (nVerts > size_t((std::numeric_limits<int>::max)()))
		{
			return "Too many vertices";
		}

		if (nFaces > size_t((std::numeric_limits<int>::max)()))
		{
			return "Too many faces";
		}

		if (!m_topologyIds.empty() && nVerts != m_topologyIds.size())
		{
			return "Mismatch in the number of topology IDs";
		}

		if (!m_normals.empty() && nVerts != m_normals.size())
		{
			return "Mismatch in the number of normals";
		}

		if (!m_texCoords.empty() && nVerts != m_texCoords.size())
		{
			return "Mismatch in the number of texture coordinates";
		}

		if (!m_colors.empty() && nVerts != m_colors.size())
		{
			return "Mismatch in the number of colors";
		}

		if (!m_alphas.empty() && nVerts != m_alphas.size())
		{
			return "Mismatch in the number of alphas";
		}

		if (!m_links.empty() && nVerts != m_links.size())
		{
			return "Mismatch in the number of vertex-bone links";
		}

		for (size_t i = 0; i < m_links.size(); ++i)
		{
			if (m_links[i].links.empty())
			{
				return "Found a vertex without bone linking";
			}
		}

		if (!m_vertexMatIds.empty() && nVerts != m_vertexMatIds.size())
		{
			return "Mismatch in the number of vertex materials";
		}

		if (!m_aux.empty() && nVerts != m_aux.size() / m_auxSizeof)
		{
			return "Mismatch in the number of auxiliary elements";
		}

		if (!m_faceMatIds.empty() && nFaces != m_faceMatIds.size())
		{
			return "Mismatch in the number of face materials";
		}

		for (size_t i = 0; i < m_morphTargets.size(); ++i)
		{
			if (m_morphTargets[i].positions.size() != nVerts)
			{
				return "Mismatch in the number of morph target vertices";
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	// Computation

	void RemoveDegenerateFaces()
	{
		int writePos = 0;
		for (int readPos = 0; readPos < (int)m_faces.size(); ++readPos)
		{
			const Face& face = m_faces[readPos];
			if (face.vertexIndex[0] != face.vertexIndex[1] && 
				face.vertexIndex[1] != face.vertexIndex[2] && 
				face.vertexIndex[0] != face.vertexIndex[2])
			{
				m_faces[writePos] = m_faces[readPos];
				if (!m_faceMatIds.empty())
				{
					m_faceMatIds[writePos] = m_faceMatIds[readPos];
				}
				++writePos;
			}
		}
		m_faces.resize(writePos);
		if (!m_faceMatIds.empty())
		{
			m_faceMatIds.resize(writePos);
		}
	}

	int AddVertexCopy(int sourceVertexIndex)
	{
		if (sourceVertexIndex < 0 || sourceVertexIndex >= m_positions.size())
		{
			assert(0);
			return -1;
		}

		m_positions.push_back(m_positions[sourceVertexIndex]);
		if (!m_topologyIds.empty())
		{
			m_topologyIds.push_back(m_topologyIds[sourceVertexIndex]);
		}
		if (!m_normals.empty())
		{
			m_normals.push_back(m_normals[sourceVertexIndex]);
		}
		if (!m_texCoords.empty())
		{
			m_texCoords.push_back(m_texCoords[sourceVertexIndex]);
		}
		if (!m_colors.empty())
		{
			m_colors.push_back(m_colors[sourceVertexIndex]);
		}
		if (!m_alphas.empty())
		{
			m_alphas.push_back(m_alphas[sourceVertexIndex]);
		}
		if (!m_links.empty())
		{
			m_links.push_back(m_links[sourceVertexIndex]);
		}
		if (!m_vertexMatIds.empty())
		{
			m_vertexMatIds.push_back(m_vertexMatIds[sourceVertexIndex]);
		}
		if (!m_aux.empty())
		{
			m_aux.resize(m_aux.size() + m_auxSizeof);
			memcpy(&m_aux[m_aux.size() - m_auxSizeof], &m_aux[sourceVertexIndex * m_auxSizeof], m_auxSizeof);
		}
		for (size_t i = 0; i < m_morphTargets.size(); ++i)
		{
			m_morphTargets[i].positions.push_back(m_morphTargets[i].positions[sourceVertexIndex]);
		}
		return (int)m_positions.size() - 1;
	}

	// Note: might create new vertices and modify vertex indices in faces
	void SetVertexMaterialIdsFromFaceMaterialIds()
	{
		m_vertexMatIds.clear();
		if (m_faceMatIds.empty())
		{
			return;
		}
		m_vertexMatIds.resize(m_positions.size(), -1);

		for (size_t i = 0; i < m_faces.size(); ++i)
		{
			const int faceMatId = m_faceMatIds[i];
			for (int j = 0; j < 3; ++j)
			{
				int v = m_faces[i].vertexIndex[j];
				if (m_vertexMatIds[v] >= 0 && m_vertexMatIds[v] != faceMatId)
				{
					v = AddVertexCopy(v);
					m_faces[i].vertexIndex[j] = v;
				}
				m_vertexMatIds[v] = faceMatId;
			}
		}
	}

	// Computes m_vertexOldToNew and m_vertexNewToOld by detecting duplicate and
	// unused vertices. Unused vertices have '-1' in m_vertexOldToNew.
	void ComputeVertexRemapping()
	{
		const int nVertsOld = GetVertexCount();

		// Put indices of used vertices in m_vertexNewToOld
		{
			m_vertexNewToOld.resize(nVertsOld, 0);

			for (int i = 0, n = GetFaceCount(); i < n; ++i)
			{
				for (int j = 0; j < 3; ++j)
				{
					m_vertexNewToOld[m_faces[i].vertexIndex[j]] = 1;
				}
			}

			int nVertsNew = 0;
			for (int i = 0; i < nVertsOld; ++i)
			{
				if (m_vertexNewToOld[i] != 0)
				{
					m_vertexNewToOld[nVertsNew++] = i;
				}
			}
			m_vertexNewToOld.resize(nVertsNew);
		}

		// Sort indices of used vertices by vertex data (to find duplicate vertices)
		const VertexLess less(*this);
		std::sort(m_vertexNewToOld.begin(), m_vertexNewToOld.end(), VertexLess(*this));
		
		// Remove indices of duplicate vertices from m_vertexNewToOld and form m_vertexOldToNew
		{
			m_vertexOldToNew.resize(nVertsOld, -1);  // -1 means "unused"

			int nVertsNew = 0;
			for (int i = 0, n = (int)m_vertexNewToOld.size(); i < n; ++i)
			{
				if (i == 0 || less(m_vertexNewToOld[i - 1], m_vertexNewToOld[i]))
				{
					m_vertexNewToOld[nVertsNew++] = m_vertexNewToOld[i];
				}
				m_vertexOldToNew[m_vertexNewToOld[i]] = nVertsNew - 1;
			}
			m_vertexNewToOld.resize(nVertsNew);
		}
	}

	// Changes order of vertices, number of vertices, vertex indices in faces
	void RemoveVerticesByUsingComputedRemapping()
	{
		CompactVertexArray(m_positions, m_vertexNewToOld);
		CompactVertexArray(m_topologyIds, m_vertexNewToOld);
		CompactVertexArray(m_normals, m_vertexNewToOld);
		CompactVertexArray(m_texCoords, m_vertexNewToOld);
		CompactVertexArray(m_colors, m_vertexNewToOld);
		CompactVertexArray(m_alphas, m_vertexNewToOld);
		CompactVertexArray(m_links, m_vertexNewToOld);
		CompactVertexArray(m_vertexMatIds, m_vertexNewToOld);
		CompactVertexArrayRaw(m_aux, m_auxSizeof, m_vertexNewToOld);
		for (size_t i = 0; i < m_morphTargets.size(); ++i)
		{
			CompactVertexArray(m_morphTargets[i].positions, m_vertexNewToOld);
		}
		for (size_t i = 0, n = m_faces.size(); i < n; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				const int oldVertedIdx = m_faces[i].vertexIndex[j];
				assert(oldVertedIdx >= 0 && (size_t)oldVertedIdx < m_vertexOldToNew.size());
				const int newVertexIndex = m_vertexOldToNew[oldVertedIdx];
				m_faces[i].vertexIndex[j] = newVertexIndex;
			}
		}
	}

	// Deletes degraded faces (faces with two or more vertices
	// sharing same position in space)
	void RemoveDegradedFaces()
	{
		size_t j = 0;

		for (size_t i = 0, n = m_faces.size(); i < n; ++i)
		{
			const Vec3& p0 = m_positions[m_faces[i].vertexIndex[0]];
			const Vec3& p1 = m_positions[m_faces[i].vertexIndex[1]];
			const Vec3& p2 = m_positions[m_faces[i].vertexIndex[2]];
			if (p0 != p1 && p1 != p2 && p2 != p0)
			{
				m_faces[j] = m_faces[i];
				if (!m_faceMatIds.empty())
				{
					m_faceMatIds[j] = m_faceMatIds[i];
				}
				++j;
			}
		}

		m_faces.resize(j);
		if (!m_faceMatIds.empty())
		{
			m_faceMatIds.resize(j);
		}
	}

private:
	//////////////////////////////////////////////////////////////////////////
	// Internal helpers

	template<class T>
	static bool AppendArray(int dstCount, std::vector<T>& dst, int srcCount, const std::vector<T>& src, const T& defaultValue)
	{
		if (dstCount < 0 || srcCount < 0)
		{
			return false;
		}

		if ((size_t)dstCount > (std::numeric_limits<size_t>::max)() - (size_t)srcCount ||
			(size_t)dstCount + (size_t)srcCount > size_t((std::numeric_limits<int>::max)()))
		{
			return false;
		}

		if (dst.size() != dstCount && !dst.empty())
		{
			return false;
		}

		if (src.size() != srcCount && !src.empty())
		{
			return false;
		}

		if (srcCount == 0)
		{
			return true;
		}

		if (dst.empty() && src.empty())
		{
			return true;
		}

		dst.reserve(dstCount + srcCount);

		if (dst.size() != dstCount)
		{
			assert(dst.empty());
			dst.resize(dstCount, defaultValue);
		}

		if (src.empty())
		{
			dst.resize(dstCount + srcCount, defaultValue);
		}
		else
		{
			dst.insert(dst.end(), src.begin(), src.end());
		}

		return true;
	}

	// Returns new maximal id or -1 if it was an error or overflow
	static int CondenseIds(std::vector<int>& tmp, int* const ptr, const int count, const int startId)
	{
		if (!ptr || count <= 0 || startId < 0)
		{
			return -1;
		}

		tmp.resize(count);
		for (int i = 0; i < count; ++i)
		{
			tmp[i] = i;
		}

		std::sort(tmp.begin(), tmp.end(), [&ptr](const int i0, const int i1)
		{
			return ptr[i0] < ptr[i1];
		});

		int currentId = startId - 1;
		int prevVal = ~ptr[tmp[0]];
		for (int i = 0; i < count; ++i)
		{
			int& val = ptr[tmp[i]];
			if (prevVal != val)
			{
				prevVal = val;
				if (currentId >= (std::numeric_limits<int>::max)())
				{
					return -1;
				}
				++currentId;
			}
			val = currentId;
		}

		return currentId;
	}

	template<class T>
	static void CompactVertexArray(std::vector<T>& arr, const std::vector<int>& newToOld)
	{
		if (arr.empty())
		{
			return;
		}

		const size_t newCount = newToOld.size();

		std::vector<T> tmp;
		tmp.reserve(newCount);

		for (size_t i = 0; i < newCount; ++i)
		{
			tmp.push_back(arr[newToOld[i]]);
		}

		arr.swap(tmp);
	}

	static void CompactVertexArrayRaw(std::vector<uint8>& arr, size_t elemSizeof, const std::vector<int>& newToOld)
	{
		if (arr.empty())
		{
			return;
		}

		const size_t newCount = newToOld.size();

		std::vector<uint8> tmp;
		tmp.resize(newCount * elemSizeof);

		for (size_t i = 0; i < newCount; ++i)
		{
			memcpy(&tmp[i * elemSizeof], &arr[newToOld[i] * elemSizeof], elemSizeof);
		}

		arr.swap(tmp);
	}

	struct VertexLess
	{
		const Mesh& m;

		VertexLess(const Mesh& mesh)
			: m(mesh) 
		{
		}

		bool operator()(int a, int b) const
		{
			if (!m.m_topologyIds.empty())
			{
				const int res = m.m_topologyIds[a] - m.m_topologyIds[b];
				if (res != 0)
				{
					return res < 0;
				}
			}

			{
				const int res = memcmp(&m.m_positions[a], &m.m_positions[b], sizeof(m.m_positions[0]));
				if (res != 0)
				{
					return res < 0;
				}
			}

			int res = 0;

			if (res == 0 && !m.m_normals.empty())
			{
				res = memcmp(&m.m_normals[a], &m.m_normals[b], sizeof(m.m_normals[0]));
			}

			if (res == 0 && !m.m_texCoords.empty())
			{
				res = memcmp(&m.m_texCoords[a], &m.m_texCoords[b], sizeof(m.m_texCoords[0]));
			}

			if (res == 0 && !m.m_colors.empty())
			{
				res = memcmp(&m.m_colors[a], &m.m_colors[b], sizeof(m.m_colors[0]));
			}

			if (res == 0 && !m.m_alphas.empty())
			{
				res = (int)m.m_alphas[a] - (int)m.m_alphas[b];
			}

			if (res == 0 && !m.m_links.empty())
			{
				if (m.m_links[a].links.size() != m.m_links[b].links.size())
				{
					res = (m.m_links[a].links.size() < m.m_links[b].links.size()) ? -1 : +1;
				}
				else
				{
					res = memcmp(&m.m_links[a].links[0], &m.m_links[b].links[0], sizeof(m.m_links[a].links[0]) * m.m_links[a].links.size());
				}
			}

			if (res == 0 && !m.m_vertexMatIds.empty())
			{
				res = m.m_vertexMatIds[a] - m.m_vertexMatIds[b];
			}

			if (res == 0 && !m.m_aux.empty())
			{
				res = memcmp(&m.m_aux[a * m.m_auxSizeof], &m.m_aux[b * m.m_auxSizeof], m.m_auxSizeof);
			}

			return res < 0;			
		}
	};
};

} // namespace MeshUtils
	
#endif
