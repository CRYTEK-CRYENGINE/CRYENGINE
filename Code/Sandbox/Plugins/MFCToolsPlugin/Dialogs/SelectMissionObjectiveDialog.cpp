// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SelectMissionObjectiveDialog.h"
#include <CryMovie/IMovieSystem.h>
#include <CrySystem/ILocalizationManager.h>
#include <CryString/UnicodeFunctions.h>
#include "Util/FileUtil.h"
#include "IResourceSelectorHost.h"

// CSelectMissionObjectiveDialog

IMPLEMENT_DYNAMIC(CSelectMissionObjectiveDialog, CGenericSelectItemDialog)

//////////////////////////////////////////////////////////////////////////
CSelectMissionObjectiveDialog::CSelectMissionObjectiveDialog(CWnd* pParent) : CGenericSelectItemDialog(pParent)
{
	m_dialogID = "Dialogs\\SelMissionObj";
}

//////////////////////////////////////////////////////////////////////////
/* virtual */ BOOL
CSelectMissionObjectiveDialog::OnInitDialog()
{
	SetTitle(_T("Select MissionObjective"));
	SetMode(eMODE_TREE);
	ShowDescription(true);
	return __super::OnInitDialog();
}

//////////////////////////////////////////////////////////////////////////
/* virtual */ void
CSelectMissionObjectiveDialog::GetItems(std::vector<SItem>& outItems)
{
	// load MOs
	CString path = PathUtil::GetGameFolder() + "/Libs/UI/";
	if (!CFileUtil::FileExists("Libs/UI/Objectives_global.xml")
	    && CFileUtil::FileExists("Libs/UI/Objectives_new.xml"))
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "File 'Objectives_new.xml' is deprecated and should be renamed to 'Objectives_global.xml'");
		path += "Objectives_new.xml";
	}
	else
	{
		path += "Objectives_global.xml";
	}
	GetItemsInternal(outItems, path.GetString(), false);

	string xmlPath = GetIEditor()->GetLevelPath();
	xmlPath = PathUtil::Make(xmlPath, "LevelData");
	xmlPath = PathUtil::Make(xmlPath, "Objectives.xml");
	GetItemsInternal(outItems, xmlPath.GetString(), true);
}

//////////////////////////////////////////////////////////////////////////
void
CSelectMissionObjectiveDialog::GetItemsInternal(std::vector<SItem>& outItems, const char* path, const bool isOptional)
{
	if (!CFileUtil::FileExists(path) && isOptional)
	{
		return;
	}

	XmlNodeRef missionObjectives = GetISystem()->LoadXmlFromFile(path);
	if (missionObjectives == 0)
	{
		if (!isOptional)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Error while loading MissionObjective file '%s'", path);
		}
		return;
	}

	for (int tag = 0; tag < missionObjectives->getChildCount(); ++tag)
	{
		XmlNodeRef mission = missionObjectives->getChild(tag);
		const char* attrib;
		const char* objective;
		const char* text;

		const char* levelName;
		if (!mission->getAttr("name", &levelName))
		{
			levelName = mission->getTag();
		}

		for (int obj = 0; obj < mission->getChildCount(); ++obj)
		{
			XmlNodeRef objectiveNode = mission->getChild(obj);
			CString id(levelName);
			id += ".";
			id += objectiveNode->getTag();
			if (objectiveNode->getAttributeByIndex(0, &attrib, &objective) && objectiveNode->getAttributeByIndex(1, &attrib, &text))
			{
				SObjective obj;
				obj.shortText = objective;
				obj.longText = text;
				m_objMap[id] = obj;
				SItem item;
				item.name = id;
				outItems.push_back(item);
			}
			else if (!isOptional)
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Error while loading MissionObjective file '%s'", path);
				return;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
/* virtual */ void CSelectMissionObjectiveDialog::ItemSelected()
{
	TObjMap::const_iterator iter = m_objMap.find(m_selectedItem);
	if (iter != m_objMap.end())
	{
		const SObjective& obj = (*iter).second;
		CString objText = obj.shortText;
		if (objText.IsEmpty() == false && objText.GetAt(0) == '@')
		{
			SLocalizedInfoGame locInfo;
			const char* key = objText.GetString() + 1;
			bool bFound = gEnv->pSystem->GetLocalizationManager()->GetLocalizedInfoByKey(key, locInfo);
			objText = "Label: ";
			objText += obj.shortText;
			if (bFound)
			{
				objText += "\r\nPlain Text: ";
				objText += Unicode::Convert<wstring>(locInfo.sUtf8TranslatedText).c_str();
			}
		}
		m_desc.SetWindowText(objText);
#if 0
		CryLogAlways("Objective: ID='%s' Text='%s' LongText='%s'",
		             m_selectedItem.GetString(), obj.shortText.GetString(), obj.longText.GetString());
#endif
	}
	else
	{
		m_desc.SetWindowText(_T("<No Objective Selected>"));
	}
}

namespace
{
dll_string ShowDialog(const SResourceSelectorContext& context, const char* szPreviousValue)
{
	CSelectMissionObjectiveDialog gtDlg(nullptr);
	gtDlg.PreSelectItem(szPreviousValue);
	if (gtDlg.DoModal() == IDOK)
	{
		CString result = gtDlg.GetSelectedItem();
		return (LPCSTR)result;
	}
	return szPreviousValue;
}

REGISTER_RESOURCE_SELECTOR("Missions", ShowDialog, "icons:General/Folder_Tinted.ico")
}
