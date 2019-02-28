// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 2:8:2004   15:20 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "CmdLine.h"

void CCmdLine::PushCommand(const string& sCommand, const string& sParameter)
{
	if (sCommand.empty())
		return;

	ECmdLineArgType type = eCLAT_Normal;
	const char* szCommand = sCommand.c_str();

	if (sCommand[0] == '-')
	{
		type = eCLAT_Pre;
		++szCommand;
	}
	else if (sCommand[0] == '+')
	{
		type = eCLAT_Post;
		++szCommand;
	}

	m_args.push_back(CCmdLineArg(szCommand, sParameter.c_str(), type));
}

CCmdLine::CCmdLine(const char* commandLine)
{
	m_sCmdLine = commandLine;

	char* src = (char*)commandLine;

	string command, parameter;

	for (;; )
	{
		if (*src == '\0')
			break;

		string arg = Next(src);

		if (m_args.empty())
		{
			// this is the filename, convert backslash to forward slash
			arg.replace('\\', '/');
			m_args.push_back(CCmdLineArg("filename", arg.c_str(), eCLAT_Executable));
		}
		else
		{
			bool bSecondCharIsNumber = false;

			if (arg[0] && arg[1] >= '0' && arg[1] <= '9')
				bSecondCharIsNumber = true;

			if ((arg[0] == '-' && !bSecondCharIsNumber)
			    || (arg[0] == '+' && !bSecondCharIsNumber)
			    || command.empty())                     // separator or first parameter
			{
				PushCommand(command, parameter);

				command = arg;
				parameter = "";
			}
			else
			{
				if (parameter.empty())
					parameter = arg;
				else
					parameter += string(" ") + arg;
			}
		}
	}

	PushCommand(command, parameter);
}

CCmdLine::~CCmdLine()
{
}

const ICmdLineArg* CCmdLine::GetArg(int n) const
{
	if ((n >= 0) && (n < (int)m_args.size()))
	{
		return &m_args[n];
	}

	return 0;
}

int CCmdLine::GetArgCount() const
{
	return (int)m_args.size();
}

const ICmdLineArg* CCmdLine::FindArg(const ECmdLineArgType ArgType, const char* name, bool caseSensitive) const
{
	if (caseSensitive)
	{
		for (std::vector<CCmdLineArg>::const_iterator it = m_args.begin(); it != m_args.end(); ++it)
		{
			if (it->GetType() == ArgType)
				if (!strcmp(it->GetName(), name))
					return &(*it);
		}
	}
	else
	{
		for (std::vector<CCmdLineArg>::const_iterator it = m_args.begin(); it != m_args.end(); ++it)
		{
			if (it->GetType() == ArgType)
				if (!stricmp(it->GetName(), name))
					return &(*it);
		}
	}

	return 0;
}

string CCmdLine::Next(char*& src)
{
	char ch = 0;
	char* org = src;

	while (ch = *src++)
	{
		switch (ch)
		{
		case '\'':
		case '\"':
			org = src;

			while ((*src++ != ch) && *src)
				;

			return string(org, src - 1);

		case '[':
			org = src;
			while ((*src++ != ']') && *src)
				;
			return string(org, src - 1);

		case ' ':
			continue;
		default:
			org = src - 1;
			for (; *src != ' ' && *src != '\t' && *src; ++src)
				;

			return string(org, src);
		}
	}

	return string();
}
