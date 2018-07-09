// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "AnimationCompiler.h"
#include "AnimationInfoLoader.h"
#include "SkeletonInfo.h"
#include "AnimationLoader.h"
#include "AnimationManager.h"
#include "IConfig.h"
#include "IAssetManager.h"
#include "TrackStorage.h"
#include "UpToDateFileHelpers.h"
#include "CGF/CGFSaver.h"

#include "IPakSystem.h"
#include "TempFilePakExtraction.h"
#include "FileUtil.h"
#include "MathHelpers.h"
#include "StringHelpers.h"
#include "StealingThreadPool.h"
#include "PakXmlFileBufferSource.h"
#include "DBATableEnumerator.h"
#include "../../../CryXML/ICryXML.h"
#include "Plugins/EditorAnimation/Shared/CompressionPresetTable.h"
#include "../CryEngine/Cry3DEngine/CGF/CGFLoader.h"
#include "../CryEngine/Cry3DEngine/CGF/ChunkData.h"
#include "../CryEngine/Cry3DEngine/CGF/ReadOnlyChunkFile.h"


// Returns -1 in case of fail. Would be nice to add this to CSkinningInfo.
static int32 FindAIMBlendIndexByAnimation(const CSkinningInfo& info, const string& animationPath)
{
	const size_t numBlends = info.m_AimDirBlends.size();
	for (size_t d = 0; d < numBlends; ++d)
	{
		const string& animToken = info.m_AimDirBlends[d].m_AnimToken;
		if (StringHelpers::ContainsIgnoreCase(animationPath, animToken))
		{
			return d;
		}
	}
	return -1;
}

static int32 FindLookBlendIndexByAnimation(const CSkinningInfo& info, const string& animationPath)
{
	const size_t numBlends = info.m_LookDirBlends.size();
	for (size_t d = 0; d < numBlends; ++d)
	{
		const string& animToken = info.m_LookDirBlends[d].m_AnimToken;
		if (StringHelpers::ContainsIgnoreCase(animationPath, animToken))
		{
			return d;
		}
	}
	return -1;
}

static bool IsLookAnimation(const CSkinningInfo& skeletonInfo, const string& animationPath)
{
	return FindLookBlendIndexByAnimation(skeletonInfo, animationPath) != -1;
}

static bool IsAimAnimation(const CSkinningInfo& skeletonInfo, const string& animationPath)
{
	if (FindAIMBlendIndexByAnimation(skeletonInfo, animationPath) != -1)
	{
		return true;
	}
	return StringHelpers::ContainsIgnoreCase(PathUtil::GetFile(animationPath), "AimPoses");
}

string UnifiedPath(const string& path)
{
	return PathUtil::ToUnixPath(StringHelpers::MakeLowerCase(path));
}


string RelativePath(const string& path, const string& relativeToPath)
{
	const auto fullpath = PathUtil::ToUnixPath(path);
	const auto rootDataFolder = PathUtil::ToUnixPath(PathUtil::AddSlash(relativeToPath));
	if (fullpath.length() > rootDataFolder.length() && strnicmp(fullpath.c_str(), rootDataFolder.c_str(), rootDataFolder.length()) == 0)
	{
		return fullpath.substr(rootDataFolder.length(), fullpath.length() - rootDataFolder.length());
	}
	return fullpath;
}


static bool CheckIfRefreshNeeded(const char* sourcePath, const char* destinationPath, FILETIME* sourceFileTimestamp = 0)
{
	const FILETIME sourceStamp = FileUtil::GetLastWriteFileTime(sourcePath);
	if (sourceFileTimestamp)
	{
		*sourceFileTimestamp = sourceStamp;
	}	

	if (!FileUtil::FileTimeIsValid(sourceStamp))
	{
		return true;
	}

	const FILETIME targetStamp = FileUtil::GetLastWriteFileTime(destinationPath);
	if (!FileUtil::FileTimeIsValid(targetStamp))
	{
		return true;
	}

	return !FileUtil::FileTimesAreEqual(targetStamp, sourceStamp);
}

static bool HasAnimationSettingsFileChanged(const string& animationSourceFilename, const string& animationTargetFilename)
{
	const string sourceAnimationSettingsFile = SAnimationDefinition::GetAnimationSettingsFilename(animationSourceFilename, SAnimationDefinition::ePASF_OVERRIDE_FILE);
	const string targetAnimationSettingsFile = SAnimationDefinition::GetAnimationSettingsFilename(animationTargetFilename, SAnimationDefinition::ePASF_UPTODATECHECK_FILE);
	const bool refreshNeeded = CheckIfRefreshNeeded(sourceAnimationSettingsFile, targetAnimationSettingsFile);
	return refreshNeeded;
}

//////////////////////////////////////////////////////////////////////////
class AutoDeleteFile
	: IResourceCompiler::IExitObserver
{
public:
	AutoDeleteFile(IResourceCompiler* pRc, const string& filename)
		: m_pRc(pRc)
		, m_filename(filename)
	{
		if (m_pRc)
		{
			m_pRc->AddExitObserver(this);
		}
	}

	virtual ~AutoDeleteFile()
	{
		Delete();
		if (m_pRc)
		{
			m_pRc->RemoveExitObserver(this);
		}
	}

	virtual void OnExit()
	{
		Delete();
	}

private:
	void Delete()
	{
		if (!m_filename.empty())
		{
			SetFileAttributesA(m_filename.c_str(), FILE_ATTRIBUTE_ARCHIVE);
			DeleteFile(m_filename.c_str());				
		}
	}

private:
	IResourceCompiler* m_pRc;
	string m_filename;
};


//////////////////////////////////////////////////////////////////////////
CAnimationConverter::CAnimationConverter(ICryXML* pXMLParser, IPakSystem* pPakSystem, IResourceCompiler* pRC)
	: m_pPakSystem(pPakSystem)
	, m_pXMLParser(pXMLParser)
	, m_config(0)
	, m_fancyAnimationIndex(0)
	, m_changedAnimationCount(0)
	, m_rc(pRC)
	, m_platform(-1)
	, m_compressionPresetTable(new SCompressionPresetTable())
{
}

void CAnimationConverter::Init(const ConverterInitContext& context)
{
	m_fancyAnimationIndex = 0;
	m_changedAnimationCount = 0;
	m_platform = -1;

	m_config = context.config;

	m_sourceGameFolderPath = context.config->GetAsString("sourceroot", "", "");

	m_configSubfolder = context.config->GetAsString("animConfigFolder", "Animations", "Animations");

	const bool bIgnorePresets = context.config->GetAsBool("ignorepresets", false, true);
	if (!bIgnorePresets)
	{
		const string compressionSettingsPath = UnifiedPath(PathUtil::Make(PathUtil::Make(m_sourceGameFolderPath, m_configSubfolder), "CompressionPresets.json"));
		if (FileUtil::FileExists(compressionSettingsPath))
		{
			if (!m_compressionPresetTable->Load(compressionSettingsPath.c_str()))
			{
				RCLogError("Failed to load compression presets table: %s", compressionSettingsPath.c_str());
			}
		}
	}

	RCLog("Looking for used skeletons...");
	std::set<string> usedSkeletons;
	{
		for (size_t i = 0; i < context.inputFileCount; ++i)
		{
			const char* const filename = context.inputFiles[i].m_sourceInnerPathAndName.c_str();
			string extension = PathUtil::GetExt(filename);
			if (StringHelpers::EqualsIgnoreCase(extension, "i_caf"))
			{
				const string animSettingsPath = context.config->GetAsString("animSettingsFile", "", "");
				if (!animSettingsPath.empty())
				{
					SAnimationDefinition::SetOverrideAnimationSettingsFilename(animSettingsPath);
				}				

				const string fullPath = PathUtil::Make(context.inputFiles[i].m_sourceLeftPath, context.inputFiles[i].m_sourceInnerPathAndName);
				if (m_sourceGameFolderPath.empty())
				{
					m_sourceGameFolderPath = PathHelpers::GetDirectory(PathHelpers::GetAbsolutePath(fullPath));
				}

				SAnimationDesc desc;
				bool bErrorReported = false;
				if (SAnimationDefinition::GetDescFromAnimationSettingsFile(&desc, &bErrorReported, 0, m_pPakSystem, m_pXMLParser, fullPath.c_str(), std::vector<string>()))
				{
					if (!desc.m_skeletonName.empty())
					{
						usedSkeletons.insert(desc.m_skeletonName.c_str());
					}
				}
			}
		}
	}

	if (m_sourceGameFolderPath.empty())
	{
		RCLogError("Failed to locate game folder.");
	}

	const int verbosityLevel = m_rc->GetVerbosityLevel();
	if (verbosityLevel > 0)
	{
		RCLog("Source game folder path: %s", m_sourceGameFolderPath.c_str());
	}

	InitSkeletonManager(usedSkeletons);

	if (!InLocalUpdateMode())
	{
		InitDbaTableEnumerator();
	}	
}

void CAnimationConverter::InitSkeletonManager(const std::set<string>& usedSkeletons)
{
	m_skeletonManager.reset(new SkeletonManager(m_pPakSystem, m_pXMLParser, m_rc));

	string skeletonListPath = UnifiedPath(PathUtil::Make(PathUtil::Make(m_sourceGameFolderPath, m_configSubfolder), "SkeletonList.xml"));
	m_skeletonManager->LoadSkeletonList(skeletonListPath, m_sourceGameFolderPath, usedSkeletons);
}

static string GetDbaTableFilename(const string& sourceGameFolderPath, const string& configSubfolder)
{
	const string filename = PathUtil::Make(PathUtil::Make(sourceGameFolderPath, configSubfolder), "DBATable.json");

	if (FileUtil::FileExists(filename.c_str()))
	{
		return filename;
	}
	
	const string obsoleteFilename = PathUtil::Make(PathUtil::Make(sourceGameFolderPath, configSubfolder), "DBATable.xml");

	if (FileUtil::FileExists(obsoleteFilename.c_str())) 
	{
		RCLogError(
			"CryENGINE 3.8.5 and higher don't support the DBATable.xml file. Instead, CryENGINE uses the\n"
			"DBATable.json file. Please refer to the online documentation for the conversion instructions:\n"
			"http://docs.cryengine.com/display/SDKDOC1/EaaS+3.7.0#EaaS3.7.0-DBATableConversion");
	}
	else
	{
		RCLogError("Missing DBA table file '%s'", filename.c_str());
	}
	return string();
}

void CAnimationConverter::InitDbaTableEnumerator()
{
	const string dbaTableFilename = GetDbaTableFilename(m_sourceGameFolderPath, m_configSubfolder);
	if (dbaTableFilename.empty())
	{
		return;
	}

	DBATableEnumerator* const dbaTable = new DBATableEnumerator();
	const string configFolderPath = PathUtil::Make(m_sourceGameFolderPath, m_configSubfolder);
	if (!dbaTable->LoadDBATable(configFolderPath, m_sourceGameFolderPath, m_pPakSystem, m_pXMLParser)) 
	{
		RCLogError("Failed to load DBA table file '%s'", dbaTableFilename.c_str());
	}
	m_dbaTableEnumerator.reset(dbaTable);
}


void CAnimationConverter::DeInit()
{
	RebuildDatabases();
}

static vector<string> GetJointNames(const CSkeletonInfo* skeleton)
{
	vector<string> result;
	for (int i = 0; i < skeleton->m_SkinningInfo.m_arrBonesDesc.size(); ++i) 
	{
		result.push_back(skeleton->m_SkinningInfo.m_arrBonesDesc[i].m_arrBoneName);
	}

	return result;
}

bool CAnimationConverter::RebuildDatabases()
{
	if (m_config->GetAsBool("SkipDba", false, true))
	{
		RCLog("/SkipDba specified. Skipping database update...");
		return true;
	}

	if (m_changedAnimationCount == 0)
	{
		RCLog("No animations changed. Skipping database update...");
		return true;
	}

	const bool bStreamPrepare = m_config->GetAsBool("dbaStreamPrepare", false, true);

	string targetGameFolderPath;
	{
		const string targetRoot = PathUtil::RemoveSlash(m_config->GetAsString("targetroot", "", ""));

		if (!targetRoot.empty())
		{
			targetGameFolderPath = PathUtil::ToDosPath(PathUtil::AddSlash(targetRoot));
		}
	}

	CAnimationManager animationManager;

	size_t totalInputSize = 0;
	size_t totalOutputSize = 0;
	size_t totalAnimCount = 0;

	std::set<string> unusedDBAs;
	{
		std::vector<string> allDBAs;

		FileUtil::ScanDirectory(targetGameFolderPath, "*.dba", allDBAs, true, string());
		for (size_t i = 0; i < allDBAs.size(); ++i)
		{
			unusedDBAs.insert(UnifiedPath(allDBAs[i]));
		}
	}

	RCLog("Rebuilding DBAs...");

	const PlatformInfo* pPlatformInfo = m_rc->GetPlatformInfo(m_platform);
	const bool bigEndianOutput = pPlatformInfo->bBigEndian;
	const int pointerSize = pPlatformInfo->pointerSize;

	const string dbaTableFilename = GetDbaTableFilename(m_sourceGameFolderPath, m_configSubfolder);
	if (dbaTableFilename.empty())
	{
		return false;
	}

	const size_t dbaCount = m_dbaTableEnumerator->GetDBACount();
	for (size_t dbaIndex = 0; dbaIndex < dbaCount; ++dbaIndex)
	{
		EnumeratedDBA dba;
		m_dbaTableEnumerator->GetDBA(&dba, dbaIndex);

		const string& dbaInnerPath = dba.innerPath;


		std::auto_ptr<CTrackStorage> storage;
		if (!dbaInnerPath.empty())
		{
			storage.reset(new CTrackStorage(bigEndianOutput));
		}

		std::vector<AnimationJob> animations;
		animations.resize(dba.animationCount);

		size_t dbaInputSize = 0;
		size_t dbaAnimCount = 0;

		for (size_t anim = 0; anim < dba.animationCount; ++anim)
		{
			EnumeratedCAF caf;
			m_dbaTableEnumerator->GetCAF(&caf, dbaIndex, anim);

			string compressedCAFPath = PathUtil::Make(targetGameFolderPath, caf.path);
			compressedCAFPath = PathUtil::ReplaceExtension(compressedCAFPath, "$caf"); // .caf -> .$caf

			if (!FileUtil::FileExists(compressedCAFPath.c_str()))
			{
				RCLogWarning("Compressed CAF '%s' not found in target folder '%s'. Please recompile '%s'.", 
					compressedCAFPath.c_str(), targetGameFolderPath.c_str(), caf.path.c_str());
				continue;
			}

			CChunkFile chunkFile;
			GlobalAnimationHeaderCAF globalAnimationHeader;
			if (!globalAnimationHeader.LoadCAF(compressedCAFPath.c_str(), eCAFLoadCompressedOnly, &chunkFile))
			{
				RCLogError("Failed to load a compressed animation '%s'.", compressedCAFPath.c_str());
				continue;
			}


			const string animationPath = UnifiedPath(caf.path);
			if (animationPath.empty())
			{
				RCLogError("Unexpected error with animation %s. Root animation folder is: %s", caf.path.c_str(), m_sourceGameFolderPath.c_str());
				continue;
			}

			const bool isAIM = chunkFile.FindChunkByType(ChunkType_GlobalAnimationHeaderAIM) != 0;
			if (isAIM)
			{
				GlobalAnimationHeaderAIM gaim;
				if (!gaim.LoadAIM(compressedCAFPath.c_str(), eCAFLoadCompressedOnly))
				{
						RCLogError("Unable to load AIM/LOOK: '%s'.", compressedCAFPath.c_str());
				}
				else if (animationPath != gaim.m_FilePath)
				{
					RCLogError("AIM loaded with wrong m_FilePath: '%s' (should be '%s').",
										 gaim.m_FilePath.c_str(), animationPath.c_str());
				}
				else if (!gaim.IsValid())
				{
					RCLogError("AIM header contains invalid values (NANs): '%s'.", compressedCAFPath.c_str());
				}
				else
				{
					if (!animationManager.AddAIMHeaderOnly(gaim))
					{
							RCLogWarning("CAF(AIM) file was added to more than one DBA: '%s'", animationPath.c_str());
					}
				}
				globalAnimationHeader.OnAimpose(); // we need this info to skip aim-poses in the IMG file
			}

			globalAnimationHeader.SetFilePathCAF(animationPath.c_str());
			const bool saveInDBA = !isAIM && !dbaInnerPath.empty() && !caf.skipDBA;

			if (saveInDBA)
			{
				const string dbaPath = UnifiedPath(dbaInnerPath);
				globalAnimationHeader.SetFilePathDBA(dbaPath);

				globalAnimationHeader.OnAssetLoaded();
				globalAnimationHeader.ClearAssetOnDemand(); 
			}
			else
			{
				globalAnimationHeader.OnAssetOnDemand(); 
				globalAnimationHeader.ClearAssetLoaded(); 
			}

			if (!animationManager.AddCAFHeaderOnly(globalAnimationHeader))
			{
				RCLogWarning("CAF file was added to more than one DBA: '%s'",
									 globalAnimationHeader.m_FilePath.c_str());
			}

			if (storage.get() && saveInDBA)
			{
				storage->AddAnimation(globalAnimationHeader);

				dbaInputSize += (size_t)FileUtil::GetFileSize(compressedCAFPath.c_str()); 
				++dbaAnimCount;
			}
		}

		{
			string unifiedPath = PathUtil::RemoveSlash(dbaInnerPath);
			UnifyPath(unifiedPath);
			std::set<string>::iterator it = unusedDBAs.find(unifiedPath);
			if (it != unusedDBAs.end())
			{
				unusedDBAs.erase(it);
			}
		}

		if (storage.get())
		{
			if (dbaAnimCount != 0)
			{
				const string dbaOutPath = PathUtil::Make(targetGameFolderPath, dbaInnerPath);
				storage->SaveDataBase905(dbaOutPath.c_str(), bStreamPrepare, pointerSize);

				size_t dbaOutputSize = 0;
				if (FileUtil::FileExists(dbaOutPath.c_str()))
				{
					dbaOutputSize = (size_t)FileUtil::GetFileSize(dbaOutPath.c_str());
				}

				RCLog("DBA %i KB -> %i KB (%02.1f%%) anims: %i '%s'",
							dbaInputSize / 1024, dbaOutputSize / 1024,
							double(dbaOutputSize) / dbaInputSize * 100.0, dbaAnimCount, dbaInnerPath.c_str());

				if (m_rc)
				{
					m_rc->AddInputOutputFilePair(dbaTableFilename, dbaOutPath.c_str());
				}

				totalInputSize += dbaInputSize;
				totalOutputSize += dbaOutputSize;
				totalAnimCount += dbaAnimCount;
				
				(void)totalInputSize;
				(void)totalOutputSize;
				(void)totalAnimCount;
			}
			else
			{
				RCLogWarning("No valid animations found which belong to the DBA '%s', so it's not created.", dbaInnerPath.c_str());
			}
		}
	}

	{
		string sourceFolder = m_sourceGameFolderPath;
		RCLog("Looking for non dba animations for creating animation .img files.");
		{
			std::vector<string> cafFileList;
			FileUtil::ScanDirectory(sourceFolder, "*.i_caf", cafFileList, true, "");

			for(size_t i = 0; i < cafFileList.size(); ++i)
			{
				const string& filePath = cafFileList[i];
				string animationPath = PathUtil::ReplaceExtension(filePath, "caf");
				UnifyPath(animationPath);

				string compressedCAFPath = PathUtil::Make(targetGameFolderPath, filePath);
				compressedCAFPath = PathUtil::ReplaceExtension(compressedCAFPath, "$caf"); // .caf -> .$caf

				if (!FileUtil::FileExists(compressedCAFPath.c_str()))
				{
					RCLogWarning("Compressed CAF '%s' not found in target folder '%s'. Please recompile it.", 
											 compressedCAFPath.c_str(), targetGameFolderPath.c_str());
					continue;
				}

				CChunkFile chunkFile;
				GlobalAnimationHeaderCAF globalAnimationHeader;
				if (!globalAnimationHeader.LoadCAF(compressedCAFPath.c_str(), eCAFLoadCompressedOnly, &chunkFile))
				{
					RCLogError("Failed to load a compressed animation '%s'.", compressedCAFPath.c_str());
					continue;
				}

				const string fullAnimationPath = PathUtil::Make(sourceFolder, filePath);
				SAnimationDesc animDesc;
				bool bErrorReported = false;
				bool bUsesNameContains = false;
				if (!SAnimationDefinition::GetDescFromAnimationSettingsFile(&animDesc, &bErrorReported, &bUsesNameContains, m_pPakSystem, m_pXMLParser, fullAnimationPath, vector<string>()))
				{
					if (!bErrorReported)
					{
						RCLogError("Failed to load animation settings for animation: %s", fullAnimationPath.c_str());
					}
					continue;
				}
				const CSkeletonInfo* pSkeleton = m_skeletonManager->LoadSkeleton(animDesc.m_skeletonName);
				if (!pSkeleton)
				{
					RCLogError("Failed to load skeleton with name '%s' for animation with name '%s'. If this animation is an aimpose or lookpose, it will not be correctly added to the .img files.", 
						animDesc.m_skeletonName.c_str(), filePath.c_str());
					continue;
				}
				if (bUsesNameContains && !SAnimationDefinition::GetDescFromAnimationSettingsFile(&animDesc, &bErrorReported, 0, m_pPakSystem, m_pXMLParser, fullAnimationPath, GetJointNames(pSkeleton)))
				{
					if (!bErrorReported)
					{
						RCLogError("Failed to load animation settings for animation: %s", fullAnimationPath.c_str());
					}
					continue;
				}

				const bool isAIM = IsAimAnimation(pSkeleton->m_SkinningInfo, animationPath);
				const bool isLook = IsLookAnimation(pSkeleton->m_SkinningInfo, animationPath);
				if (isLook || isAIM)
				{
					GlobalAnimationHeaderAIM gaim;
					if (!gaim.LoadAIM(compressedCAFPath.c_str(), eCAFLoadCompressedOnly))
					{
						RCLogError("Unable to load AIM/LOOK: '%s'.", compressedCAFPath.c_str());
					}
					else if (animationPath != gaim.m_FilePath)
					{
						RCLogError("AIM loaded with wrong m_FilePath:	'%s' (should be '%s').", 
												gaim.m_FilePath.c_str(), animationPath.c_str());
					}
					else
					{
						if (!animationManager.HasAIMHeader(gaim))
						{
							if (!animationManager.AddAIMHeaderOnly(gaim))
							{
								RCLogError("AIM header for '%s' already exists in CAnimationManager.", 
														animationPath.c_str());
							}
						}
					}
					globalAnimationHeader.OnAimpose(); // we need this info to skip aim-poses in the IMG file
				}
				else
				{
					globalAnimationHeader.SetFilePathCAF(animationPath.c_str());
					string dbaPath;

					if (m_dbaTableEnumerator.get())
					{
						if (const char* dbaPathFound = m_dbaTableEnumerator->FindDBAPath(animationPath.c_str(), animDesc.m_skeletonName.c_str(), animDesc.m_tags))
						{
							dbaPath = dbaPathFound;
						}
					}

					globalAnimationHeader.SetFilePathDBA(dbaPath);
					globalAnimationHeader.OnAssetOnDemand(); 
					globalAnimationHeader.ClearAssetLoaded();

					if (!animationManager.HasCAFHeader(globalAnimationHeader))
					{
						if (!dbaPath.empty())
						{
							RCLogError("Animation is expected to be added as a part of a DBA: %s", animationPath.c_str());
						}
						else
						{
							animationManager.AddCAFHeaderOnly(globalAnimationHeader);
						}
					}
				}
			}
		}
	}


	const string sAnimationsImgFilename = PathUtil::Make(targetGameFolderPath, "Animations\\Animations.img");
	RCLog("Saving %s...",sAnimationsImgFilename);
	const FILETIME latest = FileUtil::GetLastWriteFileTime(targetGameFolderPath);
	if (!animationManager.SaveCAFImage( sAnimationsImgFilename, latest, bigEndianOutput))
	{
		RCLogError("Error saving Animations.img");
		return false;
	}

	if (m_rc)
	{
		m_rc->AddInputOutputFilePair(dbaTableFilename.c_str(), sAnimationsImgFilename);
	}

	// Check if it can be skipped so it won't accidentally log errors.
	if (!animationManager.CanBeSkipped())
	{
		const string sDirectionalBlendsImgFilename = PathUtil::Make(targetGameFolderPath, "Animations\\DirectionalBlends.img");
		RCLog("Saving %s...", sDirectionalBlendsImgFilename);
		if (!animationManager.SaveAIMImage(sDirectionalBlendsImgFilename, latest, bigEndianOutput))
		{
			RCLogError("Error saving DirectionalBlends.img");
			return false;
		}
		if (m_rc)
		{
			m_rc->AddInputOutputFilePair(dbaTableFilename.c_str(), sDirectionalBlendsImgFilename);
		}
	}

	if (!unusedDBAs.empty())
	{
		RCLog("Following DBAs are not used anymore:");

		for (std::set<string>::iterator it = unusedDBAs.begin(); it != unusedDBAs.end(); ++it)
		{
			RCLog("    %s", it->c_str());
			if (m_rc)
			{
				const string removedFile = PathUtil::Make(targetGameFolderPath, *it);
				m_rc->MarkOutputFileForRemoval(removedFile.c_str());
			}
		}
	}

	return true;
}

void CAnimationConverter::Release()
{
	delete this;
}

ICompiler* CAnimationConverter::CreateCompiler()
{
	return new CAnimationCompiler(this);
}

bool CAnimationConverter::SupportsMultithreading() const
{
  return true;
}

void CAnimationConverter::IncrementChangedAnimationCount()
{
	InterlockedIncrement(&m_changedAnimationCount);
}

int CAnimationConverter::IncrementFancyAnimationIndex()
{
	return InterlockedIncrement(&m_fancyAnimationIndex);
}

bool CAnimationConverter::InLocalUpdateMode() const
{
	return m_config->GetAsBool("SkipDba", false, true);
}

void CAnimationConverter::SetPlatform(int platform)
{
	if (m_platform < 0)
	{
		m_platform = platform;
	}
}

//////////////////////////////////////////////////////////////////////////
CAnimationCompiler::CAnimationCompiler(CAnimationConverter* converter) 
: m_pXMLParser(converter->GetXMLParser())
, m_converter(converter)
{
}

CAnimationCompiler::~CAnimationCompiler()
{
}

string CAnimationCompiler::GetOutputFileNameOnly() const
{
	const bool localUpdateMode = Converter()->InLocalUpdateMode();

	const string overwrittenFilename = m_CC.config->GetAsString("overwritefilename", m_CC.sourceFileNameOnly.c_str(), m_CC.sourceFileNameOnly.c_str());

	const string ext = PathUtil::GetExt(overwrittenFilename);
	if (StringHelpers::EqualsIgnoreCase(ext, "i_caf"))
	{
		if (localUpdateMode)
		{
			return PathUtil::ReplaceExtension(overwrittenFilename, string("caf"));
		}
		else
		{
			return PathUtil::ReplaceExtension(overwrittenFilename, string("$caf"));
		}
	}
	else
	{
		return PathUtil::ReplaceExtension(overwrittenFilename, string("$")+ext);
	}
}

string CAnimationCompiler::GetOutputPath() const
{
	return PathUtil::Make(m_CC.GetOutputFolder(), GetOutputFileNameOnly());
}

// ---------------------------------------------------------------------------

static void ProcessAnimationJob(AnimationJob* job);

static bool PrepareAnimationJob(
	AnimationJob* job,
	const char* sourcePath,
	const char* destinationPath,
	const char* sourceGameFolderPath,
	const SAnimationDesc& desc,
	const CSkeletonInfo* skeleton,
	const char* dbaPath,
	CAnimationCompiler* compiler)
{
	const ConvertContext& context = *static_cast<const ConvertContext*>(compiler->GetConvertContext());

	bool bDebugCompression = context.config->GetAsBool("debugcompression", false, true);
	const bool bigEndianOutput = context.pRC->GetPlatformInfo(context.platform)->bBigEndian;
	bool bAlignTracks = context.config->GetAsBool("cafAlignTracks", false, true);

	string animationPath = UnifiedPath(RelativePath(sourcePath, sourceGameFolderPath));
	if (StringHelpers::Equals(PathUtil::GetExt(animationPath), "i_caf"))
	{
		animationPath = PathUtil::ReplaceExtension(animationPath, "caf");
	}

	if (animationPath.empty())
	{
		RCLogError("Skipping animation %s because it's not in a subtree of the root animation folder %s", sourcePath, sourceGameFolderPath);
		return false;
	}

	job->m_compiler = compiler;
	job->m_sourcePath = sourcePath;
	job->m_destinationPath = destinationPath;
	string animExt = PathUtil::GetExt(job->m_destinationPath);
	job->m_intermediatePath = PathUtil::ReplaceExtension(job->m_destinationPath, string("$")+animExt); // e.g. .caf -> .$caf
	job->m_animationPath = animationPath;
	UnifyPath(job->m_animationPath);
	SAnimationDesc defaultAnimDesc;
	job->m_animDesc = desc;
	job->m_bigEndianOutput = bigEndianOutput;
	job->m_debugCompression = bDebugCompression;
	job->m_alignTracks = bAlignTracks;

	const bool bRefresh = context.config->GetAsBool("refresh", false, true);

	job->m_skeleton = skeleton;
	job->m_bigEndianOutput = bigEndianOutput;

	const bool bLocalUpdateMode = compiler->Converter()->InLocalUpdateMode();
	const bool bUseDBA = (dbaPath != NULL && dbaPath[0] != '\0' && !job->m_animDesc.m_bSkipSaveToDatabase && !bLocalUpdateMode);
	if (bUseDBA)
	{
		job->m_dbaPath = UnifiedPath(dbaPath);
	}
	else
	{
		job->m_dbaPath = "";
	}

	job->m_writeIntermediateFile = !bLocalUpdateMode;
	job->m_writeDestinationFile = !bUseDBA;


	if (bRefresh ||
		(job->m_writeIntermediateFile && CheckIfRefreshNeeded(job->m_sourcePath.c_str(), job->m_intermediatePath.c_str())) ||
		(job->m_writeDestinationFile && CheckIfRefreshNeeded(job->m_sourcePath.c_str(), job->m_destinationPath.c_str())) ||
		(job->m_writeIntermediateFile && HasAnimationSettingsFileChanged(job->m_sourcePath, job->m_intermediatePath)) ||
		(job->m_writeDestinationFile && HasAnimationSettingsFileChanged(job->m_sourcePath, job->m_destinationPath)))
	{
		job->m_fileChanged = true;
	}

	return true;
}

static bool GetFromAnimSettings(SAnimationDesc* desc, const CSkeletonInfo** skeleton, bool* pbErrorReported, const char* sourcePath, const char* animationName, IPakSystem* pakSystem, ICryXML* xmlParser, SkeletonManager* skeletonManager)
{
	bool usesNameContains = false;
	if (!SAnimationDefinition::GetDescFromAnimationSettingsFile(desc, pbErrorReported, &usesNameContains, pakSystem, xmlParser, sourcePath, std::vector<string>()))
	{
		return false;
	}

	if (desc->m_skeletonName.empty())
	{
		RCLogError("No skeleton alias specified for animation: '%s'", sourcePath);
		*pbErrorReported = true;
		return false;
	}

	*skeleton = skeletonManager->FindSkeleton(desc->m_skeletonName);
	if (!*skeleton)
	{
		RCLogError("Missing skeleton alias '%s' for animation '%s'", desc->m_skeletonName.c_str(), sourcePath);
		*pbErrorReported = true;
		return false;
	}

	if (usesNameContains)
	{
		if (!SAnimationDefinition::GetDescFromAnimationSettingsFile(desc, pbErrorReported, 0, pakSystem, xmlParser, sourcePath, GetJointNames(*skeleton)))
		{
			return false;
		}
	}
	return true;
}


void CAnimationCompiler::GetFilenamesForUpToDateCheck(std::vector<string>& upToDateSrcFilenames, std::vector<string>& upToDateCheckFilenames)
{
	upToDateSrcFilenames.clear();
	upToDateCheckFilenames.clear();

	const string sourcePath = m_CC.GetSourcePath();
	const string outputPath = GetOutputPath();

	// "Main" file
	upToDateSrcFilenames.push_back(sourcePath);
	upToDateCheckFilenames.push_back(outputPath);

	const string sourcePathExt = PathUtil::GetExt(sourcePath);
	const bool isCafFile =
		StringHelpers::EqualsIgnoreCase(sourcePathExt, "caf") ||
		StringHelpers::EqualsIgnoreCase(sourcePathExt, "i_caf");

	if (isCafFile)
	{
		const string sourceAnimSettingsFilename = SAnimationDefinition::GetAnimationSettingsFilename(sourcePath, SAnimationDefinition::ePASF_OVERRIDE_FILE);
		if (FileUtil::FileExists(sourceAnimSettingsFilename))
		{
			const string animationSettingsFilenameCheck = SAnimationDefinition::GetAnimationSettingsFilename(outputPath, SAnimationDefinition::ePASF_UPTODATECHECK_FILE);

			upToDateSrcFilenames.push_back(sourceAnimSettingsFilename);
			upToDateCheckFilenames.push_back(animationSettingsFilenameCheck);
		}
	}
}


bool CAnimationCompiler::CheckIfAllFilesAreUpToDate(const std::vector<string>& upToDateSrcFilenames, const std::vector<string>& upToDateCheckFilenames)
{
	const int verbosityLevel = m_CC.pRC->GetVerbosityLevel();

	bool bAllFileTimesMatch = true;
	for (size_t i = 0; i < upToDateSrcFilenames.size() && bAllFileTimesMatch; ++i)
	{
		if (upToDateCheckFilenames[i].empty())
		{
			bAllFileTimesMatch = false;
		}
		else
		{
			bAllFileTimesMatch = UpToDateFileHelpers::FileExistsAndUpToDate(upToDateCheckFilenames[i], upToDateSrcFilenames[i]);
		}
	}

	if (bAllFileTimesMatch)
	{
		for (size_t i = 0; i < upToDateSrcFilenames.size(); ++i)
		{
			m_CC.pRC->AddInputOutputFilePair(upToDateSrcFilenames[i], GetOutputPath());
		}

		if (verbosityLevel > 0)
		{
			if (upToDateSrcFilenames.size() == 1)
			{
				RCLog("Skipping %s: File %s is up to date.", m_CC.GetSourcePath().c_str(), upToDateCheckFilenames[0].c_str());
			}
			else
			{
				RCLog("Skipping %s:", m_CC.GetSourcePath().c_str());
				for (size_t i = 0; i < upToDateSrcFilenames.size(); ++i)
				{
					RCLog("  File %s is up to date", upToDateCheckFilenames[i].c_str());
				}
			}
		}
		return true;
	}

	return false;
}


// Update input-output file list, set source's time & date to output files.
void CAnimationCompiler::HandleUpToDateCheckFilesOnReturn(const std::vector<string>& upToDateSrcFilenames, const std::vector<string>& upToDateCheckFilenames)
{
	const int verbosityLevel = m_CC.pRC->GetVerbosityLevel();

	// 1) we need to call AddInputOutputFilePair() for each up-to-date-check file
	// 2) we need to enforce that every up-to-date-check file exists
	//    (except the main up-to-date-check file which expected to be created by Process())

	for (size_t i = 0; i < upToDateSrcFilenames.size(); ++i)
	{
		m_CC.pRC->AddInputOutputFilePair(upToDateSrcFilenames[i], GetOutputPath());

		if (!upToDateCheckFilenames[i].empty())
		{
			if (i > 0 && !FileUtil::FileExists(upToDateCheckFilenames[i].c_str()))
			{
				if (verbosityLevel > 0)
				{
					RCLog("Creating file '%s' (used to check if '%s' is up to date).", upToDateCheckFilenames[i].c_str(), m_CC.GetSourcePath().c_str());
				}
				FILE* const pEmptyFile = fopen(upToDateCheckFilenames[i].c_str(), "wt");
				if (pEmptyFile)
				{
					fclose(pEmptyFile);
				}
				else
				{
					RCLogWarning("Failed to create file '%s' (used to check if '%s' is up to date).", upToDateCheckFilenames[i].c_str(), m_CC.GetSourcePath().c_str());
				}
			}

			UpToDateFileHelpers::SetMatchingFileTime(upToDateCheckFilenames[i].c_str(), upToDateSrcFilenames[i].c_str());
		}
	}
}


static void ProcessAnimationJob(AnimationJob* job);

bool CAnimationCompiler::Process() 
{
	MathHelpers::AutoFloatingPointExceptions autoFpe(0);  // TODO: it's better to replace it by autoFpe(~(_EM_INEXACT | _EM_UNDERFLOW)). it was tested and works.

	Converter()->SetPlatform(m_CC.platform); // hackish

	const int verbosityLevel = m_CC.pRC->GetVerbosityLevel();

	std::vector<string> upToDateSrcFilenames;
	std::vector<string> upToDateCheckFilenames;
	GetFilenamesForUpToDateCheck(upToDateSrcFilenames, upToDateCheckFilenames);

	if (!m_CC.bForceRecompiling && CheckIfAllFilesAreUpToDate(upToDateSrcFilenames, upToDateCheckFilenames))
	{
		return true;
	}

	if (!StringHelpers::EqualsIgnoreCase(m_CC.converterExtension, "i_caf"))
	{
		RCLogError("Expected i_caf file as input");
		return false;
	}

	const string& sourceGameFolderPath = Converter()->GetSourceGameFolderPath();
	const string fileName = UnifiedPath(RelativePath(m_CC.GetSourcePath(), sourceGameFolderPath));
	const string animationName = PathUtil::ReplaceExtension(fileName, "caf");
	if (fileName.empty())
	{
		RCLogError(
			"Animation %s cannot be processed, because it's not inside of a subtree of the root animation folder %s",
			m_CC.GetSourcePath().c_str(), sourceGameFolderPath.c_str());
		return false;
	}

	const string outputPath = PathUtil::ReplaceExtension(GetOutputPath(), "caf");

	if (verbosityLevel > 0)
	{
		RCLog("sourceGameFolderPath: %s", sourceGameFolderPath.c_str());
		RCLog("outputPath: %s", outputPath.c_str());
	}

	SAnimationDesc animDesc;
	const CSkeletonInfo* skeleton = 0;

	const string& sourcePath = m_CC.GetSourcePath();

	bool bErrorReported = false;

	string dbaPath;
	animDesc.m_bSkipSaveToDatabase = true;

	if (!GetFromAnimSettings(&animDesc, &skeleton, &bErrorReported,
		sourcePath.c_str(), animationName.c_str(),
		Converter()->GetPakSystem(),
		Converter()->GetXMLParser(),
		Converter()->m_skeletonManager.get()))
	{
		if (!bErrorReported)
		{
			RCLogError("Can't load animations settings for animation: %s", sourcePath.c_str());
		}
		return false;
	}
	
	if (!skeleton)
	{
		RCLogError("Unable to load skeleton for animation: %s", sourcePath.c_str());
		return false;
	}

	const bool isAIM = IsAimAnimation(skeleton->m_SkinningInfo, animationName.c_str());
	const bool isLook = IsLookAnimation(skeleton->m_SkinningInfo, animationName.c_str());
	if (!(isAIM || isLook) && !Converter()->InLocalUpdateMode())
	{
		const char* dbaPathFromTable = 0;	
		if (Converter()->m_dbaTableEnumerator.get())
		{
			dbaPathFromTable = Converter()->m_dbaTableEnumerator->FindDBAPath(animationName, animDesc.m_skeletonName.c_str(), animDesc.m_tags);
		}

		if (dbaPathFromTable)
		{
			dbaPath = dbaPathFromTable;
			animDesc.m_bSkipSaveToDatabase = false;
		}
		else
		{
			dbaPath.clear();
			animDesc.m_bSkipSaveToDatabase = true;
		}
	}		
	else
	{
		dbaPath.clear();
		animDesc.m_bSkipSaveToDatabase = true;
	}

	if (isAIM || isLook)
	{
		// aim and look caf-s are collection of posses and should not be compressed
		animDesc.oldFmt.m_CompressionQuality = 0;
		animDesc.oldFmt.m_fPOS_EPSILON = 0.0f;
		animDesc.oldFmt.m_fROT_EPSILON = 0.0f;
		animDesc.oldFmt.m_fSCL_EPSILON = 0.0f;
		animDesc.m_bNewFormat = false;
	}
	else if (Converter()->m_compressionPresetTable.get())
	{
		const SCompressionPresetEntry* const preset = Converter()->m_compressionPresetTable.get()->FindPresetForAnimation(animationName.c_str(), animDesc.m_tags, animDesc.m_skeletonName.c_str());
		if (preset)
		{
			const bool bDebugCompression = m_CC.config->GetAsBool("debugcompression", false, true);
			if (bDebugCompression)
			{
				RCLog("Applying compression preset '%s' to animation '%s'", preset->name.c_str(), animationName.c_str());
			}

			animDesc.oldFmt.m_CompressionQuality = preset->settings.m_compressionValue;
			animDesc.oldFmt.m_fPOS_EPSILON = preset->settings.m_positionEpsilon;
			animDesc.oldFmt.m_fROT_EPSILON = preset->settings.m_rotationEpsilon;
			animDesc.oldFmt.m_fSCL_EPSILON = preset->settings.m_scaleEpsilon;
			animDesc.m_perBoneCompressionDesc.clear();
			for (size_t i = 0; i < preset->settings.m_controllerCompressionSettings.size(); ++i)
			{
				const string& jointName = preset->settings.m_controllerCompressionSettings[i].first;
				const SControllerCompressionSettings& presetJointSettings = preset->settings.m_controllerCompressionSettings[i].second;
				const SBoneCompressionValues::EDelete jointDeletionFlag =
					(presetJointSettings.state == eCES_ForceDelete) ? SBoneCompressionValues::eDelete_Yes : 
					(presetJointSettings.state == eCES_ForceKeep) ? SBoneCompressionValues::eDelete_No : 
					SBoneCompressionValues::eDelete_Auto;

				SBoneCompressionDesc jointSettings;
				jointSettings.m_namePattern = jointName;
				jointSettings.oldFmt.m_mult = presetJointSettings.multiply;
				jointSettings.m_eDeletePos = jointDeletionFlag;
				jointSettings.m_eDeleteRot = jointDeletionFlag;
				jointSettings.m_eDeleteScl = jointDeletionFlag;
				animDesc.m_perBoneCompressionDesc.push_back(jointSettings);
			}

			// TODO: Using old format for now, to keep existing data working
			// should transition to a newer one.
			animDesc.m_bNewFormat = false;
		}
	}

	AnimationJob job;

	if (PrepareAnimationJob(&job,
		sourcePath.c_str(),
		outputPath.c_str(),
		sourceGameFolderPath.c_str(),
		animDesc,
		skeleton,
		dbaPath,
		this))
	{
		ProcessAnimationJob(&job);
	}

	const bool bOk = (job.m_resSize != 0);
	if (bOk)
	{
		HandleUpToDateCheckFilesOnReturn(upToDateSrcFilenames, upToDateCheckFilenames);
	}
	return bOk;
}

static void ProcessAnimationJob(AnimationJob* job)
{
	CAnimationConverter* converter = job->m_compiler->Converter();
	const ConvertContext& context = *static_cast<const ConvertContext*>(job->m_compiler->GetConvertContext());
	const bool bDebugCompression = job->m_debugCompression;

	const SPlatformAnimationSetup* platform = &converter->GetPlatformSetup();
	const SAnimationDesc* animDesc = &job->m_animDesc;

	int processStartTime = GetTickCount();

	// Full or incremental build without specified animation
	const bool fileChanged = job->m_fileChanged;
	const bool writeIntermediate = job->m_writeIntermediateFile;
	const bool writeDest = job->m_writeDestinationFile;
	const string& sourcePath = job->m_sourcePath;
	const string& intermediatePath = job->m_intermediatePath;
	const string& destPath = job->m_destinationPath;
	const string& reportFile = writeDest ? destPath : intermediatePath;

	const bool isAIM = IsAimAnimation(job->m_skeleton->m_SkinningInfo, job->m_animationPath);
	const bool isLook = IsLookAnimation(job->m_skeleton->m_SkinningInfo, job->m_animationPath);

	bool failedToLoadCompressed = false;

	GlobalAnimationHeaderAIM gaim;
	if (isAIM || isLook)
	{
		if (!fileChanged)
		{
			// trying to load processed AIM from .$caf-file
			if (!gaim.LoadAIM(intermediatePath.c_str(), eCAFLoadCompressedOnly))
			{
				RCLogError("Unable to load AIM: %s", intermediatePath.c_str());
				failedToLoadCompressed = true;
				gaim = GlobalAnimationHeaderAIM();
			}
			else if (job->m_animationPath != gaim.m_FilePath)
			{
				RCLogError("AIM loaded with wrong m_FilePath: \"%s\" (should be \"%s\")", gaim.m_FilePath.c_str(), job->m_animationPath.c_str());
				failedToLoadCompressed = true;
				gaim = GlobalAnimationHeaderAIM();
			}
		}

		if (fileChanged || failedToLoadCompressed)
		{
			gaim.SetFilePath(job->m_animationPath.c_str());
			if (!gaim.LoadAIM(sourcePath.c_str(), eCAFLoadUncompressedOnly))
			{
				RCLogError("Unable to load AIM: %s", sourcePath.c_str());
				return;
			}

			CAnimationCompressor compressor(*job->m_skeleton);
			compressor.SetIdentityLocator(gaim);

			if (isAIM)
			{
				if (!compressor.ProcessAimPoses(false, gaim))
				{
					RCLogError("Failed to process AIM pose for file: '%s'.", sourcePath.c_str());
					return;
				}

				if (!gaim.IsValid())
				{
					RCLogError("ProcessAimPoses outputs NANs for file: '%s'.", sourcePath.c_str());
					return;
				}
			}

			if (isLook)
			{
				if (!compressor.ProcessAimPoses(true, gaim))
				{
					RCLogError("Failed to process AIM pose for file: '%s'.", sourcePath.c_str());
					return;
				}
				if (!gaim.IsValid())
				{
					RCLogError("ProcessAimPoses outputs NANs for file: '%s'.", sourcePath.c_str());
					return;
				}
			}
		}
	}

	CAnimationCompressor compressor(*job->m_skeleton);

	if (!fileChanged && !failedToLoadCompressed)
	{
		// update mode
		const char* loadFrom = writeIntermediate ? intermediatePath.c_str() : destPath.c_str();
		if (!compressor.LoadCAF(loadFrom, *platform, *animDesc, eCAFLoadCompressedOnly))
		{
			RCLogError("Unable to load compressed animation from %s", loadFrom);
			failedToLoadCompressed = true;
			compressor = CAnimationCompressor(*job->m_skeleton);
		}
		compressor.m_GlobalAnimationHeader.SetFilePathCAF(job->m_animationPath);
		if (isAIM || isLook)
		{
			compressor.m_GlobalAnimationHeader.OnAimpose();
		}
	}

	if (fileChanged || failedToLoadCompressed)
	{
		if (failedToLoadCompressed)
		{
			RCLog("Trying to recompile animation %s", sourcePath.c_str());
		}
		// rebuild animation
		if (!compressor.LoadCAF(sourcePath.c_str(), *platform, *animDesc, eCAFLoadUncompressedOnly))
		{
			// We have enough logging in compressor
			return;
		}
		compressor.EnableTrackAligning(job->m_alignTracks);
		compressor.m_GlobalAnimationHeader.SetFilePathCAF(job->m_animationPath);
		if (!compressor.ProcessAnimation(bDebugCompression))
		{
			RCLogError("Error in processing %s", sourcePath.c_str());
			return;
		}

		compressor.m_GlobalAnimationHeader.m_nCompressedControllerCount = compressor.m_GlobalAnimationHeader.CountCompressedControllers();
		
		if (isAIM || isLook)
		{
			compressor.m_GlobalAnimationHeader.OnAimpose(); // we need this info to skip aim-poses in the IMG file
		}

		// Create the directory (including intermediate ones), if not already there.
		if (!FileUtil::EnsureDirectoryExists(PathHelpers::GetDirectory(job->m_destinationPath).c_str()))
		{
			RCLogError("Failed to create directory for '%s'.", job->m_destinationPath.c_str());
			return;
		}

		const FILETIME sourceWriteTime = FileUtil::GetLastWriteFileTime(sourcePath.c_str());
		if (writeIntermediate)
		{
			GlobalAnimationHeaderAIM* gaimToSave = (isAIM || isLook) ? &gaim : 0;
			if (gaimToSave && !gaimToSave->IsValid())
			{
				RCLogError("Trying to save AIM header with invalid values (NANs): '%s'.", intermediatePath.c_str());
				return;
			}
			job->m_resSize = compressor.SaveOnlyCompressedChunksInFile(intermediatePath.c_str(), sourceWriteTime, gaimToSave, true, false); // little-endian format
			context.pRC->AddInputOutputFilePair(sourcePath, intermediatePath);
		}

		if (writeDest)
		{
			job->m_resSize = compressor.SaveOnlyCompressedChunksInFile(destPath.c_str(), sourceWriteTime, 0, false, job->m_bigEndianOutput);
			context.pRC->AddInputOutputFilePair(sourcePath, destPath);
			context.pRC->GetAssetManager()->SaveCryasset(context.config, sourcePath, { destPath });
		}

		converter->IncrementChangedAnimationCount();
	}
	else
	{
		if (writeIntermediate)
		{
			job->m_resSize = (size_t)FileUtil::GetFileSize(intermediatePath.c_str());
		}

		if (writeDest)
		{
			job->m_resSize = (size_t)FileUtil::GetFileSize(destPath.c_str());
		}
	}

	if (!job->m_dbaPath.empty())
	{
		compressor.m_GlobalAnimationHeader.SetFilePathDBA(job->m_dbaPath);
	}

	if (context.pRC->GetVerbosityLevel() >= 1)
	{
		const int durationMS = GetTickCount() - processStartTime;

		const char* const animPath = compressor.m_GlobalAnimationHeader.m_FilePath.c_str();
		const char* const dbaPath = compressor.m_GlobalAnimationHeader.m_FilePathDBA.c_str();
		const uint32 crc32 = compressor.m_GlobalAnimationHeader.m_FilePathDBACRC32;
		const size_t compressedControllerCount = compressor.m_GlobalAnimationHeader.m_nCompressedControllerCount;

		char processTime[32];
		if (fileChanged || failedToLoadCompressed )
		{
			cry_sprintf(processTime, "in %.1f sec", float(durationMS) * 0.001f);
		}
		else
		{
			cry_strcpy(processTime, "is up to date");
		}
		const int fancyAnimationIndex = converter->IncrementFancyAnimationIndex();
		const char* const aimState = isAIM || isLook ? "AIM" : "   ";
		RCLog("CAF-%04d %13s %s 0x%08X ctrls:%03d %s %s", fancyAnimationIndex, processTime, aimState, crc32, compressedControllerCount, animPath, dbaPath);
	}
}

void CAnimationCompiler::Release()
{
	delete this;
}
