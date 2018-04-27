// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SingleThreadedConverter.h"

class CMetadataCompiler : public CSingleThreadedCompiler
{
public:
	CMetadataCompiler(IResourceCompiler* pRc);
	virtual ~CMetadataCompiler();

	// Inherited via ICompiler
	virtual void BeginProcessing(const IConfig* config) override;
	virtual void EndProcessing() override;
	virtual bool Process() override;

	// Inherited via IConverter
	virtual const char* GetExt(int index) const override;

private:
	IResourceCompiler* m_pResourceCompiler;
};
