// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   MiniTable.cpp
//  Created:     01/12/2009 by AndyM.
//  Description: Table implementation in the MiniGUI
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef _MINITABLE_H_
#define _MINITABLE_H_

#include "MiniGUI.h"

MINIGUI_BEGIN

class CMiniTable : public CMiniCtrl, public IMiniTable, public IInputEventListener
{
public:
	CMiniTable();

	//////////////////////////////////////////////////////////////////////////
	// CMiniCtrl interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EMiniCtrlType GetType() const { return eCtrlType_Table; }
	virtual void          OnPaint(CDrawContext& dc);
	virtual void          OnEvent(float x, float y, EMiniCtrlEvent event);
	virtual void          Reset();
	virtual void          SaveState();
	virtual void          RestoreState();
	virtual void          AutoResize();
	virtual void          SetVisible(bool state);

	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IMiniTable interface implementation.
	//////////////////////////////////////////////////////////////////////////

	//Add a new column to the table, return column index
	virtual int AddColumn(const char* name);

	//Remove all columns an associated data
	virtual void RemoveColumns();

	//Add data to specified column (add onto the end), return row index
	virtual int AddData(int columnIndex, ColorB col, const char* format, ...);

	//Clear all data from table
	virtual void ClearTable();

	virtual bool IsHidden() { return CheckFlag(eCtrl_Hidden); }

	virtual void Hide(bool stat);

	// IInputEventListener
	virtual bool OnInputEvent(const SInputEvent& rInputEvent);

	static const int MAX_TEXT_LENGTH = 64;

protected:

	struct SCell
	{
		char   text[MAX_TEXT_LENGTH];
		ColorB col;
	};

	struct SColumn
	{
		char               name[MAX_TEXT_LENGTH];
		float              width;
		std::vector<SCell> cells;
	};

	std::vector<SColumn> m_columns;
	int                  m_pageSize;
	int                  m_pageNum;
};

MINIGUI_END

#endif// _MINITABLE_H_
