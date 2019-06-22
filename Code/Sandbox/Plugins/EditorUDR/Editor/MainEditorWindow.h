// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

#include <QtViewPane.h>
#include <QLabel>
#include <QAbstractItemModel>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <qheaderview.h>

#include <IEditor.h>
#include <DockedWidget.h>
#include <Controls/EditorDialog.h>
#include <IPostRenderer.h>

#include <CryUDR/InterfaceIncludes.h>
#include <Serialization/QPropertyTreeLegacy/QPropertyTreeLegacy.h>
#include <CryGame/IGameFramework.h>


#define UDR_EDITOR_NAME "UDR Visualizer (Experimental)"


class CUDRTreeModel;
class CUDRTreeView;


//===================================================================================
//
// CMainEditorWindow
//
//===================================================================================

class CMainEditorWindow : public CDockableWindow
{
	Q_OBJECT

private:

	// one instance of this class exists per UDR::ETreeIndex
	class CTreeListener : public Cry::UDR::ITreeListener
	{
	public:

		CTreeListener(CMainEditorWindow& owningWindow, Cry::UDR::ITreeManager::ETreeIndex treeIndex);
		~CTreeListener();

		// UDR::ITreeListener
		virtual void OnBeforeRootNodeDeserialized() override;
		virtual void OnNodeAdded(const Cry::UDR::INode& freshlyAddedNode) override;
		virtual void OnBeforeNodeRemoved(const Cry::UDR::INode& nodeBeingRemoved) override;
		virtual void OnAfterNodeRemoved(const void* pFormerAddressOfNode) override;
		// ~UDR::ITreeListener

	private:

		Cry::UDR::ITreeManager::ETreeIndex m_treeIndex;
		CMainEditorWindow& m_owningWindow;
	};

public:

	CMainEditorWindow();

	// CDockableWindow
	virtual const char*                       GetPaneTitle() const override;
	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override { return IViewPaneClass::DOCK_FLOAT; }
	// ~CDockableWindow

protected:
	void customEvent(QEvent* event) override;

private:

	void SetActiveTree(Cry::UDR::ITreeManager::ETreeIndex treeIndex);

	// callbacks from member variables
	void OnTreeIndexComboBoxSelectionChanged(int index);
	void OnClearTreeButtonClicked(bool checked);
	void OnSaveLiveTreeToFile();
	void OnLoadTreeFromFile();

private:
	string                           m_windowTitle;
	CUDRTreeView*                    m_pTreeView;
	CUDRTreeModel*                   m_pTreeModel;
	QComboBox*                       m_pComboBoxTreeToShow;
	QPushButton*                     m_pButtonClearCurrentTree;
	QTextEdit*                       m_pTextLogMessages;
	CTreeListener                    m_treeListeners[static_cast<size_t>(Cry::UDR::ITreeManager::ETreeIndex::Count)];	// one listener per ETreeIndex
};
