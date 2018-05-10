// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __TAG_DEFINITION_XML__H__
#define __TAG_DEFINITION_XML__H__

class CTagDefinition;

//////////////////////////////////////////////////////////////////////////
// Currently this is not part of the save interface, needs some more thought!
// This is mainly here so that saving keeps the loaded import structure.
struct STagDefinitionImportsInfo
{
	void SetFilename(const char* const filename)
	{
		assert(filename);
		assert(filename[0]);

		m_filename = filename;
	};

	const char* GetFilename() const
	{
		return m_filename;
	}

	void Clear()
	{
		m_filter.clear();
		m_imports.clear();
	}

	void AddTag(const TagID tagId)
	{
		if (tagId != TAG_ID_INVALID)
		{
			m_filter.push_back(tagId);
		}
	}

	STagDefinitionImportsInfo& AddImport(const char* const filename)
	{
		assert(filename);
		assert(filename[0]);

		m_imports.push_back(STagDefinitionImportsInfo());
		STagDefinitionImportsInfo& importsInfo = m_imports.back();
		importsInfo.SetFilename(filename);
		return importsInfo;
	}

	const STagDefinitionImportsInfo& GetImport(const size_t index) const
	{
		return m_imports[index];
	}

	size_t GetImportCount() const
	{
		return m_imports.size();
	}

	void FlattenImportsInfo(std::vector<const STagDefinitionImportsInfo*>& importsInfoListOut) const
	{
		importsInfoListOut.push_back(this);

		const size_t importCount = m_imports.size();
		for (size_t i = 0; i < importCount; ++i)
		{
			const STagDefinitionImportsInfo& currentImport = m_imports[i];
			currentImport.FlattenImportsInfo(importsInfoListOut);
		}
	}

	const STagDefinitionImportsInfo& Find(const TagID tagId) const
	{
		const STagDefinitionImportsInfo* pFoundImport = FindRec(tagId);
		if (pFoundImport)
		{
			return *pFoundImport;
		}

		return *this;
	}

	bool MapTags(const CTagDefinition& originalTagDef, CTagDefinition& modifiedTagDef)
	{
		for (int32 filterIndex = m_filter.size() - 1; filterIndex >= 0; --filterIndex)
		{
			uint32 mappedTagID = (modifiedTagDef.Find(originalTagDef.GetTagCRC(m_filter[filterIndex])));
			if (mappedTagID != TAG_ID_INVALID)
			{
				if (m_filter[filterIndex] != mappedTagID)
				{
					//CryLog("[TAGDEF]: MapTags() (%s): mapping tag %i [%s] to %i [%s]", m_filename, m_filter[filterIndex], originalTagDef.GetTagName(m_filter[filterIndex]), mappedTagID, modifiedTagDef.GetTagName(mappedTagID));
				}
				m_filter[filterIndex] = mappedTagID;
			}
			else
			{
				//CryLog("[TAGDEF]: MapTags() (%s): cannot map tag %i [%s] - will be erased", m_filename, m_filter[filterIndex], originalTagDef.GetTagName(m_filter[filterIndex]));
				m_filter.erase(m_filter.begin() + filterIndex);
			}
		}

		for (int32 importsIndex = m_imports.size() - 1; importsIndex >= 0; --importsIndex)
		{
			m_imports[importsIndex].MapTags(originalTagDef, modifiedTagDef);
		}

		return true;
	}

private:
	bool IsInFilter(const TagID tagId) const
	{
		const size_t filterCount = m_filter.size();

		for (size_t i = 0; i < filterCount; ++i)
		{
			const TagID filterTagId = m_filter[i];
			const bool found = (filterTagId == tagId);
			if (found)
			{
				return true;
			}
		}
		return false;
	}

	const STagDefinitionImportsInfo* FindRec(const TagID tagId) const
	{
		const bool isInFilter = IsInFilter(tagId);
		if (isInFilter)
		{
			return this;
		}

		const size_t importCount = m_imports.size();
		for (size_t i = 0; i < importCount; ++i)
		{
			const STagDefinitionImportsInfo& currentImport = m_imports[i];
			const STagDefinitionImportsInfo* pFoundImport = currentImport.FindRec(tagId);
			if (pFoundImport)
			{
				return pFoundImport;
			}
		}

		return NULL;
	}

private:
	string m_filename;

	// A tag guid would be better... as this handles renames but not deletes on the tag definition (although we don't have that operation right now)
	std::vector<TagID>                     m_filter;

	std::vector<STagDefinitionImportsInfo> m_imports;
};

namespace mannequin
{
bool                       LoadTagDefinition(const char* const filename, CTagDefinition& tagDefinitionOut, bool isTags);

XmlNodeRef                 SaveTagDefinition(const CTagDefinition& tagDefinition);

STagDefinitionImportsInfo& GetDefaultImportsInfo(const char* const filename);

struct STagDefinitionSaveData
{
	string     filename;
	XmlNodeRef pXmlNode;
};
typedef std::vector<STagDefinitionSaveData> TTagDefinitionSaveDataList;
void SaveTagDefinition(const CTagDefinition& tagDefinition, TTagDefinitionSaveDataList& saveDataListOut);

void OnDatabaseManagerUnload();
}

#endif
