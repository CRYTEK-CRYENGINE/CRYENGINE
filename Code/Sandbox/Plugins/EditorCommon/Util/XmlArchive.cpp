// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "XmlArchive.h"
#include "Util/PakFile.h"

//////////////////////////////////////////////////////////////////////////
// CXmlArchive
bool CXmlArchive::Load(const string& file)
{
	bLoading = true;

	CFile cFile;
	if (!cFile.Open(file, CFile::modeRead))
	{
		CryLog("Warning: Loading of %s failed", (const char*)file);
		return false;
	}
	CArchive ar(&cFile, CArchive::load);

	string str;
	ar >> str;

	try
	{
		pNamedData->Serialize(ar);
	}
	catch (...)
	{
		CryLog("Error: Can't load xml file: '%s'! File corrupted. Binary file possibly was corrupted by Source Control if it was marked like text format.", (const char*)file);
		return false;
	}

	root = XmlHelpers::LoadXmlFromBuffer(str.GetBuffer(), str.GetLength());
	if (!root)
	{
		CryLog("Warning: Loading of %s failed", (const char*)file);
	}

	if (root)
	{ 
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CXmlArchive::Save(const string& file)
{
	string filename(file);
	if (GetIEditor()->GetConsoleVar("ed_lowercasepaths"))
		filename = filename.MakeLower();

	bLoading = false;
	if (!root)
	{
		return;
	}

	CFile cFile;
	// Open the file for writing, create it if needed
	if (!cFile.Open(filename, CFile::modeCreate | CFile::modeWrite))
	{
		CryLog("Warning: Saving of %s failed", filename.GetBuffer());
		return;
	}
	// Create the archive object
	CArchive ar(&cFile, CArchive::store);

	_smart_ptr<IXmlStringData> pXmlStrData = root->getXMLData(5000000);

	// Need convert to string for CArchive::operator<<
	string str = pXmlStrData->GetString();
	ar << str;

	pNamedData->Serialize(ar);
}

//////////////////////////////////////////////////////////////////////////
bool CXmlArchive::SaveToPak(const string& levelPath, CPakFile& pakFile)
{
	LOADING_TIME_PROFILE_SECTION;
	_smart_ptr<IXmlStringData> pXmlStrData = root->getXMLData(5000000);

	// Save xml file.
	string xmlFilename = "Level.editor_xml";
	pakFile.UpdateFile(xmlFilename, (void*)pXmlStrData->GetString(), pXmlStrData->GetStringLength());

	CryLog("Saving pak file %s", (const char*)pakFile.GetArchive()->GetFullPath());

	pNamedData->Save(pakFile);
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CXmlArchive::LoadFromPak(const string& levelPath)
{
	string xmlFilename = levelPath + "Level.editor_xml";
	root = XmlHelpers::LoadXmlFromFile(xmlFilename);
	if (!root)
	{
		return false;
	}

	return pNamedData->Load(levelPath);
}

bool CXmlArchive::SaveToFile(const string& filepath)
{
	LOADING_TIME_PROFILE_SECTION;

	if (!root->saveToFile(filepath))
	{
		return false;
	}

	pNamedData->Save(PathUtil::GetPathWithoutFilename(filepath));
	return true;
}

bool CXmlArchive::LoadFromFile(const string& filepath)
{
	root = XmlHelpers::LoadXmlFromFile(filepath);
	if (!root)
	{
		return false;
	}

	return pNamedData->Load(PathUtil::GetPathWithoutFilename(filepath));
}

