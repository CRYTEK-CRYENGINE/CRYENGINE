// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <NodeGraph/AbstractNodeGraphViewModel.h>
#include <NodeGraph/AbstractNodeItem.h>

class CModel;
class CAsset;
class CAssetType;
class CNodeGraphViewModel;
class CAbstractDictionaryEntry;

class CGraphViewModel : public CryGraphEditor::CNodeGraphViewModel
{
	Q_OBJECT
public:
	CGraphViewModel(CModel* pModel);

	virtual CryGraphEditor::INodeGraphRuntimeContext& GetRuntimeContext() override;
	virtual QString                                   GetGraphName() override;
	virtual uint32                                    GetNodeItemCount() const override;
	virtual CryGraphEditor::CAbstractNodeItem*        GetNodeItemByIndex(uint32 index) const override;
	virtual CryGraphEditor::CAbstractNodeItem*        GetNodeItemById(QVariant id) const override;

	virtual uint32                                    GetConnectionItemCount() const override;
	virtual CryGraphEditor::CAbstractConnectionItem*  GetConnectionItemByIndex(uint32 index) const override;
	virtual CryGraphEditor::CAbstractConnectionItem*  GetConnectionItemById(QVariant id) const override;

	const CModel*                                     GetModel() { return m_pModel; }

private:
	void OnBeginModelChange();
	void OnEndModelChange();
private:
	std::unique_ptr<CryGraphEditor::INodeGraphRuntimeContext> m_pRuntimeContext;
	CModel* m_pModel;

	std::vector<std::unique_ptr<CryGraphEditor::CAbstractNodeItem>>       m_nodes;
	std::vector<std::unique_ptr<CryGraphEditor::CAbstractConnectionItem>> m_connections;
	bool m_bSuppressComplexityWarning;
};

//! Asset view model.
class CAssetNodeBase : public CryGraphEditor::CAbstractNodeItem
{
public:
	CAssetNodeBase(CryGraphEditor::CNodeGraphViewModel& viewModel, CAsset* pAsset, const CAssetType* pAssetType, const string& m_path);

	bool                                    CanBeEdited() const;
	void                                    EditAsset() const;

	bool                                    CanBeCreated() const;
	void                                    CreateAsset() const;

	bool                                    CanBeImported() const;
	std::vector<string>                     GetSourceFileExtensions() const;
	void                                    ImportAsset(const string& sourceFilename) const;

	string                                  GetPath() const { return m_path; }
	CAsset*									GetAsset() const { return m_pAsset; }

	virtual const CAbstractDictionaryEntry* GetNodeEntry() const = 0;

protected:
	CAsset* const           m_pAsset;
	const CAssetType* const m_pAssetType;
	string                  m_path;
};

