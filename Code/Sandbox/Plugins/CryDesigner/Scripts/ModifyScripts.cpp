// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Util/BoostPythonHelpers.h"
#include "BasicScripts.h"
#include "Objects/DesignerObject.h"
#include "Core/Model.h"
#include "Tools/Edit/OffsetTool.h"
#include "Tools/Edit/ExtrudeTool.h"
#include "Tools/Select/MoveTool.h"
#include "Tools/Modify/SubdivisionTool.h"
#include "Core/LoopPolygons.h"

namespace Designer {
namespace Script
{
void PyDesignerOffset(float fScale, pSPyWrappedProperty bPyCreatBridgeEdges)
{
	if (bPyCreatBridgeEdges->type != SPyWrappedProperty::eType_Bool)
		throw std::logic_error("bBridgeEdges is invalid data type.");

	MainContext mc(GetContext());
	if (mc.pSelected->IsEmpty())
		throw std::logic_error("There are no selected elements.");
	for (int i = 0, iCount(mc.pSelected->GetCount()); i < iCount; ++i)
	{
		if (!mc.pSelected->Get(i).IsPolygon() || !mc.pSelected->Get(i).m_pPolygon)
			continue;

		std::vector<PolygonPtr> holes = mc.pSelected->Get(i).m_pPolygon->GetLoops()->GetHolePolygons();
		PolygonPtr pScaledPolygon = mc.pSelected->Get(i).m_pPolygon->Clone();
		pScaledPolygon->Scale((BrushFloat)fScale, true);

		OffsetTool::ApplyOffset(mc.pModel, pScaledPolygon, mc.pSelected->Get(i).m_pPolygon, holes, bPyCreatBridgeEdges->property.boolValue);
	}

	mc.pSelected->Clear();
	CompileModel(mc);
}

void PyDesignerExtrude(float fHeight, float fScale)
{
	if (std::abs(fHeight) < kDesignerEpsilon)
		throw std::logic_error("height is too close to 0.");

	MainContext mc(GetContext());
	if (mc.pSelected->IsEmpty())
		throw std::logic_error("There are no selected elements.");

	for (int i = 0, iCount(mc.pSelected->GetCount()); i < iCount; ++i)
	{
		if (!mc.pSelected->Get(i).IsPolygon() || !mc.pSelected->Get(i).m_pPolygon)
			continue;
		PolygonPtr pPolygon = mc.pModel->QueryEquivalentPolygon(mc.pSelected->Get(i).m_pPolygon);
		if (pPolygon)
			ExtrudeTool::Extrude(mc, pPolygon, fHeight, fScale);
	}

	mc.pSelected->Clear();
	CompileModel(mc);
}

void PyDesignerTranslate(pSPyWrappedProperty vOffset)
{
	if (vOffset->type != SPyWrappedProperty::eType_Vec3)
		throw std::logic_error("offset should be vec3 type.");
	MainContext mc(GetContext());
	BrushMatrix34 tm = BrushMatrix34::CreateTranslationMat(FromSVecToBrushVec3(vOffset->property.vecValue));
	MoveTool::Transform(mc, tm, s_bdpc.bMoveTogether);
	UpdateSelection(mc);
	CompileModel(mc);
}

void PyDesignerRotate(pSPyWrappedProperty vRotation)
{
	if (vRotation->type != SPyWrappedProperty::eType_Vec3)
		throw std::logic_error("rotation should be vec3 type.");
	MainContext mc(GetContext());
	BrushVec3 vRot = FromSVecToBrushVec3(vRotation->property.vecValue);
	BrushMatrix34 tm = BrushMatrix34::CreateRotationXYZ(Ang3_tpl<BrushFloat>(DEG2RAD(vRot.x), DEG2RAD(vRot.y), DEG2RAD(vRot.z)));
	MoveTool::Transform(mc, tm, s_bdpc.bMoveTogether);
	UpdateSelection(mc);
	CompileModel(mc);
}

void PyDesignerScale(pSPyWrappedProperty vScale)
{
	if (vScale->type != SPyWrappedProperty::eType_Vec3)
		throw std::logic_error("scale should be vec3 type.");
	MainContext mc(GetContext());
	BrushMatrix34 tm = BrushMatrix34::CreateScale(FromSVecToBrushVec3(vScale->property.vecValue));
	MoveTool::Transform(mc, tm, s_bdpc.bMoveTogether);
	UpdateSelection(mc);
	CompileModel(mc);
}

void PyDesignerSubdivide(int nLevel)
{
	if (nLevel < 0 || nLevel >= kMaximumSubdivisionLevel)
		throw std::logic_error("a level should be between 0 and 4.");

	SubdivisionTool::Subdivide(nLevel, Script::s_bdpc.bAutomaticUpdateMesh);
}
}
}

/*
   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerOffset,
                       designer,
                       offset,
                       "Adds scaled faces inside the selected faces.",
                       "designer.offset(float scale, bool bBridgeEdges)" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerExtrude,
                       designer,
                       extrude,
                       "Extrudes the selected faces by the specified height in a normal direction of each selected polygon after scaling it by the specified scale.",
                       "designer.extrude(float height, float scale)" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerTranslate,
                       designer,
                       translate,
                       "Translates the selected elements by the specified offset.",
                       "designer.translate((float offset_x,float offset_y,float offset_z))" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerRotate,
                       designer,
                       rotate,
                       "Rotates the selected elements by the specified degree offset.",
                       "designer.rotate((float rotation_x,float rotation_y,float rotation_z))" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerScale,
                       designer,
                       scale,
                       "Scales the selected elements by the specified offset.",
                       "designer.scale((float scale_x,float scale_y,float scale_z))" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerSubdivide,
                       designer,
                       subdivide,
                       "Subdivides the selected designer objects.",
                       "designer.subdivide(int nLevel)" );
 */

