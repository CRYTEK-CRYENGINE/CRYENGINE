// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// TODO:
// - tweak size of the treeview captions

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

#include <QtUtil.h>

#include <Serialization/QPropertyTree/QPropertyTree.h>
#include <Controls/QuestionDialog.h>
#include <EditorFramework/Events.h>

// UQS 3d rendering
#include <IViewportManager.h>
#include <SandboxAPI.h>	// SANDBOX_API
struct IDataBaseItem;	// CViewport should have forward-declared this
#include <Viewport.h>

//#pragma optimize("", off)

//===================================================================================
//
// SQuery - the data of the model
//
//===================================================================================

struct SQuery
{
	// columns
	enum
	{
		Column_QueryIdAndQuerierName = 0,
		Column_QueryBlueprintName,
		// TODO: column for displaying the itemType of the items in the result set
		Column_ItemCounts, // number of resulting items vs. generated items
		Column_ElapsedTime,
		Column_TimestampQueryCreated,
		Column_TimestampQueryDestroyed,

		ColumnCount
	};

	static const char* headers[ColumnCount];
	static const char* toolTips[ColumnCount];

	// control data
	SQuery*              pParent;
	std::vector<SQuery*> children;
	UQS::Core::CQueryID  queryID;
	bool                 bFoundTooFewItems;
	bool                 bExceptionEncountered;
	bool                 bEncounteredSomeWarnings;
	UQS::Core::IQueryHistoryManager::SEvaluatorDrawMasks evaluatorDrawMasks;

	std::vector<string> instantEvaluatorNames;
	std::vector<string> deferredEvaluatorNames;
	std::vector<string> instantEvaluatorLabelsForUI;   // needs to store the labels additionally, since yasli won't make its own copies of the labels until the archive is finaized (seriously!)
	std::vector<string> deferredEvaluatorLabelsForUI;

	// display data
	QVariant dataPerColumn[ColumnCount];

	SQuery() : pParent(nullptr), queryID(UQS::Core::CQueryID::CreateInvalid()), evaluatorDrawMasks(UQS::Core::IQueryHistoryManager::SEvaluatorDrawMasks::CreateAllBitsSet()) {}
	SQuery(SQuery* _pParent, const UQS::Core::IQueryHistoryConsumer::SHistoricQueryOverview& overview)
		: pParent(_pParent)
		, queryID(overview.queryID)
		, bFoundTooFewItems(overview.bFoundTooFewItems)
		, bExceptionEncountered(overview.bQueryEncounteredAnException)
		, bEncounteredSomeWarnings(overview.bQueryEncounteredSomeWarnings)
		, evaluatorDrawMasks(UQS::Core::IQueryHistoryManager::SEvaluatorDrawMasks::CreateAllBitsSet())
	{
		UpdateInformation(overview);
	}

	~SQuery()
	{
		for (SQuery* pChild : children)
			delete pChild;
	}

	void UpdateInformation(const UQS::Core::IQueryHistoryConsumer::SHistoricQueryOverview& overview)
	{
		UQS::Shared::CUqsString queryIdAsString;
		stack_string queryIdAndQuerierName;
		stack_string itemCountsAsString;
		stack_string elapsedTimeAsString;
		stack_string timestampQueryCreatedAsString;
		stack_string timestampQueryDestroyedAsString;

		int hours, minutes, seconds, milliseconds;

		overview.queryID.ToString(queryIdAsString);
		queryIdAndQuerierName.Format("#%s: %s", queryIdAsString.c_str(), overview.szQuerierName);
		itemCountsAsString.Format("%i / %i", (int)overview.numResultingItems, (int)overview.numGeneratedItems);
		elapsedTimeAsString.Format("%.2f ms", overview.timeElapsedUntilResult.GetMilliSeconds());
		UQS::Shared::CTimeValueUtil::Split(overview.timestampQueryCreated, &hours, &minutes, &seconds, &milliseconds);
		timestampQueryCreatedAsString.Format  ("%i:%02i:%02i:%03i", hours, minutes, seconds, milliseconds);
		UQS::Shared::CTimeValueUtil::Split(overview.timestampQueryDestroyed, &hours, &minutes, &seconds, &milliseconds);
		timestampQueryDestroyedAsString.Format("%i:%02i:%02i:%03i", hours, minutes, seconds, milliseconds);

		this->dataPerColumn[Column_QueryIdAndQuerierName] = QtUtil::ToQString(queryIdAndQuerierName.c_str());
		this->dataPerColumn[Column_QueryBlueprintName] = QtUtil::ToQString(overview.szQueryBlueprintName);
		this->dataPerColumn[Column_ItemCounts] = QtUtil::ToQString(itemCountsAsString.c_str());
		this->dataPerColumn[Column_ElapsedTime] = QtUtil::ToQString(elapsedTimeAsString.c_str());
		this->dataPerColumn[Column_TimestampQueryCreated] = QtUtil::ToQString(timestampQueryCreatedAsString.c_str());
		this->dataPerColumn[Column_TimestampQueryDestroyed] = QtUtil::ToQString(timestampQueryDestroyedAsString.c_str());
		this->bFoundTooFewItems = overview.bFoundTooFewItems;
		this->bExceptionEncountered = overview.bQueryEncounteredAnException;
		this->bEncounteredSomeWarnings = overview.bQueryEncounteredSomeWarnings;
	}

	static void HelpSerializeEvaluatorsBitfield(Serialization::IArchive& ar, UQS::Core::evaluatorsBitfield_t& bitfieldToSerialize, const std::vector<string>& evaluatorNames, const std::vector<string>& evaluatorLabelsForUI)
	{
		assert(evaluatorNames.size() == evaluatorLabelsForUI.size());

		for (size_t i = 0, n = evaluatorNames.size(); i < n; ++i)
		{
			const UQS::Core::evaluatorsBitfield_t bit = (UQS::Core::evaluatorsBitfield_t)1 << i;
			bool bValue;

			if (ar.isInput())
			{
				ar(bValue, evaluatorNames[i].c_str(), evaluatorLabelsForUI[i].c_str());

				if (bValue)
				{
					bitfieldToSerialize |= bit;
				}
				else
				{
					bitfieldToSerialize &= ~bit;
				}
			}
			else if (ar.isOutput())
			{
				bValue = (bitfieldToSerialize & bit) != 0;
				ar(bValue, evaluatorNames[i].c_str(), evaluatorLabelsForUI[i].c_str());
			}
		}
	}

	void Serialize(Serialization::IArchive& ar)
	{
		HelpSerializeEvaluatorsBitfield(ar, this->evaluatorDrawMasks.maskInstantEvaluators, this->instantEvaluatorNames, this->instantEvaluatorLabelsForUI);
		HelpSerializeEvaluatorsBitfield(ar, this->evaluatorDrawMasks.maskDeferredEvaluators, this->deferredEvaluatorNames, this->deferredEvaluatorLabelsForUI);
	}

	static SQuery* FindQueryByQueryID(SQuery* pStart, const UQS::Core::CQueryID& queryID)
	{
		if (pStart->queryID == queryID)
			return pStart;

		for (SQuery* pChild : pStart->children)
		{
			if (SQuery* pPotentialFind = FindQueryByQueryID(pChild, queryID))
				return pPotentialFind;
		}

		return nullptr;
	}

	bool ContainsWarningsDownwardsAlongHierarchy() const
	{
		if (this->bEncounteredSomeWarnings)
			return true;

		for (const SQuery* pChild : this->children)
		{
			if (pChild->ContainsWarningsDownwardsAlongHierarchy())
				return true;
		}

		return false;
	}
};

const char* SQuery::headers[SQuery::ColumnCount] =
{
	"Query ID + querier",         // Column_QueryIdAndQuerierName
	"Query Blueprint",            // Column_QueryBlueprintName
	"Items (accepted/generated)", // Column_ItemCounts
	"Elapsed time",               // Column_ElapsedTime
	"Timestamp query created",    // Column_TimestampQueryCreated
	"Timestamp query destroyed",  // Column_TimestampQueryDestroyed
};

const char* SQuery::toolTips[SQuery::ColumnCount] =
{
	"Unique id of the query instance and the name of who started that query",                                                                                                                           // Column_QueryIdAndQuerierName
	"Name of the blueprint from which the live query was instantiated",                                                                                                                                 // Column_QueryBlueprintName
	"Number of items that were generated and ended up in the final result set. Notice: a hierarchical query may not necessarily generate items, yet grab the resulting items from one of its children", // Column_ItemCounts
	"Elapsed time from start to finish of the query. Notice: don't confuse with *consumed* time.",                                                                                                      // Column_ElapsedTime
	"Timestamp of when the query was created in h:mm:ss:mmm",                                                                                                                                           // Column_TimestampQueryCreated
	"Timestamp of when the query was destroyed in h:mm:ss:mmm. Notice: might show some weird value if the query was canceled prematurely.",                                                             // Column_TimestampQueryDestroyed
};

//===================================================================================
//
// CHistoricQueryTreeModel
//
//===================================================================================

class CHistoricQueryTreeModel : public QAbstractItemModel
{
public:
	explicit CHistoricQueryTreeModel(QObject* pParent)
		: QAbstractItemModel(pParent)
	{}

	Q_INVOKABLE virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override
	{
		assert(row >= 0 && column >= 0);

		const SQuery* pParentQuery;

		if (parent.isValid())
		{
			pParentQuery = static_cast<const SQuery*>(parent.internalPointer());
		}
		else
		{
			pParentQuery = &m_root;
		}

		if (row < (int)pParentQuery->children.size())
		{
			const SQuery* pChildQuery = pParentQuery->children[row];
			return createIndex(row, column, reinterpret_cast<quintptr>(pChildQuery));
		}
		else
		{
			return QModelIndex();
		}
	}

	Q_INVOKABLE virtual QModelIndex parent(const QModelIndex& child) const override
	{
		if (!child.isValid())
		{
			return QModelIndex();
		}

		const SQuery* pChildQuery = static_cast<SQuery*>(child.internalPointer());
		const SQuery* pParentQuery = pChildQuery->pParent;
		assert(pParentQuery);

		if (pParentQuery == &m_root)
		{
			return QModelIndex();
		}

		for (size_t i = 0, n = pParentQuery->children.size(); i < n; ++i)
		{
			if (pParentQuery->children[i] == pChildQuery)
			{
				return createIndex((int)i, 0, reinterpret_cast<quintptr>(pParentQuery));
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

		const SQuery* pParentQuery;

		if (parent.isValid())
		{
			pParentQuery = static_cast<const SQuery*>(parent.internalPointer());
		}
		else
		{
			pParentQuery = &m_root;
		}

		return (int)pParentQuery->children.size();
	}

	Q_INVOKABLE virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override
	{
		return SQuery::ColumnCount;
	}

	Q_INVOKABLE virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
	{
		if (role == Qt::TextColorRole)
		{
			if (index.isValid())
			{
				const SQuery* pQuery = static_cast<const SQuery*>(index.internalPointer());

				// *red* text if the query encountered an exception
				if (pQuery->bExceptionEncountered)
				{
					return QtUtil::ToQColor(ColorB(255, 0, 0));
				}

				// *orange* text if the query encountered some warnings
				if (pQuery->bEncounteredSomeWarnings)
				{
					return QtUtil::ToQColor(ColorB(255, 165, 0));	// RGB values taken from Col_Orange
				}

				//
				// bonus: propagate warning's *orange* color up the query hierarchy (although the parent queries will *not* the actual warning messages of their children)
				//
				{
					// see if any of our children contains a warning
					if (pQuery->ContainsWarningsDownwardsAlongHierarchy())
						return QtUtil::ToQColor(ColorB(255, 165, 0));	// RGB values taken from Col_Orange
				}

				// *yellow* text if too few items were found
				if (pQuery->bFoundTooFewItems)
				{
					return QtUtil::ToQColor(ColorB(255, 255, 0));
				}
			}
		}

		if (role == Qt::DisplayRole)
		{
			if (index.isValid())
			{
				const SQuery* pQuery = static_cast<const SQuery*>(index.internalPointer());
				return pQuery->dataPerColumn[index.column()];
			}
		}
		return QVariant();
	}

	Q_INVOKABLE virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
	{
		switch (role)
		{
		case Qt::DisplayRole:
			assert(section >= 0 && section < SQuery::ColumnCount);
			return SQuery::headers[section];

		case Qt::ToolTipRole:
			assert(section >= 0 && section < SQuery::ColumnCount);
			return SQuery::toolTips[section];

		default:
			return QAbstractItemModel::headerData(section, orientation, role);
		}
	}

public:
	SQuery* AddOrUpdateHistoricQuery(const UQS::Core::IQueryHistoryConsumer::SHistoricQueryOverview& overview)
	{
		SQuery* pParent;

		if (overview.parentQueryID.IsValid())
		{
			pParent = SQuery::FindQueryByQueryID(&m_root, overview.parentQueryID);

			// FIXME: this assert() should be handled properly.
			//        -> What most likely happend is:
			//           The history got cleared inbetween having already stored information about the parent and this child now receiving its information.
			//           A typical scenario is when a parent query keeps running for a while, then the user clears the history and *then* the parent query starts its next child
			//           => Once that child finishes and reports back to the history, the historic counterpart of its parent is already gone.
			assert(pParent);
		}
		else
		{
			pParent = &m_root;
		}

		SQuery* pNewOrUpdatedQuery = SQuery::FindQueryByQueryID(&m_root, overview.queryID);

		if (pNewOrUpdatedQuery)
		{
			pNewOrUpdatedQuery->UpdateInformation(overview);
		}
		else
		{
			pNewOrUpdatedQuery = new SQuery(pParent, overview);
			pParent->children.push_back(pNewOrUpdatedQuery);
		}

		beginResetModel();
		endResetModel();

		return pNewOrUpdatedQuery;
	}

	void ClearAllHistoricQueries()
	{
		for (SQuery* pChild : m_root.children)
			delete pChild;
		m_root.children.clear();

		beginResetModel();
		endResetModel();
	}

private:
	SQuery m_root;
};

//===================================================================================
//
// CHistoricQueryTreeView
//
//===================================================================================

class CHistoricQueryTreeView : public QAdvancedTreeView
{
public:
	explicit CHistoricQueryTreeView(QWidget* pParent)
		: QAdvancedTreeView(QAdvancedTreeView::Behavior(QAdvancedTreeView::PreserveExpandedAfterReset | QAdvancedTreeView::PreserveSelectionAfterReset), pParent)
	{
	}

	const SQuery* GetQueryByModelIndex(const QModelIndex &modelIndex)
	{
		return static_cast<SQuery*>(modelIndex.internalPointer());
	}

	const SQuery* GetSelectedQuery() const
	{
		QModelIndexList indexList = selectedIndexes();

		if (indexList.empty())
		{
			return nullptr;
		}
		else
		{
			const QModelIndex& modelIndex = indexList.at(0);
			return static_cast<SQuery*>(modelIndex.internalPointer());
		}
	}

protected:
	virtual void currentChanged(const QModelIndex& current, const QModelIndex& previous) override
	{
		QAdvancedTreeView::currentChanged(current, previous);

		UQS::Core::IQueryHistoryManager* pHistoryQueryManager = GetHistoryQueryManager();

		if (!pHistoryQueryManager)
			return;

		if (current.isValid())
		{
			const SQuery* pQueryToSwitchTo = static_cast<const SQuery*>(current.internalPointer());
			const UQS::Core::IQueryHistoryManager::EHistoryOrigin whichHistory = pHistoryQueryManager->GetCurrentQueryHistory();
			pHistoryQueryManager->MakeHistoricQueryCurrentForInWorldRendering(whichHistory, pQueryToSwitchTo->queryID);
		}
	}

	virtual void mouseDoubleClickEvent(QMouseEvent *event) override
	{
		QAdvancedTreeView::mouseDoubleClickEvent(event);

		if (CViewport* pActiveView = GetIEditor()->GetActiveView())
		{
			if (UQS::Core::IQueryHistoryManager* pQueryHistoryManager = GetHistoryQueryManager())
			{
				UQS::Core::SDebugCameraView cameraView;

				// construct the current camera view
				{
					const Matrix34& viewTM = pActiveView->GetViewTM();
					Matrix33 orientation;
					viewTM.GetRotation33(orientation);
					cameraView.pos = viewTM.GetTranslation();
					cameraView.dir = orientation * Vec3(0, 1, 0);
				}

				cameraView = pQueryHistoryManager->GetIdealDebugCameraView(
					pQueryHistoryManager->GetCurrentQueryHistory(),
					pQueryHistoryManager->GetCurrentHistoricQueryForInWorldRendering(pQueryHistoryManager->GetCurrentQueryHistory()),
					cameraView);

				// change the editor's camera view
				{
					const Matrix33 orientation = Matrix33::CreateRotationVDir(cameraView.dir);
					const Matrix34 transform(orientation, cameraView.pos);
					pActiveView->SetViewTM(transform);
				}
			}
		}
	}

private:
	UQS::Core::IQueryHistoryManager* GetHistoryQueryManager()
	{
		if (UQS::Core::IHub* pHub = UQS::Core::IHubPlugin::GetHubPtr())
		{
			return &pHub->GetQueryHistoryManager();
		}
		else
		{
			return nullptr;
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// CMainEditorWindow
//////////////////////////////////////////////////////////////////////////

CMainEditorWindow::CUQSHistoryPostRenderer::CUQSHistoryPostRenderer(CHistoricQueryTreeView& historicQueryTreeView)
	: m_historicQueryTreeView(historicQueryTreeView)
{
	// nothing
}

void CMainEditorWindow::CUQSHistoryPostRenderer::OnPostRender() const
{
	if (UQS::Core::IHub* pHub = UQS::Core::IHubPlugin::GetHubPtr())
	{
		if (const CViewport* pGameViewport = GetIEditor()->GetViewportManager()->GetGameViewport())
		{
			const Matrix34& viewTM = pGameViewport->GetViewTM();
			Matrix33 orientation;
			viewTM.GetRotation33(orientation);
			UQS::Core::SDebugCameraView uqsCameraView;
			uqsCameraView.pos = viewTM.GetTranslation();
			uqsCameraView.dir = orientation * Vec3(0, 1, 0);

			UQS::Core::IQueryHistoryManager::SEvaluatorDrawMasks evaluatorDrawMasks = UQS::Core::IQueryHistoryManager::SEvaluatorDrawMasks::CreateAllBitsSet();

			if (const SQuery* pSelectedQuery = m_historicQueryTreeView.GetSelectedQuery())
			{
				evaluatorDrawMasks = pSelectedQuery->evaluatorDrawMasks;
			}

			pHub->GetQueryHistoryManager().UpdateDebugRendering3D(&uqsCameraView, evaluatorDrawMasks);
		}
	}
}

CMainEditorWindow::CMainEditorWindow()
	: m_windowTitle("UQS History")
	, m_pQueryHistoryManager(nullptr)
	, m_pFreshlyAddedOrUpdatedQuery(nullptr)
	, m_pHistoryPostRenderer(nullptr)
{
	// "file" menu
	{
		QMenu* pFileMenu = menuBar()->addMenu("&File");

		QAction* pLoadHistory = pFileMenu->addAction("&Load history");
		connect(pLoadHistory, &QAction::triggered, this, &CMainEditorWindow::OnLoadHistoryFromFile);

		QAction* pSaveLiveHistory = pFileMenu->addAction("&Save 'live' history");
		connect(pSaveLiveHistory, &QAction::triggered, this, &CMainEditorWindow::OnSaveLiveHistoryToFile);
	}

	m_pComboBoxHistoryOrigin = new QComboBox(this);
	m_pComboBoxHistoryOrigin->addItem("Live history", QVariant(UQS::Core::IQueryHistoryManager::EHistoryOrigin::Live));
	m_pComboBoxHistoryOrigin->addItem("Deserialized history", QVariant(UQS::Core::IQueryHistoryManager::EHistoryOrigin::Deserialized));

	m_pButtonClearCurrentHistory = new QPushButton(this);
	m_pButtonClearCurrentHistory->setText("Clear history");

	m_pTreeView = new CHistoricQueryTreeView(this);
	m_pTreeModel = new CHistoricQueryTreeModel(this);
	m_pTreeView->setModel(m_pTreeModel);

	m_pTextQueryDetails = new QTextEdit(this);
	m_pTextQueryDetails->setText("(Query details will go here)");

	m_pTextItemDetails = new QTextEdit(this);
	m_pTextItemDetails->setText("(Item details will go here)");

	m_pPropertyTree = new QPropertyTree;

	QSplitter* pDetailsSplitter = new QSplitter(this);
	pDetailsSplitter->setOrientation(Qt::Vertical);
	pDetailsSplitter->addWidget(m_pTextQueryDetails);
	pDetailsSplitter->addWidget(m_pTextItemDetails);
	pDetailsSplitter->addWidget(m_pPropertyTree);

	QHBoxLayout* pHBoxLayout = new QHBoxLayout(this);
	pHBoxLayout->addWidget(m_pComboBoxHistoryOrigin);
	pHBoxLayout->addWidget(m_pButtonClearCurrentHistory);

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

	QObject::connect(m_pComboBoxHistoryOrigin, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &CMainEditorWindow::OnHistoryOriginComboBoxSelectionChanged);	// the static_cast<> is only for disambiguation of the overloaded currentIndexChanged() method
	QObject::connect(m_pButtonClearCurrentHistory, &QPushButton::clicked, this, &CMainEditorWindow::OnClearHistoryButtonClicked);
	QObject::connect(m_pTreeView->selectionModel(), &QItemSelectionModel::currentChanged, this, &CMainEditorWindow::OnTreeViewCurrentChanged);

	// instantiate the CUQSHistoryPostRenderer on the heap - it's IPostRenderer is refcounted and deletes itself when un-registering from the game's viewport
	m_pHistoryPostRenderer = new CUQSHistoryPostRenderer(*m_pTreeView);
	GetIEditor()->GetViewportManager()->GetGameViewport()->AddPostRenderer(m_pHistoryPostRenderer);	// this increments its refcount

	if (UQS::Core::IHub* pHub = UQS::Core::IHubPlugin::GetHubPtr())
	{
		m_pQueryHistoryManager = &pHub->GetQueryHistoryManager();
		m_pQueryHistoryManager->RegisterQueryHistoryListener(this);
		m_pQueryHistoryManager->EnumerateHistoricQueries(m_pQueryHistoryManager->GetCurrentQueryHistory(), *this);
	}
	else
	{
		// disable everything
		QMessageBox::warning(this, "UQS engine-plugin not loaded", "The UQS History Inspector will be disabled as the UQS core engine-plugin hasn't been loaded.");
		setEnabled(false);
	}
}

CMainEditorWindow::~CMainEditorWindow()
{
	if (m_pQueryHistoryManager)
	{
		m_pQueryHistoryManager->UnregisterQueryHistoryListener(this);
	}

	// - remove the QueryHistoryPostRenderer
	// - notice: we will *not* leak memory if the viewport-manager and/or the game-viewport are already gone, since they would then have decremented its refcount and eventually delete'd the object already
	if (IViewportManager* pViewportManager = GetIEditor()->GetViewportManager())
	{
		if(CViewport* pGameViewport = pViewportManager->GetGameViewport())
		{
			pGameViewport->RemovePostRenderer(m_pHistoryPostRenderer);	// this decrements its refcount, eventually delete'ing it
		}
	}
}

const char* CMainEditorWindow::GetPaneTitle() const
{
	// check for whether the UQS engine plugin has been loaded
	if (UQS::Core::IHubPlugin::GetHubPtr())
	{
		return m_windowTitle;
	}
	else
	{
		return "UQS History (disabled due to the UQS engine-plugin not being loaded)";
	}
}

void CMainEditorWindow::OnQueryHistoryEvent(const UQS::Core::IQueryHistoryListener::SEvent& ev)
{
	switch (ev.type)
	{
	case UQS::Core::IQueryHistoryListener::EEventType::HistoricQueryJustGotCreatedInLiveQueryHistory:
	case UQS::Core::IQueryHistoryListener::EEventType::HistoricQueryJustFinishedInLiveQueryHistory:
		if (m_pQueryHistoryManager->GetCurrentQueryHistory() == UQS::Core::IQueryHistoryManager::EHistoryOrigin::Live)
		{
			assert(ev.relatedQueryID.IsValid());
			m_pQueryHistoryManager->EnumerateSingleHistoricQuery(m_pQueryHistoryManager->GetCurrentQueryHistory(), ev.relatedQueryID, *this);
		}
		break;

	case UQS::Core::IQueryHistoryListener::EEventType::CurrentQueryHistorySwitched:
		m_pTreeModel->ClearAllHistoricQueries();
		m_pQueryHistoryManager->EnumerateHistoricQueries(m_pQueryHistoryManager->GetCurrentQueryHistory(), *this);
		break;

	case UQS::Core::IQueryHistoryListener::EEventType::DeserializedQueryHistoryCleared:
		if (m_pQueryHistoryManager->GetCurrentQueryHistory() == UQS::Core::IQueryHistoryManager::EHistoryOrigin::Deserialized)
		{
			m_pTreeModel->ClearAllHistoricQueries();
		}
		break;

	case UQS::Core::IQueryHistoryListener::EEventType::LiveQueryHistoryCleared:
		if (m_pQueryHistoryManager->GetCurrentQueryHistory() == UQS::Core::IQueryHistoryManager::EHistoryOrigin::Live)
		{
			m_pTreeModel->ClearAllHistoricQueries();
		}
		break;

	case UQS::Core::IQueryHistoryListener::EEventType::DifferentHistoricQuerySelected:
		{
			const UQS::Core::IQueryHistoryManager::EHistoryOrigin historyOrigin = m_pQueryHistoryManager->GetCurrentQueryHistory();
			const UQS::Core::CQueryID queryID = m_pQueryHistoryManager->GetCurrentHistoricQueryForInWorldRendering(historyOrigin);
			m_pTextQueryDetails->clear();
			m_pTextItemDetails->clear();   // get rid of some leftovers in case no item will be focused anymore
			m_pQueryHistoryManager->GetDetailsOfHistoricQuery(historyOrigin, queryID, *this);
		}
		break;

	case UQS::Core::IQueryHistoryListener::EEventType::FocusedItemChanged:
		{
			m_pTextItemDetails->clear();
			m_pQueryHistoryManager->GetDetailsOfFocusedItem(*this);
		}
		break;

	case UQS::Core::IQueryHistoryListener::EEventType::QueryHistoryDeserialized:
		if (m_pQueryHistoryManager->GetCurrentQueryHistory() == UQS::Core::IQueryHistoryManager::EHistoryOrigin::Deserialized)
		{
			m_pTreeModel->ClearAllHistoricQueries();
			m_pQueryHistoryManager->EnumerateHistoricQueries(m_pQueryHistoryManager->GetCurrentQueryHistory(), *this);
		}
		break;
	}
}

void CMainEditorWindow::AddOrUpdateHistoricQuery(const SHistoricQueryOverview& overview)
{
	assert(m_pFreshlyAddedOrUpdatedQuery == nullptr);
	
	m_pFreshlyAddedOrUpdatedQuery = m_pTreeModel->AddOrUpdateHistoricQuery(overview);

	m_pQueryHistoryManager->EnumerateInstantEvaluatorNames(m_pQueryHistoryManager->GetCurrentQueryHistory(), m_pFreshlyAddedOrUpdatedQuery->queryID, *this);
	m_pQueryHistoryManager->EnumerateDeferredEvaluatorNames(m_pQueryHistoryManager->GetCurrentQueryHistory(), m_pFreshlyAddedOrUpdatedQuery->queryID, *this);

	m_pFreshlyAddedOrUpdatedQuery = nullptr;
}

void CMainEditorWindow::AddTextLineToCurrentHistoricQuery(const ColorF& color, const char* szFormat, ...)
{
	stack_string tmpText;
	va_list ap;
	va_start(ap, szFormat);
	tmpText.FormatV(szFormat, ap);
	va_end(ap);

	const unsigned int rgb888 = color.pack_rgb888();
	stack_string html;
	html.Format("<font color='#%06x'>%s</font><br>", rgb888, tmpText.c_str());

	const QString qHtml = QtUtil::ToQString(html.c_str());
	m_pTextQueryDetails->insertHtml(qHtml);
}

void CMainEditorWindow::AddTextLineToFocusedItem(const ColorF& color, const char* szFormat, ...)
{
	stack_string tmpText;
	va_list ap;
	va_start(ap, szFormat);
	tmpText.FormatV(szFormat, ap);
	va_end(ap);

	const unsigned int rgb888 = color.pack_rgb888();
	stack_string html;
	html.Format("<font color='#%06x'>%s</font><br>", rgb888, tmpText.c_str());

	const QString qHtml = QtUtil::ToQString(html.c_str());
	m_pTextItemDetails->insertHtml(qHtml);
}

void CMainEditorWindow::AddInstantEvaluatorName(const char* szInstantEvaluatorName)
{
	assert(m_pFreshlyAddedOrUpdatedQuery);

	m_pFreshlyAddedOrUpdatedQuery->instantEvaluatorNames.push_back(szInstantEvaluatorName);

	string label;
	label.Format("IE #%i: %s", (int)m_pFreshlyAddedOrUpdatedQuery->instantEvaluatorLabelsForUI.size(), szInstantEvaluatorName);
	m_pFreshlyAddedOrUpdatedQuery->instantEvaluatorLabelsForUI.push_back(label);
}

void CMainEditorWindow::AddDeferredEvaluatorName(const char* szDeferredEvaluatorName)
{
	assert(m_pFreshlyAddedOrUpdatedQuery);

	m_pFreshlyAddedOrUpdatedQuery->deferredEvaluatorNames.push_back(szDeferredEvaluatorName);

	string label;
	label.Format("DE #%i: %s", (int)m_pFreshlyAddedOrUpdatedQuery->deferredEvaluatorLabelsForUI.size(), szDeferredEvaluatorName);
	m_pFreshlyAddedOrUpdatedQuery->deferredEvaluatorLabelsForUI.push_back(label);
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

void CMainEditorWindow::OnHistoryOriginComboBoxSelectionChanged(int index)
{
	if (m_pQueryHistoryManager)
	{
		const QVariant data = m_pComboBoxHistoryOrigin->itemData(index);
		assert(data.type() == QVariant::Int);
		const UQS::Core::IQueryHistoryManager::EHistoryOrigin origin = (UQS::Core::IQueryHistoryManager::EHistoryOrigin)data.toInt();
		m_pQueryHistoryManager->MakeQueryHistoryCurrent(origin);
	}
}

void CMainEditorWindow::OnClearHistoryButtonClicked(bool checked)
{
	if (m_pQueryHistoryManager)
	{
		m_pQueryHistoryManager->ClearQueryHistory(m_pQueryHistoryManager->GetCurrentQueryHistory());
	}
}

void CMainEditorWindow::OnSaveLiveHistoryToFile()
{
	QString qFilePath = QFileDialog::getSaveFileName(this, "Save File", "uqs_history.xml", "XML file (*.xml)");
	string sFilePath = QtUtil::ToString(qFilePath);
	if (!sFilePath.empty())  // would be empty if pressing the "cancel" button
	{
		UQS::Shared::CUqsString uqsErrorMessage;
		if (!m_pQueryHistoryManager->SerializeLiveQueryHistory(sFilePath.c_str(), uqsErrorMessage))
		{
			// show the error message
			stack_string error;
			error.Format("Serializing the live query history to '%s' failed: %s", sFilePath.c_str(), uqsErrorMessage.c_str());
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "UQS Query History Inspector: %s", error.c_str());
			QMessageBox::warning(this, "Error saving the live query history", error.c_str());
		}
		else
		{
			// change the window title to also show the file name
			m_windowTitle.Format("UQS History - %s", sFilePath.c_str());
			setWindowTitle(QtUtil::ToQString(m_windowTitle));
		}
	}
}

void CMainEditorWindow::OnLoadHistoryFromFile()
{
	QString qFilePath = QFileDialog::getOpenFileName(this, "Load File", "*.xml", "XML file (*.xml)");
	if (!qFilePath.isEmpty())
	{
		string sFilePath = QtUtil::ToString(qFilePath);
		UQS::Shared::CUqsString uqsErrorMessage;
		if (m_pQueryHistoryManager->DeserializeQueryHistory(sFilePath.c_str(), uqsErrorMessage))
		{
			// ensure the "deserialized" entry in the history origin combo-box is selected
			m_pComboBoxHistoryOrigin->setCurrentIndex(1);

			// change the window title to also show the file name
			m_windowTitle.Format("UQS History - %s", sFilePath.c_str());
			setWindowTitle(QtUtil::ToQString(m_windowTitle));
		}
		else
		{
			// show the error message
			stack_string error;
			error.Format("Deserializing the query history from '%s' failed: %s", sFilePath.c_str(), uqsErrorMessage.c_str());
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "UQS Query History Inspector: %s", error.c_str());
			QMessageBox::warning(this, "Error loading the query history", error.c_str());
		}
	}
}

void CMainEditorWindow::OnTreeViewCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
{
	if (const SQuery* pQuery = m_pTreeView->GetQueryByModelIndex(current))
	{
		m_pPropertyTree->attach(Serialization::SStruct(*pQuery));
	}
}
