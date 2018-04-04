// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioControlsEditorUndo.h"
#include "AudioAssetsManager.h"
#include "AudioControlsEditorPlugin.h"
#include "QtUtil.h"

#include <QList>

namespace ACE
{
//QStandardItem* GetParent(QATLTreeModel* pTree, TPath& path)
//{
//	QStandardItem* pParent = pTree->invisibleRootItem();
//	if (pParent)
//	{
//		const size_t size = path.size();
//		for (size_t i = 0; i < size - 1; ++i)
//		{
//			pParent = pParent->child(path[(size - 1) - i]);
//		}
//	}
//	return pParent;
//}
//
//QStandardItem* GetItem(QATLTreeModel* pTree, TPath& path)
//{
//	QStandardItem* pParent = GetParent(pTree, path);
//	if (pParent && path.size() > 0)
//	{
//		return pParent->child(path[0]);
//	}
//	return nullptr;
//}
/*
   void UpdatePath(QStandardItem* pItem, TPath& path)
   {
   if (pItem)
   {
   path.clear();
   path.push_back(pItem->row());
   QStandardItem* pParent = pItem->parent();
   while (pParent)
   {
    path.push_back(pParent->row());
    pParent = pParent->parent();
   }
   }
   }*/

void IUndoControlOperation::AddStoredControl()
{
	/*const CScopedSuspendUndo suspendUndo;
	   CATLControlsModel* pModel = CAudioControlsEditorPlugin::GetAssetsManager();
	   QATLTreeModel* pTree = CAudioControlsEditorPlugin::GetControlsTree();
	   if (pModel && pTree)
	   {
	   if (m_pStoredControl)
	   {
	    pModel->InsertControl(m_pStoredControl);
	    m_id = m_pStoredControl->GetId();

	    QStandardItem* pParent = GetParent(pTree, m_path);
	    if (pParent && m_path.size() > 0)
	    {
	      pTree->AddControl(m_pStoredControl.get(), pParent, m_path[0]);
	    }
	   }
	   m_pStoredControl = nullptr;
	   }*/
}

void IUndoControlOperation::RemoveStoredControl()
{
	/*const CScopedSuspendUndo suspendUndo;
	   CATLControlsModel* pModel = CAudioControlsEditorPlugin::GetAssetsManager();
	   QATLTreeModel* pTree = CAudioControlsEditorPlugin::GetControlsTree();
	   if (pModel && pTree)
	   {
	   m_pStoredControl = pModel->TakeControl(m_id);
	   QStandardItem* pItem = pTree->GetItemFromControlID(m_id);
	   if (pItem)
	   {
	    UpdatePath(pItem, m_path);
	    pTree->RemoveItem(pTree->indexFromItem(pItem));
	   }
	   }*/
}

CUndoControlAdd::CUndoControlAdd(CID id)
{
	m_id = id;
}

void CUndoControlAdd::Undo(bool bUndo)
{
	RemoveStoredControl();
}

void CUndoControlAdd::Redo()
{
	AddStoredControl();
}

CUndoControlRemove::CUndoControlRemove(std::shared_ptr<CAudioControl>& pControl)
{
	/*CScopedSuspendUndo suspendUndo;
	   m_pStoredControl = pControl;
	   QATLTreeModel* pTree = CAudioControlsEditorPlugin::GetControlsTree();
	   if (pTree)
	   {
	   QStandardItem* pItem = pTree->GetItemFromControlID(m_pStoredControl->GetId());
	   if (pItem)
	   {
	    UpdatePath(pItem, m_path);
	   }
	   }*/
}

void CUndoControlRemove::Undo(bool bUndo)
{
	AddStoredControl();
}

void CUndoControlRemove::Redo()
{
	RemoveStoredControl();
}
/*
   IUndoFolderOperation::IUndoFolderOperation(QStandardItem* pItem)
   {
   if (pItem)
   {
    m_sName = pItem->text();
    UpdatePath(pItem, m_path);
   }
   }*/

void IUndoFolderOperation::AddFolderItem()
{
	/*CScopedSuspendUndo suspendUndo;
	   QATLTreeModel* pTree = CAudioControlsEditorPlugin::GetControlsTree();
	   if (pTree)
	   {
	   QStandardItem* pParent = GetParent(pTree, m_path);
	   if (pParent && m_path.size() > 0)
	   {
	    pTree->CreateFolder(pParent, QtUtil::ToString(m_sName), m_path[0]);
	   }
	   }*/
}

void IUndoFolderOperation::RemoveItem()
{
	/*CScopedSuspendUndo suspendUndo;
	   QATLTreeModel* pTree = CAudioControlsEditorPlugin::GetControlsTree();
	   if (pTree)
	   {
	   QStandardItem* pItem = GetItem(pTree, m_path);
	   if (pItem)
	   {
	    pTree->RemoveItem(pTree->indexFromItem(pItem));
	   }
	   }*/
}
/*
   CUndoFolderRemove::CUndoFolderRemove(QStandardItem* pItem)
   : IUndoFolderOperation(pItem)
   {
   }*/

void CUndoFolderRemove::Undo(bool bUndo)
{
	AddFolderItem();
}

void CUndoFolderRemove::Redo()
{
	RemoveItem();
}
/*
   CUndoFolderAdd::CUndoFolderAdd(QStandardItem* pItem)
   : IUndoFolderOperation(pItem)
   {
   }*/

void CUndoFolderAdd::Undo(bool bUndo)
{
	RemoveItem();
}

void CUndoFolderAdd::Redo()
{
	AddFolderItem();
}

CUndoControlModified::CUndoControlModified(CID id) : m_id(id)
{
	/*CATLControlsModel* pModel = CAudioControlsEditorPlugin::GetAssetsManager();
	   if (pModel)
	   {
	   CAudioControl* pControl = pModel->GetControlByID(m_id);
	   if (pControl)
	   {
	    m_name = pControl->GetName();
	    m_scope = pControl->GetScope();
	    m_bAutoLoad = pControl->IsAutoLoad();
	    m_connectedControls = pControl->m_connectedControls;
	   }
	   }*/
}

void CUndoControlModified::Undo(bool bUndo)
{
	SwapData();
}

void CUndoControlModified::Redo()
{
	SwapData();
}

void CUndoControlModified::SwapData()
{
	const CScopedSuspendUndo suspendUndo;
	CAudioAssetsManager* pAssetsManager = CAudioControlsEditorPlugin::GetAssetsManager();
	if (pAssetsManager)
	{
		CAudioControl* pControl = pAssetsManager->GetControlByID(m_id);
		if (pControl)
		{
			string name = pControl->GetName();
			Scope scope = pControl->GetScope();
			bool bAutoLoad = pControl->IsAutoLoad();

			std::vector<ConnectionPtr> connectedControls;
			const size_t size = pControl->GetConnectionCount();
			for (size_t i = 0; i < size; ++i)
			{
				connectedControls.push_back(pControl->GetConnectionAt(i));
			}
			pControl->ClearConnections();

			pControl->SetName(m_name);
			pControl->SetScope(m_scope);
			pControl->SetAutoLoad(m_bAutoLoad);

			for (ConnectionPtr& c : m_connectedControls)
			{
				pControl->AddConnection(c);
			}

			m_name = name;
			m_scope = scope;
			m_bAutoLoad = bAutoLoad;
			m_connectedControls = connectedControls;
		}
	}
}

void CUndoItemMove::Undo(bool bUndo)
{
	/*QATLTreeModel* pTree = CAudioControlsEditorPlugin::GetControlsTree();
	   if (pTree)
	   {
	   if (!bModifiedInitialised)
	   {
	    Copy(pTree->invisibleRootItem(), m_modified.invisibleRootItem());
	    bModifiedInitialised = true;
	   }

	   pTree->clear();
	   Copy(m_original.invisibleRootItem(), pTree->invisibleRootItem());
	   }*/
}

void CUndoItemMove::Redo()
{
	/*QATLTreeModel* pTree = CAudioControlsEditorPlugin::GetControlsTree();
	   if (pTree)
	   {
	   pTree->clear();
	   Copy(m_modified.invisibleRootItem(), pTree->invisibleRootItem());
	   }*/
}
/*
   void CUndoItemMove::Copy(QStandardItem* pSource, QStandardItem* pDest)
   {
   if (pSource && pDest)
   {
    for (int i = 0; i < pSource->rowCount(); ++i)
    {
      QStandardItem* pSourceItem = pSource->child(i);
      QStandardItem* pDestItem = pSourceItem->clone();
      Copy(pSourceItem, pDestItem);
      pDest->appendRow(pDestItem);
    }
   }
   }*/

CUndoItemMove::CUndoItemMove() : bModifiedInitialised(false)
{
	/*QATLTreeModel* pTree = CAudioControlsEditorPlugin::GetControlsTree();
	   if (pTree)
	   {
	   Copy(pTree->invisibleRootItem(), m_original.invisibleRootItem());
	   }*/
}
}
