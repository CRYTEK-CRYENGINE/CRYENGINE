// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryLinkCommands.h"

#include "MainWindow.h"

namespace Schematyc
{
static void RpcEditorShow(IConsoleCmdArgs* pArgs)
{
	if (gEnv->IsEditor())
	{
		CryGUID elementGUID;
		if (pArgs->GetArgCount() >= 2)
		{
			elementGUID = CryGUID::FromString(pArgs->GetArg(1));
		}

		CryGUID detailGUID;
		if (pArgs->GetArgCount() >= 3)
		{
			detailGUID = CryGUID::FromString(pArgs->GetArg(2));
		}

		if (!GUID::IsEmpty(elementGUID))
		{
			const char* szPaneTitle = "Schematyc";
			GetIEditor()->OpenView(szPaneTitle);
			// TODO: Try to find an opened instance or open a new editor window.
			/*CMainWindow* pMainWindow = CMainWindow::GetInstance();
			   if (pMainWindow)
			   {
			   pMainWindow->Show(elementGUID, detailGUID);
			   }*/
		}
	}
}

CCryLinkCommands::CCryLinkCommands()
	: m_pConsole(nullptr)
	, m_bRegistered(false)
{}

CCryLinkCommands::~CCryLinkCommands()
{
	Unregister();
}

void CCryLinkCommands::Register(IConsole* pConsole)
{
	CRY_ASSERT(pConsole);
	m_pConsole = pConsole;
	if (m_pConsole && !m_bRegistered)
	{
		REGISTER_COMMAND("sc_rpcShow", RpcEditorShow, VF_NULL, "Show in Schematyc editor");
		m_bRegistered = true;
	}
}

void CCryLinkCommands::Unregister()
{
	if (m_pConsole && m_bRegistered)
	{
		m_pConsole->RemoveCommand("sc_rpcShow");
		m_bRegistered = false;
	}
}

CCryLinkCommands& CCryLinkCommands::GetInstance()
{
	return ms_instance;
}

CCryLinkCommands CCryLinkCommands::ms_instance;
} // Schematyc

