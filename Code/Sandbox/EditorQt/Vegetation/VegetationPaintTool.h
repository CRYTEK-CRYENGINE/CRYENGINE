// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditTool.h"

#include <CrySandbox/CrySignal.h>
#include "Qt/Widgets/QEditToolButton.h"

class CVegetationObject;
struct CVegetationInstance;
class CVegetationMap;

class CVegetationPaintTool : public CEditTool
{
	DECLARE_DYNCREATE(CVegetationPaintTool)
public:
	CVegetationPaintTool();

	virtual string GetDisplayName() const override { return "Paint Vegetation"; }
	virtual void   Display(DisplayContext& dc);

	// Overides from CEditTool
	virtual bool MouseCallback(CViewport* pView, EMouseEvent event, CPoint& point, int flags) override;

	// Key down.
	virtual bool                             OnKeyDown(CViewport* pView, uint32 key, uint32 repCnt, uint32 flags) override;
	virtual bool                             OnKeyUp(CViewport* pView, uint32 key, uint32 repCnt, uint32 flags) override;

	void                                     SetBrushRadius(float r);
	float                                    GetBrushRadius() const { return m_brushRadius; };

	void                                     PaintBrush();
	void                                     PlaceThing(CViewport* pView, const CPoint& point);

	static QEditToolButtonPanel::SButtonInfo CreatePaintToolButtonInfo();
	static float                             GetMinBrushRadius();
	static float                             GetMaxBrushRadius();

	static void                              Command_Activate(bool activateEraseMode);

	CCrySignal<void()> signalBrushRadiusChanged;

protected:
	virtual ~CVegetationPaintTool() {}
	// Delete itself.
	virtual void DeleteThis() override { delete this; }

protected:
	bool m_eraseMode;
	bool m_isEraseTool;
	bool m_isModeSwitchAllowed;

private:
	void SetModified(AABB& bounds, bool notifySW = true);

	// Specific mouse events handlers.
	void OnLButtonDown(CViewport* pView, int flags, const CPoint& point);
	void OnLButtonUp();
	void OnMouseMove(CViewport* pView, int flags, const CPoint& point);
	void SetBrushRadiusWithSignal(float radius);

private:
	Vec3               m_pointerPos;
	CPoint             m_prevMousePos;
	CVegetationMap*    m_vegetationMap;
	bool               m_isAffectedByBrushes;
	float              m_brushRadius;
	static const float s_minBrushRadius;
	static const float s_maxBrushRadius;
};

