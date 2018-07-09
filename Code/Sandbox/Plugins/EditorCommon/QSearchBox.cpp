// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include <QSearchBox.h>

#include <QTimer>
#include <QAction>
#include <QSortFilterProxyModel>

#include "ProxyModels/DeepFilterProxyModel.h"
#include "CryIcon.h"

QSearchBox::QSearchBox(QWidget* parent /*= 0*/)
	: QLineEdit(parent)
	, m_timerMsec(0)
	, m_timer(nullptr)
	, m_displayClearButton(false)
{
	setPlaceholderText(tr("Search"));

	connect(this, &QSearchBox::editingFinished, this, &QSearchBox::OnSearch);
	connect(this, &QSearchBox::textChanged, this, &QSearchBox::OnTextChanged);

	m_action = addAction(CryIcon(QPixmap("icons:General/Search.ico")), QLineEdit::TrailingPosition);//TODO : make a real delete icon !!
	connect(m_action, &QAction::triggered, this, &QSearchBox::OnClear);
}

QSearchBox::~QSearchBox()
{

}

void QSearchBox::SetSearchFunction(std::function<void(const QString&)> function)
{
	m_searchFunction = function;
}

void QSearchBox::SetModel(QSortFilterProxyModel* model, SearchMode mode)
{
	if (model)
	{
		setEnabled(true);

		switch (mode)
		{
		case SearchMode::Wildcard:
			m_searchFunction = [model](const QString& str) { model->setFilterWildcard(str); };
			break;
		case SearchMode::Regexp:
			m_searchFunction = [model](const QString& str) { model->setFilterRegExp(str); };
			break;
		case SearchMode::TokenizedString:
			CRY_ASSERT(0);
			//intentional fallthrough
		case SearchMode::String:
		default:
			m_searchFunction = [model](const QString& str) { model->setFilterFixedString(str); };
			break;
		}
	}
	else
	{
		setEnabled(false);
		m_searchFunction = std::function<void(const QString&)>();
	}

}

void QSearchBox::SetModel(QDeepFilterProxyModel* model, SearchMode mode /*= SearchMode::TokenizedString*/)
{
	if (model)
	{
		setEnabled(true);

		switch (mode)
		{
		case SearchMode::Wildcard:
			m_searchFunction = [model](const QString& str) { model->setFilterWildcard(str); };
			break;
		case SearchMode::Regexp:
			m_searchFunction = [model](const QString& str) { model->setFilterRegExp(str); };
			break;
		case SearchMode::String:
			m_searchFunction = [model](const QString& str) { model->setFilterFixedString(str); };
			break;
		case SearchMode::TokenizedString:
		default:
			m_searchFunction = [model](const QString& str) { model->setFilterTokenizedString(str); };
			break;
		}
	}
	else
	{
		setEnabled(false);
		m_searchFunction = std::function<void(const QString&)>();
	}
}

void QSearchBox::SetAutoExpandOnSearch(QTreeView* treeView)
{
	//if need to detach, you can detach with the treeView pointer
	signalOnFiltered.Connect([this, treeView]() 
	{ 
		if(!text().isEmpty())
			treeView->expandAll(); 
	}, (uintptr_t)treeView);
}

void QSearchBox::EnableContinuousSearch(bool bEnable, float timerMsec /* =100 */)
{
	if (bEnable)
	{
		if (!m_timer)
		{
			m_timer = new QTimer(this);
			m_timer->setSingleShot(true);
			connect(m_timer, &QTimer::timeout, this, &QSearchBox::OnSearch);
		}

		m_timerMsec = timerMsec;
	}
	else if (m_timer)
	{
		delete m_timer;
		m_timer = nullptr;
	}
}

bool QSearchBox::IsEmpty() const
{
	return text().isEmpty();
}

void QSearchBox::OnTextChanged()
{
	if (m_timer)
	{
		m_timer->start(m_timerMsec);
	}

	if (text().isEmpty() && m_displayClearButton)
	{
		m_action->setIcon(CryIcon(QPixmap("icons:General/Search.ico")));
		m_displayClearButton = false;
	}
	else if (!text().isEmpty() && !m_displayClearButton)
	{
		m_action->setIcon(CryIcon(QPixmap("icons:General/Close.ico")));
		m_displayClearButton = true;
	}
}

void QSearchBox::OnSearch()
{
	if (m_timer)
	{
		m_timer->stop();
	}

	QString currentText = text();
	if (m_searchFunction && currentText != m_lastSearchedText)
	{
		m_lastSearchedText = currentText;
		m_searchFunction(text());
		signalOnFiltered();
	}
}

void QSearchBox::OnClear()
{
	clear();
}

