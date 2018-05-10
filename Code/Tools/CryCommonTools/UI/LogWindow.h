// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __LOGWINDOW_H__
#define __LOGWINDOW_H__

#include "IUIComponent.h"
#include "Layout.h"
#include "ListView.h"
#include "ToggleButton.h"
#include <list>
#include "ILogger.h"

class LogWindow : public IUIComponent
{
public:
	LogWindow();
	~LogWindow();

	void Log(ILogger::ESeverity eSeverity, const TCHAR* message);
	void SetFilter(ILogger::ESeverity eSeverity, bool visible);

	// IUIComponent
	virtual void CreateUI(void* window, int left, int top, int width, int height);
	virtual void Resize(void* window, int left, int top, int width, int height);
	virtual void DestroyUI(void* window);
	virtual void GetExtremeDimensions(void* window, int& minWidth, int& maxWidth, int& minHeight, int& maxHeight);

private:
	struct LogMessage
	{
		LogMessage(ILogger::ESeverity severity, tstring message)
			: severity(severity)
			, message(message)
		{
		}
		ILogger::ESeverity severity;
		tstring message;
	};

	void ErrorsToggled(bool value);
	void WarningsToggled(bool value);
	void InfoToggled(bool value);
	void DebugToggled(bool value);
	void RefillList();
	static int GetImageIndex(ILogger::ESeverity eSeverity);
	static int GetSeverityIndex(ILogger::ESeverity eSeverity);

	Layout m_mainLayout;
	Layout m_toolbarLayout;
	ListView m_list;
	std::vector<ToggleButton*> m_buttons;
	std::vector<LogMessage> m_messages;

	unsigned m_filterFlags;
};

#endif //__LOGWINDOW_H__
