// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SyntaxErrorCollector_XML.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace DataSource_XML
	{

		CSyntaxErrorCollector_XML::CSyntaxErrorCollector_XML(int xmlLineNumber, const std::shared_ptr<CXMLDataErrorCollector>& pDataErrorCollector)
			: m_xmlLineNumber(xmlLineNumber)
			, m_pDataErrorCollector(pDataErrorCollector)
		{}

		void CSyntaxErrorCollector_XML::AddErrorMessage(const char* szFormat, ...)
		{
			char tmpText[1024];

			va_list args;
			va_start(args, szFormat);
			vsnprintf(tmpText, CRY_ARRAY_COUNT(tmpText), szFormat, args);
			va_end(args);

			tmpText[CRY_ARRAY_COUNT(tmpText) - 1] = '\0';

			string finalText;
			finalText.Format("line #%i: %s", m_xmlLineNumber, tmpText);

			m_pDataErrorCollector->AddError(finalText.c_str());
		}

		CXMLDataErrorCollector::CXMLDataErrorCollector()
		{
			// nothing
		}

		void CXMLDataErrorCollector::AddError(const char* szError)
		{
			m_errorCollection.push_back(szError);
		}

		size_t CXMLDataErrorCollector::GetErrorCount() const
		{
			return m_errorCollection.size();
		}

		const char* CXMLDataErrorCollector::GetError(size_t index) const
		{
			assert(index < m_errorCollection.size());
			return m_errorCollection[index].c_str();
		}

	}
}
