// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "NamedData.h"
#include "EditorUtils.h"
#include <CrySystem/XML/IXml.h>

class CPakFile;
/*!
 *	CXmlArcive used to stores XML in MFC archive.
 */
class EDITOR_COMMON_API CXmlArchive
{
public:
	XmlNodeRef  root;
	CNamedData* pNamedData;
	bool        bLoading;
	bool        bOwnNamedData;

	CXmlArchive()
	{
		bLoading = false;
		bOwnNamedData = true;
		pNamedData = new CNamedData;
	}
	explicit CXmlArchive(const string& xmlRoot)
	{
		bLoading = false;
		bOwnNamedData = true;
		pNamedData = new CNamedData;
		root = XmlHelpers::CreateXmlNode(xmlRoot);
	}
	~CXmlArchive()
	{
		if (bOwnNamedData)
			delete pNamedData;
	}
	CXmlArchive(const CXmlArchive& ar) { *this = ar; }
	CXmlArchive& operator=(const CXmlArchive& ar)
	{
		root = ar.root;
		pNamedData = ar.pNamedData;
		bLoading = ar.bLoading;
		bOwnNamedData = false;
		return *this;
	}

	bool Load(const string& file);
	void Save(const string& file);
	bool Load(const char* file) { return Load(string(file)); } // for CString conversion
	void Save(const char* file) { Save(string(file)); }

	//! Save XML Archive to pak file.
	//! @return true if saved.
	bool SaveToPak(const string& levelPath, CPakFile& pakFile);
	bool LoadFromPak(const string& levelPath);

	//! Save the XML archive to the specified file.
	//! If pNamedData it may save it into a set of additional files in the target file folder.
	//! \see CNamedData::Save
	bool SaveToFile(const string& filepath);

	bool LoadFromFile(const string& filepath);
};
