// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditTool.h"

class QMimeData;

class CBaseObjectCreateTool : public CEditTool
{
	DECLARE_DYNAMIC(CBaseObjectCreateTool)

public:
	CBaseObjectCreateTool();
	virtual ~CBaseObjectCreateTool();

public:
	// Overides from CEditTool
	virtual bool MouseCallback(CViewport* pView, EMouseEvent eventId, CPoint& point, int flags) override;
	virtual bool OnDragEvent(CViewport* pView, EDragEvent eventId, QEvent* pEvent, uint32 flags) override;
	virtual bool OnKeyDown(CViewport* pView, uint32 key, uint32 repCnt, uint32 flags) override;

private:
	virtual int  ObjectMouseCallback(CViewport* pView, EMouseEvent eventId, const CPoint& point, uint32 flags) = 0;

	virtual bool CanStartCreation() = 0;
	// Has view and click point parameter to calculate a start location for the newly created object
	virtual void StartCreation(CViewport* pView = nullptr, const CPoint& point = CPoint()) = 0;
	virtual void FinishObjectCreation() = 0;
	virtual void CancelCreation(bool clear = true) = 0;
	virtual bool IsValidDragData(const QMimeData* pData, bool acceptValue) = 0;
	virtual bool ObjectWasCreated() const = 0;

	virtual void FinishCreation(bool restart, CViewport* pView = nullptr, const CPoint& point = CPoint());
};

