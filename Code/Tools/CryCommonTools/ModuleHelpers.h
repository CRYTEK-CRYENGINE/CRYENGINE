// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MODULEHELPERS_H__
#define __MODULEHELPERS_H__

namespace ModuleHelpers
{
	enum CurrentModuleSpecifier
	{
		CurrentModuleSpecifier_Executable,
		CurrentModuleSpecifier_Library
	};

	HMODULE GetCurrentModule(CurrentModuleSpecifier moduleSpecifier);
	std::basic_string<TCHAR> GetCurrentModulePath(CurrentModuleSpecifier moduleSpecifier);
}

#endif //__MODULEHELPERS_H__
