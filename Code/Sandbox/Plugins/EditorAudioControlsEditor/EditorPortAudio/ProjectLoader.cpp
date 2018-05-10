// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProjectLoader.h"

#include "Impl.h"

#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>

namespace ACE
{
namespace Impl
{
namespace PortAudio
{
//////////////////////////////////////////////////////////////////////////
CProjectLoader::CProjectLoader(string const& assetsPath, CItem& rootItem)
	: m_assetsPath(assetsPath)
{
	LoadFolder("", rootItem);
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadFolder(string const& folderPath, CItem& parent)
{
	_finddata_t fd;
	ICryPak* const pCryPak = gEnv->pCryPak;
	intptr_t const handle = pCryPak->FindFirst(m_assetsPath + "/" + folderPath + "/*.*", &fd);

	if (handle != -1)
	{
		do
		{
			string const name = fd.name;

			if ((name != ".") && (name != "..") && !name.empty())
			{
				if (fd.attrib & _A_SUBDIR)
				{
					if (folderPath.empty())
					{
						LoadFolder(name, *CreateItem(name, folderPath, EItemType::Folder, parent));
					}
					else
					{
						LoadFolder(folderPath + "/" + name, *CreateItem(name, folderPath, EItemType::Folder, parent));
					}
				}
				else
				{
					string::size_type const posExtension = name.rfind('.');

					if (posExtension != string::npos)
					{
						if ((_stricmp(name.data() + posExtension, ".ogg") == 0) || (_stricmp(name.data() + posExtension, ".wav") == 0))
						{
							// Create the event with the same name as the file
							CreateItem(name, folderPath, EItemType::Event, parent);
						}
					}
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
CItem* CProjectLoader::CreateItem(string const& name, string const& path, EItemType const type, CItem& rootItem)
{
	ControlId id;
	string filePath = m_assetsPath + "/";

	if (path.empty())
	{
		id = CryAudio::StringToId(name);
		filePath += name;
	}
	else
	{
		id = CryAudio::StringToId(path + "/" + name);
		filePath += (path + "/" + name);
	}

	EItemFlags const flags = type == EItemType::Folder ? EItemFlags::IsContainer : EItemFlags::None;
	auto const pItem = new CItem(name, id, type, flags, filePath);

	rootItem.AddChild(pItem);
	return pItem;
}
} // namespace PortAudio
} // namespace Impl
} // namespace ACE

