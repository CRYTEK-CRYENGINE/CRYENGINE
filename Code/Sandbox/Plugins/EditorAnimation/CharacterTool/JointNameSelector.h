// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Controls/EditorDialog.h"

class QDeepFilterProxyModel;
class QLabel;
class QLineEdit;
class QModelIndex;
class QStandardItemModel;
class QString;
class QAdvancedTreeView;
class QWidget;
struct IDefaultSkeleton;

class JointSelectionDialog : public CEditorDialog
{
	Q_OBJECT
public:
	JointSelectionDialog(QWidget* parent);
	QSize sizeHint() const override;

	bool  chooseJoint(string* name, IDefaultSkeleton* skeleton);
protected slots:
	void  onActivated(const QModelIndex& index);
	void  onFilterChanged(const QString&);
protected:
	bool  eventFilter(QObject* obj, QEvent* event);
private:
	QAdvancedTreeView*     m_tree;
	QStandardItemModel*    m_model;
	QDeepFilterProxyModel* m_filterModel;
	QLineEdit*             m_filterEdit;
	QLabel*                m_skeletonLabel;
};

