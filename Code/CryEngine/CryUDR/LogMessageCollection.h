// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace Cry
{
	namespace UDR
	{
		class CNode;

		class CLogMessageCollection final : public ILogMessageCollection
		{
		private:

			struct SMessage
			{
				                   SMessage();  // default ctor required for yasli serialization
				explicit           SMessage(const char* _szText, ELogMessageType _type);
				void               Serialize(Serialization::IArchive& ar);

				string             text;
				ELogMessageType    type;

				CTimeMetadata      metadata;
			};

		public:

			explicit CLogMessageCollection(CNode* pNode = nullptr);

			// ILogMessageCollection
			virtual void           LogInformation(const char* szFormat, ...) override PRINTF_PARAMS(2, 3);
			virtual void           LogWarning(const char* szFormat, ...) override PRINTF_PARAMS(2, 3);
			// ~ILogMessageCollection

			CTimeMetadata          GetTimeMetadataMin() const;
			CTimeMetadata          GetTimeMetadataMax() const;

			void                   SetParentNode(CNode* pNode);
			void                   Visit(INode::ILogMessageVisitor& visitor) const;
			void                   Visit(INode::ILogMessageVisitor& visitor, const CTimeValue start, const CTimeValue end) const;
			void                   Serialize(Serialization::IArchive& ar);

		private:

			void                   UpdateTimeMetadata(const CTimeMetadata& timeMetadata);

		private:

			CNode*                 m_pNode;
			std::vector<SMessage>  m_messages;
			
			// Required to Update Node Time Metadata when a node is deleted. Prevents us from iterating all log messages.
			CTimeMetadata          m_timeMetadataMin;
			CTimeMetadata          m_timeMetadataMax;
		};

	}
}