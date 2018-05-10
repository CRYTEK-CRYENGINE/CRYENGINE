// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
 
//! \cond INTERNAL

#pragma once

#include <CrySerialization/IArchive.h>
#include <CrySerialization/Decorators/ActionButton.h>

//! Base interface for the callback.
//! Register yoursef with the editor GetIEditor()->RegisterUriListener().
struct IUriEventListener
{
	virtual void OnUriReceived(const char* szUri) = 0;
};

//! Abstract class for PropertyTree goto button.
class CInteropBase
{
public:
	virtual ~CInteropBase()
	{
	}

protected:
	void RegisterArchive(Serialization::IArchive& archive)
	{
		if (archive.isEdit())
		{
			archive(Serialization::ActionButton(functor(*this, &CInteropBase::SendInterOpCmd)), "gotoButton", "^> =>");
		}
	}

	virtual void SendInterOpCmd() = 0;
};

//! Simple Implementation for a URI Parser.
//! Format schema://location?key1=value1;key2=value2.
class CCryURI
{
	typedef std::map<string, string> TQueries;

public:
	CCryURI(const char* szUri)
	{
		m_uri = szUri;

		m_schema = m_uri.substr(0, m_uri.find_first_of(':'));
		m_address = m_uri.substr(m_uri.find_first_of(':') + 3);

		string::size_type questionMarkPos = m_address.find_first_of('?');

		if (questionMarkPos != string::npos)
		{
			string queries = m_address.substr(questionMarkPos + 1);
			m_address = m_address.substr(0, questionMarkPos);

			while (!queries.empty())
			{
				string query = queries.substr(0, queries.find_first_of(';'));

				string::size_type equalsPos = query.find_first_of('=');

				if (equalsPos != string::npos)
				{
					string key = query.substr(0, equalsPos);
					string value = query.substr(equalsPos + 1);
					m_queries.insert(std::make_pair(key, value));
				}

				string::size_type nextSemiColonPos = queries.find_first_of(';');
				if (nextSemiColonPos == string::npos)
				{
					break;
				}

				queries = queries.substr(nextSemiColonPos + 1);
			}
		}
	}

	const char* GetUri() const
	{
		return m_uri.c_str();
	}

	const char* GetAdress() const
	{
		return m_address.c_str();
	}

	const char* GetSchema() const
	{
		return m_schema.c_str();
	}

	const char* GetQuery(const char* szKey) const
	{
		TQueries::const_iterator it = m_queries.find(string(szKey));
		if (it != m_queries.end())
		{
			return it->second.c_str();
		}

		return nullptr;
	}

private:
	string   m_schema;
	string   m_address;
	TQueries m_queries;
	string   m_uri;
};

//! \endcond