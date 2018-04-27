// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SelectTool.h"
#include "../UVCluster.h"

namespace Designer {
namespace UVMapping
{
class MoveTool : public SelectTool
{
public:
	MoveTool(EUVMappingTool tool);

	void OnLButtonDown(const SMouseEvent& me) override;
	void OnLButtonUp(const SMouseEvent& me) override;
	void OnMouseMove(const SMouseEvent& me) override;

	void OnGizmoLMBDown(int mode) override;
	void OnGizmoLMBUp(int mode) override;
	void OnTransformGizmo(int mode, const Vec3& offset) override;

private:

	void RecordUndo();
	void EndUndoRecord();

	struct DraggingMove
	{
		bool         bStarted;
		int          mouse_x, mouse_y;
		mutable bool bEnough;

		void         Start(int x, int y)
		{
			bStarted = true;
			mouse_x = x;
			mouse_y = y;
			bEnough = false;
		}

		bool IsDraggedEnough(int x, int y) const
		{
			if (!bEnough && (std::abs(mouse_x - x) > 5 || std::abs(mouse_y - y) > 5))
				bEnough = true;
			return bEnough;
		}
	};

	UVCluster    m_VertexCluster;
	DraggingMove m_DraggingContext;
	Vec3         m_PrevHitPos;
};

GENERATE_MOVETOOL_CLASS(Island, EUVMappingTool)
GENERATE_MOVETOOL_CLASS(Polygon, EUVMappingTool)
GENERATE_MOVETOOL_CLASS(Edge, EUVMappingTool)
GENERATE_MOVETOOL_CLASS(Vertex, EUVMappingTool)

}
}

