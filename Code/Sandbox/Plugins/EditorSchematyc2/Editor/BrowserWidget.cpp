// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BrowserWidget.h"

#include <ISourceControl.h>
#include <QBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <Util/QParentWndWidget.h>
#include <QPushButton.h>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QWidgetAction>
#include <CrySchematyc2/IFoundation.h>
#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/Script/IScriptRegistry.h>
#include <CrySchematyc2/Script/Elements/IScriptFunction.h>
#include <CrySchematyc2/Script/Elements/IScriptModule.h>
#include <CrySchematyc2/Serialization/ISerializationContext.h>
#include <CrySchematyc2/Serialization/IValidatorArchive.h>

#include "PluginUtils.h"

namespace Schematyc2
{
	namespace BrowserUtils
	{
		const char* g_szAddIcon               = "editor/icons/add.png";
		const char* g_szBackIcon              = "editor/icons/back_16.png";
		const char* g_szLocalFileIcon         = "editor/icons/cryschematyc/localfile.png";
		const char* g_szScriptElementIcon     = "editor/icons/cryschematyc/scriptelement.png";
		const char* g_szScriptFileIcon        = "editor/icons/cryschematyc/scriptfile.png";
		const char* g_szScriptModuleIcon      = "editor/icons/cryschematyc/scriptmodule.png";
		const char* g_szScriptEnumerationIcon = "editor/icons/cryschematyc/scriptenumeration.png";
		const char* g_szScriptSignalIcon      = "editor/icons/cryschematyc/scriptsignal.png";
		const char* g_szScriptPropertyIcon    = "editor/icons/cryschematyc/scriptproperty.png";
		const char* g_szScriptComponentIcon   = "editor/icons/cryschematyc/scriptcomponent.png";
		const char* g_szScriptActionIcon      = "editor/icons/cryschematyc/scriptaction.png";

		static const QSize g_defaultColumnSize[EBrowserColumn::Count] =
		{
			QSize(150, 20),
			QSize(50, 20)
		};

		const char* GetScriptElementIcon(EScriptElementType elementType)
		{
			switch(elementType)
			{
			case EScriptElementType::Module:
				{
					return g_szScriptModuleIcon;
				}
			case EScriptElementType::Enumeration:
				{
					return g_szScriptEnumerationIcon;
				}
			case EScriptElementType::Signal:
				{
					return g_szScriptSignalIcon;
				}
			case EScriptElementType::Class:
				{
					return g_szScriptElementIcon;
				}
			case EScriptElementType::Property:
				{
					return g_szScriptPropertyIcon;
				}
			case EScriptElementType::ComponentInstance:
				{
					return g_szScriptComponentIcon;
				}
			case EScriptElementType::ActionInstance:
				{
					return g_szScriptActionIcon;
				}
			}
			return "";
		}

		inline stack_string ConstructAbsolutePath(const char* szFileName) // #SchematycTODO : Move to PluginUtils.cpp/h?
		{
			char currentDirectory[512] = "";
			GetCurrentDirectory(sizeof(currentDirectory) - 1, currentDirectory);

			stack_string path = currentDirectory;
			path.append("\\");
			path.append(gEnv->pCryPak->GetGameFolder());
			path.append("\\");
			path.append(szFileName);
			path.replace("/", "\\");
			return path;
		}

		void MakeScriptElementNameUnique(stack_string& name, IScriptElement* pScriptElementScope) // #SchematycTODO : Move to BrowserUtils.cpp/h?
		{
			TScriptRegistry& scriptRegistry = GetSchematyc()->GetScriptRegistry();
			if(!scriptRegistry.IsElementNameUnique(name.c_str(), pScriptElementScope))
			{
				char                          stringBuffer[StringUtils::s_uint32StringBufferSize] = "";
				const stack_string::size_type counterPos = name.find(".");
				stack_string::size_type       counterLength = 0;
				uint32                        counter = 1;
				do
				{
					name.replace(counterPos, counterLength, StringUtils::UInt32ToString(counter, stringBuffer));
					counterLength = strlen(stringBuffer);
					++ counter;
				} while(!scriptRegistry.IsElementNameUnique(name.c_str()));
			}
		}
	}

	class CBrowserFilter : public QSortFilterProxyModel
	{
	public:

		CBrowserFilter(QObject* pParent, CBrowserModel& model)
			: QSortFilterProxyModel(pParent)
			, m_model(model)
			, m_pFocusItem(nullptr)
		{}

		void SetFilterText(const char* szFilterText)
		{
			m_filterText = szFilterText;
		}

		void SetFocus(const QModelIndex& index)
		{
			m_pFocusItem = m_model.ItemFromIndex(index);
		}

		QModelIndex GetFocus() const
		{
			return m_model.ItemToIndex(m_pFocusItem);
		}

		// QSortFilterProxyModel

		virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override
		{
			QModelIndex           index = sourceModel()->index(source_row, 0, source_parent);
			CBrowserModel::CItem* pItem = m_model.ItemFromIndex(index);
			if(pItem)
			{
				return FilterItemByFocus(pItem) && FilterItemByTextRecursive(pItem);
			}
			return false;
		}

		virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
		{
			return QSortFilterProxyModel::lessThan(left, right);
		}

		// ~QSortFilterProxyModel

	private:

		bool FilterItemByFocus(CBrowserModel::CItem* pItem) const
		{
			return !m_pFocusItem || (pItem == m_pFocusItem) || (pItem->GetParent() != m_pFocusItem->GetParent());
		}

		inline bool FilterItemByTextRecursive(CBrowserModel::CItem* pItem) const // #SchematycTODO : Keep an eye on performance of this test!
		{
			if(StringUtils::FilterString(pItem->GetName(), m_filterText.c_str()))
			{
				return true;
			}
			for(int childIdx = 0, childCount = pItem->GetChildCount(); childIdx < childCount; ++ childIdx)
			{
				if(FilterItemByTextRecursive(pItem->GetChildByIdx(childIdx)))
				{
					return true;
				}
			}
			return false;
		}

	private:

		CBrowserModel&        m_model;
		string                m_filterText;
		CBrowserModel::CItem* m_pFocusItem;
	};

	bool CBrowserRules::CanAddScriptElement(EScriptElementType scriptElementType, const SGUID& scopeGUID) const
	{
		return true;
	}

	const char* CBrowserRules::GetScriptElementFilter(EScriptElementType scriptElementType) const
	{
		switch(scriptElementType)
		{
		case EScriptElementType::Enumeration:
			{
				return "Enumerations";
			}
		case EScriptElementType::Signal:
			{
				return "Signals";
			}
		case EScriptElementType::Property:
			{
				return "Properties";
			}
		case EScriptElementType::ComponentInstance:
			{
				return "Components";
			}
		case EScriptElementType::ActionInstance:
			{
				return "Actions";
			}
		}
		return nullptr;
	}

	CBrowserModel::CItem::CItem(EBrowserItemType type, const char* szName, const char* szIcon)
		: m_type(type)
		, m_name(szName)
		, m_icon(szIcon)
		, m_statusFlags(EBrowserItemStatusFlags::None)
		, m_pScriptElement(nullptr)
		, m_pParent(nullptr)
	{}

	CBrowserModel::CItem::CItem(IScriptElement* pScriptElement, const char* szName, const char* szIcon)
		: m_type(EBrowserItemType::ScriptElement)
		, m_name(szName)
		, m_icon(szIcon)
		, m_statusFlags(EBrowserItemStatusFlags::None)
		, m_pScriptElement(pScriptElement)
		, m_pParent(nullptr)
	{}

	EBrowserItemType CBrowserModel::CItem::GetType() const
	{
		return m_type;
	}

	void CBrowserModel::CItem::SetName(const char* szName)
	{
		if(m_pScriptElement)
		{
			m_pScriptElement->SetName(szName);
			m_name = szName;
		}
	}

	const char* CBrowserModel::CItem::GetName() const
	{
		return m_name.c_str();
	}

	const char* CBrowserModel::CItem::GetPath() const
	{
		return m_path.c_str();
	}

	const char* CBrowserModel::CItem::GetIcon() const
	{
		return m_icon.c_str();
	}

	void CBrowserModel::CItem::SetStatusFlags(EBrowserItemStatusFlags statusFlags)
	{
		m_statusFlags = statusFlags;
	}

	EBrowserItemStatusFlags CBrowserModel::CItem::GetStatusFlags() const
	{
		return m_statusFlags;
	}

	const char* CBrowserModel::CItem::GetStatusText() const
	{
		if((m_statusFlags & EBrowserItemStatusFlags::Local) != 0)
		{
			if((m_statusFlags & EBrowserItemStatusFlags::SourceControl) != 0)
			{
				if((m_statusFlags & EBrowserItemStatusFlags::CheckedOut) != 0)
				{
					return "SourceControl | Checked Out";
				}
				else
				{
					return "SourceControl";
				}
			}
			else if((m_statusFlags & EBrowserItemStatusFlags::ReadOnly) != 0)
			{
				return "Local | Read Only";
			}
			return "Local";
		}
		else if((m_statusFlags & EBrowserItemStatusFlags::InPak) != 0)
		{
			return "In Pak";
		}
		return "";
	}

	const char* CBrowserModel::CItem::GetStatusIcon() const
	{
		if((m_statusFlags & EBrowserItemStatusFlags::Local) != 0)
		{
			return BrowserUtils::g_szLocalFileIcon;
		}
		return "";
	}

	void CBrowserModel::CItem::SetStatusToolTip(const char* szStatusToolTip)
	{
		m_statusToolTip = szStatusToolTip;
	}

	const char* CBrowserModel::CItem::GetStatusToolTip() const
	{
		return m_statusToolTip.c_str();
	}

	QVariant CBrowserModel::CItem::GetColor() const
	{
		if((m_statusFlags & EBrowserItemStatusFlags::ContainsErrors) != 0)
		{
			return QColor(Qt::red);
		}
		else if((m_statusFlags & EBrowserItemStatusFlags::ContainsWarnings) != 0)
		{
			return QColor(Qt::yellow);
		}
		return QVariant(); 
	}

	IScriptElement* CBrowserModel::CItem::GetScriptElement() const
	{
		return m_pScriptElement;
	}

	CBrowserModel::CItem* CBrowserModel::CItem::GetParent()
	{
		return m_pParent;
	}

	void CBrowserModel::CItem::AddChild(const CItemPtr& pChild)
	{
		m_children.push_back(pChild);
		pChild->m_pParent = this;
	}

	void CBrowserModel::CItem::RemoveChild(CItem* pChild)
	{
		auto matchItemPtr = [&pChild] (const CItemPtr& pItem) -> bool
		{
			return pItem.get() == pChild;
		};
		m_children.erase(std::remove_if(m_children.begin(), m_children.end(), matchItemPtr));
	}

	int CBrowserModel::CItem::GetChildCount() const
	{
		return static_cast<int>(m_children.size());
	}

	int CBrowserModel::CItem::GetChildIdx(CItem* pChild)
	{
		for(int childIdx = 0, childCount = m_children.size(); childIdx < childCount; ++ childIdx)
		{
			if(m_children[childIdx].get() == pChild)
			{
				return childIdx;
			}
		}
		return -1;
	}

	CBrowserModel::CItem* CBrowserModel::CItem::GetChildByIdx(int childIdx)
	{
		return childIdx < m_children.size() ? m_children[childIdx].get() : nullptr;
	}

	CBrowserModel::CItem* CBrowserModel::CItem::GetChildByTypeAndName(EBrowserItemType type, const char* szName)
	{
		if(szName)
		{
			for(const CItemPtr& pChild : m_children)
			{
				if((pChild->GetType() == type) && (strcmp(pChild->GetName(), szName) == 0))
				{
					return pChild.get();
				}
			}
		}
		return nullptr;
	}

	CBrowserModel::CItem* CBrowserModel::CItem::GetChildByPath(const char* szPath)
	{
		if(szPath)
		{
			for(const CItemPtr& pChild : m_children)
			{
				if(strcmp(pChild->GetPath(), szPath) == 0)
				{
					return pChild.get();
				}
			}
		}
		return nullptr;
	}

	void CBrowserModel::CItem::SortChildren()
	{
		auto compare = [] (const CItemPtr& lhs, const CItemPtr& rhs)
		{
			return lhs->GetPath() < rhs->GetPath();
		};
		std::sort(m_children.begin(), m_children.end(), compare);

		for(const CItemPtr& pChild : m_children)
		{
			pChild->SortChildren();
		}
	}

	void CBrowserModel::CItem::Validate()
	{
		if(m_pScriptElement)
		{
			IValidatorArchivePtr     pArchive = GetSchematyc()->CreateValidatorArchive(SValidatorArchiveParams());
			ISerializationContextPtr pSerializationContext = GetSchematyc()->CreateSerializationContext(SSerializationContextParams(*pArchive, nullptr, ESerializationPass::Validate));
			m_pScriptElement->Serialize(*pArchive);
			
			EBrowserItemStatusFlags statusFlags = m_statusFlags;
			if(pArchive->GetWarningCount())
			{
				statusFlags |= EBrowserItemStatusFlags::ContainsWarnings;
			}
			else
			{
				statusFlags &= ~EBrowserItemStatusFlags::ContainsWarnings;
			}

			if(pArchive->GetErrorCount())
			{
				statusFlags |= EBrowserItemStatusFlags::ContainsErrors;
			}
			else
			{
				statusFlags &= ~EBrowserItemStatusFlags::ContainsErrors;
			}

			if(statusFlags != m_statusFlags)
			{
				m_statusFlags = statusFlags;
				if(m_pParent)
				{
					m_pParent->OnChildValidate(statusFlags);
				}
			}
		}
	}

	void CBrowserModel::CItem::OnChildValidate(EBrowserItemStatusFlags statusFlags)
	{
		statusFlags = m_statusFlags | (statusFlags & EBrowserItemStatusFlags::ValidationMask);
		if((statusFlags != m_statusFlags))
		{
			if(m_pParent)
			{
				m_pParent->OnChildValidate(statusFlags);
			}
			m_statusFlags = statusFlags;
		}
	}

	CBrowserModel::CSourceControlRefreshTask::CSourceControlRefreshTask(const SGUID& guid)
		: m_guid(guid)
	{}

	CBrowserModel::ETaskStatus CBrowserModel::CSourceControlRefreshTask::Execute(CBrowserModel& model)
	{
		CItem*          pItem = model.GetScriptElementItem(m_guid);
		IScriptElement* pScriptElement = GetSchematyc()->GetScriptRegistry().GetElement(m_guid);
		if(pItem && pScriptElement)
		{
			const INewScriptFile* pNewScriptFile = pScriptElement->GetNewFile();
			if(pNewScriptFile)
			{
				uint32          sourceControlAttributes = SCC_FILE_ATTRIBUTE_INVALID;
				ISourceControl* pSourceControl = GetIEditor()->GetSourceControl();
				if(pSourceControl)
				{
					const stack_string path = BrowserUtils::ConstructAbsolutePath(pNewScriptFile->GetName());
					sourceControlAttributes = pSourceControl->GetFileAttributes(path.c_str());
				}
				// #SchematycTODO : Do we need a fallback for when GetSourceControl() fails?

				const EBrowserItemStatusFlags prevItemStatusFlags = pItem->GetStatusFlags();
				EBrowserItemStatusFlags       newItemStatusFlags = prevItemStatusFlags & ~EBrowserItemStatusFlags::FileMask;
				if((sourceControlAttributes & SCC_FILE_ATTRIBUTE_NORMAL) != 0)
				{
					newItemStatusFlags |= EBrowserItemStatusFlags::Local; // #SchematycTODO : Shouldn't this flag already be determined by pNewScriptFile->GetFlags()?
				}
				if((sourceControlAttributes & SCC_FILE_ATTRIBUTE_INPAK) != 0)
				{
					newItemStatusFlags |= EBrowserItemStatusFlags::InPak; // #SchematycTODO : Shouldn't this flag already be determined by pNewScriptFile->GetFlags()?
				}
				if((sourceControlAttributes & SCC_FILE_ATTRIBUTE_READONLY) != 0)
				{
					newItemStatusFlags |= EBrowserItemStatusFlags::ReadOnly;
				}
				if((sourceControlAttributes & SCC_FILE_ATTRIBUTE_MANAGED) != 0)
				{
					newItemStatusFlags |= EBrowserItemStatusFlags::SourceControl;
				}
				if((sourceControlAttributes & SCC_FILE_ATTRIBUTE_CHECKEDOUT) != 0)
				{
					newItemStatusFlags |= EBrowserItemStatusFlags::CheckedOut;
				}

				if(newItemStatusFlags != prevItemStatusFlags)
				{
					pItem->SetStatusFlags(newItemStatusFlags);

					QModelIndex index = model.ItemToIndex(pItem);
					model.dataChanged(index, index);
				}
			}

			return ETaskStatus::Repeat;
		}
		return ETaskStatus::Done;
	}

	CBrowserModel::CBrowserModel(QObject* pParent)
		: QAbstractItemModel(pParent)
	{
		GetSchematyc()->GetScriptRegistry().Signals().change.Connect(ScriptRegistryChangeSignal::Delegate::FromMemberFunction<CBrowserModel, &CBrowserModel::OnScriptRegistryChange>(*this), m_connectionScope);
		Populate();
	}

	QModelIndex CBrowserModel::index(int row, int column, const QModelIndex& parent) const 
	{
		CItem* pParentItem = ItemFromIndex(parent);
		if(pParentItem)
		{
			CItem* pChildItem = pParentItem->GetChildByIdx(row);
			return pChildItem ? QAbstractItemModel::createIndex(row, column, pChildItem) : QModelIndex();
		}
		else
		{
			return QAbstractItemModel::createIndex(row, column, m_pRootItem.get());
		}
	}

	QModelIndex CBrowserModel::parent(const QModelIndex& index) const
	{
		CItem* pItem = ItemFromIndex(index);
		if(pItem)
		{
			CItem* pParentItem = pItem->GetParent();
			if(pParentItem)
			{
				return ItemToIndex(pParentItem, index.column());
			}
		}
		return QModelIndex();
	}

	int CBrowserModel::rowCount(const QModelIndex& parent) const
	{
		CItem* pParentItem = ItemFromIndex(parent);
		if(pParentItem)
		{
			return pParentItem->GetChildCount();
		}
		else
		{
			return 1;
		}
	}

	int CBrowserModel::columnCount(const QModelIndex& parent) const
	{
		return EBrowserColumn::Count;
	}

	bool CBrowserModel::hasChildren(const QModelIndex &parent) const
	{
		CItem* pParentItem = ItemFromIndex(parent);
		if(pParentItem)
		{
			return pParentItem->GetChildCount() > 0;
		}
		else
		{
			return true;
		}
	}

	QVariant CBrowserModel::data(const QModelIndex& index, int role) const
	{
		CItem* pItem = ItemFromIndex(index);
		if(pItem)
		{
			switch(role)
			{
			case Qt::DisplayRole:
				{
					switch(index.column())
					{
					case EBrowserColumn::Name:
						{
							return QString(pItem->GetName());
						}
					case EBrowserColumn::Status:
						{
							return QString(pItem->GetStatusText());
						}
					}
					break;
				}
			case Qt::DecorationRole:
				{
					switch(index.column())
					{
					case EBrowserColumn::Name:
						{
							return QIcon(pItem->GetIcon());
						}
					case EBrowserColumn::Status:
						{
							return QIcon(pItem->GetStatusIcon());
						}
					}
					break;
				}
			case Qt::EditRole:
				{
					switch(index.column())
					{
					case EBrowserColumn::Name:
						{
							return pItem->GetScriptElement() ? QString(pItem->GetName()) : QVariant();
						}
					}
					break;
				}
			case Qt::ToolTipRole:
				{
					switch(index.column())
					{
					case EBrowserColumn::Status:
						{
							return QString(pItem->GetStatusToolTip());
						}
					}
					break;
				}
			case Qt::ForegroundRole:
				{
					switch(index.column())
					{
					case EBrowserColumn::Name:
						{
							return pItem->GetColor();
						}
					}
					break;
				}
			case Qt::SizeHintRole:
				{
					return BrowserUtils::g_defaultColumnSize[index.column()];
				}
			}
		}
		return QVariant();
	}

	bool CBrowserModel::setData(const QModelIndex &index, const QVariant &value, int role)
	{
		if(role == Qt::EditRole)
		{
			CItem* pItem = ItemFromIndex(index);
			if(pItem && pItem->GetScriptElement())
			{
				pItem->SetName(value.toString().toStdString().c_str());
				return true;
			}
		}
		return false;
	}

	QVariant CBrowserModel::headerData(int section, Qt::Orientation orientation, int role) const
	{
		if(orientation == Qt::Horizontal)
		{
			switch(role)
			{
			case Qt::DisplayRole:
				{
					switch(section)
					{
					case EBrowserColumn::Name:
						{
							return QString("Name");
						}
					case EBrowserColumn::Status:
						{
							return QString("Status");
						}
					}
					break;
				}
			case Qt::SizeHintRole:
				{
					return BrowserUtils::g_defaultColumnSize[section];
				}
			}
		}
		return QVariant();
	}

	Qt::ItemFlags CBrowserModel::flags(const QModelIndex& index) const
	{
		return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
	}

	void CBrowserModel::ProcessTasks()
	{
		if(!m_tasks.empty())
		{
			ITaskPtr pTask = m_tasks.front();
			m_tasks.pop();
			switch(pTask->Execute(*this))
			{
			case ETaskStatus::Repeat:
				{
					m_tasks.push(pTask);
					break;
				}
			}
		}
	}

	void CBrowserModel::Reset()
	{
		QAbstractItemModel::beginResetModel();
		m_scriptElementItems.clear();
		Populate();
		QAbstractItemModel::endResetModel();
	}

	QModelIndex CBrowserModel::ItemToIndex(CItem* pItem, int column) const
	{
		if(pItem)
		{
			CItem* pParentItem = pItem->GetParent();
			if(pParentItem)
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

	CBrowserModel::CItem* CBrowserModel::ItemFromIndex(const QModelIndex& index) const
	{
		return static_cast<CItem*>(index.internalPointer());
	}

		CBrowserModel::CItem* CBrowserModel::GetScriptElementItem(const SGUID& guid)
	{
		ScriptElementItems::iterator itItem = m_scriptElementItems.find(guid);
		return itItem != m_scriptElementItems.end() ? itItem->second : nullptr;
	}

	void CBrowserModel::Populate()
	{
		m_pRootItem = std::make_shared<CItem>(EBrowserItemType::Root, "Root", BrowserUtils::g_szScriptModuleIcon);
		
		auto visitScriptElement = [this] (IScriptElement& scriptElement) -> EVisitStatus
		{
			const IScriptElement* pParentScriptElement = scriptElement.GetParent();
			if(pParentScriptElement)
			{
				CItem* pParentItem = GetScriptElementItem(pParentScriptElement->GetGUID());
				if(!pParentItem)
				{
					pParentItem = m_pRootItem.get();
				}
				CreateScriptElementItem(scriptElement, pParentItem);
			}
			return EVisitStatus::Continue;
		};
		GetSchematyc()->GetScriptRegistry().VisitElements(ScriptElementVisitor::FromLambdaFunction(visitScriptElement), nullptr, EVisitFlags::RecurseHierarchy);
		
		m_pRootItem->SortChildren(); // #SchematycTODO : Is this really the best way to keep things sorted?
	}

	void CBrowserModel::OnScriptRegistryChange(EScriptRegistryChange change, IScriptElement* pScriptElement)
	{
		switch(change)
		{
		case EScriptRegistryChange::ElementAdded:
			{
				if(pScriptElement)
				{
					OnScriptElementAdded(*pScriptElement);
				}
				break;
			}
		case EScriptRegistryChange::ElementRemoved:
			{
				if(pScriptElement)
				{
					OnScriptElementRemoved(*pScriptElement);
				}
				break;
			}
		}
	}

	void CBrowserModel::OnScriptElementAdded(IScriptElement& scriptElement)
	{
		IScriptElement* pParentScriptElement = scriptElement.GetParent();
		if(pParentScriptElement)
		{
			CItem* pParentItem = GetScriptElementItem(pParentScriptElement->GetGUID());
			if(!pParentItem)
			{
				pParentItem = m_pRootItem.get();
			}

			// #SchematycTODO : This doesn't work when new elements are assigned to a filter!

			const int row = pParentItem->GetChildCount();
			QAbstractItemModel::beginInsertRows(ItemToIndex(pParentItem), row, row);

			CreateScriptElementItem(scriptElement, pParentItem);

			QAbstractItemModel::endInsertRows();
		}
	}

	void CBrowserModel::OnScriptElementRemoved(IScriptElement& scriptElement)
	{
		ScriptElementItems::iterator itItem = m_scriptElementItems.find(scriptElement.GetGUID());
		if(itItem != m_scriptElementItems.end())
		{
			CItem*      pItem = itItem->second;
			QModelIndex index = ItemToIndex(pItem);
			QAbstractItemModel::beginRemoveRows(index.parent(), index.row(), index.row());

			CItem* pParentItem = pItem->GetParent();
			if(pParentItem)
			{
				pParentItem->RemoveChild(pItem);
			}
			m_scriptElementItems.erase(itItem);

			QAbstractItemModel::endRemoveRows();
		}
	}

	CBrowserModel::CItemPtr CBrowserModel::CreateScriptElementItem(IScriptElement& scriptElement, CItem* pParentItem)
	{
		// Select parent/filter item.
		const char* szFilter = m_rules.GetScriptElementFilter(scriptElement.GetElementType());
		if(szFilter)
		{
			CItem* pFilterItem = pParentItem->GetChildByTypeAndName(EBrowserItemType::Filter, szFilter);
			if(!pFilterItem)
			{
				CItemPtr pNewFilterItem = std::make_shared<CItem>(EBrowserItemType::Filter, szFilter);
				pParentItem->AddChild(pNewFilterItem);
				// SchematycTODO : Can we ensure all filters are expanded by default?
				pFilterItem = pNewFilterItem.get();
			}
			pParentItem = pFilterItem;
		}
		// Create item.
		const SGUID guid = scriptElement.GetGUID();
		CItemPtr    pItem = std::make_shared<CItem>(&scriptElement, scriptElement.GetName(), BrowserUtils::GetScriptElementIcon(scriptElement.GetElementType()));
		pParentItem->AddChild(pItem);
		m_scriptElementItems.insert(ScriptElementItems::value_type(guid, pItem.get()));
		// Set initial state of item.
		pItem->Validate();
		const INewScriptFile* pNewScriptFile = scriptElement.GetNewFile();
		if(pNewScriptFile)
		{
			pItem->SetStatusToolTip(pNewScriptFile->GetName());
			ScheduleTask(std::make_shared<CSourceControlRefreshTask>(guid));
		}
		return pItem;
	}

	void CBrowserModel::ScheduleTask(const ITaskPtr& pTask)
	{
		m_tasks.push(pTask);
	}

	CBrowserTreeView::CBrowserTreeView(QWidget* pParent)
		: QTreeView(pParent)
	{}

	void CBrowserTreeView::keyPressEvent(QKeyEvent* pKeyEvent)
	{
		bool bEventHandled = false;
		keyPress(pKeyEvent, bEventHandled);
		if(!bEventHandled)
		{
			QTreeView::keyPressEvent(pKeyEvent);
		}
	}

	CBrowserWidget::CBrowserWidget(QWidget* pParent)
		: QWidget(pParent)
	{
		m_pMainLayout       = new QBoxLayout(QBoxLayout::TopToBottom);
		m_pFilterLayout     = new QBoxLayout(QBoxLayout::LeftToRight);
		m_pControlLayout    = new QBoxLayout(QBoxLayout::LeftToRight);
		m_pFilterButtonMenu = new QMenu(this);
		m_pFilterButton     = new QPushButton("Filters", this);
		m_pSearchFilter     = new QLineEdit(this);
		m_pBackButton       = new QPushButton(QIcon(BrowserUtils::g_szBackIcon), "Back", this);
		m_pAddButtonMenu    = new QMenu("Add", this);
		m_pAddButton        = new QPushButton(QIcon(BrowserUtils::g_szAddIcon), "Add", this);
		m_pTreeView         = new CBrowserTreeView(this);
		m_pModel            = new CBrowserModel(this);
		m_pFilter           = new CBrowserFilter(this, *m_pModel);

		PopulateFilterMenu(*m_pFilterButtonMenu);
		m_pFilterButton->setMenu(m_pFilterButtonMenu);

		m_pSearchFilter->setPlaceholderText("Search");
		// #SchematycTODO : Add drop down history to filter text?

		PopulateAddMenu(*m_pAddButtonMenu);
		m_pAddButton->setMenu(m_pAddButtonMenu);

		m_pFilter->setDynamicSortFilter(true);
		m_pFilter->setSourceModel(m_pModel);
		m_pTreeView->setModel(m_pFilter);
		m_pTreeView->sortByColumn(0, Qt::AscendingOrder);
		m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
		m_pTreeView->setEditTriggers(QAbstractItemView::SelectedClicked);
		m_pTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
		m_pTreeView->setExpandsOnDoubleClick(false);

		connect(m_pBackButton, SIGNAL(clicked()), this, SLOT(OnBackButtonClicked()));
		connect(m_pSearchFilter, SIGNAL(textChanged(const QString&)), this, SLOT(OnSearchFilterChanged(const QString&)));
		connect(m_pTreeView, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnTreeViewDoubleClicked(const QModelIndex&)));
		connect(m_pTreeView, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(OnTreeViewCustomContextMenuRequested(const QPoint&)));
		connect(m_pTreeView, SIGNAL(keyPress(QKeyEvent*, bool&)), this, SLOT(OnTreeViewKeyPress(QKeyEvent*, bool&)));
		connect(m_pTreeView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)), this, SLOT(OnSelectionChanged(const QItemSelection&, const QItemSelection&)));

		QWidget::startTimer(100);
	}

	CBrowserWidget::~CBrowserWidget()
	{
		m_pTreeView->setModel(nullptr);
	}

	void CBrowserWidget::InitLayout()
	{
		QWidget::setLayout(m_pMainLayout);
		m_pMainLayout->setSpacing(1);
		m_pMainLayout->setMargin(1);

		m_pMainLayout->addLayout(m_pFilterLayout, 0);
		m_pMainLayout->addLayout(m_pControlLayout, 0);
		m_pMainLayout->addWidget(m_pTreeView, 1);

		m_pFilterLayout->setSpacing(2);
		m_pFilterLayout->setMargin(4);

		m_pFilterLayout->addWidget(m_pFilterButton, 0);
		m_pFilterLayout->addWidget(m_pSearchFilter, 0);

		m_pControlLayout->setSpacing(2);
		m_pControlLayout->setMargin(4);

		m_pControlLayout->addWidget(m_pBackButton, 0);
		m_pControlLayout->addWidget(m_pAddButton, 0);

		for(int column = 0; column < EBrowserColumn::Count; ++ column)
		{
			m_pTreeView->header()->resizeSection(column, BrowserUtils::g_defaultColumnSize[column].width());
		}
	}

	void CBrowserWidget::Reset()
	{
		m_pModel->Reset();
	}

	SBrowserSignals& CBrowserWidget::Signals()
	{
		return m_signals;
	}

	void CBrowserWidget::OnBackButtonClicked()
	{
		SetFocus(m_pFilter->GetFocus().parent());
	}

	void CBrowserWidget::OnSearchFilterChanged(const QString& text)
	{
		if(m_pSearchFilter)
		{
			m_pFilter->SetFilterText(text.toStdString().c_str());
			m_pFilter->invalidate();
			ExpandAll();
		}
	}

	void CBrowserWidget::OnTreeViewDoubleClicked(const QModelIndex& index)
	{
		SetFocus(TreeViewToModelIndex(index));
	}

	void CBrowserWidget::OnTreeViewCustomContextMenuRequested(const QPoint& position)
	{
		QModelIndex           index = m_pTreeView->indexAt(position);
		CBrowserModel::CItem* pItem = m_pModel->ItemFromIndex(TreeViewToModelIndex(index));
		if(pItem)
		{
			QMenu addMenu("Add"); // #SchematycTODO : This should be declared in case EBrowserColumn::Name:!!!
			QMenu contextMenu;
			switch(index.column())
			{
			case EBrowserColumn::Name:
				{
					// #SchematycTODO : Don't show add menu unless item is of type 'root' or 'script element'!!!
					PopulateAddMenu(addMenu);
					contextMenu.addMenu(&addMenu);

					if(pItem->GetScriptElement())
					{
						QAction* pRenameAction = contextMenu.addAction(QString("Rename"));
						connect(pRenameAction, SIGNAL(triggered()), this, SLOT(OnRenameItem()));

						QAction* pRemoveAction = contextMenu.addAction(QString("Remove"));
						connect(pRemoveAction, SIGNAL(triggered()), this, SLOT(OnRemoveItem()));
					}

					break;
				}
			case EBrowserColumn::Status:
				{
					const EBrowserItemStatusFlags itemStatusFlags = pItem->GetStatusFlags();
					if((itemStatusFlags & EBrowserItemStatusFlags::Local) != 0)
					{
						if((itemStatusFlags & EBrowserItemStatusFlags::ReadOnly) == 0)
						{
							QAction* pSaveAction = contextMenu.addAction(QString("Save"));
							connect(pSaveAction, SIGNAL(triggered()), this, SLOT(OnSave()));
						}

						if((itemStatusFlags & EBrowserItemStatusFlags::SourceControl) == 0)
						{
							QAction* pAddToSourceControlAction = contextMenu.addAction(QString("Add To Source Control"));
							connect(pAddToSourceControlAction, SIGNAL(triggered()), this, SLOT(OnAddToSourceControl()));
						}
						else
						{
							if((itemStatusFlags & EBrowserItemStatusFlags::CheckedOut) == 0)
							{
								QAction* pGetLatestRevisionAction = contextMenu.addAction(QString("Get Latest Revision"));
								connect(pGetLatestRevisionAction, SIGNAL(triggered()), this, SLOT(OnGetLatestRevision()));

								QAction* pCheckOutAction = contextMenu.addAction(QString("Check Out"));
								connect(pCheckOutAction, SIGNAL(triggered()), this, SLOT(OnCheckOut()));
							}
							else
							{
								QAction* pRevertAction = contextMenu.addAction(QString("Revert"));
								connect(pRevertAction, SIGNAL(triggered()), this, SLOT(OnRevert()));
							}

							QAction* pDiffAgainsLatestRevisionAction = contextMenu.addAction(QString("Diff Against Latest Revision"));
							connect(pDiffAgainsLatestRevisionAction, SIGNAL(triggered()), this, SLOT(OnDiffAgainstLatestRevision()));
						}
						contextMenu.addSeparator();

						QAction* pShowInExplorerAction = contextMenu.addAction(QString("Show In Explorer"));
						connect(pShowInExplorerAction, SIGNAL(triggered()), this, SLOT(OnShowInExplorer()));
					}
					else if((itemStatusFlags & EBrowserItemStatusFlags::InPak) != 0)
					{
						QAction* pExtractAction = contextMenu.addAction(QString("Extract"));
						connect(pExtractAction, SIGNAL(triggered()), this, SLOT(OnExtract()));
					}

					break;
				}
			}

			const QPoint globalPosition = m_pTreeView->viewport()->mapToGlobal(position);
			contextMenu.exec(globalPosition);
		}
	}

	void CBrowserWidget::OnTreeViewKeyPress(QKeyEvent* pKeyEvent, bool& bEventHandled)
	{
		if((pKeyEvent->key() == Qt::Key_Enter) || (pKeyEvent->key() == Qt::Key_Return))
		{
			SetFocus(TreeViewToModelIndex(GetTreeViewSelection()));
			bEventHandled = true;
		}
		else if(pKeyEvent->key() == Qt::Key_Backspace)
		{
			SetFocus(m_pFilter->GetFocus().parent());
			bEventHandled = true;
		}
		else
		{
			bEventHandled = HandleKeyPress(pKeyEvent);
		}
	}

	void CBrowserWidget::OnSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
	{
		CBrowserModel::CItem* pItem = m_pModel->ItemFromIndex(TreeViewToModelIndex(GetTreeViewSelection()));
		m_signals.selection.Send(pItem ? pItem->GetScriptElement() : nullptr);
	}

	void CBrowserWidget::OnAddItem()
	{
		QAction* pAddItemAction = static_cast<QAction*>(sender());
		if(pAddItemAction)
		{
			CBrowserModel::CItem* pItem = m_pModel->ItemFromIndex(TreeViewToModelIndex(GetTreeViewSelection()));
			if(pItem)
			{
				switch(pItem->GetType())
				{
				case EBrowserItemType::Root:
				case EBrowserItemType::ScriptElement:
					{
						TScriptRegistry& scriptRegistry = GetSchematyc()->GetScriptRegistry();
						IScriptElement*  pScriptScope = pItem->GetScriptElement();
						IScriptElement*  pScriptElement = nullptr;
						switch(static_cast<EScriptElementType>(pAddItemAction->data().toInt()))
						{
						case EScriptElementType::Module:
							{
								// #SchematycTODO : Does some of this logic belong in CBrowserRules?
								stack_string name = "NewModule";
								BrowserUtils::MakeScriptElementNameUnique(name, pScriptScope); 
								pScriptElement = scriptRegistry.AddModule(name.c_str(), pScriptScope);
								break;
							}
						case EScriptElementType::Enumeration:
							{
								// #SchematycTODO : Does some of this logic belong in CBrowserRules?
								stack_string name = "NewEnumeration";
								BrowserUtils::MakeScriptElementNameUnique(name, pScriptScope); 
								pScriptElement = scriptRegistry.AddEnumeration(name.c_str(), pScriptScope);
								break;
							}
						case EScriptElementType::Function:
							{
								// #SchematycTODO : Does some of this logic belong in CBrowserRules?
								stack_string name = "NewFunction";
								BrowserUtils::MakeScriptElementNameUnique(name, pScriptScope); 
								pScriptElement = scriptRegistry.AddFunction(name.c_str(), pScriptScope);
								break;
							}
						case EScriptElementType::Class:
							{
								// #SchematycTODO : Does some of this logic belong in CBrowserRules?

								stack_string name = "NewClass";
								BrowserUtils::MakeScriptElementNameUnique(name, pScriptScope);

								SGUID foundationGUID;
								auto visitEnvFoundation = [&foundationGUID] (const IFoundationConstPtr& pFoundation) -> EVisitStatus
								{
									foundationGUID = pFoundation->GetGUID();
									return EVisitStatus::End;
								};
								GetSchematyc()->GetEnvRegistry().VisitFoundations(EnvFoundationVisitor::FromLambdaFunction(visitEnvFoundation));
								
								pScriptElement = scriptRegistry.AddClass(name.c_str(), foundationGUID, pScriptScope);

								break;
							}
						}
						if(pScriptElement)
						{
							CBrowserModel::CItem* pItem = m_pModel->GetScriptElementItem(pScriptElement->GetGUID());
							if(pItem)
							{
								const QModelIndex itemIndex = TreeViewFromModelIndex(m_pModel->ItemToIndex(pItem));
								m_pTreeView->selectionModel()->setCurrentIndex(itemIndex, QItemSelectionModel::ClearAndSelect);
								m_pTreeView->edit(itemIndex);
							}
						}
						break;
					}
				}
			}
		}
	}

	void CBrowserWidget::OnRenameItem()
	{
		m_pTreeView->edit(GetTreeViewSelection());
	}

	void CBrowserWidget::OnRemoveItem()
	{
		CBrowserModel::CItem* pItem = m_pModel->ItemFromIndex(TreeViewToModelIndex(GetTreeViewSelection()));
		if(pItem)
		{
			IScriptElement* pScriptElement = pItem->GetScriptElement();
			if(pScriptElement)
			{
				QString message;
				message.sprintf("Are you sure you want to remove '%s'?", pScriptElement->GetName());
				if(QMessageBox::question(this, "Remove Script Element?", message, QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
				{
					GetSchematyc()->GetScriptRegistry().RemoveElement(pScriptElement->GetGUID());
				}
			}
		}
	}

	void CBrowserWidget::OnSave()
	{
		// #SchematycTODO : Implement this function!!!
	}

	void CBrowserWidget::OnAddToSourceControl()
	{
		ISourceControl* pSourceControl = GetIEditor()->GetSourceControl();
		if(pSourceControl)
		{
			CBrowserModel::CItem* pItem = m_pModel->ItemFromIndex(TreeViewToModelIndex(GetTreeViewSelection()));
			if(pItem)
			{
				IScriptElement* pScriptElement = pItem->GetScriptElement();
				if(pScriptElement)
				{
					INewScriptFile* pNewScriptFile = pScriptElement->GetNewFile();
					if(pNewScriptFile)
					{
						const stack_string path = BrowserUtils::ConstructAbsolutePath(pNewScriptFile->GetName());
						pSourceControl->Add(path.c_str());
					}
				}
			}
		}

		// #SchematycTODO : Implement this function!!!
	}

	void CBrowserWidget::OnGetLatestRevision()
	{
		ISourceControl* pSourceControl = GetIEditor()->GetSourceControl();
		if(pSourceControl)
		{
			CBrowserModel::CItem* pItem = m_pModel->ItemFromIndex(TreeViewToModelIndex(GetTreeViewSelection()));
			if(pItem)
			{
				IScriptElement* pScriptElement = pItem->GetScriptElement();
				if(pScriptElement)
				{
					INewScriptFile* pNewScriptFile = pScriptElement->GetNewFile();
					if(pNewScriptFile)
					{
						const stack_string path = BrowserUtils::ConstructAbsolutePath(pNewScriptFile->GetName());
						pSourceControl->GetLatestVersion(path.c_str());
					}
				}
			}
		}
	}

	void CBrowserWidget::OnCheckOut()
	{
		ISourceControl* pSourceControl = GetIEditor()->GetSourceControl();
		if(pSourceControl)
		{
			CBrowserModel::CItem* pItem = m_pModel->ItemFromIndex(TreeViewToModelIndex(GetTreeViewSelection()));
			if(pItem)
			{
				IScriptElement* pScriptElement = pItem->GetScriptElement();
				if(pScriptElement)
				{
					INewScriptFile* pNewScriptFile = pScriptElement->GetNewFile();
					if(pNewScriptFile)
					{
						const stack_string path = BrowserUtils::ConstructAbsolutePath(pNewScriptFile->GetName());
						pSourceControl->CheckOut(path.c_str());
					}
				}
			}
		}
	}

	void CBrowserWidget::OnRevert()
	{
		ISourceControl* pSourceControl = GetIEditor()->GetSourceControl();
		if(pSourceControl)
		{
			CBrowserModel::CItem* pItem = m_pModel->ItemFromIndex(TreeViewToModelIndex(GetTreeViewSelection()));
			if(pItem)
			{
				IScriptElement* pScriptElement = pItem->GetScriptElement();
				if(pScriptElement)
				{
					INewScriptFile* pNewScriptFile = pScriptElement->GetNewFile();
					if(pNewScriptFile)
					{
						const stack_string path = BrowserUtils::ConstructAbsolutePath(pNewScriptFile->GetName());
						pSourceControl->UndoCheckOut(path.c_str());
					}
				}
			}
		}
	}

	void CBrowserWidget::OnDiffAgainstLatestRevision()
	{
		/*ISourceControl* pSourceControl = GetIEditor()->GetSourceControl();
		if(pSourceControl)
		{
			CBrowserModel::CItem* pItem = m_pModel->ItemFromIndex(TreeViewToModelIndex(GetTreeViewSelection()));
			if(pItem)
			{
				IScriptElement* pScriptElement = pItem->GetScriptElement();
				if(pScriptElement)
				{
					INewScriptFile* pNewScriptFile = pScriptElement->GetNewFile();
					if(pNewScriptFile)
					{
						const stack_string path = BrowserUtils::ConstructAbsolutePath(pNewScriptFile->GetName());
						
						char depotPath[MAX_PATH] = "";
						pSourceControl->GetInternalPath(path.c_str(), depotPath, MAX_PATH);

						stack_string tmpPath = path;
						{
							int64 haveRevision = 0;
							int64 headRevision = 0;
							pSourceControl->GetFileRev(depotPath, &haveRevision, &headRevision);

							stack_string postFix;
							postFix.Format(".v%d", headRevision);

							tmpPath.insert(tmpPath.rfind('.'), postFix);
						}

						if(pSourceControl->CopyDepotFile(depotPath, tmpPath.c_str()))
						{
							stack_string diffToolPath = "E:\\DATA\\game_hunt\\Tools\\GameHunt\\Diff\\Diff.exe"; // #SchematycTODO : This should not be hard coded!!!

							stack_string diffToolParams = "Diff "; // #SchematycTODO : This should not be hard coded!!!
							diffToolParams.append(path);
							diffToolParams.append(" ");
							diffToolParams.append(tmpPath);

							ShellExecute(NULL, "open", diffToolPath.c_str(), diffToolParams.c_str(), NULL, SW_SHOWDEFAULT);

							CrySleep(1000);
							SetFileAttributes(tmpPath, FILE_ATTRIBUTE_NORMAL);
							DeleteFile(tmpPath);
						}
					}
				}
			}
		}*/
	}

	void CBrowserWidget::OnShowInExplorer()
	{
		// #SchematycTODO : Implement this function!!!
	}

	void CBrowserWidget::OnExtract()
	{
		// #SchematycTODO : Implement this function!!!
	}

	void CBrowserWidget::keyPressEvent(QKeyEvent* pKeyEvent)
	{
		HandleKeyPress(pKeyEvent);
	}

	void CBrowserWidget::timerEvent(QTimerEvent* pTimerEvent)
	{
		m_pModel->ProcessTasks();
	}

	void CBrowserWidget::ExpandAll()
	{
		m_pTreeView->expandAll();
	}

	void CBrowserWidget::PopulateFilterMenu(QMenu& menu)
	{
		QAction* pToggleSearchFilter = menu.addAction("Search");
		//connect(pToggleSearchFilter, SIGNAL(triggered()), this, SLOT(OnToggleSearchFilter()));
	}

	void CBrowserWidget::PopulateAddMenu(QMenu& menu)
	{
		QAction* pAddModuleAction = menu.addAction("Module");
		pAddModuleAction->setData(static_cast<int>(EScriptElementType::Module));
		connect(pAddModuleAction, SIGNAL(triggered()), this, SLOT(OnAddItem()));

		QAction* pAddEnumerationAction = menu.addAction("Enumeration");
		pAddEnumerationAction->setData(static_cast<int>(EScriptElementType::Enumeration));
		connect(pAddEnumerationAction, SIGNAL(triggered()), this, SLOT(OnAddItem()));

		QAction* pAddFunctionAction = menu.addAction("Function");
		pAddFunctionAction->setData(static_cast<int>(EScriptElementType::Function));
		connect(pAddFunctionAction, SIGNAL(triggered()), this, SLOT(OnAddItem()));

		QAction* pAddClassAction = menu.addAction("Class");
		pAddClassAction->setData(static_cast<int>(EScriptElementType::Class));
		connect(pAddClassAction, SIGNAL(triggered()), this, SLOT(OnAddItem()));
	}

	void CBrowserWidget::SetFocus(const QModelIndex& index)
	{
		m_pTreeView->setRootIndex(TreeViewFromModelIndex(index.parent()));

		m_pFilter->SetFocus(index);
		m_pFilter->invalidate();
	}

	bool CBrowserWidget::HandleKeyPress(QKeyEvent* pKeyEvent)
	{
		if(pKeyEvent->modifiers() == Qt::CTRL)
		{
			if(pKeyEvent->key() == Qt::Key_A)
			{
				m_pAddButton->click();
				return true;
			}
			else if(pKeyEvent->key() == Qt::Key_F) // #SchematycTODO : Is this really the best shortcut key?
			{
				m_pFilterButton->click();
				return true;
			}
			else if(pKeyEvent->key() == Qt::Key_S) // #SchematycTODO : Is this really the best shortcut key?
			{
				m_pSearchFilter->setFocus();
				return true;
			}
		}
		return false;
	}

	QModelIndex CBrowserWidget::GetTreeViewSelection() const
	{
		const QModelIndexList& indices = m_pTreeView->selectionModel()->selection().indexes();
		if(indices.size() == EBrowserColumn::Count)
		{
			return indices.front();
		}
		else
		{
			return QModelIndex();
		}
	}

	QModelIndex CBrowserWidget::TreeViewToModelIndex(const QModelIndex& index) const
	{
		return m_pFilter->mapToSource(index);
	}

	QModelIndex CBrowserWidget::TreeViewFromModelIndex(const QModelIndex& index) const
	{
		return m_pFilter->mapFromSource(index);
	}

	BEGIN_MESSAGE_MAP(CBrowserWnd, CWnd)
		ON_WM_SIZE()
	END_MESSAGE_MAP()

	CBrowserWnd::CBrowserWnd()
		: m_pParentWndWidget(nullptr)
		, m_pBrowserWidget(nullptr)
		, m_pLayout(nullptr)
	{}

	CBrowserWnd::~CBrowserWnd()
	{
		SAFE_DELETE(m_pLayout);
		SAFE_DELETE(m_pBrowserWidget);
		SAFE_DELETE(m_pParentWndWidget);
	}

	void CBrowserWnd::Init()
	{
		m_pParentWndWidget = new QParentWndWidget(GetSafeHwnd());
		m_pBrowserWidget   = new CBrowserWidget(m_pParentWndWidget);
		m_pLayout          = new QBoxLayout(QBoxLayout::TopToBottom);
	}

	void CBrowserWnd::InitLayout()
	{
		m_pLayout->setContentsMargins(0, 0, 0, 0);
		m_pLayout->addWidget(m_pBrowserWidget, 1);
		m_pParentWndWidget->setLayout(m_pLayout);
		m_pParentWndWidget->show();
		m_pBrowserWidget->InitLayout();
	}

	void CBrowserWnd::Reset()
	{
		m_pBrowserWidget->Reset();
	}

	SBrowserSignals& CBrowserWnd::Signals()
	{
		return m_pBrowserWidget->Signals();
	}

	void CBrowserWnd::OnSize(UINT nType, int cx, int cy)
	{
		if(m_pParentWndWidget)
		{
			CRect rect; 
			CWnd::GetClientRect(&rect);
			m_pParentWndWidget->setGeometry(0, 0, rect.Width(), rect.Height());
		}
	}
}
