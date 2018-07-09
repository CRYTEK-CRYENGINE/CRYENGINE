// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ModelDB.h"
#include "Model.h"
#include "Viewport.h"

namespace Designer
{
class PlaneDB
{
public:
	static const BrushFloat& kPlaneEpsilon;

	PlaneDB(){}
	~PlaneDB(){}

	PlaneDB(const PlaneDB& planeMgr)
	{
		operator=(planeMgr);
	}

	PlaneDB& operator=(const PlaneDB& planeMgr)
	{
		m_NormalXs = planeMgr.m_NormalXs;
		m_NormalYs = planeMgr.m_NormalYs;
		m_NormalZs = planeMgr.m_NormalZs;
		m_Distances = planeMgr.m_Distances;
		return *this;
	}

#define INITIALIZE_INDICES_AND_SIGN_AND_FIND_PLANE() int nNormalXIndex(-1); \
  int nNormalYIndex(-1);                                                    \
  int nNormalZIndex(-1);                                                    \
  int nDistanceIndex(-1);                                                   \
  FindPlane(inPlane, nNormalXIndex, nNormalYIndex, nNormalZIndex, nDistanceIndex);

	BrushPlane AddPlane(const BrushPlane& inPlane)
	{
		INITIALIZE_INDICES_AND_SIGN_AND_FIND_PLANE();

		if (nNormalXIndex == -1)
		{
			nNormalXIndex = m_NormalXs.size();
			m_NormalXs.push_back(inPlane.Normal().x);
		}

		if (nNormalYIndex == -1)
		{
			nNormalYIndex = m_NormalYs.size();
			m_NormalYs.push_back(inPlane.Normal().y);
		}

		if (nNormalZIndex == -1)
		{
			nNormalZIndex = m_NormalZs.size();
			m_NormalZs.push_back(inPlane.Normal().z);
		}

		if (nDistanceIndex == -1)
		{
			nDistanceIndex = m_Distances.size();
			m_Distances.push_back(inPlane.Distance());
		}

		return BrushPlane(BrushVec3(m_NormalXs[nNormalXIndex], m_NormalYs[nNormalYIndex], m_NormalZs[nNormalZIndex]), m_Distances[nDistanceIndex]);
	}
	bool FindPlane(const BrushPlane& inPlane, BrushPlane& outPlane) const
	{
		INITIALIZE_INDICES_AND_SIGN_AND_FIND_PLANE();

		if (nNormalXIndex != -1 && nNormalYIndex != -1 && nNormalZIndex != -1 && nDistanceIndex != -1)
		{
			outPlane.Set(BrushVec3(m_NormalXs[nNormalXIndex], m_NormalYs[nNormalYIndex], m_NormalZs[nNormalZIndex]), m_Distances[nDistanceIndex]);
			return true;
		}

		return false;
	}
	void Clear()
	{
		m_NormalXs.clear();
		m_NormalYs.clear();
		m_NormalZs.clear();
		m_Distances.clear();
	}

private:
	void FindPlane(const BrushPlane& inPlane, int& outNormalXIndex, int& outNormalYIndex, int& outNormalZIndex, int& outDistanceIndex) const
	{
		Find(inPlane.Normal().x, m_NormalXs, outNormalXIndex);
		Find(inPlane.Normal().y, m_NormalYs, outNormalYIndex);
		Find(inPlane.Normal().z, m_NormalZs, outNormalZIndex);
		Find(inPlane.Distance(), m_Distances, outDistanceIndex);
	}
	void Find(const BrushFloat& inValue, const std::vector<BrushFloat>& valueList, int& outIndex) const
	{
		for (int i = 0, iValueSize(valueList.size()); i < iValueSize; ++i)
		{
			if (fabs(valueList[i] - inValue) < kPlaneEpsilon)
			{
				outIndex = i;
				return;
			}
		}
		outIndex = -1;
	}

	std::vector<BrushFloat> m_NormalXs;
	std::vector<BrushFloat> m_NormalYs;
	std::vector<BrushFloat> m_NormalZs;
	std::vector<BrushFloat> m_Distances;
};

const BrushFloat& ModelDB::kDBEpsilon = kDesignerEpsilon;
const BrushFloat& PlaneDB::kPlaneEpsilon = kDesignerEpsilon;

ModelDB::ModelDB() :
	m_pPlaneDB(new PlaneDB)
{
}

ModelDB::ModelDB(const ModelDB& db) :
	m_pPlaneDB(new PlaneDB)
{
	operator=(db);
}

ModelDB::~ModelDB()
{
}

ModelDB& ModelDB::operator=(const ModelDB& db)
{
	m_VertexDB = db.m_VertexDB;
	*m_pPlaneDB = *db.m_pPlaneDB;
	return *this;
}

void ModelDB::Reset(Model* pModel, int nFlag, ShelfID nValidShelfID)
{
	MODEL_SHELF_RECONSTRUCTOR(pModel);

	if (nFlag & eDBRF_Vertex)
	{
		m_VertexDB.clear();
		for (int shelfID = eShelf_Base; shelfID < cShelfMax; ++shelfID)
		{
			if (nValidShelfID != eShelf_Any && nValidShelfID != static_cast<ShelfID>(shelfID))
				continue;
			pModel->SetShelf(static_cast<ShelfID>(shelfID));
			int iPolygonSize(pModel->GetPolygonCount());
			for (int i = 0; i < iPolygonSize; ++i)
			{
				PolygonPtr pPolygon = pModel->GetPolygon(i);
				for (int k = 0, iVertexSize(pPolygon->GetVertexCount()); k < iVertexSize; ++k)
					AddVertex(pPolygon->GetPos(k), k, pPolygon);
			}
		}
	}

	if (nFlag & eDBRF_Plane)
	{
		m_pPlaneDB->Clear();
		for (int shelfID = eShelf_Base; shelfID < cShelfMax; ++shelfID)
		{
			if (nValidShelfID != eShelf_Any && nValidShelfID != static_cast<ShelfID>(shelfID))
				continue;
			pModel->SetShelf(static_cast<ShelfID>(shelfID));
			int iPolygonSize(pModel->GetPolygonCount());
			for (int i = 0; i < iPolygonSize; ++i)
			{
				PolygonPtr pPolygon = pModel->GetPolygon(i);
				BrushPlane plane;
				if (!m_pPlaneDB->FindPlane(pPolygon->GetPlane(), plane))
					plane = m_pPlaneDB->AddPlane(pPolygon->GetPlane());
				pPolygon->UpdatePlane(plane);
			}
		}
	}

	UpdateAllPolygonVertices();
}

void ModelDB::Reset(const std::vector<PolygonPtr>& polygons, int nFlag, ShelfID nValidShelfID)
{
	if (nFlag & eDBRF_Vertex)
	{
		m_VertexDB.clear();
		int iPolygonSize(polygons.size());
		for (int i = 0; i < iPolygonSize; ++i)
		{
			PolygonPtr pPolygon = polygons[i];
			for (int k = 0, iVertexSize(pPolygon->GetVertexCount()); k < iVertexSize; ++k)
				AddVertex(pPolygon->GetPos(k), k, pPolygon);
		}
	}

	if (nFlag & eDBRF_Plane)
	{
		m_pPlaneDB->Clear();
		int iPolygonSize(polygons.size());
		for (int i = 0; i < iPolygonSize; ++i)
		{
			PolygonPtr pPolygon = polygons[i];
			BrushPlane plane;
			if (m_pPlaneDB->FindPlane(pPolygon->GetPlane(), plane))
			{
				pPolygon->UpdatePlane(plane);
			}
			else
			{
				plane = m_pPlaneDB->AddPlane(pPolygon->GetPlane());
				pPolygon->UpdatePlane(plane);
			}
		}
	}

	UpdateAllPolygonVertices();
}

void ModelDB::AddVertex(const BrushVec3& vertex, int nVertexIndex, PolygonPtr pPolygon)
{
	Vertex newV;
	Vertex* pV = &newV;

	for (int i = 0, iVertexSize(m_VertexDB.size()); i < iVertexSize; ++i)
	{
		Vertex& v = m_VertexDB[i];
		if (Comparison::IsEquivalent(v.m_Pos, vertex))
		{
			pV = &v;
			break;
		}
	}

	Mark mark;
	mark.m_pPolygon = pPolygon;
	mark.m_VertexIndex = nVertexIndex;
	pV->m_MarkList.push_back(mark);

	if (pV == &newV)
	{
		newV.m_Pos = vertex;
		m_VertexDB.push_back(newV);
	}
}

bool ModelDB::QueryAsVertex(const BrushVec3& pos, QueryResult& qResult) const
{
	for (int i = 0, iVertexSize(m_VertexDB.size()); i < iVertexSize; ++i)
	{
		const Vertex& v = m_VertexDB[i];
		if (v.m_Pos.IsEquivalent(pos, kDBEpsilon))
		{
			bool bFoundSame(false);
			for (int k = 0, iQuerySize(qResult.size()); k < iQuerySize; ++k)
			{
				if (qResult[k].m_Pos.IsEquivalent(v.m_Pos, kDBEpsilon))
				{
					bFoundSame = true;
					break;
				}
			}
			if (!bFoundSame)
				qResult.push_back(v);
		}
	}

	return !qResult.empty();
}

bool ModelDB::QueryAsRectangle(CViewport* pView, const BrushMatrix34& worldTM, const CRect& rect, QueryResult& qResult) const
{
	for (int i = 0, iVertexSize(m_VertexDB.size()); i < iVertexSize; ++i)
	{
		const Vertex& v = m_VertexDB[i];
		CPoint pt = pView->WorldToView(worldTM.TransformPoint(v.m_Pos));
		if (rect.PtInRect(pt))
		{
			bool bFoundSame(false);
			for (int k = 0, iQuerySize(qResult.size()); k < iQuerySize; ++k)
			{
				if (qResult[k].m_Pos.IsEquivalent(v.m_Pos, kDBEpsilon))
				{
					bFoundSame = true;
					break;
				}
			}
			if (!bFoundSame)
				qResult.push_back(v);
		}
	}

	return !qResult.empty();
}

void ModelDB::AddMarkToVertex(const BrushVec3& vPos, const Mark& mark)
{
	for (int i = 0, iVertexSize(m_VertexDB.size()); i < iVertexSize; ++i)
	{
		Vertex& v = m_VertexDB[i];
		if (v.m_Pos.IsEquivalent(vPos, kDBEpsilon))
		{
			v.m_MarkList.push_back(mark);
			return;
		}
	}
}

void ModelDB::UpdateAllPolygonVertices()
{
	for (int i = 0, vListSize(m_VertexDB.size()); i < vListSize; ++i)
	{
		Vertex& v = m_VertexDB[i];
		for (int k = 0, markListSize(v.m_MarkList.size()); k < markListSize; ++k)
		{
			PolygonPtr pPolygon = v.m_MarkList[k].m_pPolygon;
			if (pPolygon == NULL)
				continue;
			if (v.m_MarkList[k].m_VertexIndex >= pPolygon->GetVertexCount())
				continue;
			pPolygon->SetPos(v.m_MarkList[k].m_VertexIndex, v.m_Pos);
		}
	}
}

bool ModelDB::UpdatePolygonVertices(PolygonPtr pPolygon)
{
	if (!pPolygon)
		return false;

	bool bChanged = false;

	for (int i = 0, iVertexSize(pPolygon->GetVertexCount()); i < iVertexSize; ++i)
	{
		BrushVec3 vPos = pPolygon->GetPos(i);
		QueryResult qResult;
		if (QueryAsVertex(vPos, qResult))
		{
			bChanged = true;
			pPolygon->SetPos(i, qResult[0].m_Pos);
		}
	}

	return bChanged;
}

BrushVec3 ModelDB::Snap(const BrushVec3& pos)
{
	QueryResult qResult;
	if (QueryAsVertex(pos, qResult))
		return qResult[0].m_Pos;
	return pos;
}

BrushPlane ModelDB::AddPlane(const BrushPlane& plane)
{
	return m_pPlaneDB->AddPlane(plane);
}

bool ModelDB::FindPlane(const BrushPlane& inPlane, BrushPlane& outPlane) const
{
	return m_pPlaneDB->FindPlane(inPlane, outPlane);
}

void ModelDB::GetVertexList(std::vector<BrushVec3>& outVertexList) const
{
	int vListSize = m_VertexDB.size();
	outVertexList.resize(vListSize);
	for (int i = 0; i < vListSize; ++i)
		outVertexList[i] = m_VertexDB[i].m_Pos;
}
};

