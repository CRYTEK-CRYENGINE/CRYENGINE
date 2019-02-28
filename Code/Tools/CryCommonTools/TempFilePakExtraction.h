// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Description: Opens a temporary file for read only access, where the file could be
// located in a zip or pak file. Note that if the file specified
// already exists it does not delete it when finished.

#ifndef __TEMPFILEPAKEXTRACTION_H__
#define __TEMPFILEPAKEXTRACTION_H__

struct IPakSystem;

class TempFilePakExtraction
{
public:
	TempFilePakExtraction(const char* filename, const char* tempPath, IPakSystem* pPakSystem);
	~TempFilePakExtraction();

	const string& GetTempName() const
	{
		return m_strTempFileName;
	}

	const string& GetOriginalName() const
	{
		return m_strOriginalFileName;
	}

	bool HasTempFile() const;

private:
	string m_strTempFileName;
	string m_strOriginalFileName;
};

#endif //__TEMPFILEPAKEXTRACTION_H__
