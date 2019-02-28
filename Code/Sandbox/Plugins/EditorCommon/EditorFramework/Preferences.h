// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.


// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"
#include "AutoRegister.h"
#include "QtViewPane.h"

#include "PreferencesDialog.h"

#include <CrySerialization/Enum.h>
#include <CryString/CryString.h>
#include <CrySandbox/CrySignal.h>

class CPreferences;

typedef CAutoRegister<CPreferences> CAutoRegisterPreferencesHelper;

#define REGISTER_PREFERENCES_PAGE(Type)                                                               \
  namespace Internal                                                                                  \
  {                                                                                                   \
  void RegisterPreferencesPage ## Type()                                                              \
  {                                                                                                   \
    GetIEditor()->GetPreferences()->RegisterPage<Type>();                                             \
  }                                                                                                   \
  CAutoRegisterPreferencesHelper g_AutoRegPreferencesHelper ## Type(RegisterPreferencesPage ## Type); \
  }

#define REGISTER_PREFERENCES_PAGE_PTR(Type, TypePtr)                                                  \
  namespace Internal                                                                                  \
  {                                                                                                   \
  void RegisterPreferencesPage ## Type()                                                              \
  {                                                                                                   \
    GetIEditor()->GetPreferences()->RegisterPage<Type>(TypePtr);                                      \
  }                                                                                                   \
  CAutoRegisterPreferencesHelper g_AutoRegPreferencesHelper ## Type(RegisterPreferencesPage ## Type); \
  }

#define ADD_PREFERENCE_PAGE_PROPERTY(type, accessor, mutator) \
  public:                                                     \
    void mutator(const type val)                              \
    {                                                         \
      if (m_ ## accessor != val)                              \
      {                                                       \
        m_ ## accessor = val;                                 \
        accessor ## Changed();                                \
        signalSettingsChanged();                              \
      }                                                       \
    }                                                         \
    const type& accessor() const { return m_ ## accessor; }   \
    CCrySignal<void()> accessor ## Changed;                   \
  private:                                                    \
    type m_ ## accessor;                                      \

struct SPreferencePage;

struct EDITOR_COMMON_API SPreferencePage
{
	SPreferencePage(const char* name, const char* path)
		: m_name(name)
		, m_path(path)
	{
	}

	virtual ~SPreferencePage() {}

	const string& GetName() const     { return m_name; }
	const string& GetPath() const     { return m_path; }
	string        GetFullPath() const { return m_path + "/" + m_name; }

	virtual bool  Serialize(yasli::Archive& ar)
	{
		return true;
	}

	void operator=(const SPreferencePage& other)
	{
		if (this != &other)
		{
			m_name = other.m_name;
			m_path = other.m_path;
		}
	}

private:
	// private empty constructor to make sure we have a valid name & path
	SPreferencePage() {}

	friend class CPreferences;
	void SetName(const string& name) { m_name = name; }
	void SetPath(const string& path) { m_path = path; }

	QString GetSerializedProperties();
	void FromSerializedProperties(const QByteArray& jsonBlob);

public:
	CCrySignal<void()> signalSettingsChanged;

private:
	CCrySignal<void()> signalRequestReset;

	string m_name;
	string m_path;
};

class EDITOR_COMMON_API CPreferences
{
	friend yasli::Serializer;
public:
	CPreferences();
	~CPreferences();

	// Need to call init after all preferences have been registered
	void Init();

	// Tray area controls the lifetime of the widget
	template<typename Type>
	void RegisterPage()
	{
		Type* pPreferencePage = new Type();
		pPreferencePage->signalSettingsChanged.Connect(std::function<void()>([this]() { signalSettingsChanged(); }));
		pPreferencePage->signalRequestReset.Connect(std::function<void()>([pPreferencePage]()
		{
			*pPreferencePage =  Type();
			pPreferencePage->signalSettingsChanged();
		}));

		AddPage(pPreferencePage);
	}

	template<typename Type>
	void RegisterPage(Type* pPreferencePage)
	{
		pPreferencePage->signalSettingsChanged.Connect(std::function<void()>([this]() { signalSettingsChanged(); }));
		pPreferencePage->signalRequestReset.Connect(std::function<void()>([pPreferencePage]() {
			*pPreferencePage = Type();
			pPreferencePage->signalSettingsChanged();
		}));

		AddPage(pPreferencePage);
	}

	std::vector<SPreferencePage*> GetPages(const char* path);
	QWidget*                      GetPageWidget(const char* path)
	{
		return new QPreferencePage(GetPages(path), path);
	}

	const std::map<string, std::vector<SPreferencePage*>>& GetPages() const { return m_preferences; }

	bool         IsLoading() const { return m_bIsLoading; }
	void         Reset(const char* path);
	void         Save();

private:
	void         Load();
	void         Load(const QString& path);
	void         AddPage(SPreferencePage* pPreferencePage);

	virtual bool Serialize(yasli::Archive& ar);

public:
	CCrySignal<void()> signalSettingsChanged;
	CCrySignal<void()> signalSettingsReset;

private:
	std::map<string, std::vector<SPreferencePage*>> m_preferences;
	bool m_bIsLoading;
};

ILINE bool Serialize(yasli::Archive& ar, SPreferencePage* val, const char* name, const char* label)
{
	if (!val)
		return false;

	return ar(*val, name ? name : val->GetFullPath(), label ? label : val->GetName());
}

