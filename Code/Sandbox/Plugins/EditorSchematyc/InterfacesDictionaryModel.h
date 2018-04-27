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

class CInterfaceDictionaryEntry : public CAbstractDictionaryEntry
{
	friend class CInterfacesDictionary;

public:
	CInterfaceDictionaryEntry();
	virtual ~CInterfaceDictionaryEntry();

	// CAbstractDictionaryEntry
	virtual uint32   GetType() const override { return CAbstractDictionaryEntry::Type_Entry; }

	virtual QVariant GetColumnValue(int32 columnIndex) const override;
	virtual QString  GetToolTip() const override;
	// ~CAbstractDictionaryEntry

	QString            GetName() const    { return m_name; }
	Schematyc::CryGUID   GetInterfaceGUID() { return m_identifier; }
	Schematyc::EDomain GetDomain()        { return m_domain; }

private:
	Schematyc::CryGUID	m_identifier;
	QString            m_name;
	QString            m_fullName;
	QString            m_description;
	Schematyc::EDomain m_domain;
};

class CInterfacesDictionary : public CAbstractDictionary
{
public:
	enum EColumn : int32
	{
		Column_Name,

		Column_COUNT
	};

public:
	CInterfacesDictionary(const Schematyc::IScriptElement* pScriptScope = nullptr);
	virtual ~CInterfacesDictionary();

	// CryGraphEditor::CAbstractDictionary
	virtual int32                           GetNumEntries() const override { return m_interfaces.size(); }
	virtual const CAbstractDictionaryEntry* GetEntry(int32 index) const override;

	virtual int32                           GetNumColumns() const override { return Column_COUNT; };
	virtual QString                         GetColumnName(int32 index) const override;

	virtual int32                           GetDefaultFilterColumn() const override { return Column_Name; }
	// ~CryGraphEditor::CAbstractDictionary

private:
	std::vector<CInterfaceDictionaryEntry> m_interfaces;
};

}

