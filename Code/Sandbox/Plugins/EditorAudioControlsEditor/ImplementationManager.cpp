// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ImplementationManager.h"

#include "SystemAssetsManager.h"
#include "AudioControlsEditorPlugin.h"
#include "IUndoManager.h"

#include <IEditorImpl.h>
#include <CrySystem/IConsole.h>
#include <IEditor.h>

string const g_sImplementationCVarName = "s_AudioImplName";

namespace ACE
{
typedef void (WINAPI* PGNSI)();
using TPfnGetAudioInterface = IEditorImpl* (*)(ISystem*);

//////////////////////////////////////////////////////////////////////////
CImplementationManager::CImplementationManager()
	: m_hMiddlewarePlugin(nullptr)
	, m_pEditorImpl(nullptr)
{
}

//////////////////////////////////////////////////////////////////////////
CImplementationManager::~CImplementationManager()
{
	if (m_hMiddlewarePlugin != nullptr)
	{
		FreeLibrary(m_hMiddlewarePlugin);
		m_hMiddlewarePlugin = nullptr;
	}

	m_pEditorImpl = nullptr;
}

//////////////////////////////////////////////////////////////////////////
bool CImplementationManager::LoadImplementation()
{
	signalImplementationAboutToChange();
	GetIEditor()->GetIUndoManager()->Suspend();

	bool isLoaded = true;
	ICVar* pCVar = gEnv->pConsole->GetCVar(g_sImplementationCVarName);

	if (pCVar != nullptr)
	{
		if (m_hMiddlewarePlugin != nullptr)
		{
			// Need to flush the undo/redo queue to make sure we're not keeping data from
			// previous implementation there
			GetIEditor()->GetIUndoManager()->Flush();

			FreeLibrary(m_hMiddlewarePlugin);
			m_hMiddlewarePlugin = nullptr;
			m_pEditorImpl = nullptr;
		}

		char szExecutableDirPath[_MAX_PATH];
		CryGetExecutableFolder(sizeof(szExecutableDirPath), szExecutableDirPath);
		cry_sprintf(szExecutableDirPath, "%sEditorPlugins\\ace\\Editor%s.dll", szExecutableDirPath, pCVar->GetString());

		m_hMiddlewarePlugin = LoadLibraryA(szExecutableDirPath);

		if (m_hMiddlewarePlugin == nullptr)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "[Audio Controls Editor] Couldn't load the middleware specific editor dll.");
			isLoaded = false;
		}
		else
		{
			TPfnGetAudioInterface pfnAudioInterface = (TPfnGetAudioInterface)GetProcAddress(m_hMiddlewarePlugin, "GetAudioInterface");

			if (pfnAudioInterface == nullptr)
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "[Audio Controls Editor] Couldn't get middleware interface from loaded dll.");
				FreeLibrary(m_hMiddlewarePlugin);
				isLoaded = false;
			}
			else
			{
				if (GetIEditor() != nullptr)
				{
					m_pEditorImpl = pfnAudioInterface(GetIEditor()->GetSystem());

					if (m_pEditorImpl != nullptr)
					{
						CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_COMMENT, "[Audio Controls Editor] Reloading audio implementation data");
						m_pEditorImpl->Reload();
					}
					else
					{
						CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "[Audio Controls Editor] Data from middleware is empty.");
					}
				}
			}
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "[Audio Controls Editor] CVar %s not defined. Needed to derive the Editor plugin name.", g_sImplementationCVarName);
		isLoaded = false;
	}

	GetIEditor()->GetIUndoManager()->Resume();
	signalImplementationChanged();
	return isLoaded;
}

//////////////////////////////////////////////////////////////////////////
void CImplementationManager::Release()
{
	if (m_hMiddlewarePlugin)
	{
		FreeLibrary(m_hMiddlewarePlugin);
	}
}

//////////////////////////////////////////////////////////////////////////
IEditorImpl* CImplementationManager::GetImplementation()
{
	return m_pEditorImpl;
}
} // namespace ACE