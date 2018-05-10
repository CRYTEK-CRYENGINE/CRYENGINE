// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ExportStatusWindow.h"
#include "UI/Win32GUI.h"
#include "StringHelpers.h"
#include <process.h>
#include <Windows.h>

enum
{
	WM_USER_TASK_FINISHED = WM_USER + 53,
	WM_USER_ACCEPTED
};

struct ThreadData
{
	ExportStatusWindow* statusWindow;
	void (ExportStatusWindow::*initialize)(int width, int height, const std::vector<std::pair<std::string, std::string> >& tasks);
	void (ExportStatusWindow::*run)();
	int width;
	int height;
	const std::vector<std::pair<std::string, std::string> >* tasks;
	HANDLE initializedSemaphore;
};
unsigned int __stdcall ThreadFunc(void* threadDataMemory)
{
	ThreadData* data = static_cast<ThreadData*>(threadDataMemory);
	ExportStatusWindow* statusWindow = data->statusWindow;
	void (ExportStatusWindow::*initialize)(int width, int height, const std::vector<std::pair<std::string, std::string> >& tasks) = data->initialize;
	void (ExportStatusWindow::*run)() = data->run;
	int width = data->width;
	int height = data->height;
	const std::vector<std::pair<std::string, std::string> >& tasks = *data->tasks;
	HANDLE initializedSemaphore = data->initializedSemaphore;

	// Initialize the data.
	(statusWindow->*initialize)(width, height, tasks);

	// Let the creating thread know that we have read the data - it is
	// now safe for it to clear it.
	ReleaseSemaphore(initializedSemaphore, 1, 0);

	// Perform the main thread processing.
	(statusWindow->*run)();
	return 0;
}

#pragma warning(push)
#pragma warning(disable: 4355) // 'this' : used in base member initializer list
ExportStatusWindow::ExportStatusWindow(int width, int height, const std::vector<std::pair<std::string, std::string> >& tasks)
:	m_threadHandle(0),
	m_warningsOrErrorsEncountered(false),
	m_waitState(WaitState_WarningsAndErrors),
	m_okButtonSpacer(0, 0, 2000, 0),
	m_okButton(_T("OK"), this, &ExportStatusWindow::OkPressed),
	m_okButtonLayout(Layout::DirectionHorizontal)
{
	OutputDebugString(_T("Showing status window.\n"));

	Win32GUI::Initialize();

	HANDLE initializedSemaphore = CreateSemaphore(0, 0, 1, 0);

	// Create a thread to handle the message pump for the window.
	ThreadData threadData;
	threadData.statusWindow = this;
	threadData.initialize = &ExportStatusWindow::Initialize;
	threadData.run = &ExportStatusWindow::Run;
	threadData.width = width;
	threadData.height = height;
	threadData.tasks = &tasks;
	threadData.initializedSemaphore = initializedSemaphore;
	m_threadHandle = (HANDLE)_beginthreadex(
		0,														//void *security,
		0,														//unsigned stack_size,
		ThreadFunc,										//unsigned ( *start_address )( void * ),
		&threadData,                  //void *arglist,
		0,														//unsigned initflag,
		0);														//unsigned *thrdaddr 

	// Wait until the thread has read the data, since once we return the data will be lost.
	WaitForSingleObject(initializedSemaphore, INFINITE);
	CloseHandle(initializedSemaphore);
}
#pragma warning(pop)

ExportStatusWindow::~ExportStatusWindow()
{
	OutputDebugString(_T("Hiding status window.\n"));

	// Tell the thread to exit and then wait for it to do so.
	if (HWND hwnd = (HWND)m_frameWindow.GetHWND())
	{
		PostMessage(hwnd, WM_USER_TASK_FINISHED, 0, 0);
		m_okButton.Enable(true);
		WaitForSingleObject((HANDLE)m_threadHandle, INFINITE);
	}
}

void ExportStatusWindow::Initialize(int width, int height, const std::vector<std::pair<std::string, std::string> >& tasks)
{
	OutputDebugString(_T("Beginning status window thread.\n"));

	for (int taskIndex = 0, taskCount = int(tasks.size()); taskIndex < taskCount; ++taskIndex)
		m_taskList.AddTask(tasks[taskIndex].first, tasks[taskIndex].second);

	m_okButtonLayout.AddComponent(&m_okButtonSpacer);
	m_okButtonLayout.AddComponent(&m_okButton);
	m_okButton.Enable(false);

	m_frameWindow.AddComponent(&m_taskList);
	m_frameWindow.AddComponent(&m_progressBar);
	m_frameWindow.AddComponent(&m_logWindow);
	m_frameWindow.AddComponent(&m_okButtonLayout);
	m_frameWindow.Show(true, width, height);
}

void ExportStatusWindow::Run()
{
	MSG msg;
	BOOL status;
	bool waitingAcceptance = false;
	while ((status = GetMessage(&msg, HWND(0), UINT(0), UINT(0))) != 0)
	{
		if (status == -1)
		{
			break;
		}
		else if (msg.message == WM_USER_TASK_FINISHED)
		{
			if( m_waitState == WaitState_Always || (m_waitState == WaitState_WarningsAndErrors && m_warningsOrErrorsEncountered) )
				waitingAcceptance = true;
			else
				break;
		}
		else if (waitingAcceptance && msg.message == WM_USER_ACCEPTED)
		{
			break;
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	m_frameWindow.Show(false, 0, 0);
	OutputDebugString(_T("Ending status window thread.\n"));
}

void ExportStatusWindow::OkPressed()
{
	if (HWND hwnd = (HWND)m_frameWindow.GetHWND())
		PostMessage(hwnd, WM_USER_ACCEPTED, 0, 0);
}

void ExportStatusWindow::SetWaitState( WaitState state )
{
	m_waitState = state;
}

void ExportStatusWindow::AddTask(const std::string& id, const std::string& description)
{
	m_taskList.AddTask(id, description);
}

void ExportStatusWindow::SetCurrentTask(const std::string& id)
{
	m_taskList.SetCurrentTask(id);
}

void ExportStatusWindow::SetProgress(float progress)
{
	TCHAR buffer[2048];
	_sntprintf_s(buffer, sizeof(buffer), _TRUNCATE, _T("%.1f%% complete - exporting scene."), progress * 100);
	m_frameWindow.SetCaption(buffer);
	m_progressBar.SetProgress(progress);
}

void ExportStatusWindow::Log(ILogger::ESeverity eSeverity, const char* message)
{
	if (eSeverity == ILogger::eSeverity_Error || eSeverity == ILogger::eSeverity_Warning)
		m_warningsOrErrorsEncountered = true;

	m_logWindow.Log(eSeverity, StringHelpers::ConvertString<tstring>(message).c_str());
}
