// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryRenderer/IRenderAuxGeom.h>
#include "Octree.h"

namespace OctreeDebug
{
enum class Style
{
	None    = 0,
	Solid   = BIT(0),
	Contour = BIT(1),
	Full    = Solid | Contour,
};

struct ConsiderAllNodesFull
{
	template<typename T>
	float operator()(const T&) const { return 1.0f; }
};

inline void DrawLeaf(IRenderAuxGeom* pRenderAuxGeom, const AABB& domain, const Matrix34& tm, const float fillRatio, const ColorB& emptyColor, const ColorB& fullColor, const Style style)
{
	if (style != Style::None)
	{
		const float shrinkRatio = 0.995f;
		const uint8 fillAlpha = 64;
		const uint8 lineAlpha = 255;

		const float fullFactor = clamp_tpl(fillRatio, 0.0f, 1.0f);
		ColorB fillColor;
		fillColor.lerpFloat(emptyColor, fullColor, fullFactor);
		fillColor.a = fillAlpha;
		const ColorB lineColor(fillColor.r, fillColor.g, fillColor.b, lineAlpha);

		// compress the box a bit for avoiding overlapping edges of the boxes
		AABB shrunkDomain = domain;
		shrunkDomain.Expand(domain.GetSize() * -shrinkRatio);

		if (0 != (int(style) & int(Style::Solid)))
			pRenderAuxGeom->DrawAABB(shrunkDomain, tm, true, fillColor, eBBD_Faceted);

		if (0 != (int(style) & int(Style::Contour)))
			pRenderAuxGeom->DrawAABB(shrunkDomain, tm, false, lineColor, eBBD_Faceted);
	}
}

template<typename TOctree, typename TRatioFunc, typename TPrimitive>
void Draw(IRenderAuxGeom* pRenderAuxGeom, const TOctree& octree, const Matrix34& octreeTM, const TPrimitive& selection, const ColorB& emptyColor, const ColorB& fullColor, const Style style, const TRatioFunc& ratioFunc)
{
	const SAuxGeomRenderFlags previousFlags = pRenderAuxGeom->GetRenderFlags();
	const bool bNeedAlphaFlag = (uint32(e_AlphaBlended) != (previousFlags.m_renderFlags & uint32(e_AlphaBlended)));
	if (bNeedAlphaFlag)
		pRenderAuxGeom->SetRenderFlags(previousFlags.m_renderFlags | uint32(e_AlphaBlended));

	typename TOctree::TConstLeaves leaves;
	octree.FindIntersectingLeaves(leaves, selection);

	for (const typename TOctree::SConstLeafInfo & leaf : leaves)
	{
		const typename TOctree::Node& node = *leaf.pNode;
		const float fillRatio = ratioFunc(node.primitiveContainer);
		DrawLeaf(pRenderAuxGeom, node.domain, octreeTM, fillRatio, emptyColor, fullColor, style);
	}

	if (bNeedAlphaFlag)
		pRenderAuxGeom->SetRenderFlags(previousFlags);
}

//! Helper function if you don't need so much control on the display of your Octree.
template<typename TOctree>
void Draw(IRenderAuxGeom* pRenderAuxGeom, const TOctree& octree, const Matrix34& octreeTM, const ColorB& nodeColor, const Style style = Style::Full)
{
	Draw(pRenderAuxGeom, octree, octreeTM, OctreeUtils::SIntersectAll(), nodeColor, nodeColor, style, ConsiderAllNodesFull());
}
}
