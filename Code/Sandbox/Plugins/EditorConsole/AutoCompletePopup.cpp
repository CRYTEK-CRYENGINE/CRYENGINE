// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 1999-2014.
// -------------------------------------------------------------------------
//  File name:   AutoCompletePopup.cpp
//  Version:     v1.00
//  Created:     03/03/2014 by Matthijs vd Meide
//  Compilers:   Visual Studio 2010
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <QtUtil.h>
#include "AutoCompletePopup.h"

#include <QVBoxLayout>

//constructor
CAutoCompletePopup::CAutoCompletePopup(QWidget* parent)
	: QWidget(parent, Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus | Qt::Tool)
	, m_bChanging(false)
{
	//set up the UI
	SetupUI();
	m_pItems->setModel(&m_model);

	m_defaultHeight = 500;

	//never take focus
	setFocusPolicy(Qt::NoFocus);
	setAttribute(Qt::WA_ShowWithoutActivating);
	setAttribute(Qt::WA_AlwaysStackOnTop);

	//attach selection event handler, UI-side
	QItemSelectionModel* pSelection = m_pItems->selectionModel();
	assert(pSelection);
	if (pSelection)
	{
		connect(pSelection, &QItemSelectionModel::selectionChanged, this, &CAutoCompletePopup::HandleSelectionChanged);
	}
}

void CAutoCompletePopup::SetupUI()
{
	setObjectName(QStringLiteral("AutoCompletePopup"));

	QVBoxLayout* verticalLayout = new QVBoxLayout();
	verticalLayout->setSpacing(0);
	verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
	verticalLayout->setContentsMargins(1, 1, 1, 1);

	m_pItems = new QListView();
	m_pItems->setObjectName(QStringLiteral("m_pItems"));
	m_pItems->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	m_pItems->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_pItems->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_pItems->setIconSize(QSize(16, 16));
	m_pItems->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	m_pItems->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

	m_pItems->setMinimumWidth(350);

	m_pHelpText = new QLabel();
	m_pHelpText->setObjectName(QStringLiteral("m_pHelpText"));

	verticalLayout->addWidget(m_pItems);
	verticalLayout->addWidget(m_pHelpText);
	verticalLayout->setAlignment(Qt::AlignBottom);

	setLayout(verticalLayout);
}

//populates the dialog with the specified options
bool CAutoCompletePopup::Set(const Messages::SAutoCompleteReply& reply)
{
	if (!reply.matches.empty())
	{
		//set the description
		size_t count = reply.matches.size();
		QString description = tr("%n items match, arrows to navigate, press TAB to complete", "", count);
		m_pHelpText->setText(std::move(description));

		//set the collection data
		m_model.Set(reply);

		//mark layout as changed
		m_model.layoutChanged();

		Select(1, true);

		//set location and show
		return true;
	}

	//hide the popup
	return false;
}

//model constructor
CAutoCompletePopup::CAutoCompleteModel::CAutoCompleteModel()
{
	//load icons
	m_icons[SAutoCompleteItem::eType_Int] = QPixmap(QStringLiteral(":/int.png"));
	m_icons[SAutoCompleteItem::eType_Float] = QPixmap(QStringLiteral(":/float.png"));
	m_icons[SAutoCompleteItem::eType_String] = QPixmap(QStringLiteral(":/string.png"));
	m_icons[SAutoCompleteItem::eType_Function] = QPixmap(QStringLiteral(":/function.png"));
}

//retrieve data from the history model
QVariant CAutoCompletePopup::CAutoCompleteModel::data(const QModelIndex& index, int role) const
{
	size_t row = index.row();
	const SAutoCompleteItem* pItem = row < m_items.size() ? &m_items.at(row) : NULL;
	if (pItem)
	{
		switch (role)
		{
		case Qt::DisplayRole:
			return pItem->display;
		case Qt::DecorationRole:
			return m_icons[pItem->type];
		}
	}
	return QVariant();
}

//set reply items
void CAutoCompletePopup::CAutoCompleteModel::Set(const Messages::SAutoCompleteReply& reply)
{
	m_items.clear();

	for (std::vector<Messages::SAutoCompleteReply::SItem>::const_iterator i = reply.matches.begin(); i != reply.matches.end(); ++i)
	{
		SAutoCompleteItem item;
		item.display = QtUtil::ToQString(i->name);
		item.name = QtUtil::ToQString(i->name);
		bool appendvalue = true;
		switch (i->type)
		{
		case Messages::eVarType_Int:
			item.type = SAutoCompleteItem::eType_Int;
			break;
		case Messages::eVarType_Float:
			item.type = SAutoCompleteItem::eType_Float;
			break;
		case Messages::eVarType_String:
			item.type = SAutoCompleteItem::eType_String;
			break;
		default:
			item.type = SAutoCompleteItem::eType_Function;
			appendvalue = false;
			break;
		}
		if (appendvalue)
		{
			//append the current value as a hint
			item.display.append(i->value.empty() ? tr(" [empty string]") : QStringLiteral(" (%1)").arg(QtUtil::ToQString(i->value)));
		}
		m_items.push_back(std::move(item));
	}

}

//find closest matching item by name
QModelIndex CAutoCompletePopup::CAutoCompleteModel::Find(const QString& name) const
{
	size_t index = 0;
	size_t match = SIZE_MAX;
	int best = INT_MAX;
	for (std::vector<SAutoCompleteItem>::const_iterator i = m_items.begin(); i != m_items.end(); ++i, ++index)
	{
		int result = name.compare(i->name, Qt::CaseInsensitive);
		int absolute = abs(result);
		if (absolute < best)
		{
			best = absolute;
			match = index;
		}
	}
	return match == SIZE_MAX ? QModelIndex() : this->index(match);
}

//select a new item given a direction
void CAutoCompletePopup::Select(int direction, bool selectCurrent)
{
	QItemSelectionModel* pSelection = m_pItems->selectionModel();
	if (pSelection)
	{
		int index = 0;
		QModelIndexList selected = pSelection->selectedIndexes();
		const QModelIndex* pIndex = !selected.empty() ? &selected.at(0) : NULL;
		if (pIndex && pIndex->isValid())
		{
			index = pIndex->row();

			if (!selectCurrent)
			{
				index += direction;
			}
		}
		else
		{
			index = 0;
		}
		if (index < 0) index = 0;
		int count = m_model.rowCount(*(QModelIndex*)NULL);
		if (index >= count) index = 0;

		//change the selection
		m_bChanging = true;
		QModelIndex toSelect = m_model.index(index);
		pSelection->select(toSelect, QItemSelectionModel::ClearAndSelect);
		m_pItems->scrollTo(toSelect);
		m_bChanging = false;

		//notify any handlers
		RaiseChanged(eSelectReasonCode);
	}
}

//select item by name
void CAutoCompletePopup::Select(const QString& name)
{
	m_popupType = ePopupType_Match;
	QItemSelectionModel* pSelection = m_pItems->selectionModel();
	if (pSelection)
	{
		QModelIndex index = m_model.Find(name);
		if (index.isValid())
		{
			//change the selection
			m_bChanging = true;
			pSelection->select(index, QItemSelectionModel::ClearAndSelect);
			m_pItems->scrollTo(index);
			m_bChanging = false;

			//notify any handlers
			RaiseChanged(eSelectReasonCode);
		}
	}
}

//get current item by name
QString CAutoCompletePopup::GetSelectedItem()
{
	QItemSelectionModel* pSelection = m_pItems->selectionModel();
	if (pSelection)
	{
		QModelIndexList selection = pSelection->selectedIndexes();
		const QModelIndex* pIndex = !selection.empty() ? &selection.at(0) : NULL;

		if (pIndex)
		{
			const SAutoCompleteItem* pCurrent = m_model.Get(pIndex->row());

			if (pCurrent)
			{
				return pCurrent->name;
			}
		}
	}
	return QString();
}

QString CAutoCompletePopup::SelectNext()
{
	Select(1);
	return GetSelectedItem();
}


//handle item changes
void CAutoCompletePopup::HandleSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
	//this change is due to code, ignore it since the code is responsible for raising the event
	if (m_bChanging) return;

	//raise the event
	RaiseChanged(eSelectReasonClick);
}

//raise changed event
void CAutoCompletePopup::RaiseChanged(ESelectReason reason)
{
	QString newSelection = GetSelectedItem();
	if (m_lastSelection != newSelection)
	{
		selectionChanged(newSelection, reason);
		qSwap(m_lastSelection, newSelection);
	}
}

void CAutoCompletePopup::DisplayHistory()
{
	m_popupType = ePopupType_History;
	m_model.ClearItems();
	m_model.SetHistoryItems(m_HistoryItems);
	SetHistoryMatches(m_historyReply);
	Set(m_historyReply);

	QItemSelectionModel* pSelection = m_pItems->selectionModel();
	if (pSelection)
	{
		int lastItemNumber = m_model.GetItemsCount();
		if (lastItemNumber > 0)
		{
			QModelIndex toSelect = m_model.index(lastItemNumber - 1);
			m_bChanging = true;
			pSelection->select(toSelect, QItemSelectionModel::ClearAndSelect);
			m_pItems->scrollTo(toSelect);
			m_bChanging = false;
			show();
		}
	}
}

