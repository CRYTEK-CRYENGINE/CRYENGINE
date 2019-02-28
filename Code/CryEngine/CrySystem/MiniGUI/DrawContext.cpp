// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   DrawContext.cpp
//  Created:     26/08/2009 by Timur.
//  Description: Implementation of the DrawContext class
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>
#include "DrawContext.h"
#include <CrySystem/ISystem.h>
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>

MINIGUI_BEGIN

//////////////////////////////////////////////////////////////////////////
CDrawContext::CDrawContext(SMetrics* pMetrics)
{
	m_currentStackLevel = 0;
	m_x = 0;
	m_y = 0;
	m_pMetrics = pMetrics;
	m_color = ColorB(0, 0, 0, 0);
	m_defaultZ = 0.0f;
	m_pAuxRender = gEnv->pRenderer->GetIRenderAuxGeom();

	m_frameWidth  = (float)gEnv->pRenderer->GetOverlayWidth();
	m_frameHeight = (float)gEnv->pRenderer->GetOverlayHeight();

}

//////////////////////////////////////////////////////////////////////////
void CDrawContext::SetColor(ColorB color)
{
	m_color = color;
}

//////////////////////////////////////////////////////////////////////////
void CDrawContext::DrawLine(float x0, float y0, float x1, float y1, float thickness /*= 1.0f */)
{
	m_pAuxRender->DrawLine(Vec3(m_x + x0, m_y + y0, m_defaultZ), m_color, Vec3(m_x + x1, m_y + y1, m_defaultZ), m_color, thickness);
}

//////////////////////////////////////////////////////////////////////////
void CDrawContext::DrawTriangle(float x0, float y0, float x1, float y1, float x2, float y2)
{
	m_pAuxRender->DrawTriangle(Vec3(m_x + x0, m_y + y0, m_defaultZ), m_color, Vec3(m_x + x1, m_y + y1, m_defaultZ), m_color, Vec3(m_x + x2, m_y + y2, m_defaultZ), m_color);
}

//////////////////////////////////////////////////////////////////////////
void CDrawContext::DrawRect(const Rect& rc)
{
	m_pAuxRender->DrawTriangle(Vec3(m_x + rc.left, m_y + rc.top, m_defaultZ), m_color, Vec3(m_x + rc.left, m_y + rc.bottom, m_defaultZ), m_color, Vec3(m_x + rc.right, m_y + rc.top, m_defaultZ), m_color);
	m_pAuxRender->DrawTriangle(Vec3(m_x + rc.left, m_y + rc.bottom, m_defaultZ), m_color, Vec3(m_x + rc.right, m_y + rc.bottom, m_defaultZ), m_color, Vec3(m_x + rc.right, m_y + rc.top, m_defaultZ), m_color);
}

//////////////////////////////////////////////////////////////////////////
void CDrawContext::DrawFrame(const Rect& rc, ColorB lineColor, ColorB solidColor, float thickness)
{
	ColorB prevColor = m_color;

	SetColor(solidColor);
	DrawRect(rc);

	SetColor(lineColor);

	uint32 curFlags = m_pAuxRender->GetRenderFlags().m_renderFlags;
	m_pAuxRender->SetRenderFlags(curFlags | e_DrawInFrontOn);
	DrawLine(rc.left, rc.top, rc.right, rc.top, thickness);
	DrawLine(rc.right, rc.top, rc.right, rc.bottom, thickness);
	DrawLine(rc.left, rc.top, rc.left, rc.bottom, thickness);
	DrawLine(rc.left, rc.bottom, rc.right, rc.bottom, thickness);
	m_pAuxRender->SetRenderFlags(curFlags);

	m_color = prevColor;
}

//////////////////////////////////////////////////////////////////////////
void CDrawContext::StartDrawing()
{
	int width  = gEnv->pRenderer->GetOverlayWidth();
	int height = gEnv->pRenderer->GetOverlayHeight();
	m_pAuxRender->SetOrthographicProjection(true, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f);

	m_prevRenderFlags = m_pAuxRender->GetRenderFlags().m_renderFlags;
	m_pAuxRender->SetRenderFlags(e_Mode3D | e_AlphaBlended | e_FillModeSolid | e_CullModeBack | e_DepthWriteOff | e_DepthTestOff);
}

//////////////////////////////////////////////////////////////////////////
void CDrawContext::StopDrawing()
{
	// Restore old flags that where set before our draw context.
	m_pAuxRender->SetRenderFlags(m_prevRenderFlags);

	int width  = gEnv->pRenderer->GetOverlayWidth();
	int height = gEnv->pRenderer->GetOverlayHeight();
	gEnv->pRenderer->GetIRenderAuxGeom()->SetOrthographicProjection(false);
}

//////////////////////////////////////////////////////////////////////////
void CDrawContext::DrawString(float x, float y, float font_size, ETextAlign align, const char* format, ...)
{
	//text will be off screen
	if (y > m_frameHeight || x > m_frameWidth)
	{
		return;
	}

	va_list args;
	va_start(args, format);

	int flags = eDrawText_Monospace | eDrawText_2D | eDrawText_FixedSize | eDrawText_IgnoreOverscan;
	if (align == eTextAlign_Left)
	{
	}
	else if (align == eTextAlign_Right)
	{
		flags |= eDrawText_Right;
	}
	else if (align == eTextAlign_Center)
	{
		flags |= eDrawText_Center;
	}

	IRenderAuxText::DrawText(Vec3(m_x + x, m_y + y, m_defaultZ), font_size / 12.0f, m_color, flags, format, args);
	va_end(args);
}

//////////////////////////////////////////////////////////////////////////
void CDrawContext::PushClientRect(const Rect& rc)
{
	m_currentStackLevel++;
	assert(m_currentStackLevel < MAX_ORIGIN_STACK);
	m_clientRectStack[m_currentStackLevel] = rc;
	m_x += rc.left;
	m_y += rc.top;
}

//////////////////////////////////////////////////////////////////////////
void CDrawContext::PopClientRect()
{
	if (m_currentStackLevel > 0)
	{
		Rect& rc = m_clientRectStack[m_currentStackLevel];
		m_x -= rc.left;
		m_y -= rc.top;

		m_currentStackLevel--;
	}
}
MINIGUI_END
