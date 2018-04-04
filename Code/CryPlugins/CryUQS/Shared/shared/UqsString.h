// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

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

	}
}
