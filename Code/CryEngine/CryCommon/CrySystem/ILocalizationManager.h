// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMemory/CrySizer.h>
//#include <platform.h> // Needed for LARGE_INTEGER (for consoles).

class XmlNodeRef;

// Localized strings manager interface.

//! \cond INTERNAL
//! Localization Info structure.
struct SLocalizedInfoGame
{

	SLocalizedInfoGame()
		: szCharacterName(NULL)
		, bUseSubtitle(false)
	{

	}

	const char* szCharacterName;
	string      sUtf8TranslatedText;

	bool        bUseSubtitle;
};

struct SLocalizedAdvancesSoundEntry
{
	string sName;
	float  fValue;
	void   GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(sName);
	}
};

//! Localization Sound Info structure, containing sound related parameters.
struct SLocalizedSoundInfoGame : public SLocalizedInfoGame
{
	SLocalizedSoundInfoGame()
		: sSoundEvent(NULL)
		, fVolume(0.f)
		, fRadioRatio(0.f)
		, bIsDirectRadio(false)
		, bIsIntercepted(false)
		, nNumSoundMoods(0)
		, pSoundMoods(NULL)
		, nNumEventParameters(0)
		, pEventParameters(NULL)
	{

	}

	const char* sSoundEvent;
	float       fVolume;
	float       fRadioRatio;
	bool        bIsDirectRadio;
	bool        bIsIntercepted;

	// SoundMoods.
	int                           nNumSoundMoods;
	SLocalizedAdvancesSoundEntry* pSoundMoods;

	// EventParameters.
	int                           nNumEventParameters;
	SLocalizedAdvancesSoundEntry* pEventParameters;
};

//! Localization Sound Info structure, containing sound related parameters.
struct SLocalizedInfoEditor : public SLocalizedInfoGame
{
	SLocalizedInfoEditor()
		: sKey(NULL)
		, sOriginalCharacterName(NULL)
		, sOriginalActorLine(NULL)
		, sUtf8TranslatedActorLine(NULL)
		, nRow(0)
	{

	}

	const char*  sKey;
	const char*  sOriginalCharacterName;
	const char*  sOriginalActorLine;
	const char*  sUtf8TranslatedActorLine;
	unsigned int nRow;
};

struct ILocalizationPostProcessor
{
	virtual ~ILocalizationPostProcessor() {}
	virtual void PostProcessString(string& inout) const = 0;
};
//! \endcond

//! Interface to the Localization Manager.
struct ILocalizationManager
{
	//! Platform independent language IDs. Used to map the platform specific language codes to localization pakfiles.
	//! Please ensure that each entry in this enum has a corresponding entry in the PLATFORM_INDEPENDENT_LANGUAGE_NAMES array which is defined in LocalizedStringManager.cpp currently.
	enum EPlatformIndependentLanguageID
	{
		ePILID_Japanese,
		ePILID_English,
		ePILID_French,
		ePILID_Spanish,
		ePILID_German,
		ePILID_Italian,
		ePILID_Dutch,
		ePILID_Portuguese,
		ePILID_Russian,
		ePILID_Korean,
		ePILID_ChineseT,        //!< Traditional Chinese.
		ePILID_ChineseS,        //!< Simplified Chinese.
		ePILID_Finnish,
		ePILID_Swedish,
		ePILID_Danish,
		ePILID_Norwegian,
		ePILID_Polish,
		ePILID_Arabic,          //!< Test value for PS3. Not currently supported by Sony on the PS3 as a system language.
		ePILID_Czech,
		ePILID_Turkish,
		ePILID_MAX_OR_INVALID   //!< Not a language, denotes the maximum number of languages or an unknown language.
	};

	typedef uint32 TLocalizationBitfield;

	// <interfuscator:shuffle>
	virtual ~ILocalizationManager(){}
	virtual const char*                                 LangNameFromPILID(const ILocalizationManager::EPlatformIndependentLanguageID id) = 0;
	virtual ILocalizationManager::TLocalizationBitfield MaskSystemLanguagesFromSupportedLocalizations(const ILocalizationManager::TLocalizationBitfield systemLanguages) = 0;
	virtual ILocalizationManager::TLocalizationBitfield IsLanguageSupported(const ILocalizationManager::EPlatformIndependentLanguageID id) = 0;
	virtual bool        SetLanguage(const char* sLanguage) = 0;
	virtual const char* GetLanguage() = 0;

	//! Load the descriptor file with tag information.
	virtual bool InitLocalizationData(const char* sFileName, bool bReload = false) = 0;

	//! Request to load loca data by tag. Actual loading will happen during next level load begin event.
	virtual bool RequestLoadLocalizationDataByTag(const char* sTag) = 0;

	//! Direct load of loca data by tag.
	virtual bool LoadLocalizationDataByTag(const char* sTag, bool bReload = false) = 0;
	virtual bool ReleaseLocalizationDataByTag(const char* sTag) = 0;

	virtual void RegisterPostProcessor(ILocalizationPostProcessor* pPostProcessor) = 0;
	virtual void UnregisterPostProcessor(ILocalizationPostProcessor* pPostProcessor) = 0;

	virtual bool LoadExcelXmlSpreadsheet(const char* sFileName, bool bReload = false) = 0;
	virtual void ReloadData() = 0;

	//! Free localization data.
	virtual void FreeData() = 0;

	//! Translate a string into the currently selected language.
	//! Processes the input string and translates all labels contained into the currently selected language.
	//! \param[in]  sString             String to be translated.
	//! \param[out] outLocalizedString  Translated version of the string.
	//! \param[in]  bEnglish            If true, translates the string into the always present English language.
	//! \return true if localization was successful, false otherwise.
	virtual bool LocalizeString(const char* sString, string& outLocalizedString, bool bEnglish = false) = 0;

	//! Same as LocalizeString( const char* sString, string& outLocalizedString, bool bEnglish=false ) but at the moment this is faster.
	virtual bool LocalizeString(const string& sString, string& outLocalizedString, bool bEnglish = false) = 0;

	//! Provides the localized version corresponding to a label.
	//! A label has to start with '@' sign.
	//! \param[in] sLabel             Label to be translated, must start with '@' sign.
	//! \param[out]outLocalizedString Localized version of the label.
	//! \param[in] bEnglish           If true, returns the always present English version of the label.
	//! \return true if localization was successful, false otherwise.
	virtual bool LocalizeLabel(const char* sLabel, string& outLocalizedString, bool bEnglish = false) = 0;

	//! Get localization info structure corresponding to a key (key=label without the '@' sign).
	//! \param[in] sKey         Key to be looked up. Key = Label without '@' sign.
	//! \param[out] outGameInfo Reference to localization info structure to be filled in.
	//! \return true if info for key was found, false otherwise.
	virtual bool GetLocalizedInfoByKey(const char* sKey, SLocalizedInfoGame& outGameInfo) = 0;

	//! Get the sound localization info structure corresponding to a key.
	//! \param[in] sKey          Key to be looked up. Key = Label without '@' sign.
	//! \param[out] outSoundInfo Reference to sound info structure to be filled in.
	//! outSoundInfo.pSoundMoods requires nNumSoundMoods-times allocated memory.
	//! On return outSoundInfo.nNumSoundMoods will hold how many SoundsMood entries are needed.
	//! On return outSoundInfo.nNumEventParameters will hold how many EventParameter entries are needed.
	//! outSoundInfo.pEventParameters requires nNumEventParameters-times allocated memory.
	//! Passing 0 in either the Num fields will make the query ignore checking for allocated memory.
	//! \return true if successful, false otherwise (key not found, or not enough memory provided to write additional info).
	virtual bool GetLocalizedInfoByKey(const char* sKey, SLocalizedSoundInfoGame* pOutSoundInfo) = 0;

	//! Return number of localization entries.
	virtual int GetLocalizedStringCount() = 0;

	//! Get the localization info structure at index nIndex.
	//! \param[in] nIndex Index.
	//! \param[out] outEditorInfo Reference to localization info structure to be filled in.
	//! \return true if successful, false otherwise (out of bounds).
	virtual bool GetLocalizedInfoByIndex(int nIndex, SLocalizedInfoEditor& outEditorInfo) = 0;

	//! Get the localization info structure at index nIndex.
	//! \param[in] nIndex Index.
	//! \param[out] outGameInfo Reference to localization info structure to be filled in.
	//! \return true if successful, false otherwise (out of bounds).
	virtual bool GetLocalizedInfoByIndex(int nIndex, SLocalizedInfoGame& outGameInfo) = 0;

	//! Get the english localization info structure corresponding to a key.
	//! \param[in] sKey Key to be looked up. Key = Label without '@' sign.
	//! \param[out] sLocalizedString Corresponding english language string.
	//! \return true if successful, false otherwise (key not found).
	virtual bool GetEnglishString(const char* sKey, string& sLocalizedString) = 0;

	//! Get Subtitle for Key or Label.
	//! \param[in] sKeyOrLabel    Key or Label to be used for subtitle lookup. Key = Label without '@' sign.
	//! \param[out] outSubtitle   Subtitle (untouched if Key/Label not found).
	//! \param[in] bForceSubtitle If true, get subtitle (sLocalized or sEnglish) even if not specified in Data file.
	//! \return true if subtitle found (and outSubtitle filled in), false otherwise.
	virtual bool GetSubtitle(const char* sKeyOrLabel, string& outSubtitle, bool bForceSubtitle = false) = 0;

	//! These methods format outString depending on sString with ordered arguments.
	//! FormatStringMessage(outString, "This is %2 and this is %1", "second", "first");
	//! \param[out] outString This is first and this is second.
	virtual void FormatStringMessage(string& outString, const string& sString, const char** sParams, int nParams) = 0;
	virtual void FormatStringMessage(string& outString, const string& sString, const char* param1, const char* param2 = 0, const char* param3 = 0, const char* param4 = 0) = 0;

	virtual void LocalizeTime(time_t t, bool bMakeLocalTime, bool bShowSeconds, string& outTimeString) = 0;
	virtual void LocalizeDate(time_t t, bool bMakeLocalTime, bool bShort, bool bIncludeWeekday, string& outDateString) = 0;
	virtual void LocalizeDuration(int seconds, string& outDurationString) = 0;
	virtual void LocalizeNumber(int number, string& outNumberString) = 0;
	virtual void LocalizeNumber(float number, int decimals, string& outNumberString) = 0;
	// </interfuscator:shuffle>

	static ILINE TLocalizationBitfield LocalizationBitfieldFromPILID(EPlatformIndependentLanguageID pilid)
	{
		assert(pilid >= 0 && pilid < ePILID_MAX_OR_INVALID);
		return (1 << pilid);
	}
};
