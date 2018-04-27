// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ResourceCompiler.h"
#include <set>
#include "ZipEncryptor.h"
#include "ZipDir/ZipDir.h"
#include "FileUtil.h"
#include "StringHelpers.h"
	
ZipEncryptor::ZipEncryptor(IResourceCompiler* pRC)
{
}

ZipEncryptor::~ZipEncryptor()
{
}

ICompiler* ZipEncryptor::CreateCompiler()
{
	return this;
}

void ZipEncryptor::Release()
{
}

string ZipEncryptor::GetOutputFileNameOnly() const
{
	return m_CC.sourceFileNameOnly;
}

string ZipEncryptor::GetOutputPath() const
{
	return PathUtil::Make(m_CC.GetOutputFolder(), GetOutputFileNameOnly());
}

const char* ZipEncryptor::GetExt(int index) const
{
	switch (index)
	{
	case 0:
		return "pak";
	case 1:
		return "zip";
	default:
		return 0;
	}
}

namespace {

struct EncryptPredicate : ZipDir::IEncryptPredicate
{
	string m_filter;
	std::vector<string> m_filterItems;

	explicit EncryptPredicate(const string& filter)
		: m_filter(filter)
	{
		if (!filter.empty())
		{
			StringHelpers::Split(filter, ";", false, m_filterItems);
		}
	}

	virtual bool Match(const char* filename)
	{
		size_t count = m_filterItems.size();
		for (size_t i = 0; i < count; ++i)
		{
			if (StringHelpers::MatchesWildcardsIgnoreCase(filename, m_filterItems[i]))
			{
				return true;
			}
		}
		return false;
	}
};

}
	
bool ZipEncryptor::Process()
{
	const IConfig* config = m_CC.config;

	if (!m_CC.config->HasKey("zip_encrypt"))
	{
		RCLogError("zip_encrypt option is not specified.");
		return false;
	}
	const bool zipEncrypt = m_CC.config->GetAsBool("zip_encrypt", false, true);
	
	const int zipFileAlignment = config->GetAsInt("zip_alignment", 1, 1);

	uint32 encryptionKey[4];
	const string zipEncryptKey = config->GetAsString("zip_encrypt_key", "", "");

	if (!zipEncryptKey.empty())
	{
		if (!ParseKey(encryptionKey, zipEncryptKey.c_str()))
		{
			RCLogError("Misformed zip_encrypt_key: expected 128-bit integer in hexadecimal format (32 character)");
			return false;
		}
	}

	const string outputPath = GetOutputPath();

	RCLog(zipEncrypt ? "Encrypting zip: %s" : "Decrypting zip: %s", outputPath.c_str());

	if ( !FileUtil::FileExists(m_CC.GetSourcePath()) )
	{
		RCLogError("Non-existing input file: %s", m_CC.GetSourcePath().c_str());
		return false;
	}

	if (!StringHelpers::EqualsIgnoreCase(m_CC.GetSourcePath().c_str(), outputPath.c_str()))
	{
		if (CopyFileA(m_CC.GetSourcePath(), outputPath.c_str(), FALSE) == FALSE)
		{
			RCLogError("Unable to copy archive from %s to %s.", m_CC.GetSourcePath().c_str(), outputPath.c_str());
			return false;
		}
	}

	PakSystemArchive *pPakFile = m_CC.pRC->GetPakSystem()->OpenArchive(outputPath.c_str(), zipFileAlignment, zipEncrypt, zipEncryptKey.empty() ? 0 : encryptionKey);
	if (!pPakFile)
	{
		RCLogError("Failed to open zip file %s", outputPath.c_str());
		return false;
	}

	const string zipEncryptFilter = config->GetAsString("zip_encrypt_filter", "", "");
	std::auto_ptr<EncryptPredicate> encryptPredicate(new EncryptPredicate(zipEncryptFilter));

	int numChanged = 0;
	int numSkipped = 0;

	if (!pPakFile->zip->EncryptArchive(zipEncrypt ? pPakFile->zip->ENCRYPT : pPakFile->zip->DECRYPT, encryptPredicate.get(), &numChanged, &numSkipped))
	{
		RCLogError("PAK encryption failed. Archive is corrupted.");
		return false;
	}

	RCLog(zipEncrypt ? "Encrypted content of %i/%i files" : "Decrypted content of %i/%i files", numChanged, numChanged + numSkipped);

	m_CC.pRC->GetPakSystem()->CloseArchive(pPakFile);
	return true;
}

static unsigned char s_charValueTable[256] = 
{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	   0,    1,    2,    3,    4,    5,    6,    7,
	   8,    9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff,   10,   11,   12,   13,   14,   15, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff,   10,   11,   12,   13,   14,   15, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

bool ZipEncryptor::ParseKey(uint32 outputKey[4], const char* inputString)
{
	if (!outputKey)
	{
		assert(outputKey);
		return false;
	}
	if (!inputString)
	{
		assert(inputString);
		return false;
	}

	size_t numBytes = sizeof(uint32) * 4;
	size_t len = strlen(inputString);

	if (len != numBytes * 2)
	{
		RCLogError("Encryption key should contain %i characters.", numBytes * 2);
		return false;
	}

	size_t i = 0;
	while (i != numBytes)
	{
		unsigned char v1 = s_charValueTable[(unsigned char)(inputString[i * 2])];
		unsigned char v2 = s_charValueTable[(unsigned char)(inputString[i * 2 + 1])];
		if (v1 == 0xff || v2 == 0xff)
		{
			size_t pos = i * 2 + (v1 == 0xff ? 1 : 2);
			RCLogError("Encryption key contains bad character at position %i", pos);
			return false;
		}

		((unsigned char*)outputKey)[numBytes - i - 1] = v2 + (v1 << 4);
		++i;
	}

	return true;
}
