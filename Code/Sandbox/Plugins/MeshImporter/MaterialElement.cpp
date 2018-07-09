// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "MaterialElement.h"
#include "MaterialModel.h"
#include <CrySerialization/IArchive.h>
#include <CrySerialization/Color.h>
#include <CrySerialization/Math.h>
#include "Serialization/Qt.h"
#include <CrySerialization/STL.h>
#include <CrySerialization/StringList.h>
#include <CrySerialization/Decorators/Range.h>
#include <CrySerialization/yasli/Enum.h>

FbxTool::EMaterialPhysicalizeSetting Convert(FbxMetaData::EPhysicalizeSetting setting)
{
	switch (setting)
	{
	case FbxMetaData::EPhysicalizeSetting::Default:
		return FbxTool::eMaterialPhysicalizeSetting_Default;
	case FbxMetaData::EPhysicalizeSetting::NoCollide:
		return FbxTool::eMaterialPhysicalizeSetting_NoCollide;
	case FbxMetaData::EPhysicalizeSetting::None:
		return FbxTool::eMaterialPhysicalizeSetting_None;
	case FbxMetaData::EPhysicalizeSetting::Obstruct:
		return FbxTool::eMaterialPhysicalizeSetting_Obstruct;
	case FbxMetaData::EPhysicalizeSetting::ProxyOnly:
		return FbxTool::eMaterialPhysicalizeSetting_ProxyOnly;
	default:
		assert(0 && "unkown physicalize setting");
		return FbxTool::eMaterialPhysicalizeSetting_Default;
	}
}

FbxMetaData::EPhysicalizeSetting Convert(FbxTool::EMaterialPhysicalizeSetting setting)
{
	switch (setting)
	{
	case FbxTool::eMaterialPhysicalizeSetting_Default:
		return FbxMetaData::EPhysicalizeSetting::Default;
	case FbxTool::eMaterialPhysicalizeSetting_NoCollide:
		return FbxMetaData::EPhysicalizeSetting::NoCollide;
	case FbxTool::eMaterialPhysicalizeSetting_None:
		return FbxMetaData::EPhysicalizeSetting::None;
	case FbxTool::eMaterialPhysicalizeSetting_Obstruct:
		return FbxMetaData::EPhysicalizeSetting::Obstruct;
	case FbxTool::eMaterialPhysicalizeSetting_ProxyOnly:
		return FbxMetaData::EPhysicalizeSetting::ProxyOnly;
	default:
		assert(0 && "unkown physicalize setting");
		return FbxMetaData::EPhysicalizeSetting::Default;
	}
}

void CMaterialElement::Serialize(Serialization::IArchive& ar)
{
	auto physicalizeSetting = Convert(m_pModel->GetScene()->GetMaterialPhysicalizeSetting(m_pMaterial));
	ar(physicalizeSetting, "phys", "Physicalization type");
	m_pModel->GetScene()->SetMaterialPhysicalizeSetting(m_pMaterial, Convert(physicalizeSetting));

	if (ar.isEdit())
	{
		Serialization::StringList options;

		options.push_back("Automatic");
		options.push_back("Delete");

		for (int i = 0; i < m_pModel->rowCount(QModelIndex()); ++i)
		{
			const QString subMaterialName = m_pModel->GetSubMaterialName(i);
			if (subMaterialName.isEmpty())
			{
				options.push_back(QtUtil::ToString(QString::number(i)));
			}
			else
			{
				options.push_back(QtUtil::ToString(QStringLiteral("%1 (%2)").arg(i).arg(subMaterialName)));
			}
		}

		int currentSelection;
		if (IsMarkedForIndexAutoAssignment())
		{
			currentSelection = 0;
		}
		else if (IsMarkedForDeletion())
		{
			currentSelection = 1;
		}
		else
		{
			currentSelection = GetSubMaterialIndex() + 2;
		}

		Serialization::StringListValue dropDown(options, currentSelection);
		ar(dropDown, "index", "Sub-material");
		if (ar.isInput())
		{
			const int nextSelection = dropDown.index();
			switch (nextSelection)
			{
			case 0:
				MarkForIndexAutoAssignment(true);
				break;
			case 1:
				MarkForIndexAutoAssignment(false);
				MarkForDeletion();
				break;
			default:
				MarkForIndexAutoAssignment(false);
				SetSubMaterialIndex(nextSelection - 2);
				break;
			}
		}
	}

	assert(m_pModel);
	m_pModel->OnDataSerialized(this, ar.isInput());
}

CMaterialElement::CMaterialElement(
  const QString& name,
  CMaterialModel* pModel,
  const FbxTool::SMaterial* pMaterial,
  bool bDummyMaterial)
	: m_name(name)
	, m_pModel(pModel)
	, m_pScene(pModel->GetScene())
	, m_pMaterial(pMaterial)
	, m_bDummyMaterial(bDummyMaterial)
{}

const QString& CMaterialElement::GetName() const
{
	return m_name;
}

const FbxTool::SMaterial* CMaterialElement::GetMaterial() const
{
	return m_pMaterial;
}

int CMaterialElement::GetSubMaterialIndex() const
{
	return m_pModel->GetScene()->GetMaterialSubMaterialIndex(m_pMaterial);
}

FbxMetaData::EPhysicalizeSetting CMaterialElement::GetPhysicalizeSetting() const
{
	return Convert(m_pModel->GetScene()->GetMaterialPhysicalizeSetting(m_pMaterial));
}

const ColorF& CMaterialElement::GetColor() const
{
	return m_pScene->GetMaterialColor(m_pMaterial);
}

bool CMaterialElement::IsMarkedForIndexAutoAssignment() const
{
	return m_pScene->IsMaterialMarkedForIndexAutoAssignment(m_pMaterial);
}

bool CMaterialElement::IsMarkedForDeletion() const
{
	return GetSubMaterialIndex() < 0;
}

bool CMaterialElement::IsDummyMaterial() const
{
	return m_bDummyMaterial;
}

void CMaterialElement::MarkForIndexAutoAssignment(bool bEnabled)
{
	m_pScene->MarkMaterialForIndexAutoAssignment(m_pMaterial, bEnabled);
}

void CMaterialElement::SetSubMaterialIndex(int index)
{
	assert(index < m_pModel->rowCount(QModelIndex()));
	m_pModel->GetScene()->SetMaterialSubMaterialIndex(m_pMaterial, index);
}

void CMaterialElement::SetPhysicalizeSetting(FbxMetaData::EPhysicalizeSetting physicalizeSetting)
{
	m_pModel->GetScene()->SetMaterialPhysicalizeSetting(m_pMaterial, Convert(physicalizeSetting));
}

void CMaterialElement::SetColor(const ColorF& color)
{
	m_pScene->SetMaterialColor(m_pMaterial, color);
}

void CMaterialElement::MarkForDeletion()
{
	SetSubMaterialIndex(-1);
}

