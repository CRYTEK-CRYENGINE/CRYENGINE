// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MaterialSettings.h"

#include <Cry3DEngine/I3DEngine.h>
#include <IEditor.h>
#include <IResourceSelectorHost.h>
#include <Material\Material.h>
#include <Material\MaterialManager.h>

#include <CrySerialization/Decorators/Resources.h>
#include <Serialization/Decorators/EditorActionButton.h>

CMaterialSettings::CMaterialSettings()
	: m_pMaterial()
{}

CMaterialSettings::~CMaterialSettings()
{}

void CMaterialSettings::SetMaterialChangedCallback(const MaterialCallback& callback)
{
	m_materialChangedCallback = callback;
}

CMaterial* CMaterialSettings::GetMaterial()
{
	return m_pMaterial;
}

const CMaterial* CMaterialSettings::GetMaterial() const
{
	return m_pMaterial;
}

string CMaterialSettings::GetMaterialName() const
{
	if (GetMaterial())
	{
		return GetMaterial()->GetName().GetString();
	}
	else
	{
		return string();
	}
}

void CMaterialSettings::SetMaterial(CMaterial* pMaterial)
{
	if (m_pMaterial == pMaterial)
	{
		return;
	}
	m_pMaterial = pMaterial;
	if (m_materialChangedCallback)
	{
		m_materialChangedCallback();
	}
}

void CMaterialSettings::SetMaterial(const string& materialName)
{
	CMaterial* pMaterial = nullptr;
	CMaterialManager* pManager = GetIEditor()->GetMaterialManager();
	if (!materialName.empty() && pManager != nullptr)
	{
		pMaterial = pManager->LoadMaterial(materialName.c_str());
	}
	SetMaterial(pMaterial);
}

void CMaterialSettings::Serialize(yasli::Archive& ar)
{
	// Material serialization.

	string materialName = "";
	if (m_pMaterial)
	{
		materialName = m_pMaterial->GetName();
	}

	if (ar.openBlock("settings", "Settings"))
	{
		ar(Serialization::MaterialPicker(materialName), "mtl", "Material");
		ar.closeBlock();
	}

	if (ar.isInput())
	{
		if (materialName.empty())
		{
			SetMaterial(nullptr);
		}
		else
		{
			SetMaterial(materialName);
		}
	}
}

