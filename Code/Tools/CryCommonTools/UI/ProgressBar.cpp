// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProgressBar.h"
#include <Windows.h>
#include <CommCtrl.h>

ProgressBar::ProgressBar()
: m_progressBar(0)
{
}

void ProgressBar::CreateUI(void* window, int left, int top, int width, int height)
{
	m_progressBar = CreateWindowEx(
		0,
		PROGRESS_CLASS,
		0,
		WS_CHILD | WS_VISIBLE,
		left,
		top,
		width,
		height,
		(HWND)window,
		(HMENU)0,
		GetModuleHandle(0),
		0);
  SendMessage((HWND)m_progressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 1000));
  SendMessage((HWND)m_progressBar, PBM_SETSTEP, (WPARAM) 1, 0);
}

void ProgressBar::Resize(void* window, int left, int top, int width, int height)
{
	MoveWindow((HWND)m_progressBar, left, top, width, height, true);
}

void ProgressBar::DestroyUI(void* window)
{
	DestroyWindow((HWND)m_progressBar);
}

void ProgressBar::GetExtremeDimensions(void* window, int& minWidth, int& maxWidth, int& minHeight, int& maxHeight)
{
	minWidth = 200;
	maxWidth = 2000;
	minHeight = 30;
	maxHeight = 30;
}

void ProgressBar::SetProgress(float progress)
{
	int newPos = int(progress * 1000.0f);
	SendMessage((HWND)m_progressBar, PBM_SETPOS, newPos, 0);
}
