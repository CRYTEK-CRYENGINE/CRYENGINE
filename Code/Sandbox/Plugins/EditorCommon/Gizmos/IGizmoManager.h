// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IGizmoManager_h__
#define __IGizmoManager_h__
#pragma once
#include "ITransformManipulator.h"
#include "IEditor.h" // for EMouseEvent

class CGizmo;
struct DisplayContext;
struct HitContext;
struct IDisplayViewport;

/** GizmoManager manages set of currently active Gizmo objects.
 */
struct EDITOR_COMMON_API IGizmoManager
{
	virtual ITransformManipulator* AddManipulator(ITransformManipulatorOwner* pOwner) = 0;
	virtual void    RemoveManipulator(ITransformManipulator* pManipulator) = 0;

	virtual void    AddGizmo(CGizmo* gizmo) = 0;
	virtual void    RemoveGizmo(CGizmo* gizmo) = 0;

	virtual int     GetGizmoCount() const = 0;
	virtual CGizmo* GetGizmoByIndex(int nIndex) const = 0;

	virtual CGizmo* GetHighlightedGizmo() const = 0;

	virtual void    Display(DisplayContext& dc) = 0;

	virtual bool    HandleMouseInput(IDisplayViewport*, EMouseEvent event, CPoint& point, int flags) = 0;
};

#endif // __IGizmoManager_h__

