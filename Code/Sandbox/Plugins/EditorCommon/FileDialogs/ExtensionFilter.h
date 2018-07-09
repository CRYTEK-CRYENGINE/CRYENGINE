// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <EditorCommonAPI.h>

#include <QVector>
#include <QString>

class CExtensionFilter;
typedef QVector<CExtensionFilter> ExtensionFilterVector;

/// \brief captures an extension filter for file dialogs
class CExtensionFilter
{
public:
	CExtensionFilter(const QString& description, const QStringList& extensions)
		: m_description(description)
		, m_extensions(extensions)
	{}

	CExtensionFilter(const QString& description, const char* extension1)
		: m_description(description)
		, m_extensions(QStringList() << QString::fromLocal8Bit(extension1))
	{}

	CExtensionFilter(const QString& description, const QString& extension1)
		: m_description(description)
		, m_extensions(QStringList() << extension1)
	{}

	CExtensionFilter(const QString& description, const char* extension1, const char* extension2)
		: m_description(description)
		, m_extensions(QStringList() << QString::fromLocal8Bit(extension1) << QString::fromLocal8Bit(extension2))
	{}

	CExtensionFilter(const QString& description, const QString& extension1, const QString& extension2)
		: m_description(description)
		, m_extensions(QStringList() << extension1 << extension2)
	{}

	CExtensionFilter(const QString& description, const char* extension1, const char* extension2, const char* extension3)
		: m_description(description)
		, m_extensions(QStringList() << QString::fromLocal8Bit(extension1) << QString::fromLocal8Bit(extension2) << QString::fromLocal8Bit(extension3))
	{}

	CExtensionFilter(const QString& description, const QString& extension1, const QString& extension2, const QString& extension3)
		: m_description(description)
		, m_extensions(QStringList() << extension1 << extension2 << extension3)
	{}

	CExtensionFilter() {}

	CExtensionFilter(const CExtensionFilter& other)
		: m_description(other.m_description)
		, m_extensions(other.m_extensions)
	{}

	const QString&     GetDescription() const { return m_description; }
	const QStringList& GetExtensions() const  { return m_extensions; }

	/// parses the old format ("<Description>|*.ext;*.other|SecondDescription|*.abc")
	EDITOR_COMMON_API static ExtensionFilterVector Parse(const char* pattern);

private:
	QString     m_description;
	QStringList m_extensions;
};

