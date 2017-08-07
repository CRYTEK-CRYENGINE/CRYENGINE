// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SingleThreadedConverter.h"


class CFbxConverter : public CSingleThreadedCompiler
{
public:

public:
	CFbxConverter();
	~CFbxConverter();

	virtual void BeginProcessing(const IConfig* config);
	virtual void EndProcessing() { }
	virtual bool Process();

	virtual const char* GetExt(int index) const;
};
