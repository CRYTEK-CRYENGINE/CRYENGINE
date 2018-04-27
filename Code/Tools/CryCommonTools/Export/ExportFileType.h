// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __EXPORTFILETYPE_H__
#define __EXPORTFILETYPE_H__

enum CryFileType
{
	CRY_FILE_TYPE_NONE = 0x0000,
	CRY_FILE_TYPE_CGF = 0x0001,
	CRY_FILE_TYPE_CGA = 0x0002,
	CRY_FILE_TYPE_CHR = 0x0004,
	CRY_FILE_TYPE_CAF = 0x0008,
	CRY_FILE_TYPE_ANM = 0x0010,
	CRY_FILE_TYPE_SKIN = 0x0020,
	CRY_FILE_TYPE_INTERMEDIATE_CAF = 0x0040,
};

namespace ExportFileTypeHelpers
{
	const char* CryFileTypeToString(int cryFileType);
	int StringToCryFileType(const char* str);
};


#endif

