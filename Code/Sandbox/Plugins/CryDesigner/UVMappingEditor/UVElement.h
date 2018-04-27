// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Core/UVIsland.h"
#include "UVMappingEditorCommon.h"

namespace Designer {
namespace UVMapping
{
struct UVElement
{
	UVElement() {}
	UVElement(UVIslandPtr pUVIsland) : m_pUVIsland(pUVIsland) {}
	UVElement(const UVPolygon& uvPolygon)
	{
		m_pUVIsland = uvPolygon.pUVIsland;
		m_UVVertices = uvPolygon.uvs;
	}
	UVElement(const UVEdge& uvEdge)
	{
		m_pUVIsland = uvEdge.pUVIsland;
		m_UVVertices.push_back(uvEdge.uv0);
		m_UVVertices.push_back(uvEdge.uv1);
	}
	UVElement(const UVVertex& uv)
	{
		m_pUVIsland = uv.pUVIsland;
		m_UVVertices.push_back(uv);
	}

	bool operator==(const UVElement& element) const
	{
		return (IsUVIsland() && element.IsUVIsland() && m_pUVIsland == element.m_pUVIsland) ||
		       (IsPolygon() && element.IsPolygon() && m_UVVertices.size() == element.m_UVVertices.size() && m_UVVertices[0].pPolygon == element.m_UVVertices[0].pPolygon) ||
		       (IsEquivalentEdge(element)) ||
		       (IsVertex() && element.IsVertex() && m_UVVertices[0].pPolygon == element.m_UVVertices[0].pPolygon && m_UVVertices[0].vIndex == element.m_UVVertices[0].vIndex);
	}

	bool IsUVIsland() const { return m_pUVIsland && m_UVVertices.empty(); }
	bool IsVertex() const   { return m_UVVertices.size() == 1; }
	bool IsEdge() const     { return m_UVVertices.size() == 2; }
	bool IsPolygon() const  { return m_UVVertices.size() >= 3; }

	bool IsEquivalentEdge(const UVElement& element) const;
	bool HasEdge(const UVElement& element) const;

	UVIslandPtr           m_pUVIsland;
	std::vector<UVVertex> m_UVVertices;
};

class UVElementSet : public _i_reference_target_t
{
public:
	bool             AddElement(const UVElement& element);
	void             EraseElement(const UVElement& element);
	bool             HasElement(const UVElement& element);
	void             Clear();

	bool             AddSharedOtherElements(const UVElement& element);
	void             EraseAllElementWithIdenticalUVs(const UVElement& element);

	UVElement&       GetElement(int idx) { return m_Elements[idx]; }
	const UVElement* FindElement(const UVElement& element) const;

	int              GetCount() const { return m_Elements.size();  }
	bool             IsEmpty() const  { return m_Elements.empty(); }

	UVElementSet&    operator=(const UVElementSet& uvSet);
	UVElementSet*    Clone();

	void             CopyFrom(UVElementSet* pElementSet);
	void             Join(UVElementSet* pElementSet);

	bool             AllOnlyVertices() const;
	bool             AllOnlyEdges() const;
	bool             AllOnlyPolygons() const;
	bool             AllOnlyUVIslands() const;

	Vec3             GetCenterPos() const;

private:

	std::vector<UVElement> m_Elements;
};

typedef _smart_ptr<UVElementSet> UVElementSetPtr;
}
}

