// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PersonalizationManager.h"
#include "QtUtil.h"
#include "FilePathUtil.h"

#include <CrySystem/ISystem.h>

#include <QSettings>
#include <QJsonDocument>
#include <QFile>
#include <QEvent>


CPersonalizationManager::CPersonalizationManager()
{
	m_saveSharedStateTimer.setInterval(5000);
	m_saveSharedStateTimer.setSingleShot(true);
	m_saveSharedStateTimer.connect(&m_saveSharedStateTimer, &QTimer::timeout, [this]() { SaveSharedState(); });
	m_saveProjectStateTimer.setInterval(5000);
	m_saveProjectStateTimer.setSingleShot(this);
	m_saveProjectStateTimer.connect(&m_saveProjectStateTimer, &QTimer::timeout, [this]() { SaveProjectState(); });

	LoadSharedState();
	LoadProjectState();
}

CPersonalizationManager::~CPersonalizationManager()
{
	SavePersonalization();
}

void CPersonalizationManager::SetProperty(const QString& moduleName, const QString& propName, const QVariant& value)
{
	QVariantMap& moduleState = m_sharedState[moduleName];
	moduleState[propName] = value;

	m_saveSharedStateTimer.start();
}

const QVariant& CPersonalizationManager::GetProperty(const QString& moduleName, const QString& propName)
{
	QVariantMap& moduleState = m_sharedState[moduleName];
	return moduleState[propName];
}

bool CPersonalizationManager::HasProperty(const QString& moduleName, const QString& propName) const
{
	const QVariantMap& moduleState = m_sharedState[moduleName];
	return moduleState.contains(propName);
}

void CPersonalizationManager::SetState(const QString& moduleName, const QVariantMap& state)
{
	m_sharedState[moduleName] = state;

	m_saveSharedStateTimer.start();
}

const QVariantMap& CPersonalizationManager::GetState(const QString& moduleName)
{
	return m_sharedState[moduleName];
}

void CPersonalizationManager::SetProjectProperty(const QString& moduleName, const QString& propName, const QVariant& value)
{
	QVariantMap& moduleState = m_projectState[moduleName];
	moduleState[propName] = value;

	m_saveProjectStateTimer.start();
}

const QVariant& CPersonalizationManager::GetProjectProperty(const QString& moduleName, const QString& propName)
{
	QVariantMap& moduleState = m_projectState[moduleName];
	return moduleState[propName];
}

bool CPersonalizationManager::HasProjectProperty(const QString& moduleName, const QString& propName) const
{
	const QVariantMap& moduleState = m_projectState[moduleName];
	return moduleState.contains(propName);
}

void CPersonalizationManager::SavePersonalization() const
{
	SaveSharedState();
	SaveProjectState();
}

QVariant CPersonalizationManager::ToVariant(const CPersonalizationManager::ModuleStateMap& map)
{
	QVariantMap variant;

	auto it = map.constBegin();

	for (; it != map.constEnd(); ++it)
	{
		variant[it.key()] = it.value();
	}

	return variant;
}

CPersonalizationManager::ModuleStateMap CPersonalizationManager::FromVariant(const QVariant& variant)
{
	ModuleStateMap map;
	QVariantMap variantMap = variant.toMap();
	auto it = variantMap.constBegin();

	for (; it != variantMap.constEnd(); ++it)
	{
		const QString& moduleName = it.key();
		QVariantMap moduleMap = it.value().toMap();
		map.insert(moduleName, moduleMap);
	}

	return map;
}

void CPersonalizationManager::SaveSharedState() const
{
	QString path = QtUtil::GetAppDataFolder();
	QDir(path).mkpath(path);
	path += "/Personalization.json";

	QFile file(path);
	if (!file.open(QIODevice::WriteOnly))
	{
		QString msg = "Failed to open path: " + path;
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, msg.toLocal8Bit());
		return;
	}

	QJsonDocument doc(QJsonDocument::fromVariant(ToVariant(m_sharedState)));
	file.write(doc.toJson());
}

void CPersonalizationManager::LoadSharedState()
{
	QString path = QtUtil::GetAppDataFolder();
	path += "/Personalization.json";

	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
	{
		QString msg = "Failed to open path: " + path;
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_COMMENT, msg.toLocal8Bit());
		return;
	}

	QJsonDocument doc(QJsonDocument::fromJson(file.readAll()));
	QVariant variant = doc.toVariant();
	m_sharedState = FromVariant(variant);
}

void CPersonalizationManager::LoadProjectState()
{
	QString path = PathUtil::GetUserSandboxFolder();
	path += "/Personalization.json";

	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
	{
		QString msg = "Failed to open path: " + path;
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_COMMENT, msg.toLocal8Bit());
		return;
	}

	QJsonDocument doc(QJsonDocument::fromJson(file.readAll()));
	QVariant variant = doc.toVariant();
	m_projectState = FromVariant(variant);
}

void CPersonalizationManager::SaveProjectState() const
{
	QString path = PathUtil::GetUserSandboxFolder();
	QDir(path).mkpath(path);
	path += "/Personalization.json";

	QFile file(path);
	if (!file.open(QIODevice::WriteOnly))
	{
		QString msg = "Failed to open path: " + path;
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, msg.toLocal8Bit());
		return;
	}

	QJsonDocument doc(QJsonDocument::fromVariant(ToVariant(m_projectState)));
	file.write(doc.toJson());
}

