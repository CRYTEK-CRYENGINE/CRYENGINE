// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PushButton.h"
#include "Win32GUI.h"

PushButton::~PushButton()
{
	m_callback->Release();
}

void PushButton::Enable(bool enabled)
{
	m_enabled = enabled;
	EnableWindow((HWND)m_button, m_enabled);
}

void PushButton::CreateUI(void* window, int left, int top, int width, int height)
{
	m_button = Win32GUI::CreateControl(_T("BUTTON"), WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, (HWND)window, left, top, 40, 20);
	m_font = Win32GUI::CreateFont();
	SendMessage((HWND)m_button, WM_SETFONT, (WPARAM)m_font, 0);
	SendMessage((HWND)m_button, WM_SETTEXT, 0, (LPARAM)m_text.c_str());
	EnableWindow((HWND)m_button, m_enabled);

	Win32GUI::SetCallback<Win32GUI::EventCallbacks::Pushed, PushButton>((HWND)m_button, this, &PushButton::OnPushed);
}

void PushButton::Resize(void* window, int left, int top, int width, int height)
{
	MoveWindow((HWND)m_button, left, top, width, height, true);
}

void PushButton::DestroyUI(void* window)
{
	DestroyWindow((HWND)m_button);
	m_button = 0;
	DeleteObject(m_font);
	m_font = 0;
}

void PushButton::GetExtremeDimensions(void* window, int& minWidth, int& maxWidth, int& minHeight, int& maxHeight)
{
	minWidth = 50;
	maxWidth = 50;
	minHeight = 20;
	maxHeight = 20;
}

void PushButton::OnPushed()
{
	m_callback->Call();
}
