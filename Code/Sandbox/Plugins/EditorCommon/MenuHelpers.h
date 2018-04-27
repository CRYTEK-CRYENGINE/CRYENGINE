// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ISourceControl.h"
#include <QMenu>
#include <QAction>

inline uint32 GetAllFileAttributes(ISourceControl* pSourceControl, const std::vector<string>& filenames)
{
	if (pSourceControl && !filenames.empty())
	{
		uint32 attributes = std::numeric_limits<uint32>::max();
		for (const string& filename : filenames)
		{
			attributes &= pSourceControl->GetFileAttributes(filename.c_str());
		}
		return attributes;
	}
	return ESccFileAttributes::SCC_FILE_ATTRIBUTE_INVALID;
}

inline void AddSourceControlOptions(QMenu* pMenu, std::vector<string>& filenames)
{
	if (pMenu && GetIEditor()->IsSourceControlAvailable())
	{
		ISourceControl* pSourceControl = GetIEditor()->GetSourceControl();
		if (pSourceControl)
		{

			// Add to Source Control
			QAction* pAction = pMenu->addAction("Add to Source Control");
			if (!(GetAllFileAttributes(pSourceControl, filenames) & ESccFileAttributes::SCC_FILE_ATTRIBUTE_MANAGED))
			{
				QObject::connect(pAction, &QAction::triggered, [filenames]()
				{
					char changeid[16];
					ISourceControl* pSourceControl = GetIEditor()->GetSourceControl();
					if (pSourceControl->CreateChangeList("New layers", changeid, sizeof(changeid)))
					{
					  for (const string& filename : filenames)
					  {
					    pSourceControl->Add(filename.c_str(), 0, ADD_WITHOUT_SUBMIT | ADD_CHANGELIST, changeid);
					  }
					}
				});
			}
			else
			{
				pAction->setEnabled(false);
			}

			// Check-In
			pAction = pMenu->addAction("Check In");
			if (GetAllFileAttributes(pSourceControl, filenames) & ESccFileAttributes::SCC_FILE_ATTRIBUTE_CHECKEDOUT)
			{
				QObject::connect(pAction, &QAction::triggered, [filenames]()
				{
					char changeid[16];
					ISourceControl* pSourceControl = GetIEditor()->GetSourceControl();
					if (pSourceControl->CreateChangeList("New layers", changeid, sizeof(changeid)))
					{
					  for (const string& filename : filenames)
					  {
					    pSourceControl->Add(filename.c_str(), 0, ADD_WITHOUT_SUBMIT | ADD_CHANGELIST, changeid);
					  }
					}
				});
			}
			else
			{
				pAction->setEnabled(false);
			}

			// Check-out
			pAction = pMenu->addAction("Check Out");
			if (GetAllFileAttributes(pSourceControl, filenames) & ESccFileAttributes::SCC_FILE_ATTRIBUTE_MANAGED)
			{
				QObject::connect(pAction, &QAction::triggered, [filenames]()
				{
					ISourceControl* pSourceControl = GetIEditor()->GetSourceControl();
					for (const string& filename : filenames)
					{
					  pSourceControl->CheckOut(filename.c_str(), ADD_WITHOUT_SUBMIT | ADD_CHANGELIST);
					}
				});
			}
			else
			{
				pAction->setEnabled(false);
			}

			// Revert
			pAction = pMenu->addAction("Revert");
			if (GetAllFileAttributes(pSourceControl, filenames) & ESccFileAttributes::SCC_FILE_ATTRIBUTE_CHECKEDOUT)
			{
				QObject::connect(pAction, &QAction::triggered, [filenames]()
				{
					ISourceControl* pSourceControl = GetIEditor()->GetSourceControl();
					for (const string& filename : filenames)
					{
					  pSourceControl->UndoCheckOut(filename.c_str());
					}
				});
			}
			else
			{
				pAction->setEnabled(false);
			}

			// Get latest version
			pAction = pMenu->addAction("Get Latest Version");
			if (GetAllFileAttributes(pSourceControl, filenames) & ESccFileAttributes::SCC_FILE_ATTRIBUTE_MANAGED)
			{
				QObject::connect(pAction, &QAction::triggered, [filenames]()
				{
					ISourceControl* pSourceControl = GetIEditor()->GetSourceControl();
					for (const string& filename : filenames)
					{
					  pSourceControl->GetLatestVersion(filename.c_str());
					}
				});
			}
			else
			{
				pAction->setEnabled(false);
			}

			// Show history
			pAction = pMenu->addAction("Show History");
			if (GetAllFileAttributes(pSourceControl, filenames) & ESccFileAttributes::SCC_FILE_ATTRIBUTE_MANAGED)
			{
				QObject::connect(pAction, &QAction::triggered, [filenames]()
				{
					ISourceControl* pSourceControl = GetIEditor()->GetSourceControl();
					for (const string& filename : filenames)
					{
					  pSourceControl->History(filename.c_str());
					}
				});
			}
			else
			{
				pAction->setEnabled(false);
			}
		}
	}
}

