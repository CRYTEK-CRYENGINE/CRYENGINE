// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "StdAfx.h"
#include "CSharpEditorPlugin.h"

#include <Include/SandboxAPI.h>
#include <IEditorImpl.h>
#include "QT/QToolTabManager.h"

#include <AssetSystem/Asset.h>
#include <AssetSystem/AssetManager.h>
#include <AssetSystem/AssetType.h>
#include <CSharpOutputWindow.h>
#include <IObjectManager.h>
#include <Objects/ClassDesc.h>
#include <CrySystem/IProjectManager.h>
#include <Preferences/GeneralPreferences.h>

#include <CryCore/Platform/platform_impl.inl>

CCSharpEditorPlugin* CCSharpEditorPlugin::s_pInstance = nullptr;

CCSharpEditorPlugin::CCSharpEditorPlugin()
	: m_messageListeners(3)
{
	s_pInstance = this;

	GetIEditor()->GetFileMonitor()->RegisterListener(this, "", "cs");

	gEnv->pSchematyc->GetEnvRegistry().RegisterListener(this);

	CRY_ASSERT(gEnv->pMonoRuntime != nullptr);
	if (gEnv->pMonoRuntime != nullptr)
	{
		gEnv->pMonoRuntime->RegisterCompileListener(this);

		OnCompileFinished(gEnv->pMonoRuntime->GetLatestCompileMessage());
	}

	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CCSharpEditorPlugin");
}

CCSharpEditorPlugin::~CCSharpEditorPlugin()
{
	GetIEditor()->GetFileMonitor()->UnregisterListener(this);

	gEnv->pSchematyc->GetEnvRegistry().UnregisterListener(this);

	if (gEnv->pMonoRuntime != nullptr)
	{
		gEnv->pMonoRuntime->UnregisterCompileListener(this);
	}

	gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
}

void CCSharpEditorPlugin::OnFileChange(const char* sFilename, EChangeType eType)
{
	if (m_sandboxInFocus)
	{
		UpdateSourceFiles();
	}
	else
	{
		m_reloadPlugins = true;
	}
}

void CCSharpEditorPlugin::OnCompileFinished(const char* szCompileMessage)
{
	m_compileMessage = szCompileMessage;
	bool messageSend = false;
	for (CSharpMessageListeners::Notifier notifier(m_messageListeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnMessagesUpdated(m_compileMessage);
		messageSend = true;
	}

	// If no message was send, it means no window is currently open.
	// If the message contains content the window is forced open to show any potential errors.
	if (!messageSend && m_compileMessage.length() > 0)
	{
		GetIEditor()->OpenView("C# Output");
		CryLogAlways(m_compileMessage);
	}
}

void CCSharpEditorPlugin::OnEnvElementAdd(Schematyc::IEnvElementPtr pElement)
{
	if (pElement->GetType() != Schematyc::EEnvElementType::Component)
	{
		return;
	}

	auto pComponent = static_cast<Schematyc::IEnvComponent*>(pElement.get());
	if (strlen(pComponent->GetDesc().GetLabel()) == 0)
	{
		return;
	}

	if (pComponent->GetDesc().GetComponentFlags().Check(EEntityComponentFlags::HideFromInspector))
	{
		return;
	}

	CObjectClassDesc* clsDesc = GetIEditor()->GetObjectManager()->FindClass("EntityWithComponent");
	if (!clsDesc)
	{
		return;
	}

	const char* szCategory = pComponent->GetDesc().GetEditorCategory();
	if (szCategory == nullptr || szCategory[0] == '\0')
	{
		szCategory = "General";
	}

	char buffer[37];
	pComponent->GetDesc().GetGUID().ToString(buffer);

	string sLabel = szCategory;
	sLabel.append("/");
	sLabel.append(pComponent->GetDesc().GetLabel());

	const char* description = pComponent->GetDesc().GetDescription();

	clsDesc->m_itemAdded(sLabel.c_str(), buffer, description);
}

void CCSharpEditorPlugin::OnEnvElementDelete(Schematyc::IEnvElementPtr pElement)
{
	if (pElement->GetType() != Schematyc::EEnvElementType::Component)
	{
		return;
	}

	auto pComponent = static_cast<Schematyc::IEnvComponent*>(pElement.get());
	if (strlen(pComponent->GetDesc().GetLabel()) == 0)
	{
		return;
	}

	if (pComponent->GetDesc().GetComponentFlags().Check(EEntityComponentFlags::HideFromInspector))
	{
		return;
	}

	CObjectClassDesc* clsDesc = GetIEditor()->GetObjectManager()->FindClass("EntityWithComponent");
	if (!clsDesc)
	{
		return;
	}

	const char* szCategory = pComponent->GetDesc().GetEditorCategory();
	if (szCategory == nullptr || szCategory[0] == '\0')
	{
		szCategory = "General";
	}

	char buffer[37];
	pComponent->GetDesc().GetGUID().ToString(buffer);

	string sLabel = szCategory;
	sLabel.append("/");
	sLabel.append(pComponent->GetDesc().GetLabel());

	clsDesc->m_itemRemoved(sLabel.c_str(), buffer);
}

void CCSharpEditorPlugin::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	if (event == ESYSTEM_EVENT_CHANGE_FOCUS)
	{
		if (wparam == 0)
		{
			// Lost focus
			m_sandboxInFocus = false;
		}
		else
		{
			// Got focus back
			m_sandboxInFocus = true;

			if (m_reloadPlugins)
			{
				UpdateSourceFiles();
				m_reloadPlugins = false;
			}
		}
	}
}

void CCSharpEditorPlugin::UpdateSourceFiles() const
{
	if (IProjectManager* pProjectManager = gEnv->pSystem->GetIProjectManager())
	{
		pProjectManager->RegenerateCSharpSolution(pProjectManager->GetCurrentAssetDirectoryRelative());
	}

	if (gEnv->pMonoRuntime != nullptr)
	{
		gEnv->pMonoRuntime->ReloadPluginDomain();
	}
}

bool CCSharpEditorPlugin::OpenCSharpFile(const string& filePath)
{
	if (gEditorFilePreferences.openCSharpSolution && OpenFileInSolution(filePath))
	{
		return true;
	}

	if (OpenFileInTextEditor(filePath))
	{
		return true;
	}
	return OpenCSharpFileSafe(filePath);
}

bool CCSharpEditorPlugin::OpenCSharpFile(const string& filePath, const int line)
{
	if (gEditorFilePreferences.openCSharpSolution && OpenFileInSolution(filePath, line))
	{
		return true;
	}

	if (OpenFileInTextEditor(filePath, line))
	{
		return true;
	}
	return OpenCSharpFileSafe(filePath);
}

bool CCSharpEditorPlugin::OpenCSharpSolution()
{
	// If a text editor is already open we assume it's still running our solution.
	if (HasExistingTextEditor())
	{
		return true;
	}

	string textEditor = gEditorFilePreferences.textEditorCSharp;
	// No active process found, so open the solution file with the selected text editor.
	if (!textEditor.empty())
	{
		string solutionFile = PathUtil::Make(gEnv->pSystem->GetIProjectManager()->GetCurrentProjectDirectoryAbsolute(), "game.sln");
		SHELLEXECUTEINFO shellInfo = SHELLEXECUTEINFO();
		shellInfo.lpVerb = "open";
		shellInfo.lpFile = textEditor.c_str();
		shellInfo.lpParameters = solutionFile.c_str();
		shellInfo.nShow = SW_SHOWNORMAL;
		shellInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		shellInfo.cbSize = sizeof(shellInfo);

		if (!ShellExecuteEx(&shellInfo) || (DWORD_PTR)shellInfo.hInstApp <= 32)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Unable to open the C# solution with %s!", textEditor);
			return false;
		}

		m_textEditorHandle = shellInfo.hProcess;
		m_createdTextEditor = textEditor;
		return true;
	}
	return false;
}

bool CCSharpEditorPlugin::OpenFileInSolution(const string& filePath)
{
	if (HasExistingTextEditor())
	{
		if (OpenFileInTextEditor(filePath))
		{
			return true;
		}
	}

	string textEditor = gEditorFilePreferences.textEditorCSharp;
	// No active process found, so open the solution file with the selected text editor.
	if (!textEditor.empty())
	{
		string solutionFile = PathUtil::Make(gEnv->pSystem->GetIProjectManager()->GetCurrentProjectDirectoryAbsolute(), "game.sln");
		SHELLEXECUTEINFO shellInfo = SHELLEXECUTEINFO();
		shellInfo.lpVerb = "open";
		shellInfo.lpFile = textEditor.c_str();
		shellInfo.lpParameters = string("%s %s").Format(solutionFile, filePath).c_str();
		shellInfo.nShow = SW_SHOWNORMAL;
		shellInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		shellInfo.cbSize = sizeof(shellInfo);

		if (!ShellExecuteEx(&shellInfo) || (DWORD_PTR)shellInfo.hInstApp <= 32)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Unable to open file %s and the C# solution with \"%s\"!", filePath, textEditor);
			return false;
		}

		m_textEditorHandle = shellInfo.hProcess;
		m_createdTextEditor = textEditor;
		return true;
	}
	return false;
}

bool CCSharpEditorPlugin::OpenFileInSolution(const string& filePath, const int line)
{
	string textEditor = gEditorFilePreferences.textEditorCSharp;
	string commandFormat = "";

	if (textEditor.find("XamarinStudio.exe") != string::npos)
	{
		commandFormat.Format(";%i;0", line);
	}
	else
	{
		// Opening a file on a specific line is not supported on all platforms. Instead only the file itself is opened.
		return OpenFileInSolution(filePath);
	}

	if (HasExistingTextEditor())
	{
		if (OpenFileInTextEditor(filePath, line))
		{
			return true;
		}
	}

	// No active process found, so open the solution file with the selected text editor.
	if (!textEditor.empty())
	{
		string solutionFile = PathUtil::Make(gEnv->pSystem->GetIProjectManager()->GetCurrentProjectDirectoryAbsolute(), "game.sln");
		SHELLEXECUTEINFO shellInfo = SHELLEXECUTEINFO();
		shellInfo.lpVerb = "open";
		shellInfo.lpFile = textEditor.c_str();

		string arguments;
		arguments.Format("%s %s%s", solutionFile, filePath, commandFormat, line);
		shellInfo.lpParameters = arguments.c_str();
		shellInfo.nShow = SW_SHOWNORMAL;
		shellInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		shellInfo.cbSize = sizeof(shellInfo);

		if (!ShellExecuteEx(&shellInfo) || (DWORD_PTR)shellInfo.hInstApp <= 32)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Unable to open file %s and the C# solution with \"%s\"!", filePath, textEditor);
			return false;
		}

		m_textEditorHandle = shellInfo.hProcess;
		m_createdTextEditor = textEditor;
		return true;
	}
	return false;
}

bool CCSharpEditorPlugin::OpenFileInTextEditor(const string& filePath) const
{
	string textEditor = gEditorFilePreferences.textEditorCSharp;
	// No active process found, so open the solution file with the selected text editor.
	if (!textEditor.empty())
	{
		string solutionFile = PathUtil::Make(gEnv->pSystem->GetIProjectManager()->GetCurrentProjectDirectoryAbsolute(), "game.sln");
		SHELLEXECUTEINFO shellInfo = SHELLEXECUTEINFO();
		shellInfo.lpVerb = "open";
		shellInfo.lpFile = textEditor.c_str();

		string arguments;
		arguments.Format("%s /edit", filePath);
		shellInfo.lpParameters = arguments.c_str();
		shellInfo.nShow = SW_SHOWNORMAL;
		shellInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		shellInfo.cbSize = sizeof(shellInfo);

		if (!ShellExecuteEx(&shellInfo) || (DWORD_PTR)shellInfo.hInstApp <= 32)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Unable to open file %s and the C# solution with \"%s\"!", filePath, textEditor);
			return false;
		}
		return true;
	}
	return false;
}

bool CCSharpEditorPlugin::OpenFileInTextEditor(const string& filePath, const int line) const
{
	string textEditor = gEditorFilePreferences.textEditorCSharp;
	string commandFormat = "";
	
	if (textEditor.find("XamarinStudio.exe") != string::npos)
	{
		commandFormat.Format(";%i;0", line);
	}
	else
	{
		// Opening a file on a specific line is not supported on all platforms. Instead only the file itself is opened.
		return OpenFileInTextEditor(filePath);
	}

	// No active process found, so open the solution file with the selected text editor.
	if (!textEditor.empty())
	{
		string solutionFile = PathUtil::Make(gEnv->pSystem->GetIProjectManager()->GetCurrentProjectDirectoryAbsolute(), "game.sln");
		SHELLEXECUTEINFO shellInfo = SHELLEXECUTEINFO();
		shellInfo.lpVerb = "open";
		shellInfo.lpFile = textEditor.c_str();

		string arguments;
		arguments.Format("%s%s /edit", filePath, commandFormat, line);
		shellInfo.lpParameters = arguments.c_str();
		shellInfo.nShow = SW_SHOWNORMAL;
		shellInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		shellInfo.cbSize = sizeof(shellInfo);

		if (!ShellExecuteEx(&shellInfo) || (DWORD_PTR)shellInfo.hInstApp <= 32)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Unable to open file %s and the C# solution with \"%s\"!", filePath, textEditor);
			return false;
		}
		return true;
	}
	return false;
}

BOOL CALLBACK SetWindowToForeground(HWND hwnd, LPARAM lparam)
{
	DWORD processId = lparam;
	DWORD id = 0;
	GetWindowThreadProcessId(hwnd, &id);

	if (id == processId)
	{
		SetForegroundWindow(hwnd);
		return false;
	}
	return true;
}

bool CCSharpEditorPlugin::HasExistingTextEditor() const
{
	string textEditor = gEditorFilePreferences.textEditorCSharp;

	// The user could've set a new preferred text editor. In that case disregard the old text editor.
	if (textEditor != m_createdTextEditor)
	{
		return false;
	}

	// Try to set the window to the foreground first if it's still active. Otherwise, open a new window with the solution.
	DWORD exitCode;
	if (m_textEditorHandle && GetExitCodeProcess(m_textEditorHandle, &exitCode))
	{
		if (exitCode == STILL_ACTIVE)
		{
			DWORD processId = GetProcessId(m_textEditorHandle);
			// Invert the value because EnumWindows will return false if SetWindowToForeground found the window, and true if it didn't.
			return !EnumWindows(&SetWindowToForeground, processId);
		}
	}
	return false;
}

bool CCSharpEditorPlugin::OpenCSharpFileSafe(const string& filePath) const
{
	HINSTANCE hInst = ShellExecute(NULL, "open", filePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
	if ((DWORD_PTR)hInst > 32)
	{
		return true;
	}

	const char* szDialog = "Can't open the file. "
	                       "You can specify a source editor in Sandbox Preferences or create an association in Windows. "
	                       "Do you want to open the file in the Notepad?";
	if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(""),
	                                                                        QObject::tr(szDialog),
	                                                                        QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No))
	{
		hInst = ShellExecute(NULL, "open", "notepad", filePath.c_str(), NULL, SW_SHOWNORMAL);
		return (DWORD_PTR)hInst > 32;
	}
	return false;
}
