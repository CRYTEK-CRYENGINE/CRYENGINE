// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "PropertyTree.h"

#include "PropertyTreeModel.h"
#include "PropertyTreeIArchive.h"
#include "PropertyTreeOArchive.h"
#include "PropertyTreeFormWidget.h"

#include <QScrollArea>
#include <QEvent.h>

#include <EditorFramework/Events.h>
#include <CrySandbox/ScopedVariableSetter.h>
#include <CrySerialization/yasli/BinArchive.h>

#include "QSearchBox.h"
#include <QToolButton>
#include <CryIcon.h>

class QPropertyTree::CScrollArea : public QScrollArea
{
public:
	// Required for intercepting all property tree children widget wheel events and forwarding
	// to the scroll area. Since QScrollArea event handlers are not accessible to the property
	// tree we expose a public member that can forward the event
	void HandleScroll(QWheelEvent* pEvent)
	{
		wheelEvent(pEvent);
	}
};

//////////////////////////////////////////////////////////////////////////

//TODOLIST:
//- optimizations:
//		* check box doesn't need to be a widget
//		* finding model children by name could be sped up by a map

QPropertyTree::QPropertyTree(QWidget* pParent /*= nullptr*/)
	: m_autoRevert(true)
	, m_ignoreChanges(false)
	, m_pCurrentSearchRow(nullptr)
	, m_pActiveRow(nullptr)
	, m_accumulateChanges(AccumulateChangesStatus::None)
	, m_autoExpandDepth(1)
	, m_collapsedIcon("icons:General/Pointer_Right.ico")
	, m_expandedIcon("icons:General/Pointer_Down_Expanded.ico")
	, m_dragHandleIcon("icons:General/Drag_Handle_Horizontal.ico")
{
	m_pScrollArea = new CScrollArea();
	m_pScrollArea->setWidgetResizable(true);
	m_pRootForm = nullptr;

	m_pScrollArea->setWidget(nullptr);
	m_pScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	m_pSearchBox = new QSearchBox();
	m_pSearchBox->SetSearchFunction(this, &QPropertyTree::OnFindNext);

	QToolButton* pCloseSearch = new QToolButton();
	pCloseSearch->setIcon(CryIcon("icons:General/Close.ico"));
	pCloseSearch->setToolTip("Close");
	connect(pCloseSearch, &QToolButton::clicked, this, &QPropertyTree::OnCloseSearch);

	QToolButton* pSearchNext = new QToolButton();
	pSearchNext->setIcon(CryIcon("icons:General/Arrow_Down.ico"));
	pSearchNext->setToolTip("Next");
	connect(pSearchNext, &QToolButton::clicked, this, &QPropertyTree::OnFindNext);

	QToolButton* pSearchPrevious = new QToolButton();
	pSearchPrevious->setIcon(CryIcon("icons:General/Arrow_Up.ico"));
	pSearchPrevious->setToolTip("Previous");
	connect(pSearchPrevious, &QToolButton::clicked, this, &QPropertyTree::OnFindPrevious);

	QHBoxLayout* pSearchLayout = new QHBoxLayout();
	pSearchLayout->setMargin(0);
	pSearchLayout->setSpacing(4);
	pSearchLayout->addWidget(m_pSearchBox);
	pSearchLayout->addWidget(pSearchNext);
	pSearchLayout->addWidget(pSearchPrevious);
	pSearchLayout->addWidget(pCloseSearch);

	m_pSearchWidget = new QWidget();
	m_pSearchWidget->setLayout(pSearchLayout);
	m_pSearchWidget->setVisible(false);

	QVBoxLayout* pLayout = new QVBoxLayout();
	pLayout->addWidget(m_pSearchWidget);
	pLayout->setMargin(0);
	pLayout->setSpacing(4);
	pLayout->addWidget(m_pScrollArea);

	setLayout(pLayout);

	m_splitterRatio = 0.5;
	m_isDraggingSplitter = false;

	setFocusPolicy(Qt::StrongFocus);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QPropertyTree::~QPropertyTree()
{

}

void QPropertyTree::attach(const yasli::Serializer& serializer)
{
	if (m_attached.size() != 1 || m_attached[0].serializer() != serializer)
	{
		m_attached.clear();
		m_attached.push_back(yasli::Object(serializer));
		m_models.clear();
		m_models.emplace_back(new PropertyTree2::CRowModel(nullptr));
	}
	revert();
}

void QPropertyTree::attach(const yasli::Serializers& serializers)
{
	m_attached.clear();
	m_attached.assign(serializers.begin(), serializers.end());
	m_models.clear();
	m_models.reserve(serializers.size());
	for (int i = 0; i < serializers.size(); i++)
	{
		m_models.emplace_back(new PropertyTree2::CRowModel(nullptr));
	}
	revert();
}

void QPropertyTree::attach(const yasli::Object& object)
{
	m_attached.clear();
	m_attached.push_back(object);
	m_models.clear();
	m_models.emplace_back(new PropertyTree2::CRowModel(nullptr));
	revert();
}

void QPropertyTree::detach()
{
	m_attached.clear();
	m_models.clear();
	m_pRoot = nullptr;
	revert();
}

void QPropertyTree::revert()
{
	using namespace PropertyTree2;

	CScopedVariableSetter<bool> ignoreChangesDuringRevert(m_ignoreChanges, true);

	if (!m_attached.empty())
	{
		for (int i = 0; i < m_attached.size(); i++)
		{
			yasli::Object& object = m_attached[i];
			_smart_ptr<PropertyTree2::CRowModel>& pModel = m_models[i];
			pModel->MarkNotVisitedRecursive();

			PropertyTreeOArchive propertyArchive(*pModel);
			propertyArchive.setLastContext(m_pArchiveContext);

			const yasli::Serializer objectSerializer = object.serializer();
			signalAboutToSerialize(objectSerializer, propertyArchive);
			object(propertyArchive);
			signalSerialized(objectSerializer, propertyArchive);
		}

		if (!m_pRoot)
		{
			if (m_attached.size() == 1)
			{
				m_pRoot = m_models[0];
			}
			else // multi-edit
			{
				//Merge the models
				m_pRoot = new CRowModel(nullptr);

				//Run the root model through serialization of the first object [0]
				m_pRoot->MarkNotVisitedRecursive();
				PropertyTreeOArchive rootOArchive(*m_pRoot);
				rootOArchive.setLastContext(m_pArchiveContext);
				const yasli::Serializer attachedObjectSerializer = m_attached[0].serializer();
				signalAboutToSerialize(attachedObjectSerializer, rootOArchive);
				m_attached[0](rootOArchive);
				signalSerialized(attachedObjectSerializer, rootOArchive);

				//Intersect with all the other models [1..n]
				auto modelsIterator = m_models.begin();
				for (++modelsIterator; modelsIterator != m_models.end(); ++modelsIterator)
				{
					m_pRoot->Intersect(*modelsIterator);
				}
			}

			m_pRootForm = new CFormWidget(this, m_pRoot, 0);
			m_pScrollArea->setWidget(m_pRootForm);
			m_pRootForm->show();
			m_pScrollArea->show();
		}
		else
		{
			if (m_attached.size() > 1) // multi-edit
			{
				auto modelsIterator = m_models.begin();

				m_pRoot->MarkNotVisitedRecursive();
				m_pRoot->MarkClean(); //The root is clean and should not be pruned

				//Run the root model through serialization of the first object [0]
				m_pRoot->MarkNotVisitedRecursive();
				PropertyTreeOArchive rootOArchive(*m_pRoot);
				rootOArchive.setLastContext(m_pArchiveContext);
				const yasli::Serializer attachedObjectSerializer = m_attached[0].serializer();
				signalAboutToSerialize(attachedObjectSerializer, rootOArchive);
				m_attached[0](rootOArchive);
				signalSerialized(attachedObjectSerializer, rootOArchive);

				//Clean after copy so to make intersection meaningful again
				m_pRoot->PruneNotVisitedChildren();

				//Intersect with all the other models [1..n]
				for (++modelsIterator; modelsIterator != m_models.end(); ++modelsIterator)
				{
					(*modelsIterator)->PruneNotVisitedChildren();
					m_pRoot->Intersect(*modelsIterator);
				}
			}
			else
			{
				m_pRoot->PruneNotVisitedChildren();
			}

			m_pRootForm->UpdateTree();

			//Clear active row and search row if they are now detached
			if (m_pActiveRow && m_pActiveRow->IsRoot())
			{
				m_pActiveRow.reset();
			}

			if (m_pCurrentSearchRow && m_pCurrentSearchRow->IsRoot())
			{
				m_pCurrentSearchRow.reset();
			}
		}
	}
	else
	{
		m_pRoot = nullptr;
		m_pScrollArea->setWidget(nullptr);
	}
}

void QPropertyTree::apply()
{
	using namespace PropertyTree2;

	if (m_attached.empty())
	{
		return;
	}

	for (int i = 0; i < m_attached.size(); i++)
	{
		yasli::Object& object = m_attached[i];
		_smart_ptr<PropertyTree2::CRowModel>& pModel = m_models[i];

		PropertyTreeIArchive modelOArchive(*pModel);
		modelOArchive.setLastContext(m_pArchiveContext);

		const yasli::Serializer objectSerializer = object.serializer();
		signalAboutToSerialize(objectSerializer, modelOArchive);
		object(modelOArchive);
		signalSerialized(objectSerializer, modelOArchive);
	}
}

void QPropertyTree::keyPressEvent(QKeyEvent* pEvent)
{
	if (pEvent->key() == Qt::Key_Escape)
	{
		if (m_pSearchWidget->isVisible())
		{
			pEvent->accept();
			OnCloseSearch();
			return;
		}
	}

	pEvent->ignore();
}

void QPropertyTree::customEvent(QEvent* pEvent)
{
	if (pEvent->type() == SandboxEvent::Command)
	{
		CommandEvent* pCommandEvent = static_cast<CommandEvent*>(pEvent);

		const string& command = pCommandEvent->GetCommand();

		if (command == "general.find")
		{
			OnFind();
			pEvent->accept();
		}
		else if (command == "general.find_previous")
		{
			OnFindPrevious();
			pEvent->accept();
		}
		else if (command == "general.find_next")
		{
			OnFindNext();
			pEvent->accept();
		}
	}
	else
	{
		QWidget::customEvent(pEvent);
	}
}

void QPropertyTree::OnRowChanged(const PropertyTree2::CRowModel* pRow)
{
	if (m_ignoreChanges)
	{
		return;
	}

	if (m_attached.size() > 1) //multi-edit
	{
		//Copy the changed value over to the individual models
		CopyRowValueToModels(pRow);
	}

	apply();

	if (m_accumulateChanges == AccumulateChangesStatus::None)
	{
		signalChanged();
	}

	if (m_autoRevert)
	{
		revert();
	}
}

void QPropertyTree::OnRowDiscarded(const PropertyTree2::CRowModel* pRow)
{
	//When a change is discarded signal it and then reload the tree as the serialized data wwill probably have changed as a result of signalDiscarded() (aka maybe undo is called)
	if (m_ignoreChanges)
	{
		return;
	}

	if (m_accumulateChanges == AccumulateChangesStatus::None)
	{
		signalDiscarded();
	}

	if (m_autoRevert)
	{
		revert();
	}
}

void QPropertyTree::OnRowContinuousChanged(const PropertyTree2::CRowModel* pRow)
{
	if (m_ignoreChanges)
	{
		return;
	}

	if (m_attached.size() > 1) //multi-edit
	{
		//Copy the changed value over to the individual models
		CopyRowValueToModels(pRow);
	}

	apply();
	signalContinuousChange();
}

void QPropertyTree::OnRowPreChanged(const PropertyTree2::CRowModel* pRow)
{
	if (m_ignoreChanges)
	{
		return;
	}

	if (m_accumulateChanges != AccumulateChangesStatus::UndoPushed)
	{
		signalPreChanged();
		if (m_accumulateChanges == AccumulateChangesStatus::Accumulate)
		{
			m_accumulateChanges = AccumulateChangesStatus::UndoPushed;
		}
	}
}

void QPropertyTree::CopyRowValueToModels(const PropertyTree2::CRowModel* pRow)
{
	//Note: for now this only copies one individual row over, but perhaps in cases of mutable containers we also need to copy the state of the children
	using namespace PropertyTree2;

	std::vector<const CRowModel*> parents;
	const CRowModel* pParent = pRow->GetParent();
	while (!pParent->IsRoot())
	{
		parents.push_back(pParent);
		pParent = pParent->GetParent();
	}

	for (_smart_ptr<PropertyTree2::CRowModel>& pModel : m_models)
	{
		const CRowModel* pCurrentModel = pModel;
		for (auto parentsIterator = parents.rbegin(); parentsIterator != parents.rend(); ++parentsIterator)
		{
			//Match by name and type
			auto foundParentIterator = std::find_if(pCurrentModel->GetChildren().begin(), pCurrentModel->GetChildren().end(), [&](const _smart_ptr<CRowModel>& child)
			{
				return child->GetType() == (*parentsIterator)->GetType() && child->GetName() == (*parentsIterator)->GetName();
			});

			CRY_ASSERT(foundParentIterator != pCurrentModel->GetChildren().end()); //Since the models have been intersected, the equivalent row must exist
			pCurrentModel = *foundParentIterator;
		}

		auto matchingRowIterator = std::find_if(pCurrentModel->GetChildren().begin(), pCurrentModel->GetChildren().end(), [&](const _smart_ptr<CRowModel>& child)
		{
			return child->GetType() == pRow->GetType() && child->GetName() == pRow->GetName();
		});
		CRY_ASSERT(matchingRowIterator != pCurrentModel->GetChildren().end()); //Since the models have been intersected, the equivalent row must exist

		//Copy value
		if (pRow->GetPropertyTreeWidget() && (*matchingRowIterator)->GetPropertyTreeWidget())
		{
			yasli::BinOArchive rowOArchive;
			pRow->GetPropertyTreeWidget()->Serialize(rowOArchive);
			yasli::BinIArchive rowIArchive;
			rowIArchive.open(rowOArchive.buffer(), rowOArchive.length());
			(*matchingRowIterator)->GetPropertyTreeWidget()->Serialize(rowIArchive);
		}
	}
}

void QPropertyTree::OnFind()
{
	if (!m_pSearchWidget->isVisible() || !m_pSearchBox->hasFocus())
	{
		m_pSearchWidget->setVisible(true);
		m_pSearchBox->setFocus();
	}
	else
	{
		OnFindNext();
	}
}

void QPropertyTree::OnFindPrevious()
{
	if (!m_pSearchWidget->isVisible())
	{
		return;
	}

	QString searchText = m_pSearchBox->text();
	if (searchText != m_lastSearchText)
	{
		m_lastSearchText = searchText;
		m_pCurrentSearchRow = nullptr;
	}

	const PropertyTree2::CRowModel* pRow = DoFind(m_pCurrentSearchRow ? m_pCurrentSearchRow.get() : m_pRoot.get(), searchText, true);
	if (pRow)
	{
		m_pCurrentSearchRow = pRow;
		FocusRow(m_pCurrentSearchRow);
	}
}

void QPropertyTree::OnFindNext()
{
	if (!m_pSearchWidget->isVisible())
	{
		return;
	}

	QString searchText = m_pSearchBox->text();
	if (searchText != m_lastSearchText)
	{
		m_lastSearchText = searchText;
		m_pCurrentSearchRow = nullptr;
	}

	const PropertyTree2::CRowModel* pRow = DoFind(m_pCurrentSearchRow ? m_pCurrentSearchRow : m_pRoot, searchText, false);
	if (pRow)
	{
		m_pCurrentSearchRow = pRow;
		FocusRow(m_pCurrentSearchRow);
	}
}

void QPropertyTree::OnCloseSearch()
{
	m_pSearchWidget->setVisible(false);
}

const PropertyTree2::CRowModel* QPropertyTree::DoFind(const PropertyTree2::CRowModel* pCurrentRow, const QString& searchString, bool searchUp /*= false*/)
{
	if (searchString.isEmpty())
	{
		return nullptr;
	}

	while (pCurrentRow)
	{
		if (searchUp)
		{
			pCurrentRow = GetRowAbove(pCurrentRow);
		}
		else
		{
			pCurrentRow = GetRowBelow(pCurrentRow);
		}

		if (pCurrentRow && pCurrentRow->GetLabel().contains(searchString, Qt::CaseInsensitive))
		{
			return pCurrentRow;
		}
	}

	return nullptr;
}

const PropertyTree2::CRowModel* QPropertyTree::GetRowBelow(const PropertyTree2::CRowModel* pRow)
{
	if (pRow->HasChildren())
	{
		return pRow->GetChildren()[0];
	}

	while (!pRow->IsRoot())
	{
		int rowIndex = pRow->GetIndex();
		const size_t rowCount = pRow->GetParent()->GetChildren().size();
		while (rowCount - 1 > rowIndex) //Go to next visible sibling
		{
			const _smart_ptr<PropertyTree2::CRowModel> pSibling = pRow->GetParent()->GetChildren()[rowIndex + 1];
			if (!pSibling->IsHidden())
			{
				return pSibling;
			}
			else
			{
				rowIndex++;
			}
		}

		pRow = pRow->GetParent();
	}

	return nullptr;
}

const PropertyTree2::CRowModel* QPropertyTree::GetRowAbove(const PropertyTree2::CRowModel* pRow)
{
	if (pRow->IsRoot())
	{
		return nullptr;
	}

	int rowIndex = pRow->GetIndex();
	while (rowIndex > 0)
	{
		//Previous sibling or its bottom-est child
		pRow = pRow->GetParent()->GetChildren()[rowIndex - 1];
		if (!pRow->IsHidden())
		{
			while (pRow->HasChildren())
			{
				pRow = pRow->GetChildren()[pRow->GetChildren().size() - 1];
			}

			if (!pRow->IsHidden())
			{
				return pRow;
			}
			else
			{
				return GetRowAbove(pRow);
			}
		}
		else
		{
			rowIndex--;
		}
	}

	return pRow->GetParent();
}

void QPropertyTree::FocusRow(const PropertyTree2::CRowModel* pRow)
{
	CRY_ASSERT(pRow);
	EnsureRowVisible(pRow);
	SetActiveRow(pRow);
}

void QPropertyTree::SetAccumulateChanges(bool accumulate)
{
	if (accumulate)
	{
		m_accumulateChanges = AccumulateChangesStatus::Accumulate;
	}
	else
	{
		if (m_accumulateChanges == AccumulateChangesStatus::UndoPushed)
		{
			signalChanged();
		}

		m_accumulateChanges = AccumulateChangesStatus::None;
	}
}

void QPropertyTree::SetSplitterPosition(int position)
{
	position = crymath::clamp(position, 20, width() - 20);
	if (position != GetSplitterPosition())
	{
		m_splitterRatio = float(position) / float(width());

		//Force relayout for all form widgets
		m_pRootForm->OnSplitterPosChanged();
	}
}

int QPropertyTree::GetSplitterPosition() const
{
	//Note: the position is stored as a float ratio because this allows a correct resize behavior.
	//Otherwise the integer conversion leads to a buggy resize behavior.
	return int(m_splitterRatio * float(width()));
}

void QPropertyTree::SetActiveRow(const PropertyTree2::CRowModel* pRow)
{
	if (m_pActiveRow != pRow)
	{
		m_pActiveRow = pRow;

		//Notify all children
		m_pRootForm->OnActiveRowChanged(m_pActiveRow);
	}
}

void QPropertyTree::EnsureRowVisible(const PropertyTree2::CRowModel* pRow)
{
	PropertyTree2::CFormWidget* pForm = m_pRootForm->ExpandToRow(pRow);
	pForm->ScrollToRow(pRow);
}
QScrollArea* QPropertyTree::GetScrollArea()
{
	return m_pScrollArea;
}

void QPropertyTree::HandleScroll(QWheelEvent* pEvent)
{
	CRY_ASSERT_MESSAGE(m_pScrollArea, "Property Tree missing scroll area");
	m_pScrollArea->HandleScroll(pEvent);
}
