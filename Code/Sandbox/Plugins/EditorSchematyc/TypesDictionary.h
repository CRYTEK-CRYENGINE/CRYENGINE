// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySandbox/CrySignal.h>
#include <Controls/DictionaryWidget.h>

#include <QVariant>
#include <QString>
#include <QIcon>

#include <CrySchematyc/FundamentalTypes.h>

namespace Schematyc {

struct IScriptView;
struct IScriptRegistry;

}

namespace CrySchematycEditor {

class CTypeDictionaryEntry : public CAbstractDictionaryEntry
{
	friend class CTypesDictionary;

public:
	CTypeDictionaryEntry();
	virtual ~CTypeDictionaryEntry();

	// CAbstractDictionaryEntry
	virtual uint32   GetType() const override { return CAbstractDictionaryEntry::Type_Entry; }

	virtual QVariant GetColumnValue(int32 columnIndex) const override;
	// ~CAbstractDictionaryEntry

	QString                      GetName() const { return m_name; }
	const Schematyc::SElementId& GetTypeId()     { return m_elementId; }

private:
	QString               m_name;
	Schematyc::SElementId m_elementId;
};

class CTypesDictionary : public CAbstractDictionary
{
	struct SType
	{
		QString m_name;
		QIcon   m_icon;
	};

public:
	enum EColumn : int32
	{
		Column_Name,

		Column_COUNT
	};

public:
	CTypesDictionary(const Schematyc::IScriptElement* pScriptScope);
	virtual ~CTypesDictionary();

	// CAbstractDictionary
	virtual int32                           GetNumEntries() const override { return m_types.size(); }
	virtual const CAbstractDictionaryEntry* GetEntry(int32 index) const override;

	virtual int32                           GetNumColumns() const override { return Column_COUNT; };
	virtual QString                         GetColumnName(int32 index) const override;

	virtual int32                           GetDefaultFilterColumn() const override { return Column_Name; }
	// ~CAbstractDictionary

	void Load(const Schematyc::IScriptElement* pScriptScope);

private:
	std::vector<CTypeDictionaryEntry> m_types;
};

}

