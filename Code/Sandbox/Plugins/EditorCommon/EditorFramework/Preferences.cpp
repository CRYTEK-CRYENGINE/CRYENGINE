// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "Preferences.h"
#include "QtUtil.h"

#include <CryString/CryPath.h>
#include <CrySystem/IProjectManager.h>

#include <CrySerialization/yasli/JSONOArchive.h>
#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/STL.h>

#include <QJsonDocument>
#include <QFile>

static const char* s_defaultPath = "Editor/Preferences";
static const char* s_fileSuffix = "pref";

QString SPreferencePage::GetSerializedProperties()
{
	yasli::JSONOArchive ar;
	ar(*this, m_name, m_name);
	return ar.buffer();
}

void SPreferencePage::FromSerializedProperties(const QByteArray& jsonBlob)
{
	yasli::JSONIArchive ar;
	ar.open(jsonBlob, jsonBlob.length());
	ar(*this, m_name, m_name);
}

CPreferences::CPreferences()
	: m_bIsLoading(false)
{
	signalSettingsChanged.Connect(this, &CPreferences::Save);
}

CPreferences::~CPreferences()
{
	signalSettingsChanged.DisconnectObject(this);
	Save();
	for (auto ite = m_preferences.begin(); ite != m_preferences.end(); ++ite)
	{
		std::vector<SPreferencePage*> m_preferencePages = ite->second;
		for (auto i = 0; i < m_preferencePages.size(); ++i)
			delete m_preferencePages[i];
	}
}

void CPreferences::Init()
{
	Load();
}

void CPreferences::Load()
{
	m_bIsLoading = true;

	QString path = QtUtil::GetAppDataFolder();
	path += "/Preferences.pref";

	QFile file(path);

	if (file.exists())
		return Load(path);

	QString projectPreferences(GetIEditor()->GetProjectManager()->GetCurrentProjectDirectoryAbsolute());
	projectPreferences = projectPreferences + "/Editor/Preferences.pref";
	Load(projectPreferences);
}

void CPreferences::Load(const QString& path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
	{
		QString msg = "Failed to open path: " + path;
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_COMMENT, msg.toLocal8Bit());
		m_bIsLoading = false;
		return;
	}

	QJsonDocument doc(QJsonDocument::fromJson(file.readAll()));
	QVariantMap preferenceMap = doc.toVariant().toMap();

	for (auto variantIte = preferenceMap.constBegin(); variantIte != preferenceMap.constEnd(); ++variantIte)
	{
		const QString& path = variantIte.key();
		auto ite = m_preferences.find(QtUtil::ToString(path));

		if (ite == m_preferences.end())
			continue;

		QVariantMap pagesMap = preferenceMap[path].toMap();
		std::vector<SPreferencePage*> pages = ite->second;

		for (auto pageIte = pagesMap.constBegin(); pageIte != pagesMap.constEnd(); ++pageIte)
		{
			for (auto i = 0; i < pages.size(); ++i)
			{
				if (pageIte.key() == QtUtil::ToQString(pages[i]->GetName()))
				{
					QJsonDocument pageDoc = QJsonDocument::fromVariant(pageIte.value());
					pages[i]->FromSerializedProperties(pageDoc.toJson());
				}
			}
		}
	}

	m_bIsLoading = false;
}

void CPreferences::Save()
{
	if (m_bIsLoading)
		return;

	QString path = QtUtil::GetAppDataFolder();
	QDir(path).mkpath(path);
	path += "/Preferences.pref";

	QFile file(path);
	if (!file.open(QIODevice::WriteOnly))
	{
		QString msg = "Failed to open path: " + path;
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, msg.toLocal8Bit());
		return;
	}

	QVariantMap preferencesVariant;
	for (auto ite = m_preferences.begin(); ite != m_preferences.end(); ++ite)
	{
		std::vector<SPreferencePage*> pages = ite->second;
		QVariantMap pagesVariant;
		for (SPreferencePage* pPage : pages)
		{
			// Create a json doc out of the yasli blob
			QJsonDocument pageDoc = QJsonDocument::fromJson(pPage->GetSerializedProperties().toUtf8());
			// Transform our yasli doc into a variant so we can then save a valid json document
			pagesVariant[QtUtil::ToQString(pPage->GetName())] = pageDoc.toVariant();
		}

		preferencesVariant[QtUtil::ToQString(ite->first)] = pagesVariant;
	}

	QJsonDocument doc(QJsonDocument::fromVariant(preferencesVariant));
	file.write(doc.toJson());
}

void CPreferences::Reset(const char* path)
{
	if (m_bIsLoading)
		return;

	auto ite = m_preferences.find(path);
	if (ite == m_preferences.end())
		return;

	std::vector<SPreferencePage*> pages = ite->second;
	for (SPreferencePage* pPage : pages)
		pPage->signalRequestReset();

	Save();
	signalSettingsReset();
}

void CPreferences::AddPage(SPreferencePage* pPreferencePage)
{
	std::vector<SPreferencePage*>& preferencePages = m_preferences[pPreferencePage->GetPath()];
	preferencePages.push_back(pPreferencePage);
	std::sort(preferencePages.begin(), preferencePages.end(), [](const SPreferencePage* pFirst, const SPreferencePage* pSecond)
	{
		static const char* szGeneral = "General";

		if (!pFirst->GetName().find(szGeneral))
			return true;
		if (!pSecond->GetName().find(szGeneral))
			return false;

		return pFirst->GetName() < pSecond->GetName();
	});

}

std::vector<SPreferencePage*> CPreferences::GetPages(const char* path)
{
	auto ite = m_preferences.find(path);
	if (ite == m_preferences.end())
		return std::vector<SPreferencePage*>();

	return ite->second;
}

bool CPreferences::Serialize(yasli::Archive& ar)
{
	for (auto ite = m_preferences.begin(); ite != m_preferences.end(); ++ite)
	{
		std::vector<SPreferencePage*> preferencePages = ite->second;
		for (SPreferencePage* pPreferencePage : preferencePages)
			ar(*pPreferencePage, pPreferencePage->GetName(), pPreferencePage->GetName());
	}
	return true;
}

