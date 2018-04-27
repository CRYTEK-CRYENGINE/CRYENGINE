// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SELECTMISSIONOBJECTIVEDIALOG_H__
#define __SELECTMISSIONOBJECTIVEDIALOG_H__
#pragma once

#include "GenericSelectItemDialog.h"

// CSelectSequence dialog

class CSelectMissionObjectiveDialog : public CGenericSelectItemDialog
{
	DECLARE_DYNAMIC(CSelectMissionObjectiveDialog)
	CSelectMissionObjectiveDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSelectMissionObjectiveDialog() {}

protected:
	virtual BOOL OnInitDialog();

	// Derived Dialogs should override this
	virtual void GetItems(std::vector<SItem>& outItems);
	void         GetItemsInternal(std::vector<SItem>& outItems, const char* path, const bool isOptional);

	// Called whenever an item gets selected
	virtual void ItemSelected();

	bool         GetItemsFromFile(std::vector<SItem>& outItems, const char* fileName);

	struct SObjective
	{
		CString shortText;
		CString longText;
	};

	typedef std::map<CString, SObjective, stl::less_stricmp<CString>> TObjMap;
	TObjMap m_objMap;
};

#endif // __SELECTMISSIONOBJECTIVEDIALOG_H__

