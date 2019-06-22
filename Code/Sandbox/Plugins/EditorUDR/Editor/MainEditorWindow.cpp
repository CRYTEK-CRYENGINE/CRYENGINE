// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MainEditorWindow.h"

#include <QDir>
#include <QMenuBar>
#include <QFileDialog>
#include <QDockWidget>
#include <QTabWidget>
#include <QLabel>
#include <QLayout>
#include <QVBoxLayout>
#include <QAbstractButton>
#include <QDialogButtonbox>
#include <QLineEdit>
#include <QAdvancedTreeView.h>
#include <QSplitter>
#include <QMessageBox>
#include <QFontMetrics>
#include <QKeyEvent>

#include <QtUtil.h>

#include <Serialization/QPropertyTreeLegacy/QPropertyTreeLegacy.h>
#include <Controls/QuestionDialog.h>
#include <EditorFramework/Events.h>

//===================================================================================
//
// Columns in the treeview
//
//===================================================================================

enum
{
	COLUMN_NODE_NAME,
	COLUMN_NO_OF_CHILDREN,
	NUM_COLUMNS
};

//===================================================================================
//
// CUDRTreeModel
//
//===================================================================================

class CUDRTreeModel: public QAbstractItemModel
{
public:
	explicit CUDRTreeModel(QObject* pParent)
		: QAbstractItemModel(pParent)
	{
	}

	Q_INVOKABLE virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override
	{
		assert(row >= 0 && column >= 0);

		if (parent.isValid())
		{
			const Cry::UDR::INode* pParentNode = static_cast < const Cry::UDR::INode * > (parent.internalPointer());

			if (row < (int)pParentNode->GetChildCount())
			{
				const Cry::UDR::INode* pChildNode = &pParentNode->GetChild(row);
				return createIndex(row, column, reinterpret_cast<quintptr>(pChildNode));
			}
			else
			{
				return QModelIndex();
			}
		}
		else
		{
			if (m_pRoot)
				return createIndex(0, column, reinterpret_cast<quintptr>(m_pRoot));
			else
				return QModelIndex(); // never been filled with nodes so far
		}
	}

	Q_INVOKABLE virtual QModelIndex parent(const QModelIndex& child) const override
	{
		if (!child.isValid())
		{
			return QModelIndex();
		}

		const Cry::UDR::INode* pChildNode = static_cast < const Cry::UDR::INode * > (child.internalPointer());
		const Cry::UDR::INode* pParentNode = pChildNode->GetParent();

		if (!pParentNode)
		{
			return QModelIndex();
		}

		for (size_t i = 0, n = pParentNode->GetChildCount(); i < n; ++i)
		{
			if (&pParentNode->GetChild(i) == pChildNode)
			{
				return createIndex((int)i, 0, reinterpret_cast<quintptr>(pParentNode));
			}
		}

		assert(0);
		return QModelIndex();
	}

	Q_INVOKABLE virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override
	{
		if (parent.column() > 0)
		{
			return 0;
		}

		if (parent.isValid())
		{
			const Cry::UDR::INode* pParentQuery = static_cast < const Cry::UDR::INode * > (parent.internalPointer());
			return (int)pParentQuery->GetChildCount();
		}
		else
		{
			if (m_pRoot)
				return 1;
			else
				return 0; // never been filled with nodes so far
		}
	}

	Q_INVOKABLE virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override
	{
		return NUM_COLUMNS;
	}

	Q_INVOKABLE virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
	{
		if (role == Qt::DisplayRole)
		{
			if (index.isValid())
			{
				assert(index.column() < NUM_COLUMNS);

				if (index.column() == COLUMN_NODE_NAME)
				{
					const Cry::UDR::INode* pNode = static_cast < const Cry::UDR::INode * > (index.internalPointer());
					return pNode->GetName();
				}

				if (index.column() == COLUMN_NO_OF_CHILDREN)
				{
					const Cry::UDR::INode* pNode = static_cast < const Cry::UDR::INode * > (index.internalPointer());
					return (int)pNode->GetChildCount();
				}
			}
		}
		return QVariant();
	}

	Q_INVOKABLE virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
	{
		if (role == Qt::DisplayRole)
		{
			assert(section < NUM_COLUMNS);

			if (section == COLUMN_NODE_NAME)
			{
				return "Node name";
			}

			if (section == COLUMN_NO_OF_CHILDREN)
			{
				return "No of Children";
			}
		}

		return QAbstractItemModel::headerData(section, orientation, role);
	}

public:

	void RefreshFromUDRTree(Cry::UDR::ITreeManager::ETreeIndex treeIndex)
	{
		m_pRoot = &gEnv->pUDR->GetTreeManager().GetTree(treeIndex).GetRootNode();
		beginResetModel();
		endResetModel();
	}

	QModelIndex GetRootIndex() const
	{
		if (m_pRoot)
		{
			return createIndex(0, 0, reinterpret_cast<quintptr>(m_pRoot));
		}
		else
		{
			return QModelIndex();
		}
	}

	size_t GetChildCountUnderRoot() const
	{
		assert(m_pRoot);
		return m_pRoot->GetChildCount();
	}

private:

	const Cry::UDR::INode* m_pRoot = nullptr;
};

//===================================================================================
//
// CUDRTreeView
//
//===================================================================================

class CUDRTreeView: public QAdvancedTreeView, public IEditorNotifyListener, public IGameFrameworkListener
{
private:

	// builds html text from the log messages that reside in UDR nodes
	class CLogMessageTextBuilder : public Cry::UDR::INode::ILogMessageVisitor
	{
	public:
		void AccumulateText(const Cry::UDR::INode& node)
		{
			node.VisitLogMessages(*this, true);
		}

		void EmitToTextBox(QTextEdit& textBox) const
		{
			textBox.clear();
			textBox.insertHtml("<pre>");
			for (const string& html : m_htmlLines)
			{
				textBox.insertHtml(QtUtil::ToQString(html));
			}
			textBox.insertHtml("</pre>");
		}

	private:
		// Cry::UDR::INode::ILogMessageVisitor
		virtual void OnLogMessageVisited(const char* szLogMessage, Cry::UDR::ELogMessageType messageType) override
		{
			string html;
			html.Format("<font color='%s'>%s</font><br>", (messageType == Cry::UDR::ELogMessageType::Warning) ? "yellow" : "white", szLogMessage);
			m_htmlLines.emplace_back(std::move(html));
		}
		// ~Cry::UDR::INode::ILogMessageVisitor

	private:
		std::vector<string> m_htmlLines;
	};

public:

	explicit CUDRTreeView(QWidget* pParent, const QComboBox* pComboBoxWithSelectedTree, QTextEdit* pTextBoxOfMainWindow)
		: QAdvancedTreeView(QAdvancedTreeView::Behavior(QAdvancedTreeView::None), pParent)
		, m_pComboBoxWithSelectedTree(pComboBoxWithSelectedTree)
		, m_pTexBoxOfMainWindow(pTextBoxOfMainWindow)
	{
		setSelectionMode(QAbstractItemView::ExtendedSelection);

		// draw the selected nodes while in edit-mode
		GetIEditor()->RegisterNotifyListener(this);

		// draw the selected nodes while in game-mode
		gEnv->pGameFramework->RegisterListener(this, "UDRRecordings", FRAMEWORKLISTENERPRIORITY_DEFAULT);
	}

	~CUDRTreeView()
	{
		GetIEditor()->UnregisterNotifyListener(this);
		gEnv->pGameFramework->UnregisterListener(this);
	}

	// Hack: I tried connecting to the selectionChanged signal from inside CMainEditorWindow like this:
	//       QObject::connect(m_pTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CMainEditorWindow::OnTreeViewSelectionChanged);
	//       But this signal just won't fire, so we override selectionChanged() and fill the CMainEditorWindow's text box directly here.
	virtual void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override
	{
		QAdvancedTreeView::selectionChanged(selected, deselected);

		CLogMessageTextBuilder textBuilder;
		std::set<const Cry::UDR::INode*> alreadyVisitedNodes;	// the selected indexes in the treeview all seem to appear twice (bug in QTreeView?)

		QModelIndexList indexList = selectedIndexes();
		std::sort(indexList.begin(), indexList.end());	// sort (by row), so that the messages appear in the same order as the nodes appear in the tree

		for (size_t i = 0, n = indexList.size(); i < n; i++)
		{
			const QModelIndex& index = indexList.at(i);
			const Cry::UDR::INode* pNode = static_cast<const Cry::UDR::INode*>(index.internalPointer());

			// not yet present? (prevent duplicates)
			if (alreadyVisitedNodes.insert(pNode).second)
			{
				textBuilder.AccumulateText(*pNode);
			}
		}

		textBuilder.EmitToTextBox(*m_pTexBoxOfMainWindow);
	}

protected:

	// QTreeView
	virtual void keyPressEvent(QKeyEvent *event) override
	{
		if (event->key() == Qt::Key_Delete && event->type() == QEvent::KeyPress)
		{
			// delete all currently selected nodes
			QModelIndexList indexList = selectedIndexes();
			if (!indexList.empty())
			{
				std::set<const Cry::UDR::INode*> selectedNodes;	// using a set<>, since the indexList seems to hold duplicated entries (Qt bug?)
				for (int i = 0; i < indexList.size(); i++)
				{
					selectedNodes.insert(static_cast<const Cry::UDR::INode*>(indexList.at(i).internalPointer()));
				}

				if (QMessageBox::warning(this, "Delete nodes", stack_string().Format("Delete %i nodes?", (int)selectedNodes.size()).c_str(), QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::Cancel)) == QMessageBox::Yes)
				{
					for (const Cry::UDR::INode* pNodeToDelete : selectedNodes)
					{
						gEnv->pUDR->GetTreeManager().GetTree((Cry::UDR::ITreeManager::ETreeIndex)m_pComboBoxWithSelectedTree->currentIndex()).RemoveNode(*pNodeToDelete);
					}
				}
				return;
			}
		}
		QTreeView::keyPressEvent(event);
	}
	// ~QTreeView

private:

	// IEditorNotifyListener
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) override
	{
		if (event == eNotify_OnIdleUpdate)
		{
			DrawSelectedNodes();
		}
	}
	// ~IEditorNotifyListener

	// IGameFrameworkListener
	virtual void OnPostUpdate(float fDeltaTime) override           {}
	virtual void OnSaveGame(ISaveGame* pSaveGame) override         {}
	virtual void OnLoadGame(ILoadGame* pLoadGame) override         {}
	virtual void OnLevelEnd(const char* nextLevel) override        {}
	virtual void OnActionEvent(const SActionEvent& event) override {}
	virtual void OnPreRender() override
	{
		DrawSelectedNodes();
	}
	// ~IGameFrameworkListener

	void DrawSelectedNodes() const
	{
		QModelIndexList indexList = selectedIndexes();

		for (size_t i = 0, n = indexList.size(); i < n; i++)
		{
			const QModelIndex& index = indexList.at(i);
			const Cry::UDR::INode* pNode = static_cast < const Cry::UDR::INode * > (index.internalPointer());
			pNode->DrawRenderPrimitives(true);
		}
	}

private:

	const QComboBox* m_pComboBoxWithSelectedTree;	// for figuring out which tree (live/deserialized) is currently selected
	QTextEdit* m_pTexBoxOfMainWindow;

};

//////////////////////////////////////////////////////////////////////////
// CMainEditorWindow
//////////////////////////////////////////////////////////////////////////

CMainEditorWindow::CTreeListener::CTreeListener(CMainEditorWindow& owningWindow, Cry::UDR::ITreeManager::ETreeIndex treeIndex)
	: m_owningWindow(owningWindow)
	, m_treeIndex(treeIndex)
{
	gEnv->pUDR->GetTreeManager().GetTree(m_treeIndex).RegisterListener(this);
}

CMainEditorWindow::CTreeListener::~CTreeListener()
{
	gEnv->pUDR->GetTreeManager().GetTree(m_treeIndex).UnregisterListener(this);
}

void CMainEditorWindow::CTreeListener::OnBeforeRootNodeDeserialized()
{
	if (m_treeIndex != (Cry::UDR::ITreeManager::ETreeIndex)m_owningWindow.m_pComboBoxTreeToShow->currentIndex())
		return;
	m_owningWindow.m_pTreeView->setModel(nullptr);  // prevent a crash when exchanging the model in the core (i.e. dangling pointer)
}

void CMainEditorWindow::CTreeListener::OnNodeAdded(const Cry::UDR::INode& freshlyAddedNode)
{
	if (m_treeIndex != (Cry::UDR::ITreeManager::ETreeIndex)m_owningWindow.m_pComboBoxTreeToShow->currentIndex())
		return;

	// refresh only if the live tree is currently selected (that one may add new nodes at runtime, so we wanna draw from the moment they are added)
	if (m_treeIndex == Cry::UDR::ITreeManager::ETreeIndex::Live)
	{
		m_owningWindow.m_pTreeModel->RefreshFromUDRTree(m_treeIndex);

		// if this was the very first node to be added, then expand the root node
		// (this has already been done, but actually did not have any effect, as there were no sub-nodes yet)
		if (m_owningWindow.m_pTreeModel->GetChildCountUnderRoot() == 1)
		{
			m_owningWindow.m_pTreeView->expand(m_owningWindow.m_pTreeModel->GetRootIndex());
		}
	}
}

void CMainEditorWindow::CTreeListener::OnBeforeNodeRemoved(const Cry::UDR::INode& nodeBeingRemoved)
{
	// nothing
}

void CMainEditorWindow::CTreeListener::OnAfterNodeRemoved(const void* pFormerAddressOfNode)
{
	if (m_treeIndex != (Cry::UDR::ITreeManager::ETreeIndex)m_owningWindow.m_pComboBoxTreeToShow->currentIndex())
		return;
	m_owningWindow.m_pTreeModel->RefreshFromUDRTree(m_treeIndex);
}

CMainEditorWindow::CMainEditorWindow()
	: m_windowTitle(UDR_EDITOR_NAME)
	, m_treeListeners{{ *this, Cry::UDR::ITreeManager::ETreeIndex::Live }, {
		*this, Cry::UDR::ITreeManager::ETreeIndex::Deserialized
	}}
{
	// "file" menu
	{
		QMenu* pFileMenu = menuBar()->addMenu("&File");

		QAction* pLoadRecording = pFileMenu->addAction("&Load...");
		connect(pLoadRecording, &QAction::triggered, this, &CMainEditorWindow::OnLoadTreeFromFile);

		QAction* pSaveLiveRecording = pFileMenu->addAction("&Save Live Tree");
		connect(pSaveLiveRecording, &QAction::triggered, this, &CMainEditorWindow::OnSaveLiveTreeToFile);
	}

	m_pComboBoxTreeToShow = new QComboBox(this);
	m_pComboBoxTreeToShow->addItem("Live Tree", QVariant(static_cast<int>(Cry::UDR::ITreeManager::ETreeIndex::Live)));
	m_pComboBoxTreeToShow->addItem("Deserialized Tree", QVariant(static_cast<int>(Cry::UDR::ITreeManager::ETreeIndex::Deserialized)));

	m_pButtonClearCurrentTree = new QPushButton(this);
	m_pButtonClearCurrentTree->setText("Clear Tree");

	m_pTextLogMessages = new QTextEdit(this);
	m_pTextLogMessages->setReadOnly(true);

	m_pTreeView = new                 CUDRTreeView(this, m_pComboBoxTreeToShow, m_pTextLogMessages);
	m_pTreeModel = new                CUDRTreeModel(this);

#if 0
	QSplitter* pDetailsSplitter = new QSplitter(this);
	pDetailsSplitter->setOrientation(Qt::Vertical);

	QHBoxLayout* pHBoxLayout = new QHBoxLayout(this);
	pHBoxLayout->addWidget(m_pComboBoxTreeToShow);
	pHBoxLayout->addWidget(m_pButtonClearCurrentTree);

	QWidget* pHorzContainer = new QWidget(this);
	pHorzContainer->setLayout(pHBoxLayout);

	QVBoxLayout* pVBoxLayout = new QVBoxLayout(this);
	pVBoxLayout->addWidget(pHorzContainer);
	pVBoxLayout->addWidget(m_pTreeView);

	QWidget* pLeftContainer = new QWidget(this);
	pLeftContainer->setLayout(pVBoxLayout);

	QSplitter* pMainSplitter = new QSplitter(this);
	pMainSplitter->addWidget(pLeftContainer);
	pMainSplitter->addWidget(pDetailsSplitter);

	setCentralWidget(pMainSplitter);
#else
	QSplitter* pMainSplitter = new QSplitter(this);
	{
		QWidget* pLeftContainer = new QWidget(this);
		{
			QVBoxLayout* pVBoxLayout = new QVBoxLayout(this);
			{
				QWidget* pHorzContainer1 = new QWidget(this);
				{
					QHBoxLayout* pHBoxLayout1 = new QHBoxLayout(this);
					pHBoxLayout1->addWidget(m_pComboBoxTreeToShow);
					pHBoxLayout1->addWidget(m_pButtonClearCurrentTree);
					pHorzContainer1->setLayout(pHBoxLayout1);
				}

				QWidget* pHorzContainer2 = new QWidget(this);
				{
					QHBoxLayout* pHBoxLayout2 = new QHBoxLayout(this);
					pHBoxLayout2->addWidget(m_pTreeView);
					pHorzContainer2->setLayout(pHBoxLayout2);
				}

				pVBoxLayout->addWidget(pHorzContainer1);
				pVBoxLayout->addWidget(pHorzContainer2);
			}

			pLeftContainer->setLayout(pVBoxLayout);
		}

		QWidget* pRightContainer = new QWidget(this);
		{
			QVBoxLayout* pVBoxLayout = new QVBoxLayout(this);
			pVBoxLayout->addWidget(m_pTextLogMessages);
			pRightContainer->setLayout(pVBoxLayout);
		}

		pMainSplitter->addWidget(pLeftContainer);
		pMainSplitter->addWidget(pRightContainer);
	}
	setCentralWidget(pMainSplitter);
#endif

	QObject::connect(m_pComboBoxTreeToShow, static_cast < void (QComboBox::*)(int) > (&QComboBox::currentIndexChanged), this, &CMainEditorWindow::OnTreeIndexComboBoxSelectionChanged); // the static_cast<> is only for disambiguation of the overloaded currentIndexChanged() method
	QObject::connect(m_pButtonClearCurrentTree, &QPushButton::clicked, this, &CMainEditorWindow::OnClearTreeButtonClicked);

	// start with the "live" tree
	SetActiveTree(Cry::UDR::ITreeManager::ETreeIndex::Live);

	m_pTextLogMessages->setText("(Log messages of selected nodes will appear here)");
}

const char* CMainEditorWindow::GetPaneTitle() const
{
	return m_windowTitle.c_str();
}

void CMainEditorWindow::customEvent(QEvent* event)
{
	// TODO: This handler should be removed whenever this editor is refactored to be a CDockableEditor
	if (event->type() == SandboxEvent::Command)
	{
		CommandEvent* commandEvent = static_cast<CommandEvent*>(event);

		const string& command = commandEvent->GetCommand();
		if (command == "general.help")
		{
			event->setAccepted(EditorUtils::OpenHelpPage(GetPaneTitle()));
		}
	}

	if (!event->isAccepted())
	{
		QWidget::customEvent(event);
	}
}

void CMainEditorWindow::SetActiveTree(Cry::UDR::ITreeManager::ETreeIndex treeIndex)
{
	m_pTreeModel->RefreshFromUDRTree(treeIndex);

	m_pTreeView->setModel(m_pTreeModel);
	m_pTreeView->header()->setSectionResizeMode(COLUMN_NODE_NAME, QHeaderView::ResizeToContents);
	m_pTreeView->setExpanded(m_pTreeModel->GetRootIndex(), true);
	m_pTreeView->setCurrentIndex(QModelIndex());

	m_pComboBoxTreeToShow->setCurrentIndex((int)treeIndex);
}

void CMainEditorWindow::OnTreeIndexComboBoxSelectionChanged(int index)
{
	const QVariant data = m_pComboBoxTreeToShow->itemData(index);
	assert(data.type() == QVariant::Int);
	const Cry::UDR::ITreeManager::ETreeIndex treeIndex = (Cry::UDR::ITreeManager::ETreeIndex)data.toInt();
	SetActiveTree(treeIndex);
}

void CMainEditorWindow::OnClearTreeButtonClicked(bool checked)
{
	const Cry::UDR::ITreeManager::ETreeIndex treeIndex = (Cry::UDR::ITreeManager::ETreeIndex)m_pComboBoxTreeToShow->currentIndex();
	Cry::UDR::ITree& tree = gEnv->pUDR->GetTreeManager().GetTree(treeIndex);
	tree.RemoveNode(tree.GetRootNode());	// notice: this will not actually remove the root node (only its children), which is a special case - see implementation
}

void CMainEditorWindow::OnSaveLiveTreeToFile()
{
	QString qFilePath = QFileDialog::getSaveFileName(this, "Save File", "udr_recording.xml", "XML file (*.xml)");
	string sFilePath = QtUtil::ToString(qFilePath);
	if (!sFilePath.empty())  // would be empty if pressing the "cancel" button
	{
		Cry::UDR::CString error;
		if (gEnv->pUDR->GetTreeManager().SerializeLiveTree(sFilePath.c_str(), error))
		{
			// change the window title to also show the file name
			m_windowTitle.Format("%s - %s", UDR_EDITOR_NAME, sFilePath.c_str());
			setWindowTitle(QtUtil::ToQString(m_windowTitle));
		}
		else
		{
			// show the error message
			stack_string error;
			error.Format("Serializing the recording to '%s' failed: %s", sFilePath.c_str(), error.c_str());
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "%s: %s", UDR_EDITOR_NAME, error.c_str());
			QMessageBox::warning(this, "Error saving the recording", error.c_str());
		}
	}
}

void CMainEditorWindow::OnLoadTreeFromFile()
{
	QString qFilePath = QFileDialog::getOpenFileName(this, "Load File", "*.xml", "XML file (*.xml)");
	if (!qFilePath.isEmpty())
	{
		string sFilePath = QtUtil::ToString(qFilePath);
		Cry::UDR::CString error;
		if (gEnv->pUDR->GetTreeManager().DeserializeTree(sFilePath.c_str(), error))
		{
			SetActiveTree(Cry::UDR::ITreeManager::ETreeIndex::Deserialized);

			// change the window title to also show the file name
			m_windowTitle.Format("%s - %s", UDR_EDITOR_NAME, sFilePath.c_str());
			setWindowTitle(QtUtil::ToQString(m_windowTitle));
		}
		else
		{
			// show the error message
			stack_string error;
			error.Format("Deserializing the recording from '%s' failed: %s", sFilePath.c_str(), error.c_str());
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "%s: %s", UDR_EDITOR_NAME, error.c_str());
			QMessageBox::warning(this, "Error loading the recording", error.c_str());
		}
	}
}
