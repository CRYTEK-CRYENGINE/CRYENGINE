// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Core/Polygon.h"
#include "Core/ModelDB.h"

class CBaseObject;

namespace Designer
{
struct Element
{
	Element() : m_bIsolated(false) {}
	Element(CBaseObject* pObject, PolygonPtr pPolygon) : m_bIsolated(false)
	{
		SetPolygon(pObject, pPolygon);
	}
	Element(CBaseObject* pObject, const BrushEdge3D& edge) : m_bIsolated(false)
	{
		SetEdge(pObject, edge);
	}
	Element(CBaseObject* pObject, const BrushVec3& vertex) : m_bIsolated(false)
	{
		SetVertex(pObject, vertex);
	}
	Element   GetMirroredElement(const BrushPlane& mirrorPlane) const;
	bool      operator==(const Element& info);
	void      Invalidate();
	bool      IsEquivalent(const Element& elementInfo) const;

	bool      IsVertex()  const { return m_Vertices.size() == 1; }
	bool      IsEdge()  const   { return m_Vertices.size() == 2; }
	bool      IsPolygon() const { return m_Vertices.size() >= 3; }

	BrushVec3 GetPos() const
	{
		assert(IsVertex());
		if (!IsVertex())
			return BrushVec3(0, 0, 0);
		return m_Vertices[0];
	}

	BrushEdge3D GetEdge() const
	{
		assert(IsEdge());
		if (!IsEdge())
			return BrushEdge3D(BrushVec3(0, 0, 0), BrushVec3(0, 0, 0));
		return BrushEdge3D(m_Vertices[0], m_Vertices[1]);
	}

	void SetPolygon(CBaseObject* pObject, PolygonPtr pPolygon)
	{
		m_pPolygon = pPolygon;
		int iVertexSize(pPolygon->GetVertexCount());
		m_Vertices.reserve(iVertexSize);
		for (int i = 0; i < iVertexSize; ++i)
			m_Vertices.push_back(pPolygon->GetPos(i));
		m_pObject = pObject;
	}

	void SetEdge(CBaseObject* pObject, const BrushEdge3D& edge)
	{
		m_pPolygon = NULL;
		m_Vertices.resize(2);
		m_Vertices[0] = edge.m_v[0];
		m_Vertices[1] = edge.m_v[1];
		m_pObject = pObject;
	}

	void SetVertex(CBaseObject* pObject, const BrushVec3& vertex)
	{
		m_pPolygon = NULL;
		m_Vertices.resize(1);
		m_Vertices[0] = vertex;
		m_pObject = pObject;
	}

	~Element(){}

	std::vector<BrushVec3>  m_Vertices;
	PolygonPtr              m_pPolygon;
	_smart_ptr<CBaseObject> m_pObject;
	bool                    m_bIsolated;
};

class ElementSet : public _i_reference_target_t
{
public:
	void Clear()
	{
		m_Elements.clear();
	}

	int GetCount() const
	{
		return (int)m_Elements.size();
	}

	int GetPolygonCount() const;
	int GetEdgeCount() const;
	int GetVertexCount() const;

	ElementSet(){}
	~ElementSet(){}

	ElementSet(const ElementSet& elementSet)
	{
		operator=(elementSet);
	}

	ElementSet& operator=(const ElementSet& elementSet)
	{
		m_Elements = elementSet.m_Elements;
		return *this;
	}

	const Element& Get(int nIndex) const  { return m_Elements[nIndex]; }
	Element&       operator[](int nIndex) { return m_Elements[nIndex]; }

	void           Set(int nIndex, const Element& element)
	{
		m_Elements[nIndex] = element;
	}

	void Set(const ElementSet& elements)
	{
		operator=(elements);
	}

	bool                    IsEmpty() const { return m_Elements.empty(); }

	bool                    Add(ElementSet& elements);
	bool                    Add(const Element& element);

	bool                    PickFromModel(CBaseObject* pObject, Model* pModel, CViewport* viewport, CPoint point, int nFlag, bool bOnlyIncludeCube, BrushVec3* pOutPickedPos);

	void                    Display(CBaseObject* pObject, DisplayContext& dc) const;
	void                    FindElementsInRect(const CRect& rect, CViewport* pView, const Matrix34& worldTM, bool bOnlyUseSelectionCube, std::vector<Element>& elements) const;

	std::vector<PolygonPtr> GetAllPolygons() const;

	BrushVec3               GetNormal(Model* pModel) const;
	Matrix33                GetOrts(Model* pModel) const;
	void                    Erase(int nElementFlags);
	void                    Erase(const Element& element);
	bool                    Erase(ElementSet& elements);
	void                    EraseUnusedPolygonElements(Model* pModel);

	bool                    Has(const Element& elementInfo) const;
	bool                    HasVertex(const BrushVec3& vertex) const;
	string                 GetElementsInfoText();
	void                    RemoveInvalidElements();

	void                    ApplyOffset(const BrushVec3& vOffset);
	void                    Transform(const BrushMatrix34& tm);

	bool                    IsIsolated() const;
	bool                    HasPolygonSelected(PolygonPtr pPolygon) const;

	ModelDB::QueryResult    QueryFromElements(Model* pModel) const;
	PolygonPtr              QueryNearestVertex(
	  CBaseObject* pObject,
	  Model* pModel,
	  CViewport* pView,
	  CPoint point,
	  const BrushRay& ray,
	  bool bOnlyPickFirstVertexInOpenPolygon,
	  BrushVec3& outPos,
	  BrushVec3* pOutNormal = NULL) const;

	_smart_ptr<Model> CreateModel();

private:
	PolygonPtr PickPolygonFromRepresentativeBox(
	  CBaseObject* pObject,
	  Model* pModel,
	  CViewport* pView,
	  CPoint point,
	  BrushVec3& outPickedPos) const;

private:
	std::vector<Element> m_Elements;
};
typedef _smart_ptr<ElementSet> ElementsPtr;
}

