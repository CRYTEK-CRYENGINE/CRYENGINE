// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "MaterialModel.h"
#include "MaterialElement.h"
#include "MaterialHelpers.h"
#include <Cry3DEngine/CGF/CryHeaders.h>  // MAX_SUB_MATERIALS

#include <QSize>

void LogPrintf(const char* szFormat, ...);

CMaterialModel::CMaterialModel()
	: m_pScene(nullptr)
{
	m_random.Seed((uint32)time(nullptr));
}

CMaterialModel::~CMaterialModel()
{
	// Empty on purpose
}

QModelIndex CMaterialModel::AtIndex(CMaterialElement* pElement) const
{
	int index = 0;
	for (auto& element : m_materials)
	{
		if (element.get() == pElement)
		{
			return createIndex(index, 0, pElement);
		}
		++index;
	}
	return QModelIndex();
}

const FbxTool::CScene* CMaterialModel::GetScene() const
{
	return m_pScene;
}

FbxTool::CScene* CMaterialModel::GetScene()
{
	return m_pScene;
}

void CMaterialModel::SetScene(FbxTool::CScene* pScene)
{
	beginResetModel();

	m_materials.clear();

	m_pScene = pScene;

	const int numMats = pScene->GetMaterialCount();
	const auto& materials = pScene->GetMaterials();

	for (int i = 0; i < numMats; ++i)
	{
		const FbxTool::SMaterial* const pMaterial = materials[i].get();
		if (pMaterial->numMeshesUsingIt > 0)
		{
			const QString name = QtUtil::ToQStringSafe(pMaterial->szName);
			CMaterialElement* const pElement = new CMaterialElement(name, this, pMaterial, pScene->IsDummyMaterial(pMaterial));

			const ColorF randomColor(
			  m_random.GetRandom(0.0f, 1.0f),
			  m_random.GetRandom(0.0f, 1.0f),
			  m_random.GetRandom(0.0f, 1.0f));
			pElement->SetColor(Lerp(randomColor, Col_LightBlue, 0.5f));

			m_materials.push_back(std::unique_ptr<CMaterialElement>(pElement));
		}
	}

	endResetModel();
}

void CMaterialModel::ClearScene()
{
	if (m_materials.empty())
	{
		return;
	}

	const int lastRow = static_cast<int>(m_materials.size() - 1);
	beginRemoveRows(QModelIndex(), 0, lastRow);
	m_materials.clear();
	endRemoveRows();
}

void CMaterialModel::Reset()
{
	beginResetModel();
	endResetModel();
}

void CMaterialModel::OnDataSerialized(CMaterialElement* pElement, bool bChanged)
{
	if (bChanged)
	{
		QModelIndex index = AtIndex(pElement);
		assert(index.isValid());
		const QModelIndex firstChanged = sibling(index.row(), 0, index);
		const QModelIndex lastChanged = sibling(index.row(), eColumnType_Count - 1, index);
		dataChanged(firstChanged, lastChanged);
	}
}

CMaterialElement* CMaterialModel::FindElement(const QString& name)
{
	for (auto& pElement : m_materials)
	{
		if (pElement->GetName() == name)
		{
			return pElement.get();
		}
	}
	return nullptr;
}

void CMaterialModel::ApplyMaterials(TKnownMaterials&& knownMaterials, bool bRefreshCollection)
{
	if (bRefreshCollection)
	{
		m_knownMaterials = std::move(knownMaterials);
	}

	AutoAssignSubMaterialIDs(m_knownMaterials, m_pScene);

	dataChanged(this->index(0, 0), this->index(m_materials.size() - 1, eColumnType_Count));
}

QString CMaterialModel::GetSubMaterialName(int index) const
{
	QString name;
	for (size_t i = 0; name.isEmpty() && i < m_knownMaterials.size(); ++i)
	{
		if (m_knownMaterials[i].first == index)
		{
			name = m_knownMaterials[i].second;
		}
	}
	return name;
}

int CMaterialModel::rowCount(const QModelIndex& index) const
{
	return (int)m_materials.size();
}

int CMaterialModel::columnCount(const QModelIndex& index) const
{
	return eColumnType_Count;
}

QVariant CMaterialModel::data(const QModelIndex& index, int role) const
{
	if (role == eItemDataRole_YasliSStruct)
	{
		std::unique_ptr<Serialization::SStruct> sstruct(new Serialization::SStruct(*AtIndex(index)));
		return QVariant::fromValue((void*)sstruct.release());
	}

	switch (index.column())
	{
	case eColumnType_Name:
		if (role == Qt::DisplayRole || role == eItemDataRole_Comparison)
		{
			return AtIndex(index)->GetName();
		}
		break;
	case eColumnType_SubMaterial:
		if (role == Qt::DisplayRole)
		{
			QStringList attribs;

			const CMaterialElement* const pMaterialElement = AtIndex(index);

			if (pMaterialElement->IsMarkedForDeletion())
			{
				return QStringLiteral("delete");
			}

			if (pMaterialElement->IsMarkedForIndexAutoAssignment())
			{
				attribs << QStringLiteral("auto");
			}

			const QString subMaterialName = GetSubMaterialName(pMaterialElement->GetSubMaterialIndex());
			if (!subMaterialName.isEmpty())
			{
				attribs << subMaterialName;
			}

			return
			  attribs.isEmpty()
			  ? QStringLiteral("%1").arg(pMaterialElement->GetSubMaterialIndex())
			  : QStringLiteral("%1 (%2)").arg(pMaterialElement->GetSubMaterialIndex()).arg(attribs.join(QStringLiteral(", ")));
		}
		else if (role == eItemDataRole_Comparison)
		{
			const CMaterialElement* const pMaterialElement = AtIndex(index);
			return
			  pMaterialElement->IsMarkedForDeletion()
			  ? MAX_SUB_MATERIALS + 1 // Put deleted materials at the very bottom of the list.
			  : pMaterialElement->GetSubMaterialIndex();
		}
		else if (role == Qt::EditRole)
		{
			const CMaterialElement* const pMaterialElement = AtIndex(index);
			int editorIndex;
			if (pMaterialElement->IsMarkedForIndexAutoAssignment())
			{
				editorIndex = 0;
			}
			else if (pMaterialElement->IsMarkedForDeletion())
			{
				editorIndex = 1;
			}
			else
			{
				editorIndex = pMaterialElement->GetSubMaterialIndex() + 2;
			}
			return editorIndex;
		}
		break;
	case eColumnType_Physicalize:
		if (role == Qt::DisplayRole || role == eItemDataRole_Comparison)
		{
			return QtUtil::ToQString(FbxMetaData::ToString(AtIndex(index)->GetPhysicalizeSetting()));
		}
		else if (role == Qt::EditRole)
		{
			return (int)Convert(AtIndex(index)->GetPhysicalizeSetting());
		}
		break;
	case eColumnType_Color:
		if (role == Qt::DisplayRole)
		{
			return QStringLiteral("  "); // Create a little amount of spacing.
		}
		else if (role == Qt::DecorationRole)
		{
			const ColorF& color = AtIndex(index)->GetColor();
			return QColor::fromRgbF(color.r, color.g, color.b);
		}
		else if (role == eItemDataRole_Comparison)
		{
			return AtIndex(index)->GetColor().Luminance();
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
	return QVariant();
}

QVariant CMaterialModel::headerData(int column, Qt::Orientation orientation, int role) const
{
	switch (column)
	{
	case eColumnType_Name:
		if (role == Qt::DisplayRole)
		{
			return tr("Material name");
		}
		break;
	case eColumnType_SubMaterial:
		if (role == Qt::DisplayRole)
		{
			return tr("Sub-material");
		}
		break;
	case eColumnType_Physicalize:
		if (role == Qt::DisplayRole)
		{
			return tr("Physicalization");
		}
		break;
	case eColumnType_Color:
		if (role == Qt::DisplayRole)
		{
			return tr("Color");
		}
		break;
	default:
		assert(false);
		break;
	}
	if (role == Qt::ToolTipRole)
	{
		return GetToolTipForColumn(column);
	}
	return QVariant();
}

bool CMaterialModel::setData(const QModelIndex& modelIndex, const QVariant& variant, int role)
{
	if (!modelIndex.isValid())
	{
		return QAbstractItemModel::setData(modelIndex, variant, role);
	}

	CMaterialElement* const pMaterialElement = AtIndex(modelIndex);
	CRY_ASSERT(pMaterialElement);

	// Use RAII to make materials consistent after setting data.
	const std::unique_ptr<CMaterialElement, std::function<void(CMaterialElement*)>> consistencyGuard(
	  pMaterialElement, [this](CMaterialElement* pEl) { MakeConsistent(*pEl); });

	if (role == eItemDataRole_Color)
	{
		const QColor color = variant.value<QColor>();
		pMaterialElement->SetColor(ColorF(color.redF(), color.greenF(), color.blueF()));
		dataChanged(modelIndex, modelIndex);
		return true;
	}
	else if (role == Qt::EditRole && modelIndex.column() == eColumnType_SubMaterial)
	{
		const int editorIndex = variant.value<int>();
		CRY_ASSERT(editorIndex >= 0);
		if (data(modelIndex, role).value<int>() == editorIndex)
		{
			// Nothing changed.
			return false;
		}
		switch (editorIndex)
		{
		case 0:
			pMaterialElement->MarkForIndexAutoAssignment(true);
			break;
		case 1:
			pMaterialElement->MarkForIndexAutoAssignment(false);
			pMaterialElement->MarkForDeletion();
			break;
		default:
			pMaterialElement->MarkForIndexAutoAssignment(false);
			pMaterialElement->SetSubMaterialIndex(editorIndex - 2);
			break;
		}
		ApplyMaterials(TKnownMaterials() /* Unused */, false);
		return true;
	}
	else if (role == Qt::EditRole && modelIndex.column() == eColumnType_Physicalize)
	{
		const auto physicalizeSetting = FbxTool::EMaterialPhysicalizeSetting(variant.value<int>());
		pMaterialElement->SetPhysicalizeSetting(Convert(physicalizeSetting));
		dataChanged(modelIndex, modelIndex);
		return true;
	}

	return QAbstractItemModel::setData(modelIndex, variant, role);
}

Qt::ItemFlags CMaterialModel::flags(const QModelIndex& modelIndex) const
{
	if (modelIndex.column() == eColumnType_Physicalize || modelIndex.column() == eColumnType_SubMaterial)
	{
		return Qt::ItemIsEditable | QAbstractItemModel::flags(modelIndex);
	}
	else
	{
		return QAbstractItemModel::flags(modelIndex);
	}
}

QVariant CMaterialModel::GetToolTipForColumn(int column)
{
	switch (column)
	{
	case eColumnType_Name:
		return tr("The name of the material");
	case eColumnType_SubMaterial:
		return tr("The sub-material index of this material");
	case eColumnType_Physicalize:
		return tr("The physicalization type of the material");
	case eColumnType_Color:
		return tr("Color of material in editor preview.");
	default:
		assert(false);
		break;
	}
	return QVariant();
}

// Postcondition: All materials having the same sub-material id as pProvokingElement will also have its phys setting.
void CMaterialModel::MakeConsistent(CMaterialElement& provokingElement)
{
	for (size_t i = 0; i < m_materials.size(); ++i)
	{
		if (provokingElement.GetSubMaterialIndex() == m_materials[i]->GetSubMaterialIndex() &&
		    provokingElement.GetPhysicalizeSetting() != m_materials[i]->GetPhysicalizeSetting())
		{
			m_materials[i]->SetPhysicalizeSetting(provokingElement.GetPhysicalizeSetting());
			QModelIndex index = AtIndex(m_materials[i].get());
			dataChanged(index, index);
		}
	}
}

void CSortedMaterialModel::SetScene(FbxTool::CScene* pScene)
{
	m_pModel->SetScene(pScene);
}

void CSortedMaterialModel::ClearScene()
{
	m_pModel->ClearScene();
}

void CSortedMaterialModel::Reset()
{
	m_pModel->Reset();
}

const FbxTool::CScene* CSortedMaterialModel::GetScene() const
{
	return m_pModel->GetScene();
}

QString CSortedMaterialModel::GetSubMaterialName(int index) const
{
	return m_pModel->GetSubMaterialName(index);
}

void CSortedMaterialModel::ApplyMaterials(const string& materialName)
{
	auto loadedMaterialNames = GetSubMaterialList(materialName);
	m_pModel->ApplyMaterials(std::move(loadedMaterialNames), true);

	Reset();
}

bool CSortedMaterialModel::lessThan(const QModelIndex& lhp, const QModelIndex& rhp) const
{
	// Always show the dummy material at the very top of the list.
	if (m_pModel->AtIndex(lhp)->IsDummyMaterial())
	{
		return true;
	}
	if (m_pModel->AtIndex(rhp)->IsDummyMaterial())
	{
		return false;
	}
	return QSortFilterProxyModel::lessThan(lhp, rhp);
}

