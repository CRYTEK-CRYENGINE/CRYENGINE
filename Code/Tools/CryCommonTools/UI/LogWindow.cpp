// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LogWindow.h"

LogWindow::LogWindow()
	: m_mainLayout(Layout::DirectionVertical)
	, m_toolbarLayout(Layout::DirectionHorizontal)
	, m_filterFlags(0)
{
	m_buttons.push_back(new ToggleButton(_T("Debug"), this, &LogWindow::DebugToggled));
	m_buttons.push_back(new ToggleButton(_T("Info"), this, &LogWindow::InfoToggled));
	m_buttons.push_back(new ToggleButton(_T("Warnings"), this, &LogWindow::WarningsToggled));
	m_buttons.push_back(new ToggleButton(_T("Errors"), this, &LogWindow::ErrorsToggled));

	SetFilter(ILogger::eSeverity_Error, true);
	SetFilter(ILogger::eSeverity_Warning, true);
	SetFilter(ILogger::eSeverity_Info, true);
	SetFilter(ILogger::eSeverity_Debug, false);

	m_toolbarLayout.AddComponent(m_buttons[3]);
	m_toolbarLayout.AddComponent(m_buttons[2]);
	m_toolbarLayout.AddComponent(m_buttons[1]);
	m_toolbarLayout.AddComponent(m_buttons[0]);

	m_mainLayout.AddComponent(&m_toolbarLayout);
	m_mainLayout.AddComponent(&m_list);
}

LogWindow::~LogWindow()
{
	for (std::vector<ToggleButton*>::iterator button = m_buttons.begin(), end = m_buttons.end(); button != end; ++button)
		delete *button;
}

void LogWindow::Log(ILogger::ESeverity eSeverity, const TCHAR* message)
{
	m_messages.push_back(LogMessage(eSeverity, message));

	if (m_filterFlags & (1 << GetSeverityIndex(eSeverity)))
	{
		m_list.Add(GetImageIndex(eSeverity), message);
	}
}

void LogWindow::SetFilter(ILogger::ESeverity eSeverity, bool visible)
{
	int index = GetSeverityIndex(eSeverity);
	if (visible)
	{
		m_filterFlags |= (1 << index);
	}
	else
	{
		m_filterFlags &= ~(1 << index);
	}
	m_buttons[index]->SetState(visible);
	RefillList();
}

void LogWindow::CreateUI(void* window, int left, int top, int width, int height)
{
	m_mainLayout.CreateUI(window, left, top, width, height);
}

void LogWindow::Resize(void* window, int left, int top, int width, int height)
{
	m_mainLayout.Resize(window, left, top, width, height);
}

void LogWindow::DestroyUI(void* window)
{
	m_mainLayout.DestroyUI(window);
}

void LogWindow::GetExtremeDimensions(void* window, int& minWidth, int& maxWidth, int& minHeight, int& maxHeight)
{
	m_mainLayout.GetExtremeDimensions(window, minWidth, maxWidth, minHeight, maxHeight);
}

void LogWindow::ErrorsToggled(bool value)
{
	SetFilter(ILogger::eSeverity_Error, value);
}

void LogWindow::WarningsToggled(bool value)
{
	SetFilter(ILogger::eSeverity_Warning, value);
}

void LogWindow::InfoToggled(bool value)
{
	SetFilter(ILogger::eSeverity_Info, value);
}

void LogWindow::DebugToggled(bool value)
{
	SetFilter(ILogger::eSeverity_Debug, value);
}

void LogWindow::RefillList()
{
	m_list.Clear();
	for (int messageIndex = 0, messageCount = int(m_messages.size()); messageIndex < messageCount; ++messageIndex)
	{
		if (m_filterFlags & (1 << m_messages[messageIndex].severity))
			m_list.Add(GetImageIndex(m_messages[messageIndex].severity), m_messages[messageIndex].message.c_str());
	}
}

int LogWindow::GetImageIndex(ILogger::ESeverity eSeverity)
{
	switch (eSeverity)
	{
	case ILogger::eSeverity_Debug:   return 2;
	case ILogger::eSeverity_Info:    return -1;
	case ILogger::eSeverity_Warning: return 1;
	case ILogger::eSeverity_Error:   return 0;
	default:                         return 0;
	}
}

int LogWindow::GetSeverityIndex(ILogger::ESeverity eSeverity)
{
	switch (eSeverity)
	{
	case ILogger::eSeverity_Debug:   return 0;
	case ILogger::eSeverity_Info:    return 1;
	case ILogger::eSeverity_Warning: return 2;
	case ILogger::eSeverity_Error:   return 3;
	default:                         return 3;
	}
}
