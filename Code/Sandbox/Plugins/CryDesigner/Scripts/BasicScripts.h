// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Util/BoostPythonHelpers.h"
#include "Util/ElementSet.h"

namespace Designer
{
class DesignerObject;
class Model;
class ModelCompiler;
class Polygon;

namespace Script
{
MainContext GetContext();
void        CompileModel(MainContext& mc, bool bForce = false);
BrushVec3   FromSVecToBrushVec3(const SPyWrappedProperty::SVec& sVec);
void        UpdateSelection(MainContext& mc);

typedef __int64 ElementID;
class CBrushDesignerPythonContext
{
public:
	CBrushDesignerPythonContext()
	{
		Init();
	}

	~CBrushDesignerPythonContext()
	{
		ClearElementVariables();
	}

	CBrushDesignerPythonContext(const CBrushDesignerPythonContext& context)
	{
		operator=(context);
	}

	CBrushDesignerPythonContext& operator=(const CBrushDesignerPythonContext& context)
	{
		bMoveTogether = context.bMoveTogether;
		bAutomaticUpdateMesh = context.bAutomaticUpdateMesh;

		ClearElementVariables();

		auto ii = context.elementVariables.begin();
		for (; ii != context.elementVariables.end(); ++ii)
		{
			(*ii)->AddRef();
			elementVariables.insert(*ii);
		}

		return *this;
	}

	void Init()
	{
		elementVariables.clear();
		bMoveTogether = true;
		bAutomaticUpdateMesh = true;
	}

	bool bMoveTogether;
	bool bAutomaticUpdateMesh;

	ElementID   RegisterElements(ElementsPtr pElements);
	ElementsPtr FindElements(ElementID id);
	void        ClearElementVariables();

private:
	std::set<ElementSet*> elementVariables;
};

extern CBrushDesignerPythonContext s_bdpc;
extern CBrushDesignerPythonContext s_bdpc_before_init;

void OutputPolygonPythonCreationCode(Polygon* pPolygon);

}
}

