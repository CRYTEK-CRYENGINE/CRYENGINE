// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TaskList.h"
#include "Win32GUI.h"
#include <Windows.h>
#include <CommCtrl.h>
#include <Richedit.h>
#include <cstdlib>

TaskList::TaskList()
:	m_edit(0)
{
}

void TaskList::AddTask(const std::string& id, const std::string& description)
{
	int taskIndex = int(m_tasks.size());
	m_tasks.push_back(std::make_pair(id, description));
	m_idTaskMap.insert(std::make_pair(id, taskIndex));
}

void TaskList::SetCurrentTask(const std::string& id)
{
	SetText(id);
}

void TaskList::SetColor()
{
	SendMessage((HWND)m_edit, EM_SETBKGNDCOLOR, 0, GetSysColor(COLOR_3DFACE));
}

void TaskList::SetText(const std::string& highlightedTask)
{
	PARAFORMAT2 paragraphFormat;
	std::memset(&paragraphFormat, 0, sizeof(paragraphFormat));
	paragraphFormat.cbSize = sizeof(paragraphFormat);
	paragraphFormat.dwMask = PFM_LINESPACING | PFM_SPACEBEFORE;
	paragraphFormat.bLineSpacingRule = 5; // Specify spacing in 20ths of a line.
	paragraphFormat.dyLineSpacing = 22;
	paragraphFormat.dySpaceBefore = 70;
	SendMessage((HWND)m_edit, EM_SETPARAFORMAT, 0, (LPARAM)&paragraphFormat);

	CHARFORMAT format;
	std::memset(&format, 0, sizeof(format));
	format.cbSize = sizeof(format);
	format.dwMask = CFM_BOLD;
	format.dwEffects = 0;
	SendMessage((HWND)m_edit, EM_SETCHARFORMAT, 0, (LPARAM)&format);

	SETTEXTEX textEx;
	std::memset(&textEx, 0, sizeof(textEx));
	textEx.flags = ST_DEFAULT;
	textEx.codepage = CP_ACP;
	SendMessage((HWND)m_edit, EM_SETTEXTEX, (WPARAM)&textEx, (LPARAM)_T(""));
	int highlightedLineStart = 0;
	int highlightedLineEnd = 0;

	for (std::vector<std::pair<std::string, std::string> >::const_iterator taskPos = m_tasks.begin(), taskEnd = m_tasks.end(); taskPos != taskEnd; ++taskPos)
	{
		textEx.flags = ST_SELECTION;
		const std::string& id = (*taskPos).first;
		const std::string& description = (*taskPos).second;
		const char* margin = "   ";
		if (highlightedTask == id)
		{
			margin = "* ";
			CHARRANGE range;
			SendMessage((HWND)m_edit, EM_EXGETSEL, 0, (LPARAM)&range);
			highlightedLineStart = range.cpMin;
		}
		SendMessageA((HWND)m_edit, EM_SETTEXTEX, (WPARAM)&textEx, (LPARAM)margin);
		SendMessageA((HWND)m_edit, EM_SETTEXTEX, (WPARAM)&textEx, (LPARAM)description.c_str());
		SendMessageA((HWND)m_edit, EM_SETTEXTEX, (WPARAM)&textEx, (LPARAM)"\n");
		if (highlightedTask == id)
		{
			CHARRANGE range;
			SendMessage((HWND)m_edit, EM_EXGETSEL, 0, (LPARAM)&range);
			highlightedLineEnd = range.cpMin;
		}
	}

	CHARRANGE range = {highlightedLineStart, highlightedLineEnd};
	SendMessage((HWND)m_edit, EM_EXSETSEL, 0, (LPARAM)&range);

	format.dwEffects = CFE_BOLD;
	SendMessage((HWND)m_edit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);

	range.cpMin = 0;
	range.cpMax = 0;
	SendMessage((HWND)m_edit, EM_EXSETSEL, 0, (LPARAM)&range);
}

void TaskList::CreateUI(void* window, int left, int top, int width, int height)
{
	// Create the window.
	LoadLibrary(_T("Riched20.dll"));
	m_edit = CreateWindowEx(
		0,                           //DWORD dwExStyle,
		RICHEDIT_CLASS,              //LPCTSTR lpClassName,
		0,                           //LPCTSTR lpWindowName,
		WS_CHILD | WS_VISIBLE | ES_LEFT | ES_MULTILINE | ES_READONLY,       //DWORD dwStyle,
		left,                        //int x,
		top,                         //int y,
		width,                       //int nWidth,
		height,                      //int nHeight,
		(HWND)window,                //HWND hWndParent,
		0,													 //HMENU hMenu,
		GetModuleHandle(0),          //HINSTANCE hInstance,
		0);                          //LPVOID lpParam);
	HFONT font = Win32GUI::CreateFont();
	SendMessage((HWND)m_edit, WM_SETFONT, (WPARAM)font, 0);
	DeleteObject(font);
	SetColor();

	SetText("");
}

void TaskList::Resize(void* window, int left, int top, int width, int height)
{
	MoveWindow((HWND)m_edit, left, top, width, height, true);
}

void TaskList::DestroyUI(void* window)
{
	DestroyWindow((HWND)m_edit);
	m_edit = 0;
}

void TaskList::GetExtremeDimensions(void* window, int& minWidth, int& maxWidth, int& minHeight, int& maxHeight)
{
	minWidth = 10;
	maxWidth = 2000;
	int height = 25 * int(m_tasks.size());
	minHeight = height;
	maxHeight = height;
}
