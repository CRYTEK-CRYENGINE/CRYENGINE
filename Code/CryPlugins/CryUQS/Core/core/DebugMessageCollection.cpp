// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		void CDebugMessageCollection::AddInformation(const char* szFormat, ...)
		{
			m_informationMessages.emplace_back();
			string&s = m_informationMessages.back();
			va_list args;

			va_start(args, szFormat);
			s.FormatV(szFormat, args);
			va_end(args);
		}

		void CDebugMessageCollection::AddWarning(const char* szFormat, ...)
		{
			m_warningMessages.emplace_back();
			string&s = m_warningMessages.back();
			va_list args;

			va_start(args, szFormat);
			s.FormatV(szFormat, args);
			va_end(args);
		}

		bool CDebugMessageCollection::HasSomeWarnings() const
		{
			return !m_warningMessages.empty();
		}

		void CDebugMessageCollection::EmitAllMessagesToQueryHistoryConsumer(IQueryHistoryConsumer& consumer, const ColorF& neutralColor, const ColorF& informationMessageColor, const ColorF& warningMessageColor) const
		{
			//
			// information messages
			//

			if (m_informationMessages.empty())
			{
				consumer.AddTextLineToCurrentHistoricQuery(neutralColor, "No informations");
			}
			else
			{
				consumer.AddTextLineToCurrentHistoricQuery(informationMessageColor, "%i informations:", (int)m_informationMessages.size());
				for (const string& informationMessage : m_informationMessages)
				{
					consumer.AddTextLineToCurrentHistoricQuery(informationMessageColor, "%s", informationMessage.c_str());
				}
			}

			//
			// warning messages
			//

			if (m_warningMessages.empty())
			{
				consumer.AddTextLineToCurrentHistoricQuery(neutralColor, "No warnings");
			}
			else
			{
				consumer.AddTextLineToCurrentHistoricQuery(warningMessageColor, "%i warnings:", (int)m_warningMessages.size());
				for (const string& warningMessage : m_warningMessages)
				{
					consumer.AddTextLineToCurrentHistoricQuery(warningMessageColor, "%s", warningMessage.c_str());
				}
			}
		}

		void CDebugMessageCollection::Serialize(Serialization::IArchive& ar)
		{
			ar(m_informationMessages, "m_informationMessages");
			ar(m_warningMessages, "m_warningMessages");
		}

	}
}
