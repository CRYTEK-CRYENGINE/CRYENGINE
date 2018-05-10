// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PropertyCtrlExt.h"

#include "Objects/EntityObject.h"
#include "Objects/SelectionGroup.h"
#include "Objects/ObjectManager.h"
#include "Controls/PropertyItem.h"
#include "VehicleData.h"
#include "VehicleXMLHelper.h"

// CPropertyCtrlExt

IMPLEMENT_DYNAMIC(CPropertyCtrlExt, CPropertyCtrl)

BEGIN_MESSAGE_MAP(CPropertyCtrlExt, CPropertyCtrl)
ON_WM_RBUTTONUP()
END_MESSAGE_MAP()

enum PropertyCtrlExContextual
{
	PCEC_USER_CANCELLED = 0,
	PCEC_ADD_ARRAY_ELEMENT,
	PCEC_REMOVE_ARRAY_ELEMENT,
	PCEC_GET_EFFECT,
	PCEC_REMOVE_OPTIONAL_VAR,
	PCEC_CREATE_OPTIONAL_VAR_FIRST,
	PCEC_CREATE_OPTIONAL_VAR_LAST = PCEC_CREATE_OPTIONAL_VAR_FIRST + 100,
};

CPropertyCtrlExt::CPropertyCtrlExt()
	: CPropertyCtrl()
	, m_pVehicleVar(NULL)
{
	m_preSelChangeFunc = 0;
}

void CPropertyCtrlExt::OnRButtonUp(UINT nFlags, CPoint point)
{
	CPropertyItem* pItem = NULL;
	if (m_multiSelectedItems.size() == 1)
	{
		pItem = m_multiSelectedItems[0];
	}
	else
	{
		pItem = GetRootItem();
		if (0 < pItem->GetChildCount())
		{
			// The root item doesn't have a variable!
			pItem = pItem->GetChild(0);
		}
	}

	if (pItem == NULL)
	{
		return;
	}

	IVariable* pVar = pItem->GetVariable();
	if (pVar == NULL)
	{
		return;
	}

	CMenu menu;
	menu.CreatePopupMenu();

	if (pVar->GetDataType() == IVariable::DT_EXTARRAY)
	{
		if (pVar->GetUserData() != NULL)
		{
			string name("Add '");
			name += static_cast<IVariable*>(pVar->GetUserData())->GetName();
			name += "'";
			menu.AppendMenu(MF_STRING, PCEC_ADD_ARRAY_ELEMENT, name);
		}
	}
	else if (pItem->GetParent() && pItem->GetParent()->GetVariable()
	         && pItem->GetParent()->GetVariable()->GetDataType() == IVariable::DT_EXTARRAY)
	{
		// if clicked on child of extendable array element
		string name("Delete '");
		name += pVar->GetName();
		name += "'";
		menu.AppendMenu(MF_STRING, PCEC_REMOVE_ARRAY_ELEMENT, name);
	}
	else if (strcmpi(pVar->GetName(), "effect") == 0)
	{
		menu.AppendMenu(MF_STRING, PCEC_GET_EFFECT, "Get effect from selection");
	}

	XmlNodeRef nodeDefinition = VehicleXml::GetXmlNodeDefinitionByVariable(CVehicleData::GetXMLDef(), m_pVehicleVar, pVar);
	if (!nodeDefinition)
	{
		return;
	}

	if (VehicleXml::IsOptionalNode(nodeDefinition))
	{
		if (!VehicleXml::IsArrayElementNode(nodeDefinition, pVar->GetName()))
		{
			string name("Delete '");
			name += pVar->GetName();
			name += "'";
			menu.AppendMenu(MF_STRING, PCEC_REMOVE_OPTIONAL_VAR, name);
		}
	}

	if (menu.GetMenuItemCount() != 0)
	{
		// TODO: only add separator if will add elements after that...
		menu.AppendMenu(MF_SEPARATOR);
	}

	std::vector<XmlNodeRef> childDefinitions;
	if (pVar->GetDataType() != IVariable::DT_EXTARRAY)
	{
		VehicleXml::GetXmlNodeChildDefinitionsByVariable(CVehicleData::GetXMLDef(), m_pVehicleVar, pVar, childDefinitions);
		for (int i = 0; i < childDefinitions.size(); ++i)
		{
			XmlNodeRef childProperty = childDefinitions[i];
			if (VehicleXml::IsOptionalNode(childProperty))
			{
				string propertyName = VehicleXml::GetNodeName(childProperty);
				IVariable* pChildVar = GetChildVar(pVar, propertyName);
				bool alreadyHasChildProperty = (pChildVar != NULL);
				int contextOptionId = PCEC_CREATE_OPTIONAL_VAR_FIRST + i;

				bool allowCreation = true;
				allowCreation &= !alreadyHasChildProperty;
				allowCreation &= (contextOptionId <= PCEC_CREATE_OPTIONAL_VAR_LAST);
				allowCreation &= !VehicleXml::IsDeprecatedNode(childProperty);

				if (allowCreation)
				{
					string optionName("Add '");
					optionName += propertyName;
					optionName += "'";
					menu.AppendMenu(MF_STRING, contextOptionId, optionName);
				}
			}
		}
	}

	CPoint p;
	::GetCursorPos(&p);
	int res = ::TrackPopupMenuEx(menu.GetSafeHmenu(), TPM_LEFTBUTTON | TPM_RETURNCMD, p.x, p.y, GetSafeHwnd(), NULL);
	switch (res)
	{
	case PCEC_ADD_ARRAY_ELEMENT:
		OnAddChild();
		break;
	case PCEC_REMOVE_ARRAY_ELEMENT:
		OnDeleteChild(pItem);
		break;
	case PCEC_GET_EFFECT:
		OnGetEffect(pItem);
		break;
	case PCEC_REMOVE_OPTIONAL_VAR:
		OnDeleteChild(pItem);
		break;
	}

	if (PCEC_CREATE_OPTIONAL_VAR_FIRST <= res && res < PCEC_CREATE_OPTIONAL_VAR_FIRST + childDefinitions.size() && res <= PCEC_CREATE_OPTIONAL_VAR_LAST)
	{
		IVariablePtr pItemVar = pItem->GetVariable();
		int index = res - PCEC_CREATE_OPTIONAL_VAR_FIRST;

		XmlNodeRef node = childDefinitions[index];
		IVariablePtr pChildVar = VehicleXml::CreateDefaultVar(CVehicleData::GetXMLDef(), node, VehicleXml::GetNodeName(node));

		if (pChildVar)
		{
			pItemVar->AddVariable(pChildVar);
			// Make sure the controls are completely rebuilt if we've created a variable.
			ReloadItem(pItem);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlExt::ReloadItem(CPropertyItem* pItem)
{
	IVariablePtr pItemVar = pItem->GetVariable();
	if (!pItemVar)
	{
		return;
	}

	CVarBlockPtr pVarBlock(new CVarBlock());
	for (int i = 0; i < pItemVar->GetNumVariables(); ++i)
	{
		pVarBlock->AddVariable(pItemVar->GetVariable(i));
	}

	ReplaceVarBlock(pItem, pVarBlock);
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlExt::OnAddChild()
{
	if (!m_multiSelectedItems.size() == 1)
		return;

	CPropertyItem* pItem = m_multiSelectedItems[0];
	IVariable* pVar = pItem->GetVariable();
	IVariable* pClone = 0;

	if (pVar->GetUserData())
	{
		pClone = static_cast<IVariable*>(pVar->GetUserData())->Clone(true);

		pVar->AddVariable(pClone);

		CPropertyItemPtr pNewItem = new CPropertyItem(this);
		pItem->AddChild(pNewItem);
		pNewItem->SetVariable(pClone);

		Invalidate();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlExt::OnDeleteChild(CPropertyItem* pItem)
{
	// remove item and variable
	CPropertyItem* pParent = pItem->GetParent();
	IVariable* pVar = pItem->GetVariable();

	pItem->DestroyInPlaceControl();
	pParent->RemoveChild(pItem);
	pParent->GetVariable()->DeleteVariable(pVar);

	ClearSelection();

	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlExt::SelectItem(CPropertyItem* item)
{
	// call PreSelect callback
	if (m_preSelChangeFunc)
		m_preSelChangeFunc(item);

	CPropertyCtrl::SelectItem(item);
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlExt::OnItemChange(CPropertyItem* item)
{
	CPropertyCtrl::OnItemChange(item);

	// handle class-like items that need to insert/remove subitems
	if (item->GetType() == ePropertySelection && item->GetName() == "class")
	{
		IVariable* pVar = item->GetVariable();

		string classname;
		pVar->Get(classname);

		// get parent var/item of class item
		CPropertyItem* pClassItem = item;
		if (!pClassItem)
			return;

		CPropertyItem* pParentItem = pClassItem->GetParent();

		assert(pParentItem);
		if (!pParentItem)
			return;

		if (IVariablePtr pMainVar = pParentItem->GetVariable())
		{
			// first remove child vars that belong to other classes
			if (IVarEnumList* pEnumList = pVar->GetEnumList())
			{
				CPropertyItemPtr pSubItem = 0;

				for (uint i = 0; cstr sEnumName = pEnumList->GetItemName(i); i++)
				{
					// if sub variables for enum classes are found
					// delete their property items and variables
					if (IVariable* pSubVar = GetChildVar(pMainVar, sEnumName))
					{
						pSubItem = pParentItem->FindItemByVar(pSubVar);
						if (pSubItem)
						{
							pParentItem->RemoveChild(pSubItem);
							pMainVar->DeleteVariable(pSubVar);
						}
					}
				}

				// now check if we have extra properties for that part class
				if (IVariable* pSubProps = CreateDefaultVar(classname, true))
				{
					// add new PropertyItem
					pSubItem = new CPropertyItem(this);
					pParentItem->AddChild(pSubItem);
					pSubItem->SetVariable(pSubProps);

					pMainVar->AddVariable(pSubProps);

					pSubItem->SetExpanded(true);
				}
				Invalidate();
			}
		}

	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyCtrlExt::OnGetEffect(CPropertyItem* pItem)
{
	const CSelectionGroup* pSel = GetIEditor()->GetObjectManager()->GetSelection();
	for (int i = 0; i < pSel->GetCount(); ++i)
	{
		CBaseObject* pObject = pSel->GetObject(i);
		if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* pEntity = (CEntityObject*)pObject;
			if (pEntity->GetProperties())
			{
				IVariable* pVar = pEntity->GetProperties()->FindVariable("ParticleEffect");
				if (pVar)
				{
					string effect;
					pVar->Get(effect);

					pItem->SetValue(effect);

					break;
				}
			}
		}
	}
}

