// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   MiniTable.cpp
//  Created:     01/12/2009 by AndyM.
//  Description: Table implementation in the MiniGUI
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>
#include "MiniTable.h"
#include "DrawContext.h"

MINIGUI_BEGIN

CMiniTable::CMiniTable()
{
	m_fTextSize = 15.f;
	m_pageNum = 0;
	m_pageSize = 35;
}

void CMiniTable::OnPaint(CDrawContext& dc)
{
	if (m_requiresResize)
	{
		AutoResize();
	}

	ColorB borderCol;
	ColorB backgroundCol = dc.Metrics().clrBackground;

	if (m_pGUI->InFocus())
	{
		if (m_moving)
		{
			borderCol = dc.Metrics().clrFrameBorderHighlight;
		}
		else
		{
			borderCol = dc.Metrics().clrFrameBorder;
		}
	}
	else
	{
		borderCol = dc.Metrics().clrFrameBorderOutOfFocus;
		backgroundCol.a = dc.Metrics().outOfFocusAlpha;
	}

	dc.DrawFrame(m_rect, borderCol, backgroundCol);

	float fTextSize = m_fTextSize;
	if (fTextSize == 0)
		fTextSize = dc.Metrics().fTextSize;

	float indent = 4.f;

	float x = m_rect.left + indent;
	float y = m_rect.top + indent;

	const int nColumns = m_columns.size();
	int numEntries = m_columns[0].cells.size();

	int startIdx = 0;
	int endIdx = numEntries;

	//Page number
	if (nColumns)
	{
		int numPages = numEntries / m_pageSize;

		if (numPages)
		{
			startIdx = m_pageNum * m_pageSize;
			endIdx = min(startIdx + m_pageSize, numEntries);
			dc.SetColor(ColorB(255, 255, 255, 255));

			//print page details (adjust for zero indexed)
			dc.DrawString(x, y, fTextSize, eTextAlign_Left, "Page %d of %d (Page Up / Page Down)", m_pageNum + 1, numPages + 1);
			y += fTextSize;
		}
	}

	float topOfTable = y;

	for (int i = 0; i < nColumns; i++)
	{
		SColumn& column = m_columns[i];

		y = topOfTable;

		dc.SetColor(ColorB(32, 32, 255, 255));

		dc.DrawString(x, y, fTextSize, eTextAlign_Left, column.name);
		y += fTextSize + indent;

		ColorB currentCol(255, 255, 255, 255);

		dc.SetColor(currentCol);

		const int nCells = column.cells.size();

		for (int j = startIdx; j < endIdx && j < nCells; j++)
		{
			if (column.cells[j].col != currentCol)
			{
				dc.SetColor(column.cells[j].col);
				currentCol = column.cells[j].col;
			}

			dc.DrawString(x, y, fTextSize, eTextAlign_Left, column.cells[j].text);
			y += fTextSize;
		}

		x += column.width;
	}
}

void CMiniTable::OnEvent(float x, float y, EMiniCtrlEvent event)
{
	//movement
	CMiniCtrl::OnEvent(x, y, event);
}

void CMiniTable::AutoResize()
{
	//must be at least the size of cross box
	float tableWidth = 30.f;

	float tableHeight = 0.f;
	const float widthScale = 0.6f;
	const int nColumns = m_columns.size();

	int startIdx = 0;
	int endIdx = 0;

	bool bPageHeader = false;

	//Update Page index
	if (nColumns)
	{
		int numEntries = m_columns[0].cells.size();
		int numPages = numEntries / m_pageSize;

		//page index is now invalid, cap at max
		if ((m_pageNum * m_pageSize) > numEntries)
		{
			m_pageNum = (numEntries / m_pageSize);
		}

		startIdx = m_pageNum * m_pageSize;
		endIdx = min(startIdx + m_pageSize, numEntries);

		if (numEntries > m_pageSize)
		{
			bPageHeader = true;
		}
	}

	for (int i = 0; i < nColumns; i++)
	{
		SColumn& column = m_columns[i];

		float width = strlen(column.name) * m_fTextSize;

		int nCells = column.cells.size();

		for (int j = startIdx; j < endIdx && j < nCells; j++)
		{
			width = max(width, strlen(column.cells[j].text) * m_fTextSize);
		}

		width *= widthScale;

		column.width = width;
		tableWidth += width;
		tableHeight = max(tableHeight, min(endIdx - startIdx, m_pageSize) * m_fTextSize);
	}

	tableHeight += m_fTextSize * 2.f;

	//Page index
	if (bPageHeader)
	{
		tableHeight += m_fTextSize;
	}

	Rect newRect = m_rect;

	newRect.right = newRect.left + tableWidth;
	newRect.bottom = newRect.top + tableHeight;

	SetRect(newRect);

	m_requiresResize = false;
}

void CMiniTable::Reset()
{
	SetFlag(eCtrl_Hidden);

	m_pageNum = 0;
}

void CMiniTable::SaveState()
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

void CMiniTable::RestoreState()
{
	if (m_saveStateOn)
	{
		ClearFlag(eCtrl_Hidden);
	}
}

int CMiniTable::AddColumn(const char* name)
{
	assert(name);

	SColumn col;

	cry_strcpy(col.name, name);
	col.width = strlen(col.name) * 8.f;

	m_columns.push_back(col);

	m_requiresResize = true;

	return m_columns.size() - 1;
}

void CMiniTable::RemoveColumns()
{
	m_columns.clear();
}

int CMiniTable::AddData(int columnIndex, ColorB col, const char* format, ...)
{
	assert(columnIndex < (int)m_columns.size());

	SCell cell;

	va_list args;
	va_start(args, format);
	cry_vsprintf(cell.text, format, args);
	va_end(args);

	cell.col = col;

	m_columns[columnIndex].cells.push_back(cell);

	m_requiresResize = true;

	return m_columns[columnIndex].cells.size() - 1;

}

void CMiniTable::ClearTable()
{
	const int nColumns = m_columns.size();

	for (int i = 0; i < nColumns; i++)
	{
		m_columns[i].cells.clear();
	}
}

void CMiniTable::SetVisible(bool state)
{
	if (state)
	{
		clear_flag(eCtrl_Hidden);

		if (gEnv->pInput)
		{
			gEnv->pInput->AddEventListener(this);
		}
	}
	else
	{
		set_flag(eCtrl_Hidden);

		if (gEnv->pInput)
		{
			gEnv->pInput->RemoveEventListener(this);
		}
	}

	if (m_pCloseButton)
	{
		m_pCloseButton->SetVisible(state);
	}
}

void CMiniTable::Hide(bool stat)
{
	SetVisible(!stat);
}

bool CMiniTable::OnInputEvent(const SInputEvent& rInputEvent)
{
	if (!IsHidden())
	{
		if (rInputEvent.state == eIS_Pressed && rInputEvent.keyId == eKI_PgUp)
		{
			m_pageNum++;
			m_requiresResize = true;
		}
		else if (rInputEvent.state == eIS_Pressed && rInputEvent.keyId == eKI_PgDn)
		{
			if (m_pageNum > 0)
			{
				m_pageNum--;
				m_requiresResize = true;
			}
		}
	}
	return false;
}
MINIGUI_END
