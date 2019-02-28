// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameResourcesExporter.h"
#include "GameEngine.h"

#include "Objects/ObjectLayerManager.h"
#include "Objects/EntityObject.h"
#include "Material/MaterialManager.h"
#include "Particles/ParticleManager.h"

#include "QT/Widgets/QWaitProgress.h"

//////////////////////////////////////////////////////////////////////////
// Static data.
//////////////////////////////////////////////////////////////////////////
CGameResourcesExporter::Files CGameResourcesExporter::m_files;

//////////////////////////////////////////////////////////////////////////
void CGameResourcesExporter::ChooseDirectoryAndSave()
{
	ChooseDirectory();
	if (!m_path.IsEmpty())
		Save(m_path);
}

//////////////////////////////////////////////////////////////////////////
void CGameResourcesExporter::ChooseDirectory()
{
	string path;
	{
		CXTBrowseDialog dlg;

		dlg.SetTitle("Choose Target (root/MasterCD) Folder");
		dlg.SetOptions(BIF_NEWDIALOGSTYLE | BIF_RETURNONLYFSDIRS | BIF_DONTGOBELOWDOMAIN);

		if (dlg.DoModal() == IDOK)
			path = dlg.GetSelPath();
		else
			path = "";
	}
	m_path = path;
}

//////////////////////////////////////////////////////////////////////////
void CGameResourcesExporter::GatherAllLoadedResources()
{
	m_files.clear();
	m_files.reserve(100000);      // count from GetResourceList, GetFilesFromObjects, GetFilesFromMaterials ...  is unknown

	IResourceList* pResList = gEnv->pCryPak->GetResourceList(ICryPak::RFOM_Level);
	{
		for (const char* filename = pResList->GetFirst(); filename; filename = pResList->GetNext())
		{
			m_files.push_back(filename);
		}
	}

	GetFilesFromObjects();
	GetFilesFromMaterials();
	GetFilesFromParticles();
}

//////////////////////////////////////////////////////////////////////////
void CGameResourcesExporter::SetUsedResources(CUsedResources& resources)
{
	for (CUsedResources::TResourceFiles::const_iterator it = resources.files.begin(); it != resources.files.end(); it++)
	{
		m_files.push_back(*it);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameResourcesExporter::Save(const string& outputDirectory)
{
	CMemoryBlock data;

	int numFiles = m_files.size();

	CLogFile::WriteLine("===========================================================================");
	CryLog("Exporting Level %s resources, %d files", (const char*)GetIEditorImpl()->GetGameEngine()->GetLevelName(), numFiles);
	CLogFile::WriteLine("===========================================================================");

	// Needed files.
	CWaitProgress wait("Exporting Resources");
	for (int i = 0; i < numFiles; i++)
	{
		string srcFilename = m_files[i];
		if (!wait.Step((i * 100) / numFiles))
			break;
		wait.SetText(srcFilename);

		CLogFile::WriteLine(srcFilename);

		CCryFile file;
		if (file.Open(srcFilename, "rb"))
		{
			// Save this file in target folder.
			string trgFilename = PathUtil::Make(outputDirectory, srcFilename);
			int fsize = file.GetLength();
			if (fsize > data.GetSize())
			{
				data.Allocate(fsize + 16);
			}
			// Read data.
			file.ReadRaw(data.GetBuffer(), fsize);

			// Save this data to target file.
			string trgFileDir = PathUtil::GetPathWithoutFilename(trgFilename);
			CFileUtil::CreateDirectory(trgFileDir);
			// Create a file.
			FILE* trgFile = fopen(trgFilename, "wb");
			if (trgFile)
			{
				// Save data to new file.
				fwrite(data.GetBuffer(), fsize, 1, trgFile);
				fclose(trgFile);
			}
		}
	}
	CLogFile::WriteLine("===========================================================================");
	m_files.clear();
}

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
template<class Container1, class Container2>
void Append(Container1& a, const Container2& b)
{
	a.reserve(a.size() + b.size());
	for (Container2::const_iterator it = b.begin(); it != b.end(); ++it)
		a.insert(a.end(), *it);
}
#else
template<class Container1, class Container2>
void Append(Container1& a, const Container2& b)
{
	a.insert(a.end(), b.begin(), b.end());
}
#endif
//////////////////////////////////////////////////////////////////////////
//
// Go through all editor objects and gathers files from thier properties.
//
//////////////////////////////////////////////////////////////////////////
void CGameResourcesExporter::GetFilesFromObjects()
{
	CUsedResources rs;
	GetIEditorImpl()->GetObjectManager()->GatherUsedResources(rs);

	Append(m_files, rs.files);
}

//////////////////////////////////////////////////////////////////////////
void CGameResourcesExporter::GetFilesFromMaterials()
{
	CUsedResources rs;
	GetIEditorImpl()->GetMaterialManager()->GatherUsedResources(rs);
	Append(m_files, rs.files);
}

//////////////////////////////////////////////////////////////////////////
void CGameResourcesExporter::GetFilesFromParticles()
{
	CUsedResources rs;
	GetIEditorImpl()->GetParticleManager()->GatherUsedResources(rs);
	Append(m_files, rs.files);
}

//////////////////////////////////////////////////////////////////////////
void CGameResourcesExporter::ExportSelectedLayerResources()
{
	CObjectLayer* pSelLayer = GetIEditorImpl()->GetObjectManager()->GetLayersManager()->GetCurrentLayer();
	if (pSelLayer)
	{
		CGameResourcesExporter exporter;
		exporter.ChooseDirectory();
		exporter.SaveLayerResources(pSelLayer->GetName(), pSelLayer, true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameResourcesExporter::GatherLayerResourceList_r(CObjectLayer* pLayer, CUsedResources& resources)
{
	GetIEditorImpl()->GetObjectManager()->GatherUsedResources(resources, pLayer);

	for (int i = 0; i < pLayer->GetChildCount(); i++)
	{
		CObjectLayer* pChildLayer = pLayer->GetChild(i);
		GatherLayerResourceList_r(pChildLayer, resources);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameResourcesExporter::SaveLayerResources(const string& subDirectory, CObjectLayer* pLayer, bool bAllChilds)
{
	if (m_path.IsEmpty())
		return;

	CUsedResources resources;
	GetIEditorImpl()->GetObjectManager()->GatherUsedResources(resources, pLayer);

	m_files.clear();
	SetUsedResources(resources);

	Save(PathUtil::Make(m_path, subDirectory).c_str());

	if (bAllChilds)
	{
		for (int i = 0; i < pLayer->GetChildCount(); i++)
		{
			CObjectLayer* pChildLayer = pLayer->GetChild(i);
			SaveLayerResources(pChildLayer->GetName(), pChildLayer, bAllChilds);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameResourcesExporter::ExportPerLayerResourceList()
{
	const auto& layers = GetIEditorImpl()->GetObjectManager()->GetLayersManager()->GetLayers();
	for (size_t i = 0; i < layers.size(); ++i)
	{
		CObjectLayer* pLayer = layers[i];

		// Only export topmost layers.
		if (pLayer->GetParent())
			continue;

		CUsedResources resources;
		GatherLayerResourceList_r(pLayer, resources);

		string listFilename;
		listFilename.Format("Layer_%s.txt", (const char*)pLayer->GetName());

		listFilename = PathUtil::Make(GetIEditorImpl()->GetLevelFolder(), listFilename);

		std::vector<string> files;

		string levelDir = GetIEditorImpl()->GetLevelFolder();
		for (CUsedResources::TResourceFiles::const_iterator it = resources.files.begin(); it != resources.files.end(); it++)
		{
			string filePath = PathUtil::MakeGamePath((*it).GetString());
			filePath.MakeLower();
			files.push_back(filePath);
		}
		if (!files.empty())
		{
			FILE* file = fopen(listFilename, "wt");
			if (file)
			{
				for (size_t c = 0; c < files.size(); c++)
				{
					fprintf(file, "%s\n", (const char*)files[c]);
				}
				fclose(file);
			}
		}
	}
}

REGISTER_EDITOR_COMMAND(CGameResourcesExporter::ExportSelectedLayerResources, editor, export_layer_resources, CCommandDescription(""));
REGISTER_EDITOR_COMMAND(CGameResourcesExporter::ExportPerLayerResourceList, editor, export_per_layer_resource_list, CCommandDescription(""));

