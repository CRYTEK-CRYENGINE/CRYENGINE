// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "../StdTypes.hpp"
#include "../Error.hpp"
#include "../STLHelper.hpp"
#include "../Common.h"
#include "../tinyxml/tinyxml.h"
#include "CrySimpleJob.hpp"

volatile long CCrySimpleJob::m_GlobalRequestNumber = 0;

CCrySimpleJob::CCrySimpleJob(uint32_t requestIP):
m_State(ECSJS_NONE),
m_RequestIP(requestIP)
{
	InterlockedIncrement(&m_GlobalRequestNumber);
}

CCrySimpleJob::~CCrySimpleJob()
{
}


bool CCrySimpleJob::ExecuteCommand(const std::string& rCmd, const std::string& args, std::string &outError)
{
#ifdef _MSC_VER
	bool	Ret	= false;
	DWORD	ExitCode	=	0;

	STARTUPINFO StartupInfo;
	PROCESS_INFORMATION ProcessInfo;
	memset(&StartupInfo, 0, sizeof(StartupInfo));
	memset(&ProcessInfo, 0, sizeof(ProcessInfo));
	StartupInfo.cb = sizeof(StartupInfo);

	std::string Path = "";
	std::string::size_type Pt = rCmd.find_last_of("\\/");
	if (Pt != std::string::npos)
		Path = std::string(rCmd.c_str(), Pt);

	std::string commandLine = "\"" + rCmd + "\" " + args;

	HANDLE hReadErr, hWriteErr;

	{
		CreatePipe(&hReadErr, &hWriteErr, NULL, 0);
		SetHandleInformation(hWriteErr, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

		StartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
		StartupInfo.hStdOutput = NULL;//GetStdHandle(STD_OUTPUT_HANDLE);
		StartupInfo.hStdError = hWriteErr;
		StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

		BOOL processCreated = CreateProcess(NULL,(char*)commandLine.c_str(),0,0, TRUE,CREATE_DEFAULT_ERROR_MODE,0,Pt!=std::string::npos?Path.c_str():0,&StartupInfo,&ProcessInfo) != false;

		if(!processCreated)
		{
			outError = "Couldn't create process - missing compiler for cmd?: '" + rCmd + "'";
		}
		else
		{
			std::string error;

			DWORD waitResult = 0;
			HANDLE waitHandles[] = { ProcessInfo.hProcess, hReadErr };
			while(true)
			{
				//waitResult = WaitForMultipleObjects(sizeof(waitHandles) / sizeof(waitHandles[0]), waitHandles, FALSE, 1000 );
				waitResult = WaitForSingleObject(ProcessInfo.hProcess, 1000 );
				if (waitResult == WAIT_FAILED)
					break;

				DWORD bytesRead, bytesAvailable;
				while(PeekNamedPipe(hReadErr, NULL, 0, NULL, &bytesAvailable, NULL) && bytesAvailable)
				{
					char buff[4096];
					ReadFile(hReadErr, buff, sizeof(buff)-1, &bytesRead, 0);
					buff[bytesRead] = '\0';
					error += buff;
				}

				//if (waitResult == WAIT_OBJECT_0 || waitResult == WAIT_TIMEOUT)
					//break;

				if (waitResult == WAIT_OBJECT_0)
					break;
			}

			//if (waitResult != WAIT_TIMEOUT)
			{
				GetExitCodeProcess(ProcessInfo.hProcess,&ExitCode);
				if (ExitCode)
				{
					Ret = false;
					outError = error;
				}
				else
				{
					Ret = true;
				}
			}
			/*
			else
			{
				Ret = false;
				outError = std::string("Timed out executing compiler: ") + rCmd;
				TerminateProcess(ProcessInfo.hProcess, 1);
			}
			*/

			CloseHandle(ProcessInfo.hProcess);
			CloseHandle(ProcessInfo.hThread);
		}

		CloseHandle(hReadErr);
		if (hWriteErr)
			CloseHandle(hWriteErr);
	}

	return Ret;
#endif

#ifdef UNIX
	return system((rFileName + " " + rParams).c_str()) == 0;
#endif
}

