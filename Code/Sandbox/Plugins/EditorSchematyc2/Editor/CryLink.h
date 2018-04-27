// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma  once

struct IConsole;

namespace Schematyc2
{
	class CCryLinkCommands
	{
	public:
		static CCryLinkCommands& GetInstance() { return s_instance; }

		void Register(IConsole* pConsole);
		void Unregister();

	private:
		CCryLinkCommands();
		~CCryLinkCommands();

	private:
		static CCryLinkCommands s_instance;

		IConsole* m_pConsole;
		bool m_bRegistered;
	};
}