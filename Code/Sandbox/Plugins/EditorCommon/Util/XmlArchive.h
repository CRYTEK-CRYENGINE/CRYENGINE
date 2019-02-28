// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __XmlArchive_h__
#define __XmlArchive_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "EditorCommonAPI.h"
#include "NamedData.h"

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
	};
	explicit CXmlArchive(const string& xmlRoot)
	{
		bLoading = false;
		bOwnNamedData = true;
		pNamedData = new CNamedData;
		root = XmlHelpers::CreateXmlNode(xmlRoot);
	};
	~CXmlArchive()
	{
		if (bOwnNamedData)
			delete pNamedData;
	};
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
	//! If pNamedData it may save it into a set of additional files in the target file filder.
	//! \see CNamedData::Save
	bool SaveToFile(const string& filepath);

	bool LoadFromFile(const string& filepath);

};

#endif // __XmlArchive_h__

