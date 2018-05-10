// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "substance/framework/inputimage.h"
#include "IConverter.h"  // IConverter
#include <CryCore/StlUtils.h>    // stl::less_stricmp

struct IResourceCompiler;
class ISubstanceInstanceRenderer;
class ISubstancePreset;

class CSubstanceConverter
	: public IConverter
{
public:
	// Maps aliases of presets to real names of presets
	typedef std::map<string, string, stl::less_stricmp<string> > PresetAliases;

public:
	explicit CSubstanceConverter(IResourceCompiler* pRC);
	~CSubstanceConverter();

	// interface IConverter ----------------------------------------------------
	virtual void Release();
	virtual void Init(const ConverterInitContext& context);
	virtual ICompiler* CreateCompiler();
	virtual bool SupportsMultithreading() const;
	virtual const char* GetExt(int index) const;
	virtual bool IsCacheable() const override { return true; }
	// -------------------------------------------------------------------------
	const string GetGameRootPath() const { return m_gameRootPath; }
	const IConfig* GetConfig() const { return m_pConfig; }
	ISubstanceInstanceRenderer* GetRenderer() { return m_pRenderer; }
	const SubstanceAir::InputImage::SPtr& GetInputImage(const ISubstancePreset* preset, const string& path);
protected:
	bool ConvertToTiff(const string& path, const string& target);
private:
	const IConfig* m_pConfig;
	PresetAliases m_presetAliases;
	IResourceCompiler* m_pRC;
	string m_gameRootPath;
	ISubstanceInstanceRenderer* m_pRenderer;
	std::unordered_map<uint32, SubstanceAir::InputImage::SPtr> m_loadedImages;
};
