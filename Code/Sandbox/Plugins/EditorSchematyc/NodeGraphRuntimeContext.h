// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <NodeGraph/ICryGraphEditor.h>
#include <Controls/DictionaryWidget.h>

#include <CrySchematyc/Script/IScriptGraph.h>

#include <ProxyModels/ItemModelAttribute.h>

class QIcon;

namespace Schematyc {

struct IScriptGraph;

}

namespace CryGraphEditor {

class CNodeGraphViewStyle;

}

namespace CrySchematycEditor {

class CNodesDictionaryCategoryEntry;

class CNodesDictionaryNodeEntry : public CAbstractDictionaryEntry
{
	friend class CNodesDictionaryCreator;

public:
	CNodesDictionaryNodeEntry(QString name, QString fullName, const Schematyc::IScriptGraphNodeCreationCommandPtr pCommand, CNodesDictionaryCategoryEntry* pParent = nullptr, const QIcon* pIcon = nullptr)
		: CAbstractDictionaryEntry()
		, m_name(name)
		, m_fullName(fullName)
		, m_pCommand(pCommand)
		, m_pParent(pParent)
		, m_pIcon(pIcon)
	{}
	virtual ~CNodesDictionaryNodeEntry() {}

	// CAbstractDictionaryEntry
	virtual uint32                          GetType() const override { return Type_Entry; }
	virtual QVariant                        GetColumnValue(int32 columnIndex) const override;
	virtual const QIcon*                    GetColumnIcon(int32 columnIndex) const override;

	virtual const CAbstractDictionaryEntry* GetParentEntry() const override;
	virtual QVariant                        GetIdentifier() const;
	// ~CAbstractDictionaryEntry

	const QString&                                      GetName() const    { return m_name; }
	const Schematyc::IScriptGraphNodeCreationCommandPtr GetCommand() const { return m_pCommand; }

private:
	CNodesDictionaryCategoryEntry*                      m_pParent;
	QString                                             m_name;
	QString                                             m_fullName;
	const QIcon*                                        m_pIcon;
	const Schematyc::IScriptGraphNodeCreationCommandPtr m_pCommand;
};

class CNodesDictionaryCategoryEntry : public CAbstractDictionaryEntry
{
	friend class CNodesDictionaryCreator;

public:
	CNodesDictionaryCategoryEntry(QString name, CNodesDictionaryCategoryEntry* pParent = nullptr)
		: CAbstractDictionaryEntry()
		, m_name(name)
		, m_pParent(pParent)
	{}
	virtual ~CNodesDictionaryCategoryEntry();

	// CryGraphEditor::CAbstractDictionaryItem
	virtual uint32                          GetType() const override { return Type_Folder; }
	virtual QVariant                        GetColumnValue(int32 columnIndex) const override;

	virtual int32                           GetNumChildEntries() const override { return m_categories.size() + m_nodes.size(); }
	virtual const CAbstractDictionaryEntry* GetChildEntry(int32 index) const override;

	virtual const CAbstractDictionaryEntry* GetParentEntry() const override;
	// ~CryGraphEditor::CAbstractDictionaryItem

	const QString& GetName() const { return m_name; }

private:
	CNodesDictionaryCategoryEntry*              m_pParent;
	QString                                     m_name;
	QString                                     m_fullName;
	std::vector<CNodesDictionaryCategoryEntry*> m_categories;
	std::vector<CNodesDictionaryNodeEntry*>     m_nodes;
};

class CNodesDictionary : public CAbstractDictionary
{
	friend class CNodesDictionaryCreator;

public:
	enum : int32
	{
		eColumn_Name,
		eColumn_Filter,
		eColumn_Identifier,

		eColumn_COUNT
	};

public:
	CNodesDictionary();
	virtual ~CNodesDictionary();

	// CAbstractDictionary
	virtual void                            ClearEntries() override;
	virtual int32                           GetNumEntries() const override { return m_categories.size() + m_nodes.size(); }
	virtual const CAbstractDictionaryEntry* GetEntry(int32 index) const override;

	virtual int32                           GetNumColumns() const                   { return eColumn_COUNT; }

	virtual int32                           GetDefaultFilterColumn() const override { return eColumn_Filter; }
	virtual int32                           GetDefaultSortColumn() const override   { return eColumn_Name; }

	virtual const CItemModelAttribute*      GetColumnAttribute(int32 index) const override;
	// ~CAbstractDictionary

	void LoadLoadsFromScriptGraph(Schematyc::IScriptGraph& scriptGraph);
	void SetStyle(const CryGraphEditor::CNodeGraphViewStyle* pStyle) { m_pStyle = pStyle; }

private:
	const CryGraphEditor::CNodeGraphViewStyle*  m_pStyle;
	std::vector<CNodesDictionaryCategoryEntry*> m_categories;
	std::vector<CNodesDictionaryNodeEntry*>     m_nodes;

	static const CItemModelAttribute            s_columnAttributes[eColumn_COUNT];
};

class CNodeGraphRuntimeContext : public CryGraphEditor::INodeGraphRuntimeContext
{
public:
	CNodeGraphRuntimeContext(Schematyc::IScriptGraph& scriptGraph);
	~CNodeGraphRuntimeContext();

	// CryGraphEditor::INodeGraphRuntimeContext
	virtual const char*                                GetTypeName() const override { return "Schematyc_Graph"; };
	virtual CAbstractDictionary*                       GetAvailableNodesDictionary() override;

	virtual const CryGraphEditor::CNodeGraphViewStyle* GetStyle() const override { return m_pStyle; }
	// ~CryGraphEditor::INodeGraphRuntimeContext

private:
	CNodesDictionary                     m_nodesDictionary;
	CryGraphEditor::CNodeGraphViewStyle* m_pStyle;
	Schematyc::IScriptGraph&             m_scriptGraph;
};

}

