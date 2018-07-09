// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_Math.h>
#include "PixelFormats.h"     // CPixelFormats
#include "IConfig.h"          // IConfig
#include "ConvertContext.h"   // ConvertContext
#include "ICfgFile.h"         // ICfgFile
#include "IRCLog.h"           // IRCLog
#include "StringHelpers.h"    // Split()
#include <CryString/StringUtils.h>      // cry_strcpy()
#include "Util.h"             // getMax()
#include "PixelFormats.h"     // CPixelFormats
#include "ImagePreview.h"     // EPreviewMode

#include "Converters/NormalFromHeight.h"  // CBumpProperties


class CImageProperties
{
public:
	enum
	{
		NUM_CONTROLLED_MIP_MAPS = 6
	};

	enum EImageCompressor
	{
		eImageCompressor_Tif,             // .TIF file format
		eImageCompressor_Hdr,             // .HDR file format
		eImageCompressor_CTSquish,        // Niels & Squish (squish-ccr, Feb. 2013) - slow to fast, very high quality, supports 3Dc and BC1-5
		eImageCompressor_CTSquishHiQ,     // Niels & Squish (squish-ccr, Jul. 2013) - slow, very high quality, supports 3Dc and BC1-5
		eImageCompressor_CTSquishFast,    // Niels & Squish (squish-ccr, Feb. 2013) - fast to very fast, very high quality, supports 3Dc and BC1-5
	};

	enum EColorModel
	{
		eColorModel_RGB,
		eColorModel_CIE,                  // the color-space terrain textures are stored in
		eColorModel_YCbCr,                // the color-space used to encode specular textures
		eColorModel_YFbFr,                // alternative symmetric and reversible/lossless color-space
		eColorModel_IRB,                  // alternative color-space used to encode specular textures
	};

	enum EInputColorSpace
	{
		eInputColorSpace_Linear,
		eInputColorSpace_Srgb,
	};

	enum EOutputColorSpace
	{
		eOutputColorSpace_Linear,
		eOutputColorSpace_Srgb,
		eOutputColorSpace_Auto,
	};

	struct ReduceItem
	{
		char platformName[PlatformInfo::kMaxNameLength + 1];
		int platformIndex;  // -1 if platform is not in the list of known platforms
		int value;
	};

	typedef std::map<string, ReduceItem> ReduceMap;

	// --------------------------------------------------------------

	CImageProperties() 
		: m_pCC(0)
		, m_AlphaAsBump(true)
		, m_BumpToNormal(false)
	{
		SetToDefault(true);
	}

	void SetToDefault(bool bClearInputImageFlags)
	{
		m_bPreview = true;
		m_ePreviewModeOriginal = ePM_RGB;
		m_ePreviewModeProcessed = ePM_RGB;
		m_bPreviewFiltered = false;
		m_bPreviewTiled = true;

		if (bClearInputImageFlags)
		{
			ClearInputImageFlags();
		}
	}

	void ClearInputImageFlags()
	{
		m_bPreserveAlpha = false;
		m_bPreserveZ = false;
	}

	// Arguments:
	//  pCC - can be 0
	void SetPropsCC(ConvertContext* pCC, bool bDisablePreview)
	{
		m_pCC = pCC;
		m_BumpToNormal.SetCC(pCC);
		m_AlphaAsBump.SetCC(pCC);
		m_bPreview = 
			bDisablePreview
			? false
			: (m_pCC ? m_pCC->config->GetAsBool("preview", true, true) : true);
	}

	void setKeyValue(const char* key, const char* value)
	{
		m_pCC->multiConfig->setKeyValue(eCP_PriorityFile, key, value);
	}

	// ----------------------------------------
	// safe configuration query

	bool GetConfigAsBool(const char* const key, const bool keyIsMissingValue, const bool emptyOrBadValue, const int ePriMask = eCP_PriorityAll) const
	{
		if (!m_pCC || !m_pCC->config)
		{
			return keyIsMissingValue;
		}
		return m_pCC->config->GetAsBool(key, keyIsMissingValue, emptyOrBadValue, ePriMask);
	}

	int GetConfigAsInt(const char* const key, const int keyIsMissingValue, const int emptyOrBadValue, const int ePriMask = eCP_PriorityAll) const
	{
		if (!m_pCC || !m_pCC->config)
		{
			return keyIsMissingValue;
		}
		return m_pCC->config->GetAsInt(key, keyIsMissingValue, emptyOrBadValue, ePriMask);
	}

	float GetConfigAsFloat(const char* const key, const float keyIsMissingValue, const float emptyOrBadValue, const int ePriMask = eCP_PriorityAll) const
	{
		if (!m_pCC || !m_pCC->config)
		{
			return keyIsMissingValue;
		}
		return m_pCC->config->GetAsFloat(key, keyIsMissingValue, emptyOrBadValue, ePriMask);
	}

	string GetConfigAsString(const char* const key, const char* const keyIsMissingValue, const char* const emptyOrBadValue, int ePriMask = eCP_PriorityAll) const
	{
		if (!m_pCC || !m_pCC->config)
		{
			return false;
		}
		return m_pCC->config->GetAsString(key, keyIsMissingValue, emptyOrBadValue, ePriMask);
	}

	// ----------------------------------------

	void SetPreset(const string& presetName)
	{
		setKeyValue("preset", presetName);

		for (int i = 0; i < m_pCC->pRC->GetPlatformCount(); ++i)
		{
			m_pCC->multiConfig->getConfig(i).ClearPriorityUsage(eCP_PriorityPreset);
			if (!presetName.empty())
			{
				const PlatformInfo* const p = m_pCC->pRC->GetPlatformInfo(i);
				const int sectionIndex = m_pCC->pRC->GetIniFile()->FindSection(presetName.c_str());
				m_pCC->pRC->GetIniFile()->CopySectionKeysToConfig(eCP_PriorityPreset, sectionIndex, p->GetCommaSeparatedNames(), &m_pCC->multiConfig->getConfig(i));
			}
		}
	}

	string GetPreset() const
	{
		return m_pCC->config->GetAsString("preset", "", "");
	}

	// ----------------------------------------

	bool GetPowOf2() const
	{
		return m_pCC->config->GetAsBool("powof2", true, true);
	}

	// ----------------------------------------

	bool GetCubemap(int platform = -1) const
	{
		if (platform < 0)
		{
			platform = m_pCC->platform;
		}

		return m_pCC->multiConfig->getConfig(platform).GetAsBool("cm", false, true);
	}

	string GetCubemapGenerateDiffuse(int platform = -1) const
	{
		if (platform < 0)
		{
			platform = m_pCC->platform;
		}

		if (m_pCC->multiConfig->getConfig(platform).GetAsBool("cm_diff", false, true))
		{
			return m_pCC->multiConfig->getConfig(platform).GetAsString("cm_diffpreset", "EnvironmentProbeHDR_Irradiance", "");
		}

		return "";
	}

	// ----------------------------------------

	void SetSupressEngineReduce(const bool bValue)
	{
		if (GetSupressEngineReduce() != bValue)
		{
			setKeyValue("ser", bValue ? "1" : "0");
		}
	}

	// flag propagated to engine to affect loading (reduce texture resolution during loading)
	bool GetSupressEngineReduce(int platform = -1) const
	{
		if (platform < 0)
		{
			platform = m_pCC->platform;
		}
		return m_pCC->multiConfig->getConfig(platform).GetAsBool("ser", false, true);
	}

	// ----------------------------------------

	// flag propagated to engine to define right border clamp behavior on alpha test
	bool GetDecal() const
	{
		return m_pCC->config->GetAsBool("decal", false, true);
	}

	// ----------------------------------------

	float GetBRDFGlossScale() const
	{
		return m_pCC->config->GetAsFloat("brdfglossscale", 10.0f, 10.0f);
	}

	// ----------------------------------------

	float GetBRDFGlossBias() const
	{
		return m_pCC->config->GetAsFloat("brdfglossbias", 1.0f, 1.0f);
	}

	// ----------------------------------------

	bool GetGlossLegacyDistribution() const
	{
		return m_pCC->config->GetAsBool("glosslegacydist", false, false);
	}

	// ----------------------------------------

	bool GetGlossFromNormals() const
	{
		return m_pCC->config->GetAsBool("glossfromnormals", false, false);
	}

	// ----------------------------------------

	void SetMaintainAlphaCoverage(const bool bValue)
	{
		if (GetMaintainAlphaCoverage() != bValue)
		{
			setKeyValue("mc", bValue ? "1" : "0");
		}
	}

	// maintain alpha coverage in mipmaps
	bool GetMaintainAlphaCoverage() const
	{
		return m_pCC->config->GetAsBool("mc", false, true);
	}

	// ----------------------------------------

	void SetMipRenormalize(const bool bValue)
	{
		if (GetMipRenormalize() != bValue)
		{
			setKeyValue("mipnormalize", bValue ? "1" : "0");
		}
	}

	bool GetMipRenormalize() const
	{
		return m_pCC->config->GetAsBool("mipnormalize", false, true);
	}

	// ----------------------------------------

	void SetMipMaps(const bool bValue)
	{
		if (GetMipMaps() != bValue)
		{
			setKeyValue("mipmaps", bValue ? "1" : "0");
		}
	}

	bool GetMipMaps(int platform = -1) const
	{
		if (platform < 0)
		{
			platform = m_pCC->platform;
		}
		const bool bRet = m_pCC->multiConfig->getConfig(platform).GetAsBool("mipmaps", true, true);
		return bRet;
	}

	// ----------------------------------------

	const std::pair<EInputColorSpace, EOutputColorSpace> GetColorSpace() const
	{
		std::pair<EInputColorSpace, EOutputColorSpace> result(eInputColorSpace_Linear, eOutputColorSpace_Linear);

		const char* const defaultInput = "linear";
		const char* const defaultOutput = "linear";
		const string defaultInputOutput = string(defaultInput) + string(",") + string(defaultOutput);

		const string optionValue = m_pCC->config->GetAsString("colorspace", defaultInputOutput, defaultInputOutput);

		std::vector<string> parts;
		StringHelpers::Split(optionValue, ",", false, parts);

		if (parts.size() != 2)
		{
			RCLogError("Bad format of /colorspace value: '%s'. Using '%s'.", optionValue.c_str(), defaultInputOutput.c_str());
			return result;
		}

		if (StringHelpers::EqualsIgnoreCase(parts[0], "linear"))
		{
			result.first = eInputColorSpace_Linear;
		}
		else if (StringHelpers::EqualsIgnoreCase(parts[0], "sRGB"))
		{
			result.first = eInputColorSpace_Srgb;
		}
		else
		{
			RCLogError("Unknown input colorspace '%s' in /colorspace='%s'. Using '%s'.", parts[0].c_str(), optionValue.c_str(), defaultInput);
		}

		if (StringHelpers::EqualsIgnoreCase(parts[1], "linear"))
		{
			result.second = eOutputColorSpace_Linear;
		}
		else if (StringHelpers::EqualsIgnoreCase(parts[1], "sRGB"))
		{
			result.second = eOutputColorSpace_Srgb;
		}
		else if (StringHelpers::EqualsIgnoreCase(parts[1], "auto"))
		{
			result.second = eOutputColorSpace_Auto;
		}
		else
		{
			RCLogError("Unknown output colorspace '%s' in /colorspace='%s'. Using '%s'.", parts[1].c_str(), optionValue.c_str(), defaultOutput);
		}

		return result;
	}

	// ----------------------------------------

	void SetAutoDetectLuminance(const bool bValue)
	{
		if (GetAutoDetectLuminance() != bValue)
		{
			setKeyValue("detectL8", bValue ? "1" : "0");
		}
	}

	bool GetAutoDetectLuminance() const
	{
		const bool bRet = m_pCC->config->GetAsBool("detectL8", false, true);
		return bRet;
	}

	// ----------------------------------------

	void SetAutoDetectBlackAndWhiteAlpha(const bool bValue)
	{
		if (GetAutoDetectBlackAndWhiteAlpha() != bValue)
		{
			setKeyValue("detectA1", bValue ? "1" : "0");
		}
	}

	bool GetAutoDetectBlackAndWhiteAlpha() const
	{
		const bool bRet = m_pCC->config->GetAsBool("detectA1", false, true);
		return bRet;
	}

	// ----------------------------------------

	// Arguments:
	//  iValue - 0=no sharpening .. 100=full sharpening
	void SetMipSharpen( const int iValue )
	{
		if (GetMipSharpen() == iValue)
		{
			return;
		}

		char str[80];	
		cry_sprintf(str, "%d", iValue);
		setKeyValue("ms" ,str);
	}

	static int GetMipSharpenDefault()
	{
		return 35;
	}

	// Returns:
	//  0=no sharpening .. 100=full sharpening
	int GetMipSharpen() const
	{
		return m_pCC->config->GetAsInt("ms", GetMipSharpenDefault(), GetMipSharpenDefault());
	}

	// ----------------------------------------
 
	static int GetMipDownsizingMethodCount();
	static int GetMipDownsizingMethodDefault();

	static const char* GetMipDownsizingMethodName(int idx);

	void SetMipDownsizingMethodIndex(const int idx)
	{
		const int curIdx = GetMipDownsizingMethodIndex();
		if (curIdx == idx)
		{
			return;
		}

		setKeyValue("mipgentype", GetMipDownsizingMethodName(idx));
	}

	const int GetMipDownsizingMethodIndex(int platform = -1) const;

	// ----------------------------------------

	static int GetMipGenerationMethodDefault()
	{
		return GetFilterFunctionDefault();
	}

	static int GetMipGenerationMethodCount()
	{
		return GetFilterFunctionCount();
	}

	static const char* GetMipGenerationMethodName(int idx)
	{
		if (idx < 0 || idx >= GetMipGenerationMethodCount())
		{
			idx = GetMipGenerationMethodDefault();
		}

		return GetFilterFunctionName(idx);
	}

	const int GetMipGenerationMethodIndex(int platform = -1) const
	{
		if (platform < 0)
		{
			platform = m_pCC->platform;
		}

		const int defaultIndex = GetMipGenerationMethodDefault();
		const char* const pDefaultName = GetMipGenerationMethodName(defaultIndex);

		const string name = m_pCC->multiConfig->getConfig(platform).GetAsString("mipgentype", pDefaultName, pDefaultName);

		int index = GetFilterFunctionIndex(name.c_str(), -1, "-1");
		if (index == -1)
		{
			index = defaultIndex;

			RCLogWarning("Unknown filtering window: '%s'. Using '%s'.", name.c_str(), GetFilterFunctionName(defaultIndex));
		}

		return index;
	}

	void SetMipGenerationMethodIndex(const int idx)
	{
		const int curIdx = GetMipGenerationMethodIndex();
		if (curIdx == idx)
		{
			return;
		}

		setKeyValue("mipgentype", GetMipGenerationMethodName(idx));
	}

	// ----------------------------------------

	static int GetMipGenerationEvalDefault()
	{
		return GetFilterEvaluationDefault();
	}

	static int GetMipGenerationEvalCount()
	{
		return GetFilterEvaluationCount();
	}

	static const char* GetMipGenerationEvalName(int idx)
	{
		if (idx < 0 || idx >= GetMipGenerationEvalCount())
		{
			idx = GetMipGenerationEvalDefault();
		}

		return GetFilterEvaluationName(idx);
	}

	const int GetMipGenerationEvalIndex(int platform = -1) const
	{
		if (platform < 0)
		{
			platform = m_pCC->platform;
		}

		const int defaultIndex = GetMipGenerationEvalDefault();
		const char* const pDefaultName = GetMipGenerationEvalName(defaultIndex);

		const string name = m_pCC->multiConfig->getConfig(platform).GetAsString("mipgeneval", pDefaultName, pDefaultName);

		int index = GetFilterEvaluationIndex(name.c_str(), -1);
		if (index == -1)
		{
			index = defaultIndex;

			RCLogWarning("Unknown filtering evaluation: '%s'. Using '%s'.", name.c_str(), GetFilterEvaluationName(defaultIndex));
		}

		return index;
	}

	void SetMipGenerationEvalIndex(const int idx)
	{
		const int curIdx = GetMipGenerationEvalIndex();
		if (curIdx == idx)
		{
			return;
		}

		setKeyValue("mipgeneval", GetMipGenerationEvalName(idx));
	}

	// ----------------------------------------

	void SetMipBlurring(float fValue)
	{
		if (GetMipBlurring() == fValue)
		{
			return;
		}

		char str[80];	
		cry_sprintf(str, "%f", fValue);
		setKeyValue("mipgenblur" ,str);
	}

	void SetMipVerticalBlurring(float fValue)
	{
		if (GetMipVerticalBlurring() == fValue)
		{
			return;
		}

		char str[80];	
		cry_sprintf(str, "%f", fValue);
		setKeyValue("mipgenvblur" ,str);
	}

	void SetMipHorizontalBlurring(float fValue)
	{
		if (GetMipHorizontalBlurring() == fValue)
		{
			return;
		}

		char str[80];	
		cry_sprintf(str, "%f", fValue);
		setKeyValue("mipgenhblur" ,str);
	}

	float GetMipBlurring() const
	{
		const float fRet = m_pCC->config->GetAsFloat("mipgenblur", 0.0f, 0.0f);
		return fRet;
	}

	float GetMipVerticalBlurring() const
	{
		const float fDef = GetMipBlurring();
		const float fRet = m_pCC->config->GetAsFloat("mipgenvblur", fDef, fDef);
		return fRet;
	}

	float GetMipHorizontalBlurring() const
	{
		const float fDef = GetMipBlurring();
		const float fRet = m_pCC->config->GetAsFloat("mipgenhblur", fDef, fDef);
		return fRet;
	}

	// ----------------------------------------
 
	// note: the setting does not become stored in the file
	void OverwriteImageCompressor(const EImageCompressor Value)
	{
		if (Value == eImageCompressor_Tif)
		{
			m_pCC->multiConfig->setKeyValue(eCP_PriorityHighest, "imagecompressor", "Tif");
			return;
		}
		else if (Value == eImageCompressor_Hdr)
		{
			m_pCC->multiConfig->setKeyValue(eCP_PriorityHighest, "imagecompressor", "Hdr");
			return;
		}

		RCLogError("%s: Unexpected value: %d", __FUNCTION__, (int)Value);
		return;
	}

	EImageCompressor GetImageCompressor() const
	{
		const string optionValue = m_pCC->config->GetAsString("imagecompressor", "", "");

		if (!optionValue.empty())
		{
			static const struct
			{
				const char* const name;
				EImageCompressor const compressor;
			} selector[] = 
			{
				{ "Tif",          eImageCompressor_Tif          },
				{ "Hdr",          eImageCompressor_Hdr          },
				{ "CTSquish",     eImageCompressor_CTSquish     },
				{ "CTSquishHiQ",  eImageCompressor_CTSquishHiQ  },
				{ "CTSquishFast", eImageCompressor_CTSquishFast },
			};

			static const int count = sizeof(selector) / sizeof(selector[0]);
			for (size_t i = 0; i < count; ++i)
			{
				if (stricmp(optionValue.c_str(), selector[i].name) == 0)
				{
					return selector[i].compressor;
				}
			}
		}

		const EImageCompressor c = eImageCompressor_CTSquish;

		if (!optionValue.empty())
		{
			const char* const name = "CTsquish";
			RCLogWarning("Unknown /imagecompressor value: '%s'. Using '%s'.", optionValue.c_str(), name);
		}

		return c;
	}

	// ----------------------------------------

	// Returns:
	//   0..255 to limit per pixel pow factor adjustment - prevents NAN in shader
	int GetMinimumAlpha() const
	{
		return m_pCC->config->GetAsInt("minalpha", 0, 0);
	}

	// ----------------------------------------

	bool GetDiscardAlpha(int platform = -1) const
	{
		if (platform < 0)
		{
			platform = m_pCC->platform;
		}
		return m_pCC->multiConfig->getConfig(platform).GetAsBool("discardalpha", false, true);
	}

	// ----------------------------------------

	// Returns:
	//   true - if texture needs range normalization
	bool GetNormalizeRange() const
	{
		const bool bRet = m_pCC->config->GetAsBool("dynscale", false, true);
		return bRet;
	}

	bool GetNormalizeRangeAlpha() const
	{
		const bool bRet = m_pCC->config->GetAsBool("dynscalealpha", GetNormalizeRange(), true);
		return bRet;
	}

	// ----------------------------------------

	// -1 if not used, otherwise 32bit value is used to set the border of the texture to this color
	uint64 GetBorderColor()
	{
		const string sHex = m_pCC->config->GetAsString("mipbordercolor", "", "");

		if (!sHex.empty())
		{
			uint32 dw32BitValue = 0;
			if (sscanf(sHex, "%x", &dw32BitValue) == 1)
			{
				return (int64)dw32BitValue;
			}
		}

		return -1;										// -1 if not used, otherwise 32bit value is used to set the border of the texture to this color
	}

	// ----------------------------------------

	// Returns:
	//  0=no sharpening .. 100=full sharpening
	int GetHighPass() const
	{
		return m_pCC->config->GetAsInt("highpass", 0, 0);
	}

	// ----------------------------------------

	// Returns:
	//   0=RGB, 1=CIE, 2=YCC, 3=YFF, 4=IRB
	EColorModel GetColorModel() const
	{
		int ret = m_pCC->config->GetAsInt("colormodel", eColorModel_RGB, eColorModel_RGB);
		if ((ret != eColorModel_RGB) && (ret != eColorModel_CIE) && (ret != eColorModel_IRB) && (ret != eColorModel_YCbCr) && (ret != eColorModel_YFbFr))
		{
			ret = eColorModel_RGB;
		}

		return EColorModel(ret);
	}

	static Vec3 GetUniformColorWeights()
	{
		return Vec3(0.3333f, 0.3334f, 0.3333f);
	}

	Vec3 GetColorWeights() const
	{
		const string rgbweights = m_pCC->config->GetAsString("rgbweights", "uniform", "uniform");

		if (StringHelpers::EqualsIgnoreCase(rgbweights, "luminance"))
		{
			return Vec3(0.3086f, 0.6094f, 0.0820f);
		}
		else if (StringHelpers::EqualsIgnoreCase(rgbweights, "ciexyz"))
		{
			return Vec3(0.2126f, 0.7152f, 0.0722f);
		}

		return GetUniformColorWeights();
	}

	// ----------------------------------------

	void SetAutoOptimizeFile(const bool bValue)
	{
		setKeyValue("autooptimizefile", bValue ? "1" : "0");
	}

	// ----------------------------------------

	EPixelFormat GetDestPixelFormat(const bool bAlphaChannelUsed, bool tryPreviewFormat, int platform) const
	{
		assert(platform >= 0 && platform < m_pCC->pRC->GetPlatformCount());

		EPixelFormat destPixelFormat = (EPixelFormat)-1;
		string sPixelformat = m_pCC->multiConfig->getConfig(platform).GetAsString("pixelformat", "", "");

		if (!sPixelformat.empty())
		{
			if (tryPreviewFormat)
			{
				const string sPreviewFormat = m_pCC->multiConfig->getConfig(platform).GetAsString("previewformat", "", "");
				if (!sPreviewFormat.empty() && CPixelFormats::FindPixelFormatByName(sPreviewFormat.c_str()) >= 0)
				{
					sPixelformat = sPreviewFormat;
				}
			}

			const EPixelFormat format = CPixelFormats::FindPixelFormatByName(sPixelformat.c_str());	

			if (format < 0)
			{ 
				assert(0);
				RCLogError("%s: pixelformat '%s' not recognized", __FUNCTION__, sPixelformat.c_str());
			}
			else
			{
				destPixelFormat = format;
			}
		}

		const bool bAlpha = bAlphaChannelUsed && !GetDiscardAlpha(platform);

		return CPixelFormats::FindFinalTextureFormat(destPixelFormat, bAlpha);
	}

	void SetDestPixelFormat(const EPixelFormat format, const bool bAlphaChannelUsed)
	{
		if ((GetDestPixelFormat(bAlphaChannelUsed, false, m_pCC->platform) != format) && (format >= 0))
		{
			setKeyValue("pixelformat", CPixelFormats::GetPixelFormatInfo(format)->szName);
		}
	}

	EPixelFormat GetDestAlphaPixelFormat(int platform) const
	{
		assert(platform >= 0 && platform < m_pCC->pRC->GetPlatformCount());

		EPixelFormat destPixelFormat = ePixelFormat_A8;
		const string sPixelformat = m_pCC->multiConfig->getConfig(platform).GetAsString("pixelformatalpha", "", "");

		if (!sPixelformat.empty())
		{
			const EPixelFormat format = CPixelFormats::FindPixelFormatByName(sPixelformat.c_str());	

			if (format < 0)
			{ 
				assert(0);
				RCLogError("%s: pixelformat '%s' not recognized", __FUNCTION__, sPixelformat.c_str());
			}
			else if(format != ePixelFormat_A8 && format != ePixelFormat_3DCp && format != ePixelFormat_EAC_R11 && format != ePixelFormat_BC4 && format != ePixelFormat_BC4s && format != ePixelFormat_DXT1 && format != ePixelFormat_BC1)
			{
				assert(0);
				RCLogError("%s: pixelformat '%s' cannot be used for alpha", __FUNCTION__, sPixelformat.c_str());
			}
			else
			{
				destPixelFormat = format;
			}
		}

		return destPixelFormat;
	}

	EPixelFormat GetDestAlphaPixelFormat() const
	{
		return GetDestAlphaPixelFormat(m_pCC->multiConfig->getActivePlatform());
	}

	// ----------------------------------------

	static int ComputeClampedReduce(int reduce)
	{
		return Util::getClamped(reduce, -2, 5);
	}

	void ConvertReduceMapToString(const ReduceMap& reduce, string& res) const
	{
		res.clear();

		// Try to use "value" format (instead of "platform:value,platform:value") if possible

		const int impossibleValue = -999;
		int value = impossibleValue;
		for (ReduceMap::const_iterator it = reduce.begin(); it != reduce.end(); ++it)
		{
			const ReduceItem& ri = it->second;
			if (ri.platformIndex < 0 ||
				(value != impossibleValue && ri.value != value))
			{
				value = impossibleValue;
				break;
			}
			value = ri.value;
		}

		if (value != impossibleValue)
		{
			res = StringHelpers::Format("%i", value);
			return;
		}

		for (ReduceMap::const_iterator it = reduce.begin(); it != reduce.end(); ++it)
		{
			const ReduceItem& ri = it->second;
			if (!res.empty())
			{
				res.append(",");
			}
			res.append(StringHelpers::Format("%s:%i", ri.platformName, ri.value));
		}
	}

	void ConvertReduceStringToMap(const string& src, ReduceMap& resReduce) const
	{
		resReduce.clear();

		std::vector<string> parts;

		StringHelpers::Split(src, ",", false, parts);
		if (parts.empty())
		{
			parts.push_back("0");
		}

		const bool bExplicitPlatforms = (strchr(src.c_str(), ':') != 0);

		if (bExplicitPlatforms)
		{
			// "platform:value,platform:value..." string format

			int defaultValue = 0;

			// Add platform:value pairs stored in source string

			std::vector<string> nameAndValue;

			for (int i = 0; i < (int)parts.size(); ++i)
			{
				nameAndValue.clear();
				StringHelpers::Split(parts[i], ":", false, nameAndValue);
				if (nameAndValue.size() != 2 ||
					nameAndValue[0].empty() || 
					nameAndValue[1].empty())
				{
					continue;
				}

				nameAndValue[0] = StringHelpers::MakeLowerCase(nameAndValue[0]);

				if (resReduce.find(nameAndValue[0]) == resReduce.end())
				{
					ReduceItem ri;

					ri.platformIndex = m_pCC->pRC->FindPlatform(nameAndValue[0].c_str());

					if (ri.platformIndex >= 0)
					{
						// It's a platform known to RC. Let's use the main name of the platform (it prevents
						// adding the platform multiple times and provides the best name to the user).
						nameAndValue[0] = StringHelpers::MakeLowerCase(m_pCC->pRC->GetPlatformInfo(ri.platformIndex)->GetMainName());
					}
					cry_strcpy(ri.platformName, nameAndValue[0].c_str());

					ri.value = ComputeClampedReduce(::atoi(nameAndValue[1].c_str()));

					// Use value from the first pair as default value (see the next 'for' loop)
					if (resReduce.empty())
					{
						defaultValue = ri.value;
					}

					resReduce[nameAndValue[0]] = ri;
				}
			}

			// Add known platforms (if they weren't added yet)

			const int knownPlatformCount = m_pCC->pRC->GetPlatformCount();

			for (int i = 0; i < knownPlatformCount; ++i)
			{
				const string name = StringHelpers::MakeLowerCase(m_pCC->pRC->GetPlatformInfo(i)->GetMainName());

				if (resReduce.find(name) == resReduce.end())
				{
					ReduceItem ri;
					cry_strcpy(ri.platformName, name.c_str());
					ri.platformIndex = i;
					ri.value = ComputeClampedReduce(defaultValue);

					resReduce[name] = ri;
				}
			}
		}
		else
		{
			// "value" or "value,value" or "value,value,value" or "value,value,value,value" string format
			//
			// The last three formats above are obsolete. Those formats were used to store nameless
			// values for up to four *hardcoded* platforms pc, x360, ps3, wiiu (in this order).
			// We don't write those formats anymore, but we still need to parse them because we
			// have a lot of old files that have strings stored in those formats. The good news are
			// that the platforms x360, ps3, wiiu are not supported by the CryEngine anymore -
			// because of that we use the first value only.

			const int value = ComputeClampedReduce(::atoi(parts[0].c_str()));

			const int knownPlatformCount = m_pCC->pRC->GetPlatformCount();

			for (int i = 0; i < knownPlatformCount; ++i)
			{
				const string name = StringHelpers::MakeLowerCase(m_pCC->pRC->GetPlatformInfo(i)->GetMainName());

				ReduceItem ri;
				cry_strcpy(ri.platformName, name.c_str());
				ri.platformIndex = i;
				ri.value = value;

				resReduce[name] = ri;
			}
		}
	}

	uint32 GetRequestedResolutionReduce(uint32 dwWidth, uint32 dwHeight, int platform = -1) const
	{
		if (platform < 0)
		{
			platform = m_pCC->platform;
		}

		const int globalReduce = m_pCC->multiConfig->getConfig(platform).GetSum("globalreduce");

		int iReduce;
		{
			ReduceMap reduce;
			GetReduceResolutionFile(reduce);

			ReduceMap::const_iterator it = reduce.find(StringHelpers::MakeLowerCase(m_pCC->pRC->GetPlatformInfo(platform)->GetMainName()));
			if (it == reduce.end())
			{
				assert(0);
				iReduce = 0;
			}
			else
			{
				iReduce = it->second.value;
			}
		}

		return (uint32)Util::getMax(0, globalReduce + iReduce);
	}

	void SetReduceResolutionFile(const ReduceMap& reduce)
	{
		string str;
		ConvertReduceMapToString(reduce, str);
		setKeyValue("reduce", str);
	}

	void GetReduceResolutionFile(ReduceMap& reduce) const
	{
		const string s = m_pCC->multiConfig->getConfig(m_pCC->platform).GetAsString("reduce", "0", "0");
		ConvertReduceStringToMap(s, reduce);
	}

	// ----------------------------------------

	int GetReduceAlpha() const
	{
		return m_pCC->config->GetAsInt("reducealpha", 0, 0);
	}

	// ----------------------------------------

	int GetMinTextureSize() const
	{
		return m_pCC->config->GetAsInt("mintexturesize", 0, 0);
	}
	
	int GetMaxTextureSize() const
	{
		return m_pCC->config->GetAsInt("maxtexturesize", 0, 0);
	}

	// ----------------------------------------
	// Arguments:
	//   iMIPAlpha - [0..NUM_CONTROLLED_MIP_MAPS-1] array to return the values in the range 0..100
	void GetMIPAlphaArray(int iMIPAlpha[NUM_CONTROLLED_MIP_MAPS]) const
	{
		std::fill(iMIPAlpha, iMIPAlpha + NUM_CONTROLLED_MIP_MAPS, 50);

		const string sMipControl = m_pCC->config->GetAsString("m", "", "");   // e.g. "50,30,50,20,20"

		if (!sMipControl.empty())
		{
			const char *p = &sMipControl[0];

			for (uint32 dwI = 0; dwI < NUM_CONTROLLED_MIP_MAPS; ++dwI)
			{
				uint32 dwValue=0;

				while (*p>='0' && *p <= '9')          // value
					dwValue = dwValue * 10 + (*p++ - '0');

				if (dwValue <= 100)
					iMIPAlpha[dwI] = dwValue;

				while (*p == ' ')                     // jump over whitespace
					++p;
				if (*p == ',')                        // separator
					++p;
				while (*p == ' ')                     // jump over whitespace
					++p;
				if (*p == 0)                          // end terminator
					break;
			}
		}
	}

	// Arguments:
	//   iId - 0..NUM_CONTROLLED_MIP_MAPS-1
	//   iValue - 0..100
	void SetMIPAlpha(const int iId, const int iValue)
	{
		assert(iId>=0 && iId<NUM_CONTROLLED_MIP_MAPS);

		int iMIPAlpha[NUM_CONTROLLED_MIP_MAPS];
		GetMIPAlphaArray(iMIPAlpha);

		iMIPAlpha[iId] = iValue;

		string mipControlString;	// e.g. "50,30,50,20,20"

		for (int i = 0; i < CImageProperties::NUM_CONTROLLED_MIP_MAPS; ++i)
		{
			if (i != 0)
			{
				mipControlString += ",";
			}

			char str[16];
			cry_sprintf(str, "%d", iMIPAlpha[i]);
			
			mipControlString += str;
		}

		setKeyValue("M", mipControlString.c_str());
	}

	// Arguments:
	//   iMip - 0..n
	// Returns:
	//   -0.5..+0.5
	float ComputeMIPAlphaOffset(const int iMip) const
	{		
		int iMIPAlpha[NUM_CONTROLLED_MIP_MAPS];

		GetMIPAlphaArray(iMIPAlpha);

		float fVal = (float)iMIPAlpha[NUM_CONTROLLED_MIP_MAPS - 1];
		if (iMip / 2 + 1 < NUM_CONTROLLED_MIP_MAPS)
		{
			float fInterpolationSlider1 = (float)iMIPAlpha[iMip / 2];
			float fInterpolationSlider2 = (float)iMIPAlpha[iMip / 2 + 1];
			fVal = fInterpolationSlider1 + (fInterpolationSlider2 - fInterpolationSlider1) * (iMip & 1) * 0.5f;
		}

		return 0.5f - fVal / 100.0f;
	}

	// -----------------------------------------------

	// cubemap filter type:
	//	DISC             0
	//	CONE             1
	//	COSINE           2
	//	ANGULAR_GAUSSIAN 3
	//	COSINE_POWER     4
	uint32 GetCubemapFilterType() const
	{
		const string sTypeName = m_pCC->config->GetAsString("cm_ftype", "cosine", "cosine");

		uint32 type;
		if (_stricmp(sTypeName.c_str(), "disc") == 0)
		{
			type = 0;
		}
		else if (_stricmp(sTypeName.c_str(), "cone") == 0)
		{
			type = 1;
		}
		else if (_stricmp(sTypeName.c_str(), "cosine") == 0)
		{
			type = 2;
		}
		else if (_stricmp(sTypeName.c_str(), "gaussian") == 0)
		{
			type = 3;
		}
		else if (_stricmp(sTypeName.c_str(), "cosine_power") == 0)
		{
			type = 4;
		}
		else if (_stricmp(sTypeName.c_str(), "ggx") == 0)
		{
			type = 5;
		}
		else
		{
			assert(0);
			type = 2;
		}
		return type;
	}

	// cubemap filter angle in degrees	
	float GetCubemapFilterAngle() const
	{
		return Util::getClamped(m_pCC->config->GetAsFloat("cm_fangle", 0.0f, 0.0f), 0.0f, 359.0f); 
	}

	// cubemap initial mip filter angle in degrees	
	float GetCubemapMipFilterAngle() const
	{
		return Util::getClamped(m_pCC->config->GetAsFloat("cm_fmipangle", 0.0f, 0.0f), 0.0f, 359.0f); 
	}

	// cubemap mip filter angle slope 
	float GetCubemapMipFilterSlope() const
	{
		return Util::getClamped(m_pCC->config->GetAsFloat("cm_fmipslope", 0.0f, 0.0f), 0.0f, 3.0f);
	}

	// cubemap edge fixup width in texels 
	int GetCubemapEdgeFixupWidth() const
	{
		return Util::getClamped(m_pCC->config->GetAsInt("cm_edgefixup", 0, 0), 0, 64);
	}

	int GetCubemapGGXSampleCount() const
	{
		return Util::getClamped(m_pCC->config->GetAsInt("cm_ggxsamplecount", 128, 128), 16, 16384);
	}

	// ----------------------------------------
	int GetRGBKCompression() const
	{
		return Util::getClamped(m_pCC->config->GetAsInt("rgbk", 0, 0), 0, 3);
	}

	static inline int GetRGBKMaxValue()
	{
		return 16;
	}
	
	// ----------------------------------------

	static const char* GetDefaultProperty(int index)
	{
		// Implementation notes:
		// 1) The key/value pairs below will be removed from CryTIF strings stored in files, so
		// please avoid adding "preset=..." or similar critical keys to the list.
		// 2) Don't forget to modify the list in case of changing default values.
		static const char* const keysValues[] =
		{
			"bumpblur=0",
			"bumpblur_a=0",
			"bumpstrength=5",
			"bumpstrength_a=5",
			"bumptype=0",
			"bumptype_a=0",
			"m=50,50,50,50,50,50",
			"mc=0",
			"mipnormalize=0",
			"mipmaps=1",
			"ms=35",
		};
		if (index >= 0 && index < sizeof(keysValues) / sizeof(keysValues[0]))
		{
			return keysValues[index];
		}
		return 0;
	}

	// ----------------------------------------
	const PlatformInfo* GetPlatformInfo() const
	{
		return m_pCC->pRC->GetPlatformInfo(m_pCC->platform);
	}

	const IConfig& GetMultiConfig() const
	{
		return m_pCC->multiConfig->getConfig();
	}

	int GetVerbosityLevel() const
	{
		return m_pCC->pRC->GetVerbosityLevel();
	}

public: // ---------------------------------------------------------------------

	CBumpProperties m_BumpToNormal;
	CBumpProperties m_AlphaAsBump;
	
	// cannot be adjusted from outside (through RC key value pairs)

	bool            m_bPreserveAlpha;         // move alpha to attachment if it would get lost
	bool            m_bPreserveZ;             // disallow 3Dc/BC5

	// preview properties
	bool            m_bPreview;
	EPreviewMode    m_ePreviewModeOriginal;   // e.g. 0:RGB, 1:AAA replicate the alpha channel as greyscale value
	EPreviewMode    m_ePreviewModeProcessed;  // e.g. 0:RGB, 1:AAA replicate the alpha channel as greyscale value
	bool            m_bPreviewFiltered;       // activate the bilinear filter in the preview
	bool            m_bPreviewTiled;

private: // ---------------------------------------------------------------------

	ConvertContext* m_pCC;                    // can be 0
};
