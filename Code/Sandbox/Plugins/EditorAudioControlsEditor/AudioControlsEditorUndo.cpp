// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioControlsEditorUndo.h"
#include "SystemAssetsManager.h"
#include "AudioControlsEditorPlugin.h"
#include "QtUtil.h"

#include <QList>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
void IUndoControlOperation::AddStoredControl()
{
}

//////////////////////////////////////////////////////////////////////////
void IUndoControlOperation::RemoveStoredControl()
{
}

//////////////////////////////////////////////////////////////////////////
CUndoControlAdd::CUndoControlAdd(CID id)
{
	m_id = id;
}

//////////////////////////////////////////////////////////////////////////
void CUndoControlAdd::Undo(bool isUndo)
{
	RemoveStoredControl();
}

//////////////////////////////////////////////////////////////////////////
void CUndoControlAdd::Redo()
{
	AddStoredControl();
}

//////////////////////////////////////////////////////////////////////////
CUndoControlRemove::CUndoControlRemove(std::shared_ptr<CSystemControl>& pControl)
{
}

//////////////////////////////////////////////////////////////////////////
void CUndoControlRemove::Undo(bool isUndo)
{
	AddStoredControl();
}

//////////////////////////////////////////////////////////////////////////
void CUndoControlRemove::Redo()
{
	RemoveStoredControl();
}

//////////////////////////////////////////////////////////////////////////
void IUndoFolderOperation::AddFolderItem()
{
}

//////////////////////////////////////////////////////////////////////////
void IUndoFolderOperation::RemoveItem()
{
}

//////////////////////////////////////////////////////////////////////////
void CUndoFolderRemove::Undo(bool isUndo)
{
	AddFolderItem();
}

void CUndoFolderRemove::Redo()
{
	RemoveItem();
}

//////////////////////////////////////////////////////////////////////////
void CUndoFolderAdd::Undo(bool isUndo)
{
	RemoveItem();
}

//////////////////////////////////////////////////////////////////////////
void CUndoFolderAdd::Redo()
{
	AddFolderItem();
}

//////////////////////////////////////////////////////////////////////////
CUndoControlModified::CUndoControlModified(CID id)
	: m_id(id)
	, m_isAutoLoad(false)
{
}

//////////////////////////////////////////////////////////////////////////
void CUndoControlModified::Undo(bool isUndo)
{
	SwapData();
}

//////////////////////////////////////////////////////////////////////////
void CUndoControlModified::Redo()
{
	SwapData();
}

//////////////////////////////////////////////////////////////////////////
void CUndoControlModified::SwapData()
{
	const CScopedSuspendUndo suspendUndo;
	CSystemAssetsManager* pAssetsManager = CAudioControlsEditorPlugin::GetAssetsManager();
	if (pAssetsManager)
	{
		CSystemControl* pControl = pAssetsManager->GetControlByID(m_id);
		if (pControl)
		{
			string name = pControl->GetName();
			Scope scope = pControl->GetScope();
			bool isAutoLoad = pControl->IsAutoLoad();

			std::vector<ConnectionPtr> connectedControls;
			const size_t size = pControl->GetConnectionCount();
			for (size_t i = 0; i < size; ++i)
			{
				connectedControls.emplace_back(pControl->GetConnectionAt(i));
			}
			pControl->ClearConnections();

			pControl->SetName(m_name);
			pControl->SetScope(m_scope);
			pControl->SetAutoLoad(m_isAutoLoad);

			for (ConnectionPtr& c : m_connectedControls)
			{
				pControl->AddConnection(c);
			}

			m_name = name;
			m_scope = scope;
			m_isAutoLoad = isAutoLoad;
			m_connectedControls = connectedControls;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CUndoItemMove::Undo(bool isUndo)
{
}

void CUndoItemMove::Redo()
{
}

//////////////////////////////////////////////////////////////////////////
CUndoItemMove::CUndoItemMove() : m_isModifiedInitialised(false)
{
}
} // namespace ACE
