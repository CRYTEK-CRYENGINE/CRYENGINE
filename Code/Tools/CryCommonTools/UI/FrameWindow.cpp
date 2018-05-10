// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FrameWindow.h"
#include "Win32GUI.h"
#include "IUIComponent.h"
#include <Windows.h>
#include <cassert>

FrameWindow::FrameWindow()
:	m_hwnd(0),
	m_layout(Layout::DirectionVertical)
{
}

FrameWindow::~FrameWindow()
{
	if (m_hwnd)
		Show(false, 0, 0);
}

void FrameWindow::AddComponent(IUIComponent* component)
{
	assert(m_hwnd	== 0);
	m_layout.AddComponent(component);
}

void FrameWindow::Show(bool show, int width, int height)
{
	if (show)
	{
		assert(m_hwnd == 0);
		TCHAR* className = _T("CustomFrameWindowClass212");
		Win32GUI::RegisterFrameClass(className);
		m_hwnd = Win32GUI::CreateFrame(className, WS_MINIMIZEBOX | WS_OVERLAPPED | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX, width, height);
		Win32GUI::SetCallback<Win32GUI::EventCallbacks::GetDimensions, FrameWindow>((HWND)m_hwnd, this, &FrameWindow::CalculateExtremeDimensions);
		Win32GUI::SetCallback<Win32GUI::EventCallbacks::SizeChanged, FrameWindow>((HWND)m_hwnd, this, &FrameWindow::OnSizeChanged);
		std::pair<int, int> size = InitializeSize();
		m_layout.CreateUI(m_hwnd, 0, 0, size.first, size.second);
		ShowWindow((HWND)m_hwnd, SW_SHOWDEFAULT);
	}
	else
	{
		m_layout.DestroyUI(m_hwnd);
		assert(m_hwnd != 0);
		DestroyWindow((HWND)m_hwnd);
		m_hwnd = 0;
	}
}

void FrameWindow::SetCaption(const TCHAR* caption)
{
	SendMessage((HWND)m_hwnd, WM_SETTEXT, 0, (LPARAM)caption);
}

void* FrameWindow::GetHWND()
{
	return m_hwnd;
}

std::pair<int, int> FrameWindow::InitializeSize()
{
	int minW, maxW, minH, maxH;
	CalculateExtremeDimensions(minW, maxW, minH, maxH);
	RECT rect;
	GetWindowRect((HWND)m_hwnd, &rect);
	int width = int((std::min)(maxW, (std::max)(minW, int(rect.right - rect.left))));
	int height = int((std::min)(maxH, (std::max)(minH, int(rect.bottom - rect.top))));
	MoveWindow((HWND)m_hwnd, rect.left, rect.top, width, height, false);
	return std::make_pair(width, height);
}

void FrameWindow::CalculateExtremeDimensions(int& minWidth, int& maxWidth, int& minHeight, int& maxHeight)
{
	int minW = 0;
	int maxW = 0;
	int minH = 0;
	int maxH = 0;
	m_layout.GetExtremeDimensions(m_hwnd, minW, maxW, minH, maxH);

	// Add the space required for the window decorations.
	RECT rect;
	rect.left = 0, rect.top = 0, rect.right = minW, rect.bottom = minH;
	unsigned style = GetWindowLong((HWND)m_hwnd, GWL_STYLE);
	AdjustWindowRect(&rect, style, false);
	minW = rect.right - rect.left;
	minH = rect.bottom - rect.top;

	rect.left = 0, rect.top = 0, rect.right = maxW, rect.bottom = maxH;
	AdjustWindowRect(&rect, style, false);
	maxW = rect.right - rect.left;
	maxH = rect.bottom - rect.top;

	minWidth = minW;
	maxWidth = maxW;
	minHeight = minH;
	maxHeight = maxH;
}

void FrameWindow::OnSizeChanged(int width, int height)
{
	m_layout.Resize(m_hwnd, 0, 0, width, height);
}
