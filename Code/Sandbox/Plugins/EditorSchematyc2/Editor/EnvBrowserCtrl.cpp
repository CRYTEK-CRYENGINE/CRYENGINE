// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EnvBrowserCtrl.h"

#include <CrySchematyc2/ISignal.h>
#include <CrySchematyc2/Deprecated/IGlobalFunction.h>

#include "BrowserIcons.h"
#include "PluginUtils.h"

namespace Schematyc2
{
	namespace EnvBrowserUtils
	{
		inline void ViewSource(const char* fileName)
		{
			CRY_ASSERT(fileName);
			if(fileName)
			{
				STARTUPINFO startupInfo;
				ZeroMemory(&startupInfo, sizeof(startupInfo));
				startupInfo.dwFlags			= STARTF_USESHOWWINDOW;
				startupInfo.wShowWindow	= SW_HIDE;

				PROCESS_INFORMATION	processInformation;
				ZeroMemory(&processInformation, sizeof(processInformation));

				char	currentDirectory[512];
				GetCurrentDirectory(sizeof(currentDirectory) - 1, currentDirectory);

				char	commandLine[1024] = "devenv /edit ";
				cry_strcat(commandLine, currentDirectory);
				cry_strcat(commandLine, "\\");
				cry_strcat(commandLine, fileName);

				if(CreateProcess(NULL, commandLine, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInformation))
				{
					CloseHandle(processInformation.hThread);
					CloseHandle(processInformation.hProcess);
				}
			}
		}

		inline void ShowInExplorer(const char* fileName)
		{
			string	filePath = fileName;
			filePath.erase(filePath.find_last_of("\\"));
			PluginUtils::ShowInExplorer(filePath.c_str());
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CEnvBrowserCtrlItem::CEnvBrowserCtrlItem(const char* text, size_t icon, const CCustomTreeCtrlItemPtr& pParent, const SGUID& guid)
		: CCustomTreeCtrlItem(text, icon, pParent, guid)
	{}

	//////////////////////////////////////////////////////////////////////////
	BEGIN_MESSAGE_MAP(CEnvBrowserCtrl, CCustomTreeCtrl)
		ON_NOTIFY_REFLECT(TVN_BEGINDRAG, OnBeginDrag)
		ON_WM_LBUTTONDBLCLK()
		ON_NOTIFY_REFLECT(NM_RCLICK, OnRClk)
		ON_WM_CONTEXTMENU()
	END_MESSAGE_MAP()

	//////////////////////////////////////////////////////////////////////////
	CEnvBrowserCtrl::CEnvBrowserCtrl()
		: CCustomTreeCtrl()
	{}

	//////////////////////////////////////////////////////////////////////////
	BOOL CEnvBrowserCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
	{
		LOADING_TIME_PROFILE_SECTION;
		if(CCustomTreeCtrl::Create(dwStyle, rect, pParentWnd, nID))
		{
			Refresh();
			return true;
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvBrowserCtrl::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult) 
	{
		if(CEnvBrowserCtrlItemPtr pItem = std::static_pointer_cast<CEnvBrowserCtrlItem>(CCustomTreeCtrl::FindItem(reinterpret_cast<NM_TREEVIEW*>(pNMHDR)->ptDrag)))
		{
			switch(pItem->GetIcon())
			{
			case BrowserIcon::BRANCH:
			case BrowserIcon::FOR_LOOP:
			case BrowserIcon::RETURN:
			case BrowserIcon::ENV_SIGNAL:
			case BrowserIcon::ENV_GLOBAL_FUNCTION:
			case BrowserIcon::ENV_COMPONENT_FUNCTION:
			case BrowserIcon::ENV_ACTION_FUNCTION:
				{
					PluginUtils::BeginDragAndDrop(PluginUtils::SDragAndDropData(pItem->GetIcon(), pItem->GetGUID()));
					break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvBrowserCtrl::OnLButtonDblClk(UINT nFlags, CPoint point)
	{
		if(CEnvBrowserCtrlItemPtr pItem = std::static_pointer_cast<CEnvBrowserCtrlItem>(CCustomTreeCtrl::FindItem(point)))
		{
			switch(pItem->GetIcon())
			{
			case BrowserIcon::ENV_SIGNAL:
				{
					ISignalConstPtr	pSignal = GetSchematyc()->GetEnvRegistry().GetSignal(pItem->GetGUID());
					CRY_ASSERT(pSignal != NULL);
					if(pSignal != NULL)
					{
						EnvBrowserUtils::ViewSource(pSignal->GetFileName());
					}
					break;
				}
			case BrowserIcon::ENV_GLOBAL_FUNCTION:
				{
					IGlobalFunctionConstPtr	pFunction = GetSchematyc()->GetEnvRegistry().GetGlobalFunction(pItem->GetGUID());
					CRY_ASSERT(pFunction != NULL);
					if(pFunction != NULL)
					{
						EnvBrowserUtils::ViewSource(pFunction->GetFileName());
					}
					break;
				}
			case BrowserIcon::ABSTRACT_INTERFACE_IMPLEMENTATION:
				{
					IAbstractInterfaceConstPtr	pInterface = GetSchematyc()->GetEnvRegistry().GetAbstractInterface(pItem->GetGUID());
					CRY_ASSERT(pInterface != NULL);
					if(pInterface != NULL)
					{
						EnvBrowserUtils::ViewSource(pInterface->GetFileName());
					}
					break;
				}
			case BrowserIcon::ABSTRACT_INTERFACE_FUNCTION_IMPLEMENTATION:
				{
					// #SchematycTODO : Traverse back up the tree hierarchy in order to find the interface this function belongs to?
					break;
				}
			case BrowserIcon::ENV_COMPONENT:
				{
					IComponentFactoryConstPtr	pComponentFactory = GetSchematyc()->GetEnvRegistry().GetComponentFactory(pItem->GetGUID());
					CRY_ASSERT(pComponentFactory != NULL);
					if(pComponentFactory != NULL)
					{
						EnvBrowserUtils::ViewSource(pComponentFactory->GetFileName());
					}
					break;
				}
			case BrowserIcon::ENV_COMPONENT_FUNCTION:
				{
					IComponentMemberFunctionConstPtr	pFunction = GetSchematyc()->GetEnvRegistry().GetComponentMemberFunction(pItem->GetGUID());
					CRY_ASSERT(pFunction != NULL);
					if(pFunction != NULL)
					{
						EnvBrowserUtils::ViewSource(pFunction->GetFileName());
					}
					break;
				}
			case BrowserIcon::ENV_ACTION:
				{
					IActionFactoryConstPtr	pActionFactory = GetSchematyc()->GetEnvRegistry().GetActionFactory(pItem->GetGUID());
					CRY_ASSERT(pActionFactory != NULL);
					if(pActionFactory != NULL)
					{
						EnvBrowserUtils::ViewSource(pActionFactory->GetFileName());
					}
					break;
				}
			case BrowserIcon::ENV_ACTION_FUNCTION:
				{
					IActionMemberFunctionConstPtr	pFunction = GetSchematyc()->GetEnvRegistry().GetActionMemberFunction(pItem->GetGUID());
					CRY_ASSERT(pFunction != NULL);
					if(pFunction != NULL)
					{
						EnvBrowserUtils::ViewSource(pFunction->GetFileName());
					}
					break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvBrowserCtrl::OnRClk(NMHDR* pNMHDR, LRESULT* pResult)
	{
		SendMessage(WM_CONTEXTMENU, (WPARAM)m_hWnd, GetMessagePos());
		*pResult = 1;
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvBrowserCtrl::OnContextMenu(CWnd* pWnd, CPoint point)
	{
		CPoint	clientPoint = point;
		CCustomTreeCtrl::ScreenToClient(&clientPoint);
		if(CEnvBrowserCtrlItemPtr pItem = std::static_pointer_cast<CEnvBrowserCtrlItem>(CCustomTreeCtrl::FindItem(clientPoint)))
		{
			const char*	wikiLink = "";
			const char*	fileName = "";
			switch(pItem->GetIcon())
			{
			case BrowserIcon::ENV_SIGNAL:
				{
					ISignalConstPtr	pSignal = GetSchematyc()->GetEnvRegistry().GetSignal(pItem->GetGUID());
					CRY_ASSERT(pSignal != NULL);
					if(pSignal != NULL)
					{
						wikiLink = pSignal->GetWikiLink();
						fileName = pSignal->GetFileName();
					}
					break;
				}
			case BrowserIcon::ENV_GLOBAL_FUNCTION:
				{
					IGlobalFunctionConstPtr	pFunction = GetSchematyc()->GetEnvRegistry().GetGlobalFunction(pItem->GetGUID());
					CRY_ASSERT(pFunction != NULL);
					if(pFunction != NULL)
					{
						wikiLink = pFunction->GetWikiLink();
						fileName = pFunction->GetFileName();
					}
					break;
				}
			case BrowserIcon::ABSTRACT_INTERFACE_IMPLEMENTATION:
				{
					IAbstractInterfaceConstPtr	pInterface = GetSchematyc()->GetEnvRegistry().GetAbstractInterface(pItem->GetGUID());
					CRY_ASSERT(pInterface != NULL);
					if(pInterface != NULL)
					{
						fileName = pInterface->GetFileName();
					}
					break;
				}
			case BrowserIcon::ABSTRACT_INTERFACE_FUNCTION_IMPLEMENTATION:
				{
					// #SchematycTODO : Traverse back up the tree hierarchy in order to find the interface this function belongs to?
					break;
				}
			case BrowserIcon::ENV_COMPONENT:
				{
					IComponentFactoryConstPtr	pComponentFactory = GetSchematyc()->GetEnvRegistry().GetComponentFactory(pItem->GetGUID());
					CRY_ASSERT(pComponentFactory != NULL);
					if(pComponentFactory != NULL)
					{
						wikiLink	= pComponentFactory->GetWikiLink();
						fileName	= pComponentFactory->GetFileName();
					}
					break;
				}
			case BrowserIcon::ENV_COMPONENT_FUNCTION:
				{
					IComponentMemberFunctionConstPtr	pFunction = GetSchematyc()->GetEnvRegistry().GetComponentMemberFunction(pItem->GetGUID());
					CRY_ASSERT(pFunction != NULL);
					if(pFunction != NULL)
					{
						wikiLink = pFunction->GetWikiLink();
						fileName = pFunction->GetFileName();
					}
					break;
				}
			case BrowserIcon::ENV_ACTION:
				{
					IActionFactoryConstPtr	pActionFactory = GetSchematyc()->GetEnvRegistry().GetActionFactory(pItem->GetGUID());
					CRY_ASSERT(pActionFactory != NULL);
					if(pActionFactory != NULL)
					{
						wikiLink	= pActionFactory->GetWikiLink();
						fileName	= pActionFactory->GetFileName();
					}
					break;
				}
			case BrowserIcon::ENV_ACTION_FUNCTION:
				{
					IActionMemberFunctionConstPtr	pFunction = GetSchematyc()->GetEnvRegistry().GetActionMemberFunction(pItem->GetGUID());
					CRY_ASSERT(pFunction != NULL);
					if(pFunction != NULL)
					{
						wikiLink = pFunction->GetWikiLink();
						fileName = pFunction->GetFileName();
					}
					break;
				}
			}

			CMenu popupMenu;
			popupMenu.CreatePopupMenu();
			if(wikiLink[0])
			{
				popupMenu.AppendMenu(MF_ENABLED, ContextMenuItem::VIEW_WIKI, "View Wiki");
			}
			if(fileName[0])
			{
				popupMenu.AppendMenu(MF_ENABLED, ContextMenuItem::VIEW_SOURCE, "View Source");
				popupMenu.AppendMenu(MF_ENABLED, ContextMenuItem::SHOW_IN_EXPLORER, "Show In Explorer");
			}
			switch(popupMenu.TrackPopupMenuEx(TPM_RETURNCMD, point.x, point.y, this, NULL))
			{
			case ContextMenuItem::VIEW_WIKI:
				{
					PluginUtils::ViewWiki(wikiLink);
					break;
				}
			case ContextMenuItem::VIEW_SOURCE:
				{
					EnvBrowserUtils::ViewSource(fileName);
					break;
				}
			case ContextMenuItem::SHOW_IN_EXPLORER:
				{
					EnvBrowserUtils::ShowInExplorer(fileName);
					break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvBrowserCtrl::LoadImageList(CImageList& imageList)
	{
		LoadBrowserIcons(imageList);
		SetImageList(&imageList, TVSIL_NORMAL);
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvBrowserCtrl::Refresh()
	{
		CWnd::SetRedraw(FALSE);
		CCustomTreeCtrl::Reset();
		// Visit environment registry and add elements to tree control.
		// N.B. We visit signals last to ensure owner items exist.
		IEnvRegistry&	envRegistry = GetSchematyc()->GetEnvRegistry();
		envRegistry.VisitGlobalFunctions(EnvGlobalFunctionVisitor::FromMemberFunction<CEnvBrowserCtrl, &CEnvBrowserCtrl::VisitGlobalFunction>(*this));
		envRegistry.VisitAbstractInterfaces(EnvAbstractInterfaceVisitor::FromMemberFunction<CEnvBrowserCtrl, &CEnvBrowserCtrl::VisitAbstractInterface>(*this));
		envRegistry.VisitComponentFactories(EnvComponentFactoryVisitor::FromMemberFunction<CEnvBrowserCtrl, &CEnvBrowserCtrl::VisitComponentFactory>(*this));
		envRegistry.VisitComponentMemberFunctions(EnvComponentMemberFunctionVisitor::FromMemberFunction<CEnvBrowserCtrl, &CEnvBrowserCtrl::VisitComponentMemberFunction>(*this));
		envRegistry.VisitActionFactories(EnvActionFactoryVisitor::FromMemberFunction<CEnvBrowserCtrl, &CEnvBrowserCtrl::VisitActionFactory>(*this));
		envRegistry.VisitActionMemberFunctions(EnvActionMemberFunctionVisitor::FromMemberFunction<CEnvBrowserCtrl, &CEnvBrowserCtrl::VisitActionMemberFunction>(*this));
		envRegistry.VisitSignals(EnvSignalVisitor::FromMemberFunction<CEnvBrowserCtrl, &CEnvBrowserCtrl::VisitSignal>(*this));
		CWnd::SetRedraw(TRUE);
	}

	//////////////////////////////////////////////////////////////////////////
	CEnvBrowserCtrlItemPtr CEnvBrowserCtrl::AddItem(const char* text, size_t icon, const CCustomTreeCtrlItemPtr& pParentItem, const SGUID& guid)
	{
		CEnvBrowserCtrlItemPtr	pItem(new CEnvBrowserCtrlItem(text, icon, pParentItem, guid));
		CCustomTreeCtrl::AddItem(pItem);
		return pItem;
	}

	//////////////////////////////////////////////////////////////////////////
	CEnvBrowserCtrlItemPtr CEnvBrowserCtrl::FindItem(const SGUID& guid)
	{
		CCustomTreeCtrlItemPtr pItem = CCustomTreeCtrl::FindItem(guid);
		if(pItem)
		{
			CRY_ASSERT(pItem->GetGUID() == guid);
			return std::static_pointer_cast<CEnvBrowserCtrlItem>(pItem);
		}
		return CEnvBrowserCtrlItemPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	CEnvBrowserCtrlItemPtr CEnvBrowserCtrl::GetOrCreateParentItem(const SGUID& ownerGUID, const char* szNamespace)
	{
		if(ownerGUID)
		{
			return FindItem(ownerGUID);
		}
		else
		{
			CCustomTreeCtrlItemPtr	pParentItem;
			if(szNamespace && (szNamespace[0] != '\0'))
			{
				stack_string tokens = szNamespace;
				if(const size_t length = tokens.length())
				{
					int	pos = 0;
					do
					{
						stack_string						token = tokens.Tokenize("::", pos);
						CCustomTreeCtrlItemPtr	pItem = CCustomTreeCtrl::FindItem(token.c_str(), pParentItem);
						if(!pItem)
						{
							pItem = AddItem(token.c_str(), BrowserIcon::FOLDER, pParentItem);
						}
						pParentItem	= pItem;
					} while(pos < length);
				}
			}
			return std::static_pointer_cast<CEnvBrowserCtrlItem>(pParentItem);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CEnvBrowserCtrl::VisitSignal(const ISignalConstPtr& pSignal)
	{
		AddItem(pSignal->GetName(), BrowserIcon::ENV_SIGNAL, GetOrCreateParentItem(pSignal->GetSenderGUID(), pSignal->GetNamespace()), pSignal->GetGUID());
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CEnvBrowserCtrl::VisitGlobalFunction(const IGlobalFunctionConstPtr& pFunction)
	{
		AddItem(pFunction->GetName(), BrowserIcon::ENV_GLOBAL_FUNCTION, GetOrCreateParentItem(SGUID(), pFunction->GetNamespace()), pFunction->GetGUID());
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CEnvBrowserCtrl::VisitAbstractInterface(const IAbstractInterfaceConstPtr& pAbstractInterface)
	{
		CEnvBrowserCtrlItemPtr	pInterfaceItem = AddItem(pAbstractInterface->GetName(), BrowserIcon::ABSTRACT_INTERFACE_IMPLEMENTATION, GetOrCreateParentItem(SGUID(), pAbstractInterface->GetNamespace()), pAbstractInterface->GetGUID());
		for(size_t iFunction = 0, functionCount = pAbstractInterface->GetFunctionCount(); iFunction < functionCount; ++ iFunction)
		{
			IAbstractInterfaceFunctionConstPtr	pFunction = pAbstractInterface->GetFunction(iFunction);
			AddItem(pFunction->GetName(), BrowserIcon::ABSTRACT_INTERFACE_FUNCTION_IMPLEMENTATION, pInterfaceItem, pFunction->GetGUID());
		}
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CEnvBrowserCtrl::VisitComponentFactory(const IComponentFactoryConstPtr& pComponentFactory)
	{
		AddItem(pComponentFactory->GetName(), BrowserIcon::ENV_COMPONENT, GetOrCreateParentItem(SGUID(), pComponentFactory->GetNamespace()), pComponentFactory->GetComponentGUID());
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CEnvBrowserCtrl::VisitComponentMemberFunction(const IComponentMemberFunctionConstPtr& pFunction)
	{
		AddItem(pFunction->GetName(), BrowserIcon::ENV_COMPONENT_FUNCTION, GetOrCreateParentItem(pFunction->GetComponentGUID(), nullptr), pFunction->GetGUID());
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CEnvBrowserCtrl::VisitActionFactory(const IActionFactoryConstPtr& pActionFactory)
	{
		AddItem(pActionFactory->GetName(), BrowserIcon::ENV_ACTION, GetOrCreateParentItem(pActionFactory->GetComponentGUID(), pActionFactory->GetNamespace()), pActionFactory->GetActionGUID());
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CEnvBrowserCtrl::VisitActionMemberFunction(const IActionMemberFunctionConstPtr& pFunction)
	{
		AddItem(pFunction->GetName(), BrowserIcon::ENV_ACTION_FUNCTION, GetOrCreateParentItem(pFunction->GetActionGUID(), pFunction->GetNamespace()), pFunction->GetGUID());
		return EVisitStatus::Continue;
	}
}
