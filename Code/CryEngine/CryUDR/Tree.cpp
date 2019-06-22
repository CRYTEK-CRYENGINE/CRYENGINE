// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace Cry
{
	namespace UDR
	{

		CTree::CTree()
			: m_root("root", *this)
		{
		}

		const CNode& CTree::GetRootNode() const
		{
			return m_root;
		}

		void CTree::RegisterListener(ITreeListener* pListenerToRegister)
		{
			stl::push_back_unique(m_listeners, pListenerToRegister);
		}

		void CTree::UnregisterListener(ITreeListener* pListenerToUnregister)
		{
			stl::find_and_erase_all(m_listeners, pListenerToUnregister);
		}

		void CTree::RemoveNode(const INode& nodeToRemove)
		{
			CRecursiveSyncObjectAutoLock _lock;
			CNode* pFoundNode = m_root.FindNodeByINode(nodeToRemove);
			CRY_ASSERT_MESSAGE(pFoundNode, "CTree::RemoveNode: the passed-in node does *not* belong to the tree! (wrong use of the RemoveNode() method!)");

			// trying to remove the tree node is a special case: just remove its children, but leave the root node around
			if (pFoundNode == &m_root)
			{
				pFoundNode->RemoveChildren();
			}
			else
			{
				CNode* pParent = pFoundNode->GetParentWritable();
				for (size_t childIndex = 0; ; ++childIndex)
				{
					if (&pParent->GetChild(childIndex) == pFoundNode)
					{
						pParent->RemoveChild(childIndex);
						break;
					}
				}
			}
		}

		CNode& CTree::GetRootNodeWritable()
		{
			return m_root;
		}

		bool CTree::Save(const char* szXmlFilePath, IString& outError) const
		{
			Serialization::IArchiveHost* pArchiveHost = gEnv->pSystem->GetArchiveHost();
			if (pArchiveHost->SaveXmlFile(szXmlFilePath, Serialization::SStruct(m_root), "UDR_Recordings"))
			{
				return true;
			}
			else
			{
				outError.Format("Could not serialize the live recordings to xml file '%s' (Serialization::IArchiveHost::SaveXmlFile() failed for some reason)", szXmlFilePath);
				return false;
			}
		}

		bool CTree::Load(const char* szXmlFilePath, IString& outError)
		{
			CNode tempRootNode("name_will_get_restored_by_deserialization_anyway", *this);

			Serialization::IArchiveHost* pArchiveHost = gEnv->pSystem->GetArchiveHost();
			if (pArchiveHost->LoadXmlFile(Serialization::SStruct(tempRootNode), szXmlFilePath))
			{
				for (ITreeListener* pListener : m_listeners)
				{
					pListener->OnBeforeRootNodeDeserialized();
				}
				m_root = std::move(tempRootNode);
				return true;
			}
			else
			{
				outError.Format("Could not de-serialize the recordings from xml file '%s'", szXmlFilePath);
				return false;
			}
		}

		void CTree::OnNodeAdded(const INode& freshlyAddedNode) const
		{
			for (ITreeListener* pListener : m_listeners)
			{
				pListener->OnNodeAdded(freshlyAddedNode);
			}
		}

		void CTree::OnBeforeNodeRemoved(const INode& nodeBeingRemoved) const
		{
			for (ITreeListener* pListener : m_listeners)
			{
				pListener->OnBeforeNodeRemoved(nodeBeingRemoved);
			}
		}

		void CTree::OnAfterNodeRemoved(const void* pFormerAddressOfNode) const
		{
			for (ITreeListener* pListener : m_listeners)
			{
				pListener->OnAfterNodeRemoved(pFormerAddressOfNode);
			}
		}

		void CTree::Clear()
		{
			CRecursiveSyncObjectAutoLock _lock;
			m_root.RemoveChildren();
		}
	}
}