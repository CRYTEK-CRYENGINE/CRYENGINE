// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   MiniInfoBox.cpp
//  Created:     26/08/2009 by Timur.
//  Description: Button implementation in the MiniGUI
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>
#include "MiniInfoBox.h"
#include "DrawContext.h"
#include <CrySystem/ISystem.h>

MINIGUI_BEGIN

CMiniInfoBox::CMiniInfoBox() :
	m_fTextIndent(4)
{
}

//////////////////////////////////////////////////////////////////////////
void CMiniInfoBox::SaveState()
{
	if (CheckFlag(eCtrl_Hidden))
	{
		m_saveStateOn = false;
	}
	else
	{
		m_saveStateOn = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniInfoBox::Reset()
{
	SetFlag(eCtrl_Hidden);
}

//////////////////////////////////////////////////////////////////////////
void CMiniInfoBox::RestoreState()
{
	if (m_saveStateOn)
	{
		ClearFlag(eCtrl_Hidden);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniInfoBox::OnPaint(CDrawContext& dc)
{
	if (m_requiresResize)
	{
		AutoResize();
	}

	ColorB borderCol = dc.Metrics().clrFrameBorder;
	ColorB backgroundCol = dc.Metrics().clrBackground;

	if (!m_pGUI->InFocus())
	{
		borderCol = dc.Metrics().clrFrameBorderOutOfFocus;
		backgroundCol.a = dc.Metrics().outOfFocusAlpha;
	}
	else if (m_moving)
	{
		borderCol = dc.Metrics().clrFrameBorderHighlight;
	}

	dc.DrawFrame(m_rect, borderCol, backgroundCol);

	dc.SetColor(dc.Metrics().clrTitle);
	dc.DrawString(m_rect.left + 4, m_rect.top, dc.Metrics().fTitleSize, eTextAlign_Left, GetTitle());

	float fTextSize = m_fTextSize;
	if (fTextSize == 0)
		fTextSize = dc.Metrics().fTextSize;

	float x = m_fTextIndent + m_rect.left + 8;
	float y = m_rect.top + fTextSize + fTextSize;
	// Draw entries.
	for (int i = 0, num = (int)m_entries.size(); i < num; i++)
	{
		SInfoEntry& info = m_entries[i];
		dc.SetColor(info.color);
		dc.DrawString(x, y, info.textSize, eTextAlign_Left, info.text);
		y += info.textSize * 0.8f;
		if (y + info.textSize > m_rect.bottom)
			break;
	}

	if (m_renderCallback)
	{
		m_renderCallback(m_rect.left, m_rect.top);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniInfoBox::OnEvent(float x, float y, EMiniCtrlEvent event)
{
	CMiniCtrl::OnEvent(x, y, event);
}

void CMiniInfoBox::AddEntry(const char* str, ColorB col, float textSize)
{
	SInfoEntry info;

	info.color = col;
	info.textSize = textSize;
	cry_strcpy(info.text, str);

	m_entries.push_back(info);

	if (CheckFlag(eCtrl_AutoResize))
	{
		//set dirty flag instead of resizing for every elem
		m_requiresResize = true; //AutoResize();
	}
}

void CMiniInfoBox::ClearEntries()
{
	m_entries.clear();
	m_requiresResize = true;
}

//////////////////////////////////////////////////////////////////////////
void CMiniInfoBox::AutoResize()
{
	float height = 32.f;

	//must be at least the size of title and cross box
	float width = m_fTextIndent + ((float)m_title.size() * 14.f) + 30.f;

	for (int i = 0, num = (int)m_entries.size(); i < num; i++)
	{
		SInfoEntry& info = m_entries[i];

		uint32 strLength = strlen(info.text);

		float strSize = info.textSize * strLength;

		width = max(strSize, width);
		height += info.textSize * 0.8f;
	}

	//scale width, could do with kerning info
	width *= 0.6f;

	Rect newRect = m_rect;
	newRect.right = newRect.left + width;
	newRect.bottom = newRect.top + height;

	SetRect(newRect);

	m_requiresResize = false;
}

MINIGUI_END
