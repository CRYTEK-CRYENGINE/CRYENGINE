// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AutoGeneratorBaseType.h"
#include <vector>

class CMesh;
namespace LODGenerator
{
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

		void Run(Vec3 *pVertices, vtx_idx *indices, int faces,EUVWarpType uvWarpType = eUV_Common);
		void Prepare(Vec3 *pVertices, vtx_idx *indices, int faces);
		Vec2 Project(SAutoUVStackEntry &se, SAutoUVFace *f, SAutoUVEdge *e);
		void SquarePacker(int &w, int &h, std::vector<SAutoUVSquare> &squares);
		void Unwrap(void);
		CMesh* MakeMesh(std::vector<SAutoUVFinalVertex> &vertices, std::vector<int> &indices);

	public:
		std::vector<SAutoUVVertex> m_vertices;
		std::vector<SAutoUVFace> m_faces;
		std::vector<SAutoUVEdge> m_edges;
		std::vector<SAutoUVTri> m_tris;
		std::vector<int> m_outIndices;
		std::vector<SAutoUVFinalVertex> m_outVertices;
		std::vector<CMesh*> m_outMeshList;
		//LODGenerator::CAtlasGenerator* textureAtlasGenerator;
		std::vector<SAutoUVSquare> m_squares;

	};
};