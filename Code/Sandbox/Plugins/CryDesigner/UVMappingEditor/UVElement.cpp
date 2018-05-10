// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UVElement.h"
#include "DesignerEditor.h"
#include "Core/UVIslandManager.h"
#include "UVMappingEditor.h"

namespace Designer {
namespace UVMapping
{

bool UVElement::IsEquivalentEdge(const UVElement& element) const
{
	if (!IsEdge() || !element.IsEdge())
		return false;

	if (m_pUVIsland != element.m_pUVIsland)
		return false;

	if (m_UVVertices[0].pPolygon == element.m_UVVertices[0].pPolygon)
	{
		if (m_UVVertices[0].vIndex == element.m_UVVertices[0].vIndex && m_UVVertices[1].vIndex == element.m_UVVertices[1].vIndex)
			return true;

		if (m_UVVertices[0].vIndex == element.m_UVVertices[1].vIndex && m_UVVertices[1].vIndex == element.m_UVVertices[0].vIndex)
			return true;
	}
	else
	{
		const Vec2& uv00 = m_UVVertices[0].GetUV();
		const Vec2& uv01 = m_UVVertices[1].GetUV();

		const Vec2& uv10 = element.m_UVVertices[0].GetUV();
		const Vec2& uv11 = element.m_UVVertices[1].GetUV();

		if (Comparison::IsEquivalent(uv00, uv10) && Comparison::IsEquivalent(uv01, uv11) || Comparison::IsEquivalent(uv00, uv11) && Comparison::IsEquivalent(uv01, uv10))
			return true;
	}

	return false;
}

bool UVElement::HasEdge(const UVElement& element) const
{
	if (!IsPolygon() || !element.IsEdge())
		return false;

	for (int i = 0, iCount(m_UVVertices.size()); i < iCount; ++i)
	{
		const UVVertex& v0 = m_UVVertices[i];
		const UVVertex& v1 = m_UVVertices[(i + 1) % iCount];

		if (v0.pPolygon == element.m_UVVertices[0].pPolygon)
		{
			if ((v0.vIndex == element.m_UVVertices[0].vIndex && v1.vIndex == element.m_UVVertices[1].vIndex) ||
			    (v0.vIndex == element.m_UVVertices[1].vIndex && v1.vIndex == element.m_UVVertices[0].vIndex))
			{
				return true;
			}
		}
		else
		{
			const Vec2& uv0 = v0.GetUV();
			const Vec2& uv1 = v1.GetUV();

			const Vec2& element_uv0 = element.m_UVVertices[0].GetUV();
			const Vec2& element_uv1 = element.m_UVVertices[1].GetUV();

			if ((Comparison::IsEquivalent(uv0, element_uv0) && Comparison::IsEquivalent(uv1, element_uv1)) || (Comparison::IsEquivalent(uv0, element_uv1) && Comparison::IsEquivalent(uv1, element_uv0)))
				return true;
		}
	}

	return false;
}

bool UVElementSet::AddElement(const UVElement& element)
{
	const UVElement* pElement = FindElement(element);
	if (pElement)
		return false;
	m_Elements.push_back(element);
	return true;
}

bool UVElementSet::AddSharedOtherElements(const UVElement& element)
{
	if (element.m_UVVertices.empty())
		return false;

	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();

	for (int i = 0, iCount(pUVIslandMgr->GetCount()); i < iCount; ++i)
	{
		UVIslandPtr pUVIsland = pUVIslandMgr->GetUVIsland(i);

		for (int k = 0, iPolygonCount(pUVIsland->GetCount()); k < iPolygonCount; ++k)
		{
			PolygonPtr polygon = pUVIsland->GetPolygon(k);
			if (element.m_UVVertices[0].pPolygon == polygon)
				continue;

			if (element.IsVertex())
			{
				int vtxIdx = 0;
				if (polygon->GetVertexIndex(element.m_UVVertices[0].GetPos(), vtxIdx))
				{
					if (!Comparison::IsEquivalent(element.m_UVVertices[0].GetUV(), polygon->GetUV(vtxIdx)))
						AddElement(UVElement(UVVertex(pUVIsland, polygon, vtxIdx)));
				}
			}
			else if (element.IsEdge())
			{
				int vtxIdx0 = 0;
				int vtxIdx1 = 0;
				if (polygon->GetVertexIndex(element.m_UVVertices[0].GetPos(), vtxIdx0) && polygon->GetVertexIndex(element.m_UVVertices[1].GetPos(), vtxIdx1))
				{
					if (!(Comparison::IsEquivalent(element.m_UVVertices[1].GetUV(), polygon->GetUV(vtxIdx0)) && Comparison::IsEquivalent(element.m_UVVertices[0].GetUV(), polygon->GetUV(vtxIdx1))) &&
					    !(Comparison::IsEquivalent(element.m_UVVertices[0].GetUV(), polygon->GetUV(vtxIdx0)) && Comparison::IsEquivalent(element.m_UVVertices[1].GetUV(), polygon->GetUV(vtxIdx1))))
					{
						AddElement(UVElement(UVEdge(pUVIsland, UVVertex(pUVIsland, polygon, vtxIdx1), UVVertex(pUVIsland, polygon, vtxIdx0))));
					}
				}
			}
		}
	}

	return true;
}

void UVElementSet::EraseAllElementWithIdenticalUVs(const UVElement& element)
{
	auto ii = m_Elements.begin();
	for (; ii != m_Elements.end(); )
	{
		bool bErased = false;
		if (ii->IsVertex() && element.IsVertex())
		{
			if (Comparison::IsEquivalent(ii->m_UVVertices[0].GetUV(), element.m_UVVertices[0].GetUV()))
			{
				ii = m_Elements.erase(ii);
				bErased = true;
			}
		}
		else if (ii->IsEdge() && element.IsEdge())
		{
			if (Comparison::IsEquivalent(ii->m_UVVertices[0].GetUV(), element.m_UVVertices[0].GetUV()) && Comparison::IsEquivalent(ii->m_UVVertices[1].GetUV(), element.m_UVVertices[1].GetUV()) ||
			    Comparison::IsEquivalent(ii->m_UVVertices[0].GetUV(), element.m_UVVertices[1].GetUV()) && Comparison::IsEquivalent(ii->m_UVVertices[1].GetUV(), element.m_UVVertices[0].GetUV()))
			{
				ii = m_Elements.erase(ii);
				bErased = true;
			}
		}
		if (!bErased)
			++ii;
	}
}

void UVElementSet::EraseElement(const UVElement& element)
{
	for (auto ii = m_Elements.begin(); ii != m_Elements.end(); )
	{
		if (*ii == element)
			ii = m_Elements.erase(ii);
		else
			++ii;
	}
}

bool UVElementSet::HasElement(const UVElement& element)
{
	for (auto ii = m_Elements.begin(); ii != m_Elements.end(); ++ii)
	{
		if (*ii == element)
			return true;
	}
	return false;
}

void UVElementSet::Clear()
{
	m_Elements.clear();
}

const UVElement* UVElementSet::FindElement(const UVElement& element) const
{
	for (auto ii = m_Elements.begin(); ii != m_Elements.end(); ++ii)
	{
		if ((*ii) == element || (*ii).HasEdge(element))
			return &(*ii);
	}
	return NULL;
}

Vec3 UVElementSet::GetCenterPos() const
{
	if (m_Elements.empty())
		return Vec3(0, 0, 0);

	auto element = m_Elements.begin();
	AABB aabb;
	aabb.Reset();
	for (; element != m_Elements.end(); ++element)
	{
		if ((*element).IsUVIsland())
		{
			aabb.Add((*element).m_pUVIsland->GetUVBoundBox().GetCenter());
		}
		else
		{
			for (int i = 0, iVertexCount((*element).m_UVVertices.size()); i < iVertexCount; ++i)
				aabb.Add((*element).m_UVVertices[i].GetUV());
		}
	}

	return aabb.GetCenter();
}

bool UVElementSet::AllOnlyVertices() const
{
	auto iter = m_Elements.begin();
	for (; iter != m_Elements.end(); ++iter)
	{
		if (!iter->IsVertex())
			return false;
	}
	return true;
}

bool UVElementSet::AllOnlyEdges() const
{
	auto iter = m_Elements.begin();
	for (; iter != m_Elements.end(); ++iter)
	{
		if (!iter->IsEdge())
			return false;
	}
	return true;
}

bool UVElementSet::AllOnlyPolygons() const
{
	auto iter = m_Elements.begin();
	for (; iter != m_Elements.end(); ++iter)
	{
		if (!iter->IsPolygon())
			return false;
	}
	return true;
}

bool UVElementSet::AllOnlyUVIslands() const
{
	auto iter = m_Elements.begin();
	for (; iter != m_Elements.end(); ++iter)
	{
		if (!iter->IsUVIsland())
			return false;
	}
	return true;
}

UVElementSet& UVElementSet::operator=(const UVElementSet& uvSet)
{
	m_Elements = uvSet.m_Elements;
	return *this;
}

UVElementSet* UVElementSet::Clone()
{
	UVElementSet* pClone = new UVElementSet;
	for (int i = 0, iCount(m_Elements.size()); i < iCount; ++i)
		pClone->AddElement(m_Elements[i]);
	return pClone;
}

void UVElementSet::CopyFrom(UVElementSet* pElementSet)
{
	*this = *pElementSet;
	for (int i = 0, iCount(m_Elements.size()); i < iCount; ++i)
		m_Elements[i].m_pUVIsland->ResetPolygonsInModel(GetUVEditor()->GetModel());
}

void UVElementSet::Join(UVElementSet* pElementSet)
{
	for (int i = 0, iCount(pElementSet->GetCount()); i < iCount; ++i)
		m_Elements.push_back(pElementSet->GetElement(i));
}

}
}

