// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BrowserCtrl.h"

#include <CrySchematyc2/Serialization/ISerializationContext.h>
#include <CrySchematyc2/Serialization/IValidatorArchive.h>
#include <CrySchematyc2/Script/IScriptRegistry.h>

#include "AddAbstractInterfaceImplementationDlg.h"
#include "AddContainerDlg.h"
#include "AddGraphDlg.h"
#include "AddStateDlg.h"
#include "BrowserIcons.h"
#include "BrowserUtils.h"
#include "PluginUtils.h"

namespace Schematyc2
{
	namespace
	{
		static const COLORREF ErrorItemColor = RGB(255, 0, 0);

		// #SchematycTODO : Can/should we merge SDocVisitor with the browser control itself?
		struct SDocVisitor
		{
			inline SDocVisitor(CBrowserCtrl& _browserCtrl, const CBrowserCtrlItemPtr& _pParentItem)
				: browserCtrl(_browserCtrl)
				, pParentItem(_pParentItem)
			{}

			inline EVisitStatus VisitInclude(const IScriptInclude& include) const
			{
				stack_string						name = include.GetName();
				stack_string::size_type	fileNamePos = name.rfind('/');
				if(fileNamePos != stack_string::npos)
				{
					name.erase(0, fileNamePos + 1);
				}
				stack_string::size_type	extensionPos = name.rfind('.');
				if(extensionPos != stack_string::npos)
				{
					name.erase(extensionPos);
				}
				browserCtrl.AddItem(name.c_str(), BrowserIcon::INCLUDE, pParentItem, include.GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
				return EVisitStatus::Continue;
			}

			inline EVisitStatus VisitGroup(const IScriptGroup& group) const
			{
				browserCtrl.AddItem(group.GetName(), BrowserIcon::GROUP, pParentItem, group.GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
				return EVisitStatus::Continue;
			}

			inline EVisitStatus VisitEnumeration(const IScriptEnumeration& enumeration) const
			{
				browserCtrl.AddItem(enumeration.GetName(), BrowserIcon::ENUMERATION, pParentItem, enumeration.GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
				return EVisitStatus::Continue;
			}

			inline EVisitStatus VisitStructure(const IScriptStructure& structure) const
			{
				browserCtrl.AddItem(structure.GetName(), BrowserIcon::STRUCTURE, pParentItem, structure.GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
				return EVisitStatus::Continue;
			}

			inline EVisitStatus VisitSignal(const IScriptSignal& signal) const
			{
				browserCtrl.AddItem(signal.GetName(), BrowserIcon::SIGNAL, pParentItem, signal.GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
				return EVisitStatus::Continue;
			}

			inline EVisitStatus VisitAbstractInterface(const IScriptAbstractInterface& abstractInterface) const
			{
				browserCtrl.AddItem(abstractInterface.GetName(), BrowserIcon::ABSTRACT_INTERFACE, pParentItem, abstractInterface.GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
				return EVisitStatus::Continue;
			}

			inline EVisitStatus VisitAbstractInterfaceFunction(const IScriptAbstractInterfaceFunction& abstractInterfaceFunction) const
			{
				browserCtrl.AddItem(abstractInterfaceFunction.GetName(), BrowserIcon::ABSTRACT_INTERFACE_FUNCTION, pParentItem, abstractInterfaceFunction.GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
				return EVisitStatus::Continue;
			}

			inline EVisitStatus VisitAbstractInterfaceTask(const IScriptAbstractInterfaceTask& abstractInterfaceTask) const
			{
				browserCtrl.AddItem(abstractInterfaceTask.GetName(), BrowserIcon::ABSTRACT_INTERFACE_TASK, pParentItem, abstractInterfaceTask.GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
				return EVisitStatus::Continue;
			}

			inline EVisitStatus VisitClass(const IScriptClass& _class) const
			{
				browserCtrl.AddItem(_class.GetName(), BrowserIcon::CLASS, pParentItem, _class.GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
				return EVisitStatus::Continue;
			}

			inline EVisitStatus VisitClassBase(const IScriptClassBase& classBase) const
			{
				browserCtrl.AddItem(classBase.GetName(), BrowserIcon::CLASS_BASE, pParentItem, classBase.GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
				return EVisitStatus::Continue;
			}

			inline EVisitStatus VisitStateMachine(const IScriptStateMachine& stateMachine) const
			{
				browserCtrl.AddItem(stateMachine.GetName(), BrowserIcon::STATE_MACHINE, pParentItem, stateMachine.GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
				return EVisitStatus::Continue;
			}

			inline EVisitStatus VisitState(const IScriptState& state) const
			{
				browserCtrl.AddItem(state.GetName(), BrowserIcon::STATE, pParentItem, state.GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
				return EVisitStatus::Continue;
			}

			inline EVisitStatus VisitVariable(const IScriptVariable& variable) const
			{
				browserCtrl.AddItem(variable.GetName(), BrowserIcon::VARIABLE, pParentItem, variable.GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
				return EVisitStatus::Continue;
			}

			inline EVisitStatus VisitProperty(const IScriptProperty& property) const
			{
				browserCtrl.AddItem(property.GetName(), BrowserIcon::PROPERTY, pParentItem, property.GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
				return EVisitStatus::Continue;
			}

			inline EVisitStatus VisitContainer(const IScriptContainer& container) const
			{
				browserCtrl.AddItem(container.GetName(), BrowserIcon::CONTAINER, pParentItem, container.GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
				return EVisitStatus::Continue;
			}

			inline EVisitStatus VisitTimer(const IScriptTimer& timer) const
			{
				browserCtrl.AddItem(timer.GetName(), BrowserIcon::TIMER, pParentItem, timer.GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
				return EVisitStatus::Continue;
			}

			inline EVisitStatus VisitAbstractInterfaceImplementation(const IScriptAbstractInterfaceImplementation& abstractInterfaceImplementation) const
			{
				browserCtrl.AddItem(abstractInterfaceImplementation.GetName(), BrowserIcon::ABSTRACT_INTERFACE_IMPLEMENTATION, pParentItem, abstractInterfaceImplementation.GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
				return EVisitStatus::Continue;
			}

			inline EVisitStatus VisitComponentInstance(const IScriptComponentInstance& componentInstance) const
			{
				if(IComponentFactoryConstPtr pComponentFactory = GetSchematyc()->GetEnvRegistry().GetComponentFactory(componentInstance.GetComponentGUID()))
				{
					if((pComponentFactory->GetFlags() & EComponentFlags::HideInEditor) == 0)
					{
						browserCtrl.AddItem(componentInstance.GetName(), BrowserIcon::COMPONENT_INSTANCE, pParentItem, componentInstance.GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
					}
				}
				return EVisitStatus::Continue;
			}

			inline EVisitStatus VisitActionInstance(const IScriptActionInstance& actionInstance) const
			{
				browserCtrl.AddItem(actionInstance.GetName(), BrowserIcon::ACTION_INSTANCE, pParentItem, actionInstance.GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
				return EVisitStatus::Continue;
			}

			inline EVisitStatus VisitGraph(const IDocGraph& graph) const
			{
				size_t icon = BrowserIcon::INVALID;
				switch(graph.GetType())
				{
				case EScriptGraphType::AbstractInterfaceFunction:
					{
						icon = BrowserIcon::ABSTRACT_INTERFACE_FUNCTION_IMPLEMENTATION;
						break;
					}
				case EScriptGraphType::Function:
					{
						icon = BrowserIcon::FUNCTION_GRAPH;
						break;
					}
				case EScriptGraphType::Condition:
					{
						icon = BrowserIcon::CONDITION_GRAPH;
						break;
					}
				case EScriptGraphType::Constructor:
					{
						icon = BrowserIcon::CONSTRUCTOR;
						break;
					}
				case EScriptGraphType::Destructor:
					{
						icon = BrowserIcon::DESTRUCTOR;
						break;
					}
				case EScriptGraphType::SignalReceiver:
					{
						icon = BrowserIcon::SIGNAL_RECEIVER;
						break;
					}
				}
				if(icon != BrowserIcon::INVALID)
				{
					browserCtrl.AddItem(graph.GetName(), icon, pParentItem, graph.GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
				}
				return EVisitStatus::Continue;
			}

			CBrowserCtrl&       browserCtrl;
			CBrowserCtrlItemPtr pParentItem;
		};

		struct SSettingsVisitor
		{
			inline SSettingsVisitor(CBrowserCtrl& _browserCtrl, const CBrowserCtrlItemPtr& _pParentItem)
				: browserCtrl(_browserCtrl)
				, pParentItem(_pParentItem)
			{}

			inline EVisitStatus VisitSettings(const char* name, const IEnvSettingsPtr&) const
			{
				browserCtrl.AddItem(name, BrowserIcon::SETTINGS_DOC, pParentItem, SGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
				return EVisitStatus::Continue;
			}

			CBrowserCtrl&				browserCtrl;
			CBrowserCtrlItemPtr	pParentItem;
		};

		//////////////////////////////////////////////////////////////////////////
		inline void GetItemPath(const CBrowserCtrlItem& item, stack_string& path, bool includeGameFolder)
		{
			path.clear();
			if(includeGameFolder)
			{
				path.append(gEnv->pCryPak->GetGameFolder());
				path.append("/");
			}
			TStringVector	folderNames;
			for(CCustomTreeCtrlItemConstPtr pParentItem = item.shared_from_this(); pParentItem; pParentItem = pParentItem->GetParent())
			{
				switch(pParentItem->GetIcon())
				{
				case BrowserIcon::FOLDER:
					{
						if(pParentItem->GetParent())
						{
							folderNames.push_back(pParentItem->GetText());
						}
						else
						{
							folderNames.push_back(GetSchematyc()->GetOldScriptsFolder());
						}
						break;
					}
				case BrowserIcon::SETTINGS_FOLDER:
					{
						folderNames.push_back(GetSchematyc()->GetSettingsFolder());
						break;
					}
				default:
					{
						folderNames.push_back(pParentItem->GetText());
					}
				}
			}
			std::reverse(folderNames.begin(), folderNames.end());
			for(TStringVector::const_iterator iFolderName = folderNames.begin(), iEndFolderName = folderNames.end(); iFolderName != iEndFolderName; ++ iFolderName)
			{
				path.append(iFolderName->c_str());
				path.append("/");
			}
			path.TrimRight("/");
			path.TrimRight("*");
		}

		//////////////////////////////////////////////////////////////////////////
		inline CBrowserCtrlItemPtr GetRealParentItem(const CBrowserCtrlItemPtr& pItem)
		{
			for(CBrowserCtrlItemPtr pParentItem = pItem ? std::static_pointer_cast<CBrowserCtrlItem>(pItem->GetParent()) : CBrowserCtrlItemPtr(); pParentItem; pParentItem = std::static_pointer_cast<CBrowserCtrlItem>(pParentItem->GetParent()))
			{
				if(pParentItem->GetIcon() != BrowserIcon::GROUP)
				{
					return pParentItem;
				}
			}
			return CBrowserCtrlItemPtr();
		}

		//////////////////////////////////////////////////////////////////////////
		inline bool IsCopyableItem(size_t icon)
		{
			static const BrowserIcon::EValue copyableItems[] =
			{
				BrowserIcon::ENUMERATION,
				BrowserIcon::STRUCTURE,
				BrowserIcon::SIGNAL,
				BrowserIcon::ABSTRACT_INTERFACE,
				BrowserIcon::ABSTRACT_INTERFACE_FUNCTION,
				BrowserIcon::ABSTRACT_INTERFACE_TASK,
				BrowserIcon::CLASS,
				BrowserIcon::STATE_MACHINE,
				BrowserIcon::STATE,
				BrowserIcon::VARIABLE,
				BrowserIcon::PROPERTY,
				BrowserIcon::CONTAINER,
				BrowserIcon::TIMER
			};
			for(size_t iCopyableItem = 0; iCopyableItem < CRY_ARRAY_COUNT(copyableItems); ++ iCopyableItem)
			{
				if(copyableItems[iCopyableItem] == icon)
				{
					return true;
				}
			}
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		inline bool IsCloneableItem(size_t icon)
		{
			static const BrowserIcon::EValue cloneableItems[] =
			{
				BrowserIcon::DOC,
				BrowserIcon::GROUP,
				BrowserIcon::ENUMERATION,
				BrowserIcon::STRUCTURE,
				BrowserIcon::SIGNAL,
				BrowserIcon::ABSTRACT_INTERFACE,
				BrowserIcon::ABSTRACT_INTERFACE_FUNCTION,
				BrowserIcon::ABSTRACT_INTERFACE_TASK,
				BrowserIcon::CLASS,
				BrowserIcon::STATE_MACHINE,
				BrowserIcon::STATE,
				BrowserIcon::VARIABLE,
				BrowserIcon::PROPERTY,
				BrowserIcon::CONTAINER,
				BrowserIcon::TIMER,
				BrowserIcon::FUNCTION_GRAPH,
				BrowserIcon::CONDITION_GRAPH,
				BrowserIcon::SIGNAL_RECEIVER
			};
			for(size_t cloneableItemIdx = 0; cloneableItemIdx < CRY_ARRAY_COUNT(cloneableItems); ++ cloneableItemIdx)
			{
				if(cloneableItems[cloneableItemIdx] == icon)
				{
					return true;
				}
			}
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		inline bool IsRenameableItem(size_t icon)
		{
			static const BrowserIcon::EValue renameableItems[] =
			{
				BrowserIcon::GROUP,
				BrowserIcon::ENUMERATION,
				BrowserIcon::STRUCTURE,
				BrowserIcon::SIGNAL,
				BrowserIcon::ABSTRACT_INTERFACE,
				BrowserIcon::ABSTRACT_INTERFACE_FUNCTION,
				BrowserIcon::ABSTRACT_INTERFACE_TASK,
				BrowserIcon::CLASS,
				BrowserIcon::STATE_MACHINE,
				BrowserIcon::STATE,
				BrowserIcon::VARIABLE,
				BrowserIcon::PROPERTY,
				BrowserIcon::CONTAINER,
				BrowserIcon::TIMER,
				BrowserIcon::COMPONENT_INSTANCE,
				BrowserIcon::ACTION_INSTANCE,
				BrowserIcon::FUNCTION_GRAPH,
				BrowserIcon::CONDITION_GRAPH,
				BrowserIcon::SIGNAL_RECEIVER
			};
			for(size_t iRenameableItem = 0; iRenameableItem < CRY_ARRAY_COUNT(renameableItems); ++ iRenameableItem)
			{
				if(renameableItems[iRenameableItem] == icon)
				{
					return true;
				}
			}
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		inline bool IsRemoveableItem(size_t icon)
		{
			static const BrowserIcon::EValue removeableItems[] =
			{
				BrowserIcon::INCLUDE,
				BrowserIcon::GROUP,
				BrowserIcon::ENUMERATION,
				BrowserIcon::STRUCTURE,
				BrowserIcon::SIGNAL,
				BrowserIcon::ABSTRACT_INTERFACE,
				BrowserIcon::ABSTRACT_INTERFACE_FUNCTION,
				BrowserIcon::ABSTRACT_INTERFACE_TASK,
				BrowserIcon::CLASS,
				BrowserIcon::STATE_MACHINE,
				BrowserIcon::STATE,
				BrowserIcon::VARIABLE,
				BrowserIcon::PROPERTY,
				BrowserIcon::CONTAINER,
				BrowserIcon::TIMER,
				BrowserIcon::ABSTRACT_INTERFACE_IMPLEMENTATION,
				BrowserIcon::COMPONENT_INSTANCE,
				BrowserIcon::ACTION_INSTANCE,
				BrowserIcon::FUNCTION_GRAPH,
				BrowserIcon::CONDITION_GRAPH,
				BrowserIcon::CONSTRUCTOR,
				BrowserIcon::DESTRUCTOR,
				BrowserIcon::SIGNAL_RECEIVER,
			};
			for(size_t iRemoveableItem = 0; iRemoveableItem < CRY_ARRAY_COUNT(removeableItems); ++ iRemoveableItem)
			{
				if(removeableItems[iRemoveableItem] == icon)
				{
					return true;
				}
			}
			return false;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CBrowserCtrlItem::CBrowserCtrlItem(const char* szText, size_t icon, const CCustomTreeCtrlItemPtr& pParent, const SGUID& guid)
		: CCustomTreeCtrlItem(szText, icon, pParent, guid)
		, m_flags(EBrowserItemFlags::None)
	{}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrlItem::SetFlags(EBrowserItemFlags flags)
	{
		m_flags = flags;
	}

	//////////////////////////////////////////////////////////////////////////
	EBrowserItemFlags CBrowserCtrlItem::GetFlags() const
	{
		return m_flags;
	}

	//////////////////////////////////////////////////////////////////////////
	BEGIN_MESSAGE_MAP(CBrowserCtrl, CCustomTreeCtrl)
		ON_NOTIFY_REFLECT(TVN_SELCHANGED, OnSelChanged)
		ON_NOTIFY_REFLECT(TVN_BEGINLABELEDIT, OnBeginLabelEdit)
		ON_NOTIFY_REFLECT(TVN_ENDLABELEDIT, OnEndLabelEdit)
		ON_NOTIFY_REFLECT(TVN_BEGINDRAG, OnBeginDrag)
		ON_WM_SETFOCUS()
		ON_WM_LBUTTONDBLCLK()
		ON_NOTIFY_REFLECT(NM_RCLICK, OnRClick)
		ON_WM_KEYDOWN()
		ON_WM_CONTEXTMENU()
	END_MESSAGE_MAP()

	//////////////////////////////////////////////////////////////////////////
	CBrowserCtrl::CBrowserCtrl()
		: CCustomTreeCtrl()
	{}

	//////////////////////////////////////////////////////////////////////////
	BOOL CBrowserCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
	{
		LOADING_TIME_PROFILE_SECTION;
		if(CCustomTreeCtrl::Create(dwStyle | TVS_EDITLABELS, rect, pParentWnd, nID))
		{
			AddItem("documents", BrowserIcon::FOLDER, CBrowserCtrlItemPtr(), SGUID(), EBrowserRefreshFlags::Down);
			AddItem("settings", BrowserIcon::SETTINGS_FOLDER, CBrowserCtrlItemPtr(), SGUID(), EBrowserRefreshFlags::Down);
			return true;
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::RefreshDocs()
	{
		TCustomTreeCtrlItemPtrVector& items = CCustomTreeCtrl::GetItems();
		for(CCustomTreeCtrlItemPtr& pItem : items)
		{
			if(pItem->GetIcon() == BrowserIcon::DOC)
			{
				RefreshItem(std::static_pointer_cast<CBrowserCtrlItem>(pItem), EBrowserRefreshFlags::None);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::RefreshItem(const SGUID& guid)	// N.B. This temporary solution and ideally the browser control should interact directly with the detail panel.
	{
		CBrowserCtrlItemPtr	pItem = GetItem(guid);
		if(pItem)
		{
			RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::SelectItem(const SGUID& itemGuid, const SGUID& childGuid)
	{
		if(CBrowserCtrlItemPtr pItem = GetItem(itemGuid))
		{
			EnsureVisible(pItem);
			CCustomTreeCtrl::SelectItem(pItem->GetHTreeItem());

			SBrowserSelectionParams params(pItem);
			if(childGuid)
			{
				params.pChildGuid = &childGuid;
			}
			m_signals.selection.Send(params);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::DiffSelectedItem()
	{
		CBrowserCtrlItemPtr	pItem = GetSelectedItem();
		if (pItem)
		{
			OnDiffAgainstHaveVersion(pItem);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CBrowserCtrlItemPtr CBrowserCtrl::AddItem(const char* text, size_t icon, const CBrowserCtrlItemPtr& pParentItem, const SGUID& guid, EBrowserRefreshFlags refreshFlags)
	{
		LOADING_TIME_PROFILE_SECTION;
		CBrowserCtrlItemPtr	pItem(new CBrowserCtrlItem(text, icon, pParentItem, guid));
		CCustomTreeCtrl::AddItem(pItem);
		RefreshItem(pItem, refreshFlags);
		return pItem;
	}

	//////////////////////////////////////////////////////////////////////////
	CBrowserCtrlItemPtr CBrowserCtrl::GetSelectedItem()
	{
		return std::static_pointer_cast<CBrowserCtrlItem>(CCustomTreeCtrl::FindItem(CCustomTreeCtrl::GetSelectedItem()));
	}

	//////////////////////////////////////////////////////////////////////////
	TScriptFile* CBrowserCtrl::GetItemScriptFile(const CBrowserCtrlItem& item) const
	{
		if(item.GetIcon() == BrowserIcon::DOC)
		{
			stack_string scriptFileName;
			GetItemPath(item, scriptFileName, false);
			return GetSchematyc()->GetScriptRegistry().GetFile(scriptFileName.c_str());
		}
		else
		{
			CBrowserCtrlItemPtr pScriptFileItem = std::static_pointer_cast<CBrowserCtrlItem>(item.FindAncestor(BrowserIcon::DOC));
			if(pScriptFileItem)
			{
				stack_string scriptFileName;
				GetItemPath(*pScriptFileItem, scriptFileName, false);
				return GetSchematyc()->GetScriptRegistry().GetFile(scriptFileName.c_str());
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptElement* CBrowserCtrl::GetItemScriptElement(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		return pScriptFile ? pScriptFile->GetElement(item.GetGUID()) : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptInclude* CBrowserCtrl::GetItemScriptInclude(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		if(pScriptFile)
		{
			return pScriptFile->GetInclude(item.GetGUID());
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptGroup* CBrowserCtrl::GetItemScriptGroup(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		if(pScriptFile)
		{
			return pScriptFile->GetGroup(item.GetGUID());
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptEnumeration* CBrowserCtrl::GetItemScriptEnumeration(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		if(pScriptFile)
		{
			return pScriptFile->GetEnumeration(item.GetGUID());
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptStructure* CBrowserCtrl::GetItemScriptStructure(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		if(pScriptFile)
		{
			return pScriptFile->GetStructure(item.GetGUID());
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptSignal* CBrowserCtrl::GetItemScriptSignal(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		if(pScriptFile)
		{
			return pScriptFile->GetSignal(item.GetGUID());
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptAbstractInterface* CBrowserCtrl::GetItemScriptAbstractInterface(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		if(pScriptFile)
		{
			return pScriptFile->GetAbstractInterface(item.GetGUID());
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptAbstractInterfaceFunction* CBrowserCtrl::GetItemScriptAbstractInterfaceFunction(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		if(pScriptFile)
		{
			return pScriptFile->GetAbstractInterfaceFunction(item.GetGUID());
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptAbstractInterfaceTask* CBrowserCtrl::GetItemScriptAbstractInterfaceTask(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		if(pScriptFile)
		{
			return pScriptFile->GetAbstractInterfaceTask(item.GetGUID());
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptClass* CBrowserCtrl::GetItemScriptClass(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		if(pScriptFile)
		{
			if(item.GetIcon() == BrowserIcon::CLASS)
			{
				return pScriptFile->GetClass(item.GetGUID());
			}
			else
			{
				CBrowserCtrlItemPtr	pScriptClassItem = std::static_pointer_cast<CBrowserCtrlItem>(item.FindAncestor(BrowserIcon::CLASS));
				if(pScriptClassItem)
				{
					return pScriptFile->GetClass(pScriptClassItem->GetGUID());
				}
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptClassBase* CBrowserCtrl::GetItemScriptClassBase(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		if(pScriptFile)
		{
			return pScriptFile->GetClassBase(item.GetGUID());
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptStateMachine* CBrowserCtrl::GetItemScriptStateMachine(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		if(pScriptFile)
		{
			return pScriptFile->GetStateMachine(item.GetGUID());
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptState* CBrowserCtrl::GetItemScriptState(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		if(pScriptFile)
		{
			return pScriptFile->GetState(item.GetGUID());
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptVariable* CBrowserCtrl::GetItemScriptVariable(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		if(pScriptFile)
		{
			return pScriptFile->GetVariable(item.GetGUID());
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptProperty* CBrowserCtrl::GetItemScriptProperty(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		if(pScriptFile)
		{
			return pScriptFile->GetProperty(item.GetGUID());
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptContainer* CBrowserCtrl::GetItemScriptContainer(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		if(pScriptFile)
		{
			return pScriptFile->GetContainer(item.GetGUID());
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptTimer* CBrowserCtrl::GetItemScriptTimer(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		if(pScriptFile)
		{
			return pScriptFile->GetTimer(item.GetGUID());
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptAbstractInterfaceImplementation* CBrowserCtrl::GetItemScriptAbstractInterfaceImplementation(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		if(pScriptFile)
		{
			return pScriptFile->GetAbstractInterfaceImplementation(item.GetGUID());
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptComponentInstance* CBrowserCtrl::GetItemScriptComponentInstance(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		if(pScriptFile)
		{
			return pScriptFile->GetComponentInstance(item.GetGUID());
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptActionInstance* CBrowserCtrl::GetItemScriptActionInstance(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		if(pScriptFile)
		{
			return pScriptFile->GetActionInstance(item.GetGUID());
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IDocGraph* CBrowserCtrl::GetItemDocGraph(const CBrowserCtrlItem& item) const
	{
		TScriptFile* pScriptFile = GetItemScriptFile(item);
		if(pScriptFile)
		{
			return pScriptFile->GetGraph(item.GetGUID());
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	SBrowserCtrlSignals& CBrowserCtrl::Signals()
	{
		return m_signals;
	}

	//////////////////////////////////////////////////////////////////////////
	CBrowserCtrl::CContextMenu::CContextMenu()
	{
		m_menu.CreatePopupMenu();
	}

	//////////////////////////////////////////////////////////////////////////
	CBrowserCtrl::CContextMenu::~CContextMenu()
	{
		for(CContextMenu* pSubMenu : m_subMenus)
		{
			delete pSubMenu;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CMenu& CBrowserCtrl::CContextMenu::GetMenu()
	{
		return m_menu;
	}

	//////////////////////////////////////////////////////////////////////////
	CBrowserCtrl::CContextMenu* CBrowserCtrl::CContextMenu::CreateSubMenu(const char* name)
	{
		CContextMenu*	pSubMenu = new CContextMenu();
		m_subMenus.push_back(pSubMenu);
		m_menu.AppendMenu(MF_POPUP, UINT_PTR(pSubMenu->m_menu.GetSafeHmenu()), name);
		return pSubMenu;
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnSelChanged(NMHDR* pNMHDR, LRESULT* pResult)
	{
		if(CBrowserCtrlItemPtr pItem = std::static_pointer_cast<CBrowserCtrlItem>(CCustomTreeCtrl::FindItem(reinterpret_cast<NM_TREEVIEW*>(pNMHDR)->itemNew.hItem)))
		{
			m_signals.selection.Send(pItem);
		}
		else
		{
			m_signals.selection.Send(CBrowserCtrlItemPtr());
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnBeginLabelEdit(NMHDR* pNMHDR, LRESULT* pResult)
	{
		LPNMTVDISPINFO	pTVDispInfo = reinterpret_cast<LPNMTVDISPINFO>(pNMHDR);
		if(CBrowserCtrlItemPtr pItem = std::static_pointer_cast<CBrowserCtrlItem>(CCustomTreeCtrl::FindItem(pTVDispInfo->item.hItem)))
		{
			if(IsRenameableItem(pItem->GetIcon()))
			{
				*pResult = 0;
				return;
			}
		}
		*pResult = 1;
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnEndLabelEdit(NMHDR* pNMHDR, LRESULT* pResult)
	{
		LPNMTVDISPINFO	pTVDispInfo = reinterpret_cast<LPNMTVDISPINFO>(pNMHDR);
		if(pTVDispInfo->item.mask & TVIF_TEXT)
		{
			HTREEITEM	hItem = pTVDispInfo->item.hItem;
			if(CBrowserCtrlItemPtr pItem = std::static_pointer_cast<CBrowserCtrlItem>(CCustomTreeCtrl::FindItem(hItem)))
			{
				if(TScriptFile* pScriptFile = GetItemScriptFile(*pItem))
				{
					IScriptElement* pElement = GetItemScriptElement(*pItem);
					if(pElement)
					{
						const char* szName = pTVDispInfo->item.pszText;
						if(PluginUtils::ValidateScriptElementName(CCustomTreeCtrl::GetSafeHwnd(), *pScriptFile, pElement->GetScopeGUID(), szName, true))
						{
							if(pElement->SetName(szName))
							{
								CCustomTreeCtrl::SetItemText(pItem, szName);
								m_signals.docModified.Send(*pScriptFile);
								RefreshItem(pItem, EBrowserRefreshFlags::None);
							}
						}
						else
						{
							CCustomTreeCtrl::EditLabel(hItem);
						}
					}
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult) 
	{
		if(CBrowserCtrlItemPtr pItem = std::static_pointer_cast<CBrowserCtrlItem>(CCustomTreeCtrl::FindItem(reinterpret_cast<NM_TREEVIEW*>(pNMHDR)->ptDrag)))
		{
			switch(pItem->GetIcon())
			{
			case BrowserIcon::SIGNAL:
			case BrowserIcon::ABSTRACT_INTERFACE_FUNCTION:
			case BrowserIcon::CLASS:
			case BrowserIcon::STATE:
			case BrowserIcon::VARIABLE:
			case BrowserIcon::PROPERTY:
			case BrowserIcon::CONTAINER:
			case BrowserIcon::TIMER:
			case BrowserIcon::COMPONENT_INSTANCE:
			case BrowserIcon::ACTION_INSTANCE:
			case BrowserIcon::FUNCTION_GRAPH:
			case BrowserIcon::CONDITION_GRAPH:
			case BrowserIcon::CONSTRUCTOR:
			case BrowserIcon::DESTRUCTOR:
			case BrowserIcon::SIGNAL_RECEIVER:
				{
					if(!pItem->FindAncestor(BrowserIcon::SIGNAL))
					{
						PluginUtils::BeginDragAndDrop(PluginUtils::SDragAndDropData(pItem->GetIcon(), pItem->GetGUID()));
					}
					break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnSetFocus(CWnd* pOldWnd)
	{
		m_signals.selection.Send(GetSelectedItem());
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnLButtonDblClk(UINT nFlags, CPoint point)
	{
		if(CBrowserCtrlItemPtr pItem = std::static_pointer_cast<CBrowserCtrlItem>(CCustomTreeCtrl::FindItem(point)))
		{
			size_t              icon = pItem->GetIcon();
			CBrowserCtrlItemPtr pScriptFileItem = ((icon == BrowserIcon::FOLDER) || (icon == BrowserIcon::DOC)) ? pItem : std::static_pointer_cast<CBrowserCtrlItem>(pItem->FindAncestor(BrowserIcon::DOC));
			if(pScriptFileItem)
			{
				m_signals.doubleClick.Send(pItem);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnRClick(NMHDR* pNMHDR, LRESULT* pResult)
	{
		SendMessage(WM_CONTEXTMENU, (WPARAM)m_hWnd, GetMessagePos());
		*pResult = 1;
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
	{
		const bool	ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
		const bool	shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
		switch(nChar)
		{
		case 'C':
			{
				if(ctrlDown)
				{
					if(shiftDown)
					{
						CloneItem(GetSelectedItem());
					}
					else
					{
						//CopyItem(GetSelectedItem());
					}
				}
				break;
			}
		case 'V':
			{
				if(ctrlDown)
				{
					//PasteToItem(GetSelectedItem());
				}
				break;
			}
		case 'R':
			{
				if(ctrlDown)
				{
					RenameItem(GetSelectedItem());
				}
				break;
			}
		case VK_DELETE:
			{
				RemoveItem(GetSelectedItem());
				break;
			}
		case VK_F5:
			{
				CBrowserCtrlItemPtr	pItem = GetSelectedItem();
				if(pItem)
				{
					RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
				}
				break;
			}
		default:
			{
				__super::OnKeyDown(nChar, nRepCnt, nFlags);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnContextMenu(CWnd* pWnd, CPoint point)
	{
		CPoint	clientPoint = point;
		CCustomTreeCtrl::ScreenToClient(&clientPoint);
		if(CBrowserCtrlItemPtr pItem = std::static_pointer_cast<CBrowserCtrlItem>(CCustomTreeCtrl::FindItem(clientPoint)))
		{
			SET_LOCAL_RESOURCE_SCOPE

			CContextMenu contextMenu;
			InitContextMenu(pItem, contextMenu);
			if(contextMenu.GetMenu().GetMenuItemCount() > 0)
			{
				switch(contextMenu.GetMenu().TrackPopupMenuEx(TPM_RETURNCMD, point.x, point.y, this, NULL))
				{
				case ContextMenuItem::COPY:
					{
						CopyItem(pItem);
						break;
					}
				case ContextMenuItem::PASTE:
					{
						PasteToItem(pItem);
						break;
					}
				case ContextMenuItem::CLONE:
					{
						CloneItem(pItem);
						break;
					}
				case ContextMenuItem::RENAME:
					{
						RenameItem(pItem);
						break;
					}
				case ContextMenuItem::REMOVE:
					{
						RemoveItem(pItem);
						break;
					}
				case ContextMenuItem::SHOW_IN_EXPLORER:
					{
						OnShowInExplorer(point, pItem);
						break;
					}
				case ContextMenuItem::DIFF_AGAINST_HAVE_VERSION:
					{
						OnDiffAgainstHaveVersion(pItem);
						break;
					}
				case ContextMenuItem::CREATE_DOC:
					{
						OnCreateDoc(point, pItem);
						break;
					}
				case ContextMenuItem::ADD_INCLUDE:
					{
						OnAddInclude(point, pItem);
						break;
					}
				case ContextMenuItem::ADD_GROUP:
					{
						OnAddGroup(point, pItem);
						break;
					}
				case ContextMenuItem::ADD_ENUMERATION:
					{
						OnAddEnumeration(point, pItem);
						break;
					}
				case ContextMenuItem::ADD_STRUCTURE:
					{
						OnAddStructure(point, pItem);
						break;
					}
				case ContextMenuItem::ADD_SIGNAL:
					{
						OnAddSignal(point, pItem);
						break;
					}
				case ContextMenuItem::ADD_ABSTRACT_INTERFACE:
					{
						OnAddAbstractInterface(point, pItem);
						break;
					}
				case ContextMenuItem::ADD_ABSTRACT_INTERFACE_FUNCTION:
					{
						OnAddAbstractInterfaceFunction(point, pItem);
						break;
					}
				case ContextMenuItem::ADD_ABSTRACT_INTERFACE_TASK:
					{
						OnAddAbstractInterfaceTask(point, pItem);
						break;
					}
				case ContextMenuItem::ADD_CLASS:
					{
						OnAddClass(point, pItem);
						break;
					}
				case ContextMenuItem::ADD_PERSISTENT_STATE_MACHINE:
					{
						OnAddStateMachine(point, pItem, EScriptStateMachineLifetime::Persistent);
						break;
					}
				case ContextMenuItem::ADD_TASK_STATE_MACHINE:
					{
						OnAddStateMachine(point, pItem, EScriptStateMachineLifetime::Task);
						break;
					}
				case ContextMenuItem::ADD_STATE:
					{
						OnAddState(point, pItem);
						break;
					}
				case ContextMenuItem::ADD_VARIABLE:
					{
						OnAddVariable(point, pItem);
						break;
					}
				case ContextMenuItem::ADD_PROPERTY:
					{
						OnAddProperty(point, pItem);
						break;
					}
				case ContextMenuItem::ADD_CONTAINER:
					{
						OnAddContainer(point, pItem);
						break;
					}
				case ContextMenuItem::ADD_TIMER:
					{
						OnAddTimer(point, pItem);
						break;
					}
				case ContextMenuItem::ADD_ABSTRACT_INTERFACE_IMPLEMENTATION:
					{
						OnAddAbstractInterfaceImplementation(point, pItem);
						break;
					}
				case ContextMenuItem::ADD_COMPONENT_INSTANCE:
					{
						OnAddComponentInstance(point, pItem);
						break;
					}
				case ContextMenuItem::ADD_ACTION_INSTANCE:
					{
						OnAddActionInstance(point, pItem);
						break;
					}
				case ContextMenuItem::ADD_FUNCTION_GRAPH:
					{
						OnAddGraph(point, pItem, EScriptGraphType::Function);
						break;
					}
				case ContextMenuItem::ADD_CONDITION_GRAPH:
					{
						OnAddGraph(point, pItem, EScriptGraphType::Condition);
						break;
					}
				case ContextMenuItem::ADD_CONSTRUCTOR_GRAPH:
					{
						OnAddGraph(point, pItem, EScriptGraphType::Constructor);
						break;
					}
				case ContextMenuItem::ADD_DESTRUCTOR_GRAPH:
					{
						OnAddGraph(point, pItem, EScriptGraphType::Destructor);
						break;
					}
				case ContextMenuItem::ADD_SIGNAL_RECEIVER_GRAPH:
					{
						OnAddGraph(point, pItem, EScriptGraphType::SignalReceiver);
						break;
					}
				case ContextMenuItem::REFRESH:
					{
						RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
						break;
					}
				case ContextMenuItem::FIND_REFERENCES:
					{
						const TScriptFile* pFile = GetItemScriptFile(*pItem);
						
						SGUID refGuid   = pItem->GetGUID();
						SGUID refGoBack = pItem->GetGUID();
						const char* szItemName = pItem->GetText();

						Schematyc2::LibUtils::FindAndLogReferences(refGuid, refGoBack, szItemName, pFile);
						break;
					}
				case ContextMenuItem::FIND_REFEFERENCES_IN_WHOLE_REGISTRY:
					{
						const TScriptFile* pFile = GetItemScriptFile(*pItem);
						SGUID refGuid = pItem->GetGUID();
						SGUID refGoBack = pItem->GetGUID();
						const char* szItemName = pItem->GetText();
	
						Schematyc2::LibUtils::FindAndLogReferences(refGuid, refGoBack, szItemName, pFile, true);
						break;
					}
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::LoadImageList(CImageList& imageList)
	{
		LoadBrowserIcons(imageList);
		SetImageList(&imageList, TVSIL_NORMAL);
	}

	//////////////////////////////////////////////////////////////////////////
	CBrowserCtrlItemPtr CBrowserCtrl::GetItem(const SGUID& guid)
	{
		CCustomTreeCtrlItemPtr pItem = CCustomTreeCtrl::FindItem(guid);
		if (pItem)
		{
			CRY_ASSERT(pItem->GetGUID() == guid);
			return std::static_pointer_cast<CBrowserCtrlItem>(pItem);
		}
		return CBrowserCtrlItemPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::RefreshItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags)
	{
		//LOADING_TIME_PROFILE_SESSION_SECTION("bp_schematyc_browser_refresh")
		LOADING_TIME_PROFILE_SECTION

		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			switch(pItem->GetIcon())
			{
			case BrowserIcon::FOLDER:
				{
					RefreshFolderItem(pItem, flags);
					break;
				}
			case BrowserIcon::DOC:
				{
					RefreshDocItem(pItem, flags);
					break;
				}
			case BrowserIcon::GROUP:
				{
					RefreshGroupItem(pItem, flags);
					break;
				}
			case BrowserIcon::ABSTRACT_INTERFACE:
				{
					RefreshAbstractInterfaceItem(pItem, flags);
					break;
				}
			case BrowserIcon::ABSTRACT_INTERFACE_TASK:
				{
					RefreshAbstractInterfaceTaskItem(pItem, flags);
					break;
				}
			case BrowserIcon::CLASS:
				{
					RefreshClassItem(pItem, flags);
					break;
				}
			case BrowserIcon::CLASS_BASE:
				{
					RefreshClassBaseItem(pItem, flags);
					break;
				}
			case BrowserIcon::STATE_MACHINE:
				{
					RefreshStateMachineItem(pItem, flags);
					break;
				}
			case BrowserIcon::STATE:
				{
					RefreshStateItem(pItem, flags);
					break;
				}
			case BrowserIcon::ABSTRACT_INTERFACE_IMPLEMENTATION:
				{
					RefreshAbstractInterfaceImplementationItem(pItem, flags);
					break;
				}
			case BrowserIcon::COMPONENT_INSTANCE:
				{
					RefreshComponentInstanceItem(pItem, flags);
					break;
				}
			case BrowserIcon::SETTINGS_FOLDER:
				{
					RefreshSettingsFolderItem(pItem, flags);
					break;
				}
			}

			COLORREF        itemColor = COLORREF(-1);
			IScriptElement* pElement = GetItemScriptElement(*pItem);
			if(pElement)
			{
				EBrowserItemFlags itemFlags = pItem->GetFlags() & ~(EBrowserItemFlags::HasWarnings | EBrowserItemFlags::HasErrors);

				IValidatorArchivePtr     pArchive = GetSchematyc()->CreateValidatorArchive(SValidatorArchiveParams());
				ISerializationContextPtr pSerializationContext = GetSchematyc()->CreateSerializationContext(SSerializationContextParams(*pArchive, &pElement->GetFile(), ESerializationPass::Validate));
				pElement->Serialize(*pArchive);
				if(pArchive->GetWarningCount())
				{
					itemFlags |= EBrowserItemFlags::HasWarnings;
				}
				if(pArchive->GetErrorCount())
				{
					itemFlags |= EBrowserItemFlags::HasErrors;
				}
				if ((pElement->GetElementFlags() & EScriptElementFlags::Discard) != 0)
				{
					itemFlags |= EBrowserItemFlags::HasErrors;
				}

				TCustomTreeCtrlItemPtrVector childItems;
				CCustomTreeCtrl::GetChildItems(*pItem, childItems);
				for(CCustomTreeCtrlItemPtr& pItem : childItems)
				{
					itemFlags |= std::static_pointer_cast<CBrowserCtrlItem>(pItem)->GetFlags() & (EBrowserItemFlags::HasWarnings | EBrowserItemFlags::HasErrors);
				}

				if((itemFlags & EBrowserItemFlags::HasErrors) != 0)
				{
					itemColor = RGB(255, 0, 0);
				}
				else if((itemFlags & EBrowserItemFlags::HasWarnings) != 0)
				{
					itemColor = RGB(255, 255, 0);
				}

				pItem->SetFlags(itemFlags);
			}
			CCustomTreeCtrl::SetItemColor(pItem, itemColor);
			CCustomTreeCtrl::SetItemBold(pItem, itemColor != COLORREF(-1));

			if(((flags & EBrowserRefreshFlags::Up) != 0) && ((flags & EBrowserRefreshFlags::FromParent) == 0))
			{
				CBrowserCtrlItemPtr	pParentItem = std::static_pointer_cast<CBrowserCtrlItem>(pItem->GetParent());
				if(pParentItem)
				{
					RefreshItem(pParentItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::FromChild);
				}
			}
		}
		__super::Invalidate(true);
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::RefreshFolderItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags)
	{
		LOADING_TIME_PROFILE_SECTION;
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			if(((flags & EBrowserRefreshFlags::Down) != 0) && ((flags & EBrowserRefreshFlags::FromChild) == 0))
			{
				CCustomTreeCtrl::RemoveItemChildren(pItem);

				stack_string itemPath;
				GetItemPath(*pItem, itemPath, false);

				TStringVector subFolderNames;
				TStringVector fileNames;
				PluginUtils::GetSubFoldersAndFileNames(itemPath.c_str(), "*.*", true, subFolderNames, fileNames);
				
				bool   bIgnoreUnderscoredFolders = false;
				ICVar* pCVar = gEnv->pConsole->GetCVar("sc_IgnoreUnderscoredFolders");
				if(pCVar)
				{
					bIgnoreUnderscoredFolders = pCVar->GetIVal() != 0;
				}

				for(string& subFolderName : subFolderNames)
				{
					if(!bIgnoreUnderscoredFolders || (subFolderName.c_str()[0] != '_'))
					{
						AddItem(subFolderName.c_str(), BrowserIcon::FOLDER, pItem, SGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
					}
				}

				for(string& fileName : fileNames)
				{
					stack_string scriptFileName = itemPath;
					scriptFileName.append("/");
					scriptFileName.append(fileName.c_str());

					const TScriptFile* pScriptFile = GetSchematyc()->GetScriptRegistry().GetFile(scriptFileName.c_str());
					if(pScriptFile)
					{
						if((pScriptFile->GetFlags() & EScriptFileFlags::OnDisk) != 0)
						{
							stack_string::size_type	extensionPos = fileName.rfind('.');
							if(extensionPos != stack_string::npos)
							{
								fileName.erase(extensionPos);
							}
							AddItem(fileName.c_str(), BrowserIcon::DOC, pItem, pScriptFile->GetGUID(), EBrowserRefreshFlags::Down | EBrowserRefreshFlags::FromParent);
						}
					}
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::RefreshDocItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				if(((flags & EBrowserRefreshFlags::Down) != 0) && ((flags & EBrowserRefreshFlags::FromChild) == 0))
				{
					CCustomTreeCtrl::RemoveItemChildren(pItem);

					const SGUID	scopeGUID;
					SDocVisitor	visitor(*this, pItem);
					pScriptFile->VisitIncludes(ScriptIncludeConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitInclude>(visitor), scopeGUID, false);
					pScriptFile->VisitGroups(ScriptGroupConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitGroup>(visitor), scopeGUID, false);
					pScriptFile->VisitEnumerations(ScriptEnumerationConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitEnumeration>(visitor), scopeGUID, false);
					pScriptFile->VisitStructures(ScriptStructureConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitStructure>(visitor), scopeGUID, false);
					pScriptFile->VisitSignals(ScriptSignalConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitSignal>(visitor), scopeGUID, false);
					pScriptFile->VisitAbstractInterfaces(ScriptAbstractInterfaceConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitAbstractInterface>(visitor), scopeGUID, false);
					pScriptFile->VisitClasses(ScriptClassConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitClass>(visitor), scopeGUID, false);
				}

				stack_string name = pItem->GetText();
				name.TrimRight("*");
				if((pScriptFile->GetFlags() & EScriptFileFlags::Modified) != 0)
				{
					name.append("*");
				}
				SetItemText(pItem, name.c_str());
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::RefreshGroupItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				if(((flags & EBrowserRefreshFlags::Down) != 0) && ((flags & EBrowserRefreshFlags::FromChild) == 0))
				{
					CCustomTreeCtrl::RemoveItemChildren(pItem);

					const SGUID	scopeGUID = pItem->GetGUID();
					SDocVisitor	visitor(*this, pItem);
					pScriptFile->VisitIncludes(ScriptIncludeConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitInclude>(visitor), scopeGUID, false);
					pScriptFile->VisitGroups(ScriptGroupConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitGroup>(visitor), scopeGUID, false);
					pScriptFile->VisitEnumerations(ScriptEnumerationConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitEnumeration>(visitor), scopeGUID, false);
					pScriptFile->VisitStructures(ScriptStructureConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitStructure>(visitor), scopeGUID, false);
					pScriptFile->VisitSignals(ScriptSignalConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitSignal>(visitor), scopeGUID, false);
					pScriptFile->VisitAbstractInterfaces(ScriptAbstractInterfaceConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitAbstractInterface>(visitor), scopeGUID, false);
					pScriptFile->VisitClasses(ScriptClassConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitClass>(visitor), scopeGUID, false);
					pScriptFile->VisitStateMachines(ScriptStateMachineConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitStateMachine>(visitor), scopeGUID, false);
					pScriptFile->VisitStates(ScriptStateConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitState>(visitor), scopeGUID, false);
					pScriptFile->VisitVariables(ScriptVariableConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitVariable>(visitor), scopeGUID, false);
					pScriptFile->VisitProperties(ScriptPropertyConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitProperty>(visitor), scopeGUID, false);
					pScriptFile->VisitContainers(ScriptContainerConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitContainer>(visitor), scopeGUID, false);
					pScriptFile->VisitTimers(ScriptTimerConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitTimer>(visitor), scopeGUID, false);
					pScriptFile->VisitComponentInstances(ScriptComponentInstanceConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitComponentInstance>(visitor), scopeGUID, false);
					pScriptFile->VisitActionInstances(ScriptActionInstanceConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitActionInstance>(visitor), scopeGUID, false);
					pScriptFile->VisitGraphs(DocGraphConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitGraph>(visitor), scopeGUID, false);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::RefreshAbstractInterfaceItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				if(((flags & EBrowserRefreshFlags::Down) != 0) && ((flags & EBrowserRefreshFlags::FromChild) == 0))
				{
					CCustomTreeCtrl::RemoveItemChildren(pItem);

					const SGUID	scopeGUID = pItem->GetGUID();
					SDocVisitor	visitor(*this, pItem);
					pScriptFile->VisitAbstractInterfaceFunctions(ScriptAbstractInterfaceFunctionConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitAbstractInterfaceFunction>(visitor), scopeGUID, false);
					pScriptFile->VisitAbstractInterfaceTasks(ScriptAbstractInterfaceTaskConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitAbstractInterfaceTask>(visitor), scopeGUID, false);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::RefreshAbstractInterfaceTaskItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				if(((flags & EBrowserRefreshFlags::Down) != 0) && ((flags & EBrowserRefreshFlags::FromChild) == 0))
				{
					CCustomTreeCtrl::RemoveItemChildren(pItem);

					const SGUID	scopeGUID = pItem->GetGUID();
					SDocVisitor	visitor(*this, pItem);
					pScriptFile->VisitProperties(ScriptPropertyConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitProperty>(visitor), scopeGUID, false);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::RefreshClassItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				if(((flags & EBrowserRefreshFlags::Down) != 0) && ((flags & EBrowserRefreshFlags::FromChild) == 0))
				{
					CCustomTreeCtrl::RemoveItemChildren(pItem);

					const SGUID	scopeGUID = pItem->GetGUID();
					SDocVisitor	visitor(*this, pItem);
					pScriptFile->VisitGroups(ScriptGroupConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitGroup>(visitor), scopeGUID, false);
					pScriptFile->VisitEnumerations(ScriptEnumerationConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitEnumeration>(visitor), scopeGUID, false);
					pScriptFile->VisitStructures(ScriptStructureConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitStructure>(visitor), scopeGUID, false);
					pScriptFile->VisitSignals(ScriptSignalConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitSignal>(visitor), scopeGUID, false);
					pScriptFile->VisitClassBases(ScriptClassBaseConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitClassBase>(visitor), scopeGUID, false);
					pScriptFile->VisitStateMachines(ScriptStateMachineConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitStateMachine>(visitor), scopeGUID, false);
					pScriptFile->VisitVariables(ScriptVariableConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitVariable>(visitor), scopeGUID, false);
					pScriptFile->VisitProperties(ScriptPropertyConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitProperty>(visitor), scopeGUID, false);
					pScriptFile->VisitContainers(ScriptContainerConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitContainer>(visitor), scopeGUID, false);
					pScriptFile->VisitTimers(ScriptTimerConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitTimer>(visitor), scopeGUID, false);
					pScriptFile->VisitAbstractInterfaceImplementations(ScriptAbstractInterfaceImplementationConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitAbstractInterfaceImplementation>(visitor), scopeGUID, false);
					pScriptFile->VisitComponentInstances(ScriptComponentInstanceConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitComponentInstance>(visitor), scopeGUID, false);
					pScriptFile->VisitActionInstances(ScriptActionInstanceConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitActionInstance>(visitor), scopeGUID, false);
					pScriptFile->VisitGraphs(DocGraphConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitGraph>(visitor), scopeGUID, false);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::RefreshClassBaseItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				if(((flags & EBrowserRefreshFlags::Down) != 0) && ((flags & EBrowserRefreshFlags::FromChild) == 0))
				{
					CCustomTreeCtrl::RemoveItemChildren(pItem);

					const SGUID scopeGUID = pItem->GetGUID();
					SDocVisitor visitor(*this, pItem);
					pScriptFile->VisitGroups(ScriptGroupConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitGroup>(visitor), scopeGUID, false);
					pScriptFile->VisitProperties(ScriptPropertyConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitProperty>(visitor), scopeGUID, false);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::RefreshStateMachineItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				if(((flags & EBrowserRefreshFlags::Down) != 0) && ((flags & EBrowserRefreshFlags::FromChild) == 0))
				{
					CCustomTreeCtrl::RemoveItemChildren(pItem);

					const SGUID	scopeGUID = pItem->GetGUID();
					SDocVisitor	visitor(*this, pItem);
					pScriptFile->VisitGroups(ScriptGroupConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitGroup>(visitor), scopeGUID, false);
					pScriptFile->VisitStates(ScriptStateConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitState>(visitor), scopeGUID, false);
					pScriptFile->VisitVariables(ScriptVariableConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitVariable>(visitor), scopeGUID, false);
					pScriptFile->VisitProperties(ScriptPropertyConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitProperty>(visitor), scopeGUID, false);
					pScriptFile->VisitGraphs(DocGraphConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitGraph>(visitor), scopeGUID, false);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::RefreshStateItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				if(((flags & EBrowserRefreshFlags::Down) != 0) && ((flags & EBrowserRefreshFlags::FromChild) == 0))
				{
					CCustomTreeCtrl::RemoveItemChildren(pItem);

					const SGUID	scopeGUID = pItem->GetGUID();
					SDocVisitor	visitor(*this, pItem);
					pScriptFile->VisitSignals(ScriptSignalConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitSignal>(visitor), scopeGUID, false);
					pScriptFile->VisitGroups(ScriptGroupConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitGroup>(visitor), scopeGUID, false);
					pScriptFile->VisitStates(ScriptStateConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitState>(visitor), scopeGUID, false);
					pScriptFile->VisitVariables(ScriptVariableConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitVariable>(visitor), scopeGUID, false);
					pScriptFile->VisitContainers(ScriptContainerConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitContainer>(visitor), scopeGUID, false);
					pScriptFile->VisitTimers(ScriptTimerConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitTimer>(visitor), scopeGUID, false);
					pScriptFile->VisitActionInstances(ScriptActionInstanceConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitActionInstance>(visitor), scopeGUID, false);
					pScriptFile->VisitGraphs(DocGraphConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitGraph>(visitor), scopeGUID, false);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::RefreshAbstractInterfaceImplementationItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				if(((flags & EBrowserRefreshFlags::Down) != 0) && ((flags & EBrowserRefreshFlags::FromChild) == 0))
				{
					CCustomTreeCtrl::RemoveItemChildren(pItem);

					const SGUID	scopeGUID = pItem->GetGUID();
					SDocVisitor	visitor(*this, pItem);
					pScriptFile->VisitGraphs(DocGraphConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitGraph>(visitor), scopeGUID, false);
					pScriptFile->VisitStateMachines(ScriptStateMachineConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitStateMachine>(visitor), scopeGUID, false);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::RefreshComponentInstanceItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			if(((flags & EBrowserRefreshFlags::Down) != 0) && ((flags & EBrowserRefreshFlags::FromChild) == 0))
			{
				TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
				if(pScriptFile)
				{
					CCustomTreeCtrl::RemoveItemChildren(pItem);

					const SGUID scopeGUID = pItem->GetGUID();
					SDocVisitor visitor(*this, pItem);
					pScriptFile->VisitComponentInstances(ScriptComponentInstanceConstVisitor::FromConstMemberFunction<SDocVisitor, &SDocVisitor::VisitComponentInstance>(visitor), scopeGUID, false);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::RefreshSettingsFolderItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			if(((flags & EBrowserRefreshFlags::Down) != 0) && ((flags & EBrowserRefreshFlags::FromChild) == 0))
			{
				GetSchematyc()->GetEnvRegistry().VisitSettings(EnvSettingsVisitor::FromConstMemberFunction<SSettingsVisitor, &SSettingsVisitor::VisitSettings>(SSettingsVisitor(*this, pItem)));
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::InitContextMenu(const CBrowserCtrlItemPtr& pItem, CContextMenu& contextMenu)
	{
		SScriptElementClipboardInfo	clipboardInfo;
		if(TScriptFile* pScriptFile = GetItemScriptFile(*pItem))
		{
			pScriptFile->GetClipboardInfo(PluginUtils::ReadXmlFromClipboard("schematyc"), clipboardInfo);
		}
		switch(pItem->GetIcon())
		{
		case BrowserIcon::FOLDER:
			{
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::CREATE_DOC, "Create Document");
				contextMenu.GetMenu().AppendMenu(MF_SEPARATOR);
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::SHOW_IN_EXPLORER, "Show In Explorer");
				break;
			}
		case BrowserIcon::DOC:
			{
				if(CContextMenu* pAddContextMenu = contextMenu.CreateSubMenu("Add"))
				{
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_INCLUDE, "Include");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_GROUP, "Group");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_ENUMERATION, "Enumeration");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_STRUCTURE, "Structure");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_SIGNAL, "Signal");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_ABSTRACT_INTERFACE, "Abstract Interface");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_CLASS, "Class");
					contextMenu.GetMenu().AppendMenu(MF_SEPARATOR);
				}
				//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::PASTE, "Paste");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::CLONE, "Clone");
				contextMenu.GetMenu().AppendMenu(MF_SEPARATOR);
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::DIFF_AGAINST_HAVE_VERSION, "Diff Against Have Revision");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::SHOW_IN_EXPLORER, "Show In Explorer");
				break;
			}
		case BrowserIcon::INCLUDE:
			{
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::REMOVE, "Remove");
				contextMenu.GetMenu().AppendMenu(MF_SEPARATOR);
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::SHOW_IN_EXPLORER, "Show In Explorer");
				break;
			}
		case BrowserIcon::GROUP:
			{
				if(CBrowserCtrlItemPtr pParentItem = GetRealParentItem(pItem))
				{
					switch(pParentItem->GetIcon())
					{
					case BrowserIcon::DOC:
						{
							if(CContextMenu* pAddContextMenu = contextMenu.CreateSubMenu("Add"))
							{
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_INCLUDE, "Include");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_GROUP, "Group");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_ENUMERATION, "Enumeration");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_STRUCTURE, "Structure");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_SIGNAL, "Signal");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_ABSTRACT_INTERFACE, "Abstract Interface");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_CLASS, "Class");
								contextMenu.GetMenu().AppendMenu(MF_SEPARATOR);
							}
							//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::PASTE, "Paste");
							contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::CLONE, "Clone");
							contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::RENAME, "Rename");
							contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::REMOVE, "Remove");
							break;
						}
						case BrowserIcon::CLASS:
						{
							if(CContextMenu* pAddContextMenu = contextMenu.CreateSubMenu("Add"))
							{
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_GROUP, "Group");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_ENUMERATION, "Enumeration");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_STRUCTURE, "Structure");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_SIGNAL, "Signal");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_VARIABLE, "Variable");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_PROPERTY, "Property");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_CONTAINER, "Container");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_TIMER, "Timer");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_COMPONENT_INSTANCE, "Component");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_ACTION_INSTANCE, "Action");
								if(CContextMenu* pStateMachineContextMenu = pAddContextMenu->CreateSubMenu("State Machine"))
								{
									pStateMachineContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_PERSISTENT_STATE_MACHINE, "Persistent");
									pStateMachineContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_TASK_STATE_MACHINE, "Task");
								}
								if(CContextMenu* pGraphContextMenu = pAddContextMenu->CreateSubMenu("Graph"))
								{
									pGraphContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_FUNCTION_GRAPH, "Function");
									pGraphContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_CONDITION_GRAPH, "Condition");
									pGraphContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_CONSTRUCTOR_GRAPH, "Constructor");
									pGraphContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_DESTRUCTOR_GRAPH, "Destructor");
									pGraphContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_SIGNAL_RECEIVER_GRAPH, "Signal Receiver");
								}
								contextMenu.GetMenu().AppendMenu(MF_SEPARATOR);
							}
							//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::PASTE, "Paste");
							contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::CLONE, "Clone");
							contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::RENAME, "Rename");
							contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::REMOVE, "Remove");
							break;
						}
					case BrowserIcon::STATE_MACHINE:
						{
							if(CContextMenu* pAddContextMenu = contextMenu.CreateSubMenu("Add"))
							{
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_STATE, "State");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_VARIABLE, "Variable");
								const IScriptStateMachine* pStateMachine = GetItemScriptStateMachine(*pParentItem);
								if(pStateMachine)
								{
									if(pStateMachine->GetLifetime() == EScriptStateMachineLifetime::Task)
									{
										pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_PROPERTY, "Property");
									}
								}
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_CONTAINER, "Container");
								contextMenu.GetMenu().AppendMenu(MF_SEPARATOR);
							}
							//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::PASTE, "Paste");
							contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::CLONE, "Clone");
							contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::RENAME, "Rename");
							contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::REMOVE, "Remove");
							break;
						}
					case BrowserIcon::STATE:
						{
							if(CContextMenu* pAddContextMenu = contextMenu.CreateSubMenu("Add"))
							{
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_SIGNAL, "Signal");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_STATE, "State");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_GROUP, "Group");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_VARIABLE, "Variable");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_CONTAINER, "Container");
								pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_TIMER, "Timer");
								if(CContextMenu* pGraphContextMenu = pAddContextMenu->CreateSubMenu("Graph"))
								{
									pGraphContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_FUNCTION_GRAPH, "Function");
									pGraphContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_CONDITION_GRAPH, "Condition");
									pGraphContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_CONSTRUCTOR_GRAPH, "Constructor");
									pGraphContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_DESTRUCTOR_GRAPH, "Destructor");
									pGraphContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_SIGNAL_RECEIVER_GRAPH, "Signal Receiver");
								}
								contextMenu.GetMenu().AppendMenu(MF_SEPARATOR);
							}
							//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::PASTE, "Paste");
							contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::CLONE, "Clone");
							contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::RENAME, "Rename");
							contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::REMOVE, "Remove");
							break;
						}
					}
				}
				break;
			}
		case BrowserIcon::ENUMERATION:
			{
				//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::COPY, "Copy");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::CLONE, "Clone");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::RENAME, "Rename");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::REMOVE, "Remove");
				break;
			}
		case BrowserIcon::STRUCTURE:
			{
				//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::COPY, "Copy");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::CLONE, "Clone");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::RENAME, "Rename");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::REMOVE, "Remove");
				break;
			}
		case BrowserIcon::SIGNAL:
			{
				//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::COPY, "Copy");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::CLONE, "Clone");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::RENAME, "Rename");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::REMOVE, "Remove");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::FIND_REFERENCES, "Find References");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::FIND_REFEFERENCES_IN_WHOLE_REGISTRY, "Find All References");
				break;
			}
		case BrowserIcon::ABSTRACT_INTERFACE:
			{
				if(CContextMenu* pAddContextMenu = contextMenu.CreateSubMenu("Add"))
				{
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_ABSTRACT_INTERFACE_FUNCTION, "Function");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_ABSTRACT_INTERFACE_TASK, "Task");
					contextMenu.GetMenu().AppendMenu(MF_SEPARATOR);
				}
				//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::COPY, "Copy");
				switch(clipboardInfo.elementType)
				{
				case EScriptElementType::AbstractInterfaceFunction:
					{
						//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::PASTE, "Paste Function");
						break;
					}
				}
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::CLONE, "Clone");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::RENAME, "Rename");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::REMOVE, "Remove");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::FIND_REFEFERENCES_IN_WHOLE_REGISTRY, "Find All References");
				break;
			}
		case BrowserIcon::ABSTRACT_INTERFACE_FUNCTION:
			{
				//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::COPY, "Copy");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::CLONE, "Clone");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::RENAME, "Rename");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::REMOVE, "Remove");
				break;
			}
		case BrowserIcon::ABSTRACT_INTERFACE_TASK:
			{
				if(CContextMenu* pAddContextMenu = contextMenu.CreateSubMenu("Add"))
				{
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_PROPERTY, "Property");
				}
				//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::COPY, "Copy");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::CLONE, "Clone");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::RENAME, "Rename");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::REMOVE, "Remove");
				break;
			}
		case BrowserIcon::CLASS:
			{
				if(CContextMenu* pAddContextMenu = contextMenu.CreateSubMenu("Add"))
				{
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_GROUP, "Group");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_ENUMERATION, "Enumeration");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_STRUCTURE, "Structure");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_SIGNAL, "Signal");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_VARIABLE, "Variable");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_PROPERTY, "Property");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_CONTAINER, "Container");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_TIMER, "Timer");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_ABSTRACT_INTERFACE_IMPLEMENTATION, "Abstract Interface");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_COMPONENT_INSTANCE, "Component");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_ACTION_INSTANCE, "Action");
					if(CContextMenu* pStateMachineContextMenu = pAddContextMenu->CreateSubMenu("State Machine"))
					{
						pStateMachineContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_PERSISTENT_STATE_MACHINE, "Persistent");
						pStateMachineContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_TASK_STATE_MACHINE, "Task");
					}
					if(CContextMenu* pGraphContextMenu = pAddContextMenu->CreateSubMenu("Graph"))
					{
						pGraphContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_FUNCTION_GRAPH, "Function");
						pGraphContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_CONDITION_GRAPH, "Condition");
						pGraphContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_CONSTRUCTOR_GRAPH, "Constructor");
						pGraphContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_DESTRUCTOR_GRAPH, "Destructor");
						pGraphContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_SIGNAL_RECEIVER_GRAPH, "Signal Receiver");
					}
					contextMenu.GetMenu().AppendMenu(MF_SEPARATOR);
				}
				//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::COPY, "Copy");
				switch(clipboardInfo.elementType)
				{
				case EScriptElementType::Group:
					{
						//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::PASTE, "Paste Group");
						break;
					}
				//case EScriptElementType::Enumeration:
				//	{
				//		contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::PASTE, "Paste Enumeration");
				//		break;
				//	}
				//case EScriptElementType::Structure:
				//	{
				//		contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::PASTE, "Paste Structure");
				//		break;
				//	}
				case EScriptElementType::Signal:
					{
						//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::PASTE, "Paste Signal");
						break;
					}
				case EScriptElementType::Variable:
					{
						//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::PASTE, "Paste Variable");
						break;
					}
				case EScriptElementType::Property:
					{
						//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::PASTE, "Paste Property");
						break;
					}
				case EScriptElementType::Container:
					{
						//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::PASTE, "Paste Container");
						break;
					}
				case EScriptElementType::Timer:
					{
						//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::PASTE, "Paste Timer");
						break;
					}
				}
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::CLONE, "Clone");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::RENAME, "Rename");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::REMOVE, "Remove");
				break;
			}
		case BrowserIcon::STATE_MACHINE:
			{
				if(CContextMenu* pAddContextMenu = contextMenu.CreateSubMenu("Add"))
				{
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_STATE, "State");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_VARIABLE, "Variable");
					const IScriptStateMachine* pStateMachine = GetItemScriptStateMachine(*pItem);
					if(pStateMachine && (pStateMachine->GetLifetime() == EScriptStateMachineLifetime::Task))
					{
						pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_PROPERTY, "Property");
					}
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_CONTAINER, "Container");
					contextMenu.GetMenu().AppendMenu(MF_SEPARATOR);
				}
				//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::COPY, "Copy");
				switch(clipboardInfo.elementType)
				{
				case EScriptElementType::State:
					{
						//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::PASTE, "Paste State");
						break;
					}
				}
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::CLONE, "Clone");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::RENAME, "Rename");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::REMOVE, "Remove");
				break;
			}
		case BrowserIcon::STATE:
			{
				if(CContextMenu* pAddContextMenu = contextMenu.CreateSubMenu("Add"))
				{
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_SIGNAL, "Signal");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_GROUP, "Group");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_STATE, "State");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_VARIABLE, "Variable");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_CONTAINER, "Container");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_TIMER, "Timer");
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_ACTION_INSTANCE, "Action");
					if(CContextMenu* pGraphContextMenu = pAddContextMenu->CreateSubMenu("Graph"))
					{
						pGraphContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_FUNCTION_GRAPH, "Function");
						pGraphContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_CONDITION_GRAPH, "Condition");
						pGraphContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_CONSTRUCTOR_GRAPH, "Constructor");
						pGraphContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_DESTRUCTOR_GRAPH, "Destructor");
						pGraphContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_SIGNAL_RECEIVER_GRAPH, "Signal Receiver");
					}
					contextMenu.GetMenu().AppendMenu(MF_SEPARATOR);
				}
				//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::COPY, "Copy");
				//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::PASTE, "Paste");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::CLONE, "Clone");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::RENAME, "Rename");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::REMOVE, "Remove");
				break;
			}
		case BrowserIcon::VARIABLE:
		case BrowserIcon::PROPERTY:
		case BrowserIcon::CONTAINER:
		case BrowserIcon::TIMER:
			{
				//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::COPY, "Copy");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::CLONE, "Clone");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::RENAME, "Rename");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::REMOVE, "Remove");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::FIND_REFERENCES, "Find References");
				break;
			}
		case BrowserIcon::ABSTRACT_INTERFACE_IMPLEMENTATION:
			{
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::REMOVE, "Remove");
				break;
			}
		case BrowserIcon::COMPONENT_INSTANCE:
			{
				if(CContextMenu* pAddContextMenu = contextMenu.CreateSubMenu("Add"))
				{
					pAddContextMenu->GetMenu().AppendMenu(MF_STRING, ContextMenuItem::ADD_COMPONENT_INSTANCE, "Component");
					contextMenu.GetMenu().AppendMenu(MF_SEPARATOR);
				}
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::RENAME, "Rename");
				const IScriptComponentInstance* pScriptComponentInstance = GetItemScriptComponentInstance(*pItem);
				if(pScriptComponentInstance && ((pScriptComponentInstance->GetFlags() & EScriptComponentInstanceFlags::Foundation) == 0))
				{
					contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::REMOVE, "Remove");
				}
				break;
			}
		case BrowserIcon::ACTION_INSTANCE:
			{
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::RENAME, "Rename");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::REMOVE, "Remove");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::FIND_REFERENCES, "Find References");
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::FIND_REFEFERENCES_IN_WHOLE_REGISTRY, "Find All References");
				break;
			}
		case BrowserIcon::SIGNAL_RECEIVER:
		case BrowserIcon::CONSTRUCTOR:
		case BrowserIcon::DESTRUCTOR:
		case BrowserIcon::FUNCTION_GRAPH:
		case BrowserIcon::CONDITION_GRAPH:
			{
				IDocGraph* pDocGraph = GetItemDocGraph(*pItem);
				if(pDocGraph)
				{
					//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::COPY, "Copy");
					//contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::CLONE, "Clone");
					contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::RENAME, "Rename");
					switch(pDocGraph->GetType())
					{
					case EScriptGraphType::Function:
					case EScriptGraphType::Condition:
						contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::FIND_REFERENCES, "Find References");
					case EScriptGraphType::Constructor:
					case EScriptGraphType::Destructor:
					case EScriptGraphType::SignalReceiver:
						{
							contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::REMOVE, "Remove");
							break;
						}
					}
				}
				break;
			}
		case BrowserIcon::SETTINGS_FOLDER:
		case BrowserIcon::SETTINGS_DOC:
			{
				contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::SHOW_IN_EXPLORER, "Show In Explorer");
				break;
			}
		}
		contextMenu.GetMenu().AppendMenu(MF_SEPARATOR);
		contextMenu.GetMenu().AppendMenu(MF_STRING, ContextMenuItem::REFRESH, "Refresh");
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::CopyItem(const CBrowserCtrlItemPtr& pItem)
	{
		if(pItem)
		{
			if(IsCopyableItem(pItem->GetIcon()))
			{
				if(TScriptFile* pScriptFile = GetItemScriptFile(*pItem))
				{
					PluginUtils::WriteXmlToClipboard(pScriptFile->CopyElementsToXml(pItem->GetGUID(), true));
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::PasteToItem(const CBrowserCtrlItemPtr& pItem)
	{
		// #SchematycTODO : Verify type of element we want to paste?
		if(pItem)
		{
			if(TScriptFile* pScriptFile = GetItemScriptFile(*pItem))
			{
				if(XmlNodeRef xml = PluginUtils::ReadXmlFromClipboard("schematyc"))
				{
					pScriptFile->PasteElementsFromXml(pItem->GetGUID(), xml);
					RefreshItem(pItem, EBrowserRefreshFlags::Up);
					m_signals.docModified.Send(*pScriptFile);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::CloneItem(const CBrowserCtrlItemPtr& pItem)
	{
		if(pItem)
		{
			const size_t icon = pItem->GetIcon();
			if(IsCloneableItem(icon))
			{
				CBrowserCtrlItemPtr pParentItem = std::static_pointer_cast<CBrowserCtrlItem>(pItem->GetParent());
				if(pParentItem)
				{
					TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
					if(pScriptFile)
					{
						if(icon == BrowserIcon::DOC)
						{
							// TODO : Create IScriptRegistry::Copy() function?

							stack_string filePath;
							GetItemPath(*pItem, filePath, false);
							filePath.append("_copy.xml");
							PluginUtils::MakeFileNameUnique(filePath);

							TScriptFile* pClonedScriptFile = GetSchematyc()->GetScriptRegistry().CreateFile(filePath.c_str());
							if(pClonedScriptFile)
							{
								XmlNodeRef xml = pScriptFile->CopyElementsToXml(SGUID(), true);
								if(xml)
								{
									pClonedScriptFile->PasteElementsFromXml(SGUID(), xml);
								}
								pClonedScriptFile->Save();

								RefreshItem(pParentItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
								m_signals.docModified.Send(*pClonedScriptFile);
							}
						}
						else
						{
							XmlNodeRef xml = pScriptFile->CopyElementsToXml(pItem->GetGUID(), true);
							if(xml)
							{
								pScriptFile->PasteElementsFromXml(pParentItem->GetIcon() != BrowserIcon::DOC ? pParentItem->GetGUID() : SGUID(), xml);

								RefreshItem(pParentItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
								m_signals.docModified.Send(*pScriptFile);
							}
						}
					}
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::RenameItem(const CBrowserCtrlItemPtr& pItem)
	{
		if(pItem)
		{
			CCustomTreeCtrl::EditLabel(pItem->GetHTreeItem());
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::RemoveItem(const CBrowserCtrlItemPtr& pItem)
	{
		if(pItem)
		{
			if(IsRemoveableItem(pItem->GetIcon()))
			{
				TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
				if(pScriptFile)
				{
					string message = "Remove '";
					message.append(pItem->GetText());
					message.append("'?");
					if (::MessageBoxA(CCustomTreeCtrl::GetSafeHwnd(), message.c_str(), "Remove", MB_OKCANCEL | MB_ICONQUESTION) == IDOK)
					{
						IScriptElement* pScriptElement = GetItemScriptElement(*pItem);
						if(pScriptElement)
						{
							m_signals.itemRemoved.Send(*pScriptFile, *pScriptElement);
							pScriptFile->RemoveElement(pScriptElement->GetGUID(), true);
							RefreshItem(std::static_pointer_cast<CBrowserCtrlItem>(pItem->GetParent()), EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
							m_signals.docModified.Send(*pScriptFile);
						}
					}
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnShowInExplorer(CPoint point, const CBrowserCtrlItemPtr& pItem)
	{
		switch(pItem->GetIcon())
		{
		case BrowserIcon::INCLUDE:
			{
				IScriptInclude* pInclude = GetItemScriptInclude(*pItem);
				SCHEMATYC2_SYSTEM_ASSERT(pInclude);
				if(pInclude)
				{
					string	itemPath = gEnv->pCryPak->GetGameFolder();
					itemPath.append("/");
					itemPath.append(pInclude->GetName());
					string::size_type	pos = itemPath.find_last_of("/\\");
					if(pos != string::npos)
					{
						itemPath.erase(pos);
					}
					PluginUtils::ShowInExplorer(itemPath.c_str());
				}
				break;
			}
		default:
			{
				const size_t        icon = pItem->GetIcon();
				const size_t        folderIcons[] = { BrowserIcon::FOLDER, BrowserIcon::SETTINGS_FOLDER };
				CBrowserCtrlItemPtr pFolderItem = (icon == BrowserIcon::FOLDER) || (icon == BrowserIcon::SETTINGS_FOLDER) ? pItem : std::static_pointer_cast<CBrowserCtrlItem>(pItem->FindAncestor(TSizeTConstArray(folderIcons)));
				SCHEMATYC2_SYSTEM_ASSERT(pFolderItem);
				if(pFolderItem)
				{
					stack_string	itemPath;
					GetItemPath(*pFolderItem, itemPath, true);
					PluginUtils::ShowInExplorer(itemPath.c_str());
				}
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnDiffAgainstHaveVersion(const CBrowserCtrlItemPtr& pItem)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if (pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if (pScriptFile)
			{
				PluginUtils::DiffAgainstHaveVersion(pScriptFile->GetFileName());
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnCreateDoc(CPoint point, const CBrowserCtrlItemPtr& pItem)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			stack_string path;
			GetItemPath(*pItem, path, false);
			TScriptFile* pScriptFile = BrowserUtils::CreateScriptFile(this, point, path.c_str());
			if(pScriptFile)
			{
				RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
				SelectItem(pScriptFile->GetGUID(), SGUID());
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnAddInclude(CPoint point, const CBrowserCtrlItemPtr& pItem)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				const SGUID     scopeGUID = pItem->GetIcon() != BrowserIcon::DOC ? pItem->GetGUID() : SGUID();
				IScriptInclude* pInclude = BrowserUtils::AddInclude(this, point, *pScriptFile, scopeGUID);
				if(pInclude)
				{
					m_signals.docModified.Send(*pScriptFile);
					RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
					SelectItem(pInclude->GetGUID(), SGUID());
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnAddGroup(CPoint point, const CBrowserCtrlItemPtr& pItem)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				const SGUID   scopeGUID = pItem->GetIcon() != BrowserIcon::DOC ? pItem->GetGUID() : SGUID();
				IScriptGroup* pGroup = BrowserUtils::AddGroup(this, point, *pScriptFile, scopeGUID);
				if(pGroup)
				{
					m_signals.docModified.Send(*pScriptFile);
					RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
					SelectItem(pGroup->GetGUID(), SGUID());
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnAddEnumeration(CPoint point, const CBrowserCtrlItemPtr& pItem)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				const SGUID         scopeGUID = pItem->GetIcon() != BrowserIcon::DOC ? pItem->GetGUID() : SGUID();
				IScriptEnumeration* pEnumeration = BrowserUtils::AddEnumeration(this, point, *pScriptFile, scopeGUID);
				if(pEnumeration)
				{
					m_signals.docModified.Send(*pScriptFile);
					RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
					SelectItem(pEnumeration->GetGUID(), SGUID());
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnAddStructure(CPoint point, const CBrowserCtrlItemPtr& pItem)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				const SGUID       scopeGUID = pItem->GetIcon() != BrowserIcon::DOC ? pItem->GetGUID() : SGUID();
				IScriptStructure* pStructure = BrowserUtils::AddStructure(this, point, *pScriptFile, scopeGUID);
				if(pStructure)
				{
					m_signals.docModified.Send(*pScriptFile);
					RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
					SelectItem(pStructure->GetGUID(), SGUID());
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnAddSignal(CPoint point, const CBrowserCtrlItemPtr& pItem)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				const SGUID    scopeGUID = pItem->GetIcon() != BrowserIcon::DOC ? pItem->GetGUID() : SGUID();
				IScriptSignal* pSignal = BrowserUtils::AddSignal(this, point, *pScriptFile, scopeGUID);
				if(pSignal)
				{
					m_signals.docModified.Send(*pScriptFile);
					RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
					SelectItem(pSignal->GetGUID(), SGUID());
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnAddAbstractInterface(CPoint point, const CBrowserCtrlItemPtr& pItem)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				const SGUID               scopeGUID = pItem->GetIcon() != BrowserIcon::DOC ? pItem->GetGUID() : SGUID();
				IScriptAbstractInterface* pAbstractInterface = BrowserUtils::AddAbstractInterface(this, point, *pScriptFile, scopeGUID);
				if(pAbstractInterface)
				{
					m_signals.docModified.Send(*pScriptFile);
					RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
					SelectItem(pAbstractInterface->GetGUID(), SGUID());
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnAddAbstractInterfaceFunction(CPoint point, const CBrowserCtrlItemPtr& pItem)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				const SGUID                       scopeGUID = pItem->GetIcon() != BrowserIcon::DOC ? pItem->GetGUID() : SGUID();
				IScriptAbstractInterfaceFunction* pAbstractInterfaceFunction = BrowserUtils::AddAbstractInterfaceFunction(this, point, *pScriptFile, scopeGUID);
				if(pAbstractInterfaceFunction)
				{
					m_signals.docModified.Send(*pScriptFile);
					RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
					SelectItem(pAbstractInterfaceFunction->GetGUID(), SGUID());
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnAddAbstractInterfaceTask(CPoint point, const CBrowserCtrlItemPtr& pItem)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				const SGUID                   scopeGUID = pItem->GetIcon() != BrowserIcon::DOC ? pItem->GetGUID() : SGUID();
				IScriptAbstractInterfaceTask* pAbstractInterfaceTask = BrowserUtils::AddAbstractInterfaceTask(this, point, *pScriptFile, scopeGUID);
				if(pAbstractInterfaceTask)
				{
					m_signals.docModified.Send(*pScriptFile);
					RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
					SelectItem(pAbstractInterfaceTask->GetGUID(), SGUID());
				}
			}
		}
	}
	
	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnAddClass(CPoint point, const CBrowserCtrlItemPtr& pItem)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				const SGUID   scopeGUID = pItem->GetIcon() != BrowserIcon::DOC ? pItem->GetGUID() : SGUID();
				IScriptClass* pClass = BrowserUtils::AddClass(this, point, *pScriptFile, scopeGUID);
				if(pClass)
				{
					m_signals.docModified.Send(*pScriptFile);
					RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
					SelectItem(pClass->GetGUID(), SGUID());
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnAddStateMachine(CPoint point, const CBrowserCtrlItemPtr& pItem, EScriptStateMachineLifetime lifetime)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				const SGUID          scopeGUID = pItem->GetIcon() != BrowserIcon::DOC ? pItem->GetGUID() : SGUID();
				IScriptStateMachine* pStateMachine = BrowserUtils::AddStateMachine(this, point, *pScriptFile, scopeGUID, lifetime);
				if(pStateMachine)
				{
					m_signals.docModified.Send(*pScriptFile);
					RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
					SelectItem(pStateMachine->GetGUID(), SGUID());
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnAddState(CPoint point, const CBrowserCtrlItemPtr& pItem)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				const SGUID  scopeGUID = pItem->GetIcon() != BrowserIcon::DOC ? pItem->GetGUID() : SGUID();
				CAddStateDlg addStateDlg(this, point, *pScriptFile, scopeGUID);
				if(addStateDlg.DoModal() == IDOK)
				{
					m_signals.docModified.Send(*pScriptFile);
					RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
					SelectItem(addStateDlg.GetStateGUID(), SGUID());
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnAddVariable(CPoint point, const CBrowserCtrlItemPtr& pItem)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				const SGUID      scopeGUID = pItem->GetIcon() != BrowserIcon::DOC ? pItem->GetGUID() : SGUID();
				IScriptVariable* pVariable = BrowserUtils::AddVariable(this, point, *pScriptFile, scopeGUID);
				if(pVariable)
				{
					m_signals.docModified.Send(*pScriptFile);
					RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
					SelectItem(pVariable->GetGUID(), SGUID());
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnAddProperty(CPoint point, const CBrowserCtrlItemPtr& pItem)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				const SGUID      scopeGUID = pItem->GetIcon() != BrowserIcon::DOC ? pItem->GetGUID() : SGUID();
				IScriptProperty* pProperty = BrowserUtils::AddProperty(this, point, *pScriptFile, scopeGUID);
				if(pProperty)
				{
					m_signals.docModified.Send(*pScriptFile);
					RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
					SelectItem(pProperty->GetGUID(), SGUID());
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnAddContainer(CPoint point, const CBrowserCtrlItemPtr& pItem)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				const SGUID      scopeGUID = pItem->GetIcon() != BrowserIcon::DOC ? pItem->GetGUID() : SGUID();
				CAddContainerDlg addContainerDlg(this, point, *pScriptFile, scopeGUID);
				if(addContainerDlg.DoModal() == IDOK)
				{
					m_signals.docModified.Send(*pScriptFile);
					RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
					SelectItem(addContainerDlg.GetContainerGUID(), SGUID());
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnAddTimer(CPoint point, const CBrowserCtrlItemPtr& pItem)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				const SGUID   scopeGUID = pItem->GetIcon() != BrowserIcon::DOC ? pItem->GetGUID() : SGUID();
				IScriptTimer* pTimer = BrowserUtils::AddTimer(this, point, *pScriptFile, scopeGUID);
				if(pTimer)
				{
					m_signals.docModified.Send(*pScriptFile);
					RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
					SelectItem(pTimer->GetGUID(), SGUID());
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnAddAbstractInterfaceImplementation(CPoint point, const CBrowserCtrlItemPtr& pItem)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				const SGUID                            scopeGUID = pItem->GetIcon() != BrowserIcon::DOC ? pItem->GetGUID() : SGUID();
				CAddAbstractInterfaceImplementationDlg addAbstractInterfaceImplementationDlg(this, point, *pScriptFile, scopeGUID);
				if(addAbstractInterfaceImplementationDlg.DoModal() == IDOK)
				{
					m_signals.docModified.Send(*pScriptFile);
					RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
					SelectItem(addAbstractInterfaceImplementationDlg.GetAbstractInterfaceImplementationGUID(), SGUID());
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnAddComponentInstance(CPoint point, const CBrowserCtrlItemPtr& pItem)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				const SGUID               scopeGUID = pItem->GetIcon() != BrowserIcon::DOC ? pItem->GetGUID() : SGUID();
				IScriptComponentInstance* pComponentInstance = BrowserUtils::AddComponentInstance(this, point, *pScriptFile, scopeGUID);
				if(pComponentInstance)
				{
					m_signals.docModified.Send(*pScriptFile);
					RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
					SelectItem(pComponentInstance->GetGUID(), SGUID());
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnAddActionInstance(CPoint point, const CBrowserCtrlItemPtr& pItem)
	{
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				const SGUID            scopeGUID = pItem->GetIcon() != BrowserIcon::DOC ? pItem->GetGUID() : SGUID();
				IScriptActionInstance* pActionInstance = BrowserUtils::AddActionInstance(this, point, *pScriptFile, scopeGUID);
				if(pActionInstance)
				{
					m_signals.docModified.Send(*pScriptFile);
					RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
					SelectItem(pActionInstance->GetGUID(), SGUID());
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CBrowserCtrl::OnAddGraph(CPoint point, const CBrowserCtrlItemPtr& pItem, EScriptGraphType type)
	{
		// #SchematycTODO : Don't allow naming of constructors and destructors!!!
		SCHEMATYC2_SYSTEM_ASSERT(pItem);
		if(pItem)
		{
			TScriptFile* pScriptFile = GetItemScriptFile(*pItem);
			if(pScriptFile)
			{
				const SGUID  scopeGUID = pItem->GetIcon() != BrowserIcon::DOC ? pItem->GetGUID() : SGUID();
				CAddGraphDlg addGraphDlg(this, point, *pScriptFile, scopeGUID, type);
				if(addGraphDlg.DoModal() == IDOK)
				{
					m_signals.docModified.Send(*pScriptFile);
					RefreshItem(pItem, EBrowserRefreshFlags::Up | EBrowserRefreshFlags::Down);
					SelectItem(addGraphDlg.GetGraphGUID(), SGUID());
				}
			}
		}
	}
}
