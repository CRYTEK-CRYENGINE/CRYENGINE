// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SensorTagLibrary.h"

namespace Cry
{
	namespace SensorSystem
	{
		CSensorTagLibrary::CSensorTagLibrary()
		{
			for (uint32 bit = 0; bit < 32; ++bit)
			{
				m_freeTags.emplace_back(static_cast<ESensorTags>(1 << bit));
			}
		}

		SensorTags CSensorTagLibrary::CreateTag(const char* szTagName)
		{
			CRY_ASSERT(szTagName && (szTagName[0] != '\0'));
			if (szTagName && (szTagName[0] != '\0'))
			{
				CRY_ASSERT(!m_freeTags.empty());
				if (!m_freeTags.empty())
				{
					const SensorTags value = m_freeTags.back();
					m_tags.insert(Tags::value_type(szTagName, value));
					m_freeTags.pop_back();
					m_tagNames.clear();
					return value;
				}
			}
			return SensorTags();
		}

		SensorTags CSensorTagLibrary::GetTag(const char* szTagName) const
		{
			Tags::const_iterator itTag = m_tags.find(szTagName);
			return itTag != m_tags.end() ? itTag->second : SensorTags();
		}

		void CSensorTagLibrary::GetTagNames(DynArray<const char*>& tagNames, SensorTags tags) const
		{
			for (uint32 bit = 0; bit < 32; ++bit)
			{
				ESensorTags value = static_cast<ESensorTags>(1 << bit);
				if (tags.Check(value))
				{
					const char* szTagName = GetTagName(value);
					if (szTagName)
					{
						tagNames.push_back(szTagName);
					}
				}
			}
		}

		bool CSensorTagLibrary::SerializeTagName(Serialization::IArchive& archive, string& value, const char* szName, const char* szLabel)
		{
			PopulateTagNames();

			bool bResult = false;
			if (archive.isInput())
			{
				Serialization::StringListValue temp(m_tagNames, 0);
				bResult = archive(temp, szName, szLabel);
				value = temp.c_str();
			}
			else if (archive.isOutput())
			{
				const int pos = m_tagNames.find(value.c_str());
				bResult = archive(Serialization::StringListValue(m_tagNames, pos), szName, szLabel);
			}
			return bResult;
		}

		void CSensorTagLibrary::ReleaseTag(const char* szTagName)
		{
			Tags::const_iterator itTag = m_tags.find(szTagName);
			if (itTag != m_tags.end())
			{
				m_freeTags.push_back(itTag->second);
				m_tags.erase(itTag);
				m_tagNames.clear();
			}
		}

		const char* CSensorTagLibrary::GetTagName(SensorTags value) const
		{
			for (const Tags::value_type& tag : m_tags)
			{
				if (tag.second == value)
				{
					return tag.first.c_str();
				}
			}
			return nullptr;
		}

		void CSensorTagLibrary::PopulateTagNames()
		{
			if (m_tagNames.empty())
			{
				m_tagNames.reserve(m_tags.size());
				for (const Tags::value_type& tag : m_tags)
				{
					m_tagNames.push_back(tag.first.c_str());
				}
			}
		}
	}
}