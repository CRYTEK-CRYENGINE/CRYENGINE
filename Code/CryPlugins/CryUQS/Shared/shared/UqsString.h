// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Shared
	{

		//===================================================================================
		//
		// CUqsString
		//
		//===================================================================================

		class CUqsString : public IUqsString
		{
		public:
			explicit                      CUqsString();
			explicit                      CUqsString(const char* szString);

			// IUqsString
			virtual void                  Set(const char* szString) override;
			virtual void                  Format(const char* szFormat, ...) override PRINTF_PARAMS(2, 3);
			virtual const char*           c_str() const override;
			// ~IUqsString

		private:
			string                        m_message;
		};

		inline CUqsString::CUqsString()
			: m_message()
		{
			// nothing
		}

		inline CUqsString::CUqsString(const char* szString)
			: m_message(szString)
		{
			// nothing
		}

		inline void CUqsString::Set(const char* szString)
		{
			m_message = szString;
		}

		inline void CUqsString::Format(const char* szFormat, ...)
		{
			va_list ap;
			va_start(ap, szFormat);
			m_message.FormatV(szFormat, ap);
			va_end(ap);
		}

		inline const char* CUqsString::c_str() const
		{
			return m_message.c_str();
		}

	}
}
