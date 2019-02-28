// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Controls/DictionaryWidget.h>

#include <QString>
#include <QVariant>

namespace CryParticleEditor {

class CNodesDictionaryCategory;

class CNodesDictionaryNode : public CAbstractDictionaryEntry
{
public:
	CNodesDictionaryNode(QString name, QString filePath, CNodesDictionaryCategory* pParent = nullptr)
		: CAbstractDictionaryEntry()
		, m_name(name)
		, m_filePath(filePath)
		, m_pParent(pParent)
	{}
	virtual ~CNodesDictionaryNode() {}

	// CAbstractDictionaryEntry
	virtual uint32                          GetType() const override { return Type_Entry; }
	virtual QVariant                        GetColumnValue(int32 columnIndex) const override;

	virtual const CAbstractDictionaryEntry* GetParentEntry() const override;
	virtual QVariant                        GetIdentifier() const override;
	// ~CAbstractDictionaryEntry

	const QString& GetName() const     { return m_name; }
	const QString& GetFilePath() const { return m_filePath; }

private:
	CNodesDictionaryCategory* m_pParent;
	QString                   m_name;
	QString                   m_filePath;
};

class CNodesDictionaryCategory : public CAbstractDictionaryEntry
{
public:
	CNodesDictionaryCategory(QString name, CNodesDictionaryCategory* pParent = nullptr)
		: CAbstractDictionaryEntry()
		, m_name(name)
		, m_pParent(pParent)
	{}
	virtual ~CNodesDictionaryCategory() {}

	// CAbstractDictionaryEntry
	virtual uint32                          GetType() const override { return Type_Folder; }
	virtual QVariant                        GetColumnValue(int32 columnIndex) const override;

	virtual int32                           GetNumChildEntries() const override { return m_categories.size() + m_nodes.size(); }
	virtual const CAbstractDictionaryEntry* GetChildEntry(int32 index) const override;

	virtual const CAbstractDictionaryEntry* GetParentEntry() const override;
	// ~CAbstractDictionaryEntry

	CNodesDictionaryCategory& CreateCategory(QString name);
	CNodesDictionaryNode&     CreateNode(QString name, QString filePath);

private:
	QString                                m_name;
	CNodesDictionaryCategory*              m_pParent;
	std::vector<CNodesDictionaryCategory*> m_categories;
	std::vector<CNodesDictionaryNode*>     m_nodes;
};

class CNodesDictionary : public CAbstractDictionary
{
public:
	enum EColumn : int32
	{
		eColumn_Name,
		eColumn_Identifier,

		eColumn_COUNT
	};

public:
	CNodesDictionary();
	virtual ~CNodesDictionary();

	// CryGraphEditor::CAbstractDictionary
	virtual int32                           GetNumEntries() const override { return m_root.GetNumChildEntries(); }
	virtual const CAbstractDictionaryEntry* GetEntry(int32 index) const override;

	virtual int32                           GetDefaultSortColumn() const override { return 0; }
	virtual int32                           GetNumColumns() const { return 1; }
	// ~CryGraphEditor::CAbstractDictionary

protected:
	void LoadTemplates();

private:
	CNodesDictionaryCategory m_root;
};

}

