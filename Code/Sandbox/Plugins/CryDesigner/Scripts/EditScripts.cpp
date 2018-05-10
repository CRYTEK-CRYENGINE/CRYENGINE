// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Util/BoostPythonHelpers.h"
#include "BasicScripts.h"
#include "Objects/DesignerObject.h"
#include "Tools/Edit/WeldTool.h"
#include "Tools/Edit/FillTool.h"
#include "Tools/Edit/FlipTool.h"
#include "Tools/Edit/MergeTool.h"
#include "Tools/Edit/SeparateTool.h"
#include "Tools/Edit/RemoveTool.h"
#include "Tools/Edit/CopyTool.h"
#include "Tools/Edit/RemoveDoublesTool.h"
#include "Tools/Select/SelectTool.h"
#include "DesignerEditor.h"
#include "Core/Model.h"
#include "Core/ModelCompiler.h"

namespace Designer {
namespace Script
{
ElementID PyDesignerFlip()
{
	MainContext mc = GetContext();
	if (mc.pSelected->IsEmpty())
		throw std::logic_error("There are no selected elements.");

	ElementSet flippedElements;
	FlipTool::FlipPolygons(mc, mc.pSelected->GetAllPolygons(), flippedElements);
	CompileModel(mc);
	return s_bdpc.RegisterElements(new ElementSet(flippedElements));
}

void PyDesignerMergePolygons()
{
	MainContext mc = GetContext();
	if (mc.pSelected->IsEmpty())
		throw std::logic_error("There are no selected elements.");

	MergeTool::MergePolygons(mc);
	mc.pSelected->Clear();
	CompileModel(mc);
}

void PyDesignerSeparate()
{
	MainContext mc = GetContext();
	if (mc.pSelected->IsEmpty())
		throw std::logic_error("There are no selected elements.");

	SeparateTool::Separate(mc);
	CompileModel(mc);
}

void PyDesignerRemove()
{
	MainContext mc = GetContext();
	if (mc.pSelected->IsEmpty())
		throw std::logic_error("There are no selected elements.");

	RemoveTool::RemoveSelectedElements(mc, false);
	mc.pSelected->Clear();
	CompileModel(mc);
}

ElementID PyDesignerCopy()
{
	MainContext mc = GetContext();
	if (mc.pSelected->IsEmpty())
		throw std::logic_error("There are no selected elements.");

	ElementSet copiedElements;
	CopyTool::Copy(mc, &copiedElements);
	CompileModel(mc);

	return s_bdpc.RegisterElements(new ElementSet(copiedElements));
}

void PyDesignerRemoveDoubles(float fDistance)
{
	MainContext mc = GetContext();
	if (mc.pSelected->IsEmpty())
		throw std::logic_error("There are no selected elements.");

	RemoveDoublesTool::RemoveDoubles(mc, fDistance);
	mc.pSelected->Clear();
	CompileModel(mc);
}

void PyDesignerWeld()
{
	MainContext mc = GetContext();
	if (mc.pSelected->IsEmpty())
		throw std::logic_error("There are no selected elements.");

	std::vector<BrushVec3> vertices;
	for (int i = 0, iSelectedElementCount(mc.pSelected->GetCount()); i < iSelectedElementCount; ++i)
	{
		if (mc.pSelected->Get(i).IsVertex())
			vertices.push_back(mc.pSelected->Get(i).GetPos());
	}

	if (vertices.size() != 2)
		throw std::logic_error("Only two vertices need to be selected.");

	WeldTool::Weld(mc, vertices[0], vertices[1]);
	mc.pSelected->Clear();
	CompileModel(mc);
}
}
}

/*
   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerFlip,
                       designer,
                       flip,
                       "Flips the selected polygons. Returns nID which points out fliped polygons.",
                       "designer.flip()" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerCopy,
                       designer,
                       copy,
                       "Copies the selected polygons. Returns nID which points out copied polygons.",
                       "designer.copy()" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerMergePolygons,
                       designer,
                       merge_polygons,
                       "Merges the selected polygons.",
                       "designer.merge_polygons()" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerRemove,
                       designer,
                       remove,
                       "Removes the selected faces or edges.",
                       "designer.remove()" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerSeparate,
                       designer,
                       separate,
                       "Separates the selected polygons to a new designer object.",
                       "designer.separate()" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerRemoveDoubles,
                       designer,
                       remove_doubles,
                       "Removes doubles from the select elements within the specified distance.",
                       "designer.remove_doubles( float distance )" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerWeld,
                       designer,
                       weld,
                       "Puts the first selected vertex together into the second selected vertex.",
                       "designer.weld()" );
 */

