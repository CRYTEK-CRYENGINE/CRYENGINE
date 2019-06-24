#pragma once

namespace LODGenerator
{
	class CTopologyGraph;
	class CTopologyTransform
	{
	public:
		static CTopologyGraph* TransformToTopology(Vec3 *pVertices, vtx_idx *indices, int faces);
		static CTopologyGraph* TransformToTopology(IStatObj *pObject,bool bCalNormal = false);
		static void FillMesh( CTopologyGraph* pPolygon, IStatObj *pObject);
	};
};

