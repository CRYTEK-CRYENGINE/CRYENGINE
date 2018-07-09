// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "CryLink.h"
#include "MainFrameWnd.h"

namespace Schematyc2
{
	CCryLinkCommands CCryLinkCommands::s_instance;

	static void RpcEditorShow(IConsoleCmdArgs* pArgs)
	{
		if(gEnv->IsEditor())
		{
			SGUID itemGuid;			
			if(pArgs->GetArgCount() >= 2)
			{
				itemGuid = StringUtils::SysGUIDFromString(pArgs->GetArg(1));
			}

			SGUID childGuid;
			if(pArgs->GetArgCount() >= 3)
			{
				childGuid = StringUtils::SysGUIDFromString(pArgs->GetArg(2));
			}

			if(itemGuid)
			{
				if(CWnd* mainWnd = GetIEditor()->OpenView("Schematyc2"))
				{
					static_cast<CMainFrameWnd*>(mainWnd)->SelectItem(itemGuid, childGuid);
				}
			}
		}
	}

	static void RpcRefreshDetailView(IConsoleCmdArgs* pArgs)
	{
		if (gEnv->IsEditor())
		{
			if (CWnd* mainWnd = GetIEditor()->FindView("Schematyc2"))
			{
				static_cast<CMainFrameWnd*>(mainWnd)->UpdatePreview();
			}
		}
	}

	static void RpcEditorGoto(IConsoleCmdArgs* pArgs)
	{
		if (gEnv->IsEditor())
		{
			SGUID itemGuid;
			if (pArgs->GetArgCount() >= 2)
			{
				itemGuid = StringUtils::SysGUIDFromString(pArgs->GetArg(1));
			}

			SGUID lastItemGuid;
			if (pArgs->GetArgCount() >= 3)
			{
				lastItemGuid = StringUtils::SysGUIDFromString(pArgs->GetArg(2));
			}

			if (itemGuid)
			{
				if (CWnd* mainWnd = GetIEditor()->OpenView("Schematyc2"))
				{
					static_cast<CMainFrameWnd*>(mainWnd)->GotoItem(itemGuid, lastItemGuid);
				}
			}
		}
	}

	static void DiffSelectedItem()
	{
		if (CWnd* mainWnd = GetIEditor()->OpenView("Schematyc2"))
		{
			static_cast<CMainFrameWnd*>(mainWnd)->DiffSelectedItem();
		}
	}

	static void RpcEditorShowAndDiff(IConsoleCmdArgs* pArgs)
	{
		RpcEditorShow(pArgs);
		DiffSelectedItem();
	}

	CCryLinkCommands::CCryLinkCommands()
		: m_pConsole(nullptr)
		, m_bRegistered(false)
	{

	}

	CCryLinkCommands::~CCryLinkCommands()
	{
		Unregister();
	}

	void CCryLinkCommands::Register(IConsole* pConsole)
	{
		CRY_ASSERT(pConsole);
		m_pConsole = pConsole;
		if(m_pConsole && !m_bRegistered)
		{
			REGISTER_COMMAND("sc_rpcShowLegacy", RpcEditorShow, VF_NULL, "Opens the specified graph in Schematyc editor and focuses the optional child: Properties: docGuid childGuid");
			REGISTER_COMMAND("sc_rpcGotoLegacy", RpcEditorGoto, VF_NULL, "Opens the specified graph in Schematyc editor");
			REGISTER_COMMAND("sc_rpcShowAndDiffLegacy", RpcEditorShowAndDiff, VF_NULL, "Opens the specified graph in Schematyc editor and diff with have version: Properties: docGuid");
			REGISTER_COMMAND("sc_rpcRefreshDetailViewLegacy", RpcRefreshDetailView, VF_NULL, "Refreshes the detail view");

			m_bRegistered = true;
		}
	}

	void CCryLinkCommands::Unregister()
	{
		if(m_pConsole && m_bRegistered)
		{
			m_pConsole->RemoveCommand("sc_rpcShowLegacy");
			m_pConsole->RemoveCommand("sc_rpcGotoLegacy");
			m_pConsole->RemoveCommand("sc_rpcShowAndDiffLegacy");
			m_pConsole->RemoveCommand("sc_rpcRefreshDetailViewLegacy");

			m_bRegistered = false;
		}
	}
}
