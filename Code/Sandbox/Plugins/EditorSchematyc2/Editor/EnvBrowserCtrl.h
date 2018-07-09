// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once


#include <CrySchematyc2/IAbstractInterface.h>
#include <CrySchematyc2/Env/IEnvRegistry.h>

#include "TreeCtrl.h"

namespace Schematyc2
{
	class CEnvBrowserCtrlItem : public CCustomTreeCtrlItem
	{
	public:

		CEnvBrowserCtrlItem(const char* text, size_t icon, const CCustomTreeCtrlItemPtr& pParent, const SGUID& guid);
	};

	DECLARE_SHARED_POINTERS(CEnvBrowserCtrlItem)

	class CEnvBrowserCtrl : public CCustomTreeCtrl
	{
		DECLARE_MESSAGE_MAP()

	public:

		CEnvBrowserCtrl();

		// CCustomTreeCtrl
		virtual BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
		// ~CCustomTreelCtrl

		void Refresh();

	protected:

		afx_msg void OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
		afx_msg void OnRClk(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);

		// CCustomTreeCtrl
		virtual void LoadImageList(CImageList& imageList) override;
		// ~CCustomTreeCtrl

	private:

		struct ContextMenuItem
		{
			enum EValue
			{
				VIEW_WIKI = 1,
				VIEW_SOURCE,
				SHOW_IN_EXPLORER
			};
		};

		CEnvBrowserCtrlItemPtr AddItem(const char* text, size_t icon, const CCustomTreeCtrlItemPtr& pParentItem = CCustomTreeCtrlItemPtr(), const SGUID& guid = SGUID());
		CEnvBrowserCtrlItemPtr FindItem(const SGUID& guid);
		CEnvBrowserCtrlItemPtr GetOrCreateParentItem(const SGUID& ownerGUID, const char* scope);
		EVisitStatus VisitSignal(const ISignalConstPtr& pSignal);
		EVisitStatus VisitGlobalFunction(const IGlobalFunctionConstPtr& pFunction);
		EVisitStatus VisitAbstractInterface(const IAbstractInterfaceConstPtr& pAbstractInterface);
		EVisitStatus VisitComponentFactory(const IComponentFactoryConstPtr& pComponentFactory);
		EVisitStatus VisitComponentMemberFunction(const IComponentMemberFunctionConstPtr& pFunction);
		EVisitStatus VisitActionFactory(const IActionFactoryConstPtr& pActionFactory);
		EVisitStatus VisitActionMemberFunction(const IActionMemberFunctionConstPtr& pFunction);
	};
}
