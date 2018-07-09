// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   DrawContext.h
//  Created:     26/08/2009 by Timur.
//  Description: DrawContext helper class for MiniGUI
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __MiniGUI_DrawContext_h__
#define __MiniGUI_DrawContext_h__

#include <CrySystem/ICryMiniGUI.h>
#include <CryMath/Cry_Color.h>

struct IRenderAuxGeom;

MINIGUI_BEGIN

enum ETextAlign
{
	eTextAlign_Left,
	eTextAlign_Right,
	eTextAlign_Center
};

//////////////////////////////////////////////////////////////////////////
// Context of MiniGUI drawing.
//////////////////////////////////////////////////////////////////////////
class CDrawContext
{
public:
	CDrawContext(SMetrics* pMetrics);

	// Must be called before any drawing happens
	void      StartDrawing();
	// Must be called after all drawing have been complete.
	void      StopDrawing();

	void      PushClientRect(const Rect& rc);
	void      PopClientRect();

	SMetrics& Metrics() { return *m_pMetrics; }
	void      SetColor(ColorB color);

	void      DrawLine(float x0, float y0, float x1, float y1, float thickness = 1.0f);
	void      DrawTriangle(float x0, float y0, float x1, float y1, float x2, float y2);
	void      DrawRect(const Rect& rc);
	void      DrawFrame(const Rect& rc, ColorB lineColor, ColorB solidColor, float thickness = 1.0f);

	void      DrawString(float x, float y, float font_size, ETextAlign align, const char* format, ...);

protected:
	SMetrics*       m_pMetrics;

	ColorB          m_color;
	float           m_defaultZ;
	IRenderAuxGeom* m_pAuxRender;
	uint32          m_prevRenderFlags;

	enum { MAX_ORIGIN_STACK = 16 };

	int   m_currentStackLevel;
	float m_x, m_y; // Reference X,Y positions
	Rect  m_clientRectStack[MAX_ORIGIN_STACK];

	float m_frameWidth;
	float m_frameHeight;
};

MINIGUI_END

#endif //__MiniGUI_DrawContext_h__
