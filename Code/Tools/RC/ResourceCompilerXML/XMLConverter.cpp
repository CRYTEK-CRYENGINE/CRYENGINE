// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "XMLConverter.h"
#include "IRCLog.h"
#include "IXmlSerializer.h"
#include <CrySystem/XML/XMLBinaryHeaders.h>
#include "XMLBinaryReader.h"
#include "XMLBinaryWriter.h"
#include "IConfig.h"
#include "IResCompiler.h"
#include "FileUtil.h"
#include "StringHelpers.h"
#include "UpToDateFileHelpers.h"
#include <CryCore/Platform/CryWindows.h>   // needed for DbgHelp.h
#include "DbgHelp.h"

XMLCompiler::XMLCompiler(
	ICryXML* pCryXML,
	const std::vector<XMLFilterElement>& filter,
	const std::vector<string>& tableFilemasks,
	const NameConverter& nameConverter)
	: m_refCount(1)
	, m_pCryXML(pCryXML)
	, m_pFilter(&filter)
	, m_pTableFilemasks(&tableFilemasks)
	, m_pNameConverter(&nameConverter)
{
}

void XMLCompiler::Release()
{
	if (--m_refCount <= 0)
	{
		delete this;
	}
}

XMLConverter::XMLConverter(ICryXML* pCryXML)
	: m_pCryXML(pCryXML)
	, m_refCount(1)
{
	m_pCryXML->AddRef();
}

XMLConverter::~XMLConverter()
{
	m_pCryXML->Release();
}

void XMLConverter::Release()
{
	if (--m_refCount <= 0)
	{
		delete this;
	}
}

void XMLConverter::Init(const ConverterInitContext& context)
{
	m_filter.clear();
	m_tableFilemasks.clear();

	if (!m_nameConverter.SetRules(context.config->GetAsString("targetnameformat", "", "")))
	{
		return;
	}

	const string xmlFilterFile = context.config->GetAsString("xmlFilterFile", "", "");
	if (xmlFilterFile.empty())
	{
		return;
	}

	FILE* const f = fopen(xmlFilterFile.c_str(), "rt");
	if (!f)
	{
		RCLogError("XML: Failed to open XML filter file \"%s\"", xmlFilterFile.c_str());
		return;
	}

	char line[1024];
	while (fgets(line, sizeof(line), f) != NULL)
	{
		if (line[0] == 0)
		{
			continue;
		}

		string sLine = line;
		sLine.Trim();
		if (sLine.empty())
		{
			continue;
		}

		if (::tolower(sLine[0]) == 'f')
		{
			sLine = sLine.substr(1, sLine.npos);
			sLine.TrimLeft();
			const string sKeywordTable = "table";
			if (StringHelpers::StartsWithIgnoreCase(sLine, sKeywordTable))
			{
				sLine.erase(0, sKeywordTable.length());
				sLine.TrimLeft();
				if (!sLine.empty())
				{
					m_tableFilemasks.push_back(PathUtil::ToDosPath(sLine));
				}
			}
			continue;
		}

		if ((::tolower(sLine[0]) == 'a' || ::tolower(sLine[0]) == 'e'))
		{
			XMLFilterElement fe;
			fe.type = (::tolower(sLine[0]) == 'a') 
				? XMLBinary::IFilter::eType_AttributeName 
				: XMLBinary::IFilter::eType_ElementName;
			sLine = sLine.substr(1, sLine.npos);
			sLine.TrimLeft();
			if (!sLine.empty() && (sLine[0] == '+' || sLine[0] == '-'))
			{
				fe.bAccept = (sLine[0] == '+');
				sLine = sLine.substr(1, sLine.npos);
				sLine.TrimLeft();
				if (!sLine.empty())
				{
					fe.wildcards = sLine;
					m_filter.push_back(fe);
				}
			}
		}
	}

	fclose(f);
}

//////////////////////////////////////////////////////////////////////////
class CXmlBinaryDataWriterFile
	: public XMLBinary::IDataWriter
{
public:
	explicit CXmlBinaryDataWriterFile(const char* file)
	{
		m_file = fopen(file, "wb");
	}
	~CXmlBinaryDataWriterFile()
	{
		if (m_file)
		{
			fclose(m_file);
		}
	}
	virtual bool IsOk()
	{
		return m_file != 0;
	}
	virtual void Write(const void* pData, size_t size)
	{
		if (m_file)
		{
			fwrite(pData, size, 1, m_file);
		}
	}
private:
	FILE* m_file;
};

//////////////////////////////////////////////////////////////////////////
class CXmlFilter
	: public XMLBinary::IFilter
{
public:
	explicit CXmlFilter(const std::vector<XMLFilterElement>* pFilterArray)
		: m_pFilterArray((pFilterArray && !pFilterArray->empty()) ? pFilterArray : 0)
	{
	}

	virtual bool IsAccepted(EType type, const char* pName) const
	{
		if (!m_pFilterArray)
		{
			return true;
		}

		const string name(pName);
		for (size_t i = 0; i < m_pFilterArray->size(); ++i)
		{
			if (((*m_pFilterArray)[i].type == type) &&
				StringHelpers::MatchesWildcardsIgnoreCase(name, (*m_pFilterArray)[i].wildcards))
			{
				return (*m_pFilterArray)[i].bAccept;
			}
		}

		return true;
	}

private:
	const std::vector<XMLFilterElement>* m_pFilterArray;
};

//////////////////////////////////////////////////////////////////////////
static bool xmlsAreEqual(XmlNodeRef node0, XmlNodeRef node1, XMLBinary::IFilter* pFilter,  const char*& mismatchInfo)
{
	mismatchInfo = 0;

	{
		const char* const tag0 = node0->getTag();
		const char* const tag1 = node1->getTag();

		if (strcmp(tag0, tag1) != 0)
		{
			mismatchInfo = "tags";
			return false;
		}
	}

	{
		const char* const content0 = node0->getContent();
		const char* const content1 = node1->getContent();

		if (strcmp(content0, content1) != 0)
		{
			mismatchInfo = "content";
			return false;
		}
	}

	{
		const int count0 = node0->getNumAttributes();
		const int count1 = node1->getNumAttributes();
		int index1 = 0;
		int filteredCount1 = 0;

		for (int index0 = 0; index0 < count0; ++index0)
		{
			const char* key0;
			const char* value0;
			node0->getAttributeByIndex(index0, &key0, &value0);

			const char* key1;
			const char* value1;
			for (;;)
			{
				if (index1 >= count1)
				{
					mismatchInfo = "attribute count";
					return false;
				}
				node1->getAttributeByIndex(index1++, &key1, &value1);
				if (!pFilter || pFilter->IsAccepted(XMLBinary::IFilter::eType_AttributeName, key1))
				{
					++filteredCount1;
					break;
				}
			}

			if (strcmp(key0, key1) != 0)
			{
				mismatchInfo = "attribute name";
				return false;
			}

			if (strcmp(value0, value1) != 0)
			{
				mismatchInfo = "attribute value";
				return false;
			}
		}

		if (count0 != filteredCount1)
		{
			mismatchInfo = "attribute count";
			return false;
		}
	}

	{
		const int count0 = node0->getChildCount();
		const int count1 = node1->getChildCount();
		int index1 = 0;
		int filteredCount1 = 0;

		for (int index0 = 0; index0 < count0; ++index0)
		{
			XmlNodeRef child0 = node0->getChild(index0);

			XmlNodeRef child1;
			for (;;)
			{
				if (index1 >= count1)
				{
					mismatchInfo = "child count";
					return false;
				}
				child1 = node1->getChild(index1++);
				if (!pFilter || pFilter->IsAccepted(XMLBinary::IFilter::eType_ElementName, child1->getTag()))
				{
					++filteredCount1;
					break;
				}
			}

			const bool bEqual = xmlsAreEqual(child0, child1, pFilter, mismatchInfo);
			if (!bEqual)
			{
				return false;
			}
		}

		if (count0 != filteredCount1)
		{
			mismatchInfo = "child count";
			return false;
		}
	}

	return true;
}

static string GetNormalizedFullPath(const string& relativePath)
{
	char fullname[MAX_PATH];
	const DWORD len = GetFullPathNameA(relativePath.c_str(), sizeof(fullname), fullname, 0);
	if ((len == 0) || (len >= sizeof(fullname)))
	{
		RCLogError("XML: Failed to obtain full path for \"%s\"", relativePath.c_str());
		return string();
	}
	// note: maybe normalization via ToDosPath() is unneeded, but MSDN is not very clear about it.
	return PathUtil::ToDosPath(string(fullname));
}

static XmlNodeRef ConvertFromExcelXmlToCryEngineTableXml(XmlNodeRef root, IXMLSerializer* pSerializer, const string& sInputFile)
{
	XmlNodeRef nodeTable;

	{
		const char* err = 0;
		XmlNodeRef nodeWorksheet = root->findChild("Worksheet");
		if (!nodeWorksheet)
		{
			err = "element 'Worksheet' is missing";
		}
		else
		{
			nodeTable = nodeWorksheet->findChild("Table");
			if (!nodeTable)
			{
				err = "element 'Table' is missing in 'Worksheet'";
			}
		}

		if (err)
		{
			RCLogError(
				"XML: File \"%s\" expected to be an Excel spreadsheet XML, but it's not: %s. Check your /xmlfilterfile's file.",
				sInputFile.c_str(), err);
			return XmlNodeRef(0);
		}
	}

	XmlNodeRef outTable = pSerializer->CreateNode("Table");

	const bool bKeepEmptyRows = false;

	const string sNewLine("\n");
	string sRow;

	int rowIndex = -1;
	for (int row = 0; row < nodeTable->getChildCount(); ++row)
	{
		XmlNodeRef nodeRow = nodeTable->getChild(row);
		if (!nodeRow->isTag("Row"))
		{
			continue;
		}

		++rowIndex;

		// Handling skipped rows
		{
			int index = 0;
			if (nodeRow->getAttr("ss:Index", index))
			{
				--index; // one-based -> zero-based
				if (index < rowIndex)
				{
					RCLogError("XML: \"%s\": ss:Index has unexpected value %d", sInputFile.c_str(), index + 1);
					return XmlNodeRef(0);
				}
				if (bKeepEmptyRows)
				{
					while (rowIndex < index)
					{
						XmlNodeRef outRow = pSerializer->CreateNode("Row");
						outTable->addChild(outRow);
						++rowIndex;
					}
				}
				else
				{
					rowIndex = index;
				}
			}
		}

		sRow.clear();

		int cellIndex = -1;
		for (int cell = 0; cell < nodeRow->getChildCount(); ++cell)
		{
			XmlNodeRef nodeCell = nodeRow->getChild(cell);
			if (!nodeCell->isTag("Cell"))
			{
				continue;
			}

			++cellIndex;

			// Handling skipped columns
			{
				int index = 0;
				if (nodeCell->getAttr("ss:Index", index))
				{
					--index; // one-based -> zero-based
					if (index < cellIndex)
					{
						RCLogError("XML: \"%s\": ss:Index has unexpected value %d", sInputFile.c_str(), index + 1);
						return XmlNodeRef(0);
					}
					while (cellIndex < index)
					{
						sRow.append(sNewLine);
						++cellIndex;
					}
				}
			}

			XmlNodeRef nodeCellData = nodeCell->findChild("Data");
			if (nodeCellData)
			{
				const char* const pContent = nodeCellData->getContent();
				if (pContent)
				{
					sRow.append(pContent);
				}
			}
			sRow.append(sNewLine);
		}

		// Erase empty trailing cells in the row
		while (StringHelpers::EndsWith(sRow, sNewLine))
		{
			sRow.erase(sRow.length() - sNewLine.length());
		}

		if (!sRow.empty() || bKeepEmptyRows)
		{
			XmlNodeRef outRow = pSerializer->CreateNode("Row");
			outRow->setContent(sRow.c_str());
			outTable->addChild(outRow);
		}
	}

	XmlNodeRef outRoot = pSerializer->CreateNode("Tables");
	outRoot->addChild(outTable);

	if (false)
	{
		const string sFilename = sInputFile + ".debug.xml";
		const bool ok = pSerializer->Write(outRoot, sFilename.c_str());
		if (!ok)
		{
			RCLogError("XML: Failed to write XML file \"%s\".", sFilename.c_str());
			return 	XmlNodeRef(0);
		}
	}

	return outRoot;
}

//////////////////////////////////////////////////////////////////////////
bool XMLCompiler::Process()
{
	const int verbosityLevel = m_CC.pRC->GetVerbosityLevel();

	const bool bNeedSwapEndian = m_CC.pRC->GetPlatformInfo(m_CC.platform)->bBigEndian;
	if (verbosityLevel >= 1 && bNeedSwapEndian)
	{
		RCLog("XML: Endian conversion specified");
	}

	// Get the files to process.
	const string sInputFile = m_CC.GetSourcePath();
	string sOutputFile = GetOutputPath();
	if (m_pNameConverter->HasRules()) 
	{
		const string oldFilename = PathUtil::GetFile(sOutputFile);
		const string newFilename = m_pNameConverter->GetConvertedName(oldFilename);
		if (newFilename.empty())
		{
			return false;
		}
		if (!StringHelpers::EqualsIgnoreCase(oldFilename, newFilename))
		{
			if (verbosityLevel >= 2)
			{
				RCLog("Target file name changed: %s -> %s", oldFilename.c_str(), newFilename.c_str());
			}
			sOutputFile = PathUtil::Make(PathUtil::GetPathWithoutFilename(sOutputFile), newFilename);
		}
	}

	// Check that we will not overwrite source file 
	{
		const string fname0 = GetNormalizedFullPath(sInputFile);
		const string fname1 = GetNormalizedFullPath(sOutputFile);

		if (fname0.empty() || fname1.empty())
		{
			return false;
		}

		if (StringHelpers::EqualsIgnoreCase(fname0, fname1))
		{
			RCLogError("XML: Source file cannot be same as target file. Use /targetroot=... option.");
			return false;
		}
	}

	if (!m_CC.bForceRecompiling && UpToDateFileHelpers::FileExistsAndUpToDate(sOutputFile, sInputFile))
	{
		// The file is up-to-date
		m_CC.pRC->AddInputOutputFilePair(sInputFile, sOutputFile);
		return true;
	}

	// Check that the input file exists.
	if (!FileUtil::FileExists(sInputFile.c_str()))
	{
		RCLogError("XML: File \"%s\" does not exist", sInputFile.c_str());
		return false;
	}

	// Get XML serializer.
	IXMLSerializer* pSerializer = m_pCryXML->GetXMLSerializer();

	// Ensure that the input file is not in binary XML format.
	{
		XMLBinary::XMLBinaryReader binReader;
		XMLBinary::XMLBinaryReader::EResult binResult;
		XmlNodeRef rootBin = binReader.LoadFromFile(sInputFile.c_str(), binResult);
		if (binResult == XMLBinary::XMLBinaryReader::eResult_Success)
		{
			RCLogError("XML: Source file is binary XML \"%s\"", sInputFile.c_str());
			return false;
		}
		if (binResult == XMLBinary::XMLBinaryReader::eResult_Error)
		{
			RCLogError("XML: Input XML file is either binary or damaged \"%s\": %s", sInputFile.c_str(), binReader.GetErrorDescription());
			return false;
		}
	}

	// Read input XML file.
	XmlNodeRef root;
	{
		const bool bRemoveNonessentialSpacesFromContent = true;
		char szErrorBuffer[1024];
		szErrorBuffer[0] = 0;
		root = pSerializer->Read(FileXmlBufferSource(sInputFile.c_str()), bRemoveNonessentialSpacesFromContent, sizeof(szErrorBuffer), szErrorBuffer);
		if (!root)
		{
			const char* const pErrorStr = 
				(szErrorBuffer[0])
				? &szErrorBuffer[0] 
				: "Probably this file has bad XML syntax or it's not XML file at all";
			RCLogError("XML: Cannot read file \"%s\": %s", sInputFile.c_str(), pErrorStr);
			return false;
		}
	}

	// Convert Excel's XML format to CryEngine's table XML format, if requested.
	{
		bool bConvert = false;
		const string filename = PathUtil::ToDosPath(sInputFile);
		for (size_t i = 0; i < m_pTableFilemasks->size(); ++i)
		{
			if (StringHelpers::MatchesWildcardsIgnoreCase(filename, (*m_pTableFilemasks)[i]))
			{
				bConvert = true;
				break;
			}
		}

		if (bConvert)
		{
			XmlNodeRef rootConverted = ConvertFromExcelXmlToCryEngineTableXml(root, pSerializer, sInputFile);
			if (!rootConverted)
			{
				return false;
			}
			root = rootConverted;
		}
	}

	// Create filter to get rid of unneeded elements and attributes
	CXmlFilter filter(m_pFilter);

	// Write out the destination file.
	{
		SetFileAttributes(sOutputFile.c_str(), FILE_ATTRIBUTE_ARCHIVE);

		FILE* const pDestinationFile = fopen(sOutputFile.c_str(), "wb");
		if (pDestinationFile == 0)
		{
			RCLogError("XML: Cannot write file \"%s\": %s", sInputFile.c_str(), strerror(errno));
			return false;
		}
		fclose(pDestinationFile);

		CXmlBinaryDataWriterFile outputFile(sOutputFile.c_str());
		XMLBinary::CXMLBinaryWriter xmlBinaryWriter;
		string error;
		const bool ok = xmlBinaryWriter.WriteNode(&outputFile, root, bNeedSwapEndian, &filter, error);
		if (!ok)
		{
			remove(sOutputFile.c_str());
			RCLogError("XML: Failed to write binary XML file \"%s\": %s", sOutputFile.c_str(), error.c_str());
			return false;
		}
	}

	// Verify that the output file was written
	if (!FileUtil::FileExists(sOutputFile.c_str()))
	{
		RCLogError("XML: Failed to write file \"%s\"", sOutputFile.c_str());
		return false;
	}

	// Check that the output binary XML file has same content as the input XML
	if (!bNeedSwapEndian) 
	{
		XMLBinary::XMLBinaryReader binReader;
		XMLBinary::XMLBinaryReader::EResult binResult;
		XmlNodeRef rootBin = binReader.LoadFromFile(sOutputFile.c_str(), binResult);
		if (!rootBin)
		{
			RCLogError("XML: Cannot read binary XML file \"%s\". Contact RC programmers.", sOutputFile.c_str());
			return false;
		}

		const char* mismatchInfo = 0;
		const bool bEqual = xmlsAreEqual(rootBin, root, &filter, mismatchInfo);
		if (!bEqual)
		{
			RCLogError(
				"XML: Source XML file \"%s\" and result binary XML file \"%s\" are different: %s. Contact RC programmers.",
				sInputFile.c_str(), sOutputFile.c_str(), mismatchInfo);
			return false;
		}
	}

	if (!UpToDateFileHelpers::SetMatchingFileTime(sOutputFile, sInputFile))
	{
		return false;
	}
	m_CC.pRC->AddInputOutputFilePair(sInputFile, sOutputFile);

	return true;
}

string XMLCompiler::GetOutputFileNameOnly() const
{
	return m_CC.config->GetAsString("overwritefilename", m_CC.sourceFileNameOnly.c_str(), m_CC.sourceFileNameOnly.c_str());
}

string XMLCompiler::GetOutputPath() const
{
	return PathUtil::Make(m_CC.GetOutputFolder(), GetOutputFileNameOnly());
}

const char* XMLConverter::GetExt(int index) const
{
	return (index == 0) ? "xml" : 0;
}

ICompiler* XMLConverter::CreateCompiler()
{
	return new XMLCompiler(m_pCryXML, m_filter, m_tableFilemasks, m_nameConverter);
}

bool XMLConverter::SupportsMultithreading() const
{
	return true;
}
