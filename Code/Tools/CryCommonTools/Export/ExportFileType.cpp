// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ExportFileType.h"
#include "StringHelpers.h"


struct SFileTypeInfo
{
	int type;
	const char* name;
};

SFileTypeInfo s_fileTypes[] =
{
	{ CRY_FILE_TYPE_CGF, "cgf" },
	{ CRY_FILE_TYPE_CGA, "cga" },
	{ CRY_FILE_TYPE_CHR, "chr" },
	{ CRY_FILE_TYPE_CAF, "caf" },
	{ CRY_FILE_TYPE_ANM, "anm" },
	{ CRY_FILE_TYPE_CHR|CRY_FILE_TYPE_CAF, "chrcaf" },
	{ CRY_FILE_TYPE_CGA|CRY_FILE_TYPE_ANM, "cgaanm" },
	{ CRY_FILE_TYPE_SKIN, "skin" },
	{ CRY_FILE_TYPE_INTERMEDIATE_CAF, "i_caf" },
};

static const int s_fileTypeCount = (sizeof(s_fileTypes) / sizeof(s_fileTypes[0]));


const char* ExportFileTypeHelpers::CryFileTypeToString(int const cryFileType)
{
	for (int i=0; i<s_fileTypeCount; ++i)
	{
		if (s_fileTypes[i].type == cryFileType)
		{
			return s_fileTypes[i].name;
		}
	}
	return "unknown";
}

int ExportFileTypeHelpers::StringToCryFileType(const char* str)
{
	if (str)
	{
		for (int i=0; i<s_fileTypeCount; ++i)
		{
			if (stricmp(str, s_fileTypes[i].name) == 0)
			{
				return s_fileTypes[i].type;
			}
		}
	}
	return CRY_FILE_TYPE_NONE;
}
