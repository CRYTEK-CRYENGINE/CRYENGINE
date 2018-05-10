// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ParamLoader.h"

#include "CharacterManager.h"
#include "Model.h"
#include "FacialAnimation/FacialModel.h"
#include "FacialAnimation/FaceEffectorLibrary.h"
#include <CryCore/Platform/IPlatformOS.h>

// gcc only allows function attributes at the definition not the declaration
void Warning(const char* calFileName, EValidatorSeverity severity, const char* format, ...) PRINTF_PARAMS(3, 4);

void Warning(const char* calFileName, EValidatorSeverity severity, const char* format, ...)
{
	if (!g_pISystem || !format)
		return;

	va_list ArgList;
	va_start(ArgList, format);
	g_pISystem->WarningV(VALIDATOR_MODULE_ANIMATION, severity, VALIDATOR_FLAG_FILE, calFileName, format, ArgList);
	va_end(ArgList);
}

namespace
{
struct FindByAliasName
{
	uint32 fileCrc;

	bool operator()(const SAnimFile& animFile) const
	{
		return fileCrc == animFile.m_crcAnim;
	}

	bool operator()(const CAnimationSet::FacialAnimationEntry& facialAnimEntry) const
	{
		return fileCrc == facialAnimEntry.crc;
	}
};

void InitalizeBlendRotJointIndices(DynArray<DirectionalBlends>& directionalBlendArray, const DynArray<SJointsAimIK_Rot>& rotationsArray)
{
	const uint32 directionalBlendsCount = directionalBlendArray.size();
	for (uint32 i = 0; i < directionalBlendsCount; ++i)
	{
		DirectionalBlends& directionalBlend = directionalBlendArray[i];
		const uint32 rotationJointsCount = rotationsArray.size();
		for (uint32 r = 0; r < rotationJointsCount; ++r)
		{
			const SJointsAimIK_Rot& jointAimIkRot = rotationsArray[r];
			const int16 jointIndex = jointAimIkRot.m_nJointIdx;
			if (jointIndex == directionalBlend.m_nParaJointIdx)
			{
				directionalBlend.m_nRotParaJointIdx = r;
			}
			if (jointIndex == directionalBlend.m_nStartJointIdx)
			{
				directionalBlend.m_nRotStartJointIdx = r;
			}
		}
	}
}

bool FindJointInParentHierarchy(const CDefaultSkeleton* const pDefaultSkeleton, const int16 jointIdToSearch, const int16 jointIdToStartSearchFrom)
{
	CRY_ASSERT(pDefaultSkeleton);
	CRY_ASSERT(0 <= jointIdToSearch);
	CRY_ASSERT(0 <= jointIdToStartSearchFrom);

	int16 currentJoint = jointIdToStartSearchFrom;
	while (currentJoint != -1)
	{
		if (currentJoint == jointIdToSearch)
		{
			return true;
		}
		currentJoint = pDefaultSkeleton->GetJointParentIDByID(currentJoint);
	}

	return false;
}
}

CParamLoader::CParamLoader() : m_pDefaultSkeleton(nullptr)
{
	m_parsedLists.reserve(256);
}

CParamLoader::~CParamLoader(void)
{
	ClearLists();
}

// for a given list id (of an already loaded list) return all list i
bool CParamLoader::BuildDependencyList(int32 rootListID, DynArray<uint32>& listIDs)
{
	SAnimListInfo& animList = m_parsedLists[rootListID];

	int32 amountDep = animList.dependencies.size();
	for (int32 i = 0; i < amountDep; i++)
	{
		BuildDependencyList(animList.dependencies[i], listIDs);
	}
	listIDs.push_back(rootListID);
	return true;
}

int32 CParamLoader::ListProcessed(const char* paramFileName)
{
	// check if List has already been processed
	int32 nListIDs = m_parsedLists.size();
	for (int32 listID = 0; listID < nListIDs; listID++)
	{
		if (strcmp(m_parsedLists[listID].fileName, paramFileName) == 0)
		{
			return listID;
		}
	}
	return -1;
}

#define MAX_STRING_LENGTH (256)

// finds the first occurence of
CAF_ID CParamLoader::MemFindFirst(const char** ppAnimPath, const char* szMask, uint32 crcFolder, CAF_ID nCafID)
{
	LOADING_TIME_PROFILE_SECTION(g_pISystem);

	(*ppAnimPath) = NULL;

	AnimSearchHelper::TIndexVector* vect = g_AnimationManager.m_animSearchHelper.GetAnimationsVector(crcFolder);
	if (vect && nCafID.m_nType == 0)
	{
		for (size_t i = nCafID.m_nCafID, end = vect->size(); i < end; ++i)
		{
			GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[(*vect)[i]];
			stack_string strCAFName = PathUtil::GetFile(rCAF.GetFilePath());

			if (PathUtil::MatchWildcard(strCAFName.c_str(), szMask))
			{
				(*ppAnimPath) = rCAF.GetFilePath();

				return CAF_ID(i, 0);
			}
		}
	}

	return CAF_ID(-1, -1);
}

void CParamLoader::ExpandWildcardsForPath(SAnimListInfo& animList, const char* szMask, uint32 crcFolder, const char* szAnimNamePre, const char* szAnimNamePost)
{
	const char* szAnimPath;
	CAF_ID nCafID = MemFindFirst(&szAnimPath, szMask, crcFolder, CAF_ID(0, 0));
	if (nCafID.m_nCafID != -1)
	{
		do
		{
			stack_string animName = szAnimNamePre;
			animName.append(PathUtil::GetFileName(szAnimPath).c_str());
			if (szAnimNamePost)
			{
				animName.append(szAnimNamePost);
			}

			AddIfNewAnimationAlias(animList, animName.c_str(), szAnimPath);

			nCafID.m_nCafID++;
			nCafID = MemFindFirst(&szAnimPath, szMask, crcFolder, nCafID);
		}
		while (nCafID.m_nCafID >= 0);
	}
}

bool CParamLoader::ExpandWildcards(uint32 listID)
{
	LOADING_TIME_PROFILE_SECTION(g_pISystem);

	CDefaultSkeleton* pDefaultSkeleton = m_pDefaultSkeleton;
	SAnimListInfo& animList = m_parsedLists[listID];

	uint32 numAnims = animList.arrAnimFiles.size();
	if (numAnims == 0)
		return 0;

	// insert all animations found in the DBA files
	uint32 numDBALoaders = g_AnimationManager.m_arrGlobalHeaderDBA.size();
	for (uint32 j = 0; j < numDBALoaders; j++)
	{
		CGlobalHeaderDBA& rGlobalHeaderDBA = g_AnimationManager.m_arrGlobalHeaderDBA[j];
		if (rGlobalHeaderDBA.m_pDatabaseInfo == 0)
		{
			const char* pName = rGlobalHeaderDBA.m_strFilePathDBA;
			uint32 nDBACRC32 = CCrc32::ComputeLowercase(pName);
			uint32 numCAF = g_AnimationManager.m_arrGlobalCAF.size();
			uint32 nFoundAssetsInAIF = 0;
			for (uint32 c = 0; c < numCAF; c++)
			{
				if (g_AnimationManager.m_arrGlobalCAF[c].m_FilePathDBACRC32 == nDBACRC32)
				{
					nFoundAssetsInAIF = 1;
					break;
				}
			}
			if (nFoundAssetsInAIF == 0)
			{
				rGlobalHeaderDBA.LoadDatabaseDBA(pDefaultSkeleton->GetModelFilePath());
				if (rGlobalHeaderDBA.m_pDatabaseInfo == 0)
				{
					continue;
				}
			}
		}
	}

	//----------------------------------------------------------------------------------------
	// expand wildcards
	uint32 numWildcard = animList.arrWildcardAnimFiles.size();
	for (uint32 w = 0; w < numWildcard; ++w)
	{
		const char* szFolder = animList.arrWildcardAnimFiles[w].m_FilePathQ;
		const char* szFile = PathUtil::GetFile(szFolder);
		const char* szFilename = PathUtil::GetFileName(szFolder);
		const char* szExt = PathUtil::GetExt(szFolder);

		uint32 IsLMG = (strcmp(szExt, "lmg") == 0) || (strcmp(szExt, "bspace") == 0) || (strcmp(szExt, "comb") == 0);
		uint32 IsFSQ = strcmp(szExt, "fsq") == 0;

		char szAnimName[MAX_STRING_LENGTH];
		cry_strcpy(szAnimName, animList.arrWildcardAnimFiles[w].m_AnimNameQ);

		const char* firstWildcard = strchr(szFolder, '*');
		if (const char* qm = strchr(szFolder, '?'))
			if (firstWildcard == NULL || firstWildcard > qm)
				firstWildcard = qm;
		int32 iFirstWildcard = (int32)(firstWildcard - szFolder);
		int32 iPathLength = min((int32)(szFile - szFolder), iFirstWildcard);

		bool parseSubfolders = (iFirstWildcard != (int32)(szFile - szFolder));

		// conversion to offset from beginning of name
		const char* fileWildcard = strchr(szFile, '*');
		int32 offset = (int32)(fileWildcard - szFile);

		stack_string filepath = PathUtil::GetParentDirectory(stack_string(szFolder));
		char* starPos = strchr(szAnimName, '*');
		if (starPos)
			*starPos++ = 0;

		char strAnimName[MAX_STRING_LENGTH];
		uint32 img = g_pCharacterManager->IsInitializedByIMG();
		if (img & 2 && IsLMG == 0 && IsFSQ == 0)
		{
			uint32 crcFolder = CCrc32::ComputeLowercase(szFolder, size_t(iPathLength), 0);

			memset(strAnimName, 0, sizeof(strAnimName));

			AnimSearchHelper::TSubFolderCrCVector* subFolders = parseSubfolders ? g_AnimationManager.m_animSearchHelper.GetSubFoldersVector(crcFolder) : NULL;

			if (subFolders)
			{
				for (uint32 i = 0; i < subFolders->size(); i++)
				{
					ExpandWildcardsForPath(animList, szFile, (*subFolders)[i], szAnimName, starPos);
				}
			}
			else
			{
				ExpandWildcardsForPath(animList, szFile, crcFolder, szAnimName, starPos);
			}

			const uint32 numAIM = g_AnimationManager.m_arrGlobalAIM.size();
			for (uint32 nCafID = 0; nCafID < numAIM; nCafID++)
			{
				GlobalAnimationHeaderAIM& rAIM = g_AnimationManager.m_arrGlobalAIM[nCafID];
				stack_string strFilename = PathUtil::GetFile(rAIM.GetFilePath());
				stack_string strFilePath = PathUtil::GetPathWithoutFilename(rAIM.GetFilePath());
				const int32 filePathLen = strFilePath.length();

				if (parseSubfolders)
				{
					if ((iPathLength > filePathLen) || (strnicmp(szFolder, rAIM.GetFilePath(), iPathLength) != 0))
						continue;
				}
				else
				{
					if ((iPathLength != filePathLen) || (strnicmp(szFolder, rAIM.GetFilePath(), iPathLength) != 0))
						continue;
				}
				if (PathUtil::MatchWildcard(strFilename.c_str(), szFile))
				{
					stack_string animName = szAnimName;
					animName.append(PathUtil::GetFileName(strFilename));
					if (starPos)
					{
						animName.append(starPos);
					}

					AddIfNewAnimationAlias(animList, animName.c_str(), rAIM.GetFilePath());
				}
			}
		}
		else if (parseSubfolders)
		{
			stack_string path(szFolder, 0, iPathLength);
			PathUtil::ToUnixPath(path);

			std::vector<string> cafFiles;
			SDirectoryEnumeratorHelper dirParser;
			dirParser.ScanDirectoryRecursive("", path, szFile, cafFiles);

			const uint32 numCAFs = cafFiles.size();
			for (uint32 i = 0; i < numCAFs; i++)
			{
				stack_string animName = szAnimName;
				animName.append(PathUtil::GetFileName(cafFiles[i].c_str()).c_str());
				if (starPos)
				{
					animName.append(starPos);
				}

				if (IsFSQ)
				{
					AddIfNewFacialAnimationAlias(animList, animName.c_str(), cafFiles[i].c_str());
				}
				else
				{
					AddIfNewAnimationAlias(animList, animName.c_str(), cafFiles[i].c_str());
				}
			}
		}
		else
		{
			//extend the files from disk
			_finddata_t fd;
			intptr_t handle = g_pIPak->FindFirst(szFolder, &fd, ICryPak::FLAGS_NO_LOWCASE);
			if (handle != -1)
			{
				do
				{
					stack_string animName = szAnimName;
					animName.append(PathUtil::GetFileName(fd.name).c_str());
					if (starPos)
					{
						animName.append(starPos);
					}

					// Check whether the filename is a facial animation, by checking the extension.
					if (IsFSQ)
					{
						// insert unique
						AddIfNewFacialAnimationAlias(animList, animName.c_str(), filepath + "/" + fd.name);
					}
					else
					{
						stack_string fpath = filepath + stack_string("/") + fd.name;
						AddIfNewAnimationAlias(animList, animName.c_str(), fpath);
					}
				}
				while (g_pIPak->FindNext(handle, &fd) >= 0);

				g_pIPak->FindClose(handle);
			}
		}

		//-----------------------------------------------------------------

		if (Console::GetInst().ca_UseIMG_CAF == 0)
		{
			// insert all animations found in the DBA files
			for (uint32 j = 0; j < numDBALoaders; j++)
			{

				CGlobalHeaderDBA& rGlobalHeaderDBA = g_AnimationManager.m_arrGlobalHeaderDBA[j];
				if (rGlobalHeaderDBA.m_pDatabaseInfo == 0)
				{
					const char* pName = rGlobalHeaderDBA.m_strFilePathDBA;
					uint32 nDBACRC32 = CCrc32::ComputeLowercase(pName);
					uint32 numCAF = g_AnimationManager.m_arrGlobalCAF.size();
					uint32 nFoundAssetsInAIF = 0;
					for (uint32 c = 0; c < numCAF; c++)
					{
						if (g_AnimationManager.m_arrGlobalCAF[c].m_FilePathDBACRC32 == nDBACRC32)
						{
							nFoundAssetsInAIF++;

							const char* currentFile = g_AnimationManager.m_arrGlobalCAF[c].GetFilePath();
							const char* file = PathUtil::GetFile(currentFile);
							const char* ext = PathUtil::GetExt(currentFile);

							bool match1 = strnicmp(szFolder, currentFile, iPathLength) == 0;
							bool match2 = PathUtil::MatchWildcard(file, szFile);
							if (match1 && match2)
							{
								stack_string folderPathCurrentFile = PathUtil::GetParentDirectory(stack_string(currentFile));
								stack_string folderPathFileName = PathUtil::GetParentDirectory(stack_string(szFolder));

								if (parseSubfolders || (!parseSubfolders && folderPathCurrentFile == folderPathFileName))
								{
									stack_string name = "";
									if (starPos == NULL)
										name = szAnimName;
									else
									{
										uint32 folderPos = folderPathCurrentFile.length() + 1;
										stack_string sa = stack_string(szAnimName) + stack_string(currentFile + folderPos, ext - currentFile - 1 - folderPos) + starPos;
										name = sa;
									}

									AddIfNewAnimationAlias(animList, name.c_str(), currentFile);
								}
							}
						}
					}

					if (nFoundAssetsInAIF == 0)
					{
						rGlobalHeaderDBA.LoadDatabaseDBA(pDefaultSkeleton->GetModelFilePath());
						if (rGlobalHeaderDBA.m_pDatabaseInfo == 0)
						{
							continue;
						}
					}
				}

				DynArray<string> arrDBAPathNames;
				if (rGlobalHeaderDBA.m_pDatabaseInfo)
				{
					uint32 numCAF = g_AnimationManager.m_arrGlobalCAF.size();
					for (uint32 c = 0; c < numCAF; c++)
					{
						if (g_AnimationManager.m_arrGlobalCAF[c].m_FilePathDBACRC32 == rGlobalHeaderDBA.m_FilePathDBACRC32)
							arrDBAPathNames.push_back(g_AnimationManager.m_arrGlobalCAF[c].GetFilePath());
					}
				}

				uint32 numCafFiles = arrDBAPathNames.size();
				for (uint32 f = 0; f < numCafFiles; f++)
				{
					stack_string currentFile = arrDBAPathNames[f].c_str();
					PathUtil::UnifyFilePath(currentFile);

					const char* file = PathUtil::GetFile(currentFile.c_str());
					const char* ext = PathUtil::GetExt(currentFile.c_str());
					if (strnicmp(szFolder, currentFile, iPathLength) == 0 && PathUtil::MatchWildcard(file, szFile))
					{
						stack_string folderPathCurrentFile = PathUtil::GetParentDirectory(currentFile);
						stack_string folderPathFileName = PathUtil::GetParentDirectory(szFolder);

						if (parseSubfolders || (!parseSubfolders && folderPathCurrentFile == folderPathFileName))
						{
							stack_string name = "";
							if (starPos == NULL)
								name = szAnimName;
							else
							{
								uint32 folderPos = folderPathCurrentFile.length() + 1;
								stack_string sa = stack_string(szAnimName) + stack_string(currentFile.c_str() + folderPos, ext - currentFile - 1 - folderPos) + starPos;
								name = sa;
							}

							AddIfNewAnimationAlias(animList, name.c_str(), currentFile);
						}
					}
				}

			}
		}
	}

	//	return 0;
	animList.PrepareHeaderList();
	return true;
}

// check if a list has already been parsed and is in memory, if not, parse it and load it into m_parsedLists
int32 CParamLoader::LoadAnimList(const XmlNodeRef calNode, const char* paramFileName, string strAnimDirName)
{
	LOADING_TIME_PROFILE_SECTION(g_pISystem);

	if (!calNode)
		return -1;

	// check if List has already been processed
	int32 nListIDs = m_parsedLists.size();

	int32 newID = ListProcessed(paramFileName);
	if (newID != -1)
		return newID;

	SAnimListInfo animList(paramFileName);
	const char* pFilePath = m_pDefaultSkeleton->GetModelFilePath();
	const char* pFileName = PathUtil::GetFile(pFilePath);
	animList.arrAnimFiles.push_back(SAnimFile(stack_string(NULL_ANIM_FILE "/") + pFileName, "null"));

	const int BITE = 512;

	// [Alexey} - TODO! FIXME!!! FIXME!!!
	// We have 50 from 100 identical file parsing!!!
	//total++;

	//std::unordered_map<string, int, stl::hash_stricmp<string>, stl::hash_stricmp<string>>::iterator it = m_checkMap.find(string(calFileName));
	//if (it != m_checkMap.end())
	//{
	//	it->second = it->second + 1;
	//	int a = 0;
	//	identical++;
	//	Warning(calFileName, VALIDATOR_ERROR, "Identical chrparams readings %i from %i", identical, total);
	//} else
	//	m_checkMap[string(calFileName)] = 0;

	uint32 count = calNode->getChildCount();
	for (uint32 i = 0; i < count; ++i)
	{

		XmlNodeRef assignmentNode = calNode->getChild(i);

		if (stricmp(assignmentNode->getTag(), "Animation"))
			continue;

		CryFixedStringT<BITE> line;

		CryFixedStringT<BITE> key;
		CryFixedStringT<BITE> value;

		key.append(assignmentNode->getAttr("name"));
		line.append(assignmentNode->getAttr("path"));

		int32 pos = 0;
		value = line.Tokenize(" \t\n\r=", pos);

		// now only needed for aim poses
		char buffer[BITE];
		cry_strcpy(buffer, line.c_str());

		if (value.empty() || value.at(0) == '?')
		{
			continue;
		}

		if (0 == stricmp(key, "#filepath"))
		{
			strAnimDirName = PathUtil::ToUnixPath(line.c_str());
			strAnimDirName.TrimRight('/'); // delete the trailing slashes
			continue;
		}

		if (0 == stricmp(key, "#ParseSubFolders"))
		{
			Warning(paramFileName, VALIDATOR_WARNING, "Ignoring deprecated #ParseSubFolders directive");
			continue;
		}

		// remove first '\' and replace '\' with '/'
		value.replace('\\', '/');
		value.TrimLeft('/');

		// process the possible directives
		if (key.at(0) == '$')
		{
			if (!stricmp(key, "$AnimationDir") || !stricmp(key, "$AnimDir") || !stricmp(key, "$AnimationDirectory") || !stricmp(key, "$AnimDirectory"))
			{
				Warning(paramFileName, VALIDATOR_ERROR, "Deprecated directive \"%s\"", key.c_str());
			}
			else if (!stricmp(key, "$AnimEventDatabase"))
			{
				if (animList.animEventDatabase.empty())
					animList.animEventDatabase = value.c_str();
				//				else
				//					Warning(calFileName, VALIDATOR_WARNING, "Failed to set animation event database \"%s\". Animation event database is already set to \"%s\"", value.c_str(), m_animEventDatabase.c_str());
			}
			else if (!stricmp(key, "$TracksDatabase"))
			{
				int wildcardPos = value.find('*');
				if (wildcardPos >= 0)
				{
					//--- Wildcard include

					stack_string path = value.Left(wildcardPos);
					PathUtil::ToUnixPath(path);
					stack_string filename = PathUtil::GetFile(value.c_str());

					std::vector<string> dbaFiles;
					SDirectoryEnumeratorHelper dirParser;
					dirParser.ScanDirectoryRecursive("", path, filename, dbaFiles);

					const uint32 numDBAs = dbaFiles.size();
					for (uint32 d = 0; d < numDBAs; d++)
					{
						if (!AddIfNewModelTracksDatabase(animList, dbaFiles[d]))
							Warning(paramFileName, VALIDATOR_WARNING, "Duplicate model tracks database declared \"%s\"", dbaFiles[d].c_str());
					}
				}
				else
				{
					if (!AddIfNewModelTracksDatabase(animList, value.c_str()))
						Warning(paramFileName, VALIDATOR_WARNING, "Duplicate model tracks database declared \"%s\"", value.c_str());

					CryFixedStringT<BITE> flags;
					flags.append(assignmentNode->getAttr("flags"));

					// flag handling
					if (!flags.empty())
					{
						if (strstr(flags.c_str(), "persistent"))
						{
							animList.lockedDatabases.push_back(value.c_str());
						}
					}
				}
			}
			else if (!stricmp(key, "$Include"))
			{
				int32 listID = ListProcessed(value.c_str());

				if (listID == -1)
				{

					// load the new params file, but only parse the AnimationList section

					XmlNodeRef topRoot;
#ifdef EDITOR_PCDEBUGCODE
					string cacheKey = value;
					cacheKey.MakeLower();
					CachedCHRPARAMS::iterator it = m_cachedCHRPARAMS.find(cacheKey);
					if (it != m_cachedCHRPARAMS.end())
					{
						topRoot = g_pISystem->LoadXmlFromBuffer(it->second.data(), it->second.size());
					}
					else
#endif
					{
						topRoot = g_pISystem->LoadXmlFromFile(value.c_str());
					}

					if (topRoot)
					{
						const char* nodeTag = topRoot->getTag();
						if (stricmp(nodeTag, "Params") == 0)
						{
							int32 numChildren = topRoot->getChildCount();
							for (int32 iChild = 0; iChild < topRoot->getChildCount(); ++iChild)
							{
								XmlNodeRef node = topRoot->getChild(iChild);

								const char* newNodeTag = node->getTag();
								if (stricmp(newNodeTag, "AnimationList") == 0)
								{
									listID = LoadAnimList(node, value, strAnimDirName);
								}
							}
						}
					}
				}

				// we found a new dependency, add it if it is not already in
				if (listID >= 0)
				{
					if (std::find(animList.dependencies.begin(), animList.dependencies.end(), listID) == animList.dependencies.end())
						animList.dependencies.push_back(listID);
				}

			}
			else if (!stricmp(key, "$FaceLib"))
			{
				if (animList.faceLibFile.empty())
				{
					animList.faceLibFile = value.c_str();
					animList.faceLibDir = strAnimDirName;
				}
				else
					Warning(paramFileName, VALIDATOR_WARNING, "Failed to set face lib \"%s\". Face lib is already set to \"%s\"", value.c_str(), animList.faceLibFile.c_str());
			}
			else
				Warning(paramFileName, VALIDATOR_ERROR, "Unknown directive in '%s'", key.c_str());
		}
		else
		{
			// Check whether the filename is a facial animation, by checking the extension.
			const char* szExtension = PathUtil::GetExt(value);
			stack_string szFileName;
			szFileName.Format("%s/%s", strAnimDirName.c_str(), value.c_str());

			// is there any wildcard in the file name?
			if (strchr(value, '*') != NULL || strchr(value, '?') != NULL)
			{
				animList.arrWildcardAnimFiles.push_back(SAnimFile(szFileName, key));
			}
			else
			{
				const char* failedToCreateAlias = "Failed to create animation alias \"%s\" for file \"%s\". Such alias already exists.\"";
				if (szExtension != 0 && stricmp("fsq", szExtension) == 0)
				{
					bool added = AddIfNewFacialAnimationAlias(animList, key, szFileName.c_str());
					if (!added)
						Warning(paramFileName, VALIDATOR_WARNING, failedToCreateAlias, key.c_str(), szFileName.c_str());
				}
				else
				{
					bool added = AddIfNewAnimationAlias(animList, key, szFileName.c_str());
					if (!added)
						Warning(paramFileName, VALIDATOR_WARNING, failedToCreateAlias, key.c_str(), szFileName.c_str());
				}
			}

			//check if CAF or LMG has an Aim-Pose
			uint32 numAnims = animList.arrAnimFiles.size();
			if (szExtension != 0 && numAnims)
			{
				if ((stricmp("lmg", szExtension) == 0) || (stricmp("bspace", szExtension) == 0) || (stricmp("comb", szExtension) == 0) || (stricmp("caf", szExtension) == 0))
				{
					//PREFAST_SUPPRESS_WARNING(6031) strtok (buffer, " \t\n\r=");
					const char* fname = strtok(buffer, " \t\n\r=(");
					while ((fname = strtok(NULL, " \t\n\r,()")) != NULL && fname[0] != 0)
					{
						if (fname[0] == '/')
							break;
					}
				}
			}
		}
	}

	//end of loop
	m_parsedLists.push_back(animList);
	return m_parsedLists.size() - 1;
}

bool CParamLoader::AddIfNewAnimationAlias(SAnimListInfo& animList, const char* animName, const char* szFileName)
{
#if defined(_RELEASE)
	// We assume there are no duplicates in _RELEASE
	const bool addAnimationAlias = true;
#else
	const SAnimFile* pFoundAnimFile = FindAnimationAliasInDependencies(animList, animName);
	bool bDuplicate = (pFoundAnimFile != NULL);

	bool addAnimationAlias = true;
	if (bDuplicate)
	{
		// When using local files the duplicate check is used to prevent the file
		// from the PAK to be added too - so do not warn and do not add duplicates
		// when using local files and the filename is exactly the same
		const bool allowLocalFiles = (Console::GetInst().ca_UseIMG_CAF == 0);
		const bool sameFileName = (stricmp(pFoundAnimFile->m_FilePathQ, szFileName) == 0);
		const bool isValidDuplicate = (allowLocalFiles && sameFileName);
		if (isValidDuplicate)
		{
			addAnimationAlias = false;
		}
		else
		{
			Warning(animList.fileName.c_str(), VALIDATOR_WARNING, "Duplicate animation reference not supported: %s:%s", szFileName, pFoundAnimFile->m_FilePathQ);
	#if 0
			IPlatformOS::EMsgBoxResult result;
			IPlatformOS* pOS = gEnv->pSystem->GetPlatformOS();
			if (pOS)
			{
				char errorBuf[512];
				cry_sprintf(errorBuf, "Duplicate animation reference not supported: %s %s:%s", animList.fileName.c_str(), szFileName, pFoundAnimFile->m_FilePathQ);
				result = pOS->DebugMessageBox(errorBuf, "Animation");

				if (result == IPlatformOS::eMsgBox_Cancel)
				{
					__debugbreak();
				}
			}
	#endif
		}
	}
#endif

	if (addAnimationAlias)
	{
		animList.arrAnimFiles.push_back(SAnimFile(szFileName, animName));
	}

	return true;
}

const SAnimFile* CParamLoader::FindAnimationAliasInDependencies(SAnimListInfo& animList, const char* animName)
{
#if !defined(RELEASE)
	FindByAliasName pd;
	pd.fileCrc = CCrc32::ComputeLowercase(animName);
	DynArray<SAnimFile>::const_iterator found_it = std::find_if(animList.arrAnimFiles.begin(), animList.arrAnimFiles.end(), pd);
	if (found_it != animList.arrAnimFiles.end())
		return &(*found_it);

	uint32 numDeps = animList.dependencies.size();
	for (uint32 i = 0; i < numDeps; i++)
	{
		SAnimListInfo& childList = m_parsedLists[animList.dependencies[i]];

		const SAnimFile* pFoundAnimFile = FindAnimationAliasInDependencies(childList, animName);
		if (pFoundAnimFile)
			return pFoundAnimFile;
	}
#endif
	return NULL;
}

bool CParamLoader::AddIfNewFacialAnimationAlias(SAnimListInfo& animList, const char* animName, const char* szFileName)
{
	if (NoFacialAnimationAliasInDependencies(animList, animName))
	{
		animList.facialAnimations.push_back(CAnimationSet::FacialAnimationEntry(animName, szFileName));
		return true;
	}

	return false;
};

bool CParamLoader::NoModelTracksDatabaseInDependencies(SAnimListInfo& animList, const char* dataBase)
{
	DynArray<string>& mTB = animList.modelTracksDatabases;

	uint32 numDBA = mTB.size();
	if (std::find(mTB.begin(), mTB.end(), dataBase) != mTB.end())
	{
		return false;
	}

	uint32 numDeps = animList.dependencies.size();
	for (uint32 i = 0; i < numDeps; i++)
	{
		SAnimListInfo& childList = m_parsedLists[animList.dependencies[i]];
		if (!NoModelTracksDatabaseInDependencies(childList, dataBase))
			return false;
	}

	return true;
}

bool CParamLoader::AddIfNewModelTracksDatabase(SAnimListInfo& animList, const char* dataBase)
{

	stack_string tmp = dataBase;
	PathUtil::UnifyFilePath(tmp);
	if (NoModelTracksDatabaseInDependencies(animList, tmp.c_str()))
	{
		animList.modelTracksDatabases.push_back(tmp);
		return true;
	}
	else
		return false;
};

bool CParamLoader::NoFacialAnimationAliasInDependencies(SAnimListInfo& animList, const char* animName)
{
	for (uint32 i = 0; i < animList.facialAnimations.size(); ++i)
		if (0 == stricmp(animList.facialAnimations[i], animName))
		{
			return false;
		}

	uint32 numDeps = animList.dependencies.size();
	for (uint32 i = 0; i < numDeps; i++)
	{
		SAnimListInfo& childList = m_parsedLists[animList.dependencies[i]];
		if (!NoFacialAnimationAliasInDependencies(childList, animName))
			return false;
	}
	return true;
}

bool CParamLoader::LoadXML(CDefaultSkeleton* pDefaultSkeleton, string defaultAnimDir, const char* const paramFileName, DynArray<uint32>& listIDs)
{
	LOADING_TIME_PROFILE_SECTION(g_pISystem);

	XmlNodeRef topRoot;
#ifdef EDITOR_PCDEBUGCODE
	string key = paramFileName;
	key.MakeLower();
	CachedCHRPARAMS::iterator it = m_cachedCHRPARAMS.find(key);
	if (it != m_cachedCHRPARAMS.end())
	{
		topRoot = g_pISystem->LoadXmlFromBuffer(it->second.data(), it->second.size());
	}
	else
#endif
	{
		topRoot = g_pISystem->LoadXmlFromFile(paramFileName);
	}

	m_pDefaultSkeleton = pDefaultSkeleton;
	m_defaultAnimDir = defaultAnimDir;

	//in case we reload the CHRPARAMS again, we have to make sure we do a full initializations
	pDefaultSkeleton->m_IKLimbTypes.clear();
	pDefaultSkeleton->m_ADIKTargets.clear();

	pDefaultSkeleton->m_poseBlenderLookDesc.m_error = -1;  //not initialized by default
	pDefaultSkeleton->m_poseBlenderLookDesc.m_blends.clear();
	pDefaultSkeleton->m_poseBlenderLookDesc.m_rotations.clear();
	pDefaultSkeleton->m_poseBlenderLookDesc.m_positions.clear();

	pDefaultSkeleton->m_poseBlenderAimDesc.m_error = -1;   //not initialized by default
	pDefaultSkeleton->m_poseBlenderAimDesc.m_blends.clear();
	pDefaultSkeleton->m_poseBlenderAimDesc.m_rotations.clear();
	pDefaultSkeleton->m_poseBlenderAimDesc.m_positions.clear();
	pDefaultSkeleton->m_poseBlenderAimDesc.m_procAdjustments.clear();

	if (!topRoot)
	{
		return false;
	}

	uint32 numChildren = topRoot->getChildCount();
	for (uint32 i = 0; i < numChildren; ++i)
	{
		XmlNodeRef node = topRoot->getChild(i);

		const char* nodeTag = node->getTag();

		if (stricmp(nodeTag, "Lod") == 0)
		{
			if (!LoadLod(node))
				return false;
		}
		else if (stricmp(nodeTag, "BBoxExcludeList") == 0)
		{
			g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, paramFileName, "BBoxExcludeList is ignored: this feature is not supported.");
			continue;
		}
		else if (stricmp(nodeTag, "BBoxIncludeList") == 0)
		{
			if (!LoadBBoxInclusionList(node))
				return false;
		}
		else if (stricmp(nodeTag, "BBoxExtension") == 0)
		{
			if (!LoadBBoxExtension(node))
				return false;
		}
		else if (stricmp(nodeTag, "ShadowCapsulesList") == 0)
		{
			if (!LoadShadowCapsulesList(node))
				return false;
		}
		else if (stricmp(nodeTag, "UsePhysProxyBBox") == 0)
		{
			m_pDefaultSkeleton->m_usePhysProxyBBox = 1;
		}
		else if (stricmp(nodeTag, "IK_Definition") == 0)
		{
			if (!ParseIKDef(node))
				return false;
		}
		else if (stricmp(nodeTag, "AnimationList") == 0)
		{
			int32 rootNode = LoadAnimList(node, paramFileName, m_defaultAnimDir);
			BuildDependencyList(rootNode, listIDs); // add to the list a depth-first list of all the referenced animlist IDs, including the root. Root comes last.
			if (rootNode < 0)
			{
				g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, paramFileName, "Failed to load animations: %s", paramFileName);
				return 0;
			}
		}
	}

	return true;
}

bool CParamLoader::LoadLod(const XmlNodeRef lodNode)
{
	m_pDefaultSkeleton->m_arrAnimationLOD.clear();

	if (!lodNode)
		return false;

	XmlNodeRef lod = lodNode;

	uint32 count = lod->getChildCount();
	for (uint32 i = 0; i < count; ++i)
	{
		XmlNodeRef jointList = lod->getChild(i);
		if (!jointList->isTag("JointList"))
			continue;

		uint32 level;
		if (!jointList->getAttr("level", level))
			continue;

		DynArray<uint32> jointMask;
		uint32 jointCount = jointList->getChildCount();
		for (uint32 j = 0; j < jointCount; ++j)
		{
			XmlNodeRef joint = jointList->getChild(j);
			if (!joint->isTag("Joint"))
				continue;

			const char* name = joint->getAttr("name");
			if (!name)
				continue;

			uint32 nameCrc32 = CCrc32::Compute(name);
			jointMask.push_back(nameCrc32);
		}

		if (!jointMask.empty())
		{
			std::sort(jointMask.begin(), jointMask.end());
			m_pDefaultSkeleton->m_arrAnimationLOD.push_back()->swap(jointMask);
		}
	}

	return true;
}

bool CParamLoader::LoadBBoxInclusionList(const XmlNodeRef node)
{
	if (m_pDefaultSkeleton == 0)
		return false;
	if (node == 0)
		return false;

	CDefaultSkeleton& rDefaultSkeleton = *m_pDefaultSkeleton;
	uint count = node->getChildCount();
	rDefaultSkeleton.m_BBoxIncludeList.clear();
	rDefaultSkeleton.m_BBoxIncludeList.reserve(count);
	for (uint i = 0; i < count; ++i)
	{
		XmlNodeRef joint = node->getChild(i);
		if (!joint->isTag("Joint"))
			continue;

		const char* pJointName = joint->getAttr("name");
		if (!pJointName)
			continue;

		int jointIndex = rDefaultSkeleton.GetJointIDByName(pJointName);
		if (jointIndex < 0)
		{
			g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pDefaultSkeleton->GetModelFilePath(), "BBoxInclusionList Error: JointName '%s' for Inclusion not found in model", pJointName);
			continue;
		}
		rDefaultSkeleton.m_BBoxIncludeList.push_back(jointIndex);
	}
	return true;
}

bool CParamLoader::LoadBBoxExtension(const XmlNodeRef node)
{
	if (m_pDefaultSkeleton == 0)
		return false;
	if (node == 0)
		return false;

	CDefaultSkeleton& rDefaultSkeleton = *m_pDefaultSkeleton;
	uint count = node->getChildCount();

	rDefaultSkeleton.m_AABBExtension.min = Vec3(ZERO);
	rDefaultSkeleton.m_AABBExtension.max = Vec3(ZERO);

	for (uint i = 0; i < count; ++i)
	{
		XmlNodeRef joint = node->getChild(i);
		if (!joint->isTag("Axis"))
			continue;
		joint->getAttr("negX", rDefaultSkeleton.m_AABBExtension.min.x);
		rDefaultSkeleton.m_AABBExtension.min.x = fabsf(rDefaultSkeleton.m_AABBExtension.min.x);
		joint->getAttr("negY", rDefaultSkeleton.m_AABBExtension.min.y);
		rDefaultSkeleton.m_AABBExtension.min.y = fabsf(rDefaultSkeleton.m_AABBExtension.min.y);
		joint->getAttr("negZ", rDefaultSkeleton.m_AABBExtension.min.z);
		rDefaultSkeleton.m_AABBExtension.min.z = fabsf(rDefaultSkeleton.m_AABBExtension.min.z);
		joint->getAttr("posX", rDefaultSkeleton.m_AABBExtension.max.x);
		rDefaultSkeleton.m_AABBExtension.max.x = fabsf(rDefaultSkeleton.m_AABBExtension.max.x);
		joint->getAttr("posY", rDefaultSkeleton.m_AABBExtension.max.y);
		rDefaultSkeleton.m_AABBExtension.max.y = fabsf(rDefaultSkeleton.m_AABBExtension.max.y);
		joint->getAttr("posZ", rDefaultSkeleton.m_AABBExtension.max.z);
		rDefaultSkeleton.m_AABBExtension.max.z = fabsf(rDefaultSkeleton.m_AABBExtension.max.z);
	}
	return true;
}

bool CParamLoader::LoadShadowCapsulesList(const XmlNodeRef node)
{
	if (m_pDefaultSkeleton == 0)
		return false;
	if (node == 0)
		return false;

	CDefaultSkeleton& rDefaultSkeleton = *m_pDefaultSkeleton;
	uint count = node->getChildCount();
	rDefaultSkeleton.m_ShadowCapsulesList.clear();
	rDefaultSkeleton.m_ShadowCapsulesList.reserve(count);
	for (uint i = 0; i < count; ++i)
	{
		XmlNodeRef capsuleNode = node->getChild(i);
		if (!capsuleNode->isTag("Capsule"))
			continue;

		SBoneShadowCapsule shadowCapsule;
		shadowCapsule.arrJoints[0] = -1;
		shadowCapsule.arrJoints[1] = -1;
		shadowCapsule.radius = 0;

		for (int nJ = 0; nJ <= 1; nJ++)
		{
			const char* pAttrName = nJ ? "JointName1" : "JointName0";

			const char* pJointName = capsuleNode->getAttr(pAttrName);
			if (!pJointName)
			{
				g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pDefaultSkeleton->GetModelFilePath(), "LoadShadowCapsulesList Error: Attribute '%s' is missing", pAttrName);
				continue;
			}

			int jointIndex = rDefaultSkeleton.GetJointIDByName(pJointName);
			if (jointIndex < 0)
			{
				g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pDefaultSkeleton->GetModelFilePath(), "LoadShadowCapsulesList Error: JointName '%s' not found in model", pJointName);
				continue;
			}

			shadowCapsule.arrJoints[nJ] = jointIndex;
		}

		if (!capsuleNode->getAttr("Radius", shadowCapsule.radius))
		{
			g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pDefaultSkeleton->GetModelFilePath(), "LoadShadowCapsulesList Error: Attribute 'Radius' is missing");
			continue;
		}

		if (shadowCapsule.arrJoints[0] >= 0 && shadowCapsule.arrJoints[1] >= 0 && shadowCapsule.radius > 0)
		{
			rDefaultSkeleton.m_ShadowCapsulesList.push_back(shadowCapsule);
		}
	}

	return true;
}

bool CParamLoader::ParseIKDef(const XmlNodeRef ikNode)
{
	CDefaultSkeleton* pDefaultSkeleton = m_pDefaultSkeleton;

	if (ikNode)
	{
		//use XML file
		const char* TopRootXMLTAG = ikNode->getTag();
		if (stricmp(TopRootXMLTAG, "IK_Definition") == 0)
		{

			uint32 numIKTypes = ikNode->getChildCount();
			for (uint32 iktype = 0; iktype < numIKTypes; iktype++)
			{
				XmlNodeRef IKNode = ikNode->getChild(iktype);

				const char* IKTypeTAG = IKNode->getTag();

				//-----------------------------------------------------------
				//check LimbIK-Setup
				//-----------------------------------------------------------
				if (stricmp(IKTypeTAG, "LimbIK_Definition") == 0)
				{
					if (!LoadIKDefLimbIK(IKNode))
						return false;
				}

				//-----------------------------------------------------------
				//check Animation-Driven IK-Targets
				//-----------------------------------------------------------
				if (stricmp(IKTypeTAG, "Animation_Driven_IK_Targets") == 0)
					LoadIKDefAnimDrivenIKTargets(IKNode);

				//-----------------------------------------------------------
				//check FeetLock pose-modifier
				//-----------------------------------------------------------
				if (stricmp(IKTypeTAG, "FeetLock_Definition") == 0)
					LoadIKDefFeetLock(IKNode);

				//-----------------------------------------------------------
				//check Recoil-Setup
				//-----------------------------------------------------------
				if (stricmp(IKTypeTAG, "Recoil_Definition") == 0)
					LoadIKDefRecoil(IKNode);

				//-----------------------------------------------------------
				//check LookIK-Setup
				//-----------------------------------------------------------
				if (stricmp(IKTypeTAG, "LookIK_Definition") == 0)
				{
					LoadIKDefLookIK(IKNode);
					uint32 numRot = pDefaultSkeleton->m_poseBlenderLookDesc.m_rotations.size();
					uint32 numPos = pDefaultSkeleton->m_poseBlenderLookDesc.m_positions.size();
					for (uint32 r = 0; r < numRot; r++)
					{
						for (uint32 p = 0; p < numPos; p++)
						{
							const char* pRotName = pDefaultSkeleton->m_poseBlenderLookDesc.m_rotations[r].m_strJointName;
							if (pRotName == 0)
								continue;
							const char* pPosName = pDefaultSkeleton->m_poseBlenderLookDesc.m_positions[p].m_strJointName;
							if (pPosName == 0)
								continue;
							uint32 SameJoint = strcmp(pRotName, pPosName) == 0;
							if (SameJoint)
							{
								pDefaultSkeleton->m_poseBlenderLookDesc.m_rotations[r].m_nPosIndex = p;
								break;
							}
						}
					}
				}
				//-----------------------------------------------------------
				//check AimIK-Setup
				//-----------------------------------------------------------
				if (stricmp(IKTypeTAG, "AimIK_Definition") == 0)
				{
					LoadIKDefAimIK(IKNode);
					uint32 numRot = pDefaultSkeleton->m_poseBlenderAimDesc.m_rotations.size();
					uint32 numPos = pDefaultSkeleton->m_poseBlenderAimDesc.m_positions.size();
					for (uint32 r = 0; r < numRot; r++)
					{
						for (uint32 p = 0; p < numPos; p++)
						{
							const char* pRotName = pDefaultSkeleton->m_poseBlenderAimDesc.m_rotations[r].m_strJointName;
							if (pRotName == 0)
								continue;
							const char* pPosName = pDefaultSkeleton->m_poseBlenderAimDesc.m_positions[p].m_strJointName;
							if (pPosName == 0)
								continue;
							uint32 SameJoint = strcmp(pRotName, pPosName) == 0;
							if (SameJoint)
							{
								pDefaultSkeleton->m_poseBlenderAimDesc.m_rotations[r].m_nPosIndex = p;
								break;
							}
						}
					}
				}

			}
		}
	}

	return true;
}

void CParamLoader::ClearLists()
{
	int32 n = m_parsedLists.size();
	for (int32 i = 0; i < n; i++)
	{
		stl::free_container(m_parsedLists[i].arrWildcardAnimFiles);
		stl::free_container(m_parsedLists[i].arrAnimFiles);
		stl::free_container(m_parsedLists[i].modelTracksDatabases);
		stl::free_container(m_parsedLists[i].facialAnimations);
	}
	stl::free_container(m_parsedLists);
}

PREFAST_SUPPRESS_WARNING(6262)
bool CParamLoader::LoadIKDefLimbIK(const XmlNodeRef limbNode)
{
	CDefaultSkeleton* pDefaultSkeleton = m_pDefaultSkeleton;

	uint32 num = limbNode->getChildCount();
	for (uint32 likdef = 0; likdef < num; likdef++)
	{
		IKLimbType IKLimb;
		XmlNodeRef nodePos = limbNode->getChild(likdef);
		const char* PosTag = nodePos->getTag();
		if (stricmp(PosTag, "IK") == 0)
		{
			const char* pHandle = nodePos->getAttr("Handle");
			if (pHandle == 0)
				continue;
			IKLimb.m_nHandle = CCrc32::ComputeLowercase(pHandle);

			const char* pSolver = nodePos->getAttr("Solver");
			if (pSolver == 0)
				continue;

			IKLimb.m_nSolver = *(uint32*)pSolver;

			//-----------------------------------------------------------------------------------

			if (IKLimb.m_nSolver == *(uint32*)"2BIK")  //check the Solver
			{
				const char* pRoot = nodePos->getAttr("Root");
				if (pRoot == 0)
					continue;
				int32 nRootIdx = pDefaultSkeleton->GetJointIDByName(pRoot);
				if (nRootIdx < 0)
					continue;

				const char* pEndEffector = nodePos->getAttr("EndEffector");
				if (pEndEffector == 0)
					continue;
				int32 nEndEffectorIdx = pDefaultSkeleton->GetJointIDByName(pEndEffector);
				if (nEndEffectorIdx < 0)
					continue;

				assert(nRootIdx < nEndEffectorIdx);
				if (nRootIdx >= nEndEffectorIdx)
					continue;

				int32 nBaseRootIdx = pDefaultSkeleton->GetJointParentIDByID(nRootIdx);
				if (nBaseRootIdx < 0)
					continue;
				int32 nMiddleIdx = pDefaultSkeleton->GetJointParentIDByID(nEndEffectorIdx);
				if (nMiddleIdx < 0)
					continue;
				//do a simple verification
				int32 idx = pDefaultSkeleton->GetJointParentIDByID(nMiddleIdx);
				const char* strRoot = pDefaultSkeleton->GetJointNameByID(idx);
				if (idx != nRootIdx)
					continue;

				IKLimb.m_arrJointChain.resize(4);
				IKLimb.m_arrJointChain[0].m_idxJoint = nBaseRootIdx;
				IKLimb.m_arrJointChain[0].m_strJoint = pDefaultSkeleton->GetJointNameByID(nBaseRootIdx);
				IKLimb.m_arrJointChain[1].m_idxJoint = nRootIdx;
				IKLimb.m_arrJointChain[1].m_strJoint = pDefaultSkeleton->GetJointNameByID(nRootIdx);
				IKLimb.m_arrJointChain[2].m_idxJoint = nMiddleIdx;
				IKLimb.m_arrJointChain[2].m_strJoint = pDefaultSkeleton->GetJointNameByID(nMiddleIdx);
				IKLimb.m_arrJointChain[3].m_idxJoint = nEndEffectorIdx;
				IKLimb.m_arrJointChain[3].m_strJoint = pDefaultSkeleton->GetJointNameByID(nEndEffectorIdx);

				uint32 numJoints = pDefaultSkeleton->GetJointCount();
				int16 arrIndices[MAX_JOINT_AMOUNT];
				int32 icounter = 0;
				int32 start = nRootIdx;
				for (uint32 i = start; i < numJoints; i++)
				{
					int32 c = i;
					while (c > 0)
					{
						if (nRootIdx == c) { arrIndices[icounter++] = i; break; }
						else c = pDefaultSkeleton->m_arrModelJoints[c].m_idxParent;
					}
				}
				if (icounter)
				{
					IKLimb.m_arrLimbChildren.resize(icounter);
					for (int32 i = 0; i < icounter; i++)
						IKLimb.m_arrLimbChildren[i] = arrIndices[i];
				}

				int32 arrEndEff2Root[MAX_JOINT_AMOUNT];
				arrEndEff2Root[0] = nEndEffectorIdx;

				int32 error = -1;
				uint32 links;
				for (links = 0; links < (MAX_JOINT_AMOUNT - 1); links++)
				{
					arrEndEff2Root[links + 1] = pDefaultSkeleton->GetJointParentIDByID(arrEndEff2Root[links]);
					error = arrEndEff2Root[links + 1];
					if (error < 0)
						break;
					if (arrEndEff2Root[links + 1] == 0)
						break;
				}
				if (error < 0)
					continue;
				links += 2;
				IKLimb.m_arrRootToEndEffector.resize(links);
				int32 s = links - 1;
				for (uint32 j = 0; j < links; j++)
					IKLimb.m_arrRootToEndEffector[s--] = arrEndEff2Root[j];

				pDefaultSkeleton->m_IKLimbTypes.push_back(IKLimb);
			} //if "2BIK"

			//------------------------------------------------------------------------------------

			if (IKLimb.m_nSolver == *(uint32*)"3BIK")  //check the Solver
			{
				const char* pRoot = nodePos->getAttr("Root");
				if (pRoot == 0)
					continue;
				int32 nRootIdx = pDefaultSkeleton->GetJointIDByName(pRoot);
				if (nRootIdx < 0)
					continue;

				const char* pEndEffector = nodePos->getAttr("EndEffector");
				if (pEndEffector == 0)
					continue;
				int32 nEndEffectorIdx = pDefaultSkeleton->GetJointIDByName(pEndEffector);
				if (nEndEffectorIdx < 0)
					continue;

				assert(nRootIdx < nEndEffectorIdx);
				if (nRootIdx >= nEndEffectorIdx)
					continue;

				int32 nBaseRootIdx = pDefaultSkeleton->GetJointParentIDByID(nRootIdx);
				if (nBaseRootIdx < 0)
					continue;
				int32 nLeg03Idx = pDefaultSkeleton->GetJointParentIDByID(nEndEffectorIdx);
				if (nLeg03Idx < 0)
					continue;
				int32 nLeg02Idx = pDefaultSkeleton->GetJointParentIDByID(nLeg03Idx);
				if (nLeg02Idx < 0)
					continue;
				//do a simple verification
				int32 idx = pDefaultSkeleton->GetJointParentIDByID(nLeg02Idx);
				const char* strRoot = pDefaultSkeleton->GetJointNameByID(idx);
				if (idx != nRootIdx)
					continue;

				IKLimb.m_arrJointChain.resize(5);
				IKLimb.m_arrJointChain[0].m_idxJoint = nBaseRootIdx;
				IKLimb.m_arrJointChain[0].m_strJoint = pDefaultSkeleton->GetJointNameByID(nBaseRootIdx);

				IKLimb.m_arrJointChain[1].m_idxJoint = nRootIdx;
				IKLimb.m_arrJointChain[1].m_strJoint = pDefaultSkeleton->GetJointNameByID(nRootIdx);

				IKLimb.m_arrJointChain[2].m_idxJoint = nLeg02Idx;
				IKLimb.m_arrJointChain[2].m_strJoint = pDefaultSkeleton->GetJointNameByID(nLeg02Idx);

				IKLimb.m_arrJointChain[3].m_idxJoint = nLeg03Idx;
				IKLimb.m_arrJointChain[3].m_strJoint = pDefaultSkeleton->GetJointNameByID(nLeg03Idx);

				IKLimb.m_arrJointChain[4].m_idxJoint = nEndEffectorIdx;
				IKLimb.m_arrJointChain[4].m_strJoint = pDefaultSkeleton->GetJointNameByID(nEndEffectorIdx);

				uint32 numJoints = pDefaultSkeleton->GetJointCount();
				int16 arrIndices[MAX_JOINT_AMOUNT];
				int32 icounter = 0;
				int32 start = nRootIdx;
				for (uint32 i = start; i < numJoints; i++)
				{
					int32 c = i;
					while (c > 0)
					{
						if (nRootIdx == c) { arrIndices[icounter++] = i; break; }
						else c = pDefaultSkeleton->m_arrModelJoints[c].m_idxParent;
					}
				}
				if (icounter)
				{
					IKLimb.m_arrLimbChildren.resize(icounter);
					for (int32 i = 0; i < icounter; i++)
						IKLimb.m_arrLimbChildren[i] = arrIndices[i];
				}

				int32 arrEndEff2Root[MAX_JOINT_AMOUNT];
				arrEndEff2Root[0] = nEndEffectorIdx;

				int32 error = -1;
				uint32 links;
				for (links = 0; links < (MAX_JOINT_AMOUNT - 1); links++)
				{
					arrEndEff2Root[links + 1] = pDefaultSkeleton->GetJointParentIDByID(arrEndEff2Root[links]);
					error = arrEndEff2Root[links + 1];
					if (error < 0)
						break;
					if (arrEndEff2Root[links + 1] == 0)
						break;
				}
				if (error < 0)
					continue;
				links += 2;
				IKLimb.m_arrRootToEndEffector.resize(links);
				int32 s = links - 1;
				for (uint32 j = 0; j < links; j++)
					IKLimb.m_arrRootToEndEffector[s--] = arrEndEff2Root[j];

				pDefaultSkeleton->m_IKLimbTypes.push_back(IKLimb);
			} //if "3BIK"

			//--------------------------------------------------------------------------

			if (IKLimb.m_nSolver == *(uint32*)"CCDX")  //check the Solver
			{
				const char* pRoot = nodePos->getAttr("Root");
				if (pRoot == 0)
					continue;
				int32 nRootIdx = pDefaultSkeleton->GetJointIDByName(pRoot);
				if (nRootIdx < 0)
					continue;

				const char* pEndEffector = nodePos->getAttr("EndEffector");
				if (pEndEffector == 0)
					continue;
				int32 nEndEffectorIdx = pDefaultSkeleton->GetJointIDByName(pEndEffector);
				if (nEndEffectorIdx < 0)
					continue;

				assert(nRootIdx < nEndEffectorIdx);
				if (nRootIdx >= nEndEffectorIdx)
					continue;

				int32 nBaseRootIdx = pDefaultSkeleton->GetJointParentIDByID(nRootIdx);
				if (nBaseRootIdx < 0)
					continue;

				if (!nodePos->getAttr("fStepSize", IKLimb.m_fStepSize))
					continue;
				if (!nodePos->getAttr("fThreshold", IKLimb.m_fThreshold))
					continue;
				if (!nodePos->getAttr("nMaxInteration", IKLimb.m_nInterations)) // keep this for downward-compatibility reasons, even if 'Iteration' is misspelled
					if (!nodePos->getAttr("nMaxIteration", IKLimb.m_nInterations))
						continue;

				int32 arrCCDChainIdx[MAX_JOINT_AMOUNT];
				const char* arrCCDChainStr[MAX_JOINT_AMOUNT];

				arrCCDChainIdx[0] = nEndEffectorIdx;
				arrCCDChainStr[0] = pDefaultSkeleton->GetJointNameByID(nEndEffectorIdx);

				int32 error = -1;
				uint32 links;
				for (links = 0; links < (MAX_JOINT_AMOUNT - 1); links++)
				{
					arrCCDChainIdx[links + 1] = pDefaultSkeleton->GetJointParentIDByID(arrCCDChainIdx[links]);
					error = arrCCDChainIdx[links + 1];
					if (error < 0)
						break;
					arrCCDChainStr[links + 1] = pDefaultSkeleton->GetJointNameByID(arrCCDChainIdx[links + 1]);
					if (arrCCDChainIdx[links + 1] == nRootIdx)
						break;
				}
				if (error < 0)
					continue;

				links++;
				arrCCDChainIdx[links + 1] = nBaseRootIdx;
				arrCCDChainStr[links + 1] = pDefaultSkeleton->GetJointNameByID(arrCCDChainIdx[links + 1]);

				links += 2;
				IKLimb.m_arrJointChain.resize(links);
				int32 s = links - 1;
				for (uint32 j = 0; j < links; j++)
				{
					IKLimb.m_arrJointChain[s].m_idxJoint = arrCCDChainIdx[j];
					IKLimb.m_arrJointChain[s].m_strJoint = arrCCDChainStr[j];
					s--;
				}

				uint32 numJoints = pDefaultSkeleton->GetJointCount();
				int16 arrIndices[MAX_JOINT_AMOUNT];
				int32 icounter = 0;
				for (uint32 i = nRootIdx; i < numJoints; i++)
				{
					int32 c = i;
					while (c > 0)
					{
						if (nRootIdx == c) { arrIndices[icounter++] = i; break; }
						else c = pDefaultSkeleton->m_arrModelJoints[c].m_idxParent;
					}
				}
				if (icounter)
				{
					IKLimb.m_arrLimbChildren.resize(icounter);
					for (int32 i = 0; i < icounter; i++)
						IKLimb.m_arrLimbChildren[i] = arrIndices[i];
				}

				int32 arrEndEff2Root[MAX_JOINT_AMOUNT];
				arrEndEff2Root[0] = nEndEffectorIdx;
				error = -1;
				for (links = 0; links < (MAX_JOINT_AMOUNT - 1); links++)
				{
					arrEndEff2Root[links + 1] = pDefaultSkeleton->GetJointParentIDByID(arrEndEff2Root[links]);
					error = arrEndEff2Root[links + 1];
					if (error < 0)
						break;
					if (arrEndEff2Root[links + 1] == 0)
						break;
				}
				if (error < 0)
					continue;
				links += 2;
				IKLimb.m_arrRootToEndEffector.resize(links);
				s = links - 1;
				for (uint32 j = 0; j < links; j++)
					IKLimb.m_arrRootToEndEffector[s--] = arrEndEff2Root[j];

				pDefaultSkeleton->m_IKLimbTypes.push_back(IKLimb);
			} //if "CCDX"

			//--------------------------------------------------------------------------

			if (IKLimb.m_nSolver == *(uint32*)"CCD5")  //check the Solver
			{
				int32 varify = 3;
				IKLimb.m_arrJointChain.resize(5);

				const char* pJ0 = nodePos->getAttr("J0");
				if (pJ0)
				{
					int32 nJ0Idx = pDefaultSkeleton->GetJointIDByName(pJ0);
					if (nJ0Idx > 0)
					{
						IKLimb.m_arrJointChain[0].m_idxJoint = nJ0Idx;
						IKLimb.m_arrJointChain[0].m_strJoint = pDefaultSkeleton->GetJointNameByID(nJ0Idx);
						varify--;
					}
					ANIM_ASSET_CHECK_TRACE(nJ0Idx > 0, ("For J0 %s", pJ0));
				}
				const char* pJ1 = nodePos->getAttr("J1");
				if (pJ1)
				{
					int32 nJ1Idx = pDefaultSkeleton->GetJointIDByName(pJ1);
					if (nJ1Idx > 0)
					{
						IKLimb.m_arrJointChain[1].m_idxJoint = nJ1Idx;
						IKLimb.m_arrJointChain[1].m_strJoint = pDefaultSkeleton->GetJointNameByID(nJ1Idx);
						varify--;
					}
					ANIM_ASSET_CHECK_TRACE(nJ1Idx > 0, ("For J1 %s", pJ1));
				}

				const char* pJ2 = nodePos->getAttr("J2");
				if (pJ2)
				{
					int32 nJ2Idx = pDefaultSkeleton->GetJointIDByName(pJ2);
					if (nJ2Idx > 0)
					{
						IKLimb.m_arrJointChain[2].m_idxJoint = nJ2Idx;
						IKLimb.m_arrJointChain[2].m_strJoint = pDefaultSkeleton->GetJointNameByID(nJ2Idx);
						varify--;
					}
					ANIM_ASSET_CHECK_TRACE(nJ2Idx > 0, ("For J2 %s", pJ2));
				}

				const char* pJ3 = nodePos->getAttr("J3");
				if (pJ3)
				{
					int32 nJ3Idx = pDefaultSkeleton->GetJointIDByName(pJ3);
					if (nJ3Idx > 0)
					{
						IKLimb.m_arrJointChain[3].m_idxJoint = nJ3Idx;
						IKLimb.m_arrJointChain[3].m_strJoint = pDefaultSkeleton->GetJointNameByID(nJ3Idx);
						varify--;
					}
					ANIM_ASSET_CHECK_TRACE(nJ3Idx > 0, ("Failure for J3 %s", pJ3));
				}
				const char* pJ4 = nodePos->getAttr("J4");
				if (pJ4)
				{
					int32 nJ4Idx = pDefaultSkeleton->GetJointIDByName(pJ4);
					if (nJ4Idx > 0)
					{
						IKLimb.m_arrJointChain[4].m_idxJoint = nJ4Idx;
						IKLimb.m_arrJointChain[4].m_strJoint = pDefaultSkeleton->GetJointNameByID(nJ4Idx);
						varify--;
					}
					ANIM_ASSET_CHECK_TRACE(nJ4Idx > 0, ("Failure for J4 %s", pJ4));
				}

				if (varify == -2)
					pDefaultSkeleton->m_IKLimbTypes.push_back(IKLimb);

			}
		}
	}
	return true;
}

bool CParamLoader::LoadIKDefAnimDrivenIKTargets(const XmlNodeRef adNode)
{
	CDefaultSkeleton* pDefaultSkeleton = m_pDefaultSkeleton;

	uint32 num = adNode->getChildCount();
	for (uint32 i = 0; i < num; i++)
	{
		ADIKTarget IKTarget;
		XmlNodeRef nodePos = adNode->getChild(i);
		const char* PosTag = nodePos->getTag();
		if (stricmp(PosTag, "ADIKTarget") == 0)
		{
			uint32 varify = 3;
			const char* pHandle = nodePos->getAttr("Handle");
			if (pHandle)
			{
				IKTarget.m_nHandle = CCrc32::ComputeLowercase(pHandle);
				varify--;
			}
			const char* pTarget = nodePos->getAttr("Target");
			if (pTarget)
			{
				int32 nTargetIdx = pDefaultSkeleton->GetJointIDByName(pTarget);
				if (nTargetIdx > 0)
				{
					IKTarget.m_idxTarget = nTargetIdx;
					IKTarget.m_strTarget = pDefaultSkeleton->GetJointNameByID(nTargetIdx);
					varify--;
				}
				ANIM_ASSET_CHECK_TRACE(nTargetIdx > 0, ("Failure for target %s", pTarget));
			}

			const char* pWeight = nodePos->getAttr("Weight");
			if (pWeight)
			{
				int32 nWeightIdx = pDefaultSkeleton->GetJointIDByName(pWeight);
				if (nWeightIdx > 0)
				{
					IKTarget.m_idxWeight = nWeightIdx;
					IKTarget.m_strWeight = pDefaultSkeleton->GetJointNameByID(nWeightIdx);
					varify--;
				}
				ANIM_ASSET_CHECK_TRACE(nWeightIdx > 0, ("Failure for weight %s", pWeight));
			}
			if (varify == 0)
				pDefaultSkeleton->m_ADIKTargets.push_back(IKTarget);
		}
	}
	return true;
}

bool CParamLoader::LoadIKDefFeetLock(XmlNodeRef aimNode)
{
	CDefaultSkeleton* pDefaultSkeleton = m_pDefaultSkeleton;

	uint32 numChilds = min(MAX_FEET_AMOUNT, aimNode->getChildCount());
	for (uint32 c = 0; c < numChilds; c++)
	{
		XmlNodeRef node = aimNode->getChild(c);
		const char* strTag = node->getTag();

		if (stricmp(strTag, "IKHandle") == 0 ||
		    stricmp(strTag, "RIKHandle") == 0 ||
		    stricmp(strTag, "LIKHandle") == 0)
		{
			const char* pJointName = node->getAttr("Handle");
			pDefaultSkeleton->m_strFeetLockIKHandle[c] = pJointName;
		}
	}
	return true;
}

bool CParamLoader::LoadIKDefRecoil(XmlNodeRef aimNode)
{
	CDefaultSkeleton* pDefaultSkeleton = m_pDefaultSkeleton;

	uint32 numChilds = aimNode->getChildCount();
	for (uint32 c = 0; c < numChilds; c++)
	{
		XmlNodeRef node = aimNode->getChild(c);
		const char* strTag = node->getTag();

		if (stricmp(strTag, "RIKHandle") == 0)
		{
			const char* pJointName = node->getAttr("Handle");
			pDefaultSkeleton->m_recoilDesc.m_ikHandleRight = pJointName;
		}

		if (stricmp(strTag, "LIKHandle") == 0)
		{
			const char* pJointName = node->getAttr("Handle");
			pDefaultSkeleton->m_recoilDesc.m_ikHandleLeft = pJointName;
		}

		if (stricmp(strTag, "RWeaponJoint") == 0)
		{
			const char* pJointName = node->getAttr("JointName");
			int32 jidx = pDefaultSkeleton->GetJointIDByName(pJointName);
			assert(jidx > 0);
			if (jidx > 0)
				pDefaultSkeleton->m_recoilDesc.m_weaponRightJointIndex = jidx;
		}

		//----------------------------------------------------------------------------------------

		if (stricmp(strTag, "LWeaponJoint") == 0)
		{
			const char* pJointName = node->getAttr("JointName");
			int32 jidx = pDefaultSkeleton->GetJointIDByName(pJointName);
			assert(jidx > 0);
			if (jidx > 0)
				pDefaultSkeleton->m_recoilDesc.m_weaponLeftJointIndex = jidx;
		}

		//----------------------------------------------------------------------------
		if (stricmp(strTag, "ImpactJoints") == 0)
		{
			uint32 num = node->getChildCount();
			pDefaultSkeleton->m_recoilDesc.m_joints.reserve(num);
			for (uint32 i = 0; i < num; i++)
			{
				SRecoilJoints Recoil;
				XmlNodeRef nodeRot = node->getChild(i);
				const char* RotTag = nodeRot->getTag();
				if (stricmp(RotTag, "ImpactJoint") == 0)
				{
					const char* pJointName = nodeRot->getAttr("JointName");
					int32 jidx = pDefaultSkeleton->GetJointIDByName(pJointName);
					if (jidx > 0)
					{
						Recoil.m_nIdx = jidx;
						Recoil.m_strJointName = pDefaultSkeleton->GetJointNameByID(jidx);
					}
					assert(jidx > 0);
					nodeRot->getAttr("Arm", Recoil.m_nArm);
					nodeRot->getAttr("Delay", Recoil.m_fDelay);
					nodeRot->getAttr("Weight", Recoil.m_fWeight);
					//	Recoil.m_nAdditive=0;
					pDefaultSkeleton->m_recoilDesc.m_joints.push_back(Recoil);
				}
			}
		}
	}
	return true;
}

bool CParamLoader::LoadIKDefLookIK(const XmlNodeRef lookNode)
{
	CDefaultSkeleton* pDefaultSkeleton = m_pDefaultSkeleton;
	PoseBlenderLookDesc& poseBlenderLookDesc = pDefaultSkeleton->m_poseBlenderLookDesc;

	pDefaultSkeleton->m_poseBlenderLookDesc.m_error = 0;
	uint32 numChilds = lookNode->getChildCount();
	for (uint32 c = 0; c < numChilds; c++)
	{
		XmlNodeRef node = lookNode->getChild(c);
		const char* strTag = node->getTag();

		if (stricmp(strTag, "LEyeAttachment") == 0)
		{
			const char* pName = node->getAttr("Name");
			poseBlenderLookDesc.m_eyeAttachmentLeftName = pName;        //left eyeball attachment
		}

		if (stricmp(strTag, "REyeAttachment") == 0)
		{
			const char* pName = node->getAttr("Name");
			poseBlenderLookDesc.m_eyeAttachmentRightName = pName;       //right eyeball attachment
		}

		if (stricmp(strTag, "EyeLimits") == 0)
		{
			float halfYawDegrees = 45;
			float pitchDegreesUp = 45;
			float pitchDegreesDown = 45;

			node->getAttr("halfYawDegrees", halfYawDegrees);
			node->getAttr("pitchDegreesUp", pitchDegreesUp);
			node->getAttr("pitchDegreesDown", pitchDegreesDown);

			poseBlenderLookDesc.m_eyeLimitHalfYawRadians = DEG2RAD(halfYawDegrees);
			poseBlenderLookDesc.m_eyeLimitPitchRadiansUp = DEG2RAD(pitchDegreesUp);
			poseBlenderLookDesc.m_eyeLimitPitchRadiansDown = DEG2RAD(pitchDegreesDown);
		}

		//----------------------------------------------------------------------------
		if (stricmp(strTag, "RotationList") == 0)
		{
			uint32 num = node->getChildCount();
			poseBlenderLookDesc.m_rotations.reserve(num);
			for (uint32 i = 0; i < num; i++)
			{
				SJointsAimIK_Rot LookIK_Rot;
				XmlNodeRef nodeRot = node->getChild(i);
				const char* RotTag = nodeRot->getTag();
				if (stricmp(RotTag, "Rotation") == 0)
				{
					const char* pJointName = nodeRot->getAttr("JointName");
					const int32 jidx = pDefaultSkeleton->GetJointIDByName(pJointName);
					if (jidx > 0)
					{
						LookIK_Rot.m_nJointIdx = jidx;
						LookIK_Rot.m_strJointName = pDefaultSkeleton->GetJointNameByID(jidx);
					}
					else
					{
						poseBlenderLookDesc.m_error++;
						g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pDefaultSkeleton->GetModelFilePath(), "JointName '%s' for Look-Pose not found in model", pJointName);
					}
					assert(jidx > 0);
					nodeRot->getAttr("Primary", LookIK_Rot.m_nPreEvaluate);
					nodeRot->getAttr("Additive", LookIK_Rot.m_nAdditive);
					//	AimIK_Rot.m_nAdditive=0;
					const uint32 numRotations = poseBlenderLookDesc.m_rotations.size();
					const int32 parentJointIndex = pDefaultSkeleton->GetJointParentIDByID(jidx);
					for (uint32 p = 0; p < numRotations; ++p)
					{
						const SJointsAimIK_Rot& otherLookIkRot = poseBlenderLookDesc.m_rotations[p];
						if (LookIK_Rot.m_nPreEvaluate && otherLookIkRot.m_nPreEvaluate)
						{
							const bool isCurrentJointParentOfPreviousJoint = FindJointInParentHierarchy(pDefaultSkeleton, jidx, otherLookIkRot.m_nJointIdx);
							if (isCurrentJointParentOfPreviousJoint)
							{
								poseBlenderLookDesc.m_error++;
								g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pDefaultSkeleton->GetModelFilePath(), "Primary joint with name '%s' for Look-Pose should not be declared after joint '%s'.", pJointName, otherLookIkRot.m_strJointName);
							}
						}
						if (otherLookIkRot.m_nJointIdx == parentJointIndex)
						{
							LookIK_Rot.m_nRotJointParentIdx = p;
							break;
						}
					}
					poseBlenderLookDesc.m_rotations.push_back(LookIK_Rot);
				}
			}
		}

		//----------------------------------------------------------------------------------------

		if (stricmp(strTag, "PositionList") == 0)
		{
			uint32 num = node->getChildCount();
			poseBlenderLookDesc.m_positions.reserve(num);
			for (uint32 i = 0; i < num; i++)
			{
				SJointsAimIK_Pos LookIK_Pos;
				XmlNodeRef nodePos = node->getChild(i);
				const char* PosTag = nodePos->getTag();
				if (stricmp(PosTag, "Position") == 0)
				{
					const char* pJointName = nodePos->getAttr("JointName");
					int32 jidx = pDefaultSkeleton->GetJointIDByName(pJointName);
					if (jidx > 0)
					{
						LookIK_Pos.m_nJointIdx = jidx;
						LookIK_Pos.m_strJointName = pDefaultSkeleton->GetJointNameByID(jidx);
					}
					else
					{
						poseBlenderLookDesc.m_error++;
						g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pDefaultSkeleton->GetModelFilePath(), "JointName '%s' for Look-Pose not found in model", pJointName);
					}

					assert(jidx > 0);
					nodePos->getAttr("Additive", LookIK_Pos.m_nAdditive);
					poseBlenderLookDesc.m_positions.push_back(LookIK_Pos);
				}
			}
		}

		//----------------------------------------------------------------------------------------

		if (stricmp(strTag, "DirectionalBlends") == 0)
		{
			uint32 num = node->getChildCount();
			poseBlenderLookDesc.m_blends.reserve(num);
			for (uint32 i = 0; i < num; i++)
			{
				DirectionalBlends DirBlend;
				XmlNodeRef nodePos = node->getChild(i);
				const char* DirTag = nodePos->getTag();
				if (stricmp(DirTag, "Joint") == 0)
				{
					const char* pAnimToken = nodePos->getAttr("AnimToken");
					DirBlend.m_AnimToken = pAnimToken;
					DirBlend.m_AnimTokenCRC32 = CCrc32::ComputeLowercase(pAnimToken);

					const char* pParameterJointName = nodePos->getAttr("ParameterJoint");
					int jidx1 = pDefaultSkeleton->GetJointIDByName(pParameterJointName);
					if (jidx1 >= 0)
					{
						DirBlend.m_nParaJointIdx = jidx1;
						DirBlend.m_strParaJointName = pDefaultSkeleton->GetJointNameByID(jidx1);
					}
					else
					{
						poseBlenderLookDesc.m_error++;
						g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pDefaultSkeleton->GetModelFilePath(), "Look-Pose Error: JointName '%s' for Look-Pose not found in model", pParameterJointName);
					}
					assert(jidx1 >= 0);

					const char* pStartJointName = nodePos->getAttr("StartJoint");
					int jidx2 = pDefaultSkeleton->GetJointIDByName(pStartJointName);
					if (jidx2 >= 0)
					{
						DirBlend.m_nStartJointIdx = jidx2;
						DirBlend.m_strStartJointName = pDefaultSkeleton->GetJointNameByID(jidx2);
					}
					else
					{
						poseBlenderLookDesc.m_error++;
						g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pDefaultSkeleton->GetModelFilePath(), "Look-Pose Error:  Start JointName '%s' for Look-Pose not found in model", pStartJointName);
					}
					assert(jidx2 >= 0);

					const char* pReferenceJointName = nodePos->getAttr("ReferenceJoint");
					int jidx3 = pDefaultSkeleton->GetJointIDByName(pReferenceJointName);
					if (jidx3 >= 0)
					{
						DirBlend.m_nReferenceJointIdx = jidx3;
						DirBlend.m_strReferenceJointName = pDefaultSkeleton->GetJointNameByID(jidx3);
					}
					else
					{
						poseBlenderLookDesc.m_error++;
						g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pDefaultSkeleton->GetModelFilePath(), "Look-Pose Error: Reference JointName '%s' for Look-Pose not found in model", pReferenceJointName);
					}
					assert(jidx3 >= 0);

					poseBlenderLookDesc.m_blends.push_back(DirBlend);
				}
			}
		}
	}

	{
		InitalizeBlendRotJointIndices(poseBlenderLookDesc.m_blends, poseBlenderLookDesc.m_rotations);
	}

	//----------------------------------------------------------------------------------------
	if (poseBlenderLookDesc.m_error)
	{
		poseBlenderLookDesc.m_rotations.clear();
		poseBlenderLookDesc.m_positions.clear();
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pDefaultSkeleton->GetModelFilePath(), "CryAnimation Error: Look-Poses disabled");
		return false;
	}

	return true;
}

bool CParamLoader::LoadIKDefAimIK(XmlNodeRef aimNode)
{
	CDefaultSkeleton* pDefaultSkeleton = m_pDefaultSkeleton;
	PoseBlenderAimDesc& poseBlenderAimDesc = pDefaultSkeleton->m_poseBlenderAimDesc;

	uint32 numChilds = aimNode->getChildCount();
	for (uint32 c = 0; c < numChilds; c++)
	{
		XmlNodeRef node = aimNode->getChild(c);
		const char* strTag = node->getTag();

		//---------------------------------------------------------------------
		poseBlenderAimDesc.m_error = 0;
		if (stricmp(strTag, "DirectionalBlends") == 0)
		{
			uint32 num = node->getChildCount();
			poseBlenderAimDesc.m_blends.reserve(num);
			for (uint32 i = 0; i < num; i++)
			{
				DirectionalBlends DirBlend;
				XmlNodeRef nodePos = node->getChild(i);
				const char* DirTag = nodePos->getTag();
				if (stricmp(DirTag, "Joint") == 0)
				{
					const char* pAnimToken = nodePos->getAttr("AnimToken");
					DirBlend.m_AnimToken = pAnimToken;
					DirBlend.m_AnimTokenCRC32 = CCrc32::ComputeLowercase(pAnimToken);

					const char* pParameterJointName = nodePos->getAttr("ParameterJoint");
					int jidx1 = pDefaultSkeleton->GetJointIDByName(pParameterJointName);
					if (jidx1 >= 0)
					{
						DirBlend.m_nParaJointIdx = jidx1;
						DirBlend.m_strParaJointName = pDefaultSkeleton->GetJointNameByID(jidx1);
					}
					else
					{
						poseBlenderAimDesc.m_error++;
						g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pDefaultSkeleton->GetModelFilePath(), "Aim-Pose Error: JointName '%s' for Aim-Pose not found in model", pParameterJointName);
					}
					assert(jidx1 >= 0);

					const char* pStartJointName = nodePos->getAttr("StartJoint");
					int jidx2 = pDefaultSkeleton->GetJointIDByName(pStartJointName);
					if (jidx2 >= 0)
					{
						DirBlend.m_nStartJointIdx = jidx2;
						DirBlend.m_strStartJointName = pDefaultSkeleton->GetJointNameByID(jidx2);
					}
					else
					{
						poseBlenderAimDesc.m_error++;
						g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pDefaultSkeleton->GetModelFilePath(), "Aim-Pose Error: StartJointName '%s' for Aim-Pose not found in model", pStartJointName);
					}
					assert(jidx2 >= 0);

					const char* pReferenceJointName = nodePos->getAttr("ReferenceJoint");
					int jidx3 = pDefaultSkeleton->GetJointIDByName(pReferenceJointName);
					if (jidx3 >= 0)
					{
						DirBlend.m_nReferenceJointIdx = jidx3;
						DirBlend.m_strReferenceJointName = pDefaultSkeleton->GetJointNameByID(jidx3);
					}
					else
					{
						poseBlenderAimDesc.m_error++;
						g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pDefaultSkeleton->GetModelFilePath(), "Aim-Pose Error: Reference JointName '%s' for Aim-Pose not found in model", pReferenceJointName);
					}
					assert(jidx3 >= 0);

					poseBlenderAimDesc.m_blends.push_back(DirBlend);
				}
			}
		}

		//----------------------------------------------------------------------------
		if (stricmp(strTag, "RotationList") == 0)
		{
			uint32 num = node->getChildCount();
			poseBlenderAimDesc.m_rotations.reserve(num);
			for (uint32 i = 0; i < num; i++)
			{
				SJointsAimIK_Rot AimIK_Rot;
				XmlNodeRef nodeRot = node->getChild(i);
				const char* RotTag = nodeRot->getTag();
				if (stricmp(RotTag, "Rotation") == 0)
				{
					const char* pJointName = nodeRot->getAttr("JointName");
					int32 jidx = pDefaultSkeleton->GetJointIDByName(pJointName);
					if (jidx > 0)
					{
						AimIK_Rot.m_nJointIdx = jidx;
						AimIK_Rot.m_strJointName = pDefaultSkeleton->GetJointNameByID(jidx);
					}
					else
					{
						poseBlenderAimDesc.m_error++;
						g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pDefaultSkeleton->GetModelFilePath(), "Aim-Pose Error: JointName '%s' for Aim-Pose not found in model", pJointName);
					}
					assert(jidx > 0);
					nodeRot->getAttr("Primary", AimIK_Rot.m_nPreEvaluate);
					nodeRot->getAttr("Additive", AimIK_Rot.m_nAdditive);
					//	AimIK_Rot.m_nAdditive=0;
					const uint32 numRotations = poseBlenderAimDesc.m_rotations.size();
					const int32 parentJointIndex = pDefaultSkeleton->GetJointParentIDByID(jidx);
					for (uint32 p = 0; p < numRotations; ++p)
					{
						const SJointsAimIK_Rot& otherAimIkRot = poseBlenderAimDesc.m_rotations[p];
						if (AimIK_Rot.m_nPreEvaluate && otherAimIkRot.m_nPreEvaluate)
						{
							const bool isCurrentJointParentOfPreviousJoint = FindJointInParentHierarchy(pDefaultSkeleton, jidx, otherAimIkRot.m_nJointIdx);
							if (isCurrentJointParentOfPreviousJoint)
							{
								poseBlenderAimDesc.m_error++;
								g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pDefaultSkeleton->GetModelFilePath(), "Primary joint with name '%s' for Aim-Pose should not be declared after joint '%s'.", pJointName, otherAimIkRot.m_strJointName);
							}
						}
						if (otherAimIkRot.m_nJointIdx == parentJointIndex)
						{
							AimIK_Rot.m_nRotJointParentIdx = p;
							break;
						}
					}
					poseBlenderAimDesc.m_rotations.push_back(AimIK_Rot);
				}
			}
		}

		//----------------------------------------------------------------------------------------

		if (stricmp(strTag, "PositionList") == 0)
		{
			uint32 num = node->getChildCount();
			poseBlenderAimDesc.m_positions.reserve(num);
			for (uint32 i = 0; i < num; i++)
			{
				SJointsAimIK_Pos AimIK_Pos;
				XmlNodeRef nodePos = node->getChild(i);
				const char* PosTag = nodePos->getTag();
				if (stricmp(PosTag, "Position") == 0)
				{
					const char* pJointName = nodePos->getAttr("JointName");
					int32 jidx = pDefaultSkeleton->GetJointIDByName(pJointName);
					if (jidx > 0)
					{
						AimIK_Pos.m_nJointIdx = jidx;
						AimIK_Pos.m_strJointName = pDefaultSkeleton->GetJointNameByID(jidx);
					}
					else
					{
						poseBlenderAimDesc.m_error++;
						g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pDefaultSkeleton->GetModelFilePath(), "Aim-Pose Error: JointName '%s' for Aim-Pose not found in model", pJointName);
					}
					assert(jidx > 0);
					nodePos->getAttr("Additive", AimIK_Pos.m_nAdditive);
					poseBlenderAimDesc.m_positions.push_back(AimIK_Pos);
				}
			}
		}

		//----------------------------------------------------------------------------------------
		if (stricmp(strTag, "ProcAdjustments") == 0)
		{
			uint32 num = node->getChildCount();
			pDefaultSkeleton->m_recoilDesc.m_joints.reserve(num);
			for (uint32 i = 0; i < num; i++)
			{
				ProcAdjust Spine;
				XmlNodeRef nodeRot = node->getChild(i);
				const char* SpineTag = nodeRot->getTag();
				if (stricmp(SpineTag, "Spine") == 0)
				{
					const char* pJointName = nodeRot->getAttr("JointName");
					int32 jidx = pDefaultSkeleton->GetJointIDByName(pJointName);
					if (jidx > 0)
					{
						Spine.m_nIdx = jidx;
						Spine.m_strJointName = pDefaultSkeleton->GetJointNameByID(jidx);
					}
					else
					{
						poseBlenderAimDesc.m_error++;
						g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pDefaultSkeleton->GetModelFilePath(), "Aim-Pose Error: ProcAdjustment JointName '%s' for Aim-Pose not found in model", pJointName);
					}
					poseBlenderAimDesc.m_procAdjustments.push_back(Spine);
				}
			}
		}
	}

	{
		InitalizeBlendRotJointIndices(poseBlenderAimDesc.m_blends, poseBlenderAimDesc.m_rotations);
	}

	//----------------------------------------------------------------------------------------
	if (poseBlenderAimDesc.m_error)
	{
		poseBlenderAimDesc.m_rotations.clear();
		poseBlenderAimDesc.m_positions.clear();
		poseBlenderAimDesc.m_procAdjustments.clear();
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pDefaultSkeleton->GetModelFilePath(), "CryAnimation Error: Aim-Poses disabled");
	}

	return true;
}

#ifdef EDITOR_PCDEBUGCODE
void CParamLoader::InjectCHRPARAMS(const char* filename, const char* content, size_t contentLength)
{
	string key = filename;
	key.MakeLower();
	m_cachedCHRPARAMS[key].assign(content, content + contentLength);
}

bool CParamLoader::HasInjectedCHRPARAMS(const char* filename) const
{
	string key = filename;
	key.MakeLower();
	return m_cachedCHRPARAMS.find(key) != m_cachedCHRPARAMS.end();
}

void CParamLoader::ClearCHRPARAMSCache()
{
	stl::free_container(m_cachedCHRPARAMS);
}
#endif
