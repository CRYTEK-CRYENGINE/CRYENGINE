// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Unify uri format used by CreateUri and Execute.

#pragma once

#include <CrySystem/IConsole.h>

#include <CrySchematyc2/GUID.h>
#include <CrySchematyc2/Utils/IString.h>
#include <CrySchematyc2/Utils/StackString.h>

namespace Schematyc2
{
namespace CryLinkUtils
{
enum class ECommand
{
	None = 0,     // Do nothing.
	Show          // Show element in editor.
};

inline bool CreateUri(IString& output, ECommand command, const Schematyc2::SGUID& elementGUID, const Schematyc2::SGUID& detailGUID = Schematyc2::SGUID())
{
	CStackString commandLine;
	switch (command)
	{
	case ECommand::Show:
		{
			CStackString temp;
			commandLine = "sc_rpcShowLegacy ";
			Schematyc2::StringUtils::SysGUIDToString(elementGUID.sysGUID, temp);
			commandLine.append(temp);
			commandLine.append(" ");
			Schematyc2::StringUtils::SysGUIDToString(detailGUID.sysGUID, temp);
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

inline void ExecuteCommand(ECommand command, const Schematyc2::SGUID& elementGUID, const Schematyc2::SGUID& detailGUID = Schematyc2::SGUID())
{
	CStackString uri;
	CreateUri(uri, command, elementGUID, detailGUID);
	ExecuteUri(uri.c_str());
}
} // CryLinkUtils
} // Schematyc
