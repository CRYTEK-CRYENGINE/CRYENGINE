// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Polygon.h"

namespace Designer
{
class Model;
class PlaneDB;

class ModelDB : public _i_reference_target_t
{
public:
	static const BrushFloat& kDBEpsilon;
	struct Mark
	{
		Mark() :
			m_pPolygon(NULL),
			m_VertexIndex(-1)
		{
		}

		PolygonPtr m_pPolygon;
		int        m_VertexIndex;
	};
	typedef std::vector<Mark> MarkList;

	struct Vertex
	{
		BrushVec3 m_Pos;
		MarkList  m_MarkList;

		void      Merge(const Vertex& v)
		{
			m_MarkList.insert(m_MarkList.end(), v.m_MarkList.begin(), v.m_MarkList.end());
		}
	};

	typedef std::vector<Vertex> QueryResult;

public:
	ModelDB();
	ModelDB(const ModelDB& db);
	~ModelDB();
	ModelDB&   operator=(const ModelDB& db);

	void       Reset(Model* pModel, int nFlag, ShelfID nValidShelfID = eShelf_Any);
	void       Reset(const std::vector<PolygonPtr>& polygons, int nFlag, ShelfID nValidShelfID = eShelf_Any);
	bool       QueryAsVertex(const BrushVec3& pos, QueryResult& qResult) const;
	bool       QueryAsRectangle(CViewport* pView, const BrushMatrix34& worldTM, const CRect& rect, QueryResult& qResult) const;
	bool       QueryAsRay(BrushVec3& vRaySrc, BrushVec3& vRayDir, QueryResult& qResult) const;
	void       AddMarkToVertex(const BrushVec3& vPos, const Mark& mark);
	bool       UpdatePolygonVertices(PolygonPtr pPolygon);
	BrushVec3  Snap(const BrushVec3& pos);

	BrushPlane AddPlane(const BrushPlane& plane);
	bool       FindPlane(const BrushPlane& inPlane, BrushPlane& outPlane) const;

	void       GetVertexList(std::vector<BrushVec3>& outVertexList) const;

private:
	void AddVertex(const BrushVec3& vertex, int nVertexIndex, PolygonPtr pPolygon);
	void UpdateAllPolygonVertices();

private:

	std::vector<Vertex>      m_VertexDB;
	std::unique_ptr<PlaneDB> m_pPlaneDB;
};
}

