// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "UVMappingEditorCommon.h"
#include "QViewportEvents.h"

class QViewport;

namespace Designer {
namespace UVMapping
{

class BaseTool : public _i_reference_target_t
{
public:

	BaseTool(EUVMappingTool tool) : m_Tool(tool) {}

	virtual void   Enter()                                        {}
	virtual void   Leave()                                        {}

	virtual void   OnLButtonDown(const SMouseEvent& me)           {}
	virtual void   OnLButtonUp(const SMouseEvent& me)             {}
	virtual void   OnMouseMove(const SMouseEvent& me)             {}

	virtual void   Display(DisplayContext& dc)                    {}

	virtual void   OnGizmoLMBDown(int mode)                       {}
	virtual void   OnGizmoLMBUp(int mode)                         {}
	virtual void   OnTransformGizmo(int mode, const Vec3& offset) {}

	EUVMappingTool GetTool() const                                { return m_Tool; }

private:

	EUVMappingTool m_Tool;
};

}
}

