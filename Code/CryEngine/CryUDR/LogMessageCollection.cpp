// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace Cry
{
	namespace UDR
	{

		SERIALIZATION_ENUM_BEGIN(ELogMessageType, "")
		SERIALIZATION_ENUM(ELogMessageType::Information, "Information", "")
		SERIALIZATION_ENUM(ELogMessageType::Warning, "Warning", "")
		SERIALIZATION_ENUM_END()

		CLogMessageCollection::SMessage::SMessage()
			: text()
			, type(ELogMessageType::Information)
		{
		}

		CLogMessageCollection::SMessage::SMessage(const char* _szText, ELogMessageType _type)
			: text(_szText)
			, type(_type)
		{
			metadata.Initialize();
		}

		CLogMessageCollection::CLogMessageCollection(CNode* pNode)
			: m_pNode(pNode)
		{
			// Empty
		}

		void CLogMessageCollection::SMessage::Serialize(Serialization::IArchive& ar)
		{
			ar(text, "text");
			ar(type, "type");
			ar(metadata, "metadata");
		}

		void CLogMessageCollection::LogInformation(const char* szFormat, ...)
		{
			stack_string text;
			va_list args;
			va_start(args, szFormat);
			text.FormatV(szFormat, args);
			va_end(args);
			m_messages.emplace_back(text.c_str(), ELogMessageType::Information);
			UpdateTimeMetadata(m_messages.back().metadata);
		}

		void CLogMessageCollection::LogWarning(const char* szFormat, ...)
		{
			stack_string text;
			va_list args;
			va_start(args, szFormat);
			text.FormatV(szFormat, args);
			va_end(args);
			m_messages.emplace_back(text.c_str(), ELogMessageType::Warning);
			UpdateTimeMetadata(m_messages.back().metadata);
		}

//#error next steps: (1) test: add some log messages via game code and save them, (2) editor: show log messages of the selected nodes via an additional text box

		CTimeMetadata CLogMessageCollection::GetTimeMetadataMin() const
		{
			return m_timeMetadataMin;
		}
		
		CTimeMetadata CLogMessageCollection::GetTimeMetadataMax() const
		{
			return m_timeMetadataMax;
		}

		void CLogMessageCollection::SetParentNode(CNode* pNode)
		{
			m_pNode = pNode;
		}

		void CLogMessageCollection::Visit(INode::ILogMessageVisitor& visitor) const
		{
			for (const SMessage& msg : m_messages)
			{
				visitor.OnLogMessageVisited(msg.text.c_str(), msg.type);
			}
		}

		void CLogMessageCollection::Visit(INode::ILogMessageVisitor& visitor, const CTimeValue start, const CTimeValue end) const
		{
			for (const SMessage& msg : m_messages)
			{
				if (msg.metadata.IsInTimeInterval(start, end))
				{
					visitor.OnLogMessageVisited(msg.text.c_str(), msg.type);
				}
			}
		}

		void CLogMessageCollection::Serialize(Serialization::IArchive& ar)
		{
			ar(m_messages, "messages");
			ar(m_timeMetadataMin, "timeMetadataMin");
			ar(m_timeMetadataMax, "timeMetadataMax");
		}

		void CLogMessageCollection::UpdateTimeMetadata(const CTimeMetadata& timeMetadata)
		{
			CRY_ASSERT_MESSAGE(timeMetadata.IsValid(), "Parameter 'timeMetadata' must be valid.");

			if (!m_timeMetadataMin.IsValid() || m_timeMetadataMin > timeMetadata)
			{
				m_timeMetadataMin = timeMetadata;
				m_pNode->OnTimeMetadataMinChanged(m_timeMetadataMin);
			}

			if (!m_timeMetadataMax.IsValid() || m_timeMetadataMax < timeMetadata)
			{
				m_timeMetadataMax = timeMetadata;
				m_pNode->OnTimeMetadataMaxChanged(m_timeMetadataMax);
			}
		}

	}
}