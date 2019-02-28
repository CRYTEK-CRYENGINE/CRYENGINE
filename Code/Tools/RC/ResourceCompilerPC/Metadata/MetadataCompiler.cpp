// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MetadataCompiler.h"
#include "Metadata/Metadata.h"
#include "FileUtil.h"
#include "StringHelpers.h"
#include "CGF/CGFLoader.h"

namespace
{

std::vector<string> FindCgfAssetFiles(const string& filename)
{
	std::vector<string> files;

	// Find lods:
	// name.cgf
	// name_lod1.cgf
	// name_lod2.cgf
	// ...

	files.emplace_back(filename);

	const char* const szExt = PathUtil::GetExt(filename.c_str());
	const string name = PathUtil::RemoveExtension(filename.c_str());

	CRY_ASSERT(MAX_STATOBJ_LODS_NUM < 10);
	for (int i = 1; i < MAX_STATOBJ_LODS_NUM; ++i)
	{
		const string lodFilename = name + "_lod" + ('0' + i) + "." + szExt;
		if (!FileUtil::FileExists(lodFilename))
		{
			continue;
		}
		files.emplace_back(lodFilename);
	}

	return files;
}

std::vector<string> FindDdsAssetFiles(const string& filename)
{
	std::vector<string> files;

	// Find mips:
	// name.dds
	// name.dds.a
	// name.dds.1
	// name.dds.1a
	// ...

	int i = 0;
	for (string color = filename, alpha = color + ".a";
	     FileUtil::FileExists(color);
	     color = string().Format("%s.%i", filename.c_str(), ++i), alpha = color + "a")
	{
		files.emplace_back(color);

		if (FileUtil::FileExists(alpha))
		{
			files.emplace_back(alpha);
		}
	}
	return files;
}

bool IsCgfLod(const string& cgf)
{
	const char* const szExt = PathUtil::GetExt(cgf.c_str());
	const string filename(string(cgf).MakeLower());
	const char* const szLod = strstr(filename.c_str(), "_lod");
	return szLod && (szLod != filename.begin()) && FileUtil::FileExists(string(filename.begin(), szLod) + "." + szExt);
}

//! Removes the read-only attribute and deletes the file. Returns true if successful or file does not exist, otherwise returns false.
bool DeleteFileIfExists(const string filename)
{
	SetFileAttributesA(filename.c_str(), FILE_ATTRIBUTE_ARCHIVE);
	return (DeleteFileA(filename.c_str()) != 0) || (GetLastError() == ERROR_FILE_NOT_FOUND);
}

void CollectCgfDetails(XmlNodeRef& xmlnode, const CContentCGF& cgf)
{
	size_t triangleCount = 0;
	size_t vertexCount = 0;
	bool b16BitPrecision = false;

	for (int i = 0, n = cgf.GetNodeCount(); i < n; ++i)
	{
		const CNodeCGF* const pNode = cgf.GetNode(i);
		if (!pNode)
		{
			continue;
		}

		const CMesh* const pMesh = pNode->pMesh;
		if (pMesh)
		{
			triangleCount += std::max(pMesh->GetFaceCount(), pMesh->GetIndexCount() / 3); // Compiled meshes do not have faces.
			vertexCount += pMesh->GetVertexCount();
			b16BitPrecision = b16BitPrecision || (pMesh->m_pPositionsF16 != nullptr);
		}
	}

	const int materialCount = cgf.GetMaterialCount();

	std::vector<std::pair<string, string>> details;
	details.emplace_back("materialCount", string().Format("%i", materialCount));
	details.emplace_back("triangleCount", string().Format("%i", triangleCount));
	details.emplace_back("vertexCount", string().Format("%i", vertexCount));
	details.emplace_back("16BitPrecision", b16BitPrecision ? "true" : "false");

	const CSkinningInfo* pSkin = cgf.GetSkinningInfo();
	if (pSkin && !pSkin->m_arrBoneEntities.empty())
	{
		details.emplace_back("bonesCount", string().Format("%i", pSkin->m_arrBoneEntities.size()));
	}

	std::vector<std::pair<string,int32>> dependencies;
	if (materialCount)
	{
		dependencies.emplace_back(PathUtil::ReplaceExtension(cgf.GetMaterial(0)->name, "mtl"), 1);
	}

	AssetManager::AddDetails(xmlnode, details);
	AssetManager::AddDependencies(xmlnode, dependencies);
}

bool CollectCgfDetails(XmlNodeRef& xmlnode, const char* szFilename, IResourceCompiler* pRc)
{
	using namespace AssetManager;

	CChunkFile chunkFile;
	CLoaderCGF cgfLoader;
	std::unique_ptr<CContentCGF> pCGF(cgfLoader.LoadCGF(szFilename, chunkFile, nullptr));
	if (!pCGF)
	{
		return false;
	}

	XmlNodeRef metadata = GetMetadataNode(xmlnode);
	if (!metadata)
	{
		return false;
	}

	CollectCgfDetails(metadata, *pCGF);
	return true;
}

bool CollectMtlDetails(XmlNodeRef& xmlnode, const char* szFilename, IResourceCompiler* pRc)
{
	XmlNodeRef metadata = AssetManager::GetMetadataNode(xmlnode);
	if (!metadata)
	{
		return false;
	}

	const XmlNodeRef mtl = pRc->LoadXml(szFilename);
	if (!mtl || stricmp(mtl->getTag(), "Material"))
	{
		return false;
	}

	int subMaterialCount = 0;
	int textureCount = 0;
	std::vector<std::pair<string, int32>> dependencies;
	std::stack<XmlNodeRef> mtls;
	mtls.push(mtl);
	while (!mtls.empty())
	{
		XmlNodeRef mtl = mtls.top();
		mtls.pop();

		// Process this material.
		const XmlNodeRef textures = mtl->findChild("Textures");
		if (textures)
		{
			for (int i = 0, N = textures->getChildCount(); i < N; ++i)
			{
				const XmlNodeRef texture = textures->getChild(i);
				const char* filename;
				if (!texture || stricmp(texture->getTag(), "Texture") || !texture->getAttr("File", &filename))
				{
					continue;
				}
				const string path = PathUtil::ReplaceExtension(filename, "dds");
				auto it = std::find_if(dependencies.begin(), dependencies.end(), [&path](const auto& x) 
				{ 
					return path.CompareNoCase(x.first) == 0; 
				});

				if (it != dependencies.end())
				{
					++(it->second);
					continue;
				}
				dependencies.emplace_back(path, 1);
				++textureCount;
			}
		}

		// Add sub materials to the queue.
		const XmlNodeRef subs = mtl->findChild("SubMaterials");
		if (subs)
		{
			for (int i = 0, N = subs->getChildCount(); i < N; ++i)
			{
				const XmlNodeRef subMtl = subs->getChild(i);
				if (!subMtl || stricmp(subMtl->getTag(), "Material"))
				{
					continue;
				}
				mtls.push(subMtl);
				++subMaterialCount;
			}
		}
	}
	std::vector<std::pair<string, string>> details;
	details.emplace_back("subMaterialCount", string().Format("%i", subMaterialCount));
	details.emplace_back("textureCount", string().Format("%i", textureCount));

	AssetManager::AddDetails(metadata, details);
	AssetManager::AddDependencies(metadata, dependencies);
	return true;
}

// Assets for XML files have an attribute that store the top-level node tag.
// On editor side this is used, for example, to classify XML files as libraries (particles, prefabs, ...).
// Having the tag part of the cryasset means we do not have to touch the data XML file on disk.
static bool CollectXmlDetails(XmlNodeRef& xmlnode, const char* szFilename, IResourceCompiler* pRc)
{
	XmlNodeRef metadata = AssetManager::GetMetadataNode(xmlnode);
	if (!metadata)
	{
		return false;
	}

	const XmlNodeRef libRoot = pRc->LoadXml(szFilename);
	if (!libRoot)
	{
		return false;
	}

	AssetManager::AddDetails(metadata, {
			{ "rootTag", string(libRoot->getTag()) }
	  });
	return true;
}

bool CollectCdfDetails(XmlNodeRef& xmlnode, const char* szFilename, IResourceCompiler* pRc)
{
	XmlNodeRef metadata = AssetManager::GetMetadataNode(xmlnode);
	if (!metadata)
	{
		return false;
	}

	const XmlNodeRef cdf = pRc->LoadXml(szFilename);
	if (!cdf || stricmp(cdf->getTag(), "CharacterDefinition"))
	{
		return false;
	}

	std::vector<std::pair<string,int32>> dependencies;

	const char* filename;
	const XmlNodeRef model = cdf->findChild("Model");
	if (model && model->getAttr("File", &filename))
	{
		dependencies.emplace_back(filename, 0);
		if (model->getAttr("Material", &filename))
		{
			dependencies.emplace_back(filename, 0);
		}
	}

	const XmlNodeRef list = cdf->findChild("AttachmentList");
	if (list)
	{
		static const char* const attributes[]
		{
			"Binding", "simBinding", "Material", "MaterialLOD0", "MaterialLOD1", "MaterialLOD2", "MaterialLOD3", "MaterialLOD4", "MaterialLOD5"
		};

		for (int i = 0, N = list->getChildCount(); i < N; ++i)
		{
			const XmlNodeRef item = list->getChild(i);
			if (!item || stricmp(item->getTag(), "Attachment"))
			{
				continue;
			}

			for (const char* szAttr : attributes)
			{
				if (item->getAttr(szAttr, &filename))
				{
					dependencies.emplace_back(filename, 0);
				}
			}
		}
	}

	AssetManager::AddDependencies(metadata, dependencies);
	return true;
}

} // namespace

CMetadataCompiler::CMetadataCompiler(IResourceCompiler* pRc)
	: m_pResourceCompiler(pRc)
{
	for (const char* szExt : { "cga", "cgf", "skin" })
	{
		pRc->GetAssetManager()->RegisterDetailProvider(CollectCgfDetails, szExt);
	}

	pRc->GetAssetManager()->RegisterDetailProvider(CollectMtlDetails, "mtl");
	pRc->GetAssetManager()->RegisterDetailProvider(CollectXmlDetails, "xml");
	pRc->GetAssetManager()->RegisterDetailProvider(CollectCdfDetails, "cdf");
}

CMetadataCompiler::~CMetadataCompiler()
{
}

void CMetadataCompiler::BeginProcessing(const IConfig* pConfig)
{
}

void CMetadataCompiler::EndProcessing()
{
}

bool CMetadataCompiler::Process()
{
	const string sourceFilename = m_CC.GetSourcePath();
	const string metadataFilename = IAssetManager::GetMetadataFilename(sourceFilename);

	// If source file does not exist, ignore it.
	if (!FileUtil::FileExists(sourceFilename))
	{
		if (!m_CC.config->GetAsBool("skipmissing", false, true))
		{
			RCLogWarning("File does not exist: %s", sourceFilename.c_str());
		}
		return true;
	}

	if (m_CC.config->GetAsBool("stripMetadata", false, true))
	{
		return DeleteFileIfExists(metadataFilename);
	}

	std::vector<string> files;

	const char* szExt = PathUtil::GetExt(sourceFilename.c_str());
	if (stricmp(szExt, "dds") == 0)
	{
		files = FindDdsAssetFiles(sourceFilename);
	}
	else if (!stricmp(szExt, "cgf") || !stricmp(szExt, "cga") || !stricmp(szExt, "skin"))
	{
		// Do not create cryassets for lod files, but repair existent.
		if (IsCgfLod(m_CC.GetSourcePath()))
		{
			if (FileUtil::FileExists(metadataFilename.c_str()))
			{
				files.emplace_back(sourceFilename);
			}
			else
			{
				return true;
			}
		}
		else
		{
			files = FindCgfAssetFiles(sourceFilename);
		}
	}
	else
	{
		files.emplace_back(sourceFilename);
	}

	return m_pResourceCompiler->GetAssetManager()->SaveCryasset(m_CC.config, "", files, m_CC.GetOutputFolder());
}

const char* CMetadataCompiler::GetExt(int index) const
{
	switch (index)
	{
	case 0:
		return IAssetManager::GetExtension();
	default:
		return nullptr;
	}
}
