// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __EXPORTERSTATUSWINDOW_H__
#define __EXPORTERSTATUSWINDOW_H__

#include "UI/FrameWindow.h"
#include "UI/ProgressBar.h"
#include "UI/TaskList.h"
#include "UI/LogWindow.h"
#include "UI/Spacer.h"
#include "UI/Layout.h"
#include "UI/PushButton.h"
#include "ILogger.h"

class ExportStatusWindow
{
public:
	enum WaitState
	{
		WaitState_WarningsAndErrors,
		WaitState_Always,
		WaitState_Never,
	};

	ExportStatusWindow(int width, int height, const std::vector<std::pair<std::string, std::string> >& tasks);
	~ExportStatusWindow();

	void SetWaitState( WaitState state );

	void AddTask(const std::string& id, const std::string& description);
	void SetCurrentTask(const std::string& id);
	void SetProgress(float progress);
	void Log(ILogger::ESeverity eSeverity, const char* message);

private:
	void Initialize(int width, int height, const std::vector<std::pair<std::string, std::string> >& tasks);
	void Run();
	void OkPressed();

	FrameWindow m_frameWindow;
	TaskList m_taskList;
	ProgressBar m_progressBar;
	Spacer m_okButtonSpacer;
	PushButton m_okButton;
	Layout m_okButtonLayout;
	LogWindow m_logWindow;
	void* m_threadHandle;
	bool m_warningsOrErrorsEncountered;
	WaitState m_waitState;
};

#endif //__EXPORTERSTATUSWINDOW_H__
