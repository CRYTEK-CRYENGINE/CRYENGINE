// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem\File\CryFile.h>  // Includes CryPath.h in correct order. 

struct IResourceCompiler;
class IConfig;
class XmlNodeRef;


// Fills xmlnode with details metatada properties for given asset data file.
// \return true if successfully completed, false if the file could not be recognized.
typedef bool(*FnDetailsProvider)(XmlNodeRef& xmlnode, const char* szFilename, IResourceCompiler* pRc);

struct IAssetManager
{
	virtual ~IAssetManager() {}

	//! Bind the file extension to the details provider.
	virtual void RegisterDetailProvider(FnDetailsProvider detailsProvider, const char* szExt) = 0;

	//! Save .cryasset file. The implementation calls the appropriate provider(s) to collect details of the asset data files.
	virtual bool SaveCryasset(const IConfig* const pConfig, const char* szSourceFilepath, size_t filesCount, const char** pFiles, const char* szOutputFolder) const = 0;

	bool SaveCryasset(const IConfig* const pConfig, const string& sourceFilepath, const std::vector<string>& files, const char* szOutputFolder = nullptr)
	{
		std::unique_ptr<const char*> pFiles(new const char*[files.size()]);
		for (size_t i = 0, n = files.size(); i < n; ++i)
		{
			pFiles.get()[i] = files[i].c_str();
		}
		return SaveCryasset(pConfig, sourceFilepath.c_str(), files.size(), pFiles.get(), szOutputFolder);
	}

	static const char* GetExtension()
	{
		return "cryasset";
	}

	// Builds .cryasset file name for the data filename.
	static string GetMetadataFilename(const string& filename)
	{
		//  Special handling of RC intermediate files: filename.$ext -> filename.$ext.$cryasset
		const char* szExt = PathUtil::GetExt(filename);
		if (szExt && szExt[0] == '$')
		{
			return  filename + ".$" + GetExtension();
		}

		return  filename + "." + GetExtension();
	}
};
