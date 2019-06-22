// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include <CryCore/Platform/platform_impl.inl>
#include "CryTestRunnerPlugin.h"
#include "CryTestRunnerSystem.h"
#include <Menu/MenuWidgetBuilders.h>
#include <EditorFramework/Editor.h>
#include <ProxyModels/DeepFilterProxyModel.h>
#include <QAdvancedTreeView.h>

#include <QHeaderView>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

//! Abstract item model for CryTest treeview
class CCryTestRunnerTreeViewModel : public QAbstractItemModel
{
public:

	CCryTestRunnerTreeViewModel(QObject* pParent, CCryTestRunnerSystem& system)
		: QAbstractItemModel(pParent)
		, m_system(system)
	{
	}

private:

	virtual int      columnCount(const QModelIndex& parent = QModelIndex()) const override { return 2; }
	virtual QVariant data(const QModelIndex& index, int role) const override
	{
		if (!index.isValid())
		{
			return QVariant();
		}

		switch (role)
		{
		case Qt::DisplayRole:
		{
			ICryTestNode* pNode = reinterpret_cast<ICryTestNode*>(index.internalPointer());
			if (pNode)
			{
				if (index.column() == 0)
				{
					return pNode->GetDisplayName();
				}
				else if (index.column() == 1)
				{
					return pNode->GetTestSummary();
				}
			}
			break;
		}
		}
		return QVariant();
	}
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override
	{
		if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
		{
			if (section == 0)
			{
				return "Tests";
			}
			else if (section == 1)
			{
				return "Results";
			}
		}

		return QVariant();
	}
	virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override
	{
		if (!parent.isValid())
		{
			return createIndex(row, column, static_cast<ICryTestNode*>(m_system.GetTestModules()[row]));
		}
		else if (!parent.parent().isValid())
		{
			return createIndex(row, column, static_cast<ICryTestNode*>(&m_system.GetTestModules()[parent.row()]->GetTestEntries()[row]));
		}
		else
		{
			return QModelIndex();
		}
	}
	virtual QModelIndex parent(const QModelIndex& index) const override
	{
		ICryTestNode* pNode = reinterpret_cast<ICryTestNode*>(index.internalPointer());
		if (pNode)
		{
			ICryTestNode* pParentNode = pNode->GetParent();
			if (pParentNode)
			{
				return createIndex(pParentNode->GetIndex(), 0, pParentNode);
			}
		}

		return QModelIndex();
	}
	virtual int rowCount(const QModelIndex& parent) const override
	{
		if (parent.isValid())
		{
			ICryTestNode* pNode = reinterpret_cast<ICryTestNode*>(parent.internalPointer());
			if (CRY_VERIFY(pNode))
			{
				return pNode->GetChildrenCount();
			}
		}
		else
		{
			return m_system.GetTestModules().size();
		}
		return 0;
	}

	CCryTestRunnerSystem& m_system;
};

//! Widget containing a treeview of CryTest and buttons
class CCryTestBrowserWidget : public CDockableWidget
{
public:

	CCryTestBrowserWidget(CCryTestRunnerSystem& system)
		: CDockableWidget()
		, m_system(system)
		, m_pModel(new CCryTestRunnerTreeViewModel(this, system))
	{
		m_system.SignalTestsStarted.Connect(this, &CCryTestBrowserWidget::OnTestsStarted);
		m_system.SignalTestsFinished.Connect(this, &CCryTestBrowserWidget::OnTestsFinished);

		const QDeepFilterProxyModel::BehaviorFlags behavior = QDeepFilterProxyModel::AcceptIfChildMatches | QDeepFilterProxyModel::AcceptIfParentMatches;
		m_pFilterProxy = new QDeepFilterProxyModel(behavior);
		m_pFilterProxy->setSourceModel(m_pModel);
		m_pFilterProxy->setFilterKeyColumn(-1);
		m_pFilterProxy->setFilterRole(Qt::DisplayRole);
		m_pFilterProxy->setSortRole(Qt::DisplayRole);

		const auto pSearchBox = new QSearchBox();
		pSearchBox->SetModel(m_pFilterProxy);
		pSearchBox->EnableContinuousSearch(true);

		pRunAllBtn = new QToolButton();
		pRunAllBtn->setIcon(CryIcon("icons:Graph/Node_connection_arrow_R_blue.ico"));
		pRunAllBtn->setText("Run All");
		pRunAllBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

		m_pTreeView = new QAdvancedTreeView();
		m_pTreeView->setModel(m_pFilterProxy);
		m_pTreeView->setSortingEnabled(true);
		m_pTreeView->setUniformRowHeights(true);
		m_pTreeView->setAllColumnsShowFocus(true);
		m_pTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
		m_pTreeView->header()->setStretchLastSection(true);
		m_pTreeView->header()->setDefaultSectionSize(300);

		m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(m_pTreeView, &QTreeView::customContextMenuRequested, [this] { OnContextMenu(); });
		connect(m_pTreeView->selectionModel(), &QItemSelectionModel::selectionChanged,
			[this](const QItemSelection& selected, const QItemSelection& deselected)
		{
			const auto& selectedIndexes = selected.indexes();
			if (!selectedIndexes.empty())
			{
				QModelIndex index = selectedIndexes[0];
				QModelIndex srcIdx = m_pFilterProxy->mapToSource(index);
				ICryTestNode* pNode = reinterpret_cast<ICryTestNode*>(srcIdx.internalPointer());
				if (pNode)
				{
					m_system.SignalNodeSelected(pNode);
				}
			}
		});

		pSearchBox->SetAutoExpandOnSearch(m_pTreeView);

		const auto pLayout = new QVBoxLayout();
		pLayout->setContentsMargins(1, 1, 1, 1);
		pLayout->addWidget(pRunAllBtn, Qt::AlignTop);
		pLayout->addWidget(pSearchBox);
		pLayout->addWidget(m_pTreeView, Qt::AlignBottom);
		setLayout(pLayout);

		connect(pRunAllBtn, &QToolButton::clicked, [this]
		{
			m_system.RunAll();
		});
	}

	~CCryTestBrowserWidget()
	{
		m_system.SignalTestsStarted.DisconnectObject(this);
		m_system.SignalTestsFinished.DisconnectObject(this);
	}

	void OnTestsStarted()
	{
		pRunAllBtn->setEnabled(false);
	}

	void OnTestsFinished()
	{
		pRunAllBtn->setEnabled(true);
	}

	void OnContextMenu()
	{
		CAbstractMenu abstractMenu;

		auto action = abstractMenu.CreateAction(CryIcon("icons:Graph/Node_connection_arrow_R_blue.ico"), tr("Run selected tests"));
		connect(action, &QAction::triggered, [this]
		{
			QModelIndexList const& selectedIndexes = m_pTreeView->selectionModel()->selectedRows();
			if (selectedIndexes.size())
			{
				QModelIndex index = selectedIndexes[0];
				QModelIndex srcIdx = m_pFilterProxy->mapToSource(index);
				ICryTestNode* pNode = reinterpret_cast<ICryTestNode*>(srcIdx.internalPointer());
				if (CRY_VERIFY(pNode))
				{
					pNode->Run();
				}
			}
		});

		QMenu menu;
		abstractMenu.Build(MenuWidgetBuilders::CMenuBuilder(&menu));

		if (menu.actions().count() > 0)
		{
			menu.exec(QCursor::pos());
		}
	}

	virtual const char* GetPaneTitle() const override { return "Tests"; }

private:
	friend class CCryTestRunnerWindow;

	CCryTestRunnerSystem&        m_system;
	CCryTestRunnerTreeViewModel* m_pModel;
	QDeepFilterProxyModel*       m_pFilterProxy;
	QAdvancedTreeView*           m_pTreeView;
	QToolButton*                 pRunAllBtn;
};

//! Widget containing the output text field for CryTest
class CCryTestOutputWidget : public CDockableWidget
{
public:
	CCryTestOutputWidget(CCryTestRunnerSystem& system, QWidget* const pParent = nullptr)
		: CDockableWidget(pParent)
		, m_system(system)
	{
		m_pTextEdit = new QTextEdit();
		m_pTextEdit->setReadOnly(true);
		m_pTextEdit->setUndoRedoEnabled(false);

		const auto pLayout = new QVBoxLayout();
		pLayout->setContentsMargins(1, 1, 1, 1);
		pLayout->addWidget(m_pTextEdit);

		setLayout(pLayout);

		m_system.SignalNodeSelected.Connect(this, &CCryTestOutputWidget::OnSelected);
		m_system.SignalTestsFinished.Connect(this, &CCryTestOutputWidget::OnTestsFinished);
	}

	~CCryTestOutputWidget()
	{
		m_system.SignalNodeSelected.DisconnectObject(this);
	}

	virtual const char* GetPaneTitle() const override { return "Output"; }

	void                SetText(const QString& content)
	{
		m_pTextEdit->setText(content);
	}

	void UpdateOutput()
	{
		if (m_pCurrentNode)
		{
			SetText(m_pCurrentNode->GetOutput());
		}
	}

	void OnSelected(ICryTestNode* pNode)
	{
		m_pCurrentNode = pNode;
		UpdateOutput();
	}

	void OnTestsFinished()
	{
		UpdateOutput();
	}

private:
	CCryTestRunnerSystem& m_system;
	QTextEdit*            m_pTextEdit;
	ICryTestNode*         m_pCurrentNode = nullptr;
};

//! CryTest main window
class CCryTestRunnerWindow : public CDockableEditor
{
public:
	CCryTestRunnerWindow(QWidget* const pParent = nullptr)
		: CDockableEditor(pParent)
	{

		EnableDockingSystem();

		RegisterDockableWidget("Tests", [&]()
		{
			return new CCryTestBrowserWidget(g_cryTestRunnerSystem);
		}, true);

		RegisterDockableWidget("Output", [&]()
		{
			return new CCryTestOutputWidget(g_cryTestRunnerSystem);
		}, true);
	}

	virtual const char* GetEditorName() const override { return "Test Runner"; }
	virtual void        CreateDefaultLayout(CDockableContainer* sender) override
	{
		auto centerWidget = sender->SpawnWidget("Tests");
		sender->SpawnWidget("Output", centerWidget, QToolWindowAreaReference::Bottom);
	}
};

REGISTER_VIEWPANE_FACTORY_AND_MENU(CCryTestRunnerWindow, "Test Runner", "Tools", true, "Advanced");

REGISTER_PLUGIN(CCryTestRunnerPlugin);

CCryTestRunnerPlugin::CCryTestRunnerPlugin()
{
	g_cryTestRunnerSystem.Init();
}
