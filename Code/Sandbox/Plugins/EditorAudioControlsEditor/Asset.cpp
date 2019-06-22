// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Asset.h"

#include "AssetsManager.h"
#include "AssetUtils.h"
#include "NameValidator.h"

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CAsset* CAsset::GetChild(size_t const index) const
{
	CRY_ASSERT_MESSAGE(index < m_children.size(), "Asset child index out of bounds during %s", __FUNCTION__);

	CAsset* pAsset = nullptr;

	if (index < m_children.size())
	{
		pAsset = m_children[index];
	}

	return pAsset;
}

//////////////////////////////////////////////////////////////////////////
void CAsset::SetParent(CAsset* const pParent)
{
	m_pParent = pParent;
	SetModified(true);
}

//////////////////////////////////////////////////////////////////////////
void CAsset::AddChild(CAsset* const pChildControl)
{
	m_children.push_back(pChildControl);
	SetModified(true);
}

//////////////////////////////////////////////////////////////////////////
void CAsset::RemoveChild(CAsset const* const pChildControl)
{
	m_children.erase(std::remove(m_children.begin(), m_children.end(), pChildControl), m_children.end());
	SetModified(true);
}

//////////////////////////////////////////////////////////////////////////
void CAsset::SetName(string const& name)
{
	string fixedName = name;
	g_nameValidator.FixupString(fixedName);

	if ((!fixedName.IsEmpty()) && (fixedName != m_name) && ((m_flags& EAssetFlags::IsDefaultControl) == 0) && g_nameValidator.IsValid(fixedName))
	{
		if (m_type == EAssetType::Library)
		{
			m_name = AssetUtils::GenerateUniqueLibraryName(fixedName);
			m_id = AssetUtils::GenerateUniqueAssetId(m_name, m_type);
			SetModified(true);
			g_assetsManager.OnAssetRenamed(this);
		}
		else if (m_type == EAssetType::Folder)
		{
			m_name = AssetUtils::GenerateUniqueName(fixedName, m_type, m_pParent);
			m_id = AssetUtils::GenerateUniqueFolderId(m_name, m_pParent);
			SetModified(true);
			g_assetsManager.OnAssetRenamed(this);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAsset::UpdateNameOnMove(CAsset* const pParent)
{
	m_name = AssetUtils::GenerateUniqueName(m_name, m_type, pParent);

	switch (m_type)
	{
	case EAssetType::State:
		{
			m_id = AssetUtils::GenerateUniqueStateId(pParent->GetName(), m_name);
			break;
		}
	case EAssetType::Folder:
		{
			m_id = AssetUtils::GenerateUniqueFolderId(m_name, pParent);
			break;
		}
	default:
		{
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAsset::SetDescription(string const& description)
{
	if (description != m_description)
	{
		m_description = description;
		SetModified(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAsset::SetModified(bool const isModified, bool const isForced /* = false */)
{
	if (!g_assetsManager.IsLoading() || isForced)
	{
		g_assetsManager.SetAssetModified(this, isModified);

		isModified ? (m_flags |= EAssetFlags::IsModified) : (m_flags &= ~EAssetFlags::IsModified);

		// Note: This need to get changed once undo is working.
		// Then we can't set the parent to be not modified if it still could contain other modified children.
		if (m_pParent != nullptr)
		{
			m_pParent->SetModified(isModified, isForced);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
string CAsset::GetFullHierarchyName() const
{
	string name = m_name;
	CAsset* pParent = m_pParent;

	while (pParent != nullptr)
	{
		name = pParent->GetName() + "/" + name;
		pParent = pParent->GetParent();
	}

	return name;
}

//////////////////////////////////////////////////////////////////////////
bool CAsset::HasDefaultControlChildren(AssetNames& names) const
{
	bool hasDefaultControlChildren = (m_flags& EAssetFlags::IsDefaultControl) != 0;

	if (hasDefaultControlChildren)
	{
		names.emplace_back(GetFullHierarchyName());
	}
	else
	{
		size_t const childCount = ChildCount();

		for (size_t i = 0; i < childCount; ++i)
		{
			CAsset const* const pChild = GetChild(i);

			if ((pChild != nullptr) && (pChild->HasDefaultControlChildren(names)))
			{
				hasDefaultControlChildren = true;
			}
		}
	}

	return hasDefaultControlChildren;
}

//////////////////////////////////////////////////////////////////////////
void CAsset::Serialize(Serialization::IArchive& ar)
{
	string const name = m_name;

	if ((m_flags& EAssetFlags::IsDefaultControl) != 0)
	{
		ar(name, "name", "!Name");
	}
	else
	{
		ar(name, "name", "Name");
	}

	ar.doc(name);

	string const description = m_description;

	if ((m_flags& EAssetFlags::IsDefaultControl) != 0)
	{
		ar(description, "description", "!Description");
	}
	else
	{
		ar(description, "description", "Description");
	}

	ar.doc(description);

	if (ar.isInput())
	{
		SetName(name);
		SetDescription(description);
	}
}
} // namespace ACE
