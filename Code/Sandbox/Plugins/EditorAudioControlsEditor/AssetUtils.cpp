// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetUtils.h"

#include "AudioControlsEditorPlugin.h"

#include <FilePathUtil.h>

namespace ACE
{
namespace AssetUtils
{
//////////////////////////////////////////////////////////////////////////
string GenerateUniqueName(string const& name, EAssetType const type, CAsset* const pParent)
{
	string finalName = name;

	if (pParent != nullptr)
	{
		size_t const numChildren = pParent->ChildCount();
		AssetNames names;
		names.reserve(numChildren);

		for (size_t i = 0; i < numChildren; ++i)
		{
			CAsset const* const pChild = pParent->GetChild(i);

			if ((pChild != nullptr) && (pChild->GetType() == type))
			{
				names.emplace_back(pChild->GetName());
			}
		}

		finalName = PathUtil::GetUniqueName(name, names);
	}

	return finalName;
}

//////////////////////////////////////////////////////////////////////////
string GenerateUniqueLibraryName(string const& name)
{
	size_t const numLibraries = g_assetsManager.GetLibraryCount();
	AssetNames names;
	names.reserve(numLibraries);

	for (size_t i = 0; i < numLibraries; ++i)
	{
		CLibrary const* const pLibrary = g_assetsManager.GetLibrary(i);

		if (pLibrary != nullptr)
		{
			names.emplace_back(pLibrary->GetName());
		}
	}

	return PathUtil::GetUniqueName(name, names);
}

//////////////////////////////////////////////////////////////////////////
string GenerateUniqueControlName(string const& name, EAssetType const type)
{
	Controls const& controls(g_assetsManager.GetControls());
	AssetNames names;
	names.reserve(controls.size());

	for (auto const* const pControl : controls)
	{
		if (type == pControl->GetType())
		{
			names.emplace_back(pControl->GetName());
		}
	}

	return PathUtil::GetUniqueName(name, names);
}

//////////////////////////////////////////////////////////////////////////
CAsset* GetParentLibrary(CAsset* const pAsset)
{
	CAsset* pParent = pAsset;

	while ((pParent != nullptr) && (pParent->GetType() != EAssetType::Library))
	{
		pParent = pParent->GetParent();
	}

	return pParent;
}

//////////////////////////////////////////////////////////////////////////
char const* GetTypeName(EAssetType const type)
{
	char const* szTypeName = nullptr;

	switch (type)
	{
	case EAssetType::Trigger:
		szTypeName = "Trigger";
		break;
	case EAssetType::Parameter:
		szTypeName = "Parameter";
		break;
	case EAssetType::Switch:
		szTypeName = "Switch";
		break;
	case EAssetType::State:
		szTypeName = "State";
		break;
	case EAssetType::Environment:
		szTypeName = "Environment";
		break;
	case EAssetType::Preload:
		szTypeName = "Preload";
		break;
	case EAssetType::Folder:
		szTypeName = "Folder";
		break;
	case EAssetType::Library:
		szTypeName = "Library";
		break;
	default:
		szTypeName = nullptr;
		break;
	}

	return szTypeName;
}

//////////////////////////////////////////////////////////////////////////
void SelectTopLevelAncestors(Assets const& source, Assets& dest)
{
	for (auto const pAsset : source)
	{
		// Check if item has ancestors that are also selected
		bool isAncestorAlsoSelected = false;

		for (auto const pOtherItem : source)
		{
			if (pAsset != pOtherItem)
			{
				// Find if pOtherItem is the ancestor of pAsset
				CAsset const* pParent = pAsset->GetParent();

				while (pParent != nullptr)
				{
					if (pParent == pOtherItem)
					{
						break;
					}

					pParent = pParent->GetParent();
				}

				if (pParent != nullptr)
				{
					isAncestorAlsoSelected = true;
					break;
				}
			}
		}

		if (!isAncestorAlsoSelected)
		{
			dest.push_back(pAsset);
		}
	}
}
} // namespace AssetUtils
} // namespace ACE
