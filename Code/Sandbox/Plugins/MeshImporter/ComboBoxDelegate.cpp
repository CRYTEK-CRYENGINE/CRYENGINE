// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ComboBoxDelegate.h"
#include <QAbstractItemView>
#include <QPainter>

namespace Private_ComboBoxDelegate
{

static const int MARGIN = 2;

static bool ConvertToInt(const QModelIndex& modelIndex, int* value = nullptr)
{
	if (modelIndex.model()->data(modelIndex, Qt::EditRole).canConvert<int>())
	{
		if (value)
		{
			*value = modelIndex.model()->data(modelIndex, Qt::EditRole).toInt();
		}
		return true;
	}
	return false;
}

} // namespace

CComboBoxDelegate::CComboBoxDelegate(QAbstractItemView* pParentView)
	: QItemDelegate(pParentView)
	, m_pParentView(pParentView)
	, m_pPaintHelper(nullptr)
{
	assert(m_pParentView);
}

void CComboBoxDelegate::SetFillEditorFunction(const FillEditorFunc& func)
{
	auto clearAndFill = [func](QMenuComboBox* cb)
	{
		cb->Clear();
		func(cb);
	};
	m_fillEditorFunc = clearAndFill;

	m_pPaintHelper.clear();
}

QWidget* CComboBoxDelegate::createEditor(QWidget* pParent, const QStyleOptionViewItem& option, const QModelIndex& modelIndex) const
{
	QMenuComboBox* const pEditor = new QMenuComboBox(pParent);
	CRY_ASSERT(m_fillEditorFunc);
	m_fillEditorFunc(pEditor);

	QAbstractItemView* const pParentView = m_pParentView;
	pEditor->signalCurrentIndexChanged.Connect([pParentView, modelIndex](int index)
	{
		pParentView->model()->setData(modelIndex, index, Qt::EditRole);
	});

	pEditor->ShowPopup();

	return pEditor;
}

void CComboBoxDelegate::setEditorData(QWidget* pEditorWidget, const QModelIndex& modelIndex) const
{
	CRY_ASSERT(modelIndex.model()->data(modelIndex, Qt::EditRole).canConvert<int>());
	const int value = modelIndex.model()->data(modelIndex, Qt::EditRole).toInt();

	QMenuComboBox* const pEditor = static_cast<QMenuComboBox*>(pEditorWidget);
	CRY_ASSERT(value >= 0 && value < pEditor->GetItemCount());
	pEditor->SetChecked(value);
}

void CComboBoxDelegate::setModelData(QWidget* pEditorWidget, QAbstractItemModel* pModel, const QModelIndex& modelIndex) const
{
	QMenuComboBox* const pEditor = static_cast<QMenuComboBox*>(pEditorWidget);
	const int currentIndex = pEditor->GetCheckedItem();
	assert(currentIndex >= 0);  // Is single-selection?
	pModel->setData(modelIndex, currentIndex, Qt::EditRole);
}

void CComboBoxDelegate::updateEditorGeometry(QWidget* pEditorWidget, const QStyleOptionViewItem& option, const QModelIndex& modelIndex) const
{
	using namespace Private_ComboBoxDelegate;
	QMenuComboBox* const pEditor = static_cast<QMenuComboBox*>(pEditorWidget);
	pEditor->setGeometry(option.rect.adjusted(0, 0, -MARGIN, -MARGIN));
}

QMenuComboBox* CComboBoxDelegate::GetPaintHelper() const
{
	if (!m_pPaintHelper && m_fillEditorFunc)
	{
		m_pPaintHelper = QSharedPointer<QMenuComboBox>(new QMenuComboBox(), &QObject::deleteLater);
		m_fillEditorFunc(m_pPaintHelper.data());
	}
	return m_pPaintHelper.data();
}

void CComboBoxDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& modelIndex) const
{
	using namespace Private_ComboBoxDelegate;

	if (!ConvertToInt(modelIndex))
	{
		QItemDelegate::paint(painter, option, modelIndex);
		return;
	}

	QMenuComboBox* const pPaintHelper = GetPaintHelper();
	CRY_ASSERT(pPaintHelper);

	const int w = option.rect.size().width();
	const int h = option.rect.size().height();

	m_pPaintHelper->setGeometry(QRect(0, 0, w - MARGIN, h - MARGIN));

	if (modelIndex.data().canConvert<QString>())
	{
		const QString label = modelIndex.data().toString();
		m_pPaintHelper->Clear();
		m_pPaintHelper->AddItem(label);
	}
	else
	{
		m_fillEditorFunc(m_pPaintHelper.data());
		setEditorData(m_pPaintHelper.data(), modelIndex);
	}

	QPixmap pixmap(w, h);
	pixmap.fill(Qt::transparent);
	m_pPaintHelper->render(&pixmap);

	painter->drawPixmap(option.rect.x(), option.rect.y(), pixmap);
}

QSize CComboBoxDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& modelIndex) const
{
	using namespace Private_ComboBoxDelegate;

	if (!ConvertToInt(modelIndex))
	{
		return QItemDelegate::sizeHint(option, modelIndex);
	}

	QMenuComboBox* const pPaintHelper = GetPaintHelper();
	CRY_ASSERT(pPaintHelper);

	QWidget* const w = pPaintHelper;
	return w->sizeHint() + QSize(0, MARGIN);
}
