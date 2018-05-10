// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"
#include "Util/ElementSet.h"

namespace Designer
{
//! Every tool in CryDesigner is inherited from the BaseTool class or the class inherited from the BaseTool class.
class FlipTool : public BaseTool
{
public:

	FlipTool(EDesignerTool tool) : BaseTool(tool)
	{
	}

	//! When switched to this tool, Enter() method is called.
	void Enter() override;
	//! When switched to the another tool, Leave() method is called.
	//! DesignerEditor::SwitchTool() calls the Leave() method of the current tool and then the Enter() method of the new tool. The new tool becomes the current tool.
	void Leave() override;
	//! Flips the input polygons
	//! param mc the main context which contains the CBaseObject, Model and ModelCompiler instances.
	//! param polygons the input polygons which will be flipped.
	//! param outFlippedElements the flipped polygons are returns in a form of the ElementSet.
	static void FlipPolygons(MainContext mc, std::vector<PolygonPtr>& polygons, ElementSet& outFlipedElements);

private:
	void FlipPolygons();

	ElementSet m_FlipedSelectedElements;
};
}

