// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "QDialogLineDatabaseModel.h"
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
#include "IResourceSelectorHost.h"
#include <CrySystem/ISystem.h>
#include "IEditor.h"
#include <QtUtil.h>
#include <EditorStyleHelper.h>
#include "Controls/QMenuComboBox.h"

#include <QEvent>
#include <QApplication>
#include <QPixmap>



enum class EColumns
{
	ID = 0,
	PRIORITY,
	FLAGS,
	START_TRIGGER,
	END_TRIGGER,
	SUBTITLE,
	LIPSYNC_ANIMATION,
	STANDALONE_FILE,
	PAUSE,
	CUSTOM_DATA,
	MAX_QUEUING_DURATION,
	SIZE
};

namespace
{
static const QString s_randomSelectionMode("RandomVariation");
static const QString s_sequentialSelectionMode("SequentialVariationsRepeatAll");
static const QString s_sequentialSelectionModeClamp("SequentialVariationsClampLast");
static const QString s_sequentialSelectionSuccessivelyMode("SequentialAll");
static const QString s_sequentialSelectionModeAllOnlyOnce("SequentialOnlyOnce");
}

struct SLineItem
{
	SLineItem(SLineItem* parent) : pParent(parent) {}
	~SLineItem()
	{
		for (SLineItem* pItem : child)
		{
			delete pItem;
		}
		child.clear();
	}

	SLineItem*              pParent;
	std::vector<SLineItem*> child;

	void                    DeleteChild(uint index)
	{
		if (index < child.size())
		{
			delete child[index];
			child.erase(child.begin() + index);
		}
	}
};

//--------------------------------------------------------------------------------------------------
QDialogLineDatabaseModel::QDialogLineDatabaseModel(QObject* pParentWidget)
	: QAbstractItemModel(pParentWidget)
	, m_pRoot(new SLineItem(nullptr))
{
	m_pDatabase = gEnv->pDynamicResponseSystem->GetDialogLineDatabase();
	ForceDataReload();
}

//--------------------------------------------------------------------------------------------------
QDialogLineDatabaseModel::~QDialogLineDatabaseModel()
{
	delete m_pRoot;
}

//--------------------------------------------------------------------------------------------------
EItemType QDialogLineDatabaseModel::ItemType(const QModelIndex& index) const
{
	if (index.isValid())
	{
		if (index.parent().isValid())
		{
			return EItemType::DIALOG_LINE;
		}
		return EItemType::DIALOG_LINE_SET;
	}
	return EItemType::INVALID;
}

//--------------------------------------------------------------------------------------------------
bool QDialogLineDatabaseModel::CanEdit(const QModelIndex& index) const
{
	EItemType itemType = ItemType(index);
	EColumns column = (EColumns)index.column();

	if (itemType == EItemType::DIALOG_LINE_SET)
	{
		return true;
	}
	else if (itemType == EItemType::DIALOG_LINE)
	{
		return (column == EColumns::SUBTITLE || column == EColumns::START_TRIGGER || column == EColumns::END_TRIGGER || column == EColumns::LIPSYNC_ANIMATION || column == EColumns::STANDALONE_FILE || column == EColumns::PAUSE || column == EColumns::CUSTOM_DATA || column == EColumns::MAX_QUEUING_DURATION);
	}
	return false;
}

//--------------------------------------------------------------------------------------------------
int QDialogLineDatabaseModel::rowCount(const QModelIndex& parent) const
{
	SLineItem* pItem = ItemFromIndex(parent);
	return pItem ? pItem->child.size() : 0;
}

//--------------------------------------------------------------------------------------------------
int QDialogLineDatabaseModel::columnCount(const QModelIndex& parent) const
{
	return (int)EColumns::SIZE;
}

//--------------------------------------------------------------------------------------------------
QVariant QDialogLineDatabaseModel::data(const QModelIndex& index, int role) const
{
	if (m_pDatabase && index.isValid())
	{
		switch (role)
		{
		case Qt::ToolTipRole:
			switch ((EColumns)index.column())
			{
			case EColumns::ID:
				return tr("Unique ID for the line.");
			case EColumns::SUBTITLE:
				return tr("Text used as subtitle.");
			case EColumns::PRIORITY:
				return tr("Priority of the line. Higher priority lines will interrupt lower priority ones.");
			case EColumns::FLAGS:
				return tr("Criteria to pick line variations. Random: Every time pick a line at random. Sequential: Pick lines in the order they are defined.");
			case EColumns::START_TRIGGER:
				return tr("ATL audio trigger executed when playing the line. Usually used to play the dialog written in the subtitles.");
			case EColumns::END_TRIGGER:
				return tr("ATL audio trigger executed if the line is interrupted.");
			case EColumns::LIPSYNC_ANIMATION:
				return tr("Lipsync animation that will be played when the line is started.");
			case EColumns::STANDALONE_FILE:
				return tr("The standalone file played for this file.");
			case EColumns::PAUSE:
				return tr("The length of the additional pause after the line has finished (in seconds).");
			case EColumns::CUSTOM_DATA:
				return tr("Additional game specific data associated with the line.");
			case EColumns::MAX_QUEUING_DURATION:
				return tr("Max time in seconds that this line can be queued if another (more important line) is currently playing. After this time, the line will be skipped");
			}
			break;
		case Qt::BackgroundRole:
			if (ItemType(index) == EItemType::DIALOG_LINE)
			{
				return GetStyleHelper()->alternateBaseColor();
			}
			break;
		case Qt::DecorationRole:
			if (index.column() == 0 && ItemType(index) == EItemType::DIALOG_LINE_SET)
			{
				return QPixmap("icons:Dialog/vocals16.ico");
			}
			break;
		case Qt::DisplayRole:
		case Qt::EditRole:
			{
				if (ItemType(index) == EItemType::DIALOG_LINE)
				{
					DRS::IDialogLineSet* pDialogLineSet = m_pDatabase->GetLineSetByIndex(index.parent().row());
					if (pDialogLineSet)
					{
						DRS::IDialogLine* pLine = pDialogLineSet->GetLineByIndex(index.row() + 1); // +1 because the data for the first line is actually displayed with the parent
						if (pLine)
						{
							switch ((EColumns)index.column())
							{
							case EColumns::SUBTITLE:
								return QtUtil::ToQStringSafe(pLine->GetText());
							case EColumns::START_TRIGGER:
								return QtUtil::ToQStringSafe(pLine->GetStartAudioTrigger());
							case EColumns::END_TRIGGER:
								return QtUtil::ToQStringSafe(pLine->GetEndAudioTrigger());
							case EColumns::LIPSYNC_ANIMATION:
								return QtUtil::ToQStringSafe(pLine->GetLipsyncAnimation());
							case EColumns::STANDALONE_FILE:
								return QtUtil::ToQStringSafe(pLine->GetStandaloneFile());
							case EColumns::PAUSE:
								return QtUtil::ToQStringSafe(CryStringUtils::toString(pLine->GetPauseLength()));
							case EColumns::CUSTOM_DATA:
								return QtUtil::ToQStringSafe(pLine->GetCustomData());
							}
						}
					}
				}
				else
				{
					DRS::IDialogLineSet* pDialogLineSet = m_pDatabase->GetLineSetByIndex(index.row());
					if (pDialogLineSet)
					{
						switch ((EColumns)index.column())
						{
						case EColumns::ID:
							return QtUtil::ToQStringSafe(pDialogLineSet->GetLineId().GetText());
						case EColumns::PRIORITY:
							return QtUtil::ToQStringSafe(CryStringUtils::toString(pDialogLineSet->GetPriority()));
						case EColumns::FLAGS:
							{
								uint32 lineFlags = pDialogLineSet->GetFlags();
								if ((lineFlags& DRS::IDialogLineSet::EPickModeFlags_RandomVariation) > 0)
									return s_randomSelectionMode;
								else if ((lineFlags& DRS::IDialogLineSet::EPickModeFlags_SequentialVariationRepeat) > 0)
									return s_sequentialSelectionMode;
								else if ((lineFlags& DRS::IDialogLineSet::EPickModeFlags_SequentialVariationClamp) > 0)
									return s_sequentialSelectionModeClamp;
								else if ((lineFlags& DRS::IDialogLineSet::EPickModeFlags_SequentialAllSuccessively) > 0)
									return s_sequentialSelectionSuccessivelyMode;
								else if ((lineFlags& DRS::IDialogLineSet::EPickModeFlags_SequentialVariationOnlyOnce) > 0)
									return s_sequentialSelectionModeAllOnlyOnce;
							}
							break;
						case EColumns::MAX_QUEUING_DURATION:
							return QtUtil::ToQStringSafe(CryStringUtils::toString(pDialogLineSet->GetMaxQueuingDuration()));
						case EColumns::SUBTITLE:
							if (pDialogLineSet->GetLineCount() > 0)
							{
								return QtUtil::ToQStringSafe(pDialogLineSet->GetLineByIndex(0)->GetText());
							}
							break;
						case EColumns::START_TRIGGER:
							if (pDialogLineSet->GetLineCount() > 0)
							{
								return QtUtil::ToQStringSafe(pDialogLineSet->GetLineByIndex(0)->GetStartAudioTrigger());
							}
							break;
						case EColumns::END_TRIGGER:
							if (pDialogLineSet->GetLineCount() > 0)
							{
								return QtUtil::ToQStringSafe(pDialogLineSet->GetLineByIndex(0)->GetEndAudioTrigger());
							}
							break;
						case EColumns::LIPSYNC_ANIMATION:
							if (pDialogLineSet->GetLineCount() > 0)
							{
								return QtUtil::ToQStringSafe(pDialogLineSet->GetLineByIndex(0)->GetLipsyncAnimation());
							}
							break;
						case EColumns::STANDALONE_FILE:
							if (pDialogLineSet->GetLineCount() > 0)
							{
								return QtUtil::ToQStringSafe(pDialogLineSet->GetLineByIndex(0)->GetStandaloneFile());
							}
							break;
						case EColumns::PAUSE:
							if (pDialogLineSet->GetLineCount() > 0)
							{
								return QtUtil::ToQStringSafe(CryStringUtils::toString(pDialogLineSet->GetLineByIndex(0)->GetPauseLength()));
							}
							break;
						case EColumns::CUSTOM_DATA:
							if (pDialogLineSet->GetLineCount() > 0)
							{
								return QtUtil::ToQStringSafe(pDialogLineSet->GetLineByIndex(0)->GetCustomData());
							}
							break;
						}
					}
				}
			}
			break;
		}
	}
	return QVariant();
}

//--------------------------------------------------------------------------------------------------
bool QDialogLineDatabaseModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	if (m_pDatabase && index.isValid() && role == Qt::EditRole)
	{
		uint row = index.row();
		if (ItemType(index) == EItemType::DIALOG_LINE)
		{
			DRS::IDialogLineSet* pDialogLineSet = m_pDatabase->GetLineSetByIndex(index.parent().row());
			if (pDialogLineSet)
			{
				DRS::IDialogLine* pLine = pDialogLineSet->GetLineByIndex(row + 1); // +1 because the data for the first line is actually displayed with the parent
				if (pLine)
				{
					switch ((EColumns)index.column())
					{
					case EColumns::SUBTITLE:
						pLine->SetText(QtUtil::ToString(value.toString()));
						break;
					case EColumns::START_TRIGGER:
						pLine->SetStartAudioTrigger(QtUtil::ToString(value.toString()));
						break;
					case EColumns::END_TRIGGER:
						pLine->SetEndAudioTrigger(QtUtil::ToString(value.toString()));
						break;
					case EColumns::LIPSYNC_ANIMATION:
						pLine->SetLipsyncAnimation(QtUtil::ToString(value.toString()));
						break;
					case EColumns::STANDALONE_FILE:
						pLine->SetStandaloneFile(QtUtil::ToString(value.toString()));
						break;
					case EColumns::PAUSE:
						pLine->SetPauseLength(value.toFloat());
						break;
					case EColumns::CUSTOM_DATA:
						pLine->SetCustomData(QtUtil::ToString(value.toString()));
						break;
					}
				}
			}
		}
		else
		{
			DRS::IDialogLineSet* pDialogLineSet = m_pDatabase->GetLineSetByIndex(row);
			if (pDialogLineSet)
			{
				switch ((EColumns)index.column())
				{
				case EColumns::ID:
					{
						// Check that a line with that ID does not already exist
						const CHashedString id(QtUtil::ToString(value.toString()));
						if (!id.IsValid() || m_pDatabase->GetLineSetById(id))
						{
							return false;
						}
						pDialogLineSet->SetLineId(id);
					}
					break;
				case EColumns::PRIORITY:
					pDialogLineSet->SetPriority(value.toInt());
					break;
				case EColumns::FLAGS:
					{
						QString selectionMode = value.toString();
						if (selectionMode == s_randomSelectionMode)
						{
							pDialogLineSet->SetFlags(DRS::IDialogLineSet::EPickModeFlags_RandomVariation);
						}
						else if (selectionMode == s_sequentialSelectionMode)
						{
							pDialogLineSet->SetFlags(DRS::IDialogLineSet::EPickModeFlags_SequentialVariationRepeat);
						}
						else if (selectionMode == s_sequentialSelectionModeClamp)
						{
							pDialogLineSet->SetFlags(DRS::IDialogLineSet::EPickModeFlags_SequentialVariationClamp);
						}
						else if (selectionMode == s_sequentialSelectionSuccessivelyMode)
						{
							pDialogLineSet->SetFlags(DRS::IDialogLineSet::EPickModeFlags_SequentialAllSuccessively);
						}
						else if (selectionMode == s_sequentialSelectionModeAllOnlyOnce)
						{
							pDialogLineSet->SetFlags(DRS::IDialogLineSet::EPickModeFlags_SequentialVariationOnlyOnce);
						}
					}
					break;
				case EColumns::MAX_QUEUING_DURATION:
					pDialogLineSet->SetMaxQueuingDuration(value.toFloat());
					break;
				case EColumns::SUBTITLE:
					if (pDialogLineSet->GetLineCount() > 0)
					{
						pDialogLineSet->GetLineByIndex(0)->SetText(QtUtil::ToString(value.toString()));
					}
					break;
				case EColumns::START_TRIGGER:
					if (pDialogLineSet->GetLineCount() > 0)
					{
						pDialogLineSet->GetLineByIndex(0)->SetStartAudioTrigger(QtUtil::ToString(value.toString()));
					}
					break;
				case EColumns::END_TRIGGER:
					if (pDialogLineSet->GetLineCount() > 0)
					{
						pDialogLineSet->GetLineByIndex(0)->SetEndAudioTrigger(QtUtil::ToString(value.toString()));
					}
					break;
				case EColumns::LIPSYNC_ANIMATION:
					if (pDialogLineSet->GetLineCount() > 0)
					{
						pDialogLineSet->GetLineByIndex(0)->SetLipsyncAnimation(QtUtil::ToString(value.toString()));
					}
					break;
				case EColumns::STANDALONE_FILE:
					if (pDialogLineSet->GetLineCount() > 0)
					{
						pDialogLineSet->GetLineByIndex(0)->SetStandaloneFile(QtUtil::ToString(value.toString()));
					}
					break;
				case EColumns::PAUSE:
					if (pDialogLineSet->GetLineCount() > 0)
					{
						pDialogLineSet->GetLineByIndex(0)->SetPauseLength(value.toFloat());
					}
					break;
				case EColumns::CUSTOM_DATA:
					if (pDialogLineSet->GetLineCount() > 0)
					{
						pDialogLineSet->GetLineByIndex(0)->SetCustomData(QtUtil::ToString(value.toString()));
					}
					break;
				}
			}
		}
		dataChanged(index, index);
		return true;
	}
	return false;
}

//--------------------------------------------------------------------------------------------------
QVariant QDialogLineDatabaseModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
	{
		switch ((EColumns)section)
		{
		case EColumns::ID:
			return tr("ID");
		case EColumns::SUBTITLE:
			return tr("Subtitle");
		case EColumns::PRIORITY:
			return tr("Priority");
		case EColumns::FLAGS:
			return tr("Selection Mode");
		case EColumns::START_TRIGGER:
			return tr("Line Start Trigger");
		case EColumns::END_TRIGGER:
			return tr("Line Interrupted Trigger");
		case EColumns::LIPSYNC_ANIMATION:
			return tr("Lipsync animation");
		case EColumns::STANDALONE_FILE:
			return tr("Standalone file");
		case EColumns::PAUSE:
			return tr("After line Pause");
		case EColumns::CUSTOM_DATA:
			return tr("Custom data");
		case EColumns::MAX_QUEUING_DURATION:
			return tr("Max Queuing Duration");
		}
	}
	return QAbstractItemModel::headerData(section, orientation, role);
}

//--------------------------------------------------------------------------------------------------
Qt::ItemFlags QDialogLineDatabaseModel::flags(const QModelIndex& index) const
{
	Qt::ItemFlags f = QAbstractItemModel::flags(index);
	if (index.isValid() && CanEdit(index))
	{
		f |= Qt::ItemIsEditable;
	}
	return f;
}

//--------------------------------------------------------------------------------------------------
bool QDialogLineDatabaseModel::insertRows(int row, int count, const QModelIndex& parent)
{
	SLineItem* pParent = ItemFromIndex(parent);
	if (!pParent)
	{
		return false;
	}

	beginInsertRows(parent, row, row + count - 1);

	if (ItemType(parent) == EItemType::DIALOG_LINE_SET)
	{
		// Adding a line variation
		DRS::IDialogLineSet* pLineSet = m_pDatabase->GetLineSetByIndex(parent.row());
		if (pLineSet)
		{
			for (int i = 0; i < count; ++i)
			{
				pLineSet->InsertLine(row + 1);
				pParent->child.insert(pParent->child.begin() + row, new SLineItem(pParent));
			}
		}
	}
	else
	{
		for (int i = 0; i < count; ++i)
		{
			DRS::IDialogLineSet* pLineSet = m_pDatabase->InsertLineSet(row);
			if (pLineSet)
			{
				pLineSet->InsertLine();
				SLineItem* pItem = new SLineItem(pParent);
				for (int i = 0; i < pLineSet->GetLineCount() - 1; ++i)
				{
					pItem->child.push_back(new SLineItem(pItem));
				}
				pParent->child.insert(pParent->child.begin() + row, pItem);
			}
		}
	}

	endInsertRows();

	return true;
}

//--------------------------------------------------------------------------------------------------
bool QDialogLineDatabaseModel::removeRows(int row, int count, const QModelIndex& parent)
{
	SLineItem* pParent = ItemFromIndex(parent);
	if (pParent)
	{
		beginRemoveRows(parent, row, row + count - 1);
		if (ItemType(parent) == EItemType::DIALOG_LINE_SET)
		{
			// Deleting specific line
			DRS::IDialogLineSet* pLineSet = m_pDatabase->GetLineSetByIndex(parent.row());
			if (pLineSet)
			{
				for (int i = 0; i < count; ++i)
				{
					pLineSet->RemoveLine(row + 1);
					pParent->DeleteChild(row);
				}
			}
		}
		else
		{
			// Deleting a whole line set
			const uint lineCount = m_pDatabase->GetLineSetCount();
			if (row < lineCount)
			{
				if (row + count > lineCount)
				{
					count = lineCount - row;
				}
				for (int i = 0; i < count; ++i)
				{
					pParent->DeleteChild(row);
					m_pDatabase->RemoveLineSet(row);
				}
			}
		}
		endRemoveRows();
		return true;
	}
	return false;
}

//--------------------------------------------------------------------------------------------------
QModelIndex QDialogLineDatabaseModel::index(int row, int column, const QModelIndex& parent) const
{
	if ((row >= 0) && (column >= 0))
	{
		const SLineItem* pParentItem = ItemFromIndex(parent);
		if (pParentItem)
		{
			return createIndex(row, column, reinterpret_cast<quintptr>(pParentItem));
		}
	}
	return QModelIndex();
}

//--------------------------------------------------------------------------------------------------
QModelIndex QDialogLineDatabaseModel::parent(const QModelIndex& index) const
{
	if (index.isValid())
	{
		SLineItem* pParentItem = static_cast<SLineItem*>(index.internalPointer());
		return IndexFromItem(pParentItem);
	}
	return QModelIndex();
}

//--------------------------------------------------------------------------------------------------
bool QDialogLineDatabaseModel::hasChildren(const QModelIndex& parent) const
{
	SLineItem* pItem = ItemFromIndex(parent);
	return pItem ? !pItem->child.empty() : false;
}

//--------------------------------------------------------------------------------------------------
bool QDialogLineDatabaseModel::ExecuteScript(int row, int count)
{
	const uint lineCount = m_pDatabase->GetLineSetCount();
	if (row + count > lineCount)
	{
		count = lineCount - row;
	}

	for (int i = 0; i < count; ++i)
	{
		if (!m_pDatabase->ExecuteScript(row + i))
		{
			return false;
		}
	}

	return true;
}

void QDialogLineDatabaseModel::ForceDataReload()
{
	delete m_pRoot;
	m_pRoot = new SLineItem(nullptr);

	beginResetModel();
	if (m_pDatabase)
	{
		const uint32 size = m_pDatabase->GetLineSetCount();
		for (uint32 i = 0; i < size; ++i)
		{
			DRS::IDialogLineSet* pLineSet = m_pDatabase->GetLineSetByIndex(i);
			if (pLineSet)
			{
				SLineItem* pParent = new SLineItem(m_pRoot);
				const uint32 count = pLineSet->GetLineCount() - 1;
				for (uint32 j = 0; j < count; ++j)
				{
					pParent->child.push_back(new SLineItem(pParent));
				}
				m_pRoot->child.push_back(pParent);
			}
		}
	}
	endResetModel();
}

//--------------------------------------------------------------------------------------------------
SLineItem* QDialogLineDatabaseModel::ItemFromIndex(const QModelIndex& index) const
{
	if ((index.row() < 0) || (index.column() < 0))
	{
		return m_pRoot;
	}
	SLineItem* pParent = static_cast<SLineItem*>(index.internalPointer());
	if (!pParent)
	{
		return nullptr;
	}

	// lazy init
	uint row = index.row();
	while (pParent->child.size() <= row)
	{
		SLineItem* pItem = new SLineItem(pParent);
		pParent->child.push_back(pItem);
	}
	return pParent->child[row];
}

//--------------------------------------------------------------------------------------------------
QModelIndex QDialogLineDatabaseModel::IndexFromItem(const SLineItem* pItem) const
{
	if (pItem && pItem->pParent)
	{
		SLineItem* pParent = pItem->pParent;
		const uint size = pParent->child.size();
		for (uint i = 0; i < size; ++i)
		{
			if (pParent->child[i] == pItem)
			{
				return createIndex(i, 0, reinterpret_cast<quintptr>(pParent));
			}
		}
	}
	return QModelIndex();
}

//--------------------------------------------------------------------------------------------------
QDialogLineDelegate::QDialogLineDelegate(QWidget* pParent)
	: QStyledItemDelegate(pParent)
	, m_pParent(pParent)
{
}

//--------------------------------------------------------------------------------------------------
bool QDialogLineDelegate::editorEvent(QEvent* pEvent, QAbstractItemModel* pModel, const QStyleOptionViewItem& option, const QModelIndex& index)
{
	if (index.flags() & Qt::ItemIsEditable)
	{
		switch ((EColumns)index.column())
		{
		case EColumns::END_TRIGGER:
		case EColumns::START_TRIGGER:
			{
				if (pEvent->type() == QEvent::MouseButtonDblClick)
				{
					QString previous = index.data(Qt::DisplayRole).toString();
					dll_string control = GetIEditor()->GetResourceSelectorHost()->SelectResource("AudioTrigger", QtUtil::ToString(previous), m_pParent);
					pModel->setData(index, QtUtil::ToQStringSafe(control.c_str()));
					return true;
				}
				return false;
			}
		}
	}

	return QStyledItemDelegate::editorEvent(pEvent, pModel, option, index);
}

//--------------------------------------------------------------------------------------------------
QWidget* QDialogLineDelegate::createEditor(QWidget* pParent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	if (index.flags() & Qt::ItemIsEditable)
	{
		switch ((EColumns)index.column())
		{
		case EColumns::FLAGS:
			{
				QMenuComboBox* pEditor = new QMenuComboBox(pParent);
				QStringList selectionMode;
				selectionMode << s_randomSelectionMode << s_sequentialSelectionMode << s_sequentialSelectionModeClamp << s_sequentialSelectionSuccessivelyMode << s_sequentialSelectionModeAllOnlyOnce;
				pEditor->AddItems(selectionMode);
				return pEditor;
			}
		}
	}

	return QStyledItemDelegate::createEditor(pParent, option, index);
}

//--------------------------------------------------------------------------------------------------
void QDialogLineDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QStyleOptionViewItem opt = option;

	// if it's a child item remove the indentation
	if (index.column() == 0 && index.parent().isValid())
	{
		opt.rect.setLeft(0);
	}

	QStyledItemDelegate::paint(painter, opt, index);
}

