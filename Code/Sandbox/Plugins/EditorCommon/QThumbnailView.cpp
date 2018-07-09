// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "QThumbnailView.h"

#include <qevent.h> // contains sub events
#include <QListView>
#include <QBoxLayout>
#include <QDrag>
#include <QStyledItemDelegate>
#include <QDateTime>
#include <QButtonGroup>

#include "QtUtil.h"
#include "EditorFramework/Events.h"
#include "EditorFramework/PersonalizationManager.h"
#include "DragDrop.h"

namespace Private_QThumbnailView
{
	class CListView : public QListView
	{
	public:
		CListView(QWidget* pParent = nullptr)
			: QListView(pParent)
		{
		}

	protected:
		virtual void startDrag(Qt::DropActions supportedActions) override
		{
			if (model() && selectionModel())
			{
				QMimeData* const pMimeData = model()->mimeData(selectionModel()->selectedIndexes());
				CDragDropData::StartDrag(this, supportedActions, pMimeData);
			}
		}
	};
}

class QThumbnailsView::Delegate : public QStyledItemDelegate
{
public:
	Delegate(QThumbnailsView* view)
		: m_view(view)
		, m_loadingIcon("icons:Dialogs/dialog_loading.ico")
	{}

	void UpdateLoadingPixmap()
	{
		//TODO: could unify this so QLoading and all instances of Thumbnail view share the same pixmap
		QPixmap loading = m_loadingIcon.pixmap(m_loadingIcon.actualSize(m_view->GetItemSize()));
		auto pixmapSize = loading.size().width();//this assumes equal height and width

		QTransform rotation;
		rotation.translate(pixmapSize / 2, pixmapSize / 2);
		rotation.rotate(0.360 * (QDateTime::currentMSecsSinceEpoch() % 1000));
		rotation.translate(-pixmapSize / 2, -pixmapSize / 2);

		QPixmap rotatedPixmap = QPixmap(loading.size());
		rotatedPixmap.fill(Qt::transparent);

		QPainter painter(&rotatedPixmap);
		painter.setRenderHint(QPainter::SmoothPixmapTransform);
		painter.setTransform(rotation);
		painter.drawPixmap(QRect(0, 0, pixmapSize, pixmapSize), loading);

		m_rotatedLoading = CryIcon(rotatedPixmap);
	}

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
	{
		QVariant v = index.data(QThumbnailsView::s_ThumbnailRole);
		if (v.isValid())
		{
			if(v.type() == QVariant::Bool && !v.toBool())
			{
				const QWidget* widget = option.widget;
				QStyle* style = widget ? widget->style() : QApplication::style();

				QStyleOptionViewItem viewOpt(option);
				initStyleOption(&viewOpt, index);

				viewOpt.icon = m_rotatedLoading;
				style->drawControl(QStyle::CE_ItemViewItem, &viewOpt, painter, widget);

				m_view->StartUpdateTimer();
				return;
			}
			else if (v.type() == QVariant::Icon)
			{
				const QWidget* widget = option.widget;
				QStyle* style = widget ? widget->style() : QApplication::style();

				QStyleOptionViewItem viewOpt(option);
				initStyleOption(&viewOpt, index);

				viewOpt.icon = qvariant_cast<QIcon>(v);
				style->drawControl(QStyle::CE_ItemViewItem, &viewOpt, painter, widget);

				return;
			}
		}

		QStyledItemDelegate::paint(painter, option, index);
	}

	virtual void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override
	{
		QStyledItemDelegate::initStyleOption(option, index);

		//always force to use the decoration size that we want, even if the icon doesn't support it
		option->decorationSize = m_view->GetItemSize();
	}

private:

	QThumbnailsView* m_view;
	CryIcon m_loadingIcon;
	CryIcon m_rotatedLoading;
};


//TODO : expose the default, min and max item sizes in the stylesheet

QThumbnailsView::QThumbnailsView(QListView* pListView, bool showSizeButtons /* = true*/, QWidget* parent /*= nullptr*/)
	: QWidget(parent)
	, m_minItemSize(16, 16)
	, m_maxItemSize(256, 256)
	, m_timerStarted(false)
	, m_restoreSelection(true)
	, m_previewSizeButtons(nullptr)
{
	m_listView = pListView ? pListView : new Private_QThumbnailView::CListView();
	m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_listView, &QTreeView::customContextMenuRequested, [this]() { signalShowContextMenu(); });

	m_listView->viewport()->installEventFilter(this);

	m_pDelegate = new Delegate(this);
	m_listView->setItemDelegate(m_pDelegate);


	//Set default values for thumbnails View, optimizes large number of items
	m_listView->setUniformItemSizes(true);
	m_listView->setViewMode(QListView::IconMode);
	m_listView->setFlow(QListView::LeftToRight);
	m_listView->setWrapping(true);
	m_listView->setResizeMode(QListView::Adjust);
	m_listView->setSelectionRectVisible(true);
	m_listView->setMovement(QListView::Static);

	//For some reason the cell is still grown by text. Ideally we would not allow growing past cell size, instead we would elide the text
	m_listView->setWordWrap(true);

	//Makes the layout faster (smoother resizes) but the scrollbar may need some time to adjust. Seems to be a better experience for bigger data sets (>10k)
	m_listView->setLayoutMode(QListView::Batched);
	m_listView->setBatchSize(500);// items that must be laid out in one go, default value is 100

	//This means the layout takes a lot longer, but the scrollbar is accurate
	//setLayoutMode(QListView::SinglePass);

	auto layout = new QVBoxLayout();
	layout->setSpacing(0);
	layout->setMargin(0);
	layout->addWidget(m_listView);

	if (showSizeButtons)
	{
		m_previewSizeButtons = new QButtonGroup(this);
		for (int i = 0; i < m_numOfPreviewSizes; i++)
		{
			AddSizingButton(16 << i, i);
		}

		//TODO : add item count

		//layout and content
		auto bottomBox = new QHBoxLayout();
		bottomBox->setMargin(0);
		bottomBox->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
		QList<QAbstractButton*> buttons = m_previewSizeButtons->buttons();
		for each (QAbstractButton* button in buttons)
		{
			bottomBox->addWidget(button);
		}
		layout->addLayout(bottomBox);
	}

	setLayout(layout);
	
	SetState(GET_PERSONALIZATION_STATE(QThumbnailsView));
}

QThumbnailsView::~QThumbnailsView()
{
	m_pDelegate->deleteLater();
}

bool QThumbnailsView::SetItemSize(const QSize& size)
{
	if (size.width() >= m_minItemSize.width() && size.width() <= m_maxItemSize.width() 
		&& size.height() >= m_minItemSize.height() && size.height() <= m_maxItemSize.height())
	{
		m_itemSize = size;
		m_listView->setIconSize(size);

		//Either set correct Button as checked or remove checked state
		if (m_previewSizeButtons)
		{
			QAbstractButton* button = m_previewSizeButtons->button(size.height());
			if (size.height() == size.width() && button)
				button->setChecked(true);
			else
			{
				button = m_previewSizeButtons->checkedButton();
				if (button)
					button->setChecked(false);
			}
		}

		//change scale but try to always keep the zoomed-on item in the center
		if(m_listView->selectionModel() && m_listView->selectionModel()->currentIndex().isValid())
			ScrollToRow(m_listView->selectionModel()->currentIndex(), QAbstractItemView::PositionAtCenter);

		//m_listView->scheduleDelayedItemsLayout();

		return true;
	}
	return false;
}

void QThumbnailsView::SetItemSizeBounds(const QSize& min, const QSize& max)
{
	m_minItemSize = min;
	m_maxItemSize = max;

	//will update slider and set item size to something within bounds
	if (!SetItemSize(GetItemSize()))
		SetItemSize(m_minItemSize);
}

void QThumbnailsView::SetSpacing(int space)
{
	m_listView->setSpacing(space);
}

int QThumbnailsView::GetSpacing() const
{
	return m_listView->spacing();
}

void QThumbnailsView::SetRootIndex(const QModelIndex& index)
{
	m_listView->setRootIndex(index);
}

QModelIndex QThumbnailsView::GetRootIndex() const
{
	return m_listView->rootIndex();
}

void QThumbnailsView::SetDataColumn(int column)
{
	m_listView->setModelColumn(column);
}

bool QThumbnailsView::SetDataAttribute(const CItemModelAttribute* attribute /*= &Attributes::s_thumbnailAttribute*/, int attributeRole /*= Attributes::s_getAttributeRole*/)
{
	//TODO : handle model changes after this is set
	CRY_ASSERT(attribute);

	auto pModel = m_listView->model();
	CRY_ASSERT(pModel);//attribute cannot be set before model

	const int columnCount = pModel->columnCount(GetRootIndex());
	for (int i = 0; i < columnCount; i++)
	{
		QVariant value = pModel->headerData(i, Qt::Horizontal, attributeRole);
		CItemModelAttribute* pAttribute = value.value<CItemModelAttribute*>();
		if (pAttribute == attribute)
		{
			SetDataColumn(i);
			return true;
		}
	}

	return false;
}

void QThumbnailsView::SetModel(QAbstractItemModel* model)
{
	QAbstractItemModel* old = m_listView->model();

	if (old != model)
	{
		if (old)
		{
			disconnect(old, &QAbstractItemModel::modelAboutToBeReset, this, &QThumbnailsView::OnModelAboutToBeReset);
			disconnect(old, &QAbstractItemModel::modelReset, this, &QThumbnailsView::OnModelReset);
		}

		//TODO : connect to the model to show item count
		m_listView->setModel(model);
		SetDataAttribute();

		if (model)
		{
			connect(model, &QAbstractItemModel::modelAboutToBeReset, this, &QThumbnailsView::OnModelAboutToBeReset);
			connect(model, &QAbstractItemModel::modelReset, this, &QThumbnailsView::OnModelReset);
		}
	}
}

void QThumbnailsView::ScrollToRow(const QModelIndex& indexInRow, QAbstractItemView::ScrollHint scrollHint)
{
	if(indexInRow.isValid())
	{
		CRY_ASSERT(indexInRow.model() == m_listView->model());
		m_listView->scrollTo(indexInRow.sibling(indexInRow.row(), m_listView->modelColumn()), scrollHint);
	}
}

void QThumbnailsView::AppendPreviewSizeActions(CAbstractMenu& menu)
{
	for (int i = 0; i < m_numOfPreviewSizes; ++i)
	{
		const int size = 16 << i;
		QString icon = QString("icons:common/general_preview_size_%1.ico").arg(i);
		QAction* pAction = menu.CreateAction(CryIcon(icon), QString("%1").arg(size));
		connect(pAction, &QAction::triggered, [this, size]()
		{ 
			OnPreviewSizeButtonClicked(size); 
		});
	}
}

bool QThumbnailsView::eventFilter(QObject* pObject, QEvent* pEvent)
{
	switch (pEvent->type())
	{
	case QEvent::Paint:
	{
		m_pDelegate->UpdateLoadingPixmap();
		m_timerStarted = false;
		return false;
	}
	case QEvent::Wheel:
		if (QtUtil::IsModifierKeyDown(Qt::ControlModifier))
		{
			QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(pEvent);
			//a delta of 120 is standard, 15 degrees angle, positive is mousewheel up, negative is down
			//wheel up zooms, wheel down dezooms
			auto size = GetItemSize();

			auto angleDelta = wheelEvent->angleDelta();
			float y = angleDelta.y();
			if (y >= 0)
				size *= 1.2f * y / 120;
			else
				size /= -1.2f * y / 120;

			SetItemSize(size);
			return true;
		}
		else
			return false;
	default:
		return false;
	}

}

void QThumbnailsView::customEvent(QEvent* pEvent)
{
	if (pEvent->type() == SandboxEvent::Command)
	{
		CommandEvent* commandEvent = static_cast<CommandEvent*>(pEvent);
		const string& command = commandEvent->GetCommand();

		if (command == "general.zoom_in")
		{
			SetItemSize(GetItemSize() * 1.2f);
			commandEvent->setAccepted(true);
		}
		else if (command == "general.zoom_out")
		{
			SetItemSize(GetItemSize() / 1.2f);
			commandEvent->setAccepted(true);
		}
	}
	else
	{
		QWidget::customEvent(pEvent);
	}
}

void QThumbnailsView::OnModelAboutToBeReset()
{
	if (!m_restoreSelection)
		return;

	QAdvancedTreeView::BackupSelection(m_selectedBackup, m_listView->model(), m_listView->selectionModel());
}

void QThumbnailsView::OnModelReset()
{
	if (!m_restoreSelection)
		return;

	QAdvancedTreeView::RestoreSelection(m_selectedBackup, m_listView->model(), m_listView->selectionModel());
}

void QThumbnailsView::OnPreviewSizeButtonClicked(int value)
{
	SetItemSize(QSize(value,value));
	SaveState();
}

QVariantMap QThumbnailsView::GetState() const
{
	QVariantMap varMap;
	varMap["size"] = QtUtil::ToQVariant(GetItemSize());

	return varMap;
}

void QThumbnailsView::SetState(const QVariantMap& state)
{
	QVariant size = state["size"];

	if (size.isValid())
	{
		SetItemSize(QtUtil::ToQSize(size));
	}
	else // set default value if one was not found
	{
		SetItemSize(QSize(64, 64));
	}
}

void QThumbnailsView::SaveState() const
{
	SET_PERSONALIZATION_PROPERTY(QThumbnailsView, "size", QtUtil::ToQVariant(GetItemSize()));
}

void QThumbnailsView::StartUpdateTimer()
{
	if (!m_timerStarted)
	{
		QTimer::singleShot(10, [this]() { m_listView->update(); });
		m_timerStarted = true;
	}
	
}

void QThumbnailsView::AddSizingButton(int size, int iconNumber)
{
	QToolButton* m_button = new QToolButton;
	connect(m_button, &QToolButton::clicked, this, [=]() { OnPreviewSizeButtonClicked(size); });
	QString icon = QString("icons:common/general_preview_size_%1.ico").arg(iconNumber);
	m_button->setIcon(CryIcon(icon));
	m_button->setCheckable(true);
	m_button->setAutoRaise(true);
	m_previewSizeButtons->addButton(m_button, size);
}


