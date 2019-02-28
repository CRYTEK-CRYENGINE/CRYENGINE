// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "QtCommon.h"

#include <ProxyModels/ItemModelAttribute.h>

#include <QAbstractItemModel>

namespace FbxTool
{

class CScene;
struct SNode;

} // namespace FbxTool;

class CSceneElementCommon;
class CSceneElementSourceNode;
class CScene;

class CSceneModelCommon : public QAbstractItemModel
{
public:
	enum EColumn
	{
		eColumn_SourceNodeAttribute,
		eColumn_COUNT
	};

	enum ERole
	{
		eRole_InternalPointerRole = eItemDataRole_MAX
	};

public:
	CSceneModelCommon(QObject* pParent = nullptr);
	virtual ~CSceneModelCommon();

	void                 SetPseudoRootVisible(bool bVisible);

	CSceneElementSourceNode* FindSceneElementOfNode(const FbxTool::SNode* pNode);

	CSceneElementCommon* GetSceneElementFromModelIndex(const QModelIndex& modelIdx) const;
	QModelIndex          GetModelIndexFromSceneElement(CSceneElementCommon* pElement) const;

	// returns the pseudo root element, which is not part of the view.
	// this is the only node with nullptr != GetScene().
	CSceneElementSourceNode*   GetRootElement();
	bool                   IsSceneEmpty() const;

	int                    GetElementCount() const;

	CSceneElementCommon*   GetElementById(int id);

	CScene* GetModelScene(); // TODO: Rename

	const FbxTool::CScene* GetScene() const;
	FbxTool::CScene*       GetScene();

	void                   SetScene(FbxTool::CScene* pScene);
	void                   ClearScene();

	void Reset();

	// QAbstractItemModel implementation.
	virtual QVariant data(const QModelIndex& index, int role) const override;
	virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override;
	virtual QModelIndex parent(const QModelIndex& index) const override;
	virtual int         rowCount(const QModelIndex& index) const override;
protected:
	const CSceneElementCommon* GetRoot() const;
private:
	struct ISceneBuilder;
	struct CSceneModelWithPseudoRoot;
	struct CSceneModelWithoutPseudoRoot;

	void                         ClearSceneWithoutReset();

public:
	//! Source node elements (scene elements that correspond to a node in the FBX scene) can be filtered
	//! by the attribute of the source node. In general, nodes can have multiple attributes. Here, however,
	//! we only filter by the primary attribute.
	//! Source nodes with multiple attributes seem to be rare in practice. If they do occur, it only
	//! affects filtering.
	CItemModelAttribute* GetSourceNodeAttributeAttribute() const;

private:
	std::unique_ptr<CScene> m_pScene;
	FbxTool::CScene*                                  m_pFbxScene;
	CSceneElementSourceNode*                              m_pRoot;
	std::unique_ptr<ISceneBuilder>                    m_pSceneBuilder;

	std::unique_ptr<CItemModelAttributeEnum> m_pSourceNodeAttributeAttribute;
};

