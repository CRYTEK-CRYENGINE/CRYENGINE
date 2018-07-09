// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Qt item model for FbxTool scene.
#pragma once

#include "SceneModelCommon.h"
#include "QtCommon.h"

#include <vector>
#include <memory>
#include <QAbstractItemModel>
#include <Cry3DEngine/CGF/CryHeaders.h> // MAX_STATOBJ_LODS_NUM

class CryIcon;

class QAdvancedTreeView;
class QVariant;

class CSceneElementSkin;

namespace DialogMesh
{

class CSceneUserData;

} // namespace DialogMesh

class CSceneModel : public CSceneModelCommon
{
	Q_OBJECT
public:
	enum EColumnType
	{
		eColumnType_Name,
		eColumnType_Type,
		eColumnType_SourceNodeAttribute,
		eColumnType_COUNT
	};
private:
	enum ERoleType
	{
		eRoleType_Export = eItemDataRole_MAX
	};
public:
	CSceneModel();
	~CSceneModel();

	const FbxMetaData::SSceneUserSettings& GetSceneUserSettings() const;

	void                                   SetExportSetting(const QModelIndex& index, FbxTool::ENodeExportSetting value);
	void                                   ResetExportSettingsInSubtree(const QModelIndex& index);

	void                                   OnDataSerialized(CSceneElementCommon* pElement, bool bChanged);

	void                                   SetScene(FbxTool::CScene* pScene, const FbxMetaData::SSceneUserSettings& userSettings);
	void SetSceneUserData(DialogMesh::CSceneUserData* pSceneUserData);

	// QAbstractItemModel
	int           columnCount(const QModelIndex& index) const override;
	QVariant      data(const QModelIndex& index, int role) const override;
	bool          setData(const QModelIndex& index, const QVariant& value, int role) override;
	QVariant      headerData(int column, Qt::Orientation orientation, int role) const override;
	Qt::ItemFlags flags(const QModelIndex& modelIndex) const override;
	// ~QAbstractItemModel
private:
	enum EState
	{
		eState_Enabled_Included,
		eState_Enabled_Excluded,
		eState_Disabled_Included,
		eState_Disabled_Excluded
	};
	void GetCheckStateRole(CSceneElementSourceNode* pSceneElement, EState& state, bool& partial) const;

	QVariant GetSourceNodeData(CSceneElementSourceNode* pElement, const QModelIndex& index, int role) const;
	QVariant GetSkinData(CSceneElementSkin* pElement, const QModelIndex& index, int role) const;

	static QVariant              GetToolTipForColumn(int column);

	bool SetDataSourceNodeElement(const QModelIndex& index, const QVariant& value, int role);
	bool SetDataSkinElement(const QModelIndex& index, const QVariant& value, int role);

	CItemModelAttribute* GetColumnAttribute(int col) const;
private:
	DialogMesh::CSceneUserData* m_pSceneUserData;
	FbxMetaData::SSceneUserSettings m_userSettings;

	CryIcon* const                  m_pExportIcon;
	int                             m_iconDimension;
};

