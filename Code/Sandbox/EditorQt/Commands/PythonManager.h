// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "BoostPythonMacros.h"

class CEditorPythonManager : public IPythonManager
{
public:
	CEditorPythonManager() {}
	~CEditorPythonManager() { Deinit(); }

	void                              Init();
	void                              Deinit();

	virtual void                      RegisterPythonCommand(const SPythonCommand& command) override;
	virtual void                      RegisterPythonModule(const SPythonModule& module) override;
	virtual void                      Execute(const char* command) override;
	virtual float                     GetAsFloat(const char* variable) override;
	const std::vector<SPythonModule>& GetPythonModules() const { return m_pythonModules; }

private:

	std::vector<SPythonModule> m_pythonModules;
};

