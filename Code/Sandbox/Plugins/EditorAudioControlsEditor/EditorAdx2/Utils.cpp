// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Utils.h"

#include <CrySystem/File/CryFile.h>

namespace ACE
{
namespace Impl
{
namespace Adx2
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

	if (pItem != nullptr)
	{
		string fullName = pItem->GetName();
		auto pParent = static_cast<CItem const*>(pItem->GetParent());

		while ((pParent != nullptr) && (pParent != &rootItem))
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
	case EItemType::FolderCue:      // Intentional fall-through.
	case EItemType::FolderCueSheet: // Intentional fall-through.
	case EItemType::FolderGlobal:
		{
			name = "folder:";
			break;
		}
	case EItemType::Cue:
		{
			name = "cue:";
			break;
		}
	case EItemType::CueSheet:
		{
			name = "cuesheet:";
			break;
		}
	case EItemType::AisacControl:
		{
			name = "aisaccontrol:";
			break;
		}
	case EItemType::Binary:
		{
			name = "binary:";
			break;
		}
	case EItemType::DspBusSetting:
		{
			name = "dspbussettiing:";
			break;
		}
	case EItemType::GameVariable:
		{
			name = "gamevariable:";
			break;
		}
	case EItemType::Selector:
		{
			name = "selector:";
			break;
		}
	case EItemType::SelectorLabel:
		{
			name = "selectorlabel:";
			break;
		}
	case EItemType::Snapshot:
		{
			name = "snapshot:";
			break;
		}
	case EItemType::WorkUnit:
		{
			name = "workunit:";
			break;
		}
	default:
		{
			name = "";
			break;
		}
	}

	return name;
}

//////////////////////////////////////////////////////////////////////////
string FindCueSheetName(CItem const* const pItem, CItem const& rootItem)
{
	string cueSheetName = "";
	CItem const* pParent = pItem;

	while ((pParent != nullptr) && (pParent != &rootItem))
	{
		if (pParent->GetType() == EItemType::CueSheet)
		{
			cueSheetName = pParent->GetName();
			break;
		}
		else
		{
			pParent = static_cast<CItem const*>(pParent->GetParent());
		}
	}

	return cueSheetName;
}
} // namespace Utils
} // namespace Adx2
} // namespace Impl
} // namespace ACE
