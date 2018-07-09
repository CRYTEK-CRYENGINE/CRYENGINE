// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySandbox/CrySignal.h>
#include <Controls/DictionaryWidget.h>

#include <QVariant>
#include <QString>
#include <QIcon>

namespace Schematyc {

struct IScriptView;
struct IScriptRegistry;

}

namespace CrySchematycEditor {

class CComponentDictionaryEntry : public CAbstractDictionaryEntry
{
	friend class CComponentsDictionary;

public:
	CComponentDictionaryEntry();
	virtual ~CComponentDictionaryEntry();

	// CAbstractDictionaryEntry
	virtual uint32   GetType() const override { return CAbstractDictionaryEntry::Type_Entry; }

	virtual QVariant GetColumnValue(int32 columnIndex) const override;
	virtual QString  GetToolTip() const override;
	// ~CAbstractDictionaryEntry

	QString          GetName() const { return m_name; }
	CryGUID GetTypeGUID()   { return m_identifier; }

private:
	CryGUID m_identifier;
	QString          m_name;
	QString          m_fullName;
	QString          m_description;
	QIcon            m_icon;
};

class CComponentsDictionary : public CAbstractDictionary
{
public:
	enum EColumn : int32
	{
		Column_Name,

		Column_COUNT
	};

public:
	CComponentsDictionary(const Schematyc::IScriptElement* pScriptScope = nullptr);
	virtual ~CComponentsDictionary();

	// CryGraphEditor::CAbstractDictionary
	virtual int32                           GetNumEntries() const override { return m_components.size(); }
	virtual const CAbstractDictionaryEntry* GetEntry(int32 index) const override;

	virtual int32                           GetNumColumns() const override { return Column_COUNT; };
	virtual QString                         GetColumnName(int32 index) const override;

	virtual int32                           GetDefaultFilterColumn() const override { return Column_Name; }
	virtual int32                           GetDefaultSortColumn() const override { return Column_Name; }
	// ~CryGraphEditor::CAbstractDictionary

	void Load(const Schematyc::IScriptElement* pScriptScope);

private:
	std::vector<CComponentDictionaryEntry> m_components;
};

}

