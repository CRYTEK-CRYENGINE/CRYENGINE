// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <memory>

namespace RCConfigParser
{


	template <class TS>
	static inline void Split_Tpl(const TS& str, const TS& separator, bool bReturnEmptyPartsToo, std::vector<TS>& outParts)
	{
		if (str.empty())
		{
			return;
		}

		if (separator.empty())
		{
			for (size_t i = 0; i < str.length(); ++i)
			{
				outParts.push_back(str.substr(i, 1));
			}
			return;
		}

		size_t partStart = 0;

		for (;;)
		{
			const size_t pos = str.find(separator, partStart);
			if (pos == TS::npos)
			{
				break;
			}
			if (bReturnEmptyPartsToo || (pos > partStart))
			{
				outParts.push_back(str.substr(partStart, pos - partStart));
			}
			partStart = pos + separator.length();
		}

		if (bReturnEmptyPartsToo || (partStart < str.length()))
		{
			outParts.push_back(str.substr(partStart, str.length() - partStart));
		}
	}

	void Split(const string& str, const string& separator, bool bReturnEmptyPartsToo, std::vector<string>& outParts)
	{
		Split_Tpl(str, separator, bReturnEmptyPartsToo, outParts);
	}

	void Split(const wstring& str, const wstring& separator, bool bReturnEmptyPartsToo, std::vector<wstring>& outParts)
	{
		Split_Tpl(str, separator, bReturnEmptyPartsToo, outParts);
	}



	//////////////////////////////////////////////////////////////////////////
	// Classes
	//////////////////////////////////////////////////////////////////////////

	struct ConfigEntry
	{
		ConfigEntry(string& name, string& value)
			: name(name), value(value)
		{}

		virtual bool IsEmpty() {
			return value.empty();
		}
		const string& GetName() { return name; }
		const string GetAsString() { return value; }
		const bool GetAsBool() { return atoi(value.c_str()) == 1; }
		const std::vector<string> GetAsStringVector() {
			std::vector<string> result;
			if (!IsEmpty())
			{
				Split(value, ",", false, result);
			}
			return result;
		}
	private:
		string name;
		string value;
	};

	typedef std::map<string, std::shared_ptr<ConfigEntry>> ConfigEntries;

	struct ConfigSection
	{
	public:
		const ConfigEntries& GetEntries() const { return m_entries; }
		void AddEntry(ConfigEntry* entry)
		{
			m_entries[entry->GetName()] = std::shared_ptr<ConfigEntry>(entry);
		}

		string name;
	private:
		ConfigEntries m_entries;
	};

	typedef std::map<string, ConfigSection> ConfigSections;

	class ParsedConfig
	{
	public:
		ConfigSection& GetSection(const string& sectionName)
		{
			ConfigSections::iterator it = m_sections.find(sectionName);
			if (it == m_sections.end())
			{
				ConfigSection section;
				section.name = sectionName;
				m_sections[sectionName] = section;
			}
			return m_sections[sectionName];
		}
		const ConfigSections& GetSections() const { return m_sections; };
	private:
		ConfigSections m_sections;
	};

	bool IsConfigStringComment(const string& value)
	{
		const char* pBegin = value.c_str();
		while (pBegin[0] == ' ' || pBegin[0] == '\t')
		{
			++pBegin;
		}

		// "//" comment
		if (pBegin[0] == '/' && pBegin[1] == '/')
		{
			return true;
		}

		// ";" comment
		if (pBegin[0] == ';')
		{
			return true;
		}

		// empty line (treat it as comment)
		if (pBegin[0] == 0)
		{
			return true;
		}

		return false;
	}

	void LoadConfigFromBuffer(const char* const buf, const size_t bufSize, ParsedConfig& config)
	{
		// Read entries from config string buffer.
		ConfigSection* currentSection = &config.GetSection("");

		string entrystr;
		entrystr.reserve(80);

		const auto isEof = [&](const int idx)
		{
			return idx >= bufSize || buf[idx] == 0x1a;
		};

		for (size_t i = 0; !isEof(i); )
		{
			entrystr.clear();

			for (;;)
			{
				if (buf[i] == '\n')
				{
					++i;
					break;
				}
				if (buf[i] != '\r')
				{
					entrystr.push_back(buf[i]);
				}
				++i;
				if (isEof(i))
				{
					break;
				}
			}

			const bool bComment = IsConfigStringComment(entrystr);

			// Analyze entry string, split on key and value and store in lists.
			if (!bComment)
				entrystr.Trim();

			if (bComment || entrystr.empty())
				continue;

			int splitter = int(entrystr.find('='));

			if (splitter > 0)
			{
				// Key found.
				string key = entrystr.Mid(0, splitter);	// Before spliter is key name.
				string value = entrystr.Mid(splitter + 1);	// Everything after splittes is value string.
				key.Trim();
				value.Trim();

				if (value.empty())
					continue;			
				currentSection->AddEntry(new ConfigEntry(key, value));
			}
			else
			{
				// If not key then probably section string.
				if (entrystr[0] == '[' && entrystr[entrystr.size() - 1] == ']')
				{
					currentSection = &config.GetSection(entrystr.Mid(1, entrystr.size() - 2));
				}
			}
		}
	}

	// Load configuration file.
	bool LoadConfigFile(const char* fileName, ParsedConfig& config)
	{
		FILE *file = fopen(fileName, "rb");
		if (!file)
		{
			return false;
		}

		fseek(file, 0, SEEK_END);
		int size = ftell(file);
		fseek(file, 0, SEEK_SET);

		// Read whole file to memory.
		char *s = (char*)malloc(size + 1);
		memset(s, 0, size + 1);
		fread(s, 1, size, file);
		LoadConfigFromBuffer(s, size, config);
		free(s);

		fclose(file);

		return true;
	}
}
