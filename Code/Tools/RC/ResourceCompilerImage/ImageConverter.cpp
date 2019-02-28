// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "IRCLog.h"
#include "ICfgFile.h"
#include "ImageConverter.h"
#include "ImageCompiler.h"
#include "IAssetManager.h"
#include "ImageDetails.h"

#include "../../../SDKs/tiff-4.0.4/libtiff/tiffio.h"

static void TiffWarningHandler(const char* module, const char* fmt, va_list args)
{
	// unused tags, etc
	char str[2 * 1024];
	cry_vsprintf(str, fmt, args);
	RCLogWarning("LibTIFF: %s", str);
}

static void TiffErrorHandler(const char* module, const char* fmt, va_list args)
{
	char str[2 * 1024];
	cry_vsprintf(str, fmt, args);
	RCLogError("LibTIFF: %s", str);
}

static void LoadPresetAliases(
	const ICfgFile* const pIni,
	CImageConverter::PresetAliases& presetAliases)
{
	presetAliases.clear();

	const char* const szPresetAliasesSectionName = "_presetAliases";

	// Load preset map entries
	if (pIni->FindSection(szPresetAliasesSectionName) >= 0)
	{
		class CMappingFiller
			: public IConfigSink
		{
		public:
			CMappingFiller(CImageConverter::PresetAliases& presetAliases, const char* const szPresetAliasesSectionName)
				: m_presetAliases(presetAliases)
				, m_szPresetAliasesSectionName(szPresetAliasesSectionName)
				, m_bFailed(false)
			{
			}

			virtual void SetKeyValue(EConfigPriority ePri, const char* szKey, const char* szValue) override
			{
				const string key(szKey);
				if (m_presetAliases.find(key) != m_presetAliases.end())
				{
					RCLogError("Preset name '%s' specified *multiple* times in section '%s' of rc.ini", szKey, m_szPresetAliasesSectionName);
				}
				m_presetAliases[key] = string(szValue);
			}

			CImageConverter::PresetAliases& m_presetAliases;
			const char* const m_szPresetAliasesSectionName;
			bool m_bFailed;
		};

		CMappingFiller filler(presetAliases, szPresetAliasesSectionName);

		pIni->CopySectionKeysToConfig(eCP_PriorityRcIni, pIni->FindSection(szPresetAliasesSectionName), "", &filler);

		if (filler.m_bFailed)
		{
			presetAliases.clear();
			return;
		}
	}

	// Check that preset names actually exist in rc.ini
	for (CImageConverter::PresetAliases::iterator it = presetAliases.begin(); it != presetAliases.end(); ++it)
	{
		const string& presetName = it->second;
		if (pIni->FindSection(presetName.c_str()) < 0)
		{
			RCLogError("Preset '%s' specified in section '%s' of rc.ini doesn't exist", presetName.c_str(), szPresetAliasesSectionName);
			presetAliases.clear();
			return;
		}
	}
}

CImageConverter::CImageConverter(IResourceCompiler* pRC)
{
	LoadPresetAliases(pRC->GetIniFile(), m_presetAliases);
	pRC->GetAssetManager()->RegisterDetailProvider(AssetManager::CollectDDSImageDetails, "dds");
}

CImageConverter::~CImageConverter()
{
}

void CImageConverter::Release()
{
	delete this;
}

void CImageConverter::Init(const ConverterInitContext& context)
{
	TIFFSetErrorHandler(TiffErrorHandler);
	TIFFSetWarningHandler(TiffWarningHandler);
}

ICompiler* CImageConverter::CreateCompiler()
{
	return new CImageCompiler(m_presetAliases);
}

bool CImageConverter::SupportsMultithreading() const
{
	return true;
}

const char* CImageConverter::GetExt(int index) const
{
	if (index == 0)
	{
		return "tif";
	}
	else if (index == 1)
	{
		return "hdr";
	}

	return NULL;
}
