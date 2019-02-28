// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySandbox/CrySignal.h>

#include <QAdvancedItemDelegate.h>

class CLineEditDelegate : public QAbstractItemDelegate
{
public:
	typedef std::function<string(const string&)> ValidateNameFunc;

public:
	CLineEditDelegate(QAbstractItemView* pParentView);

	void SetValidateNameFunc(const ValidateNameFunc& f);

	// QStyledItemDelegate implementation.
	virtual QWidget* createEditor(QWidget* pParent, const QStyleOptionViewItem& option, const QModelIndex& modelIndex) const override;

    // painting
    virtual void paint(QPainter *painter,
                       const QStyleOptionViewItem &option,
                       const QModelIndex &index) const override
	{
		m_pDelegate->paint(painter, option, index);
	}

    virtual QSize sizeHint(const QStyleOptionViewItem &option,
                           const QModelIndex &index) const
	{
		return m_pDelegate->sizeHint(option, index);
	}

    // editing

	virtual void destroyEditor(QWidget *editor, const QModelIndex &index) const
	{
		m_pDelegate->destroyEditor(editor, index);
	}

	virtual void setEditorData(QWidget *editor, const QModelIndex &index) const
	{
		m_pDelegate->setEditorData(editor, index);
	}

	virtual void setModelData(QWidget *editor,
		QAbstractItemModel *model,
		const QModelIndex &index) const
	{
		m_pDelegate->setModelData(editor, model, index);
	}

	virtual void updateEditorGeometry(QWidget *editor,
		const QStyleOptionViewItem &option,
		const QModelIndex &index) const
	{
		m_pDelegate->updateEditorGeometry(editor, option, index);
	}

    // for non-widget editors
	virtual bool editorEvent(QEvent *event,
		QAbstractItemModel *model,
		const QStyleOptionViewItem &option,
		const QModelIndex &index)
	{
		return m_pDelegate->editorEvent(event, model, option, index);
	}

	virtual bool helpEvent(QHelpEvent *event,
		QAbstractItemView *view,
		const QStyleOptionViewItem &option,
		const QModelIndex &index)
	{
		return m_pDelegate->helpEvent(event, view, option, index);
	}

    virtual QVector<int> paintingRoles() const
	{
		return m_pDelegate->paintingRoles();
	}

	CCrySignal<void(const QModelIndex&)> signalEditingFinished;
	CCrySignal<void(const QModelIndex&)> signalEditingAborted;

private:
	QAbstractItemView* m_pParentView;
	ValidateNameFunc m_validateNameFunc;
	QAbstractItemDelegate* m_pDelegate;
	int m_editRole;
};
