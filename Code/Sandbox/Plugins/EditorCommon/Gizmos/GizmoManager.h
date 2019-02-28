// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IEditor.h"
#include "IGizmoManager.h"
#include <EditorFramework/Preferences.h>
#include "Gizmo.h"

typedef _smart_ptr<CGizmo> CGizmoPtr;

//////////////////////////////////////////////////////////////////////////
struct EDITOR_COMMON_API SGizmoPreferences : public SPreferencePage
{
	enum ERotationInteractionMode
	{
		eRotationDial,
		eRotationLinear
	};

	SGizmoPreferences()
		: SPreferencePage("Gizmo", "Viewport/Gizmo")
		, axisGizmoSize(0.15f)
		, helperScale(1.0f)
		, axisConstraint(AXIS_XY)
		, referenceCoordSys(COORDS_LOCAL)
		, enabled(true)
		, axisGizmoText(false)
		, rotationInteraction(eRotationDial)
	{
	}

	virtual bool Serialize(yasli::Archive& ar) override;

	float                         axisGizmoSize;
	float                         helperScale;
	AxisConstrains                axisConstraint;
	RefCoordSys                   referenceCoordSys;
	bool                          enabled;
	bool                          axisGizmoText;
	enum ERotationInteractionMode rotationInteraction;
};

EDITOR_COMMON_API extern SGizmoPreferences gGizmoPreferences;

/** GizmoManager manages set of currently active Gizmo objects.
 */
class EDITOR_COMMON_API CGizmoManager : public IGizmoManager
{
public:
	ITransformManipulator* AddManipulator(ITransformManipulatorOwner* pOwner);
	void                   RemoveManipulator(ITransformManipulator* pManipulator);

	void    AddGizmo(CGizmo* gizmo);
	void    RemoveGizmo(CGizmo* gizmo);

	int     GetGizmoCount() const override;
	CGizmo* GetGizmoByIndex(int nIndex) const override;

	CGizmo* GetHighlightedGizmo() const override;

	void    Display(DisplayContext& dc) override;
	bool    HitTest(HitContext& hc);

	bool    HandleMouseInput(IDisplayViewport* view, EMouseEvent event, CPoint& point, int flags) override;

private:
	typedef std::set<CGizmoPtr> Gizmos;
	Gizmos m_gizmos;
	//! gizmo that cursor is over. It receives input events and may start interaction
	CGizmoPtr m_highlighted;
};

