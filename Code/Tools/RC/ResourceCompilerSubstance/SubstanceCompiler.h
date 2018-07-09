// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IConverter.h"             // IConverter
#include "SubstanceConverter.h"         // ImageConverter::PresetAliases
#include "ResourceCompiler.h"


enum EConfigPriority; 

class CSubstanceCompiler 
	: public ICompiler
{
	friend class CGenerationProgress;

public:

	explicit CSubstanceCompiler(CSubstanceConverter* converter);
	virtual ~CSubstanceCompiler();
	// interface ICompiler ----------------------------------------------------

	virtual void Release();
	virtual void BeginProcessing(const IConfig* config);
	virtual void EndProcessing();
	virtual IConvertContext* GetConvertContext() { return &m_CC; }
	virtual bool Process();

	// ------------------------------------------------------------------------


	static bool CheckForExistingPreset(const ConvertContext& CC, const string& presetName, bool errorInsteadOfWarning);

private:
	string GetOutputFileNameOnly() const;
	string GetOutputPath() const;

	// actual implementation of Process method
	bool ProcessImplementation();

public:  // ------------------------------------------------------------------
	ConvertContext            m_CC;


private: // ------------------------------------------------------------------
	CSubstanceConverter* m_pConverter;
};
