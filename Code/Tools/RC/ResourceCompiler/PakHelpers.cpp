// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PakHelpers.h"

#include "PathHelpers.h"
#include "StringHelpers.h"
#include <CryString/StringUtils.h>
#include <CryString/CryPath.h>
#include "FileUtil.h"
#include "IRCLog.h"

//////////////////////////////////////////////////////////////////////////
bool PakHelpers::PakEntry::MakeSortableStreamingSuffix(const string& suffix, string* sortable, int nDigits, int nIncrement)
{
	int nMipmap = 0;
	char cAttached = '\0';
	char verify[32] = "";
	
	// Scan, recreate and compare to verify if the given string is a valid streaming suffix
	int nTokens = sscanf(suffix.data(), "%d%c", &nMipmap, &cAttached);
	switch (nTokens)
	{
	case 2:
		// .dds.0a, .dds.1a, etc.
		cry_sprintf(verify, "%d%c", nMipmap, cAttached);
		break;
	case 1:
		// .dds.0, .dds.1, etc.
		cry_sprintf(verify, "%d", nMipmap);
		break;
	default:
		// .dds.a
		if (sscanf(suffix.data(), "%c", &cAttached) == 1)
		{
			cry_sprintf(verify, "%c", cAttached);
			nTokens = 2;
		}
		break;
	}
	
	if (!StringHelpers::Equals(suffix, verify) || ((nTokens > 1) && (cAttached != 'a')))
	{
		return false;
	}

	// Recreate a streaming suffix we can use to sort (constant length)
	if (sortable)
	{
		char digits[32];

		if (nDigits > 0)
		{
			cry_sprintf(digits, "%%0%dd", nDigits);
		}
		else
		{
			cry_strcpy(digits, "%d");
		}

		if (nTokens > 1)
		{
			cry_strcat(digits, "%c");
		}

		switch (nTokens)
		{
		case 2:
			cry_sprintf(verify, digits, nMipmap + nIncrement, cAttached);
			break;
		case 1:
			cry_sprintf(verify, digits, nMipmap + nIncrement);
			break;
		default:
			break;
		}

		*sortable = verify;
	}

	return true;
}

string PakHelpers::PakEntry::GetRealFilename() const
{
	const string sRealFilename = PathUtil::Make(m_rcFile.m_sourceLeftPath, 
		m_rcFile.m_sourceInnerPathAndName);
	return sRealFilename;
}

string PakHelpers::PakEntry::GetStreamingSuffix() const
{
	const size_t splitter = m_rcFile.m_sourceInnerPathAndName.find_last_of('.');
	if (splitter == string::npos) 
	{
		return string();
	}

	// for split DDS files the file extension is .dds, .dds.1, .dds.a, .dds.1a ...
	// Take the number part, and return just the mip number and alpha name
	const string suffix = m_rcFile.m_sourceInnerPathAndName.substr(splitter + 1);
	string sortablesuffix;
	if (!MakeSortableStreamingSuffix(suffix, &sortablesuffix))
	{
		return string();
	}

	return sortablesuffix;
}

string PakHelpers::PakEntry::GetExtension() const
{
	size_t splitter = m_rcFile.m_sourceInnerPathAndName.find_last_of('.');
	if (splitter == string::npos) 
	{
		return string();
	}

	// for DDS files on consoles the file extension is .dds.0, .dds.1, .dds.0a, .dds.1a ...
	// Skip the number part, and return the actual file extension without the mip number and alpha name
	const string extension = m_rcFile.m_sourceInnerPathAndName.substr(splitter + 1);
	if (!MakeSortableStreamingSuffix(extension))
	{
		return extension;
	}

	const string tmp = m_rcFile.m_sourceInnerPathAndName.substr(0, splitter);
	splitter = tmp.find_last_of('.');
	if (splitter == string::npos) 
	{
		return string();
	}

	return tmp.substr(splitter + 1);
}

string PakHelpers::PakEntry::GetNameWithoutExtension(bool bFilenameOnly) const
{
	string name = m_rcFile.m_sourceInnerPathAndName;

	if (bFilenameOnly)
	{
		name = PathUtil::GetFile(name);
	}

	size_t splitter = name.find_last_of('.');
	if (splitter == string::npos) 
	{
		return name;
	}

	// for DDS files on consoles the file extension is .dds.0, .dds.1, .dds.0a, .dds.1a ...
	// Skip the number part, and return the actual file extension without the mip number and alpha name
	const string extension = name.substr(splitter + 1);
	if (MakeSortableStreamingSuffix(extension))
	{
		name = name.substr(0, splitter);
	}

	splitter = name.find_last_of('.');
	if (splitter == string::npos) 
	{
		return name;
	}

	return name.substr(0, splitter);
}

string PakHelpers::PakEntry::GetDirnameWithoutFile(bool bRootdirOnly) const
{
	string name = m_rcFile.m_sourceInnerPathAndName;

	if (bRootdirOnly)
	{
		const size_t splitter = name.find_first_of("\\/");
		if (splitter != string::npos) 
		{
			name = name.substr(0, splitter);
		}
		else
		{
			name = "";
		}
	}
	else
	{
		name = PathHelpers::GetDirectory(name);
	}

	return name;
}

PakHelpers::ETextureType PakHelpers::PakEntry::GetTextureType() const
{
	const string sLowerCaseName = StringHelpers::MakeLowerCase(PathUtil::RemoveExtension(m_rcFile.m_sourceInnerPathAndName));
	if (StringHelpers::EndsWith(sLowerCaseName, "_diff"))
	{
		return eTextureType_Diffuse;
	}
	if (StringHelpers::EndsWith(sLowerCaseName, "_ddn") ||
		  StringHelpers::EndsWith(sLowerCaseName, "_ddna"))
	{
		return eTextureType_Normal;
	}
	if (StringHelpers::EndsWith(sLowerCaseName, "_spec"))
	{
		return eTextureType_Specular;
	}
	if (StringHelpers::EndsWith(sLowerCaseName, "_detail"))
	{
		return eTextureType_Detail;
	}
	if (StringHelpers::EndsWith(sLowerCaseName, "_mask"))
	{
		return eTextureType_Mask;
	}
	if (StringHelpers::EndsWith(sLowerCaseName, "_sss"))
	{
		return eTextureType_SubSurfaceScattering;
	}
	if (StringHelpers::EndsWith(sLowerCaseName, "_cm") ||
	    StringHelpers::EndsWith(sLowerCaseName, "_cubemap"))
	{
		return eTextureType_Cubemap;
	}
	if (StringHelpers::EndsWith(sLowerCaseName, "_cch"))
	{
		return eTextureType_Colorchart;
	}
	if (StringHelpers::EndsWith(sLowerCaseName, "_displ") ||
	    StringHelpers::EndsWith(sLowerCaseName, "_dmap"))
	{
		return eTextureType_Displacement;
	}

	return eTextureType_Undefined;
}

//////////////////////////////////////////////////////////////////////////

namespace
{
	struct StreamingFileOrder
	{
		bool operator()(const PakHelpers::PakEntry& left, const PakHelpers::PakEntry& right) const
		{
			// first sort by extension
			const string& extA = left.m_extension;
			const string& extB = right.m_extension;
			int res = StringHelpers::CompareIgnoreCase(extA, extB);
			if (res != 0)
			{
				return res < 0;
			}

			if (StringHelpers::EqualsIgnoreCase(extA, "dds"))
			{	
				// then sort dds textures by type
				const PakHelpers::ETextureType eAType = left.m_textureType;
				const PakHelpers::ETextureType eBType = right.m_textureType;
				if (eAType != eBType)
				{
					return eAType < eBType; 
				}

				// then sort by name
				const string& shortNameA = PathUtil::Make(left.m_innerDir, left.m_baseName);
				const string& shortNameB = PathUtil::Make(right.m_innerDir, right.m_baseName);
				res = StringHelpers::CompareIgnoreCase(shortNameA, shortNameB);
				if (res != 0)
				{
					return res < 0;
				}

				if (left.m_sourceFileSize != right.m_sourceFileSize)
				{
					return left.m_sourceFileSize < right.m_sourceFileSize;
				}
			} 
			else 
			{
				/*
				// The following code is commented out because some of the files
				// are loaded directly from an alphabetical resource list which breaks
				// the whole loading times
				if (left.m_sourceFileSize != right.m_sourceFileSize)
				{
					return left.m_sourceFileSize < right.m_sourceFileSize;
				}
				*/
			}

			res = StringHelpers::CompareIgnoreCase(
				left.m_rcFile.m_sourceInnerPathAndName, right.m_rcFile.m_sourceInnerPathAndName);
			return res < 0;
		}
	};

	struct SizeFileOrder
	{
		bool operator()(const PakHelpers::PakEntry& left, const PakHelpers::PakEntry& right) const
		{
			return left.m_sourceFileSize < right.m_sourceFileSize;
		}
	};

	struct AlphabeticalFileOrder
	{
		bool operator()(const PakHelpers::PakEntry& left, const PakHelpers::PakEntry& right) const
		{
			const int res = StringHelpers::CompareIgnoreCase(
				left.m_rcFile.m_sourceInnerPathAndName, right.m_rcFile.m_sourceInnerPathAndName);
			return res < 0;
		}
	};

	struct StreamingSuffixFileOrder : public AlphabeticalFileOrder
	{
		bool operator()(const PakHelpers::PakEntry& left, const PakHelpers::PakEntry& right) const
		{
			// first sort by streaming suffix
			const string& sfxA = left.m_streamingSuffix;
			const string& sfxB = right.m_streamingSuffix;

			// Empty suffices always at the end of the PAK, no matter what
			if (!sfxA.empty() || !sfxB.empty())
			{
				if (sfxA.empty())
				{
					return true;
				}

				if (sfxB.empty())
				{
					return false;
				}

				const int res = StringHelpers::CompareIgnoreCase(sfxA, sfxB);
				if (res != 0)
				{
					return res < 0;
				}
			}

			// then sort by name
			const string& shortNameA = left.m_baseName;
			const string& shortNameB = right.m_baseName;

			int res = StringHelpers::CompareIgnoreCase(shortNameA, shortNameB);
			if (res != 0)
			{
				return res < 0;
			}

			// then sort by extension
			const string& extA = left.m_extension;
			const string& extB = right.m_extension;
			res = StringHelpers::CompareIgnoreCase(extA, extB);
			if (res != 0)
			{
				return res < 0;
			}

			// then alphabetically
			return AlphabeticalFileOrder::operator()(left, right);
		}
	};
}

//////////////////////////////////////////////////////////////////////////

size_t PakHelpers::CreatePakEntryList(
	const std::vector<RcFile>& files,
	std::map<string, std::vector<PakHelpers::PakEntry> >& pakEntries, 
	PakHelpers::ESortType eSortType,
	PakHelpers::ESplitType eSplitType,
	const string& sPakName)
{
	const string sPakBase = PathUtil::RemoveExtension(PathUtil::GetFile(sPakName));
	string sPakDir;
	size_t entryCount = 0;

	for (std::vector<RcFile>::const_iterator it = files.begin(); it != files.end(); ++it)
	{
		const RcFile& file = *it;
		string sNewPakName = sPakName;
		bool bSkip = false;

		PakEntry entry;
		entry.m_rcFile = file;
		entry.m_rcFile.m_sourceLeftPath = PathUtil::ToDosPath(file.m_sourceLeftPath);
		entry.m_rcFile.m_sourceInnerPathAndName = PathUtil::ToDosPath(file.m_sourceInnerPathAndName);
		entry.m_rcFile.m_targetLeftPath = PathUtil::ToDosPath(file.m_targetLeftPath);

		// cache values used for fast sorting
		entry.m_streamingSuffix = entry.GetStreamingSuffix();
		entry.m_extension = entry.GetExtension();
		entry.m_textureType = entry.GetTextureType();
		entry.m_baseName = entry.GetNameWithoutExtension(true);
		entry.m_innerDir = entry.GetDirnameWithoutFile(false);
		entry.m_sourceFileSize = FileUtil::GetFileSize(entry.GetRealFilename());

		if (eSplitType == eSplitType_ExtensionMipmap)
		{
			if (entry.m_sourceFileSize >= 0)
			{
				if (StringHelpers::EqualsIgnoreCase(entry.m_extension, "dds"))
				{
					if (!entry.m_streamingSuffix.empty())
					{
						const string szPlainSuffix = entry.m_rcFile.m_sourceInnerPathAndName.substr(entry.m_rcFile.m_sourceInnerPathAndName.find_last_of('.') + 1);
						string szIncrementedSuffix = "";

						if (entry.MakeSortableStreamingSuffix(szPlainSuffix, &szIncrementedSuffix, 0, 1))
						{
							const string tmpFilename = PathUtil::Make(file.m_sourceLeftPath, 
								entry.m_innerDir + "\\" + entry.m_baseName + "." + entry.m_extension + "." + szIncrementedSuffix);

							entry.m_bIsLastMip = !FileUtil::FileExists(tmpFilename);
						}
					}

					if (entry.m_bIsLastMip)
					{
						sNewPakName = sPakName + string("streaming\\dds_high.pak");
					} 
					else 
					{
						sNewPakName = sPakName + string("streaming\\dds_low.pak");
					}
				} 
				else 
				{
					sNewPakName = sPakName + string("streaming\\") + entry.m_extension + string(".pak");
				}
			}
			else
			{
				bSkip = true;
			}
		}
		else if (eSplitType == eSplitType_Suffix)
		{
			// filter files without suffices > 0 into different pak
			if (!entry.m_streamingSuffix.empty())
			{
				const string tmpFilename = PathUtil::Make(file.m_sourceLeftPath, 
					entry.m_innerDir + "\\" + entry.m_baseName + "." + entry.m_extension + ".1");

				if (!FileUtil::FileExists(tmpFilename))
				{
					entry.m_streamingSuffix = "";//"00o";
				}
			}

			sNewPakName = sPakDir + sPakBase + "-m" + entry.m_streamingSuffix + string(".pak");
		}
		else if (eSplitType == eSplitType_Basedir)
		{
			sNewPakName = sPakDir + entry.GetDirnameWithoutFile(true) + string(".pak");
		}
		else 
		{
			if (eSortType == eSortType_Size)
			{
				bSkip = !(entry.m_sourceFileSize >= 0);
			} 
		}

		if (StringHelpers::EqualsIgnoreCase(entry.m_extension, "$dds") ||
		    StringHelpers::EqualsIgnoreCase(entry.m_extension, "pak"))
		{
			bSkip = true;
		}

		if (!bSkip)
		{
			pakEntries[sNewPakName].push_back(entry);
			++entryCount;
		}
	}

	// sort the entries by requested sorting operator
	for (std::map<string, std::vector<PakEntry> >::iterator it = pakEntries.begin(); it != pakEntries.end(); ++it)
	{
		std::vector<PakEntry>& files = it->second;

		// Sort by type
		switch (eSortType)
		{
		case PakHelpers::eSortType_NoSort:
			{
				RCLog("Using sort method to add to pack : nosort");
				break;
			}
		case PakHelpers::eSortType_Size:
			{
				RCLog("Using sort method to add to pack : size");
				// Sort alphabetically first so there is a consistent ordering before sorting by size
				// (to ensure that files with the same size are ordered by name)
				std::sort(files.begin(), files.end(), AlphabeticalFileOrder());
				std::stable_sort(files.begin(), files.end(), SizeFileOrder());
				break;
			}	
		case PakHelpers::eSortType_Streaming:
			{
				RCLog("Using sort method to add to pack : streaming");
				std::sort(files.begin(), files.end(), StreamingFileOrder());
				break;
			}
		case PakHelpers::eSortType_Suffix:
			{
				RCLog("Using sort method to add to pack : suffix");
				std::sort(files.begin(), files.end(), StreamingSuffixFileOrder());
				break;
			}
		case PakHelpers::eSortType_Alphabetically:
			{
				RCLog("Using sort method to add to pack : alphabetically");				
				std::sort(files.begin(), files.end(), AlphabeticalFileOrder());
				break;
			}	
		default:
			{
				assert(0);
				RCLogError("Using sort method to add to pack : undefined!");
				break;
			}
		}
	}

	return entryCount;
}

//////////////////////////////////////////////////////////////////////////