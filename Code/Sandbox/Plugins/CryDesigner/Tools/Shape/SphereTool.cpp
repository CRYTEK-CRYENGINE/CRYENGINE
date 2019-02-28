// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SphereTool.h"
#include "Core/Model.h"
#include "Util/PrimitiveShape.h"
#include "DesignerEditor.h"
#include "ToolFactory.h"
#include "Core/Helper.h"

namespace Designer
{
void SphereTool::UpdateShape()
{
	__super::UpdateShape();

	if (!m_pBasePolygon)
		return;

	m_MatTo001 = ToBrushMatrix33(Matrix33::CreateRotationV0V1(GetPlane().Normal(), Vec3(0, 0, 1)));
	m_MatTo001.SetTranslation(-GetCurrentSpotPos());

	AABB aabb;
	aabb.Reset();
	for (int i = 0, iVertexCount(m_pBasePolygon->GetVertexCount()); i < iVertexCount; ++i)
		aabb.Add(m_MatTo001.TransformPoint(m_pBasePolygon->GetPos(i)));

	aabb.max.z = aabb.max.z + m_DiscParameter.m_Radius;
	aabb.min.z = aabb.min.z - m_DiscParameter.m_Radius;

	if (aabb.IsReset())
		return;

	m_SpherePolygons.clear();
	PrimitiveShape bp;
	bp.CreateSphere(
	  aabb.min,
	  aabb.max,
	  m_DiscParameter.m_NumOfSubdivision,
	  &m_SpherePolygons);

	UpdateDesignerBasedOnSpherePolygons(BrushMatrix34::CreateIdentity());
}

void SphereTool::UpdateDesignerBasedOnSpherePolygons(const BrushMatrix34& tm)
{
	BrushMatrix34 matFrom001 = m_MatTo001.GetInverted();

	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Construction);
	GetModel()->Clear();
	for (int i = 0, iPolygonCount(m_SpherePolygons.size()); i < iPolygonCount; ++i)
	{
		m_SpherePolygons[i]->Transform(matFrom001);
		AddPolygonWithSubMatID(m_SpherePolygons[i]);
	}
	ApplyPostProcess(ePostProcess_Mirror);
	CompileShelf(eShelf_Construction);
}

void SphereTool::Register()
{
	ShapeTool::RegisterShape(GetStartSpot().m_pPolygon);
}
}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_Sphere, eToolGroup_Shape, "Sphere", SphereTool,
                                                           sphere, "runs sphere tool", "designer.sphere")

