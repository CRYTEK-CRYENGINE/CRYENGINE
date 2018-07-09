// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "PropertyTree2.h"

#include "PropertyTreeModel.h"
#include "PropertyTreeIArchive.h"
#include "PropertyTreeOArchive.h"
#include "PropertyTreeFormWidget.h"

#include <QScrollArea>
#include <QEvent.h>

#include <CrySandbox/ScopedVariableSetter.h>
#include <CrySerialization/yasli/BinArchive.h>

#include "QSearchBox.h"

//TODOLIST:
//- optimizations: 
//		* check box doesn't need to be a widget
//		* finding model children by name could be sped up by a map

QPropertyTree2::QPropertyTree2(QWidget* parent /*= nullptr*/)
	: m_autoRevert(true)
	, m_ignoreChanges(false)
	, m_currentSearchRow(nullptr)
	, m_activeRow(nullptr)
	, m_accumulateChanges(AccumulateChangesStatus::None)
	, m_autoExpandDepth(1)
{
	m_scrollArea = new QScrollArea();
	m_scrollArea->setWidgetResizable(true);
	m_rootForm = nullptr;

	m_scrollArea->setWidget(nullptr);
	m_scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	m_searchBox = new QSearchBox();
	m_searchBox->SetSearchFunction(this, &QPropertyTree2::OnFindNext);

	auto closeSearch = new QToolButton();
	closeSearch->setIcon(CryIcon("icons:General/Close.ico"));
	closeSearch->setToolTip("Close");
	connect(closeSearch, &QToolButton::clicked, this, &QPropertyTree2::OnCloseSearch);

	auto searchNext = new QToolButton();
	searchNext->setIcon(CryIcon("icons:General/Arrow_Down.ico"));
	searchNext->setToolTip("Next");
	connect(searchNext, &QToolButton::clicked, this, &QPropertyTree2::OnFindNext);

	auto searchPrev = new QToolButton();
	searchPrev->setIcon(CryIcon("icons:General/Arrow_Up.ico"));
	searchPrev->setToolTip("Previous");
	connect(searchPrev, &QToolButton::clicked, this, &QPropertyTree2::OnFindPrevious);

	QHBoxLayout* searchLayout = new QHBoxLayout();
	searchLayout->setMargin(0);
	searchLayout->setSpacing(4);
	searchLayout->addWidget(m_searchBox);
	searchLayout->addWidget(searchNext);
	searchLayout->addWidget(searchPrev);
	searchLayout->addWidget(closeSearch);

	m_searchWidget = new QWidget();
	m_searchWidget->setLayout(searchLayout);
	m_searchWidget->setVisible(false);

	QVBoxLayout* layout = new QVBoxLayout();
	layout->addWidget(m_searchWidget);
	layout->setMargin(0);
	layout->setSpacing(4);
	layout->addWidget(m_scrollArea);

	setLayout(layout);

	m_splitterRatio = 0.5;
	m_isDraggingSplitter = false;

	//Styling
	m_collapsedIcon = QIcon("icons:General/Pointer_Right.ico");
	m_expandedIcon = QIcon("icons:General/Pointer_Down_Expanded.ico");
	m_dragHandleIcon = QIcon("icons:General/Drag_Handle.ico");

	setFocusPolicy(Qt::StrongFocus);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QPropertyTree2::~QPropertyTree2()
{

}

void QPropertyTree2::attach(const yasli::Serializer& serializer)
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

void QPropertyTree2::attach(const yasli::Serializers& serializers)
{
	m_attached.clear();
	m_attached.assign(serializers.begin(), serializers.end());
	m_models.clear();
	m_models.reserve(serializers.size());
	for(int i = 0; i < serializers.size(); i++)
		m_models.emplace_back(new PropertyTree2::CRowModel(nullptr));
	revert();
}

void QPropertyTree2::attach(const yasli::Object& object)
{
	m_attached.clear();
	m_attached.push_back(object);
	m_models.clear();
	m_models.emplace_back(new PropertyTree2::CRowModel(nullptr));
	revert();
}

void QPropertyTree2::detach()
{
	m_attached.clear();
	m_models.clear();
	m_root = nullptr;
	revert();
}

void QPropertyTree2::revert()
{
	using namespace PropertyTree2;

	CScopedVariableSetter<bool> ignoreChangesDuringRevert(m_ignoreChanges, true);

	if (!m_attached.empty())
	{
		for(int i = 0; i < m_attached.size(); i++)
		{
			auto& object = m_attached[i];
			auto& model = m_models[i];
			model->MarkNotVisitedRecursive();

			PropertyTreeOArchive oa(*model);
			oa.setLastContext(m_archiveContext);

			const auto yasliSerializer = object.serializer();
			signalAboutToSerialize(yasliSerializer, oa);
			object(oa);
			signalSerialized(yasliSerializer, oa);
		}

		if(!m_root)
		{
			if (m_attached.size() == 1)
				m_root = m_models[0];
			else // multi-edit
			{
				//Merge the models
				m_root = new CRowModel(nullptr);

				//Run the root model through serialization of the first object [0]
				m_root->MarkNotVisitedRecursive();
				PropertyTreeOArchive oa(*m_root);
				oa.setLastContext(m_archiveContext);
				const auto yasliSerializer = m_attached[0].serializer();
				signalAboutToSerialize(yasliSerializer, oa);
				m_attached[0](oa);
				signalSerialized(yasliSerializer, oa);

				//Intersect with all the other models [1..n]
				auto it = m_models.begin();
				for (++it; it != m_models.end(); ++it)
				{
					m_root->Intersect(*it);
				}
			}

			m_rootForm = new CFormWidget(this, m_root, 0);
			m_scrollArea->setWidget(m_rootForm);
			m_rootForm->show();
			m_scrollArea->show();
		}
		else
		{
			if(m_attached.size() > 1) // multi-edit
			{
				auto it = m_models.begin();

				m_root->MarkNotVisitedRecursive();
				m_root->MarkClean(); //The root is clean and should not be pruned

				//Run the root model through serialization of the first object [0]
				m_root->MarkNotVisitedRecursive();
				PropertyTreeOArchive oa(*m_root);
				oa.setLastContext(m_archiveContext);
				const auto yasliSerializer = m_attached[0].serializer();
				signalAboutToSerialize(yasliSerializer, oa);
				m_attached[0](oa);
				signalSerialized(yasliSerializer, oa);

				//Clean after copy so to make intersection meaningful again
				m_root->PruneNotVisitedChildren();

				//Intersect with all the other models [1..n]
				for (++it; it != m_models.end(); ++it)
				{
					(*it)->PruneNotVisitedChildren();
					m_root->Intersect(*it);
				}
			}
			else
			{
				m_root->PruneNotVisitedChildren();
			}

			m_rootForm->UpdateTree();

			//Clear active row and search row if they are now detached
			if (m_activeRow && m_activeRow->IsRoot())
				m_activeRow.reset();

			if (m_currentSearchRow && m_currentSearchRow->IsRoot())
				m_currentSearchRow.reset();
		}
	}
	else
	{
		m_root = nullptr;
		m_scrollArea->setWidget(nullptr);
	}
}

void QPropertyTree2::apply()
{
	using namespace PropertyTree2;

	if (m_attached.empty())
		return;

	for (int i = 0; i < m_attached.size(); i++)
	{
		auto& object = m_attached[i];
		auto& model = m_models[i];

		PropertyTreeIArchive oa(*model);
		oa.setLastContext(m_archiveContext);

		const auto yasliSerializer = object.serializer();
		signalAboutToSerialize(yasliSerializer, oa);
		object(oa);
		signalSerialized(yasliSerializer, oa);
	}
}

void QPropertyTree2::keyPressEvent(QKeyEvent* ev)
{
	if (ev->key() == Qt::Key_Escape)
	{
		if (m_searchWidget->isVisible())
		{
			ev->accept();
			OnCloseSearch();
			return;
		}
	}

	ev->ignore();
}

void QPropertyTree2::customEvent(QEvent* event)
{
	if (event->type() == SandboxEvent::Command)
	{
		CommandEvent* commandEvent = static_cast<CommandEvent*>(event);

		const string& command = commandEvent->GetCommand();
		
		if (command == "general.find")
		{
			OnFind();
			event->accept();
		}
		else if (command == "general.find_previous")
		{
			OnFindPrevious();
			event->accept();
		}
		else if (command == "general.find_next")
		{
			OnFindNext();
			event->accept();
		}
	}
	else
	{
		QWidget::customEvent(event);
	}
}

void QPropertyTree2::OnRowChanged(const PropertyTree2::CRowModel* row)
{
	if (m_ignoreChanges)
		return;

	if (m_attached.size() > 1) //multi-edit
	{
		//Copy the changed value over to the individual models
		CopyRowValueToModels(row);
	}

	if (m_accumulateChanges != AccumulateChangesStatus::UndoPushed)
	{
		signalPushUndo();
		if (m_accumulateChanges == AccumulateChangesStatus::Accumulate)
			m_accumulateChanges = AccumulateChangesStatus::UndoPushed;
	}

	apply();

	if(m_accumulateChanges == AccumulateChangesStatus::None)
		signalChanged();

	if (m_autoRevert)
		revert();
}

void QPropertyTree2::OnRowContinuousChanged(const PropertyTree2::CRowModel* row)
{
	if (m_ignoreChanges)
		return;

	if (m_attached.size() > 1) //multi-edit
	{
		//Copy the changed value over to the individual models
		CopyRowValueToModels(row);
	}

	apply();
	signalContinuousChange();
}

void QPropertyTree2::CopyRowValueToModels(const PropertyTree2::CRowModel* row)
{
	//Note: for now this only copies one individual row over, but perhaps in cases of mutable containers we also need to copy the state of the children
	using namespace PropertyTree2;

	std::vector<const CRowModel*> parents;
	const CRowModel* parent = row->GetParent();
	while (!parent->IsRoot())
	{
		parents.push_back(parent);
		parent = parent->GetParent();
	}

	for (auto& model : m_models)
	{
		const CRowModel* current = model;
		for (auto it = parents.rbegin(); it != parents.rend(); ++it)
		{
			//Match by name and type
			auto foundIt = std::find_if(current->GetChildren().begin(), current->GetChildren().end(), [&](const _smart_ptr<CRowModel>& child)
			{
				return child->GetType() == (*it)->GetType() && child->GetName() == (*it)->GetName();
			});

			CRY_ASSERT(foundIt != current->GetChildren().end()); //Since the models have been intersected, the equivalent row must exist
			current = *foundIt;
		}

		auto matchingRowIt = std::find_if(current->GetChildren().begin(), current->GetChildren().end(), [&](const _smart_ptr<CRowModel>& child)
		{
			return child->GetType() == row->GetType() && child->GetName() == row->GetName();
		});
		CRY_ASSERT(matchingRowIt != current->GetChildren().end()); //Since the models have been intersected, the equivalent row must exist

		//Copy value
		if (row->GetPropertyTreeWidget() && (*matchingRowIt)->GetPropertyTreeWidget())
		{
			yasli::BinOArchive oar;
			row->GetPropertyTreeWidget()->Serialize(oar);
			yasli::BinIArchive iar;
			iar.open(oar.buffer(), oar.length());
			(*matchingRowIt)->GetPropertyTreeWidget()->Serialize(iar);
		}
	}
}

void QPropertyTree2::OnFind()
{
	if (!m_searchWidget->isVisible() || !m_searchBox->hasFocus())
	{
		m_searchWidget->setVisible(true);
		m_searchBox->setFocus();
	}
	else
	{
		OnFindNext();
	}
}

void QPropertyTree2::OnFindPrevious()
{
	if (!m_searchWidget->isVisible())
		return;

	QString searchText = m_searchBox->text();
	if (searchText != m_lastSearchText)
	{
		m_lastSearchText = searchText;
		m_currentSearchRow = nullptr;
	}

	auto row = DoFind(m_currentSearchRow ? m_currentSearchRow : m_root, searchText, true);
	if (row)
	{
		m_currentSearchRow = row;
		FocusRow(m_currentSearchRow);
	}
}

void QPropertyTree2::OnFindNext()
{
	if (!m_searchWidget->isVisible())
		return;

	QString searchText = m_searchBox->text();
	if (searchText != m_lastSearchText)
	{
		m_lastSearchText = searchText;
		m_currentSearchRow = nullptr;
	}

	auto row = DoFind(m_currentSearchRow ? m_currentSearchRow : m_root, searchText, false);
	if (row)
	{
		m_currentSearchRow = row;
		FocusRow(m_currentSearchRow);
	}
}

void QPropertyTree2::OnCloseSearch()
{
	m_searchWidget->setVisible(false);
}

const PropertyTree2::CRowModel* QPropertyTree2::DoFind(const PropertyTree2::CRowModel* currentRow, const QString& searchStr, bool searchUp /*= false*/)
{
	if (searchStr.isEmpty())
		return nullptr;

	while (currentRow)
	{
		if(searchUp)
			currentRow = GetRowAbove(currentRow);
		else
			currentRow = GetRowBelow(currentRow);

		if (currentRow && currentRow->GetLabel().contains(searchStr, Qt::CaseInsensitive))
		{
			return currentRow;
		}
	}

	return nullptr;
}

const PropertyTree2::CRowModel* QPropertyTree2::GetRowBelow(const PropertyTree2::CRowModel* row)
{
	if (row->HasChildren())
		return row->GetChildren()[0];

	while(!row->IsRoot())
	{
		auto index = row->GetIndex();
		const auto count = row->GetParent()->GetChildren().size();
		while (count - 1 > index) //Go to next visible sibling
		{
			const auto sibling = row->GetParent()->GetChildren()[index + 1];
			if (!sibling->IsHidden())
				return sibling;
			else
				index++;
		}

		row = row->GetParent();
	}

	return nullptr;
}

const PropertyTree2::CRowModel* QPropertyTree2::GetRowAbove(const PropertyTree2::CRowModel* row)
{
	if (row->IsRoot())
		return nullptr;

	auto index = row->GetIndex();
	while (index > 0)
	{
		//Previous sibling or its bottom-est child
		row = row->GetParent()->GetChildren()[index - 1];
		if(!row->IsHidden())
		{
			while (row->HasChildren())
			{
				row = row->GetChildren()[row->GetChildren().size() - 1];
			}

			if (!row->IsHidden())
				return row;
			else
				return GetRowAbove(row);
		}
		else
		{
			index--;
		}
	}

	return row->GetParent();
}

void QPropertyTree2::FocusRow(const PropertyTree2::CRowModel* row)
{
	CRY_ASSERT(row);
	EnsureRowVisible(row);
	SetActiveRow(row);
}

void QPropertyTree2::SetAccumulateChanges(bool accumulate)
{
	if (accumulate)
	{
		m_accumulateChanges = AccumulateChangesStatus::Accumulate;
	}
	else 
	{
		if (m_accumulateChanges == AccumulateChangesStatus::UndoPushed)
			signalChanged();

		m_accumulateChanges = AccumulateChangesStatus::None;
	}
}

void QPropertyTree2::SetSplitterPosition(int pos)
{
	pos = crymath::clamp(pos, 20, width() - 20);
	if (pos != GetSplitterPosition())
	{
		m_splitterRatio = float(pos) / float(width());

		//Force relayout for all form widgets
		m_rootForm->OnSplitterPosChanged();
	}
}

int QPropertyTree2::GetSplitterPosition() const
{
	//Note: the position is stored as a float ratio because this allows a correct resize behavior.
	//Otherwise the integer conversion leads to a buggy resize behavior.
	return int(m_splitterRatio * float(width()));
}

void QPropertyTree2::SetActiveRow(const PropertyTree2::CRowModel* row)
{
	if(m_activeRow != row)
	{
		m_activeRow = row;

		//Notify all children
		m_rootForm->OnActiveRowChanged(m_activeRow);
	}
}

void QPropertyTree2::EnsureRowVisible(const PropertyTree2::CRowModel* row)
{
	const auto form = m_rootForm->ExpandToRow(row);
	form->ScrollToRow(row);
}

