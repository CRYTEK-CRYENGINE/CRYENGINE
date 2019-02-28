// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EditControl.h"
#include "Win32GUI.h"
#include <Windows.h>
#include <CommCtrl.h>
#include <Richedit.h>

EditControl::EditControl()
: m_edit(0)
{
}

void EditControl::CreateUI(void* window, int left, int top, int width, int height)
{
	m_edit = Win32GUI::CreateControl(RICHEDIT_CLASS, ES_MULTILINE /*| ES_READONLY*/, (HWND)window, left, top, width, height);
}

void EditControl::Resize(void* window, int left, int top, int width, int height)
{
	MoveWindow((HWND)m_edit, left, top, width, height, true);
}

void EditControl::DestroyUI(void* window)
{
	DestroyWindow((HWND)m_edit);
	m_edit = 0;
}

void EditControl::GetExtremeDimensions(void* window, int& minWidth, int& maxWidth, int& minHeight, int& maxHeight)
{
	minWidth = 20;
	maxWidth = 2000;
	minHeight = 20;
	maxHeight = 2000;
}
