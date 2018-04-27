// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 1999-2014.
// -------------------------------------------------------------------------
//  File name:   AutoCompletePopup.h
//  Version:     v1.00
//  Created:     03/03/2014 by Matthijs vd Meide
//  Compilers:   Visual Studio 2010
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include "Messages.h"
#include <QtWidgets/QLabel>
#include <QtWidgets/QListView>

//auto-complete dialog for the console
class CAutoCompletePopup : public QWidget
{
public:
	//the model of a single auto-complete item
	struct SAutoCompleteItem
	{
		QString display;
		QString name;
		enum EType
		{
			eType_Int,
			eType_Float,
			eType_String,
			eType_Function,
			eType_Count
		} type;

		//this will also interpret markup and set the type accordingly
		void Set(const char* text);
	};

private:
	//the model of all hints
	class CAutoCompleteModel : public QAbstractListModel
	{
		//the collection of auto-complete hints
		std::vector<SAutoCompleteItem> m_items;

		//icons for the hints
		QPixmap m_icons[SAutoCompleteItem::eType_Count];

	public:
		//constructor
		CAutoCompleteModel();

		//the number of rows in the model
		int rowCount(const QModelIndex& parent) const { return m_items.size(); };

		//retrieve data for an item
		QVariant data(const QModelIndex& index, int role) const;

		//get item by index
		const SAutoCompleteItem* Get(size_t index) const { return index < m_items.size() ? &m_items.at(index) : NULL; }

		//apply items
		void Set(const Messages::SAutoCompleteReply& reply);

		//find an item by name
		QModelIndex Find(const QString& name) const;

		void        ClearItems()    { m_items.clear(); }

		size_t      GetItemsCount() { return m_items.size(); };
		void        SetHistoryItems(std::vector<SAutoCompleteItem>& historyItems)
		{
			m_items.assign(historyItems.begin(), historyItems.end());
		}
	};

public:
	//the possible reasons of a selection change
	enum ESelectReason
	{
		eSelectReasonCode,
		eSelectReasonClick,
	};

	enum EPopupType
	{
		ePopupType_History,
		ePopupType_Match
	};

	//constructor
	CAutoCompletePopup(QWidget* parent);

	//destructor
	~CAutoCompletePopup() {}

	//populates the dialog with the specified options
	//returns true if teh window should be shown
	bool Set(const Messages::SAutoCompleteReply& reply);

	//select the next or previous item in the list
	void Select(int direction, bool selectCurrent = false);

	//select the item with the given name
	void Select(const QString& name);

	int GetDefaultHeight() { return m_defaultHeight; }

	//get the currently selected item
	QString    GetSelectedItem();
	QString    SelectNext();

	void       SetHistoryMatches(const Messages::SAutoCompleteReply item)        { m_model.Set(item); };
	void       AddHistoryMatch(Messages::SAutoCompleteReply::SItem newMatchItem) { m_historyReply.matches.push_back(newMatchItem); }

	void       DisplayHistory();
	void       AddHistoryItem(SAutoCompleteItem newItem) { m_HistoryItems.push_back(newItem); }

	EPopupType GetPopupType() const                      { return m_popupType; }

signals:
	//set after the selection changed, second parameter identifies the reason
	void selectionChanged(const QString& newselection, ESelectReason reason);

private:

	void SetupUI();
	//handle item selection change due ot user-input
	void HandleSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

	//raise changed event
	void RaiseChanged(ESelectReason reason);

	//the model for the collection to display
	CAutoCompleteModel m_model;

	//set if the selection is being changed through code
	bool m_bChanging;

	//the last selection
	QString                        m_lastSelection;

	QListView*                     m_pItems;
	QLabel*                        m_pHelpText;

	std::vector<SAutoCompleteItem> m_HistoryItems;
	Messages::SAutoCompleteReply   m_historyReply;

	EPopupType                     m_popupType;

	int m_defaultHeight;

	Q_OBJECT;
};

