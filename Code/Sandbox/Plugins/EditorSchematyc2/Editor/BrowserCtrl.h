// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/IScriptGraph.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_Signalv2.h>

#include "TreeCtrl.h"

namespace Schematyc2
{
	enum class EBrowserItemFlags
	{
		None        = 0,
		HasWarnings = BIT(0),
		HasErrors   = BIT(1)
	};

	DECLARE_ENUM_CLASS_FLAGS(EBrowserItemFlags)

	class CBrowserCtrlItem : public CCustomTreeCtrlItem, public std::enable_shared_from_this<CBrowserCtrlItem>
	{
	public:

		CBrowserCtrlItem(const char* szText, size_t icon, const CCustomTreeCtrlItemPtr& pParent, const SGUID& guid);

		void SetFlags(EBrowserItemFlags flags);
		EBrowserItemFlags GetFlags() const;

	private:
		EBrowserItemFlags m_flags;
	};

	DECLARE_SHARED_POINTERS(CBrowserCtrlItem)

	enum class EBrowserRefreshFlags
	{
		None       = 0,
		Up         = BIT(0),
		Down       = BIT(1),
		FromParent = BIT(2),
		FromChild  = BIT(3)
	};

	DECLARE_ENUM_CLASS_FLAGS(EBrowserRefreshFlags)

	struct SBrowserSelectionParams
	{
		inline SBrowserSelectionParams(CBrowserCtrlItemPtr& _item)
			: item(_item)
			, pChildGuid(nullptr)
		{}

		CBrowserCtrlItemPtr& item;
		const SGUID*         pChildGuid;
	};

	typedef TemplateUtils::CSignalv2<void (const SBrowserSelectionParams&)> BrowserCtrlSelectionSignal;
	typedef TemplateUtils::CSignalv2<void (const CBrowserCtrlItemPtr&)>     BrowserCtrlDoubleClickSignal;
	typedef TemplateUtils::CSignalv2<void (TScriptFile&, IScriptElement&)>  BrowserCtrlItemRemovedSignal;
	typedef TemplateUtils::CSignalv2<void (TScriptFile&)>                   BrowserCtrlDocModifiedSignal;

	struct SBrowserCtrlSignals
	{
		BrowserCtrlSelectionSignal   selection;
		BrowserCtrlDoubleClickSignal doubleClick;
		BrowserCtrlItemRemovedSignal itemRemoved;
		BrowserCtrlDocModifiedSignal docModified;
	};

	class CBrowserCtrl : public CCustomTreeCtrl
	{
		DECLARE_MESSAGE_MAP()

	public:

		CBrowserCtrl();

		// CCustomTreeCtrl
		virtual BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
		// ~CCustomTreeCtrl

		void RefreshDocs();
		void RefreshItem(const SGUID& guid); // N.B. This temporary solution and ideally the browser control should interact directly with the detail panel.
		void SelectItem(const SGUID& itemGuid, const SGUID& nodeGuid);
		void DiffSelectedItem();
		CBrowserCtrlItemPtr AddItem(const char* szText, size_t icon, const CBrowserCtrlItemPtr& pParentItem, const SGUID& guid, EBrowserRefreshFlags refreshFlags);
		CBrowserCtrlItemPtr GetSelectedItem();
		TScriptFile* GetItemScriptFile(const CBrowserCtrlItem& item) const;
		IScriptElement* GetItemScriptElement(const CBrowserCtrlItem& item) const;
		IScriptInclude* GetItemScriptInclude(const CBrowserCtrlItem& item) const;
		IScriptGroup* GetItemScriptGroup(const CBrowserCtrlItem& item) const;
		IScriptEnumeration* GetItemScriptEnumeration(const CBrowserCtrlItem& item) const;
		IScriptStructure* GetItemScriptStructure(const CBrowserCtrlItem& item) const;
		IScriptSignal* GetItemScriptSignal(const CBrowserCtrlItem& item) const;
		IScriptAbstractInterface* GetItemScriptAbstractInterface(const CBrowserCtrlItem& item) const;
		IScriptAbstractInterfaceFunction* GetItemScriptAbstractInterfaceFunction(const CBrowserCtrlItem& item) const;
		IScriptAbstractInterfaceTask* GetItemScriptAbstractInterfaceTask(const CBrowserCtrlItem& item) const;
		IScriptClass* GetItemScriptClass(const CBrowserCtrlItem& item) const;
		IScriptClassBase* GetItemScriptClassBase(const CBrowserCtrlItem& item) const;
		IScriptStateMachine* GetItemScriptStateMachine(const CBrowserCtrlItem& item) const;
		IScriptState* GetItemScriptState(const CBrowserCtrlItem& item) const;
		IScriptVariable* GetItemScriptVariable(const CBrowserCtrlItem& item) const;
		IScriptProperty* GetItemScriptProperty(const CBrowserCtrlItem& item) const;
		IScriptContainer* GetItemScriptContainer(const CBrowserCtrlItem& item) const;
		IScriptTimer* GetItemScriptTimer(const CBrowserCtrlItem& item) const;
		IScriptAbstractInterfaceImplementation* GetItemScriptAbstractInterfaceImplementation(const CBrowserCtrlItem& item) const;
		IScriptComponentInstance* GetItemScriptComponentInstance(const CBrowserCtrlItem& item) const;
		IScriptActionInstance* GetItemScriptActionInstance(const CBrowserCtrlItem& item) const;
		IDocGraph* GetItemDocGraph(const CBrowserCtrlItem& item) const;
		SBrowserCtrlSignals& Signals();

	protected:

		class CContextMenu
		{
		public:

			CContextMenu();

			~CContextMenu();

			CMenu& GetMenu();
			CContextMenu* CreateSubMenu(const char* szName);

		private:

			typedef std::vector<CContextMenu*> SubMenus;

			CMenu    m_menu;
			SubMenus m_subMenus;
		};

		afx_msg void OnSelChanged(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnBeginLabelEdit(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnEndLabelEdit(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnSetFocus(CWnd* pOldWnd);
		afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
		afx_msg void OnRClick(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
		afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);

		// CCustomTreeCtrl
		virtual void LoadImageList(CImageList& imageList) override;
		// ~CCustomTreeCtrl

	private:

		struct ContextMenuItem
		{
			enum EValue
			{
				COPY = 1,
				PASTE,
				CLONE,
				RENAME,
				REMOVE,
				REFRESH, // Temporary solution for dependency tracking issues. Ideally we should always be able to work out in code when to refresh items.
				SHOW_IN_EXPLORER,
				DIFF_AGAINST_HAVE_VERSION,
				CREATE_DOC,
				ADD_INCLUDE,
				ADD_GROUP,
				ADD_ENUMERATION,
				ADD_STRUCTURE,
				ADD_SIGNAL,
				ADD_ABSTRACT_INTERFACE,
				ADD_ABSTRACT_INTERFACE_FUNCTION,
				ADD_ABSTRACT_INTERFACE_TASK,
				ADD_CLASS,
				ADD_STATE_MACHINE,
				ADD_PERSISTENT_STATE_MACHINE,
				ADD_TASK_STATE_MACHINE,
				ADD_STATE,
				ADD_VARIABLE,
				ADD_PROPERTY,
				ADD_CONTAINER,
				ADD_TIMER,
				ADD_ABSTRACT_INTERFACE_IMPLEMENTATION,
				ADD_COMPONENT_INSTANCE,
				ADD_ACTION_INSTANCE,
				ADD_CONSTRUCTOR_GRAPH,
				ADD_DESTRUCTOR_GRAPH,
				ADD_SIGNAL_RECEIVER_GRAPH,
				ADD_FUNCTION_GRAPH,
				ADD_CONDITION_GRAPH,
				FIND_REFERENCES,
				FIND_REFEFERENCES_IN_WHOLE_REGISTRY
			};
		};

		CBrowserCtrlItemPtr GetItem(const SGUID& guid);
		void RefreshItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags);
		void RefreshFolderItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags);
		void RefreshDocItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags);
		void RefreshGroupItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags);
		void RefreshAbstractInterfaceItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags);
		void RefreshAbstractInterfaceTaskItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags);
		void RefreshClassItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags);
		void RefreshClassBaseItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags);
		void RefreshStateMachineItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags);
		void RefreshStateItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags);
		void RefreshAbstractInterfaceImplementationItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags);
		void RefreshComponentInstanceItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags);
		void RefreshSettingsFolderItem(const CBrowserCtrlItemPtr& pItem, EBrowserRefreshFlags flags);
		void InitContextMenu(const CBrowserCtrlItemPtr& pItem, CContextMenu& contextMenu);
		void CopyItem(const CBrowserCtrlItemPtr& pItem);
		void PasteToItem(const CBrowserCtrlItemPtr& pItem);
		void CloneItem(const CBrowserCtrlItemPtr& pItem);
		void RenameItem(const CBrowserCtrlItemPtr& pItem);
		void RemoveItem(const CBrowserCtrlItemPtr& pItem);
		void OnShowInExplorer(CPoint point, const CBrowserCtrlItemPtr& pItem);
		void OnDiffAgainstHaveVersion(const CBrowserCtrlItemPtr& pItem);
		void OnCreateDoc(CPoint point, const CBrowserCtrlItemPtr& pItem);
		void OnAddInclude(CPoint point, const CBrowserCtrlItemPtr& pItem);
		void OnAddGroup(CPoint point, const CBrowserCtrlItemPtr& pItem);
		void OnAddEnumeration(CPoint point, const CBrowserCtrlItemPtr& pItem);
		void OnAddStructure(CPoint point, const CBrowserCtrlItemPtr& pItem);
		void OnAddSignal(CPoint point, const CBrowserCtrlItemPtr& pItem);
		void OnAddAbstractInterface(CPoint point, const CBrowserCtrlItemPtr& pItem);
		void OnAddAbstractInterfaceFunction(CPoint point, const CBrowserCtrlItemPtr& pItem);
		void OnAddAbstractInterfaceTask(CPoint point, const CBrowserCtrlItemPtr& pItem);
		void OnAddClass(CPoint point, const CBrowserCtrlItemPtr& pItem);
		void OnAddStateMachine(CPoint point, const CBrowserCtrlItemPtr& pItem, EStateMachineLifetime lifetime);
		void OnAddState(CPoint point, const CBrowserCtrlItemPtr& pItem);
		void OnAddVariable(CPoint point, const CBrowserCtrlItemPtr& pItem);
		void OnAddProperty(CPoint point, const CBrowserCtrlItemPtr& pItem);
		void OnAddContainer(CPoint point, const CBrowserCtrlItemPtr& pItem);
		void OnAddTimer(CPoint point, const CBrowserCtrlItemPtr& pItem);
		void OnAddAbstractInterfaceImplementation(CPoint point, const CBrowserCtrlItemPtr& pItem);
		void OnAddComponentInstance(CPoint point, const CBrowserCtrlItemPtr& pItem);
		void OnAddActionInstance(CPoint point, const CBrowserCtrlItemPtr& pItem);
		void OnAddGraph(CPoint point, const CBrowserCtrlItemPtr& pItem, EScriptGraphType type);

		SBrowserCtrlSignals m_signals;
	};
}
