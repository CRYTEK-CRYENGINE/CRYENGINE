// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Utils.h"

#include <CrySystem/File/CryFile.h>

namespace ACE
{
namespace Impl
{
namespace Fmod
{
namespace Utils
{
//////////////////////////////////////////////////////////////////////////
ControlId GetId(EItemType const type, string const& name, CItem* const pParent, CItem const& rootItem)
{
	string const fullName = GetTypeName(type) + GetPathName(pParent, rootItem) + "/" + name;
	return CryAudio::StringToId(fullName.c_str());
}

//////////////////////////////////////////////////////////////////////////
string GetPathName(CItem const* const pItem, CItem const& rootItem)
{
	string pathName = "";
	EItemType const editorFolderType = EItemType::EditorFolder;

	if (pItem != nullptr)
	{
		string fullName = pItem->GetName();
		auto pParent = static_cast<CItem const*>(pItem->GetParent());

		while ((pParent != nullptr) && (pParent->GetType() != editorFolderType) && (pParent != &rootItem))
		{
			// The id needs to represent the full path, as we can have items with the same name in different folders
			fullName = pParent->GetName() + "/" + fullName;
			pParent = static_cast<CItem const*>(pParent->GetParent());
		}

		pathName = fullName;
	}

	return pathName;
}

//////////////////////////////////////////////////////////////////////////
string GetTypeName(EItemType const type)
{
	string name = "";

	switch (type)
	{
	case EItemType::Folder:
		name = "folder:";
		break;
	case EItemType::Event:
		name = "event:";
		break;
	case EItemType::Parameter:
		name = "parameter:";
		break;
	case EItemType::Snapshot:
		name = "snapshot:";
		break;
	case EItemType::Bank:
		name = "bank:";
		break;
	case EItemType::Return:
		name = "return:";
		break;
	case EItemType::VCA:
		name = "vca:";
		break;
	case EItemType::MixerGroup:
		name = "group:";
		break;
	case EItemType::EditorFolder:
		name = "editorfolder:";
		break;
	default:
		name = "";
		break;
	}

	return name;
}
} // namespace Utils
} // namespace Fmod
} // namespace Impl
} // namespace ACE

