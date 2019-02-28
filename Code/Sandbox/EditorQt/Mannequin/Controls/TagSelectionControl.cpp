// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "TagSelectionControl.h"

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CTagSelectionControl, CXTResizeDialog)

//////////////////////////////////////////////////////////////////////////
enum
{
	IDC_PROPERTIES_CONTROL = 1,
};

//////////////////////////////////////////////////////////////////////////
CTagSelectionControl::CTagSelectionControl()
	: m_pTagDef(NULL)
	, m_pVarBlock(new CVarBlock())
	, m_ignoreInternalVariableChange(false)
{
}

//////////////////////////////////////////////////////////////////////////
CTagSelectionControl::~CTagSelectionControl()
{
}

//////////////////////////////////////////////////////////////////////////
BOOL CTagSelectionControl::OnInitDialog()
{
	__super::OnInitDialog();

	CRect rc;
	GetClientRect(rc);
	m_propertyControl.Create(WS_CHILD | WS_VISIBLE, rc, this, IDC_PROPERTIES_CONTROL);
	m_propertyControl.ModifyStyleEx(0, WS_EX_CLIENTEDGE);
	m_propertyControl.AddVarBlock(m_pVarBlock);

	Reset();

	SetResize(IDC_PROPERTIES_CONTROL, SZ_TOP_LEFT, SZ_BOTTOM_RIGHT);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CTagSelectionControl::SetTagDef(const CTagDefinition* pTagDef)
{
	Reset();

	m_pTagDef = pTagDef;
	if (m_pTagDef == NULL)
	{
		return;
	}

	const int numTags = m_pTagDef->GetNum();
	const int numGroups = m_pTagDef->GetNumGroups();

	for (int groupID = 0; groupID < numGroups; groupID++)
	{
		CSmartVariableEnum<int> enumList;
		const char* groupName = m_pTagDef->GetGroupName(groupID);
		enumList->SetName(groupName);

		enumList->AddEnumItem("", -1);

		for (int tagID = 0; tagID < numTags; tagID++)
		{
			const int tagGroupId = m_pTagDef->GetGroupID(tagID);
			if (tagGroupId == groupID)
			{
				const char* tagName = m_pTagDef->GetTagName(tagID);
				enumList->AddEnumItem(tagName, tagID);
			}
		}

		enumList->Set(-1);
		enumList->AddOnSetCallback(functor(*this, &CTagSelectionControl::OnInternalVariableChange));

		CVariableBase& var = *enumList.GetVar();
		var.SetUserData(reinterpret_cast<void*>(static_cast<intptr_t>(groupID)));

		m_tagGroupList.push_back(enumList);

		m_pVarBlock->AddVariable(&var);
	}

	for (int tagID = 0; tagID < numTags; tagID++)
	{
		const int tagGroupID = m_pTagDef->GetGroupID(tagID);
		if (tagGroupID < 0)
		{
			const char* tagName = m_pTagDef->GetTagName(tagID);

			CSmartVariable<bool> tagVar;
			tagVar->SetName(tagName);

			tagVar->SetDataType(IVariable::DT_SIMPLE);
			tagVar->Set(false);
			tagVar->AddOnSetCallback(functor(*this, &CTagSelectionControl::OnInternalVariableChange));

			tagVar->SetUserData(reinterpret_cast<void*>(static_cast<intptr_t>(tagID)));

			m_tagVarList.push_back(tagVar);

			m_pVarBlock->AddVariable(tagVar.GetVar());
		}
	}

	m_propertyControl.ReplaceVarBlock(m_propertyControl.GetRootItem(), m_pVarBlock);
}

//////////////////////////////////////////////////////////////////////////
const CTagDefinition* CTagSelectionControl::GetTagDef() const
{
	return m_pTagDef;
}

//////////////////////////////////////////////////////////////////////////
CVarBlockPtr CTagSelectionControl::GetVarBlock() const
{
	return m_pVarBlock;
}

//////////////////////////////////////////////////////////////////////////
TagState CTagSelectionControl::GetTagState() const
{
	if (m_pTagDef == NULL)
	{
		return TAG_STATE_EMPTY;
	}

	TagState state = TAG_STATE_EMPTY;

	const size_t numGroupVars = m_tagGroupList.size();
	for (size_t g = 0; g < numGroupVars; g++)
	{
		CVariableBase& var = *m_tagGroupList[g];
		int tagID = -1;
		var.Get(tagID);

		if (0 <= tagID)
		{
			m_pTagDef->Set(state, tagID, true);
		}
	}

	const size_t numTagVars = m_tagVarList.size();
	for (size_t i = 0; i < numTagVars; i++)
	{
		CVariableBase& var = *m_tagVarList[i];
		bool isSet = false;
		var.Get(isSet);

		const int tagID = static_cast<int>(reinterpret_cast<intptr_t>(var.GetUserData()));
		m_pTagDef->Set(state, tagID, isSet);
	}

	return state;
}

//////////////////////////////////////////////////////////////////////////
void CTagSelectionControl::SetTagState(const TagState tagState)
{
	if (m_pTagDef == NULL)
	{
		return;
	}

	const TagState oldTagState = GetTagState();
	assert(m_ignoreInternalVariableChange == false);
	m_ignoreInternalVariableChange = true;

	const uint numTags = m_pTagDef->GetNum();

	const size_t numGroupVars = m_tagGroupList.size();
	for (size_t g = 0; g < numGroupVars; g++)
	{
		CVariableBase& var = *m_tagGroupList[g];
		const int groupID = static_cast<int>(reinterpret_cast<intptr_t>(var.GetUserData()));

		int value = -1;
		for (int i = 0; i < numTags; i++)
		{
			const int tagGroupID = m_pTagDef->GetGroupID(i);
			const bool isSet = m_pTagDef->IsSet(tagState, i);
			if (groupID == tagGroupID && isSet)
			{
				value = i;
				break;
			}
		}

		var.Set(value);
	}

	const size_t numTagVars = m_tagVarList.size();
	for (size_t i = 0; i < numTagVars; i++)
	{
		CVariableBase& var = *m_tagVarList[i];
		const int tagID = static_cast<int>(reinterpret_cast<intptr_t>(var.GetUserData()));
		const bool isSet = m_pTagDef->IsSet(tagState, tagID);
		var.Set(isSet);
	}

	m_ignoreInternalVariableChange = false;
	const TagState newTagState = GetTagState();

	if (oldTagState != newTagState)
	{
		OnTagStateChange();
	}
}

//////////////////////////////////////////////////////////////////////////
void CTagSelectionControl::Reset()
{
	assert(m_pVarBlock);

	m_pVarBlock->DeleteAllVariables();

	m_tagVarList.clear();
	m_tagGroupList.clear();
	m_pTagDef = NULL;
}

//////////////////////////////////////////////////////////////////////////
void CTagSelectionControl::OnInternalVariableChange(IVariable* pVar)
{
	if (m_ignoreInternalVariableChange)
	{
		return;
	}

	OnTagStateChange();
}

//////////////////////////////////////////////////////////////////////////
void CTagSelectionControl::OnTagStateChange()
{
	if (m_onTagStateChangeCallback)
	{
		m_onTagStateChangeCallback(this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTagSelectionControl::SetOnTagStateChangeCallback(OnTagStateChangeCallback onTagStateChangeCallback)
{
	m_onTagStateChangeCallback = onTagStateChangeCallback;
}

