// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Controls/DictionaryWidget.h>
#include <CrySchematyc2/GUID.h>

namespace Cry {
namespace SchematycEd {

class CLegacyOpenDlgEntry : public CAbstractDictionaryEntry
{
	friend class CLegacyOpenDlgModel;

public:
	// CAbstractDictionaryEntry
	virtual uint32 GetType() const override;
	virtual QVariant GetColumnValue(int32 columnIndex) const override;
	virtual QString  GetToolTip(int32 columnIndex) const override;
	// ~CAbstractDictionaryEntry

	QString GetName() const;
	QString GetFullName() const;
	SGUID   GetTypeGUID();

private:
	SGUID   m_guid;
	QString m_name;
	QString m_fullName;
	QString m_description;
};

class CLegacyOpenDlgModel : public CAbstractDictionary
{
public:
	enum EColumn : int32
	{
		Column_Name = 0,
		Column_Count
	};

public:
	CLegacyOpenDlgModel();
	virtual ~CLegacyOpenDlgModel();

	// CryGraphEditor::CAbstractDictionary
	virtual int32                           GetNumEntries() const override;
	virtual const CAbstractDictionaryEntry* GetEntry(int32 index) const override;
	virtual int32                           GetNumColumns() const override;
	virtual QString                         GetColumnName(int32 index) const override;
	virtual int32                           GetDefaultFilterColumn() const override;
	virtual int32                           GetDefaultSortColumn() const override;
	// ~CryGraphEditor::CAbstractDictionary

private:
	void Load();

private:
	std::vector<CLegacyOpenDlgEntry> m_entries;
};

} // namespace SchematycEd
} // namespace Cry
