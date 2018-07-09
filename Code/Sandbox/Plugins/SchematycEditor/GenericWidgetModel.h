// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <Controls/DictionaryWidget.h>
#include <CrySchematyc2/GUID.h>
#include <CrySchematyc2/Script/IScriptElement.h>

//////////////////////////////////////////////////////////////////////////
namespace Cry {	
namespace SchematycEd {

	class CWidgetDictionary;
	class CGenericWidgetDictionaryModel;

} // namespace SchematycEd
} // namespace Cry



//////////////////////////////////////////////////////////////////////////
namespace Cry {
namespace SchematycEd {

class CGenericWidgetDictionaryEntry : public CAbstractDictionaryEntry
{
	friend class CGenericWidgetDictionaryModel;

public:
	CGenericWidgetDictionaryEntry(CGenericWidgetDictionaryModel& dictionaryModel);

	// CAbstractDictionaryEntry
	virtual uint32    GetType() const override;
	virtual int32     GetNumChildEntries() const override;
	virtual const     CAbstractDictionaryEntry* GetChildEntry(int32 index) const override;
	virtual const     CAbstractDictionaryEntry* GetParentEntry() const override;
	virtual QVariant  GetColumnValue(int32 columnIndex) const override;
	virtual QString   GetToolTip() const override;
	// ~CAbstractDictionaryEntry

	QString GetName() const;
	QString GetFullName() const;
	SGUID   GetTypeGUID();

private:
	using Childs = std::vector<SGUID>;

	EType   m_node;
	QString m_name;
	QString m_type;
	QString m_fullName;
	QString m_description;

	SGUID  m_guid;
	SGUID  m_parent;
	Childs m_childs;

	CGenericWidgetDictionaryModel& m_dictionaryModel;
};

class CGenericWidgetDictionaryModel : public CAbstractDictionary
{
	friend class CGenericWidget;
	friend class CGenericWidgetDictionaryEntry;

public:
	enum EColumn : int32
	{
		Column_Name = 0,
		Column_Type,
		Column_Count
	};

public:
	template <class T> struct Unit;
	template <class T> struct Impl;

public:
	virtual void BuildFromScriptClass(IScriptFile* file, const SGUID& classGUID) = 0;

	// CryGraphEditor::CAbstractDictionary
	virtual int32                           GetNumEntries() const override;
	virtual const CAbstractDictionaryEntry* GetEntry(int32 index) const override;
	virtual int32                           GetNumColumns() const override;
	virtual QString                         GetColumnName(int32 index) const override;
	virtual int32                           GetDefaultFilterColumn() const override;
	virtual int32                           GetDefaultSortColumn() const override;
	virtual const CItemModelAttribute*      GetColumnAttribute(int32 index) const override;
	// ~CryGraphEditor::CAbstractDictionary	

	CGenericWidgetDictionaryEntry*          GetEntryByGUID(const SGUID& guid) const;

private:
	void                                    ClearScriptElementModel();

	template <typename T> void              LoadScriptElementType(IScriptFile* file, const SGUID& scopeGUID);
	template <typename T> EVisitStatus      BuildScriptElementModel(const T& element);

	CGenericWidgetDictionaryEntry*          BuildScriptElementEntry(const IScriptElement& element, const char* typeName);
	void                                    BuildScriptElementParent(const IScriptElement& element, CGenericWidgetDictionaryEntry* entry, const std::vector<EScriptElementType>& parentType);

private:	
	static CGenericWidgetDictionaryModel*   CreateByCategory(const string& category);
	static const CItemModelAttribute*       GetColumnAttributeInternal(int column);

private:
	std::vector<SGUID>                              m_entryGuid;
	std::vector<CGenericWidgetDictionaryEntry>      m_entryPool;
	std::map<SGUID, CGenericWidgetDictionaryEntry*> m_entryDict;
};

} //namespace SchematycEd
} //namespace Cry
