// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __pakhelpers_h__
#define __pakhelpers_h__
#pragma once

#include <map>                 // stl multimap<>

#include "RcFile.h"

namespace PakHelpers
{
	enum ESortType
	{
		eSortType_NoSort,               // don't sort files
		eSortType_Size,                 // sort files by size
		eSortType_Streaming,            // sort files by extension+type+name+size
		eSortType_Suffix,               // sort files by suffix+name+extension
		eSortType_Alphabetically,       // sort files by full path
	};

	enum ESplitType
	{
		eSplitType_Original,            // don't create paks differing from the original configuration (XML or command line)
		eSplitType_Basedir,             // split paks by base directory name
		eSplitType_ExtensionMipmap,     // split paks by extension and mipmap level (high/low)
		eSplitType_Suffix,              // split paks by suffix
	};

	enum ETextureType 
	{
		eTextureType_Diffuse,
		eTextureType_Normal,
		eTextureType_Specular,
		eTextureType_Detail,
		eTextureType_Mask,
		eTextureType_SubSurfaceScattering,
		eTextureType_Cubemap,
		eTextureType_Colorchart,
		eTextureType_Displacement,
		eTextureType_Undefined,
	};

	struct PakEntry
	{
		PakEntry()
			: m_sourceFileSize(-1)
			, m_bIsLastMip(false)
			, m_textureType(eTextureType_Undefined)
		{
		}

		RcFile m_rcFile;
		__int64 m_sourceFileSize;
		bool m_bIsLastMip;

		string m_streamingSuffix;
		string m_extension;
		ETextureType m_textureType;
		string m_baseName;
		string m_innerDir;

		static bool MakeSortableStreamingSuffix(const string& suffix, string* sortable = NULL, int nDigits = 2, int nIncrement = 0);

		string GetRealFilename() const;
		string GetStreamingSuffix() const;
		string GetExtension() const;
		string GetNameWithoutExtension(bool bFilenameOnly) const;
		string GetDirnameWithoutFile(bool bRootdirOnly) const;

		ETextureType GetTextureType() const;
	};

	size_t CreatePakEntryList(
		const std::vector<RcFile>& files,
		std::map<string, std::vector<PakEntry> >& pakEntries, 
		ESortType eSortType,
		ESplitType eSplitType,
		const string& sPakName);	
}

#endif // __pakhelpers_h__
