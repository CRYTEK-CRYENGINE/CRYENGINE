#pragma once

#include "AutoUVBaseType.h"
#include <vector>

namespace LODGenerator
{
	class CAtlasGenerator;

	enum EUVWarpType
	{
		eUV_Common,
		eUV_ABF,
	};

	class AutoUV
	{
	public:
		AutoUV();
		~AutoUV();

		void Run(Vec3 *pVertices, vtx_idx *indices, int faces,EUVWarpType uvWarpType = eUV_ABF);
		void Prepare(Vec3 *pVertices, vtx_idx *indices, int faces);
		Vec2 Project(SAutoUVStackEntry &se, SAutoUVFace *f, SAutoUVEdge *e);
		void SquarePacker(int &w, int &h, std::vector<SAutoUVSquare> &squares);
		void Unwrap(void);
		IStatObj* MakeMesh(std::vector<SAutoUVFinalVertex> &vertices, std::vector<int> &indices);
		void Render();

	public:
		std::vector<SAutoUVVertex> m_vertices;
		std::vector<SAutoUVFace> m_faces;
		std::vector<SAutoUVEdge> m_edges;
		std::vector<SAutoUVTri> m_tris;
		std::vector<int> m_outIndices;
		std::vector<SAutoUVFinalVertex> m_outVertices;
		IStatObj* m_outMesh;
		LODGenerator::CAtlasGenerator* textureAtlasGenerator;
		std::vector<SAutoUVSquare> m_squares;

	};
};