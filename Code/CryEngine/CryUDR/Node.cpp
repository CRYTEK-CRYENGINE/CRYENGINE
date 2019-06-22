// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace Cry
{
	namespace UDR
	{

		CNode::CNode()
			: m_pOwningTree(nullptr)
			, m_pParent(nullptr)
			, m_name()
			, m_renderPrimitives(this)
			, m_logMessageCollection(this)
		{
			// nothing
		}

		CNode::CNode(const char* szNodeName, CTree& owningTree)
			: m_pOwningTree(&owningTree)
			, m_pParent(nullptr)
			, m_name(szNodeName)
			, m_renderPrimitives(this)
			, m_logMessageCollection(this)
		{
			// nothing
		}

		CNode& CNode::operator=(CNode&& other)
		{
			if (this != &other)
			{
				m_pOwningTree = std::move(other.m_pOwningTree);
				m_name = std::move(other.m_name);
				m_renderPrimitives = std::move(other.m_renderPrimitives);
				m_renderPrimitives.SetParentNode(this);
				m_children = std::move(other.m_children);
				m_childrenByName = std::move(other.m_childrenByName);
				m_uniqueNamesCache = std::move(other.m_uniqueNamesCache);
				m_logMessageCollection = std::move(other.m_logMessageCollection);
				m_logMessageCollection.SetParentNode(this);
				m_timeMetadataMin = std::move(other.m_timeMetadataMin);
				m_timeMetadataMax = std::move(other.m_timeMetadataMax);
				SetOwningTreeInChildrenRecursively();
				SetParentInChildrenRecursively();
				other.m_pOwningTree = nullptr;
				other.m_pParent = nullptr;
			}
			return *this;
		}

		INode& CNode::AddChild_FixedStringIfNotYetExisting(const char* szChildNodeName)
		{
			CNode* pChild = FindChildNode(szChildNodeName);

			if (!pChild)
			{
				pChild = &AddChildNode(szChildNodeName);
			}

			return *pChild;
		}

		INode& CNode::AddChild_UniqueStringAutoIncrement(const char* szChildNodeNamePrefix)
		{
			int nextAutoIncrementValueToUse;
			auto pair = m_uniqueNamesCache.insert(std::pair<string, int>(szChildNodeNamePrefix, 0));
			const bool isFreshlyInserted = pair.second;

			if (isFreshlyInserted)
			{
				nextAutoIncrementValueToUse = 0;
			}
			else
			{
				nextAutoIncrementValueToUse = ++(pair.first->second);
			}

			stack_string uniqueName;
			uniqueName.Format("%s%i", szChildNodeNamePrefix, nextAutoIncrementValueToUse);
			return AddChildNode(uniqueName.c_str());
		}

		INode& CNode::AddChild_FindLastAutoIncrementedUniqueNameOrCreateIfNotYetExisting(const char* szChildNodeNamePrefix)
		{
			auto it = m_uniqueNamesCache.find(szChildNodeNamePrefix);
			if (it == m_uniqueNamesCache.cend())
			{
				return AddChild_UniqueStringAutoIncrement(szChildNodeNamePrefix);
			}
			else
			{
				const int autoIncrement = it->second;
				stack_string uniqueName;
				uniqueName.Format("%s%i", szChildNodeNamePrefix, autoIncrement);
				CNode* pChild = FindChildNode(uniqueName.c_str());
				CRY_ASSERT(pChild);	// if this fails, then we kept track of an alleged unique name in m_uniqueNamesCache when in fact there was no corresponding actual node in m_childrenByName
				return *pChild;
			}
		}

		INode& CNode::AddChild_CurrentSystemFrame()
		{
			stack_string name;
			name.Format("SystemFrame_%i", static_cast<int>(gEnv->nMainFrameID));
			CNode* pChild = FindChildNode(name.c_str());
			if (!pChild)
			{
				pChild = &AddChildNode(name.c_str());
			}
			return *pChild;
		}

		IRenderPrimitiveCollection& CNode::GetRenderPrimitiveCollection()
		{
			return m_renderPrimitives;
		}

		ILogMessageCollection& CNode::GetLogMessageCollection()
		{
			return m_logMessageCollection;
		}

		const char* CNode::GetName() const
		{
			return m_name.c_str();
		}

		const INode* CNode::GetParent() const
		{
			return m_pParent;
		}

		size_t CNode::GetChildCount() const
		{
			return m_children.size();
		}

		const INode& CNode::GetChild(size_t index) const
		{
			CRY_ASSERT(index < m_children.size());
			return *m_children[index];
		}

		const ITimeMetadata& CNode::GetTimeMetadataMin() const
		{
			return m_timeMetadataMin;
		}

		const ITimeMetadata& CNode::GetTimeMetadataMax() const
		{
			return m_timeMetadataMax;
		}

		void CNode::DrawRenderPrimitives(bool recursivelyDrawChildrenAsWell) const
		{
			m_renderPrimitives.Draw();

			if (recursivelyDrawChildrenAsWell)
			{
				for (const std::unique_ptr<CNode>& pChild : m_children)
				{
					pChild->DrawRenderPrimitives(true);
				}
			}
		}

		void CNode::DrawRenderPrimitives(bool recursivelyDrawChildrenAsWell, const CTimeValue start, const CTimeValue end) const
		{
			m_renderPrimitives.Draw(start, end);

			if (recursivelyDrawChildrenAsWell)
			{
				for (const std::unique_ptr<CNode>& pChild : m_children)
				{
					pChild->DrawRenderPrimitives(true, start, end);
				}
			}
		}

		void CNode::VisitLogMessages(ILogMessageVisitor& visitor, bool bRecurseDownAllChildren) const
		{
			m_logMessageCollection.Visit(visitor);

			if (bRecurseDownAllChildren)
			{
				for (const std::unique_ptr<CNode>& pChild : m_children)
				{
					pChild->VisitLogMessages(visitor, bRecurseDownAllChildren);
				}
			}
		}

		void CNode::VisitLogMessages(ILogMessageVisitor& visitor, bool bRecurseDownAllChildren, const CTimeValue start, const CTimeValue end) const
		{
			m_logMessageCollection.Visit(visitor, start, end);

			if (bRecurseDownAllChildren)
			{
				for (const std::unique_ptr<CNode>& pChild : m_children)
				{
					pChild->VisitLogMessages(visitor, bRecurseDownAllChildren, start, end);
				}
			}
		}

		CNode* CNode::FindChildNode(const char* szChildNodeName) const
		{
			auto it = m_childrenByName.find(szChildNodeName);
			if (it == m_childrenByName.cend())
			{
				return nullptr;
			}
			else
			{
				const size_t index = it->second;
				return m_children[index].get();
			}
		}

		CNode& CNode::AddChildNode(const char* szChildNodeName)
		{
			CRY_ASSERT(m_pOwningTree);

			CNode* pChild = new CNode(szChildNodeName, *m_pOwningTree);
			pChild->m_pParent = this;
			m_children.emplace_back(pChild);
			m_childrenByName[pChild->m_name] = m_children.size() - 1;
			m_pOwningTree->OnNodeAdded(*pChild);
			return *pChild;
		}

		void CNode::RemoveChild(size_t index)
		{
			CRY_ASSERT(index < m_children.size());

			const CTimeMetadata removedNodeMetadataMin = m_children[index]->m_timeMetadataMin;
			const CTimeMetadata removedNodeMetadataMax = m_children[index]->m_timeMetadataMax;

			const void* pFormerAddressOfNode =  m_children[index].get();
			m_pOwningTree->OnBeforeNodeRemoved(*m_children[index]);
			m_childrenByName.erase(m_children[index]->m_name);
			m_children.erase(m_children.begin() + index);
			m_pOwningTree->OnAfterNodeRemoved(pFormerAddressOfNode);

			if (removedNodeMetadataMin.IsValid() && m_timeMetadataMin.IsValid() && removedNodeMetadataMin == m_timeMetadataMin)
			{
				m_pOwningTree->GetRootNodeWritable().UpdateTimeMetadataMin();
			}

			if (removedNodeMetadataMax.IsValid() && m_timeMetadataMax.IsValid() && removedNodeMetadataMax == m_timeMetadataMax)
			{
				m_pOwningTree->GetRootNodeWritable().UpdateTimeMetadataMax();
			}
		}

		void CNode::Serialize(Serialization::IArchive& ar)
		{
			ar(m_name, "m_name");
			m_renderPrimitives.Serialize(ar);	// calling this directly is for backwards compatibility of recordings (previously, the render-primitives were stored in this CNode class, now they're stored in a separate class thus adding another layer in the archive, which makes old recordings incompatible)
			ar(m_children, "m_children");
			ar(m_uniqueNamesCache, "m_uniqueNamesCache");
			ar(m_logMessageCollection, "m_logMessageCollection");
			ar(m_timeMetadataMin, "m_timeMetadataMin");
			ar(m_timeMetadataMax, "m_timeMetadataMax");
		}

		CNode* CNode::GetParentWritable() const
		{
			return m_pParent;
		}

		CNode* CNode::FindNodeByINode(const INode& nodeToSearchFor)
		{
			if (&nodeToSearchFor == this)
			{
				return this;
			}

			for (const std::unique_ptr<CNode>& pChild : m_children)
			{
				if (CNode* pNode = pChild->FindNodeByINode(nodeToSearchFor))
				{
					return pNode;
				}
			}

			return nullptr;
		}

		void CNode::RemoveChildren()
		{
			m_timeMetadataMin = CTimeMetadata();
			m_timeMetadataMax = CTimeMetadata();

			for (const std::unique_ptr<CNode>& pChild : m_children)
			{
				pChild->RemoveChildren();
			}

			while (!m_children.empty())
			{
				RemoveChild(m_children.size() - 1);
			}
		}

		void CNode::GatherStatisticsRecursively(SStatistics& outStats) const
		{
			outStats.numChildren += m_children.size();

			m_renderPrimitives.GatherStatistics(outStats.numRenderPrimitives, outStats.roughMemoryUsageOfRenderPrimitives);

			for (const std::unique_ptr<CNode>& pChild : m_children)
			{
				pChild->GatherStatisticsRecursively(outStats);
			}
		}

		void CNode::PrintHierarchyToConsoleRecursively(int indentationAmount) const
		{
			CryLogAlways("%*s%s [%i children]", indentationAmount, "", m_name.c_str(), static_cast<size_t>(m_children.size()));

			for (const std::unique_ptr<CNode>& pChild : m_children)
			{
				pChild->PrintHierarchyToConsoleRecursively(indentationAmount + 2);
			}
		}

		void CNode::PrintStatisticsToConsole(const char* szMessagePrefix) const
		{
			SStatistics stats;

			GatherStatisticsRecursively(stats);

			CryLogAlways("%s%i nodes in total, %i render primitives in total, %i bytes (%.2f kb [%.2f mb]) in all render primitives",
				szMessagePrefix,
				static_cast<size_t>(stats.numChildren),
				static_cast<size_t>(stats.numRenderPrimitives),
				static_cast<size_t>(stats.roughMemoryUsageOfRenderPrimitives),
				static_cast<float>(stats.roughMemoryUsageOfRenderPrimitives) / 1024.0f,
				static_cast<float>(stats.roughMemoryUsageOfRenderPrimitives) / 1024.0f / 1024.0f);
		}

		void CNode::OnTimeMetadataMinChanged(const CTimeMetadata& metadata)
		{
			if (!m_timeMetadataMin.IsValid() || m_timeMetadataMin > metadata)
			{
				m_timeMetadataMin = metadata;
				if (m_pParent)
				{
					m_pParent->OnTimeMetadataMinChanged(m_timeMetadataMin);
				}
			}
		}

		void CNode::OnTimeMetadataMaxChanged(const CTimeMetadata& metadata)
		{
			if (!m_timeMetadataMax.IsValid() || m_timeMetadataMax < metadata)
			{
				m_timeMetadataMax = metadata;
				if (m_pParent)
				{
					m_pParent->OnTimeMetadataMaxChanged(m_timeMetadataMax);
				}
			}
		}

		void CNode::SetOwningTreeInChildrenRecursively()
		{
			for (std::unique_ptr<CNode>& pChild : m_children)
			{
				pChild->m_pOwningTree = m_pOwningTree;
				pChild->SetOwningTreeInChildrenRecursively();
			}
		}

		void CNode::SetParentInChildrenRecursively()
		{
			for (std::unique_ptr<CNode>& pChild : m_children)
			{
				pChild->m_pParent = this;
				pChild->SetParentInChildrenRecursively();
			}
		}

		void CNode::BuildFastLookupTable()
		{
			CRY_ASSERT(m_childrenByName.empty());	// We assert that something is true, otherwise the code was used wrongly.

			for (size_t i = 0; i < m_children.size(); i++)
			{
				m_childrenByName[m_children[i]->m_name] = i;
			}
		}
	
		void CNode::UpdateTimeMetadataMin()
		{
			CTimeMetadata min;
			for (std::unique_ptr<CNode>& pChild : m_children)
			{
				pChild->UpdateTimeMetadataMin();
				if (!min.IsValid() || (pChild->m_timeMetadataMin.IsValid() && min > pChild->m_timeMetadataMin))
				{
					min = pChild->m_timeMetadataMin;
				}
			}

			const CTimeMetadata renderPrimitivesTimeMetadataMin = m_renderPrimitives.GetTimeMetadataMin();
			if (!min.IsValid() || (renderPrimitivesTimeMetadataMin.IsValid() && min > renderPrimitivesTimeMetadataMin))
			{
				min = renderPrimitivesTimeMetadataMin;
			}

			const CTimeMetadata logMessagesTimeMetadataMin = m_logMessageCollection.GetTimeMetadataMin();
			if (!min.IsValid() || (logMessagesTimeMetadataMin.IsValid() && min > logMessagesTimeMetadataMin))
			{
				min = logMessagesTimeMetadataMin;
			}

			m_timeMetadataMin = min;
		}

		void CNode::UpdateTimeMetadataMax()
		{
			CTimeMetadata max;
			for (std::unique_ptr<CNode>& pChild : m_children)
			{
				pChild->UpdateTimeMetadataMax();
				if (!max.IsValid() || (pChild->m_timeMetadataMax.IsValid() && max < pChild->m_timeMetadataMax))
				{
					max = pChild->m_timeMetadataMax;
				}
			}

			const CTimeMetadata renderPrimitivesTimeMetadataMax = m_renderPrimitives.GetTimeMetadataMax();
			if (!max.IsValid() || (renderPrimitivesTimeMetadataMax.IsValid() && max < renderPrimitivesTimeMetadataMax))
			{
				max = renderPrimitivesTimeMetadataMax;
			}

			const CTimeMetadata logMessagesTimeMetadataMax = m_logMessageCollection.GetTimeMetadataMax();
			if (!max.IsValid() || (logMessagesTimeMetadataMax.IsValid() && max < logMessagesTimeMetadataMax))
			{
				max = logMessagesTimeMetadataMax;
			}

			m_timeMetadataMax = max;
		}

	}
}