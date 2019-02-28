// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IHyperNodePainter.h"

class CHyperNodePainter_Default : public IHyperNodePainter
{
public:
	virtual void Paint(CHyperNode* pNode, CDisplayList* pList);
private:
	void         AddDownArrow(CDisplayList* pList, const Gdiplus::PointF& p, Gdiplus::Pen* pPen);
	void         CheckBreakpoint(IFlowGraphPtr pFlowGraph, const SFlowAddress& addr, bool& bIsBreakPoint, bool& bIsTracepoint);
};

static const float BREAKPOINT_X_OFFSET = 10.0f;
static const float MINIMIZE_BOX_MAX_HEIGHT = 12.0f;
static const float MINIMIZE_BOX_WIDTH = 16.0f;
static const int PORTS_OUTER_MARGIN = 12;

