// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BasicScripts.h"
#include "DesignerEditor.h"
#include "Objects/DesignerObject.h"
#include "Tools/Select/SelectTool.h"
#include "ToolFactory.h"
#include "Core/LoopPolygons.h"

namespace Designer {
namespace Script
{
CBrushDesignerPythonContext s_bdpc;
CBrushDesignerPythonContext s_bdpc_before_init;

MainContext GetContext()
{
	const CSelectionGroup* pGroup = GetIEditor()->GetObjectManager()->GetSelection();
	CBaseObject* pObject = pGroup->GetObject(0);
	if (!pObject->IsKindOf(RUNTIME_CLASS(DesignerObject)))
		throw std::logic_error("The selected object isn't a designer object type");

	CEditTool* pEditor = GetIEditor()->GetEditTool();
	if (pEditor == NULL || !pEditor->IsKindOf(RUNTIME_CLASS(DesignerEditor)))
		throw std::logic_error("The selected object isn't a designer object type");

	DesignerObject* pDesignerObject = (DesignerObject*)pObject;
	DesignerEditor* pDesignerTool = (DesignerEditor*)pEditor;
	DesignerSession* pSession = DesignerSession::GetInstance();

	MainContext mc;
	mc.pObject = pObject;
	mc.pModel = pDesignerObject->GetModel();
	mc.pCompiler = pDesignerObject->GetCompiler();
	mc.pSelected = pSession->GetSelectedElements();

	return mc;
}

void CompileModel(MainContext& mc, bool bForce)
{
	if (bForce || s_bdpc.bAutomaticUpdateMesh)
		ApplyPostProcess(mc, ePostProcess_DataBase | ePostProcess_Mesh);
}

void UpdateSelection(MainContext& mc)
{
	UpdateDrawnEdges(mc);
	DesignerSession::GetInstance()->UpdateSelectionMeshFromSelectedElements(mc);
}

ElementID CBrushDesignerPythonContext::RegisterElements(ElementsPtr pElements)
{
	ElementSet* pPureElements = pElements.get();
	pPureElements->AddRef();
	elementVariables.insert(pPureElements);
	return (ElementID)pPureElements;
}

ElementsPtr CBrushDesignerPythonContext::FindElements(ElementID id)
{
	auto ii = elementVariables.find((ElementSet*)id);
	if (ii == elementVariables.end())
		return NULL;
	return *ii;
}

void CBrushDesignerPythonContext::ClearElementVariables()
{
	auto ii = elementVariables.begin();
	for (; ii != elementVariables.end(); ++ii)
		(*ii)->Release();
	elementVariables.clear();
}

BrushVec3 FromSVecToBrushVec3(const SPyWrappedProperty::SVec& sVec)
{
	return ToBrushVec3(Vec3(sVec.x, sVec.y, sVec.z));
}

void OutputPolygonPythonCreationCode(Polygon* pPolygon)
{
	std::vector<PolygonPtr> outerPolygons = pPolygon->GetLoops()->GetOuterPolygons();
	std::vector<PolygonPtr> innerPolygons = pPolygon->GetLoops()->GetHolePolygons();

	string buffer;
	OutputDebugString("designer.start_polygon_addition()\n");

	std::vector<Vertex> vList;
	for (int i = 0, iPolygonCount(outerPolygons.size()); i < iPolygonCount; ++i)
	{
		outerPolygons[i]->GetLinkedVertices(vList);
		for (int k = 0, iVListCount(vList.size()); k < iVListCount; ++k)
		{
			buffer.Format("designer.add_vertex_to_polygon((%f,%f,%f))\n", vList[k].pos.x, vList[k].pos.y, vList[k].pos.z);
			OutputDebugString(buffer);
		}
	}

	if (!innerPolygons.empty())
	{
		for (int i = 0, iPolygonCount(innerPolygons.size()); i < iPolygonCount; ++i)
		{
			innerPolygons[i]->GetLinkedVertices(vList);
			buffer.Format("\ndesigner.start_to_add_another_hole()\n");
			OutputDebugString(buffer);
			for (int k = 0, iVListCount(vList.size()); k < iVListCount; ++k)
			{
				buffer.Format("designer.add_vertex_to_polygon((%f,%f,%f))\n", vList[k].pos.x, vList[k].pos.y, vList[k].pos.z);
				OutputDebugString(buffer);
			}
		}
	}

	OutputDebugString("designer.finish_polygon_addition()\n");
}

void PyDesignerStart()
{
	s_bdpc_before_init = s_bdpc;
	s_bdpc.Init();
}

void PyDesignerEnd()
{
	s_bdpc.ClearElementVariables();
	s_bdpc = s_bdpc_before_init;
}

void PyDesignerSetEnv(pSPyWrappedProperty name, pSPyWrappedProperty value)
{
	if (name->type != SPyWrappedProperty::eType_String)
		throw std::logic_error("Invalid name type.");

	if (name->stringValue == "move_together" || name->stringValue == "automatic_update_mesh")
	{
		if (value->type != SPyWrappedProperty::eType_Bool)
			throw std::logic_error("the type of the value should be Bool.");
	}

	if (name->stringValue == "move_together")
		s_bdpc.bMoveTogether = value->property.boolValue;
	else if (name->stringValue == "automatic_update_mesh")
		s_bdpc.bAutomaticUpdateMesh = value->property.boolValue;
	else
		throw std::logic_error("The name doesn't exist.");
}

void PyDesignerRecordUndo()
{
	CUndo undo("Calls python scripts in a designer object");
	MainContext mc(GetContext());
	mc.pModel->RecordUndo("Calls python scripts in a designer object", mc.pObject);
}

void PyDesignerUpdateMesh()
{
	CompileModel(GetContext(), true);
}
}
}

/*
   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerStart,
                       designer,
                       start,
                       "Initializes all external variables used in the designer scripts.",
                       "designer.start()" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerEnd,
                       designer,
                       end,
                       "Restores the designer context to the context used before initializing.",
                       "designer.end()" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerSetEnv,
                       designer,
                       set_env,
                       "Sets environment variables.",
                       "designer.set_env( str name, [bool || int || float || str || (float,float,float)] paramValue )" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerRecordUndo,
                       designer,
                       record_undo,
                       "Records the current designer states to be undone.",
                       "designer.record_undo()" );

   REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE( Designer::Script::PyDesignerUpdateMesh,
                       designer,
                       update_mesh,
                       "Updates mesh.",
                       "designer.update_mesh()" );
 */

