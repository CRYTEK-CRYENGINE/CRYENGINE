// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../BaseTool.h"
#include "../UVElement.h"

namespace Designer {
namespace UVMapping
{
struct UVElement;

class LoopSelectionTool : public BaseTool
{
public:
	LoopSelectionTool(EUVMappingTool tool) : BaseTool(tool) {}

	void        Enter() override;

	static bool BorderSelection(UVElementSetPtr pUVElements = NULL);
	static bool LoopSelection(UVElementSetPtr pUVElements = NULL);

private:
	static std::vector<UVElement> BorderSelection(const std::vector<UVElement>& input);
	static int                    FindBestBorderUVEdge(const Vec2& uv0, const Vec2& uv1, UVIslandPtr pUVIslandPtr, const std::vector<UVEdge>& candidateUVEdges);

	static std::vector<UVElement> LoopSelection(const std::vector<UVElement>& input);
	static int                    FindBestLoopUVEdge(const Vec2& uv0, const Vec2& uv1, UVIslandPtr pUVIslandPtr, const std::vector<UVEdge>& candidateUVEdges);
};
}
}

