// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QLineEdit>

#include "EditorCommonAPI.h"
#include <CrySandbox/CryFunction.h>
#include <CrySandbox/CrySignal.h>

class QSortFilterProxyModel;
class QDeepFilterProxyModel;
class QTreeView;

class EDITOR_COMMON_API QSearchBox : public QLineEdit
{
	Q_OBJECT
public:
	enum class SearchMode
	{
		String,
		Regexp,
		Wildcard,
		TokenizedString
	};

public:
	QSearchBox(QWidget* parent = 0);
	virtual ~QSearchBox();

	void SetSearchFunction(std::function<void(const QString&)> function);

	template<typename Object, typename Function>
	void SetSearchFunction(Object* object, Function function);

	template<typename Function>
	void SetSearchFunction(Function function);

	void SetModel(QSortFilterProxyModel* model, SearchMode mode = SearchMode::String);
	void SetModel(QDeepFilterProxyModel* model, SearchMode mode = SearchMode::TokenizedString);

	void SetAutoExpandOnSearch(QTreeView* treeView);

	//! When enabled, search will be updated after typing when the timer expires.
	//! When disabled, search will only update by pressing enter when done entering text
	//! Default is disabled
	void EnableContinuousSearch(bool bEnable, float timerMsec = 100);

	//! Returns true if there is an active search text in the searchBox
	bool IsEmpty() const;

	CCrySignal<void()> signalOnFiltered;

private:

	void OnTextChanged();
	void OnSearch();
	void OnClear();

	bool                                m_displayClearButton;
	QString								m_lastSearchedText;
	QAction*                            m_action;
	std::function<void(const QString&)> m_searchFunction;
	float                               m_timerMsec;
	QTimer*                             m_timer;
};

template<typename Object, typename Function>
void QSearchBox::SetSearchFunction(Object* object, Function function)
{
	SetSearchFunction(function_cast<void(const QString&)>(WrapMemberFunction(object, function)));
}

template<typename Function>
void QSearchBox::SetSearchFunction(Function function)
{
	SetSearchFunction(function_cast<void(const QString&)>(function));
}

