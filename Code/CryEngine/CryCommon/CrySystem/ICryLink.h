// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/StringUtils.h>

namespace CryLinkService
{
	namespace UriEncoding
	{
		inline string Encode(const char* str, size_t length);
		inline string Decode(const char* str, size_t length);
	}

	enum class ELinkType
	{
		Commands = 0,		// 'CryLink://<processName>/exec?<commands>'

		Count
	};

	//
	// CCryLink
	//
	// Format: CryLink://<processName>/<additionalInfo>/.../<linkType>?<query1>&<query2>& ...
	//
	//	For instance 'CryLink://game_1/exec?cmd1=mission+tutorial&cmd2=missionSetQuest+5'
	//
	// Syntax: 
	//	As defined by RFC1808, RFC2396, RFC2732
	//
	//	Pseudo code
	//	--------------------------------------
	//	result = ""
	// 
	//	if scheme is defined then
	// 		append scheme to result
	// 		append ":" to result
	// 
	//	if authority is defined then
	// 		append "//" to result
	// 		append authority to result
	// 
	//	append path to result
	// 
	//	if query is defined then
	// 		append "?" to result
	// 		append query to result
	// 
	//	if fragment is defined then
	// 		append "#" to result
	// 		append fragment to result
	// 
	//	return result
	//	--------------------------------------
	//

#define CRYLINK_SCHEME_NAME "crylink"
#define CRYLINK_SCHEME_BEGIN CRYLINK_SCHEME_NAME "://"
#define CRYLINK_MAX_MESSAGE_LENGTH 4096

	class CCryLink
	{
	public:
		typedef std::map<string, string> Queries;
		typedef Queries::value_type Query;

		enum { maxLinkLength = 1024 };
		enum { invalidNetworkPort = 0 };

		CCryLink(const char* szUri = CRYLINK_SCHEME_BEGIN)
			: m_uri(szUri)
			, m_port(invalidNetworkPort)
		{
			const string::size_type schemeEndIdx = m_uri.find("://");
			if (schemeEndIdx != string::npos)
			{
				m_scheme = m_uri.substr(0, schemeEndIdx);
			}
			else
			{
				return;
			}

			const string::size_type authorityBegIdx = schemeEndIdx + sizeof("://") - 1;
			const string::size_type authorityEndIdx = m_uri.find('/', authorityBegIdx);
			if (authorityEndIdx != string::npos)
			{
				const string::size_type authorityLength = authorityEndIdx - authorityBegIdx;
				m_authority = m_uri.substr(authorityBegIdx, authorityLength);
			}
			else
			{
				const string::size_type authorityLength = m_uri.length() - schemeEndIdx;
				m_authority = m_uri.substr(authorityBegIdx, authorityLength);
				return;
			}

			// TODO: Find port!

			const string::size_type pathEndIdx = m_uri.find('?', authorityEndIdx);
			if (pathEndIdx != string::npos)
			{
				const string::size_type pathLength = pathEndIdx - authorityEndIdx - 1;
				m_path = m_uri.substr(authorityEndIdx + 1, pathLength);

				const string::size_type queryLength = m_uri.length() - pathEndIdx;
				string querry = m_uri.substr(pathEndIdx + 1, queryLength);

				string::size_type lastQueryIdx = 0;
				string::size_type nextQueryIdx = pathEndIdx + 1;

				do
				{
					lastQueryIdx = nextQueryIdx;
					nextQueryIdx = querry.find_first_of('&', nextQueryIdx);

					const string::size_type equalIdx = m_uri.find('=', lastQueryIdx);
					if (equalIdx != string::npos)
					{
						string::size_type valueEndIdx;
						if (nextQueryIdx != string::npos)
						{
							valueEndIdx = nextQueryIdx - lastQueryIdx;
						}
						else
						{
							valueEndIdx = m_uri.length();
						}

						const string::size_type keyLength = equalIdx - lastQueryIdx;
						const string::size_type valLength = valueEndIdx - equalIdx;

						stack_string encodedKey = m_uri.substr(lastQueryIdx, keyLength);
						stack_string encodedVal = m_uri.substr(equalIdx + 1, valLength);

						string key = UriEncoding::Decode(encodedKey.c_str(), encodedKey.length());
						string val = UriEncoding::Decode(encodedVal.c_str(), encodedVal.length());

						if (!key.empty() && !val.empty())
						{
							m_queries.insert(std::make_pair(key, val));
						}
						else
						{
							continue;
						}
					}
				} while (nextQueryIdx != string::npos);
			}
			else
			{
				m_path = m_uri.substr(authorityEndIdx + 1, m_uri.length() - pathEndIdx - 1);
				return;
			}
		}

		const char* GetUri() const
		{
			return m_uri.c_str();
		}


		const char* GetScheme() const
		{
			return m_scheme.c_str();
		}

		const char* GetAuthority() const
		{
			return m_authority.c_str();
		}

		uint32 GetPort() const
		{
			return m_port;
		}

		const Queries& GetQueries()
		{
			return m_queries;
		}

		const char* GetQuery(const char* szKey) const
		{
			Queries::const_iterator it = m_queries.find(string(szKey));
			if (it != m_queries.end())
			{
				return it->second.c_str();
			}

			return nullptr;
		}

	private:
		string m_scheme;
		string m_authority;
		string m_path;
		string m_uri;
		Queries m_queries;
		uint32 m_port;
	};

	class CCryLinkUriFactory
	{
	public:
		CCryLinkUriFactory(const char* szProcessName, ELinkType linkType = ELinkType::Commands)
			: m_szProcessName(szProcessName)
			, m_linkType(linkType)
			, m_commandStringsLength(0)
		{}

		void AddCommand(const char* szCommand, size_t length)
		{
			string encodedCmd = UriEncoding::Encode(szCommand, length);
			stack_string cmd;
			cmd.Format("cmd%d=%s", m_commands.size() + 1, encodedCmd.c_str());

			m_commandStringsLength += cmd.length();
			m_commands.push_back(cmd.c_str());
		}

		static string GetUri(const char* szProcessName, std::vector<string> commands)
		{
			CCryLinkUriFactory uriFactory(szProcessName);
			for (const string& command : commands)
				uriFactory.AddCommand(command, command.length());
			return uriFactory.GetUri();
		}

		static string GetUri(const char* szProcessName, const string& command)
		{
			CCryLinkUriFactory uriFactory(szProcessName);
			uriFactory.AddCommand(command, command.length());
			return uriFactory.GetUri();
		}

		static string GetUriV(const char* szProcessName, const char* szCommand, ...)
		{
			va_list args;
			va_start(args, szCommand);
			char szBuffer[CRYLINK_MAX_MESSAGE_LENGTH];
			cry_vsprintf(szBuffer, szCommand, args);
			va_end(args);

			return CCryLinkUriFactory::GetUri(szProcessName, szBuffer);
		}

		string GetUri() const
		{
			string uri;

			uri.Format("%s://%s/%s?", CRYLINK_SCHEME_NAME, m_szProcessName, GetType());
			uri.reserve(uri.length() + m_commandStringsLength);

			for (const string& cmd : m_commands)
			{
				uri.append(cmd);
			}

			return uri;
		}

	private:
		const char* GetType() const
		{
			static const char* linkTypes[] =
			{
				"exec"
			};

			static_assert(static_cast<size_t>(ELinkType::Count) == (sizeof(linkTypes) / sizeof(const char*)), "linkTypes size mismatch!");

			return linkTypes[static_cast<uint32>(m_linkType)];
		}

		std::vector<string> m_commands;
		const char* m_szProcessName;
		uint32 m_commandStringsLength;
		ELinkType m_linkType;
	};

	namespace UriEncoding
	{
		static const char* s_szAsHex[] =
		{
			"%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
			"%08", "%09", "%0a", "%0b", "%0c", "%0d", "%0e", "%0f",
			"%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
			"%18", "%19", "%1a", "%1b", "%1c", "%1d", "%1e", "%1f",
			"%20", "%21", "%22", "%23", "%24", "%25", "%26", "%27",
			"%28", "%29", "%2a", "%2b", "%2c", "%2d", "%2e", "%2f",
			"%30", "%31", "%32", "%33", "%34", "%35", "%36", "%37",
			"%38", "%39", "%3a", "%3b", "%3c", "%3d", "%3e", "%3f",
			"%40", "%41", "%42", "%43", "%44", "%45", "%46", "%47",
			"%48", "%49", "%4a", "%4b", "%4c", "%4d", "%4e", "%4f",
			"%50", "%51", "%52", "%53", "%54", "%55", "%56", "%57",
			"%58", "%59", "%5a", "%5b", "%5c", "%5d", "%5e", "%5f",
			"%60", "%61", "%62", "%63", "%64", "%65", "%66", "%67",
			"%68", "%69", "%6a", "%6b", "%6c", "%6d", "%6e", "%6f",
			"%70", "%71", "%72", "%73", "%74", "%75", "%76", "%77",
			"%78", "%79", "%7a", "%7b", "%7c", "%7d", "%7e", "%7f"
		};

		string Encode(const char* szBuffer, size_t length)
		{
			stack_string encodedQuery;

			for (size_t i = 0; i < length; ++i)
			{
				char ch = szBuffer[i];

				CRY_ASSERT(ch <= 0x7f);

				if (('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z') || '0' <= ch && ch <= '9') // a-z, A-Z, 0-9
				{
					encodedQuery.append(&ch, 1);
				}
				else if (ch == ' ') // Space
				{
					encodedQuery.append("+", 1);
				}
				else if (ch == '-' || ch == '_' || ch == '.' || ch == '!' || ch == '~' || ch == '*' || ch == '\'' || ch == '(' || ch == ')') // unreserved
				{
					encodedQuery.append(&ch, 1);
				}
				else if (ch <= 0x7f && ch >= 0)
				{
					encodedQuery.append(s_szAsHex[ch], 3);
				}
				else
				{
					return "";
				}
			}

			return encodedQuery;
		}

		string Decode(const char* szBuffer, size_t length)
		{
			stack_string decodedQuery;

			char ch = -1;
			char result = 0;

			for (size_t i = 0; i < length; ++i)
			{
				switch (ch = szBuffer[i])
				{
				case '%':
					ch = szBuffer[++i];
					result = ((isdigit(ch) ? ch - '0' : 10 + CryStringUtils::toLowerAscii(ch) - 'a') & 0xF) << 4;
					ch = szBuffer[++i];
					result += ((isdigit(ch) ? ch - '0' : 10 + CryStringUtils::toLowerAscii(ch) - 'a') & 0xF);
					decodedQuery.append(&result, 1);
					break;
				case '+':
					decodedQuery.append(" ", 1);
					break;
				default:
					decodedQuery.append(&ch, 1);
				}
			}

			return decodedQuery.c_str();
		}
	}

}