// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SubIconContainer.h"

#include <CryIcon.h>
#include <QThumbnailView.h>

CSubIconContainer::CSubIconContainer()
{
	m_languageIcons[0].setValue(QThumbnailsView::SSubIcon{ CryIcon("icons:General/Project_Language_CSharp.ico"), QThumbnailsView::SSubIcon::EPosition::BottomRight });
	m_languageIcons[1].setValue(QThumbnailsView::SSubIcon{ CryIcon("icons:General/Project_Language_Cpp.ico"), QThumbnailsView::SSubIcon::EPosition::BottomRight });
	m_languageIcons[2].setValue(QThumbnailsView::SSubIcon{ CryIcon("icons:General/Project_Language_VisualScripting.ico"), QThumbnailsView::SSubIcon::EPosition::BottomRight });
	m_languageIcons[3].setValue(QThumbnailsView::SSubIcon{ CryIcon("icons:General/Project_Language_Unknown.ico"), QThumbnailsView::SSubIcon::EPosition::BottomRight });

	m_startupIcon.setValue(QThumbnailsView::SSubIcon{ CryIcon("icons:General/Project_Browser_startup_project_badge.ico"), QThumbnailsView::SSubIcon::EPosition::TopLeft });
}

QVariantList CSubIconContainer::GetIcons(const string& language, bool isStartupProject) const
{
	QVariantList lst;

	if (language.CompareNoCase("C#") == 0)
	{
		lst.push_back(m_languageIcons[0]);
	}
	else if (language.CompareNoCase("C++") == 0)
	{
		lst.push_back(m_languageIcons[1]);
	}
	else if (language.CompareNoCase("schematyc") == 0)
	{
		lst.push_back(m_languageIcons[2]);
	}
	else
	{
		lst.push_back(m_languageIcons[3]);
	}

	if (isStartupProject)
	{
		lst.push_back(m_startupIcon);
	}

	return lst;
}
