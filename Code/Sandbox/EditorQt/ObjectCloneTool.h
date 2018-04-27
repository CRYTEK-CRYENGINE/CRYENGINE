// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ObjectCloneTool_h__
#define __ObjectCloneTool_h__

#if _MSC_VER > 1000
	#pragma once
#endif

class CBaseObject;

/*!
 *	CObjectCloneTool, When created duplicate current selection, and manages cloned selection.
 *
 */

class CObjectCloneTool : public CEditTool
{
public:
	DECLARE_DYNCREATE(CObjectCloneTool)

	CObjectCloneTool();

	//////////////////////////////////////////////////////////////////////////
	// Overrides from CEditTool
	virtual string GetDisplayName() const override { return "Clone Object"; }
	bool           MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags);

	virtual void   Display(DisplayContext& dc);
	virtual bool   OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags);
	virtual bool   OnKeyUp(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags) { return false; };
	//////////////////////////////////////////////////////////////////////////

	void Accept();
	void Abort();

	void OnSelectionChanged();

protected:
	virtual ~CObjectCloneTool();
	// Delete itself.
	void DeleteThis() { delete this; };

private:
	void CloneSelection();
	void SetConstrPlane(CViewport* view, CPoint point);

	const CSelectionGroup* m_selection;
	Vec3             m_origin;
	bool             m_bSetConstrPlane;
	Vec3             m_initPosition;
	//bool m_bSetCapture;
};

#endif // __ObjectCloneTool_h__

