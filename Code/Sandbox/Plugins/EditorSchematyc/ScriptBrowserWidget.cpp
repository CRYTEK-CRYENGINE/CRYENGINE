// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ScriptBrowserWidget.h"

#include "ScriptUndo.h"
#include "MainWindow.h"

#include <QBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QSplitter>
#include <QToolbar>
#include <QWidgetAction>

#include <QPushButton.h>
#include <QSearchBox.h>
#include <QAdvancedTreeView.h>
#include <AssetSystem/AssetEditor.h>

#include <CrySchematyc/Compiler/ICompiler.h>
#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/Elements/IEnvClass.h>
#include <CrySchematyc/Script/IScript.h>
#include <CrySchematyc/Script/IScriptRegistry.h>
#include <CrySchematyc/Script/Elements/IScriptActionInstance.h>
#include <CrySchematyc/Script/Elements/IScriptClass.h>
#include <CrySchematyc/Script/Elements/IScriptComponentInstance.h>
#include <CrySchematyc/Script/Elements/IScriptEnum.h>
#include <CrySchematyc/Script/Elements/IScriptFunction.h>
#include <CrySchematyc/Script/Elements/IScriptInterface.h>
#include <CrySchematyc/Script/Elements/IScriptInterfaceFunction.h>
#include <CrySchematyc/Script/Elements/IScriptInterfaceImpl.h>
#include <CrySchematyc/Script/Elements/IScriptInterfaceTask.h>
#include <CrySchematyc/Script/Elements/IScriptModule.h>
#include <CrySchematyc/Script/Elements/IScriptSignal.h>
#include <CrySchematyc/Script/Elements/IScriptSignalReceiver.h>
#include <CrySchematyc/Script/Elements/IScriptState.h>
#include <CrySchematyc/Script/Elements/IScriptStateMachine.h>
#include <CrySchematyc/Script/Elements/IScriptStruct.h>
#include <CrySchematyc/Script/Elements/IScriptTimer.h>
#include <CrySchematyc/Script/Elements/IScriptVariable.h>
#include <CrySchematyc/SerializationUtils/ISerializationContext.h>
#include <CrySchematyc/SerializationUtils/IValidatorArchive.h>
#include <CrySchematyc/Utils/StackString.h>
#include <Serialization/Qt.h>

#include "PluginUtils.h"
#include <CryIcon.h>

#include <ProxyModels/DeepFilterProxyModel.h>
#include <EditorFramework/BroadcastManager.h>
#include <Controls/QuestionDialog.h>

namespace Schematyc
{
namespace
{
static const char* g_szAddIcon = "icons:schematyc/add.png";
static const char* g_szRemoveIcon = "icons:General/Remove_Item.ico";
static const char* g_szHomeIcon = "icons:schematyc/home.png";
static const char* g_szBackIcon = "icons:schematyc/back.png";
static const char* g_szForwardIcon = "icons:schematyc/forward.png";
static const char* g_szFileNewIcon = "icons:schematyc/file_new.png";
static const char* g_szFileLocalIcon = "icons:schematyc/file_local.png";
static const char* g_szFileManagedIcon = "icons:schematyc/file_managed.png";
static const char* g_szFileCheckedOutIcon = "icons:schematyc/file_checked_out.png";
static const char* g_szFileAddIcon = "icons:schematyc/file_add.png";
static const char* g_szScriptRootIcon = "icons:schematyc/script_module.png";

const QSize g_defaultColumnSize[EScriptBrowserColumn::COUNT] = { QSize(100, 20), QSize(20, 20) };

const char* g_szClipboardPrefix = "[schematyc_xml]";
}

const CItemModelAttribute CScriptBrowserModel::s_columnAttributes[] =
{
	CItemModelAttribute("Name",            eAttributeType_String, CItemModelAttribute::Visible,      false, ""),
	CItemModelAttribute("_sort_string_",   eAttributeType_String, CItemModelAttribute::AlwaysHidden, false, ""),
	CItemModelAttribute("_filter_string_", eAttributeType_String, CItemModelAttribute::AlwaysHidden, false, "")
};

class CScriptElementFilterProxyModel : public QDeepFilterProxyModel
{
	using QDeepFilterProxyModel::QDeepFilterProxyModel;

	virtual bool lessThan(const QModelIndex& lhs, const QModelIndex& rhs) const override
	{
		CScriptBrowserItem* pLeft = reinterpret_cast<CScriptBrowserItem*>(lhs.data(CScriptBrowserModel::PointerRole).value<quintptr>());
		CScriptBrowserItem* pRight = reinterpret_cast<CScriptBrowserItem*>(rhs.data(CScriptBrowserModel::PointerRole).value<quintptr>());
		if (pLeft && pRight && pLeft->GetType() != pRight->GetType())
		{
			return pLeft->GetType() == EScriptBrowserItemType::Filter;
		}

		return QSortFilterProxyModel::lessThan(lhs, rhs);
	}
};

CScriptBrowserItem::CScriptBrowserItem(EScriptBrowserItemType type, const char* szName, const char* szIcon)
	: m_type(type)
	, m_name(szName)
	, m_iconName(szIcon)
	, m_filterType(EFilterType::None)
	, m_pScriptElement(nullptr)
	, m_pParent(nullptr)
{

}

CScriptBrowserItem::CScriptBrowserItem(EScriptBrowserItemType type, const char* szName, EFilterType filterType)
	: m_type(type)
	, m_name(szName)
	, m_filterType(filterType)
	, m_pScriptElement(nullptr)
	, m_pParent(nullptr)
{

}

CScriptBrowserItem::CScriptBrowserItem(EScriptBrowserItemType type, const char* szName, const char* szIcon, IScriptElement* pScriptElement)
	: m_type(type)
	, m_name(szName)
	, m_iconName(szIcon)
	, m_filterType(EFilterType::None)
	, m_pScriptElement(pScriptElement)
	, m_pParent(nullptr)
{

}

EScriptBrowserItemType CScriptBrowserItem::GetType() const
{
	return m_type;
}

void CScriptBrowserItem::SetName(const char* szName)
{
	if (m_pScriptElement)
	{
		if (ScriptBrowserUtils::RenameScriptElement(*m_pScriptElement, szName))
		{
			m_name = szName;
		}
	}
}

const char* CScriptBrowserItem::GetName() const
{
	return m_name.c_str();
}

const char* CScriptBrowserItem::GetPath() const
{
	return m_path.c_str();
}

const char* CScriptBrowserItem::GetIcon() const
{
	return m_iconName.c_str();
}

const char* CScriptBrowserItem::GetTooltip() const
{
	return m_tooltip.c_str();
}

const char* CScriptBrowserItem::GetSortString() const
{
	return m_sortString.c_str();
}

void CScriptBrowserItem::SetSortString(const char* szSort)
{
	m_sortString = szSort;
}

const char* CScriptBrowserItem::GetFilterString() const
{
	return m_filterString.c_str();
}

void CScriptBrowserItem::SetFilterString(const char* szFilter)
{
	m_filterString = szFilter;
}

void CScriptBrowserItem::SetFlags(const ScriptBrowserItemFlags& flags)
{
	m_flags = flags;
}

ScriptBrowserItemFlags CScriptBrowserItem::GetFlags() const
{
	return m_flags;
}

EFilterType CScriptBrowserItem::GetFilterType() const
{
	return m_filterType;
}

void CScriptBrowserItem::SetFilterType(EFilterType filterType)
{
	m_filterType = filterType;
}

const char* CScriptBrowserItem::GetFileText() const
{
	if (m_flags.Check(EScriptBrowserItemFlags::FileLocal))
	{
		if (m_flags.Check(EScriptBrowserItemFlags::FileManaged))
		{
			if (m_flags.Check(EScriptBrowserItemFlags::FileAdd))
			{
				return "SourceControl | Add";
			}
			else if (m_flags.Check(EScriptBrowserItemFlags::FileCheckedOut))
			{
				return "SourceControl | Checked Out";
			}
			else
			{
				return "SourceControl";
			}
		}
		else if (m_flags.Check(EScriptBrowserItemFlags::FileReadOnly))
		{
			return "Local | Read Only";
		}
		return "Local";
	}
	else if (m_flags.Check(EScriptBrowserItemFlags::FileInPak))
	{
		return "In Pak";
	}
	return "";
}

const char* CScriptBrowserItem::GetFileIcon() const
{
	IScript* pScript = GetScript();
	if (pScript)
	{
		if (m_flags.Check(EScriptBrowserItemFlags::FileLocal))
		{
			if (m_flags.Check(EScriptBrowserItemFlags::FileManaged))
			{
				if (m_flags.Check(EScriptBrowserItemFlags::FileCheckedOut))
				{
					if (m_flags.Check(EScriptBrowserItemFlags::FileAdd))
					{
						return g_szFileAddIcon;
					}
					else
					{
						return g_szFileCheckedOutIcon;
					}
				}
				else
				{
					return g_szFileManagedIcon;
				}
			}
			else
			{
				return g_szFileLocalIcon;
			}
		}
		else
		{
			return g_szFileNewIcon;
		}
	}
	return "";
}

QVariant CScriptBrowserItem::GetColor() const
{
	if (m_flags.CheckAny({ EScriptBrowserItemFlags::ContainsErrors, EScriptBrowserItemFlags::ChildContainsErrors }))
	{
		return QColor(Qt::red);
	}
	else if (m_flags.CheckAny({ EScriptBrowserItemFlags::ContainsWarnings, EScriptBrowserItemFlags::ChildContainsWarnings }))
	{
		return QColor(Qt::yellow);
	}
	return QVariant();
}

IScriptElement* CScriptBrowserItem::GetScriptElement() const
{
	return m_pScriptElement;
}

IScript* CScriptBrowserItem::GetScript() const
{
	return m_pScriptElement ? m_pScriptElement->GetScript() : nullptr;
}

CScriptBrowserItem* CScriptBrowserItem::GetParent()
{
	return m_pParent;
}

void CScriptBrowserItem::AddChild(const CScriptBrowserItemPtr& pChild)
{
	m_children.push_back(pChild);
	pChild->m_pParent = this;
}

CScriptBrowserItemPtr CScriptBrowserItem::RemoveChild(CScriptBrowserItem* pChild)
{
	for (Items::iterator itChild = m_children.begin(), itEndChild = m_children.end(); itChild != itEndChild; ++itChild)
	{
		if (itChild->get() == pChild)
		{
			CScriptBrowserItemPtr erasedItem = *itChild;
			m_children.erase(itChild);
			return erasedItem;
		}
	}
	return CScriptBrowserItemPtr();
}

int CScriptBrowserItem::GetChildCount() const
{
	return static_cast<int>(m_children.size());
}

int CScriptBrowserItem::GetChildIdx(CScriptBrowserItem* pChild)
{
	for (int childIdx = 0, childCount = m_children.size(); childIdx < childCount; ++childIdx)
	{
		if (m_children[childIdx].get() == pChild)
		{
			return childIdx;
		}
	}
	return -1;
}

CScriptBrowserItem* CScriptBrowserItem::GetChildByIdx(int childIdx)
{
	return childIdx < m_children.size() ? m_children[childIdx].get() : nullptr;
}

CScriptBrowserItem* CScriptBrowserItem::GetChildByTypeAndName(EScriptBrowserItemType type, const char* szName)
{
	if (szName)
	{
		for (const CScriptBrowserItemPtr& pChild : m_children)
		{
			if ((pChild->GetType() == type) && (strcmp(pChild->GetName(), szName) == 0))
			{
				return pChild.get();
			}
		}
	}
	return nullptr;
}

void CScriptBrowserItem::Validate()
{
	if (m_pScriptElement)
	{
		m_tooltip.clear();

		IValidatorArchivePtr pArchive = gEnv->pSchematyc->CreateValidatorArchive(SValidatorArchiveParams());
		ISerializationContextPtr pSerializationContext = gEnv->pSchematyc->CreateSerializationContext(SSerializationContextParams(*pArchive, ESerializationPass::Validate));
		m_pScriptElement->Serialize(*pArchive);

		ScriptBrowserItemFlags flags = m_flags;
		if (pArchive->GetWarningCount())
		{
			auto collectWarnings = [this](EValidatorMessageType messageType, const char* szMessage)
			{
				if (messageType == EValidatorMessageType::Warning)
				{
					m_tooltip.append("Warning: ");
					m_tooltip.append(szMessage);
					m_tooltip.append("\n");
				}
			};
			pArchive->Validate(collectWarnings);

			flags.Add(EScriptBrowserItemFlags::ContainsWarnings);
		}
		else
		{
			flags.Remove(EScriptBrowserItemFlags::ContainsWarnings);
		}

		if (pArchive->GetErrorCount())
		{
			auto collectErrors = [this](EValidatorMessageType messageType, const char* szMessage)
			{
				if (messageType == EValidatorMessageType::Error)
				{
					m_tooltip.append("Error: ");
					m_tooltip.append(szMessage);
					m_tooltip.append("\n");
				}
			};
			pArchive->Validate(collectErrors);

			flags.Add(EScriptBrowserItemFlags::ContainsErrors);
		}
		else
		{
			flags.Remove(EScriptBrowserItemFlags::ContainsErrors);
		}

		m_flags = flags;
	}

	RefreshChildFlags();
}

void CScriptBrowserItem::RefreshChildFlags()
{
	m_flags.Remove(EScriptBrowserItemFlags::ChildMask);

	for (const CScriptBrowserItemPtr& pChild : m_children)
	{
		const ScriptBrowserItemFlags childFlags = pChild->GetFlags();
		if (childFlags.CheckAny({ EScriptBrowserItemFlags::ContainsWarnings, EScriptBrowserItemFlags::ChildContainsWarnings }))
		{
			m_flags.Add(EScriptBrowserItemFlags::ChildContainsWarnings);
		}
		if (childFlags.CheckAny({ EScriptBrowserItemFlags::ContainsErrors, EScriptBrowserItemFlags::ChildContainsErrors }))
		{
			m_flags.Add(EScriptBrowserItemFlags::ChildContainsErrors);
		}
	}
}

CScriptBrowserModel::CScriptBrowserModel(CrySchematycEditor::CMainWindow& editor, CAsset& asset, const CryGUID& assetGUID)
	: QAbstractItemModel(&editor)
	, m_editor(editor)
	, m_assetGUID(assetGUID)
	, m_asset(asset)
{
	gEnv->pSchematyc->GetScriptRegistry().GetChangeSignalSlots().Connect(SCHEMATYC_MEMBER_DELEGATE(&CScriptBrowserModel::OnScriptRegistryChange, *this), m_connectionScope);

	Populate();
}

CScriptBrowserModel::~CScriptBrowserModel()
{
	gEnv->pSchematyc->GetScriptRegistry().GetChangeSignalSlots().Disconnect(m_connectionScope);
}

QModelIndex CScriptBrowserModel::index(int row, int column, const QModelIndex& parent) const
{
	if (parent.isValid())
	{
		CScriptBrowserItem* pParentItem = ItemFromIndex(parent);
		if (pParentItem)
		{
			CScriptBrowserItem* pChildItem = pParentItem->GetChildByIdx(row);
			return pChildItem ? QAbstractItemModel::createIndex(row, column, pChildItem) : QModelIndex();
		}
	}
	else if (row < m_filterItems.size())
	{
		return QAbstractItemModel::createIndex(row, column, m_filterItems[row].get());
	}

	return QModelIndex();
}

QModelIndex CScriptBrowserModel::parent(const QModelIndex& index) const
{
	CScriptBrowserItem* pItem = ItemFromIndex(index);
	if (pItem)
	{
		CScriptBrowserItem* pParentItem = pItem->GetParent();
		if (pParentItem)
		{
			return ItemToIndex(pParentItem, index.column());
		}
	}
	return QModelIndex();
}

int CScriptBrowserModel::rowCount(const QModelIndex& parent) const
{
	if (parent.isValid())
	{
		CScriptBrowserItem* pParentItem = ItemFromIndex(parent);
		if (pParentItem)
		{
			return pParentItem->GetChildCount();
		}
	}

	return m_filterItems.size();
}

int CScriptBrowserModel::columnCount(const QModelIndex& parent) const
{
	return EScriptBrowserColumn::COUNT;
}

bool CScriptBrowserModel::hasChildren(const QModelIndex& parent) const
{
	if (parent.isValid())
	{
		CScriptBrowserItem* pParentItem = ItemFromIndex(parent);
		if (pParentItem)
		{
			return pParentItem->GetChildCount() > 0;
		}
	}

	return m_filterItems.size() > 0;
}

QVariant CScriptBrowserModel::data(const QModelIndex& index, int role) const
{
	CScriptBrowserItem* pItem = ItemFromIndex(index);
	if (pItem)
	{
		switch (role)
		{
		case ERole::DisplayRole:
			{
				switch (index.column())
				{
				case EScriptBrowserColumn::Name:
					{
						QString name = pItem->GetName();
						if (pItem->GetFlags().Check(EScriptBrowserItemFlags::Modified))
						{
							name.append("*");
						}
						return name;
					}
				case EScriptBrowserColumn::Sort:
					{
						return QString(pItem->GetSortString());
					}
				case EScriptBrowserColumn::Filter:
					{
						return QString(pItem->GetName());
					}
				}
				break;
			}
		case ERole::DecorationRole:
			{
				switch (index.column())
				{
				case EScriptBrowserColumn::Name:
					{
						return CryIcon(pItem->GetIcon());
					}
				}
				break;
			}
		case ERole::EditRole:
			{
				switch (index.column())
				{
				case EScriptBrowserColumn::Name:
					{
						return QString(pItem->GetName());
					}
				}
				break;
			}
		case ERole::ToolTipRole:
			{
				switch (index.column())
				{
				case EScriptBrowserColumn::Name:
					{
						return QString(pItem->GetTooltip());
					}
				}
				break;
			}
		case ERole::ForegroundRole:
			{
				switch (index.column())
				{
				case EScriptBrowserColumn::Name:
					{
						return pItem->GetColor();
					}
				}
				break;
			}
		case ERole::SizeHintRole:
			{
				return g_defaultColumnSize[index.column()];
			}
		case ERole::PointerRole:
			{
				return QVariant::fromValue(reinterpret_cast<quintptr>(pItem));
			}
		}
	}
	return QVariant();
}

bool CScriptBrowserModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	if (role == Qt::EditRole)
	{
		CScriptBrowserItem* pItem = ItemFromIndex(index);
		if (pItem)
		{
			const char* szNewName = value.toString().toStdString().c_str();
			stack_string desc("Renamed '");
			desc.append(pItem->GetName());
			desc.append("' to '");
			desc.append(szNewName);
			desc.append("'");

			GetIEditor()->GetIUndoManager()->Begin();
			CrySchematycEditor::CScriptUndoObject* pUndoObject = new CrySchematycEditor::CScriptUndoObject(desc.c_str(), m_editor);
			CUndo::Record(pUndoObject);
			GetIEditor()->GetIUndoManager()->Accept(pUndoObject->GetDescription());

			pItem->SetName(value.toString().toStdString().c_str());
			return true;
		}
	}
	return false;
}

QVariant CScriptBrowserModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	return GetHeaderData(section, orientation, role);
}

Qt::ItemFlags CScriptBrowserModel::flags(const QModelIndex& index) const
{
	Qt::ItemFlags flags = QAbstractItemModel::flags(index);
	CScriptBrowserItem* pItem = reinterpret_cast<CScriptBrowserItem*>(index.data(ERole::PointerRole).value<quintptr>());
	if (pItem)
	{
		switch (pItem->GetType())
		{
		case EScriptBrowserItemType::Filter:
			{
				flags &= ~Qt::ItemIsEnabled;
				break;
			}
		case EScriptBrowserItemType::ScriptElement:
			{
				IScriptElement* pScriptElement = pItem->GetScriptElement();
				if (pScriptElement && ScriptBrowserUtils::CanRenameScriptElement(*pScriptElement))
				{
					flags |= Qt::ItemIsEditable;
				}
				break;
			}
		}
	}
	return flags;
}

QVariant CScriptBrowserModel::GetHeaderData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal)
	{
		return QVariant();
	}

	const CItemModelAttribute* pAttribute = &s_columnAttributes[section];
	if (pAttribute)
	{
		if (role == Qt::DisplayRole || role == Qt::ToolTipRole)
		{
			return pAttribute->GetName();
		}
		else if (role == Attributes::s_getAttributeRole)
		{
			return QVariant::fromValue(const_cast<CItemModelAttribute*>(pAttribute));
		}
		else if (role == Attributes::s_attributeMenuPathRole)
		{
			return "";
		}
	}

	return QVariant();
}

void CScriptBrowserModel::Reset()
{
	m_filterItems.clear();
	m_itemsByGUID.clear();
	m_itemsByFileName.clear();
	m_pRootItem.reset();

	QAbstractItemModel::beginResetModel();
	{
		Populate();
	}
	QAbstractItemModel::endResetModel();
}

QModelIndex CScriptBrowserModel::ItemToIndex(CScriptBrowserItem* pItem, int column) const
{
	if (pItem && pItem != m_pRootItem.get())
	{
		CScriptBrowserItem* pParentItem = pItem->GetParent();
		if (pParentItem)
		{
			return QAbstractItemModel::createIndex(pParentItem->GetChildIdx(pItem), column, pItem);
		}
		else
		{
			return QAbstractItemModel::createIndex(0, column, pItem);
		}
	}
	return QModelIndex();
}

CScriptBrowserItem* CScriptBrowserModel::ItemFromIndex(const QModelIndex& index) const
{
	return static_cast<CScriptBrowserItem*>(index.internalPointer());
}

CScriptBrowserItem* CScriptBrowserModel::GetRootItem()
{
	return m_pRootItem.get();
}

CScriptBrowserItem* CScriptBrowserModel::GetItemByGUID(const CryGUID& guid)
{
	ItemsByGUID::iterator itItem = m_itemsByGUID.find(guid);
	return itItem != m_itemsByGUID.end() ? itItem->second : nullptr;
}

CScriptBrowserItem* CScriptBrowserModel::GetItemByFileName(const char* szFileName)
{
	SCHEMATYC_EDITOR_ASSERT(szFileName);
	if (szFileName)
	{
		ItemsByFileName::iterator itItem = m_itemsByFileName.find(szFileName);
		if (itItem != m_itemsByFileName.end())
		{
			return itItem->second;
		}
	}
	return nullptr;
}

Schematyc::IScriptElement* CScriptBrowserModel::GetRootElement()
{
	return m_pRootItem->GetScriptElement();
}

void CScriptBrowserModel::Populate()
{
	IScriptElement* pScripElement = gEnv->pSchematyc->GetScriptRegistry().GetElement(m_assetGUID);
	// TODO: Print asset guid.
	CRY_ASSERT_MESSAGE(pScripElement, "Couldn't find Schematyc asset!");
	// ~TODO
	if (pScripElement)
	{
		m_pRootItem = std::make_shared<CScriptBrowserItem>(EScriptBrowserItemType::ScriptElement, pScripElement->GetName(), nullptr, pScripElement);

		CScriptBrowserItem* pItemBase = nullptr;

		// Item
		auto visitScriptElement = [this, &pItemBase](IScriptElement& scriptElement) -> EVisitStatus
		{
			const IScriptElement* pParentScriptElement = scriptElement.GetParent();
			if (pParentScriptElement)
			{
				CScriptBrowserItem* pParentItem = GetItemByGUID(pParentScriptElement->GetGUID());
				CScriptBrowserItem* pItem = CreateScriptElementItem(scriptElement, EScriptBrowserItemFlags::None, pParentItem);

				if (scriptElement.GetType() == EScriptElementType::Base)
				{
					pItemBase = pItem;
					return EVisitStatus::Continue;
				}
			}
			return EVisitStatus::Recurse;
		};
		pScripElement->VisitChildren(visitScriptElement);

		// Base
		// TODO: Schematyc doesn't support to list components, variables etc. of derived classes yet.
		/*if (pItemBase)
		   {
		   auto visitScriptElementBase = [this, pItemBase](IScriptElement& scriptElement) -> EVisitStatus
		   {
		    const IScriptElement* pParentScriptElement = scriptElement.GetParent();
		    if (pParentScriptElement)
		    {
		      CScriptBrowserItem* pParentItem = GetItemByGUID(pParentScriptElement->GetGUID());
		      CreateScriptElementBaseItem(scriptElement, EScriptBrowserItemFlags::None, pParentItem, pItemBase);
		    }

		    return EVisitStatus::Recurse;
		   };
		   pItemBase->GetScriptElement()->VisitChildren(visitScriptElementBase);
		   }*/
		// ~TODO
	}
}

void CScriptBrowserModel::OnScriptRegistryChange(const SScriptRegistryChange& change)
{
	bool bRecompile = false;
	bool bValidate = true;

	// TODO: Instead of filtering here we should only listen to changes made in the current opened asset.
	std::function<bool(IScriptElement*)> isThisElement;
	isThisElement = [this, &isThisElement](IScriptElement* pScriptElement) -> bool
	{
		if (pScriptElement)
		{
			if (pScriptElement->GetType() == EScriptElementType::Class || pScriptElement->GetType() == EScriptElementType::Module)
			{
				if (pScriptElement == m_pRootItem->GetScriptElement())
					return true;
			}
			return isThisElement(pScriptElement->GetParent());
		}
		return false;
	};
	// ~TODO

	if (isThisElement(&change.element))
	{
		switch (change.type)
		{
		case EScriptRegistryChangeType::ElementAdded:
			{
				OnScriptElementAdded(change.element);
				bRecompile = false; // Happens in backend.
				break;
			}
		case EScriptRegistryChangeType::ElementModified:
			{
				OnScriptElementModified(change.element);
				bRecompile = true;
				break;
			}
		case EScriptRegistryChangeType::ElementRemoved:
			{
				OnScriptElementRemoved(change.element);
				bRecompile = false; // Happens in backend.
				break;
			}
		case EScriptRegistryChangeType::ElementSaved:
			{
				OnScriptElementSaved(change.element);
				bValidate = false;
				break;
			}
		}

		// TODO: Recompilation should not happen in editor code at all!
		if (bRecompile)
		{
			gEnv->pSchematyc->GetCompiler().CompileDependencies(change.element.GetGUID());
		}
		// ~TODO

		if (bValidate)
		{
			CScriptBrowserItem* pItem = GetItemByGUID(change.element.GetGUID());
			if (pItem)
			{
				ValidateItem(*pItem);
			}
		}
	}
}

void CScriptBrowserModel::OnScriptElementAdded(IScriptElement& scriptElement)
{
	IScriptElement* pParentScriptElement = scriptElement.GetParent();
	if (pParentScriptElement)
	{
		CScriptBrowserItem* pParentItem = GetItemByGUID(pParentScriptElement->GetGUID());
		CreateScriptElementItem(scriptElement, EScriptBrowserItemFlags::Modified, pParentItem);
	}
}

void CScriptBrowserModel::OnScriptElementModified(IScriptElement& scriptElement)
{
	CScriptBrowserItem* pItem = GetItemByGUID(scriptElement.GetGUID());
	while (pItem)
	{
		IScriptElement* pScriptElement = pItem->GetScriptElement();
		if (pScriptElement)
		{
			ScriptBrowserItemFlags itemFlags = pItem->GetFlags();
			itemFlags.Add(EScriptBrowserItemFlags::Modified);
			pItem->SetFlags(itemFlags);

			QModelIndex index = ItemToIndex(pItem);
			QAbstractItemModel::dataChanged(index, index);
		}

		if (!pScriptElement || !pScriptElement->GetScript())
		{
			pItem = pItem->GetParent();
		}
		else
		{
			break;
		}
	}
}

void CScriptBrowserModel::OnScriptElementRemoved(IScriptElement& scriptElement)
{
	ItemsByGUID::iterator itItem = m_itemsByGUID.find(scriptElement.GetGUID());
	if (itItem != m_itemsByGUID.end())
	{
		CScriptBrowserItem* pItem = itItem->second;
		CScriptBrowserItem* pParentItem = pItem->GetParent();

		RemoveItem(*pItem);
		m_itemsByGUID.erase(itItem);

		if (pParentItem && (pParentItem->GetType() == EScriptBrowserItemType::Filter) && !pParentItem->GetChildCount())
		{
			RemoveItem(*pParentItem);
		}
	}
}

void CScriptBrowserModel::OnScriptElementSaved(IScriptElement& scriptElement)
{
	ItemsByGUID::iterator itItem = m_itemsByGUID.find(scriptElement.GetGUID());
	if (itItem != m_itemsByGUID.end())
	{
		CScriptBrowserItem* pItem = itItem->second;

		ScriptBrowserItemFlags itemFlags = pItem->GetFlags();
		itemFlags.Remove(EScriptBrowserItemFlags::Modified);
		pItem->SetFlags(itemFlags);

		QModelIndex index = ItemToIndex(pItem);
		QAbstractItemModel::dataChanged(index, index);
	}
	else if (m_pRootItem->GetScriptElement()->GetGUID() == scriptElement.GetGUID())
	{
		ScriptBrowserItemFlags itemFlags = m_pRootItem->GetFlags();
		itemFlags.Remove(EScriptBrowserItemFlags::Modified);
		m_pRootItem->SetFlags(itemFlags);
	}
}

CScriptBrowserItem* CScriptBrowserModel::CreateScriptElementItem(IScriptElement& scriptElement, const ScriptBrowserItemFlags& flags, CScriptBrowserItem* pParentItem)
{
	const EScriptElementType scriptElementType = scriptElement.GetType();
	if (scriptElementType == EScriptElementType::Base)
	{
		const SFilterAttributes filterAttributes = ScriptBrowserUtils::GetScriptElementFilterAttributes(EScriptElementType::Base);
		CScriptBrowserItemPtr pBaseFilter = std::make_shared<CScriptBrowserItem>(EScriptBrowserItemType::Filter, scriptElement.GetName(), ScriptBrowserUtils::GetScriptElementIcon(scriptElement), &scriptElement);
		pBaseFilter->SetSortString(filterAttributes.szOrder);
		pBaseFilter->SetFlags(flags);
		pBaseFilter->SetFilterType(EFilterType::Base);

		const QModelIndex rootIndex = QModelIndex();
		const int32 row = m_filterItems.size();
		QAbstractItemModel::beginInsertRows(rootIndex, row, row);
		m_filterItems.emplace_back(pBaseFilter);
		m_pRootItem->AddChild(pBaseFilter);
		QAbstractItemModel::endInsertRows();
		QAbstractItemModel::dataChanged(rootIndex, rootIndex);

		if (scriptElementType == EScriptElementType::Base)
		{
			ValidateItem(*pBaseFilter);

			return pBaseFilter.get();
		}
	}

	// Select parent/filter item.
	if (pParentItem == nullptr)
	{
		const SFilterAttributes filterAttributes = ScriptBrowserUtils::GetScriptElementFilterAttributes(scriptElementType);
		if (filterAttributes.szName && filterAttributes.szName[0])
		{
			CScriptBrowserItem* pFilterItem = GetFilter(filterAttributes.szName);
			if (!pFilterItem)
			{
				pFilterItem = CreateFilter(filterAttributes);
			}

			pParentItem = pFilterItem;
		}
	}

	CRY_ASSERT_MESSAGE(scriptElement.GetType() == Schematyc::EScriptElementType::Class || scriptElement.GetType() == Schematyc::EScriptElementType::Module || pParentItem, "Error adding script element to model because of a missing parent element.");
	if (pParentItem)
	{
		// Create item.
		stack_string filter;
		ScriptBrowserUtils::AppendFilterTags(scriptElementType, filter);
		filter.append(scriptElement.GetName());

		CScriptBrowserItemPtr pItem = std::make_shared<CScriptBrowserItem>(EScriptBrowserItemType::ScriptElement, scriptElement.GetName(), ScriptBrowserUtils::GetScriptElementIcon(scriptElement), &scriptElement);
		pItem->SetFlags(flags);
		pItem->SetSortString(scriptElement.GetName());
		pItem->SetFilterString(filter.c_str());

		AddItem(*pParentItem, pItem);

		m_itemsByGUID.emplace(scriptElement.GetGUID(), pItem.get());

		// Set initial state of item.
		ValidateItem(*pItem);
		return pItem.get();
	}

	return nullptr;
}

CScriptBrowserItem* CScriptBrowserModel::CreateScriptElementBaseItem(IScriptElement& scriptElement, const ScriptBrowserItemFlags& flags, CScriptBrowserItem* pParentItem, CScriptBrowserItem* pBaseItem)
{
	const EScriptElementType scriptElementType = scriptElement.GetType();

	// Select parent/filter item.
	const SFilterAttributes filterAttributes = ScriptBrowserUtils::GetScriptElementFilterAttributes(scriptElementType);
	if (filterAttributes.szName && filterAttributes.szName[0])
	{
		pParentItem = pBaseItem->GetChildByTypeAndName(EScriptBrowserItemType::Filter, filterAttributes.szName);
		if (pParentItem == nullptr)
		{
			stack_string filter("Base ");
			filter.append(filterAttributes.szName);

			const SFilterAttributes filterAttributes = ScriptBrowserUtils::GetScriptElementFilterAttributes(scriptElementType);
			CScriptBrowserItemPtr pFilterItem = std::make_shared<CScriptBrowserItem>(EScriptBrowserItemType::Filter, filterAttributes.szName, filterAttributes.filterType);
			pFilterItem->SetSortString(filterAttributes.szOrder);
			pFilterItem->SetFilterString(filter.c_str());

			AddItem(*pBaseItem, pFilterItem);
			pParentItem = pFilterItem.get();
		}
	}

	// Create item.
	stack_string filter("Base ");
	ScriptBrowserUtils::AppendFilterTags(scriptElementType, filter);
	filter.append(scriptElement.GetName());

	CScriptBrowserItemPtr pItem = std::make_shared<CScriptBrowserItem>(EScriptBrowserItemType::ScriptElement, scriptElement.GetName(), ScriptBrowserUtils::GetScriptElementIcon(scriptElement), &scriptElement);
	pItem->SetFlags(flags);
	pItem->SetSortString(scriptElement.GetName());
	pItem->SetFilterString(filter.c_str());

	AddItem(*pParentItem, pItem);

	m_itemsByGUID.emplace(scriptElement.GetGUID(), pItem.get());

	// Set initial state of item.
	ValidateItem(*pItem);

	return pItem.get();
}

CScriptBrowserItem* CScriptBrowserModel::AddItem(CScriptBrowserItem& parentItem, const CScriptBrowserItemPtr& pItem)
{
	const QModelIndex parentIndex = ItemToIndex(&parentItem);
	const int row = parentItem.GetChildCount();

	QAbstractItemModel::beginInsertRows(parentIndex, row, row);
	parentItem.AddChild(pItem);
	QAbstractItemModel::endInsertRows();
	QAbstractItemModel::dataChanged(parentIndex, parentIndex);

	return pItem.get();
}

CScriptBrowserItemPtr CScriptBrowserModel::RemoveItem(CScriptBrowserItem& item)
{
	CScriptBrowserItemPtr pResult;
	CScriptBrowserItem* pParentItem = item.GetParent();
	if (pParentItem)
	{
		QModelIndex index = ItemToIndex(&item);
		QAbstractItemModel::beginRemoveRows(index.parent(), index.row(), index.row());
		pResult = pParentItem->RemoveChild(&item);
		if (item.GetType() == EScriptBrowserItemType::Filter)
		{
			for (auto itr = m_filterItems.begin(); itr != m_filterItems.end(); ++itr)
			{
				if ((*itr).get() == &item)
				{
					m_filterItems.erase(itr);
					break;
				}
			}
		}
		QAbstractItemModel::endRemoveRows();

		ValidateItem(*pParentItem);
	}
	return pResult;
}

void CScriptBrowserModel::ValidateItem(CScriptBrowserItem& item)
{
	ScriptBrowserItemFlags prevFlags = item.GetFlags();

	item.Validate();

	if (item.GetFlags() != prevFlags)
	{
		const QModelIndex itemIndex = ItemToIndex(&item);
		QAbstractItemModel::dataChanged(itemIndex, itemIndex);

		for (CScriptBrowserItem* pParentItem = item.GetParent(); pParentItem; pParentItem = pParentItem->GetParent())
		{
			prevFlags = pParentItem->GetFlags();

			pParentItem->RefreshChildFlags();

			if (pParentItem->GetFlags() != prevFlags)
			{
				const QModelIndex parentItemIndex = ItemToIndex(pParentItem);
				QAbstractItemModel::dataChanged(parentItemIndex, parentItemIndex);
			}
			else
			{
				break;
			}
		}
	}
}

CScriptBrowserItem* CScriptBrowserModel::GetFilter(const char* szName) const
{
	stack_string filterName = szName;
	for (const CScriptBrowserItemPtr& filter : m_filterItems)
	{
		if (filter->GetName() == filterName)
		{
			return filter.get();
		}
	}
	return nullptr;
}

CScriptBrowserItem* CScriptBrowserModel::CreateFilter(const SFilterAttributes& filterAttributes)
{
	CScriptBrowserItemPtr pFilter = std::make_shared<CScriptBrowserItem>(EScriptBrowserItemType::Filter, filterAttributes.szName, filterAttributes.filterType);
	pFilter->SetSortString(filterAttributes.szOrder);
	pFilter->SetFilterString(filterAttributes.szName);

	const QModelIndex rootIndex = QModelIndex();
	const int32 row = m_filterItems.size();
	QAbstractItemModel::beginInsertRows(rootIndex, row, row);
	m_filterItems.emplace_back(pFilter);
	m_pRootItem->AddChild(pFilter);
	QAbstractItemModel::endInsertRows();
	QAbstractItemModel::dataChanged(rootIndex, rootIndex);

	return pFilter.get();
}

bool CScriptBrowserModel::HasUnsavedChanged()
{
	std::function<bool(CScriptBrowserItem*)> hasUnsavedChanges;
	hasUnsavedChanges = [&hasUnsavedChanges](CScriptBrowserItem* pItem) -> bool
	{
		if (pItem && !pItem->GetFlags().Check(EScriptBrowserItemFlags::Modified))
		{
			for (uint32 i = pItem->GetChildCount(); i--; )
			{
				if (hasUnsavedChanges(pItem->GetChildByIdx(i)))
					return true;
			}
			return false;
		}
		return true;

	};

	return hasUnsavedChanges(m_pRootItem.get());
}

SScriptBrowserSelection::SScriptBrowserSelection(IScriptElement* _pScriptElement)
	: pScriptElement(_pScriptElement)
{}

CScriptBrowserWidget::CScriptBrowserWidget(CrySchematycEditor::CMainWindow& editor)
	: QWidget(&editor)
	, m_editor(editor)
	, m_pContextMenu(nullptr)
	, m_pModel(nullptr)
	, m_pFilterProxy(nullptr)
	, m_pAsset(nullptr)
{
	m_pMainLayout = new QBoxLayout(QBoxLayout::TopToBottom);
	m_pHeaderLayout = new QBoxLayout(QBoxLayout::LeftToRight);
	m_pAddButton = new QPushButton("Add", this);
	m_pAddMenu = new QMenu("Add", this);

	const uint32 behaviorFlags = QAdvancedTreeView::PreserveExpandedAfterReset | QAdvancedTreeView::PreserveSelectionAfterReset | QAdvancedTreeView::UseItemModelAttribute;
	m_pTreeView = new QAdvancedTreeView(static_cast<QAdvancedTreeView::Behavior>(behaviorFlags));
	m_pTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
	m_pTreeView->setItemsExpandable(true);
	m_pTreeView->setHeaderHidden(true);
	m_pTreeView->setMouseTracking(true);
	m_pTreeView->setSortingEnabled(true);
	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pTreeView->setEditTriggers(QAbstractItemView::DoubleClicked);
	m_pTreeView->setExpandsOnDoubleClick(false);

	m_pFilter = new QSearchBox(this);
	m_pFilter->EnableContinuousSearch(true);
	m_pFilter->setPlaceholderText("Search");
	m_pFilter->signalOnFiltered.Connect(this, &CScriptBrowserWidget::OnFiltered);

	m_pAddButton->setMenu(m_pAddMenu);
	m_pAddButton->setEnabled(false);

	QObject::connect(m_pTreeView, SIGNAL(clicked(const QModelIndex &)), this, SLOT(OnTreeViewClicked(const QModelIndex &)));
	QObject::connect(m_pTreeView, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(OnTreeViewCustomContextMenuRequested(const QPoint &)));
	QObject::connect(m_pTreeView, SIGNAL(keyPress(QKeyEvent*, bool&)), this, SLOT(OnTreeViewKeyPress(QKeyEvent*, bool&)));

	QWidget::startTimer(500);
}

CScriptBrowserWidget::~CScriptBrowserWidget()
{
	if (m_pTreeView)
		m_pTreeView->setModel(nullptr);
}

void CScriptBrowserWidget::InitLayout()
{
	QWidget::setLayout(m_pMainLayout);
	m_pMainLayout->setSpacing(1);
	m_pMainLayout->setMargin(1);

	m_pHeaderLayout->addWidget(m_pFilter), 1;
	m_pHeaderLayout->addWidget(m_pAddButton), 1;

	m_pMainLayout->addLayout(m_pHeaderLayout, 1);
	m_pMainLayout->addWidget(m_pTreeView, 1);
}

void CScriptBrowserWidget::SelectItem(const CryGUID& guid)
{
	if (m_pModel)
	{
		CScriptBrowserItem* pItem = m_pModel->GetItemByGUID(guid);
		if (pItem)
		{
			const QModelIndex itemIndex = TreeViewFromModelIndex(m_pModel->ItemToIndex(pItem));
			m_pTreeView->selectionModel()->setCurrentIndex(itemIndex, QItemSelectionModel::ClearAndSelect);
			m_signals.selection.Send(SScriptBrowserSelection(pItem ? pItem->GetScriptElement() : nullptr));
		}
	}
}

CryGUID CScriptBrowserWidget::GetSelectedItemGUID() const
{
	const QItemSelection selection = m_pTreeView->selectionModel()->selection();
	if (selection.indexes().size() > 0)
	{
		if (const CScriptBrowserItem* pItem = m_pModel->ItemFromIndex(TreeViewToModelIndex(selection.indexes().at(0))))
		{
			if (IScriptElement* pScriptElement = pItem->GetScriptElement())
				return pScriptElement->GetGUID();
		}
	}
	return CryGUID::Null();
}

bool CScriptBrowserWidget::SetModel(CScriptBrowserModel* pModel)
{
	if (m_pFilterProxy)
	{
		m_pFilterProxy->setSourceModel(nullptr);
		m_pFilterProxy->deleteLater();
		QObject::disconnect(m_pFilterProxy);
		m_pFilterProxy = nullptr;
	}

	if (pModel)
	{
		CScriptBrowserItem* pItem = pModel->GetRootItem();
		if (pItem && pItem->GetType() == EScriptBrowserItemType::ScriptElement)
		{
			IScriptElement* pScriptElement = pItem->GetScriptElement();
			if (pScriptElement && (pScriptElement->GetType() == EScriptElementType::Class || pScriptElement->GetType() == EScriptElementType::Module))
			{
				m_pModel = pModel;
				m_pAsset = &m_pModel->GetAsset();

				const CScriptElementFilterProxyModel::BehaviorFlags filterBehavior = CScriptElementFilterProxyModel::AcceptIfChildMatches | CScriptElementFilterProxyModel::AcceptIfParentMatches;
				m_pFilterProxy = new CScriptElementFilterProxyModel(filterBehavior, this);
				m_pFilterProxy->setSourceModel(m_pModel);
				m_pFilterProxy->setFilterKeyColumn(EScriptBrowserColumn::Filter);

				m_pTreeView->sortByColumn(EScriptBrowserColumn::Sort, Qt::SortOrder::AscendingOrder);

				m_pFilter->SetModel(m_pFilterProxy);
				m_pTreeView->setModel(m_pFilterProxy);
				m_pTreeView->expandToDepth(0);

				m_pAddMenu->clear();
				PopulateAddMenu(m_pAddMenu, m_pModel->GetRootElement());
				m_pAddButton->setEnabled(!m_pAddMenu->isEmpty());

				return true;
			}
		}
	}

	m_pModel = nullptr;
	m_pAddMenu->clear();
	m_pAddButton->setEnabled(false);
	return (pModel == nullptr);
}

void CScriptBrowserWidget::Serialize(Serialization::IArchive& archive)
{
	QByteArray geometry;
	if (archive.isOutput())
	{
		geometry = QWidget::saveGeometry();
	}
	archive(geometry, "geometry");
	if (archive.isInput() && !geometry.isEmpty())
	{
		QWidget::restoreGeometry(geometry);
	}
}

ScriptBrowserSelectionSignal::Slots& CScriptBrowserWidget::GetSelectionSignalSlots()
{
	return m_signals.selection.GetSlots();
}

bool CScriptBrowserWidget::HasScriptUnsavedChanges() const
{
	if (m_pModel)
	{
		return m_pModel->HasUnsavedChanged();
	}
	return false;
}

void CScriptBrowserWidget::OnFiltered()
{
	const QString text = m_pFilter->text();
	if (!text.isEmpty())
	{
		m_pTreeView->expandAll();
	}
	else
	{
		m_pTreeView->collapseAll();
		m_pTreeView->expandToDepth(0);
	}
}

void CScriptBrowserWidget::OnTreeViewClicked(const QModelIndex& index)
{
	if (index.isValid())
	{
		if (m_pModel)
		{
			CScriptBrowserItem* pItem = m_pModel->ItemFromIndex(TreeViewToModelIndex(index));
			if (pItem)
			{
				m_signals.selection.Send(SScriptBrowserSelection(pItem ? pItem->GetScriptElement() : nullptr));
			}
		}
	}
}

void CScriptBrowserWidget::OnTreeViewCustomContextMenuRequested(const QPoint& position)
{
	if (m_pModel)
	{
		QMenu contextMenu;

		QModelIndex index = m_pTreeView->indexAt(position);
		CScriptBrowserItem* pItem = m_pModel->ItemFromIndex(TreeViewToModelIndex(index));
		if (pItem)
		{
			m_pContextMenu = &contextMenu;

			IScriptElement* pScriptElement = pItem->GetScriptElement();
			if (pScriptElement)
			{
				QMenu* pAddMenu = new QMenu("Add");

				PopulateAddMenu(pAddMenu, pScriptElement);
				if (!pAddMenu->isEmpty())
				{
					contextMenu.addMenu(pAddMenu);
				}

				if (ScriptBrowserUtils::CanRemoveScriptElement(*pScriptElement))
				{
					QAction* pRemoveAction = contextMenu.addAction(QString("Remove"));
					QObject::connect(pRemoveAction, SIGNAL(triggered()), this, SLOT(OnRemoveItem()));
				}

				if (ScriptBrowserUtils::CanRenameScriptElement(*pScriptElement))
				{
					QAction* pRenameAction = contextMenu.addAction(QString("Rename"));
					QObject::connect(pRenameAction, SIGNAL(triggered()), this, SLOT(OnRenameItem()));
				}
			}
			else
			{
				if (pItem->GetType() == EScriptBrowserItemType::Filter && pItem->GetParent() == m_pModel->GetRootItem())
				{
					PopulateFilterMenu(&contextMenu, pItem->GetFilterType());
				}
			}
		}
		/*else
		   {
		   RefreshAddMenu(m_pScriptElement);
		   if (!m_pAddMenu->isEmpty())
		   {
		    m_pContextMenu = m_pAddMenu;
		   }
		   }*/

		if (m_pContextMenu)
		{
			const QPoint globalPosition = m_pTreeView->viewport()->mapToGlobal(position);
			m_pContextMenu->exec(globalPosition);
			m_pContextMenu = nullptr;
		}
	}
}

void CScriptBrowserWidget::OnTreeViewKeyPress(QKeyEvent* pKeyEvent, bool& bEventHandled)
{
	bEventHandled = HandleKeyPress(pKeyEvent);
}

void CScriptBrowserWidget::OnFindReferences()
{
	if (m_pModel)
	{
		CScriptBrowserItem* pItem = m_pModel->ItemFromIndex(TreeViewToModelIndex(GetTreeViewSelection()));
		if (pItem)
		{
			IScriptElement* pScriptElement = pItem->GetScriptElement();
			if (pScriptElement)
			{
				ScriptBrowserUtils::FindReferences(*pScriptElement);
			}
		}
	}
}

void CScriptBrowserWidget::OnAddItem(IScriptElement* pScriptElement, EScriptElementType elementType)
{
	if (m_pModel)
	{
		IScriptElement* pScriptScope = pScriptElement;
		if (!pScriptElement)
		{
			pScriptScope = m_pModel->GetRootElement();
		}

		bool bRenameElement = false;
		switch (elementType)
		{
		/*case EScriptElementType::Module:
		   {
		    pScriptElement = ScriptBrowserUtils::AddScriptModule(pScriptScope);
		    bRenameElement = true;
		    break;
		   }*/
		case EScriptElementType::Enum:
			{
				GetIEditor()->GetIUndoManager()->Begin();
				CrySchematycEditor::CScriptUndoObject* pUndoObject = new CrySchematycEditor::CScriptUndoObject("Added Enumeration", m_editor);
				CUndo::Record(pUndoObject);
				GetIEditor()->GetIUndoManager()->Accept(pUndoObject->GetDescription());

				pScriptElement = ScriptBrowserUtils::AddScriptEnum(pScriptScope);
				bRenameElement = true;
				break;
			}
		case EScriptElementType::Struct:
			{
				GetIEditor()->GetIUndoManager()->Begin();
				CrySchematycEditor::CScriptUndoObject* pUndoObject = new CrySchematycEditor::CScriptUndoObject("Added Struct", m_editor);
				CUndo::Record(pUndoObject);
				GetIEditor()->GetIUndoManager()->Accept(pUndoObject->GetDescription());

				pScriptElement = ScriptBrowserUtils::AddScriptStruct(pScriptScope);
				bRenameElement = true;
				break;
			}
		case EScriptElementType::Signal:
			{
				GetIEditor()->GetIUndoManager()->Begin();
				CrySchematycEditor::CScriptUndoObject* pUndoObject = new CrySchematycEditor::CScriptUndoObject("Added Signal", m_editor);
				CUndo::Record(pUndoObject);
				GetIEditor()->GetIUndoManager()->Accept(pUndoObject->GetDescription());

				pScriptElement = ScriptBrowserUtils::AddScriptSignal(pScriptScope);
				bRenameElement = true;
				break;
			}
		case EScriptElementType::Function:
			{
				GetIEditor()->GetIUndoManager()->Begin();
				CrySchematycEditor::CScriptUndoObject* pUndoObject = new CrySchematycEditor::CScriptUndoObject("Added Function Graph", m_editor);
				CUndo::Record(pUndoObject);
				GetIEditor()->GetIUndoManager()->Accept(pUndoObject->GetDescription());

				pScriptElement = ScriptBrowserUtils::AddScriptFunction(pScriptScope);
				bRenameElement = true;
				break;
			}
		case EScriptElementType::Interface:
			{
				GetIEditor()->GetIUndoManager()->Begin();
				CrySchematycEditor::CScriptUndoObject* pUndoObject = new CrySchematycEditor::CScriptUndoObject("Added Interface", m_editor);
				CUndo::Record(pUndoObject);
				GetIEditor()->GetIUndoManager()->Accept(pUndoObject->GetDescription());

				pScriptElement = ScriptBrowserUtils::AddScriptInterface(pScriptScope);
				bRenameElement = true;
				break;
			}
		case EScriptElementType::InterfaceFunction:
			{
				GetIEditor()->GetIUndoManager()->Begin();
				CrySchematycEditor::CScriptUndoObject* pUndoObject = new CrySchematycEditor::CScriptUndoObject("Added Interface Function", m_editor);
				CUndo::Record(pUndoObject);
				GetIEditor()->GetIUndoManager()->Accept(pUndoObject->GetDescription());

				pScriptElement = ScriptBrowserUtils::AddScriptInterfaceFunction(pScriptScope);
				bRenameElement = true;
				break;
			}
		case EScriptElementType::InterfaceTask:
			{
				GetIEditor()->GetIUndoManager()->Begin();
				CrySchematycEditor::CScriptUndoObject* pUndoObject = new CrySchematycEditor::CScriptUndoObject("Added Interface Task", m_editor);
				CUndo::Record(pUndoObject);
				GetIEditor()->GetIUndoManager()->Accept(pUndoObject->GetDescription());

				pScriptElement = ScriptBrowserUtils::AddScriptInterfaceTask(pScriptScope);
				bRenameElement = true;
				break;
			}
		/*case EScriptElementType::Class:
		   {
		    pScriptElement = ScriptBrowserUtils::AddScriptClass(pScriptScope);
		    bRenameElement = true;
		    break;
		   }*/
		case EScriptElementType::StateMachine:
			{
				GetIEditor()->GetIUndoManager()->Begin();
				CrySchematycEditor::CScriptUndoObject* pUndoObject = new CrySchematycEditor::CScriptUndoObject("Added State Machine", m_editor);
				CUndo::Record(pUndoObject);
				GetIEditor()->GetIUndoManager()->Accept(pUndoObject->GetDescription());

				pScriptElement = ScriptBrowserUtils::AddScriptStateMachine(pScriptScope);
				bRenameElement = true;
				break;
			}
		case EScriptElementType::State:
			{
				GetIEditor()->GetIUndoManager()->Begin();
				CrySchematycEditor::CScriptUndoObject* pUndoObject = new CrySchematycEditor::CScriptUndoObject("Added State", m_editor);
				CUndo::Record(pUndoObject);
				GetIEditor()->GetIUndoManager()->Accept(pUndoObject->GetDescription());

				pScriptElement = ScriptBrowserUtils::AddScriptState(pScriptScope);
				bRenameElement = true;
				break;
			}
		case EScriptElementType::Variable:
			{
				GetIEditor()->GetIUndoManager()->Begin();
				CrySchematycEditor::CScriptUndoObject* pUndoObject = new CrySchematycEditor::CScriptUndoObject("Added Variable", m_editor);
				CUndo::Record(pUndoObject);
				GetIEditor()->GetIUndoManager()->Accept(pUndoObject->GetDescription());

				pScriptElement = ScriptBrowserUtils::AddScriptVariable(pScriptScope);
				bRenameElement = true;
				break;
			}
		case EScriptElementType::Timer:
			{
				GetIEditor()->GetIUndoManager()->Begin();
				CrySchematycEditor::CScriptUndoObject* pUndoObject = new CrySchematycEditor::CScriptUndoObject("Added Timer", m_editor);
				CUndo::Record(pUndoObject);
				GetIEditor()->GetIUndoManager()->Accept(pUndoObject->GetDescription());

				pScriptElement = ScriptBrowserUtils::AddScriptTimer(pScriptScope);
				bRenameElement = true;
				break;
			}
		case EScriptElementType::SignalReceiver:
			{
				GetIEditor()->GetIUndoManager()->Begin();
				CrySchematycEditor::CScriptUndoObject* pUndoObject = new CrySchematycEditor::CScriptUndoObject("Added Signal Receiver", m_editor);
				CUndo::Record(pUndoObject);
				GetIEditor()->GetIUndoManager()->Accept(pUndoObject->GetDescription());

				pScriptElement = ScriptBrowserUtils::AddScriptSignalReceiver(pScriptScope);
				bRenameElement = true;
				break;
			}
		case EScriptElementType::InterfaceImpl:
			{
				GetIEditor()->GetIUndoManager()->Begin();
				CrySchematycEditor::CScriptUndoObject* pUndoObject = new CrySchematycEditor::CScriptUndoObject("Added Interface Implementation", m_editor);
				CUndo::Record(pUndoObject);
				GetIEditor()->GetIUndoManager()->Accept(pUndoObject->GetDescription());

				if (m_pContextMenu)
				{
					QPoint pos = m_pContextMenu->pos();
					pScriptElement = ScriptBrowserUtils::AddScriptInterfaceImpl(pScriptScope, &pos);
				}
				else if (m_pAddMenu)
				{
					QPoint pos = m_pAddMenu->pos();
					pScriptElement = ScriptBrowserUtils::AddScriptInterfaceImpl(pScriptScope, &pos);
				}
				else
				{
					pScriptElement = ScriptBrowserUtils::AddScriptInterfaceImpl(pScriptScope);
				}
				bRenameElement = true;
				break;
			}
		case EScriptElementType::ComponentInstance:
			{
				GetIEditor()->GetIUndoManager()->Begin();
				CrySchematycEditor::CScriptUndoObject* pUndoObject = new CrySchematycEditor::CScriptUndoObject("Added Component", m_editor);
				CUndo::Record(pUndoObject);
				GetIEditor()->GetIUndoManager()->Accept(pUndoObject->GetDescription());

				if (m_pContextMenu)
				{
					QPoint pos = m_pContextMenu->pos();
					pScriptElement = ScriptBrowserUtils::AddScriptComponentInstance(pScriptScope, &pos);
				}
				else if (m_pAddMenu)
				{
					QPoint pos = m_pAddMenu->pos();
					pScriptElement = ScriptBrowserUtils::AddScriptComponentInstance(pScriptScope, &pos);
				}
				else
				{
					pScriptElement = ScriptBrowserUtils::AddScriptComponentInstance(pScriptScope);
				}
				bRenameElement = true;
				break;
			}
		case EScriptElementType::ActionInstance:
			{
				CRY_ASSERT_MESSAGE(false, "Actions not supported!");
				break;
			}
		}

		if (pScriptElement)
		{
			SelectItem(pScriptElement->GetGUID());
			if (bRenameElement)
			{
				CScriptBrowserItem* pItem = m_pModel->GetItemByGUID(pScriptElement->GetGUID());
				if (pItem)
				{
					const QModelIndex itemIndex = TreeViewFromModelIndex(m_pModel->ItemToIndex(pItem));
					m_pTreeView->edit(itemIndex);
				}
			}
		}
	}
}

void CScriptBrowserWidget::OnCopyItem()
{
	if (m_pModel)
	{
		CScriptBrowserItem* pItem = m_pModel->ItemFromIndex(TreeViewToModelIndex(GetTreeViewSelection()));
		if (pItem)
		{
			IScriptElement* pScriptElement = pItem->GetScriptElement();
			if (pScriptElement)
			{
				XmlNodeRef xml;
				if (gEnv->pSchematyc->GetScriptRegistry().CopyElementsToXml(xml, *pScriptElement))
				{
					CrySchematycEditor::Utils::WriteToClipboard(xml->getXMLData()->GetString(), g_szClipboardPrefix);
				}
			}
		}
	}
}

void CScriptBrowserWidget::OnPasteItem()
{
	if (m_pModel)
	{
		CScriptBrowserItem* pItem = m_pModel->ItemFromIndex(TreeViewToModelIndex(GetTreeViewSelection()));
		if (pItem)
		{
			IScriptElement* pScriptElement = pItem->GetScriptElement();
			if (pScriptElement)
			{
				string clipboardText;
				if (CrySchematycEditor::Utils::ReadFromClipboard(clipboardText, g_szClipboardPrefix))
				{
					XmlNodeRef xml = gEnv->pSystem->LoadXmlFromBuffer(clipboardText.c_str(), clipboardText.length());
					if (xml)
					{
						gEnv->pSchematyc->GetScriptRegistry().PasteElementsFromXml(xml, pScriptElement);
					}
				}
			}
		}
	}
}

void CScriptBrowserWidget::OnRemoveItem()
{
	if (m_pModel)
	{
		CScriptBrowserItem* pItem = m_pModel->ItemFromIndex(TreeViewToModelIndex(GetTreeViewSelection()));
		if (pItem)
		{
			IScriptElement* pScriptElement = pItem->GetScriptElement();
			if (pScriptElement)
			{
				const char* szElementType = nullptr;
				switch (pScriptElement->GetType())
				{
				case EScriptElementType::Module:
					{
						break;
					}
				case EScriptElementType::Enum:
					{
						szElementType = "Enumeration";
						break;
					}
				case EScriptElementType::Struct:
					{
						szElementType = "Struct";
						break;
					}
				case EScriptElementType::Signal:
					{
						szElementType = "Signal";
						break;
					}
				case EScriptElementType::Function:
					{
						szElementType = "Function Graph";
						break;
					}
				case EScriptElementType::Interface:
					{
						szElementType = "Interface";
						break;
					}
				case EScriptElementType::InterfaceFunction:
					{
						szElementType = "Interface Function";
						break;
					}
				case EScriptElementType::InterfaceTask:
					{
						szElementType = "Interface Task";
						break;
					}
				case EScriptElementType::Class:
					{
						break;
					}
				case EScriptElementType::StateMachine:
					{
						szElementType = "State Machine";
						break;
					}
				case EScriptElementType::State:
					{
						szElementType = "State";
						break;
					}
				case EScriptElementType::Variable:
					{
						szElementType = "Variable";
						break;
					}
				case EScriptElementType::Timer:
					{
						szElementType = "Timer";
						break;
					}
				case EScriptElementType::SignalReceiver:
					{
						szElementType = "Signal Receiver";
						break;
					}
				case EScriptElementType::InterfaceImpl:
					{
						szElementType = "Interface Implementation";
						break;
					}
				case EScriptElementType::ComponentInstance:
					{
						szElementType = "Component";
						break;
					}
				}

				if (ScriptBrowserUtils::CanRemoveScriptElement(*pScriptElement))
				{
					CStackString question = "Are you sure you want to remove '";
					question.append(pScriptElement->GetName());
					question.append("'?");

					CQuestionDialog dialog;
					dialog.SetupQuestion("Remove", question.c_str(), QDialogButtonBox::Yes | QDialogButtonBox::No);

					QDialogButtonBox::StandardButton result = dialog.Execute();

					if (result == QDialogButtonBox::Yes)
					{
						if (szElementType && m_pModel)
						{
							Schematyc::IScriptElement* pRootElement = m_pModel->GetRootElement();

							GetIEditor()->GetIUndoManager()->Begin();
							stack_string desc("Added ");
							desc.append(szElementType);
							CrySchematycEditor::CScriptUndoObject* pUndoObject = new CrySchematycEditor::CScriptUndoObject(desc.c_str(), m_editor);
							CUndo::Record(pUndoObject);
							GetIEditor()->GetIUndoManager()->Accept(pUndoObject->GetDescription());
						}

						OnScriptElementRemoved(*pScriptElement);
						gEnv->pSchematyc->GetScriptRegistry().RemoveElement(pScriptElement->GetGUID());
					}

					if (CBroadcastManager* pBroadcastManager = CBroadcastManager::Get(this))
					{
						PopulateInspectorEvent popEvent([](CInspector& inspector) {});
						pBroadcastManager->Broadcast(popEvent);
					}
				}
			}
		}
	}
}

void CScriptBrowserWidget::OnRenameItem()
{
	m_pTreeView->edit(GetTreeViewSelection());
}

void CScriptBrowserWidget::OnSave()
{
	// #SchematycTODO : Implement this function!!!
}

void CScriptBrowserWidget::OnExtract()
{
	// #SchematycTODO : Implement this function!!!
}

void CScriptBrowserWidget::keyPressEvent(QKeyEvent* pKeyEvent)
{
	HandleKeyPress(pKeyEvent);
}

void CScriptBrowserWidget::showEvent(QShowEvent* pEvent)
{
	m_pFilter->setText("");
	QWidget::showEvent(pEvent);
}

void CScriptBrowserWidget::ExpandAll()
{
	m_pTreeView->expandAll();
}

void CScriptBrowserWidget::PopulateAddMenu(QMenu* pMenu, IScriptElement* pScriptScope)
{
	if (pScriptScope)
	{
		/*if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::Module, pScriptScope))
		   {
		   QAction* pAction = pMenu->addAction("Folder");
		   QObject::connect(pAction, &QAction::triggered, [this, pScriptScope]()
		    {
		      OnAddItem(pScriptScope, EScriptElementType::Module);
		    });
		   }*/
		/*if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::Import, pScriptScope))
		   {
		   QAction* pAddImportAction = pMenu->addAction("Import");
		   pAddImportAction->setData(static_cast<int>(EScriptElementType::Import));
		   connect(pAddImportAction, SIGNAL(triggered()), this, SLOT(OnAddItem()));
		   }*/
		if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::Enum, pScriptScope))
		{
			QAction* pAction = pMenu->addAction("Enumeration");
			QObject::connect(pAction, &QAction::triggered, [this, pScriptScope]()
				{
					OnAddItem(pScriptScope, EScriptElementType::Enum);
			  });
		}
		/*if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::Struct, pScriptScope))
		   {
		   QAction* pAddStructAction = pMenu->addAction("Structure");
		   pAddStructAction->setData(static_cast<int>(EScriptElementType::Struct));
		   connect(pAddStructAction, SIGNAL(triggered()), this, SLOT(OnAddItem()));
		   }*/
		if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::Signal, pScriptScope))
		{
			QAction* pAction = pMenu->addAction("Signal");
			QObject::connect(pAction, &QAction::triggered, [this, pScriptScope]()
				{
					OnAddItem(pScriptScope, EScriptElementType::Signal);
			  });
		}
		if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::Function, pScriptScope))
		{
			QAction* pAction = pMenu->addAction("Function");
			QObject::connect(pAction, &QAction::triggered, [this, pScriptScope]()
				{
					OnAddItem(pScriptScope, EScriptElementType::Function);
			  });
		}
		/*if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::Interface, pScriptScope))
		   {
		   QAction* pAction = pMenu->addAction("Interface");
		   QObject::connect(pAction, &QAction::triggered, [this, pScriptScope]()
		    {
		      OnAddItem(pScriptScope, EScriptElementType::Interface);
		    });
		   }
		   if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::InterfaceFunction, pScriptScope))
		   {
		   QAction* pAction = pMenu->addAction("Interface Function");
		   QObject::connect(pAction, &QAction::triggered, [this, pScriptScope]()
		    {
		      OnAddItem(pScriptScope, EScriptElementType::InterfaceFunction);
		    });
		   }
		   if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::InterfaceImpl, pScriptScope))
		   {
		   QAction* pAction = pMenu->addAction("Interface Implementation");
		   QObject::connect(pAction, &QAction::triggered, [this, pScriptScope]()
		    {
		      OnAddItem(pScriptScope, EScriptElementType::InterfaceImpl);
		    });
		   }*/
		/*if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::InterfaceTask, pScriptScope))
		   {
		   QAction* pAddInterfaceTask = pMenu->addAction("Task");
		   pAddInterfaceTask->setData(static_cast<int>(EScriptElementType::InterfaceTask));
		   connect(pAddInterfaceTask, SIGNAL(triggered()), this, SLOT(OnAddItem()));
		   }*/
		if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::StateMachine, pScriptScope))
		{
			QAction* pAction = pMenu->addAction("State Machine");
			QObject::connect(pAction, &QAction::triggered, [this, pScriptScope]()
				{
					OnAddItem(pScriptScope, EScriptElementType::StateMachine);
			  });
		}
		if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::State, pScriptScope))
		{
			QAction* pAction = pMenu->addAction("State");
			QObject::connect(pAction, &QAction::triggered, [this, pScriptScope]()
				{
					OnAddItem(pScriptScope, EScriptElementType::State);
			  });
		}
		if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::Variable, pScriptScope))
		{
			QAction* pAction = pMenu->addAction("Variable");
			QObject::connect(pAction, &QAction::triggered, [this, pScriptScope]()
				{
					OnAddItem(pScriptScope, EScriptElementType::Variable);
			  });
		}
		/*if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::Property, pScriptScope))
		   {
		   QAction* pAddPropertyAction = pMenu->addAction("Property");
		   pAddPropertyAction->setData(static_cast<int>(EScriptElementType::Property));
		   connect(pAddPropertyAction, SIGNAL(triggered()), this, SLOT(OnAddItem()));
		   }*/
		if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::Timer, pScriptScope))
		{
			QAction* pAction = pMenu->addAction("Timer");
			QObject::connect(pAction, &QAction::triggered, [this, pScriptScope]()
				{
					OnAddItem(pScriptScope, EScriptElementType::Timer);
			  });
		}
		if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::SignalReceiver, pScriptScope))
		{
			QAction* pAction = pMenu->addAction("Signal Receiver");
			QObject::connect(pAction, &QAction::triggered, [this, pScriptScope]()
				{
					OnAddItem(pScriptScope, EScriptElementType::SignalReceiver);
			  });
		}
		/*if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::InterfaceImpl, pScriptScope))
		   {
		   QAction* pAddInterfaceImplAction = pMenu->addAction("Interface");
		   pAddInterfaceImplAction->setData(static_cast<int>(EScriptElementType::InterfaceImpl));
		   connect(pAddInterfaceImplAction, SIGNAL(triggered()), this, SLOT(OnAddItem()));
		   }*/
		if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::ComponentInstance, pScriptScope))
		{
			QAction* pAction = pMenu->addAction("Component");
			QObject::connect(pAction, &QAction::triggered, [this, pScriptScope]()
				{
					OnAddItem(pScriptScope, EScriptElementType::ComponentInstance);
			  });
		}
	}
}

void CScriptBrowserWidget::PopulateFilterMenu(QMenu* pMenu, EFilterType filterType)
{
	if (m_pModel)
	{
		Schematyc::IScriptElement* pRootElement = m_pModel->GetRootElement();

		switch (filterType)
		{
		case EFilterType::Types:
			{
				if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::Enum, pRootElement))
				{
					QAction* pAction = pMenu->addAction("Add Enumeration");
					QObject::connect(pAction, &QAction::triggered, [this, pRootElement]()
						{
							OnAddItem(pRootElement, EScriptElementType::Enum);
					  });
				}

				/*if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::Struct, pScriptScope))
				   {
				   QAction* pAddStructAction = pMenu->addAction("Add Structure");
				   QObject::connect(pAction, &QAction::triggered, [this, pRootElement]()
				   {
				    OnAddItem(pRootElement, EScriptElementType::Struct);
				   });
				   }*/
				break;
			}

		case EFilterType::Variables:
			{
				if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::Variable, pRootElement))
				{
					QAction* pAction = pMenu->addAction("Add Variable");
					QObject::connect(pAction, &QAction::triggered, [this, pRootElement]()
						{
							OnAddItem(pRootElement, EScriptElementType::Variable);
					  });
				}

				if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::Timer, pRootElement))
				{
					QAction* pAction = pMenu->addAction("Add Timer");
					QObject::connect(pAction, &QAction::triggered, [this, pRootElement]()
						{
							OnAddItem(pRootElement, EScriptElementType::Timer);
					  });
				}
				break;
			}

		case EFilterType::Signals:
			{
				if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::Signal, pRootElement))
				{
					QAction* pAction = pMenu->addAction("Add Signal");
					QObject::connect(pAction, &QAction::triggered, [this, pRootElement]()
						{
							OnAddItem(pRootElement, EScriptElementType::Signal);
					  });
				}
				break;
			}
		case EFilterType::Graphs:
			{
				if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::Function, pRootElement))
				{
					QAction* pAction = pMenu->addAction("Add Function");
					QObject::connect(pAction, &QAction::triggered, [this, pRootElement]()
						{
							OnAddItem(pRootElement, EScriptElementType::Function);
					  });
				}
				if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::SignalReceiver, pRootElement))
				{
					QAction* pAction = pMenu->addAction("Add Signal Receiver");
					QObject::connect(pAction, &QAction::triggered, [this, pRootElement]()
						{
							OnAddItem(pRootElement, EScriptElementType::SignalReceiver);
					  });
				}
				break;
			}

		case EFilterType::StateMachine:
			{
				/*if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::StateMachine, m_pScriptElement))
				   {
				   QAction* pAction = pMenu->addAction("Add State Machine");
				   QObject::connect(pAction, &QAction::triggered, [this, pRootElement]()
				   {
				    OnAddItem(pRootElement, EScriptElementType::SignalReceiver);
				   });
				   }*/
				break;
			}

		case EFilterType::Components:
			{
				if (ScriptBrowserUtils::CanAddScriptElement(EScriptElementType::ComponentInstance, pRootElement))
				{
					QAction* pAction = pMenu->addAction("Add Component");
					QObject::connect(pAction, &QAction::triggered, [this, pRootElement]()
						{
							OnAddItem(pRootElement, EScriptElementType::ComponentInstance);
					  });
				}
				break;
			}
		default:
			break;
		}
	}

	return;
}

bool CScriptBrowserWidget::HandleKeyPress(QKeyEvent* pKeyEvent)
{
	if (pKeyEvent->modifiers() == Qt::CTRL)
	{
		if (pKeyEvent->key() == Qt::Key_A)
		{
			// #SchematycTODO : Make sure selected item is a script element?
			m_pAddButton->click();
			return true;
		}
		else if (pKeyEvent->key() == Qt::Key_S)  // #SchematycTODO : Is this really the best shortcut key?
		{
			m_pFilter->setFocus();
			return true;
		}
	}
	return false;
}

QModelIndex CScriptBrowserWidget::GetTreeViewSelection() const
{
	const QModelIndexList& indices = m_pTreeView->selectionModel()->selection().indexes();
	if (indices.size())
	{
		return indices.front();
	}
	return QModelIndex();
}

QModelIndex CScriptBrowserWidget::TreeViewToModelIndex(const QModelIndex& index) const
{
	return m_pFilterProxy->mapToSource(index);
}

QModelIndex CScriptBrowserWidget::TreeViewFromModelIndex(const QModelIndex& index) const
{
	return m_pFilterProxy->mapFromSource(index);
}

} // Schematyc

