// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Util/BoostPythonHelpers.h"
#include "Objects/DesignerObject.h"
#include "DesignerEditor.h"
#include "Tools/Select/SelectTool.h"
#include "Tools/Select/SelectGrowTool.h"
#include "Tools/Select/InvertSelectionTool.h"
#include "Tools/Select/SelectAllNoneTool.h"
#include "Tools/Select/SelectConnectedTool.h"
#include "Tools/Select/LoopSelectionTool.h"
#include "Tools/Select/RingSelectionTool.h"
#include "Tools/Select/SelectAllNoneTool.h"
#include "Util/ElementSet.h"
#include "BasicScripts.h"

namespace Designer {
namespace Script
{
DesignerEditor* GetDesignerEditTool()
{
	CEditTool* pEditor = GetIEditor()->GetEditTool();
	if (pEditor->IsKindOf(RUNTIME_CLASS(DesignerEditor)))
		return (DesignerEditor*)pEditor;
	return NULL;
}

void PyDesignerSelect(ElementID nElementKeyID)
{
	ElementsPtr pElements = s_bdpc.FindElements(nElementKeyID);
	if (pElements == NULL)
		throw std::logic_error("The element doesn't exist.");
	MainContext mc(GetContext());
	mc.pSelected->Add(*pElements);
	UpdateSelection(mc);
}

void PyDesignerDeselect(ElementID nElementKeyID)
{
	ElementsPtr pElements = s_bdpc.FindElements(nElementKeyID);
	if (pElements == NULL)
		throw std::logic_error("The element doesn't exist.");
	MainContext mc(GetContext());
	mc.pSelected->Erase(*pElements);
	UpdateSelection(mc);
}

void PyDesignerSetSelection(ElementID nElementKeyID)
{
	ElementsPtr pElements = s_bdpc.FindElements(nElementKeyID);
	if (pElements == NULL)
		throw std::logic_error("The element doesn't exist.");
	MainContext mc(GetContext());
	mc.pSelected->Clear();
	mc.pSelected->Set(*pElements);
	UpdateSelection(mc);
}

void PyDesignerSelectAllVertices()
{
	MainContext mc(GetContext());
	SelectAllNoneTool::SelectAllVertices(mc.pObject, mc.pModel);
	UpdateSelection(mc);
	if (GetDesignerEditTool())
		GetDesignerEditTool()->SwitchTool(eDesigner_Vertex);
}

void PyDesignerSelectAllEdges()
{
	MainContext mc(GetContext());
	SelectAllNoneTool::SelectAllEdges(mc.pObject, mc.pModel);
	UpdateSelection(mc);
	if (GetDesignerEditTool())
		GetDesignerEditTool()->SwitchTool(eDesigner_Edge);
}

void PyDesignerSelectAllFaces()
{
	MainContext mc(GetContext());
	SelectAllNoneTool::SelectAllFaces(mc.pObject, mc.pModel);
	UpdateSelection(mc);
	if (GetDesignerEditTool())
		GetDesignerEditTool()->SwitchTool(eDesigner_Polygon);
}

void PyDesignerDeselectAllVertices()
{
	MainContext mc(GetContext());
	SelectAllNoneTool::DeselectAllVertices();
	UpdateSelection(mc);
}

void PyDesignerDeselectAllEdges()
{
	MainContext mc(GetContext());
	SelectAllNoneTool::DeselectAllEdges();
	UpdateSelection(mc);
}

void PyDesignerDeselectAllFaces()
{
	MainContext mc(GetContext());
	SelectAllNoneTool::DeselectAllFaces();
	UpdateSelection(mc);
}

void PyDesignerDeselectAll()
{
	MainContext mc(GetContext());
	mc.pSelected->Clear();
	UpdateSelection(mc);
}

void PyDesignerGrowSelection()
{
	MainContext mc(GetContext());
	SelectGrowTool::GrowSelection(mc);
	UpdateSelection(mc);
}

void PyDesignerSelectConnectedFaces()
{
	MainContext mc(GetContext());
	SelectConnectedTool::SelectConnectedPolygons(mc);
	UpdateSelection(mc);
}

void PyDesignerLoopSelection()
{
	MainContext mc(GetContext());
	LoopSelectionTool::LoopSelection(mc);
	UpdateSelection(mc);
}

void PyDesignerRingSelection()
{
	MainContext mc(GetContext());
	RingSelectionTool::RingSelection(mc);
	UpdateSelection(mc);
}

void PyDesignerInvertSelection()
{
	MainContext mc(GetContext());
	InvertSelectionTool::InvertSelection(mc);
	UpdateSelection(mc);
}

void PyDesignerSelectVertexMode()
{
	if (GetDesignerEditTool())
		GetDesignerEditTool()->SwitchTool(eDesigner_Vertex);
}

void PyDesignerSelectEdgeMode()
{
	if (GetDesignerEditTool())
		GetDesignerEditTool()->SwitchTool(eDesigner_Edge);
}

void PyDesignerSelectFaceMode()
{
	if (GetDesignerEditTool())
		GetDesignerEditTool()->SwitchTool(eDesigner_Polygon);
}

void PyDesignerSelectPivotMode()
{
	if (GetDesignerEditTool())
		GetDesignerEditTool()->SwitchTool(eDesigner_Pivot);
}
}
}
/*
   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerSelect,
                       designer,
                       select,
                       "Selects elements with nID.",
                       "designer.select( int nID )" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerDeselect,
                       designer,
                       deselect,
                       "Deselects elements with nID.",
                       "designer.deselect( int nID )" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerSetSelection,
                       designer,
                       set_selection,
                       "Deselect the existing selections and sets the new selection.",
                       "designer.set_selection( int nID )" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerSelectAllVertices,
                       designer,
                       select_all_vertices,
                       "Selects all vertices.",
                       "designer.select_all_vertices()" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerSelectAllEdges,
                       designer,
                       select_all_edges,
                       "Selects all edges.",
                       "designer.select_all_edges()" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerSelectAllFaces,
                       designer,
                       select_all_polygons,
                       "Selects all polygons.",
                       "designer.select_all_polygons()" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerDeselectAllVertices,
                       designer,
                       deselect_all_vertices,
                       "Deselects all vertices.",
                       "designer.deselect_all_vertices()" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerDeselectAllEdges,
                       designer,
                       deselect_all_edges,
                       "Deselects all edges.",
                       "designer.deselect_all_edges()" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerDeselectAllFaces,
                       designer,
                       deselect_all_polygons,
                       "Deselects all faces.",
                       "designer.deselect_all_polygons()" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerDeselectAll,
                       designer,
                       deselect_all,
                       "Deselects all elements.",
                       "designer.deselect_all()" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerSelectConnectedFaces,
                       designer,
                       select_connected_faces,
                       "Selects all connected faces with the selected elements.",
                       "designer.select_connected_faces()" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerGrowSelection,
                       designer,
                       grow_selection,
                       "Grows selection",
                       "designer.grow_selection()" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerLoopSelection,
                       designer,
                       loop_selection,
                       "Selects a loop of edges that are connected in a end to end.",
                       "designer.loop_selection()" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerRingSelection,
                       designer,
                       ring_selection,
                       "Selects a sequence of edges that are not connected, but on opposite sides to each other continuing along a face loop.",
                       "designer.ring_selection()" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerInvertSelection,
                       designer,
                       invert_selection,
                       "Selects all components that are not selected, and deselect currently selected components.",
                       "designer.invert_selection()" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerSelectVertexMode,
                       designer,
                       select_vertexmode,
                       "Select Vertex Mode of CryDesigner",
                       "designer.select_vertexmode()");

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerSelectEdgeMode,
                       designer,
                       select_edgemode,
                       "Select Edge Mode of CryDesigner",
                       "designer.select_edgemode()");

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerSelectFaceMode,
                       designer,
                       select_facemode,
                       "Select Face Mode of CryDesigner",
                       "designer.select_facemode()");

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerSelectPivotMode,
                       designer,
                       select_pivotmode,
                       "Select Pivot Mode of CryDesigner",
                       "designer.select_pivotmode()");
 */

