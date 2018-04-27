// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CDebugMessageCollection
		//
		//===================================================================================

		class CDebugMessageCollection final : public IDebugMessageCollection
		{
		public:

			// IDebugMessageCollection
			virtual void          AddInformation(const char* szFormat, ...) override PRINTF_PARAMS(2, 3);
			virtual void          AddWarning(const char* szFormat, ...) override PRINTF_PARAMS(2, 3);
			// ~IDebugMessageCollection

			bool                  HasSomeWarnings() const;
			void                  EmitAllMessagesToQueryHistoryConsumer(IQueryHistoryConsumer& consumer, const ColorF& neutralColor, const ColorF& informationMessageColor, const ColorF& warningMessageColor) const;
			void                  Serialize(Serialization::IArchive& ar);

		private:

			std::vector<string>   m_informationMessages;
			std::vector<string>   m_warningMessages;
		};

	}
}
