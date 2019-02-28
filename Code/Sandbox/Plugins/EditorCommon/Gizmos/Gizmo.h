// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Objects/BaseObject.h"

// forward declarations.
struct IDisplayViewport;
struct DisplayContext;
struct HitContext;

enum EGizmoFlags
{
	EGIZMO_SELECTABLE            = 0x0001, //! If set gizmo can be selected by clicking.
	EGIZMO_HIDDEN                = 0x0002, //! If set gizmo hidden and should not be displayed.
	EGIZMO_HIGHLIGHTED           = 0x0004, //! gizmo is highlighted
	EGIZMO_INTERACTING           = 0x0008, //! gizmo is active and manipulated right now. Implies highlighted
	EGIZMO_DELETE                = 0x0010, //! gizmo is marked for deletion
	EGIZMO_INVALID               = 0x0020, //! gizmo is invalid, mark for update next cycle
};

/** Any helper object that BaseObjects can use to display some useful information like tracks.
    Gizmo's life time should be controlled by their owning BaseObjects.
 */
class EDITOR_COMMON_API CGizmo : public _i_reference_target_t
{
public:
	CGizmo();
	virtual ~CGizmo();

	virtual void        SetName(const char* sName) {};
	virtual const char* GetName() { return ""; };

	//! Set gizmo object flags.
	void SetFlag(EGizmoFlags flag) { m_flags |= flag; }
	void UnsetFlag(EGizmoFlags flag) { m_flags &= ~flag; }
	//! Get gizmo object flags.
	bool GetFlag(EGizmoFlags flag) const { return (m_flags & flag) != 0; }

	/** Get bounding box of Gizmo in world space.
	   @param bbox Returns bounding box.
	 */
	virtual void GetWorldBounds(AABB& bbox) = 0;

	/** Display Gizmo in the viewport.
	 */
	virtual void Display(DisplayContext& dc) = 0;

	/** Performs hit testing on gizmo object.
	 */
	virtual bool HitTest(HitContext& hc) { return false; };

	/** Control interaction of gizmo with the mouse once it is active
	 */
	virtual bool MouseCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int nFlags) = 0;

	//! Is this gizmo need to be deleted?.
	bool IsDelete() const { return GetFlag(EGIZMO_DELETE); }
	//! Set this gizmo to be deleted.
	void DeleteThis() { SetFlag(EGIZMO_DELETE); }

	//! Enables or disables gizmo highlight, also takes care of disabling any other state related to highlights
	virtual void SetHighlighted(bool highlighted);

	//! Returns true when viewport should display snapping grid during interaction
	virtual bool NeedsSnappingGrid() const { return false; }
protected:
	uint32                       m_flags;
};

