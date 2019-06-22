// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ImplManager.h"

#include "Common.h"
#include "AudioControlsEditorPlugin.h"
#include "FileImporterUtils.h"
#include "Common/IImpl.h"

#include <IUndoManager.h>
#include <CrySystem/IConsole.h>

namespace ACE
{
typedef void (WINAPI * PGNSI)();
using TPfnGetAudioInterface = Impl::IImpl* (*)(ISystem*);

//////////////////////////////////////////////////////////////////////////
void ReloadSystemControls()
{
	if (g_pMainWindow == nullptr)
	{
		CAudioControlsEditorPlugin::ReloadData(EReloadFlags::ReloadSystemControls);
	}
}

//////////////////////////////////////////////////////////////////////////
CImplManager::~CImplManager()
{
	if (m_hMiddlewarePlugin != nullptr)
	{
		FreeLibrary(m_hMiddlewarePlugin);
		m_hMiddlewarePlugin = nullptr;
	}

	g_pIImpl = nullptr;
}

//////////////////////////////////////////////////////////////////////////
bool CImplManager::LoadImpl()
{
	SignalOnBeforeImplChange();
	GetIEditor()->GetIUndoManager()->Suspend();

	bool isLoaded = true;
	ICVar const* const pCVar = gEnv->pConsole->GetCVar(CryAudio::g_szImplCVarName);

	if (pCVar != nullptr)
	{
		if (m_hMiddlewarePlugin != nullptr)
		{
			// Need to flush the undo/redo queue to make sure we're not keeping data from
			// previous impl there
			GetIEditor()->GetIUndoManager()->Flush();

			FreeLibrary(m_hMiddlewarePlugin);
			m_hMiddlewarePlugin = nullptr;
			g_pIImpl = nullptr;
		}

		char szExecutableDirPath[_MAX_PATH];
		CryGetExecutableFolder(sizeof(szExecutableDirPath), szExecutableDirPath);
		cry_sprintf(szExecutableDirPath, "%sEditorPlugins\\ace\\Editor%s.dll", szExecutableDirPath, pCVar->GetString());

		m_hMiddlewarePlugin = LoadLibraryA(szExecutableDirPath);

		if (m_hMiddlewarePlugin == nullptr)
		{
			ReloadSystemControls();
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "[Audio Controls Editor] Couldn't load the middleware specific editor dll.");
			isLoaded = false;
		}
		else
		{
			auto const pfnAudioInterface = (TPfnGetAudioInterface)GetProcAddress(m_hMiddlewarePlugin, "GetAudioInterface");

			if (pfnAudioInterface == nullptr)
			{
				ReloadSystemControls();
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "[Audio Controls Editor] Couldn't get middleware interface from loaded dll.");
				FreeLibrary(m_hMiddlewarePlugin);
				isLoaded = false;
			}
			else
			{
				if (GetIEditor() != nullptr)
				{
					g_pIImpl = pfnAudioInterface(GetIEditor()->GetSystem());
					ZeroStruct(g_implInfo);
					g_extensionFilters.clear();
					g_supportedFileTypes.clear();
					g_pIImpl->Initialize(g_implInfo, g_extensionFilters, g_supportedFileTypes);

					if (g_pMainWindow != nullptr)
					{
						CAudioControlsEditorPlugin::ReloadData(EReloadFlags::ReloadImplData);
					}
					else
					{
						CAudioControlsEditorPlugin::ReloadData(EReloadFlags::ReloadSystemControls | EReloadFlags::SendSignals | EReloadFlags::ReloadImplData);
					}

				}
			}
		}
	}
	else
	{
		ReloadSystemControls();
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "[Audio Controls Editor] CVar %s not defined. Needed to derive the Editor plugin name.", CryAudio::g_szImplCVarName);
		isLoaded = false;
	}

	GetIEditor()->GetIUndoManager()->Resume();
	SignalOnAfterImplChange();
	return isLoaded;
}

//////////////////////////////////////////////////////////////////////////
void CImplManager::Release()
{
	if (m_hMiddlewarePlugin)
	{
		FreeLibrary(m_hMiddlewarePlugin);
	}
}
} // namespace ACE
