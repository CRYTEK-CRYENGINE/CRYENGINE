// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SceneModelCommon.h"
#include <vector>

namespace FbxTool { class CScene; }

class CSkeleton
{
public:
	typedef std::function<void()> Callback;

	CSkeleton()
		: m_pExportRoot(nullptr)
	{}

	void SetScene(const FbxTool::CScene* pScene);

	void SetNodeInclusion(const FbxTool::SNode* pNode, bool bIncluded);

	const FbxTool::SNode* GetExportRoot() const;
	const std::vector<bool>& GetNodesInclusion() const;

	void SetCallback(const Callback& callback);
private:
	std::vector<bool> m_nodesInclusion;
	const FbxTool::SNode* m_pExportRoot;
	Callback m_callback;
};

class CSceneModelSkeleton : public CSceneModelCommon
{
private:
	enum EColumnType
	{
		eColumnType_Name,
		eColumnType_SourceNodeAttribute,
		eColumnType_COUNT
	};

public:
	CSceneModelSkeleton(CSkeleton* pNodeSkeleton)
		: m_pNodeSkeleton(pNodeSkeleton)
	{}

	// QAbstractItemModel implementation.
	int           columnCount(const QModelIndex& index) const override;
	QVariant      data(const QModelIndex& index, int role) const override;
	QVariant      headerData(int column, Qt::Orientation orientation, int role) const override;
	Qt::ItemFlags flags(const QModelIndex& modelIndex) const override;
	bool          setData(const QModelIndex& index, const QVariant& value, int role) override;

private:
	CItemModelAttribute* GetColumnAttribute(int col) const;

private:
	CSkeleton* m_pNodeSkeleton;
};
