// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FileMonitorMiddleware.h"

#include "AudioControlsEditorPlugin.h"
#include "ImplementationManager.h"

#include <ISettings.h>
#include <CryString/CryPath.h>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CFileMonitorMiddleware::CFileMonitorMiddleware(int const delay, QObject* const pParent)
	: CFileMonitorBase(delay, pParent)
{
	Enable();
}

//////////////////////////////////////////////////////////////////////////
void CFileMonitorMiddleware::Enable()
{
	if (g_pIImpl != nullptr)
	{
		Impl::ISettings const* const pISettings = g_pIImpl->GetSettings();

		if (pISettings != nullptr)
		{
			stop();
			m_monitorFolders.clear();
			GetIEditor()->GetFileMonitor()->UnregisterListener(this);

			m_monitorFolders.push_back(pISettings->GetAssetsPath());
			m_monitorFolders.push_back(PathUtil::GetLocalizationFolder().c_str());

			if (pISettings->SupportsProjects())
			{
				m_monitorFolders.push_back(pISettings->GetProjectPath());
			}

			for (auto const& folder : m_monitorFolders)
			{
				GetIEditor()->GetFileMonitor()->RegisterListener(this, folder);
			}
		}
	}
}
} // namespace ACE
