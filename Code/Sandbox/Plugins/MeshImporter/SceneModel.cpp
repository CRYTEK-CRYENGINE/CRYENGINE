// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "SceneModel.h"
#include "Scene/SceneElementSourceNode.h"
#include "Scene/SceneElementPhysProxies.h"
#include "Scene/SceneElementProxyGeom.h"
#include "Scene/SceneElementSkin.h"
#include "Scene/SceneElementTypes.h"
#include "Scene/Scene.h"
#include "SceneUserData.h"
#include "DialogMesh/DialogMesh_SceneUserData.h"
#include "FbxMetaData.h"
#include <QVariant>
#include <QPixmap>

#include <CryIcon.h>

#include <EditorStyleHelper.h>

namespace
{

const FbxTool::ENodeExportSetting kDefaultSceneExportSetting = FbxTool::eNodeExportSetting_Include;

CItemModelAttributeEnumFunc s_meshTypeAttribute("Mesh Type", []()
{
	QStringList strs;
	strs.reserve(MAX_STATOBJ_LODS_NUM + 1);
	for (int i = 0; i < MAX_STATOBJ_LODS_NUM; ++i)
	{
		strs.push_back(QString("LOD %1").arg(i));
	}
	strs.push_back(QStringLiteral("Physics Proxy"));
	return strs;
});

} // namespace

CSceneModel::CSceneModel()
	: CSceneModelCommon()
	, m_pExportIcon(new CryIcon())
	, m_pSceneUserData(nullptr)
{
	QPixmap enabledIcon = QStringLiteral("icons:General/Tick_On.ico");
	QPixmap disabledIcon = QStringLiteral("icons:General/Tick_Off.ico");
	assert(enabledIcon.width() == enabledIcon.height());
	assert(disabledIcon.width() == disabledIcon.height());
	assert(enabledIcon.width() == disabledIcon.width());
	m_pExportIcon->addPixmap(enabledIcon, CryIcon::Normal, CryIcon::On);
	m_pExportIcon->addPixmap(disabledIcon, CryIcon::Normal, CryIcon::Off);
	m_iconDimension = enabledIcon.height();
}

CSceneModel::~CSceneModel()
{
	delete m_pExportIcon;
}

static bool IsNotExcluded(CSceneElementSourceNode* pSceneElement)
{
	const FbxTool::SNode* pNode = pSceneElement->GetNode();
	const FbxTool::ENodeExportSetting exportSetting = pNode->pScene->GetNodeExportSetting(pNode);
	return exportSetting != FbxTool::eNodeExportSetting_Exclude;
}

const FbxMetaData::SSceneUserSettings& CSceneModel::GetSceneUserSettings() const
{
	return m_userSettings;
}

static bool GetLodIndex(const char* const szName, const char* szPrefix, int& lod)
{
	const size_t lodNamePrefixLength = strlen(szPrefix);

	if (strnicmp(szName, szPrefix, lodNamePrefixLength))
	{
		return false;
	}

	const int value = atoi(szName + lodNamePrefixLength);
	if (value >= 1 && value < MAX_STATOBJ_LODS_NUM)
	{
		lod = value;
		return true;
	}

	return false;
}

static bool IsSceneRoot(CSceneElementSourceNode* pSceneElement)
{
	return pSceneElement->GetNode() && !pSceneElement->GetNode()->pParent;
}

static void AutoAssignLods(CScene& scene, FbxTool::CScene* pFbxScene)
{
	for (int i = 0; i < scene.GetElementCount(); ++i)
	{
		if (scene.GetElementByIndex(i)->GetType() != ESceneElementType::SourceNode)
		{
			continue;
		}

		CSceneElementSourceNode* const pElement = (CSceneElementSourceNode*)scene.GetElementByIndex(i);

		if (!pElement->IsLeaf())
		{
			continue;
		}

		int lod = -1;

		// Try to match 3ds Max convention:
		// $lod{N}[name] is N-th LOD of parent.
		if (GetLodIndex(pElement->GetName().c_str(), "$lod", lod))
		{
			pFbxScene->SetNodeLod(pElement->GetNode(), lod);
		}

		// Try to match Maya convention:
		// single child of node with name _lod{N}_name_group is N-th LOD of parent.
		if (pElement->GetParent() && GetLodIndex(pElement->GetParent()->GetName().c_str(), "_lod", lod))
		{
			pFbxScene->SetNodeLod(pElement->GetNode(), lod);
		}
	}
}

void CSceneModel::SetScene(FbxTool::CScene* pScene, const FbxMetaData::SSceneUserSettings& userSettings)
{
	CSceneModelCommon::SetScene(pScene);

	m_userSettings = userSettings;

	for (int i = 0; i < GetElementCount(); ++i)
	{
		if (GetElementById(i)->GetType() != ESceneElementType::SourceNode)
		{
			continue;
		}

		CSceneElementSourceNode* const pElement = (CSceneElementSourceNode*)GetElementById(i);
		const FbxTool::SNode* const pNode = pElement->GetNode();

		pScene->SetNodeExportSetting(pNode, GetDefaultNodeExportSetting(pNode));
	}

	AutoAssignLods(*GetModelScene(), GetScene());
}

void CSceneModel::SetSceneUserData(DialogMesh::CSceneUserData* pSceneUserData)
{
	m_pSceneUserData = pSceneUserData;
}

void CSceneModel::SetExportSetting(const QModelIndex& index, FbxTool::ENodeExportSetting value)
{
	setData(index, (int)value, eRoleType_Export);
}

static void ResetExportSettingsInSubtree(FbxTool::CScene* pScene, const FbxTool::SNode* pFbxNode)
{
	pScene->SetNodeExportSetting(pFbxNode, FbxTool::eNodeExportSetting_Include);

	for (int i = 0; i < pFbxNode->numChildren; ++i)
	{
		ResetExportSettingsInSubtree(pScene, pFbxNode->ppChildren[i]);
	}
}

void CSceneModel::ResetExportSettingsInSubtree(const QModelIndex& index)
{
	CSceneElementCommon* const pSelf = GetSceneElementFromModelIndex(index);
	if (!pSelf || pSelf->GetType() != ESceneElementType::SourceNode)
	{
		return;
	}

	CSceneElementSourceNode* const pSourceNodeElement = (CSceneElementSourceNode*)pSelf;
	CRY_ASSERT(IsSceneRoot(pSourceNodeElement));

	// Reset settings of this LOD's FBX root element.
	::ResetExportSettingsInSubtree(GetScene(), pSourceNodeElement->GetNode());

	Reset();
}

void CSceneModel::OnDataSerialized(CSceneElementCommon* pElement, bool bChanged)
{
	if (bChanged)
	{
		QModelIndex index = GetModelIndexFromSceneElement(pElement);
		assert(index.isValid());
		const QModelIndex firstChanged = sibling(index.row(), 0, index);
		const QModelIndex lastChanged = sibling(index.row(), columnCount(index) - 1, index);
		dataChanged(firstChanged, lastChanged);
	}
}

int CSceneModel::columnCount(const QModelIndex& index) const
{
	return eColumnType_COUNT;
}

static FbxTool::ENodeExportSetting EvaluateExportSetting(const FbxTool::CScene* pFbxScene, const FbxTool::SNode* pFbxNode)
{
	return pFbxScene->IsNodeIncluded(pFbxNode) ? FbxTool::eNodeExportSetting_Include : FbxTool::eNodeExportSetting_Exclude;
}

void CSceneModel::GetCheckStateRole(CSceneElementSourceNode* pSelf, EState& state, bool& bPartial) const
{
	const CSceneElementSourceNode* pParent = nullptr;
	if (pSelf->GetParent() && pSelf->GetParent()->GetType() == ESceneElementType::SourceNode)
	{
		pParent = (CSceneElementSourceNode*)pSelf->GetParent();
	}

	if (EvaluateExportSetting(GetScene(), pSelf->GetNode()) == FbxTool::eNodeExportSetting_Include)
	{
		state = eState_Enabled_Included;
	}
	else if (pParent && EvaluateExportSetting(GetScene(), pParent->GetNode()) == FbxTool::eNodeExportSetting_Include)
	{
		state = eState_Enabled_Excluded;
	}
	else if (GetScene()->GetNodeExportSetting(pSelf->GetNode()) == FbxTool::eNodeExportSetting_Include)
	{
		state = eState_Disabled_Included;
	}
	else
	{
		state = eState_Disabled_Excluded;
	}

	bPartial = false;

	// TODO: Compute this for all elements in a single pass.
	std::vector<const CSceneElementCommon*> elementStack;
	elementStack.push_back(pSelf);
	while (!elementStack.empty() && !bPartial)
	{
		const CSceneElementCommon* pElement = elementStack.back();
		elementStack.pop_back();

		if (pElement->GetType() == ESceneElementType::SourceNode)
		{
			const FbxTool::SNode* const pNode = ((CSceneElementSourceNode*)pElement)->GetNode();
			if (GetScene()->GetNodeExportSetting(pNode) == FbxTool::eNodeExportSetting_Exclude)
			{
				bPartial = true;
			}
		}

		for (int i = 0; i < pElement->GetNumChildren(); ++i)
		{
			elementStack.push_back(pElement->GetChild(i));
		}
	}

}

bool CanBeLod(const FbxTool::SNode* pNode)
{
	return
		pNode->pParent &&
		!pNode->numChildren &&
		EvaluateExportSetting(pNode->pScene, pNode) == FbxTool::eNodeExportSetting_Include &&
		pNode->numMeshes;
}

template<typename T>
QVariant CreateSerializationVariant(const T& t)
{
	std::unique_ptr<Serialization::SStruct> sstruct(new Serialization::SStruct(t));
	return QVariant::fromValue((void*)sstruct.release());
}

QVariant CSceneModel::GetSourceNodeData(CSceneElementSourceNode* pSelf, const QModelIndex& index, int role) const
{
	if (role == eItemDataRole_YasliSStruct)
	{
		return CreateSerializationVariant(*pSelf);
	}

	const FbxTool::SNode* const pNode = pSelf->GetNode();

	EState state;
	bool bPartial;
	GetCheckStateRole(pSelf, state, bPartial);

	switch (index.column())
	{
	case eColumnType_Name:
		if (role == Qt::DisplayRole)
		{
			return QtUtil::ToQString(pSelf->GetName());
		}
		else if (role == Qt::CheckStateRole)
		{
			if (m_pSceneUserData->GetSelectedSkin())
			{
				return QVariant();
			}

			if (state == eState_Enabled_Included || state == eState_Disabled_Included)
			{
				return bPartial ? Qt::PartiallyChecked : Qt::Checked;
			}
			else if (state == eState_Enabled_Excluded || state == eState_Disabled_Excluded)
			{
				return Qt::Unchecked;
			}
		}
		else if (role == Qt::ForegroundRole)
		{
			if (state == eState_Disabled_Included || state == eState_Disabled_Excluded)
			{
				return QBrush(GetStyleHelper()->disabledTextColor());
			}
			else
			{
				return QBrush(GetStyleHelper()->textColor());
			}
		}
		break;
	case eColumnType_Type:
		if (role == Qt::EditRole && CanBeLod(pNode))
		{
			return GetScene()->IsProxy(pNode) ? MAX_STATOBJ_LODS_NUM : GetScene()->GetNodeLod(pNode);
		}
		else if (role == Qt::DisplayRole && CanBeLod(pNode))
		{
			return QString("LOD %1").arg(GetScene()->IsProxy(pNode) ? MAX_STATOBJ_LODS_NUM : GetScene()->GetNodeLod(pNode));
		}
		else if (role == Qt::DisplayRole && !IsSceneRoot(pSelf) && !CanBeLod(pNode))
		{
			return "LOD 0";  // Just display lod, non-editable.
		}
		break;
	case eColumnType_SourceNodeAttribute:
		if (role == Qt::DisplayRole && !pNode->namedAttributes.empty())
		{
			return QtUtil::ToQString(pNode->namedAttributes[0]);
		}
		break;
	default:
		assert(false);
		break;
	}
	if (role == Qt::ToolTipRole)
	{
		return GetToolTipForColumn(index.column());
	}

	return CSceneModelCommon::data(index, role);
}

bool IsSkinElementSelected(DialogMesh::CSceneUserData* pSceneUserData, CSceneElementSkin* pSkinElement)
{
	CSceneElementSourceNode* const pSourceNodeElement = (CSceneElementSourceNode*)pSkinElement->GetParent();
	return pSceneUserData->GetSelectedSkin() && pSourceNodeElement->GetNode() == pSceneUserData->GetSelectedSkin()->pNode;
}

QVariant CSceneModel::GetSkinData(CSceneElementSkin* pSelf, const QModelIndex& index, int role) const
{	
	switch (index.column())
	{
	case eColumnType_Name:
		if (role == Qt::DisplayRole)
		{
			return QtUtil::ToQString(pSelf->GetName());
		}
		else if (role == Qt::CheckStateRole)
		{
			if (IsSkinElementSelected(m_pSceneUserData, pSelf))
			{
				return Qt::Checked;
			}
			else
			{
				return Qt::Unchecked;
			}
		}
		break;
	case eColumnType_Type:
		if (role == Qt::DisplayRole)
		{
			return "Skin";
		}
		break;
	default:
		break;
	}
	if (role == Qt::ToolTipRole)
	{
		return GetToolTipForColumn(index.column());
	}

	return CSceneModelCommon::data(index, role);
}

QVariant CSceneModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
	{
		return QVariant();
	}

	CSceneElementCommon* const pSelf = GetSceneElementFromModelIndex(index);
	assert(pSelf);

	if (pSelf->GetType() == ESceneElementType::SourceNode)
	{
		return GetSourceNodeData((CSceneElementSourceNode*)pSelf, index, role);
	}
	else if(pSelf->GetType() == ESceneElementType::Skin)
	{
		return GetSkinData((CSceneElementSkin*)pSelf, index, role);
	}
	else if (pSelf->GetType() == ESceneElementType::ProxyGeom || pSelf->GetType() == ESceneElementType::PhysProxy)
	{
		if (role == eItemDataRole_YasliSStruct)
		{
			if (pSelf->GetType() == ESceneElementType::PhysProxy)
			{
				return CreateSerializationVariant(*(CSceneElementPhysProxies*)pSelf);
			}
			else
			{
				return CreateSerializationVariant(*(CSceneElementProxyGeom*)pSelf);
			}
		}
		else if (index.column() == eColumnType_Name && role == Qt::DisplayRole)
		{
			return QtUtil::ToQString(pSelf->GetName());
		}
	}
	return CSceneModelCommon::data(index, role);
}

bool CSceneModel::SetDataSourceNodeElement(const QModelIndex& index, const QVariant& value, int role)
{
	CSceneElementSourceNode* const pSourceNodeElement = (CSceneElementSourceNode*)GetSceneElementFromModelIndex(index);

	const FbxTool::SNode* const pNode = pSourceNodeElement->GetNode();

	if (role == Qt::CheckStateRole)
	{
		const auto checkState = (Qt::CheckState)value.toUInt();

		if (GetScene()->GetNodeExportSetting(pNode) == FbxTool::eNodeExportSetting_Include && checkState == Qt::Checked)
		{
			GetScene()->SetNodeExportSetting(pNode, FbxTool::eNodeExportSetting_Exclude);
		}
		else
		{
			const auto exportSetting =
			  checkState == Qt::Checked
			  ? FbxTool::eNodeExportSetting_Include
			  : FbxTool::eNodeExportSetting_Exclude;
			GetScene()->SetNodeExportSetting(pNode, exportSetting);
		}

		Reset();

		return true;
	}
	if (role == eRoleType_Export)
	{
		GetScene()->SetNodeExportSetting(pNode, static_cast<FbxTool::ENodeExportSetting>(value.value<int>()));

		Reset();

		return true;
	}
	else if (index.column() == eColumnType_Type && role == Qt::EditRole)
	{
		const int i = value.toInt();
		if (i < MAX_STATOBJ_LODS_NUM)
		{ 
			GetScene()->SetNodeLod(pNode, i);
			GetScene()->SetProxy(pNode, false);
		}
		else
		{
			GetScene()->SetNodeLod(pNode, 0);
			GetScene()->SetProxy(pNode, true);
		}

		const QModelIndex firstChanged = sibling(index.row(), 0, index);
		const QModelIndex lastChanged = sibling(index.row(), eColumnType_COUNT - 1, index);
		dataChanged(firstChanged, lastChanged);
		return true;
	}

	return false;
}

bool CSceneModel::SetDataSkinElement(const QModelIndex& index, const QVariant& value, int role)
{
	CSceneElementSkin* const pSkinElement = (CSceneElementSkin*)GetSceneElementFromModelIndex(index);

	CRY_ASSERT(pSkinElement->GetParent() && pSkinElement->GetParent()->GetType() == ESceneElementType::SourceNode);
	CSceneElementSourceNode* const pSourceNodeElement = (CSceneElementSourceNode*)pSkinElement->GetParent();

	const FbxTool::SNode* const pNode = pSourceNodeElement->GetNode();

	if (role == Qt::CheckStateRole)
	{
		const auto checkState = (Qt::CheckState)value.toUInt();

		if (checkState == Qt::Checked)
		{
			m_pSceneUserData->SelectAnySkin(pNode);
		}
		else
		{
			m_pSceneUserData->DeselectSkin();
		}

		Reset();

		return true;
	}

	return false;
}

bool CSceneModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	if (!index.isValid())
	{
		return false;
	}

	CSceneElementCommon* const pElement = GetSceneElementFromModelIndex(index);
	CRY_ASSERT(pElement);

	if (pElement->GetType() == ESceneElementType::SourceNode)
	{
		return SetDataSourceNodeElement(index, value, role);
	}
	else if (pElement->GetType() == ESceneElementType::Skin)
	{
		return SetDataSkinElement(index, value, role);
	}

	return false;
}

CItemModelAttribute* CSceneModel::GetColumnAttribute(int col) const
{
	switch(col)
	{
	case eColumnType_Name:
		return &Attributes::s_nameAttribute;
	case eColumnType_Type:
		return &s_meshTypeAttribute;
	case eColumnType_SourceNodeAttribute:
		return GetSourceNodeAttributeAttribute();
	default:
		return nullptr;
	};
}

QVariant CSceneModel::headerData(int column, Qt::Orientation orientation, int role) const
{
	switch (column)
	{
	case eColumnType_Name:
		if (role == Qt::DisplayRole)
		{
			return tr("Name");
		}
		break;
	case eColumnType_Type:
		if (role == Qt::DisplayRole)
		{
			return tr("Type");
		}
		break;
	case eColumnType_SourceNodeAttribute:
		if (role == Qt::DisplayRole)
		{
			return tr("Attribute");
		}
	default:
		break;
	}

	if (role == Qt::ToolTipRole)
	{
		return GetToolTipForColumn(column);
	}

	// Attributes.
	if (role == Attributes::s_getAttributeRole)
	{
		CItemModelAttribute* const pAttribute = GetColumnAttribute(column);
		if (pAttribute)
		{
			return QVariant::fromValue(pAttribute);
		}
	}

	return QVariant();
}

Qt::ItemFlags CSceneModel::flags(const QModelIndex& modelIndex) const
{
	if (!modelIndex.isValid())
	{
		return CSceneModelCommon::flags(modelIndex);
	}

	const CSceneElementCommon* const pSelf = GetSceneElementFromModelIndex(modelIndex);
	CRY_ASSERT(pSelf);

	bool bCanBeLod = false;

	if (pSelf->GetType() == ESceneElementType::SourceNode)
	{
		bCanBeLod = CanBeLod(((CSceneElementSourceNode*)pSelf)->GetNode());
	}

	if (modelIndex.column() == eColumnType_Type && bCanBeLod)
	{
		return Qt::ItemIsEditable | QAbstractItemModel::flags(modelIndex);
	}
	else if (modelIndex.column() == eColumnType_Name)
	{
		return Qt::ItemIsUserCheckable | QAbstractItemModel::flags(modelIndex);
	}
	else
	{
		return QAbstractItemModel::flags(modelIndex);
	}
}

QVariant CSceneModel::GetToolTipForColumn(int column)
{
	switch (column)
	{
	case eColumnType_Name:
		return tr("Name of the node in the scene");
	case eColumnType_Type:
		return tr("Type of this node");
	case eColumnType_SourceNodeAttribute:
		return tr("Attributes of this node");
	default:
		assert(false);
		break;
	}
	return QVariant();
}

