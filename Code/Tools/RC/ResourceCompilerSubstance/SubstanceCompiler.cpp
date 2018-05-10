// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include "IRCLog.h"                         // IRCLog
#include "IConfig.h"                        // IConfig
#include "IAssetManager.h"

#include "SubstanceCompiler.h"             
#include "SubstanceConverter.h"
#include <commctrl.h>                       // TCITEM

#include <CryMath/Cry_Math.h>                       // Vec3
#include <CryMath/Cry_Color.h>                      // ColorF
#include "IResCompiler.h"
#include <CryCore/CryEndian.h>                         // Endian converting (for ImageExtensionHelper)
#include <CryRenderer/ITexture.h>                       // eTEX enum
           // GetBoolParam()
#include <CryCore/Containers/CryPtrArray.h>                    // auto_ptr
#include "Util.h"                           // getMin(), getMax()
#include "UpToDateFileHelpers.h"
#include <CryString/CryPath.h>                        // PathUtil::

#include "SuffixUtil.h"                     // SuffixUtil
#include <Cry3DEngine/ImageExtensionHelper.h>         // CImageExtensionHelper

#include "FileUtil.h"                       // GetFileSize()

#include <CryString/CryPath.h>
#include "StringHelpers.h"

#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>

#include <iterator>

#include "ISubstanceManager.h"
#include "ISubstanceInstanceRenderer.h"

// checks if preset exists, reports warning or error if doesn't
bool CSubstanceCompiler::CheckForExistingPreset(const ConvertContext& CC, const string& presetName, bool errorInsteadOfWarning)
{
	if (CC.pRC->GetIniFile()->FindSection(presetName.c_str()) < 0)
	{
		if (errorInsteadOfWarning)
		{
			RCLogError("Preset '%s' doesn't exist in rc.ini. Please contact a lead technical artist.", presetName.c_str());
		}
		else
		{
			RCLogWarning("Preset '%s' doesn't exist in rc.ini.", presetName.c_str());
		}
		return false;
	}
	return true;
}

// constructor
CSubstanceCompiler::CSubstanceCompiler(CSubstanceConverter* converter)
	: m_pConverter(converter)
{
}

// destructor
CSubstanceCompiler::~CSubstanceCompiler()
{
}

void CSubstanceCompiler::Release()
{
	delete this;
}


//#include <crtdbg.h>		// to find memory leaks

void CSubstanceCompiler::BeginProcessing(const IConfig* config)
{
}

void CSubstanceCompiler::EndProcessing()
{
}


static string AutoselectPreset(const ConvertContext& CC, const uint32 width, const uint32 height, const bool hasAlpha)
{
	const char* const defaultColorchart = "ColorChart";
	const char* const defaultBump       = "NormalsFromDisplacement";
	const char* const defaultNormalmap  = "Normals";
	const char* const defaultNormalmapGloss  = "NormalsWithSmoothness";
	const char* const defaultReflectance = "Reflectance";
	const char* const defaultCubemap    = "EnvironmentProbeHDR";
	const char* const defaultHDRCubemap = "EnvironmentProbeHDR";
	const char* const defaultPow2       = "Albedo";
	const char* const defaultPow2Alpha  = "AlbedoWithGenericAlpha";
	const char* const defaultNonpow2    = "ReferenceImage";

	const string fileName = CC.config->GetAsString("overwritefilename", CC.sourceFileNameOnly, CC.sourceFileNameOnly);

	string presetName;

	// TODO: make auto-preset assignment configurable in the rc.ini
	if (SuffixUtil::HasSuffix(fileName.c_str(), '_', "cch"))
	{
		presetName = defaultColorchart;
	}
	else if (SuffixUtil::HasSuffix(fileName.c_str(), '_', "bump"))
	{
		presetName = defaultBump;
	}
	else if (SuffixUtil::HasSuffix(fileName.c_str(), '_', "spec"))
	{
		presetName = defaultReflectance;
	}
	else if (SuffixUtil::HasSuffix(fileName.c_str(), '_', "ddn"))
	{
		presetName = defaultNormalmap;
	}
	else if (SuffixUtil::HasSuffix(fileName.c_str(), '_', "ddna"))
	{
		presetName = defaultNormalmapGloss;
	}
	else if (Util::isPowerOfTwo(width) && Util::isPowerOfTwo(height))
	{
		presetName = hasAlpha ? defaultPow2Alpha : defaultPow2;
	}
	else
	{
		presetName = defaultNonpow2;
	}

	return presetName;
}


bool CSubstanceCompiler::ProcessImplementation()
{
	const string sourceFile = m_CC.GetSourcePath();
	if (m_CC.pRC->GetVerbosityLevel() > 0)
	{
		RCLog("Processing substance preset: %s", sourceFile.c_str());
	}

	ISubstancePreset* pCurrentPreset = ISubstancePreset::Load(sourceFile);
	if (!pCurrentPreset || pCurrentPreset->GetSubstanceArchive().empty())
	{
		RCLogError("Can't load substance preset: %s", sourceFile.c_str());
		return false;
	}

	ISubstanceManager::Instance()->GenerateOutputs(pCurrentPreset, m_pConverter->GetRenderer());
	return true;
}

bool CSubstanceCompiler::Process()
{
	if (m_CC.config->GetAsBool("skipmissing", false, true) && !FileUtil::FileExists(m_CC.GetSourcePath()))
	{
		// If source file does not exist, ignore it.
		return true;
	}
	
	if (!m_CC.bForceRecompiling &&
	    UpToDateFileHelpers::FileExistsAndUpToDate(GetOutputPath(), m_CC.GetSourcePath()))
	{
		// The file is up-to-date
		m_CC.pRC->AddInputOutputFilePair(m_CC.GetSourcePath(), GetOutputPath());
		return true;
	}

	const bool bSuccess = ProcessImplementation();

	return bSuccess;
}

string CSubstanceCompiler::GetOutputFileNameOnly() const
{
	const string sourceFileFinal = m_CC.config->GetAsString("overwritefilename", m_CC.sourceFileNameOnly.c_str(), m_CC.sourceFileNameOnly.c_str());

	const bool bUseTiffExtension = 
		StringHelpers::EqualsIgnoreCase(PathUtil::GetExt(m_CC.sourceFileNameOnly), "tif") &&
		(m_CC.config->GetAsBool("cleansettings", false, true) || 
		m_CC.config->HasKey("savesettings") || 
		m_CC.config->HasKey("savepreset"));
	const char* szExtension = "tif";

	return PathUtil::ReplaceExtension(sourceFileFinal, szExtension);
}

string CSubstanceCompiler::GetOutputPath() const
{
	return PathUtil::Make(m_CC.GetOutputFolder(), GetOutputFileNameOnly());
}