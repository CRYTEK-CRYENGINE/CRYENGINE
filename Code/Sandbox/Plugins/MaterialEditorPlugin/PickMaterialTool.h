// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditTool.h"

struct IMaterial;
class CMaterialEditor;

class CPickMaterialTool : public CEditTool
{
public:
	DECLARE_DYNCREATE(CPickMaterialTool)

	CPickMaterialTool();

	//! Use this to attach the edit tool to an editor. This means the asset picked will be opened in the current editor
	void SetActiveEditor(CMaterialEditor* pActiveEditor) { m_pMaterialEditor = pActiveEditor; }

	//////////////////////////////////////////////////////////////////////////
	// CEditTool implementation
	//////////////////////////////////////////////////////////////////////////
	virtual string GetDisplayName() const override { return "Pick Material"; }
	virtual bool   MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags);
	virtual void   Display(DisplayContext& dc);
	//////////////////////////////////////////////////////////////////////////

protected:

	bool OnMouseMove(CViewport* view, UINT nFlags, CPoint point);
	void OnHover(IMaterial* pMaterial);
	void OnPicked(IMaterial* pEditorMaterial);

	virtual ~CPickMaterialTool();
	// Delete itself.
	void DeleteThis() { delete this; };

	_smart_ptr<IMaterial>	m_pHoveredMaterial;
	string					m_displayString;
	SRayHitInfo				m_HitInfo;
	CMaterialEditor*		m_pMaterialEditor;
};


