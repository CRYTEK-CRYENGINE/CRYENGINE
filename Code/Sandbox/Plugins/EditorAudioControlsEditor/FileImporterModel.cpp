// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FileImporterModel.h"

#include <ProxyModels/ItemModelAttribute.h>
#include <EditorStyleHelper.h>

namespace ACE
{
QString const CFileImporterModel::s_newAction("Import (as new)");
QString const CFileImporterModel::s_replaceAction("Import (as replacement)");
QString const CFileImporterModel::s_unsupportedAction("Unsupported file type");
QString const CFileImporterModel::s_sameFileAction("Source file is target file");

static CItemModelAttribute s_sourceAttribute("Source", eAttributeType_String, CItemModelAttribute::Visible, false);
static CItemModelAttribute s_targetAttribute("Target", eAttributeType_String, CItemModelAttribute::Visible, false);
static CItemModelAttribute s_importAttribute("Import", eAttributeType_Boolean, CItemModelAttribute::Visible, false);

//////////////////////////////////////////////////////////////////////////
CItemModelAttribute* GetAttributeForColumn(CFileImporterModel::EColumns const column)
{
	CItemModelAttribute* pAttribute = nullptr;

	switch (column)
	{
	case CFileImporterModel::EColumns::Source:
		pAttribute = &s_sourceAttribute;
		break;
	case CFileImporterModel::EColumns::Target:
		pAttribute = &s_targetAttribute;
		break;
	case CFileImporterModel::EColumns::Import:
		pAttribute = &s_importAttribute;
		break;
	default:
		pAttribute = nullptr;
		break;
	}

	return pAttribute;
}

//////////////////////////////////////////////////////////////////////////
CFileImporterModel::CFileImporterModel(FileImportInfos& fileInfos, QString const& assetFolderPath, QString const& targetPath, QObject* const pParent)
	: QAbstractItemModel(pParent)
	, m_fileImportInfos(fileInfos)
	, m_targetPath(targetPath)
	, m_assetFolder(assetFolderPath)
{
}

//////////////////////////////////////////////////////////////////////////
int CFileImporterModel::rowCount(QModelIndex const& parent) const
{
	return static_cast<int>(m_fileImportInfos.size());
}

//////////////////////////////////////////////////////////////////////////
int CFileImporterModel::columnCount(QModelIndex const& parent) const
{
	return static_cast<int>(EColumns::Count);
}

//////////////////////////////////////////////////////////////////////////
QVariant CFileImporterModel::data(QModelIndex const& index, int role) const
{
	QVariant variant;

	if (index.isValid() && (index.row() < static_cast<int>(m_fileImportInfos.size())))
	{
		if (role == Qt::ForegroundRole)
		{
			if (!m_fileImportInfos[index.row()].isTypeSupported || (m_fileImportInfos[index.row()].actionType == SFileImportInfo::EActionType::SameFile))
			{
				variant = GetStyleHelper()->errorColor();
			}
			else if (m_fileImportInfos[index.row()].actionType == SFileImportInfo::EActionType::Ignore)
			{
				variant = GetStyleHelper()->disabledTextColor();
			}
		}
		else
		{
			switch (index.column())
			{
			case static_cast<int>(EColumns::Source):
				{
					switch (role)
					{
					case Qt::DisplayRole:
					case Qt::ToolTipRole:
						variant = m_fileImportInfos[index.row()].sourceInfo.filePath();
						break;
					}
				}
				break;
			case static_cast<int>(EColumns::Target):
				{
					switch (role)
					{
					case Qt::DisplayRole:
					case Qt::ToolTipRole:
						{
							SFileImportInfo const& fileInfo = m_fileImportInfos[index.row()];

							if (fileInfo.isTypeSupported)
							{
								variant = m_assetFolder.relativeFilePath(m_fileImportInfos[index.row()].targetInfo.filePath());
							}
							else
							{
								variant = "-";
							}
						}
						break;
					}
				}
				break;
			case static_cast<int>(EColumns::Import):
				{
					switch (role)
					{
					case Qt::DisplayRole:
						switch (m_fileImportInfos[index.row()].actionType)
						{
						case SFileImportInfo::EActionType::New:
							variant = s_newAction;
							break;
						case SFileImportInfo::EActionType::Replace:
							variant = s_replaceAction;
							break;
						case SFileImportInfo::EActionType::Ignore:
							variant = m_fileImportInfos[index.row()].targetInfo.isFile() ? s_replaceAction : s_newAction;
							break;
						case SFileImportInfo::EActionType::None:
							variant = s_unsupportedAction;
							break;
						case SFileImportInfo::EActionType::SameFile:
							variant = s_sameFileAction;
							break;
						}
						break;
					case Qt::ToolTipRole:
						switch (m_fileImportInfos[index.row()].actionType)
						{
						case SFileImportInfo::EActionType::New:
							variant = tr("New file in target location will be created.");
							break;
						case SFileImportInfo::EActionType::Replace:
							variant = tr("Existing file in target location will get replaced.");
							break;
						case SFileImportInfo::EActionType::Ignore:
							variant = tr("File will not get imported.");
							break;
						case SFileImportInfo::EActionType::None:
							variant = tr("File type is not supported.");
							break;
						case SFileImportInfo::EActionType::SameFile:
							variant = tr("Source files is target file.");
							break;
						}
						break;
					case Qt::CheckStateRole:
						{
							SFileImportInfo const& fileInfo = m_fileImportInfos[index.row()];

							if (fileInfo.isTypeSupported && (fileInfo.actionType != SFileImportInfo::EActionType::SameFile))
							{
								variant = (fileInfo.actionType != SFileImportInfo::EActionType::Ignore) ? Qt::Checked : Qt::Unchecked;
							}
						}
						break;
					}
				}
				break;
			}
		}
	}

	return variant;
}

//////////////////////////////////////////////////////////////////////////
bool CFileImporterModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
	bool wasDataChanged = false;

	if (index.isValid() && (index.row() < static_cast<int>(m_fileImportInfos.size())))
	{
		if ((index.column() == static_cast<int>(EColumns::Import)) && (role == Qt::CheckStateRole))
		{
			SFileImportInfo& fileInfo = m_fileImportInfos[index.row()];

			if (value == Qt::Checked)
			{
				if (fileInfo.targetInfo.isFile())
				{
					fileInfo.actionType = SFileImportInfo::EActionType::Replace;
				}
				else
				{
					fileInfo.actionType = SFileImportInfo::EActionType::New;
				}

				SignalActionChanged(Qt::Checked);
				wasDataChanged = true;
			}
			else
			{
				fileInfo.actionType = SFileImportInfo::EActionType::Ignore;
				SignalActionChanged(Qt::Unchecked);
				wasDataChanged = true;
			}
		}
	}

	return wasDataChanged;
}

//////////////////////////////////////////////////////////////////////////
QVariant CFileImporterModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	QVariant variant;

	if (orientation == Qt::Horizontal)
	{
		auto const pAttribute = GetAttributeForColumn(static_cast<EColumns>(section));

		if (pAttribute != nullptr)
		{
			switch (role)
			{
			case Qt::DisplayRole:
				variant = pAttribute->GetName();
				break;
			case Qt::ToolTipRole:
				variant = pAttribute->GetName();
				break;
			case Attributes::s_getAttributeRole:
				variant = QVariant::fromValue(pAttribute);
				break;
			default:
				break;
			}
		}
	}

	return variant;
}

//////////////////////////////////////////////////////////////////////////
Qt::ItemFlags CFileImporterModel::flags(QModelIndex const& index) const
{
	Qt::ItemFlags flags = QAbstractItemModel::flags(index);

	if (index.isValid())
	{
		if (index.column() == static_cast<int>(EColumns::Import) &&
		    (m_fileImportInfos[index.row()].isTypeSupported) &&
		    (m_fileImportInfos[index.row()].actionType != SFileImportInfo::EActionType::SameFile))
		{
			flags = QAbstractItemModel::flags(index) | Qt::ItemIsUserCheckable;
		}
		else
		{
			flags = QAbstractItemModel::flags(index) | Qt::ItemIsSelectable;
		}
	}

	return flags;
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CFileImporterModel::index(int row, int column, QModelIndex const& parent /*= QModelIndex()*/) const
{
	QModelIndex modelIndex = QModelIndex();

	if ((row >= 0) && (row < static_cast<int>(m_fileImportInfos.size())))
	{
		modelIndex = createIndex(row, column, reinterpret_cast<quintptr>(&m_fileImportInfos[row]));
	}

	return modelIndex;
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CFileImporterModel::parent(QModelIndex const& index) const
{
	QModelIndex modelIndex = QModelIndex();
	return modelIndex;
}

//////////////////////////////////////////////////////////////////////////
bool CFileImporterModel::canDropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) const
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CFileImporterModel::dropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
Qt::DropActions CFileImporterModel::supportedDragActions() const
{
	return Qt::IgnoreAction;
}

//////////////////////////////////////////////////////////////////////////
void CFileImporterModel::Reset()
{
	beginResetModel();
	endResetModel();
}

SFileImportInfo& CFileImporterModel::ItemFromIndex(QModelIndex const& index)
{
	return m_fileImportInfos[index.row()];
}
} // namespace ACE
