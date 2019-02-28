// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Unify uri format used by CreateUri and Execute.

#pragma once

#include <CrySystem/IConsole.h>

#include "CrySchematyc/Utils/GUID.h"
#include "CrySchematyc/Utils/IString.h"
#include <CrySchematyc/Utils/StackString.h>

namespace Schematyc
{
namespace CryLinkUtils
{
enum class ECommand
{
	None = 0,     // Do nothing.
	Show          // Show element in editor.
};

inline bool CreateUri(IString& output, ECommand command, const CryGUID& elementGUID, const CryGUID& detailGUID = CryGUID())
{
	CStackString commandLine;
	switch (command)
	{
	case ECommand::Show:
		{
			CStackString temp;
			commandLine = "sc_rpcShow ";
			GUID::ToString(temp, elementGUID);
			commandLine.append(temp);
			commandLine.append(" ");
			GUID::ToString(temp, detailGUID);
			commandLine.append(temp);
			break;
		}
	}
	if (!commandLine.empty())
	{
		CryLinkService::CCryLinkUriFactory cryLinkFactory("editor", CryLinkService::ELinkType::Commands);
		cryLinkFactory.AddCommand(commandLine.c_str(), commandLine.length());
		output.assign(cryLinkFactory.GetUri());
		return true;
	}
	return false;
}

inline void ExecuteUri(const char* szUri)
{
	CryLinkService::CCryLink cryLink(szUri);
	const char* szCmd = cryLink.GetQuery("cmd1");
	if (szCmd && szCmd[0])
	{
		gEnv->pConsole->ExecuteString(szCmd);
	}
}

inline void ExecuteCommand(ECommand command, const CryGUID& elementGUID, const CryGUID& detailGUID = CryGUID())
{
	CStackString uri;
	CreateUri(uri, command, elementGUID, detailGUID);
	ExecuteUri(uri.c_str());
}
} // CryLinkUtils
} // Schematyc
