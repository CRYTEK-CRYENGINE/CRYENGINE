// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace DataSource_XML
	{

		class CXMLDataErrorCollector;    // below

		//===================================================================================
		//
		// CSyntaxErrorCollector_XML
		//
		//===================================================================================

		class CSyntaxErrorCollector_XML : public DataSource::ISyntaxErrorCollector
		{
		public:
			explicit                                   CSyntaxErrorCollector_XML(int xmlLineNumber, const std::shared_ptr<CXMLDataErrorCollector>& pDataErrorCollector);
			virtual void                               AddErrorMessage(const char* szFormat, ...) override;

		private:
			                                           UQS_NON_COPYABLE(CSyntaxErrorCollector_XML);

			int                                        m_xmlLineNumber;
			std::shared_ptr<CXMLDataErrorCollector>    m_pDataErrorCollector;
		};

		//===================================================================================
		//
		// CXMLDataErrorCollector
		//
		//===================================================================================

		class CXMLDataErrorCollector
		{
		public:
			explicit                              CXMLDataErrorCollector();

			void                                  AddError(const char* szError);
			size_t                                GetErrorCount() const;
			const char*                           GetError(size_t index) const;

		private:
			                                      UQS_NON_COPYABLE(CXMLDataErrorCollector);

		private:
			std::vector<string>                   m_errorCollection;
		};

	}
}
