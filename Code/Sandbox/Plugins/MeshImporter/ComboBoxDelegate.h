// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Controls/QMenuComboBox.h>

#include <QItemDelegate>

class QAbstractItemView;
class QMenuComboBox;

class CComboBoxDelegate : public QItemDelegate
{
public:
	typedef std::function<void (QMenuComboBox* pEditor)> FillEditorFunc;

	CComboBoxDelegate(QAbstractItemView* pParentView = nullptr);

	void SetFillEditorFunction(const FillEditorFunc& func);

	// QItemDelegate implementation.

	virtual QWidget* createEditor(QWidget* pParent, const QStyleOptionViewItem& option, const QModelIndex& modelIndex) const override;
	virtual void setEditorData(QWidget* pEditorWidget, const QModelIndex& modelIndex) const override;
	virtual void setModelData(QWidget* pEditorWidget, QAbstractItemModel* pModel, const QModelIndex& modelIndex) const override;
	virtual void updateEditorGeometry(QWidget* pEditor, const QStyleOptionViewItem& option, const QModelIndex& modelIndex) const override;
	virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
private:
	QMenuComboBox* GetPaintHelper() const;

	QAbstractItemView* m_pParentView;
	FillEditorFunc     m_fillEditorFunc;
	mutable QSharedPointer<QMenuComboBox> m_pPaintHelper;
};
