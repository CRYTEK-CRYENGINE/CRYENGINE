// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"

#include "Controls/EditorDialog.h"

class QDeepFilterProxyModel;
class QLineEdit;
class QModelIndex;
class QStandardItemModel;
class QStandardItem;
class QString;
class QAdvancedTreeView;
class QWidget;

//This is mostly used by the character tool, evaluate usability before reusing
class EDITOR_COMMON_API ListSelectionDialog : public CEditorDialog
{
	Q_OBJECT
public:
	ListSelectionDialog(const QString& dialogNameId, QWidget* parent);
	void        SetColumnText(int column, const char* text);
	void        SetColumnWidth(int column, int width);

	void        AddRow(const char* firstColumnValue);
	void        AddRow(const char* firstColumnValue, QIcon& icon);
	void        AddRowColumn(const char* value);

	const char* ChooseItem(const char* currentValue);

	QSize       sizeHint() const override;
protected slots:
	void        onActivated(const QModelIndex& index);
	void        onFilterChanged(const QString&);
protected:
	bool        eventFilter(QObject* obj, QEvent* event);
private:
	struct less_str : public std::binary_function<std::string, std::string, bool>
	{
		bool operator()(const std::string& left, const std::string& right) const { return _stricmp(left.c_str(), right.c_str()) < 0; }
	};

	QAdvancedTreeView*     m_tree;
	QStandardItemModel*    m_model;
	QDeepFilterProxyModel* m_filterModel;
	typedef std::map<std::string, QStandardItem*, less_str> StringToItem;
	StringToItem           m_firstColumnToItem;
	QLineEdit*             m_filterEdit;
	std::string            m_chosenItem;
	int                    m_currentColumn;
};

