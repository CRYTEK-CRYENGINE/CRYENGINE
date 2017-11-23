// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProjectLoader.h"

#include "EditorImpl.h"

#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <CryString/CryPath.h>
#include <CryCore/CryCrc32.h>

namespace ACE
{
namespace PortAudio
{
//////////////////////////////////////////////////////////////////////////
CProjectLoader::CProjectLoader(string const& assetsPath, CImplItem& rootItem)
	: m_assetsPath(assetsPath)
{
	LoadFolder("", rootItem);
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadFolder(string const& folderPath, CImplItem& parent)
{
	_finddata_t fd;
	ICryPak* const pCryPak = gEnv->pCryPak;
	intptr_t const handle = pCryPak->FindFirst(m_assetsPath + CRY_NATIVE_PATH_SEPSTR + folderPath + CRY_NATIVE_PATH_SEPSTR + "*.*", &fd);

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
						LoadFolder(name, *CreateItem(name, folderPath, EImpltemType::Folder, parent));
					}
					else
					{
						LoadFolder(folderPath + CRY_NATIVE_PATH_SEPSTR + name, *CreateItem(name, folderPath, EImpltemType::Folder, parent));
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
							CreateItem(name, folderPath, EImpltemType::Event, parent);
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
CImplItem* CProjectLoader::CreateItem(string const& name, string const& path, EImpltemType const type, CImplItem& rootItem)
{
	CID id;

	if (path.empty())
	{
		id = CCrc32::ComputeLowercase(name);
	}
	else
	{
		id = CCrc32::ComputeLowercase(path + CRY_NATIVE_PATH_SEPSTR + name);
	}

	CImplControl* const pControl = new CImplControl(name, id, static_cast<ItemType>(type));

	if (type == EImpltemType::Folder)
	{
		pControl->SetContainer(true);
	}

	rootItem.AddChild(pControl);
	return pControl;
}
} // namespace PortAudio
} // namespace ACE
