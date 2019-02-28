// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "TrackViewEventsDialog.h"
#include "TrackViewPlugin.h"
#include "TrackViewSequenceManager.h"
#include "Nodes/TrackViewSequence.h"
#include "EditorFramework/Events.h"

#include "TrackViewPlugin.h"

#include "QtUtil.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QAdvancedTreeView.h>
#include <QLabel>
#include <QAbstractItemModel>
#include <QFormLayout>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QKeyEvent>
#include <QVariant>
#include <QStyledItemDelegate>
#include <QWidget>
#include <QEvent>

class CTrackViewSequenceEventsModel : public QAbstractItemModel
{
	struct SRow
	{
		string eventName;
		int32  usageCount;
		float  firstUsage;

		SRow()
			: usageCount(0)
			, firstUsage(.0f)
		{}
	};

public:
	struct EEventColumns
	{
		enum : int
		{
			Name,
			UsageCount,
			FirstUse,

			TotalCount
		};
	};

	CTrackViewSequenceEventsModel(CTrackViewSequence& sequence, QObject* pParent = nullptr)
		: m_sequence(sequence)
	{
		LoadEvents();
	}

	virtual ~CTrackViewSequenceEventsModel()
	{

	}

	// QAbstractItemModel
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override
	{
		if (!parent.isValid())
		{
			return m_dataRows.size();
		}
		return 0;
	}

	virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override
	{
		return EEventColumns::TotalCount;
	}

	virtual QVariant data(const QModelIndex& index, int role) const override
	{
		if (index.isValid() && index.row() < m_dataRows.size())
		{
			switch (role)
			{
			case Qt::DisplayRole:
			case Qt::EditRole:
				switch (index.column())
				{
				case EEventColumns::Name:
					return m_dataRows[index.row()].eventName.c_str();
				case EEventColumns::UsageCount:
					return m_dataRows[index.row()].usageCount;
				case EEventColumns::FirstUse:
					return m_dataRows[index.row()].firstUsage;
				}
				break;
			case Qt::DecorationRole:
			default:
				break;
			}
		}

		return QVariant();
	}

	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override
	{
		if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
		{
			switch (section)
			{
			case 0:
				return QObject::tr("Name");
			case 1:
				return QObject::tr("Usage Count");
			case 2:
				return QObject::tr("First Usage (Time)");
			default:
				break;
			}
		}
		return QVariant();
	}

	virtual Qt::ItemFlags flags(const QModelIndex& index) const override
	{
		if (index.isValid() && index.column() == EEventColumns::Name)
		{
			return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
		}
		return QAbstractItemModel::flags(index);
	}

	virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override
	{
		if (row >= 0 && row < m_dataRows.size())
		{
			return QAbstractItemModel::createIndex(row, column, reinterpret_cast<quintptr>(&m_dataRows[row]));
		}
		return QModelIndex();
	}

	virtual QModelIndex parent(const QModelIndex& index) const override
	{
		return QModelIndex();
	}

	virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override
	{
		if (role == Qt::EditRole)
		{
			if (index.isValid())
			{
				switch (index.column())
				{
				case EEventColumns::Name:
					{
						SRow& row = m_dataRows[index.row()];

						string newEventName = QtUtil::ToString(value.toString());
						m_sequence.RenameTrackEvent(row.eventName.c_str(), newEventName.c_str());
						row.eventName = newEventName;
					}
					break;
				default:
					return false;
				}
			}

			QVector<int> roles;
			roles.push_back(role);
			dataChanged(index, index, roles);

			return true;
		}

		return false;
	}

	virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override
	{
		if (row >= 0 && row + count <= m_dataRows.size())
		{
			beginRemoveRows(QModelIndex(), row, count);

			const std::vector<SRow>::iterator firstRow = m_dataRows.begin() + row;
			const std::vector<SRow>::iterator end = firstRow + count;
			for (std::vector<SRow>::iterator itr = firstRow; itr != end; ++itr)
			{
				m_sequence.RemoveTrackEvent(itr->eventName.c_str());
			}

			m_dataRows.erase(firstRow, end);

			endRemoveRows();

			return true;
		}

		return false;
	}
	// ~QAbstractItemModel

	void OnRowAdded()
	{
		const int32 idx = m_sequence.GetTrackEventsCount() - 1;
		beginInsertRows(QModelIndex(), idx, idx);

		SRow row;
		row.eventName = m_sequence.GetTrackEvent(idx);
		m_dataRows.emplace_back(row);

		endInsertRows();
	}

	void LoadEvents()
	{
		const int32 numPrevRows = max(static_cast<int32>(m_dataRows.size()) - 1, 0);
		m_dataRows.clear();

		SRow row;
		for (int32 i = 0, e = m_sequence.GetTrackEventsCount(); i < e; ++i)
		{
			row.eventName = m_sequence.GetTrackEvent(i);
			row.usageCount = 0;
			row.firstUsage = std::numeric_limits<float>::max();

			CTrackViewAnimNodeBundle nodeBundle = m_sequence.GetAnimNodesByType(eAnimNodeType_Event);

			const uint32 numNodes = nodeBundle.GetCount();
			for (uint32 currentNode = 0; currentNode < numNodes; ++currentNode)
			{
				CTrackViewAnimNode* pCurrentNode = nodeBundle.GetNode(currentNode);
				CTrackViewTrackBundle tracks = pCurrentNode->GetTracksByParam(CAnimParamType(eAnimParamType_TrackEvent));

				const uint32 numTracks = tracks.GetCount();
				for (uint32 currentTrack = 0; currentTrack < numTracks; ++currentTrack)
				{
					const CTrackViewTrack* pTrack = tracks.GetTrack(currentTrack);
					for (uint32 currentKey = 0; currentKey < pTrack->GetKeyCount(); ++currentKey)
					{
						CTrackViewKeyConstHandle keyHandle = pTrack->GetKey(currentKey);

						STrackEventKey key;
						keyHandle.GetKey(&key);
						if (key.m_event == row.eventName)
						{
							++row.usageCount;
							const float keyTime = keyHandle.GetTime().ToFloat();
							if (keyTime < row.firstUsage)
							{
								row.firstUsage = keyTime;
							}
						}
					}
				}
			}

			if (row.usageCount == 0)
			{
				row.firstUsage = .0f;
			}

			m_dataRows.emplace_back(row);
		}
	}

private:
	CTrackViewSequence& m_sequence;
	std::vector<SRow>   m_dataRows;
};

class CEventNameLineEdit : public QLineEdit
{
public:
	CEventNameLineEdit(CTrackViewSequence& sequence, QWidget* pParent)
		: QLineEdit(pParent)
		, m_sequence(sequence)
		, m_bIsValidName(false)
	{
		setFrame(true);
		QObject::connect(this, &QLineEdit::textChanged, this, &CEventNameLineEdit::OnTextChanged);
	}

	bool IsValidName() const
	{
		return m_bIsValidName;
	}

	void OnTextChanged(const QString& text)
	{
		m_bIsValidName = false;

		if (!text.isEmpty())
		{
			QByteArray byteArray = text.toLocal8Bit();
			CryStackStringT<char, 256> enteredName(byteArray.data());
			int32 eventNameCrc = CCrc32::ComputeLowercase(enteredName.c_str());

			for (int32 i = m_sequence.GetTrackEventsCount(); i--; )
			{
				if (CCrc32::ComputeLowercase(m_sequence.GetTrackEvent(i)) == eventNameCrc)
				{
					return;
				}
			}

			m_bIsValidName = true;
		}
	}

private:
	CTrackViewSequence& m_sequence;
	bool                m_bIsValidName;
};

class CEventNameEditDelegate : public QStyledItemDelegate
{
public:
	CEventNameEditDelegate(CTrackViewSequence& sequence, QObject* pParent = nullptr)
		: QStyledItemDelegate(pParent)
		, m_sequence(sequence)
	{}

	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		if (index.column() == CTrackViewSequenceEventsModel::EEventColumns::Name)
		{
			return new CEventNameLineEdit(m_sequence, parent);
		}
		else
		{
			return QStyledItemDelegate::createEditor(parent, option, index);
		}
	}

private:
	CTrackViewSequence& m_sequence;
};

CTrackViewNewEventDialog::CTrackViewNewEventDialog(CTrackViewSequence& sequence)
	: CEditorDialog("New Event")
	, m_sequence(sequence)
{
	QVBoxLayout* pDialogLayout = new QVBoxLayout(this);
	QFormLayout* pFormLayout = new QFormLayout();
	pDialogLayout->addLayout(pFormLayout);

	m_pEventName = new CEventNameLineEdit(m_sequence, this);
	QObject::connect(m_pEventName, &CEventNameLineEdit::textChanged, this, &CTrackViewNewEventDialog::OnTextChanged);
	pFormLayout->addRow(QObject::tr("&Event Name:"), m_pEventName);

	m_pWarning = new QLabel("Name is already in use!");
	m_pWarning->setHidden(true);
	pFormLayout->addRow("", m_pWarning);

	QDialogButtonBox* pButtonBox = new QDialogButtonBox(this);
	pButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	pDialogLayout->addWidget(pButtonBox);

	m_pOkButton = pButtonBox->button(QDialogButtonBox::Ok);
	m_pOkButton->setEnabled(false);

	QObject::connect(pButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	QObject::connect(pButtonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
}

string CTrackViewNewEventDialog::GetEventName() const
{
	return QtUtil::ToString(m_pEventName->text());
}

void CTrackViewNewEventDialog::OnTextChanged(const QString& text)
{
	if (!m_pEventName->text().isEmpty())
	{
		if (!m_pEventName->IsValidName())
		{
			m_pWarning->setVisible(true);
			m_pOkButton->setEnabled(false);
			return;
		}

		m_pOkButton->setEnabled(true);
		m_pWarning->setVisible(false);
	}
	else
	{
		m_pOkButton->setEnabled(false);
		m_pWarning->setVisible(false);
	}
}

CTrackViewEventsDialog::CTrackViewEventsDialog(CTrackViewSequence& sequence)
	: CEditorDialog("EventsDialog")
	, m_sequence(sequence)
{
	Initialize();
}

CTrackViewEventsDialog::~CTrackViewEventsDialog()
{

}

void CTrackViewEventsDialog::Initialize()
{
	m_pModel = new CTrackViewSequenceEventsModel(m_sequence, this);

	QVBoxLayout* pDialogLayout = new QVBoxLayout(this);

	m_pEventsView = new QAdvancedTreeView();
	m_pEventsView->setModel(m_pModel);
	m_pEventsView->setItemDelegate(new CEventNameEditDelegate(m_sequence));
	m_pEventsView->setEditTriggers(QTreeView::DoubleClicked | QTreeView::EditKeyPressed | QTreeView::SelectedClicked);
	m_pEventsView->setSelectionMode(QAbstractItemView::SingleSelection);
	m_pEventsView->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_pEventsView->setItemsExpandable(false);
	m_pEventsView->setRootIsDecorated(false);

	QObject::connect(m_pEventsView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CTrackViewEventsDialog::OnSelectionChanged);

	pDialogLayout->addWidget(m_pEventsView);

	QHBoxLayout* pButtonsLayout = new QHBoxLayout();

	m_pAddButton = new QPushButton("Add");

	m_pRemoveButton = new QPushButton("Remove");
	m_pRemoveButton->setEnabled(false);

	pButtonsLayout->addWidget(m_pAddButton);
	pButtonsLayout->addWidget(m_pRemoveButton);

	pDialogLayout->addLayout(pButtonsLayout);

	setMinimumSize(200, 500);
	setLayout(pDialogLayout);

	QObject::connect(m_pAddButton, &QPushButton::pressed, this, &CTrackViewEventsDialog::OnAddEventButtonPressed);
	QObject::connect(m_pRemoveButton, &QPushButton::pressed, this, &CTrackViewEventsDialog::OnRemoveEventButtonPressed);
}

void CTrackViewEventsDialog::OnAddEventButtonPressed()
{
	CTrackViewNewEventDialog dlg(m_sequence);
	if (dlg.exec())
	{
		m_sequence.AddTrackEvent(dlg.GetEventName().c_str());
		m_pModel->OnRowAdded();
		m_pEventsView->setCurrentIndex(m_pModel->index(m_sequence.GetTrackEventsCount() - 1, 0));
	}
}

void CTrackViewEventsDialog::OnRemoveEventButtonPressed()
{
	const QModelIndex index = m_pEventsView->currentIndex();
	if (index.isValid())
	{
		m_pModel->removeRows(index.row(), 1);
		m_pEventsView->setCurrentIndex(QModelIndex());
	}
}

void CTrackViewEventsDialog::OnSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
	m_pRemoveButton->setEnabled(!selected.empty());
}

void CTrackViewEventsDialog::keyPressEvent(QKeyEvent* pEvent)
{
	switch (pEvent->key())
	{
	case Qt::Key_Return:
	case Qt::Key_Enter:
		break;

	default:
		QDialog::keyPressEvent(pEvent);
	}
}

void CTrackViewEventsDialog::customEvent(QEvent* pEvent)
{
	if (pEvent->type() == SandboxEvent::Command)
	{
		CommandEvent* commandEvent = static_cast<CommandEvent*>(pEvent);
		const string& command = commandEvent->GetCommand();

		if (command == "general.delete")
		{
			OnRemoveEventButtonPressed();
		}
	}
	else
	{
		QWidget::customEvent(pEvent);
	}
}

