// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __LinkTool_h__
#define __LinkTool_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "EditTool.h"

class CGeomCacheEntity;

//////////////////////////////////////////////////////////////////////////
class CLinkTool : public CEditTool
{
public:
	DECLARE_DYNAMIC(CLinkTool)

	CLinkTool(); // IPickObjectCallback *callback,CRuntimeClass *targetClass=NULL );

	// Overrides from CEditTool
	virtual string GetDisplayName() const override { return "Link Objects"; }
	bool           MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags);

	virtual void   Display(DisplayContext& dc);
	virtual bool   OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags);
	virtual bool   OnKeyUp(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags) { return false; };

	virtual void   DrawObjectHelpers(CBaseObject* pObject, DisplayContext& dc) override;
	virtual bool   HitTest(CBaseObject* pObject, HitContext& hc) override;

	static void    PickObject();

protected:
	virtual ~CLinkTool();
	// Delete itself.
	void DeleteThis() { delete this; };

private:
	CBaseObject*          m_pChild;
	Vec3                  m_StartDrag;
	Vec3                  m_EndDrag;

	HCURSOR               m_hLinkCursor;
	HCURSOR               m_hLinkNowCursor;
	HCURSOR               m_hCurrCursor;

	const char*           m_nodeName;
	uint                  m_hitNodeIndex;
	IGeomCacheRenderNode* m_pGeomCacheRenderNode;
};

#endif // __LinkTool_h__

