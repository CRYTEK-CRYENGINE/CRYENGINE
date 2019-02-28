// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Subdivision.h"
#include "Polygon.h"
#include "Convexes.h"

namespace Designer
{
SubdivisionContext Subdivision::CreateSubdividedMesh(Model* pModel, int nSubdivisionLevel)
{
	_smart_ptr<HalfEdgeMesh> pHalfEdgeMesh = new HalfEdgeMesh;
	pHalfEdgeMesh->ConstructMesh(pModel);

	SubdivisionContext sc;
	sc.fullPatches = pHalfEdgeMesh;

	for (int i = 0; i < nSubdivisionLevel; ++i)
		sc = CreateSubdividedMesh(sc);

	return sc;
}

SubdivisionContext Subdivision::CreateSubdividedMesh(SubdivisionContext& sc)
{
	std::vector<Position> vNextFaceLocations;
	std::vector<Position> vNextEdgeLocations;
	std::vector<Position> vNextVertexLocations;

	SubdivisionContext next_sc;
	next_sc.fullPatches = new HalfEdgeMesh;
	next_sc.transitionPatches = sc.transitionPatches ? sc.transitionPatches : new HalfEdgeMesh;
	std::vector<HE_Position>& he_fp_positions = next_sc.fullPatches->GetPositions();

	CalculateNextLocations(sc.fullPatches, he_fp_positions, vNextFaceLocations, vNextEdgeLocations, vNextVertexLocations);

	std::vector<HE_Vertex>& he_fp_vertices = next_sc.fullPatches->GetVertices();
	std::vector<HE_Edge>& he_fp_edges = next_sc.fullPatches->GetEdges();
	std::vector<HE_Face>& he_fp_faces = next_sc.fullPatches->GetFaces();

	std::vector<HE_Vertex>& he_tp_vertices = next_sc.transitionPatches->GetVertices();
	std::vector<HE_Edge>& he_tp_edges = next_sc.transitionPatches->GetEdges();
	std::vector<HE_Face>& he_tp_faces = next_sc.transitionPatches->GetFaces();

	he_fp_vertices.reserve(sc.fullPatches->GetVertexCount() * 4);
	he_fp_edges.reserve(sc.fullPatches->GetEdgeCount() * 4);
	he_fp_faces.reserve(sc.fullPatches->GetFaceCount() * 4);

	int iFaceCount(sc.fullPatches->GetFaceCount());

	std::vector<std::vector<int>> mapFromPrevFaceToEdges(iFaceCount);
	std::vector<int> adjacentPrevFaces;
	adjacentPrevFaces.reserve(sc.fullPatches->GetEdgeCount() * 4);

	for (int i = 0; i < iFaceCount; ++i)
	{
		std::vector<int>& edgeIndices = mapFromPrevFaceToEdges[i];
		const HE_Face& f = sc.fullPatches->GetFace(i);

		//    if( bApplyFeatureAdaptiveSubdivision && !sc.fullPatches->IsIrregularFace(f) )
		//    {
		//      const HE_Edge* first_e = &sc.fullPatches->GetEdge(f);
		//      const HE_Edge* e = first_e;
		//      int vOffset = he_tp_vertices.size();
		//      int eOffset = he_tp_edges.size();
		//      int fOffset = he_tp_faces.size();
		//      int nCount = 0;
		//      do
		//      {
		//        int nPosIndex = next_sc.transitionPatches->AddPos(sc.fullPatches->GetPos(e->vertex));
		//        he_tp_vertices.push_back(HE_Vertex(nPosIndex,eOffset+nCount));
		//        he_tp_edges.push_back(HE_Edge(vOffset+nCount,-1,fOffset,eOffset+nCount+1,0,false));
		//        ++nCount;
		//        e = &sc.fullPatches->GetEdge(e->next_edge);
		//      } while( e != first_e );
		//      he_tp_edges[he_tp_edges.size()-1].next_edge = eOffset;
		//      he_tp_faces.push_back(HE_Face(eOffset,f.pOriginPolygon));
		//      continue;
		//    }
		//
		int nEdge = f.edge;
		std::vector<BrushVec3> vList;
		int nLastEdgeInPrevFace = -1;
		int nHEInitEdgeOffset = he_fp_edges.size();
		do
		{
			const HE_Edge& e = sc.fullPatches->GetEdge(nEdge);
			const HE_Edge& next_e = sc.fullPatches->GetEdge(e.next_edge);
			int nNextVertex = sc.fullPatches->GetEdge(e.next_edge).vertex;
			if (vNextEdgeLocations[nEdge].IsValid() && vNextEdgeLocations[e.next_edge].IsValid() && vNextVertexLocations[nNextVertex].IsValid())
			{
				int nHEFaceOffset = he_fp_faces.size();
				int nHEVertexOffset = he_fp_vertices.size();
				int nHEEdgeOffset = he_fp_edges.size();

				if (nLastEdgeInPrevFace != -1)
					he_fp_edges[nLastEdgeInPrevFace].pair_edge = nHEEdgeOffset;

				adjacentPrevFaces.push_back(-1);
				adjacentPrevFaces.push_back(e.pair_edge == -1 ? -1 : sc.fullPatches->GetEdge(e.pair_edge).face);
				int nNextPairEdge = next_e.pair_edge;
				adjacentPrevFaces.push_back(nNextPairEdge == -1 ? -1 : sc.fullPatches->GetEdge(nNextPairEdge).face);
				adjacentPrevFaces.push_back(-1);

				float sharpness1 = e.sharpness - 1.0f > 0 ? e.sharpness - 1.0f : 0;
				float sharpness2 = next_e.sharpness - 1.0f > 0 ? next_e.sharpness - 1.0f : 0;

				he_fp_edges.push_back(HE_Edge(nHEVertexOffset, nLastEdgeInPrevFace, nHEFaceOffset, nHEEdgeOffset + 1, 0, false));
				he_fp_edges.push_back(HE_Edge(nHEVertexOffset + 1, -1, nHEFaceOffset, nHEEdgeOffset + 2, sharpness1, e.irregular));
				he_fp_edges.push_back(HE_Edge(nHEVertexOffset + 2, -1, nHEFaceOffset, nHEEdgeOffset + 3, sharpness2, next_e.irregular));
				nLastEdgeInPrevFace = he_fp_edges.size();
				if (e.next_edge == f.edge)
				{
					he_fp_edges.push_back(HE_Edge(nHEVertexOffset + 3, nHEInitEdgeOffset, nHEFaceOffset, nHEEdgeOffset, 0, false));
					DESIGNER_ASSERT(he_fp_edges[nHEInitEdgeOffset].pair_edge == -1);
					he_fp_edges[nHEInitEdgeOffset].pair_edge = nLastEdgeInPrevFace;
				}
				else
				{
					he_fp_edges.push_back(HE_Edge(nHEVertexOffset + 3, -1, nHEFaceOffset, nHEEdgeOffset, 0, false));
				}

				he_fp_vertices.push_back(HE_Vertex(vNextFaceLocations[i].pos_index, nHEEdgeOffset, vNextFaceLocations[i].uv));
				he_fp_vertices.push_back(HE_Vertex(vNextEdgeLocations[nEdge].pos_index, nHEEdgeOffset + 1, vNextEdgeLocations[nEdge].uv));
				he_fp_vertices.push_back(HE_Vertex(vNextVertexLocations[nNextVertex].pos_index, nHEEdgeOffset + 2, vNextVertexLocations[nNextVertex].uv));
				he_fp_vertices.push_back(HE_Vertex(vNextEdgeLocations[e.next_edge].pos_index, nHEEdgeOffset + 3, vNextEdgeLocations[e.next_edge].uv));

				if (sharpness1 > 0)
					he_fp_positions[vNextEdgeLocations[nEdge].pos_index].edges.push_back(nHEEdgeOffset + 1);
				if (sharpness2 > 0)
					he_fp_positions[vNextVertexLocations[nNextVertex].pos_index].edges.push_back(nHEEdgeOffset + 2);

				he_fp_faces.push_back(HE_Face(nHEEdgeOffset, f.pOriginPolygon));

				edgeIndices.push_back(nHEEdgeOffset + 1);
				edgeIndices.push_back(nHEEdgeOffset + 2);
			}
			nEdge = e.next_edge;
		}
		while (f.edge != nEdge);
	}

	for (int i = 0, iEdgeCount(he_fp_edges.size()); i < iEdgeCount; ++i)
	{
		if (adjacentPrevFaces[i] == -1 || he_fp_edges[i].pair_edge != -1)
			continue;

		const std::pair<int, int> e0(next_sc.fullPatches->GetVertex(he_fp_edges[i]).pos_index, next_sc.fullPatches->GetVertex(he_fp_edges[he_fp_edges[i].next_edge]).pos_index);

		const std::vector<int>& edges = mapFromPrevFaceToEdges[adjacentPrevFaces[i]];
		for (int k = 0, iEdgeCount2(edges.size()); k < iEdgeCount2; ++k)
		{
			const HE_Edge& he_e = he_fp_edges[edges[k]];
			if (he_e.pair_edge != -1)
				continue;
			const std::pair<int, int> e1(next_sc.fullPatches->GetVertex(he_e).pos_index, next_sc.fullPatches->GetVertex(he_fp_edges[he_e.next_edge]).pos_index);
			if (e0.first == e1.second && e0.second == e1.first)
			{
				he_fp_edges[edges[k]].pair_edge = i;
				he_fp_edges[i].pair_edge = edges[k];
				break;
			}
		}
	}

	return next_sc;
}

void Subdivision::CalculateNextLocations(
  HalfEdgeMesh* pHalfEdgeMesh,
  std::vector<HE_Position>& outNextPosList,
  std::vector<Position>& outNextFaceLocations,
  std::vector<Position>& outNextEdgeLocations,
  std::vector<Position>& outNextVertexLocations)
{
	int iFaceCount(pHalfEdgeMesh->GetFaceCount());
	outNextFaceLocations.reserve(iFaceCount);
	for (int i = 0; i < iFaceCount; ++i)
	{
		const HE_Face& f = pHalfEdgeMesh->GetFace(i);
		outNextPosList.push_back(pHalfEdgeMesh->GetFaceAveragePos(f));
		outNextFaceLocations.push_back(Position(outNextPosList.size() - 1, pHalfEdgeMesh->GetFaceAverageUV(f)));
	}

	int iEdgeCount(pHalfEdgeMesh->GetEdgeCount());
	outNextEdgeLocations.reserve(iEdgeCount);
	std::map<std::pair<int, int>, int> edgeLocationMap;
	for (int i = 0; i < iEdgeCount; ++i)
	{
		const HE_Edge& e = pHalfEdgeMesh->GetEdge(i);
		const HE_Edge* pPairEdge = pHalfEdgeMesh->GetPairEdge(e);
		const BrushVec3& vPos = pHalfEdgeMesh->GetPos(e);
		const HE_Edge& next_e = pHalfEdgeMesh->GetNextEdge(e);
		const BrushVec3& vNextPos = pHalfEdgeMesh->GetPos(next_e);
		const HE_Vertex& v = pHalfEdgeMesh->GetVertex(e);
		const HE_Vertex& next_v = pHalfEdgeMesh->GetVertex(next_e);
		Vec2 edgeUV = (pHalfEdgeMesh->GetUV(e) + pHalfEdgeMesh->GetUV(next_e)) * 0.5f;

		if (edgeLocationMap.find(std::pair<int, int>(next_v.pos_index, v.pos_index)) != edgeLocationMap.end())
		{
			outNextEdgeLocations.push_back(Position(edgeLocationMap[std::pair < int, int > (next_v.pos_index, v.pos_index)], edgeUV));
			continue;
		}

		BrushVec3 vCrease = (vPos + vNextPos) / (BrushFloat)2;
		if (!pPairEdge || e.sharpness >= 0.9999f)
		{
			outNextPosList.push_back(vCrease);
		}
		else
		{
			const BrushVec3& vFacePos = outNextPosList[outNextFaceLocations[e.face].pos_index].pos;
			const BrushVec3& vPairFacePos = outNextPosList[outNextFaceLocations[pPairEdge->face].pos_index].pos;
			BrushVec3 vSmooth = (vPos + vNextPos + vFacePos + vPairFacePos) / (BrushFloat)4;
			if (std::abs(e.sharpness) <= 0.0001f)
				outNextPosList.push_back(vSmooth);
			else
				outNextPosList.push_back((1 - e.sharpness) * vSmooth + e.sharpness * vCrease);
		}
		outNextEdgeLocations.push_back(Position(outNextPosList.size() - 1, edgeUV));
		edgeLocationMap[std::pair < int, int > (v.pos_index, next_v.pos_index)] = outNextPosList.size() - 1;
	}

	int iVertexCount(pHalfEdgeMesh->GetVertexCount());
	outNextVertexLocations.reserve(iVertexCount);
	std::vector<int> vertex_positions(pHalfEdgeMesh->GetPosCount());
	for (int i = 0; i < pHalfEdgeMesh->GetPosCount(); ++i)
		vertex_positions[i] = -1;

	for (int i = 0; i < iVertexCount; ++i)
	{
		BrushVec3 vSum(0, 0, 0);
		int nCount = 0;
		const HE_Vertex& v = pHalfEdgeMesh->GetVertex(i);
		Vec2 uv = pHalfEdgeMesh->GetUV(pHalfEdgeMesh->GetEdge(v.edge));
		if (vertex_positions[v.pos_index] != -1)
		{
			outNextVertexLocations.push_back(Position(vertex_positions[v.pos_index], uv));
			continue;
		}

		bool bFailToFindPair = false;
		const HE_Edge* pEdge = &(pHalfEdgeMesh->GetEdge(v.edge));
		Vec2 faceUV = pHalfEdgeMesh->GetFaceAverageUV(pHalfEdgeMesh->GetFace(pHalfEdgeMesh->GetEdge(v.edge)));
		do
		{
			const HE_Edge& next_e = pHalfEdgeMesh->GetEdge(pEdge->next_edge);
			const HE_Edge* pNextEdgeAroundVertex = pHalfEdgeMesh->FindNextEdgeClockwiseAroundVertex(*pEdge);
			++nCount;
			if (pNextEdgeAroundVertex == NULL || nCount > iEdgeCount)
			{
				bFailToFindPair = true;
				break;
			}
			vSum += (outNextPosList[outNextFaceLocations[pEdge->face].pos_index].pos + pHalfEdgeMesh->GetPos(next_e));
			pEdge = pNextEdgeAroundVertex;
		}
		while (pEdge != &(pHalfEdgeMesh->GetEdge(v.edge)));

		if (bFailToFindPair)
		{
			const HE_Edge& next_e = pHalfEdgeMesh->GetNextEdge(*pEdge);
			const BrushVec3& vSharpEdgePos0 = pHalfEdgeMesh->GetPos(next_e);
			const HE_Edge& end_edge_counterclockwise_aroundvertex = pHalfEdgeMesh->FindEndEdgeCounterClockwiseAroundVertex(*pEdge);
			const BrushVec3& vSharpEdgePos1 = pHalfEdgeMesh->GetPos(end_edge_counterclockwise_aroundvertex);
			outNextPosList.push_back((6 * pHalfEdgeMesh->GetPos(v) + vSharpEdgePos0 + vSharpEdgePos1) / (BrushFloat)8);
		}
		else
		{
			const HE_Position& p = pHalfEdgeMesh->GetPosition(v);
			int edgeCount = p.edges.size();
			const std::vector<HE_Edge>& he_fp_edges = pHalfEdgeMesh->GetEdges();
			BrushFloat sharpness = edgeCount >= 2 ? (BrushFloat)p.GetSharpness(he_fp_edges) : 0;

			BrushVec3 vSmooth = vSum / (BrushFloat)(nCount * nCount) + (pHalfEdgeMesh->GetPos(v) * (nCount - 2)) / (BrushFloat)nCount;
			BrushVec3 vCorner = p.pos;

			if (edgeCount < 2 || sharpness == 0)
				outNextPosList.push_back(vSmooth);
			else if (edgeCount > 2 && sharpness >= 0.9999f)
				outNextPosList.push_back(vCorner);
			else if (edgeCount > 2 && sharpness >= 0 && sharpness < 0.9999f)
				outNextPosList.push_back((1 - sharpness) * vSmooth + sharpness * vCorner);
			else if (edgeCount == 2)
			{
				const BrushVec3 vSharpEdgePos0 = pHalfEdgeMesh->GetPos(he_fp_edges[he_fp_edges[p.edges[0]].next_edge]);
				const BrushVec3 vShartEdgePos1 = pHalfEdgeMesh->GetPos(he_fp_edges[he_fp_edges[p.edges[1]].next_edge]);
				BrushVec3 vCrease = (6 * vCorner + vSharpEdgePos0 + vShartEdgePos1) / (BrushFloat)8;

				if (sharpness >= 0.9999f)
					outNextPosList.push_back(vCrease);
				else if (sharpness >= 0 && sharpness < 0.9999f)
					outNextPosList.push_back((1 - sharpness) * vSmooth + sharpness * vCrease);
			}
		}
		outNextVertexLocations.push_back(Position(outNextPosList.size() - 1, uv));
		vertex_positions[v.pos_index] = outNextPosList.size() - 1;
	}
}
}

