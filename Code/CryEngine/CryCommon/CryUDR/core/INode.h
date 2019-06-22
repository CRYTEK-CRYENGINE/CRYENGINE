// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace Cry
{
	namespace UDR
	{

		struct INode
		{
			struct ILogMessageVisitor
			{
				virtual void                    OnLogMessageVisited(const char* szLogMessage, ELogMessageType messageType) = 0;
			};

			virtual INode&                      AddChild_FixedStringIfNotYetExisting(const char* szChildNodeName) = 0;
			virtual INode&                      AddChild_UniqueStringAutoIncrement(const char* szChildNodeNamePrefix) = 0;
			virtual INode&                      AddChild_FindLastAutoIncrementedUniqueNameOrCreateIfNotYetExisting(const char* szChildNodeNamePrefix) = 0;
			virtual INode&                      AddChild_CurrentSystemFrame() = 0;

			virtual IRenderPrimitiveCollection& GetRenderPrimitiveCollection() = 0;
			virtual ILogMessageCollection&      GetLogMessageCollection() = 0;
			virtual const char*                 GetName() const = 0;
			virtual const INode*                GetParent() const = 0;
			virtual size_t                      GetChildCount() const = 0;
			virtual const INode&                GetChild(size_t index) const = 0;
			virtual const ITimeMetadata&        GetTimeMetadataMin() const = 0;
			virtual const ITimeMetadata&        GetTimeMetadataMax() const = 0;

			virtual void                        DrawRenderPrimitives(bool RecursivelyDrawChildrenAsWell) const = 0;
			virtual void                        DrawRenderPrimitives(bool recursivelyDrawChildrenAsWell, const CTimeValue start, const CTimeValue end) const = 0;
			virtual void                        VisitLogMessages(ILogMessageVisitor& visitor, bool bRecurseDownAllChildren) const = 0;
			virtual void                        VisitLogMessages(ILogMessageVisitor& visitor, bool bRecurseDownAllChildren, const CTimeValue start, const CTimeValue end) const = 0;

		protected:

			~INode() = default;  // not intended to get deleted via base class pointers

		};

	}
}