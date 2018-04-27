// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ModuleHelpers.h"

HMODULE ModuleHelpers::GetCurrentModule(CurrentModuleSpecifier moduleSpecifier)
{
	switch (moduleSpecifier)
	{
	case CurrentModuleSpecifier_Executable:
		return GetModuleHandle(0);

	case CurrentModuleSpecifier_Library:
		MEMORY_BASIC_INFORMATION mbi;
		static int dummy;
		VirtualQuery( &dummy, &mbi, sizeof(mbi) );
		HMODULE instance = reinterpret_cast<HMODULE>(mbi.AllocationBase);
		return instance;
	}

	return 0;
}

std::basic_string<TCHAR> ModuleHelpers::GetCurrentModulePath(CurrentModuleSpecifier moduleSpecifier)
{
  // Here's a trick that will get you the handle of the module
  // you're running in without any a-priori knowledge:
  // http://www.dotnet247.com/247reference/msgs/13/65259.aspx
	HMODULE instance = GetCurrentModule(moduleSpecifier);
	TCHAR moduleNameBuffer[MAX_PATH];
	GetModuleFileName(instance, moduleNameBuffer, sizeof(moduleNameBuffer) / sizeof(moduleNameBuffer[0]));
	return moduleNameBuffer;
}
