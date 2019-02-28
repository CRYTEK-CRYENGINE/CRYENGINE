// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <QPixmap>
#include <QIcon>
#include <QImage>
#include <QEvent>
#include <QMouseEvent>
#include <QMenu>
#include <QItemDelegate>

#include "Terrain/TerrainLayerView.h"
#include "Terrain/TerrainManager.h"
#include "TerrainTexture.h"
#include "Terrain/Layer.h"
#include "RecursionLoopGuard.h"
#include "EditorFramework/Events.h"
#include "QAdvancedItemDelegate.h"

#define LAYER_PREVIEW_SIZE 32

namespace Private_TerrainLayerListView
{
CItemModelAttribute s_LayerAttributes[] = {
	Attributes::s_nameAttribute,
	CItemModelAttribute("Filter Color", eAttributeType_String, CItemModelAttribute::StartHidden, false, ""),
	CItemModelAttribute("Min Height", eAttributeType_Float, CItemModelAttribute::StartHidden, false, ""),
	CItemModelAttribute("Max Height", eAttributeType_Float, CItemModelAttribute::StartHidden, false, ""),
	CItemModelAttribute("Min Angle", eAttributeType_Float, CItemModelAttribute::StartHidden, false, ""),
	CItemModelAttribute("Max Angle", eAttributeType_Float, CItemModelAttribute::StartHidden, false, ""),
	CItemModelAttribute("Texture", eAttributeType_String, CItemModelAttribute::StartHidden, false, ""),
	CItemModelAttribute("Material", eAttributeType_String, CItemModelAttribute::StartHidden, false, ""),
};

const int s_attributeCount = sizeof(s_LayerAttributes) / sizeof(CItemModelAttribute);

class QLayerModel : public QAbstractTableModel
{
public:
	enum LayerInfo
	{
		eLayerInfo_Name,
		eLayerInfo_FilterColor,
		eLayerInfo_MinHeight,
		eLayerInfo_MaxHeight,
		eLayerInfo_MinAngle,
		eLayerInfo_MaxAngle,
		eLayerInfo_Texture,
		eLayerInfo_Material
	};

	QLayerModel(QWidget* parent, CTerrainManager* pTerrainManager);
	~QLayerModel();
	int           rowCount(const QModelIndex& parent = QModelIndex()) const;
	int           columnCount(const QModelIndex& parent = QModelIndex()) const;
	bool          setData(const QModelIndex& index, const QVariant& value, int role);
	QVariant      data(const QModelIndex& index, int role = Qt::DisplayRole) const;
	QVariant      headerData(int section, Qt::Orientation orientation, int role) const;
	void          ReloadLayers();
	Qt::ItemFlags flags(const QModelIndex& index) const;

private:
	CTerrainManager*       m_pTerrainManager;
	int                    m_layersCount;
	QList<QIcon>           m_previews;
};

QLayerModel::QLayerModel(QWidget* parent, CTerrainManager* pTerrainManager)
	: QAbstractTableModel(parent)
	, m_pTerrainManager(pTerrainManager)
	, m_layersCount(0)
{
	ReloadLayers();
	if (pTerrainManager)
		pTerrainManager->signalLayersChanged.Connect(this, &QLayerModel::ReloadLayers);
}

QLayerModel::~QLayerModel()
{
	if (m_pTerrainManager)
		m_pTerrainManager->signalLayersChanged.DisconnectObject(this);
}

Qt::ItemFlags QLayerModel::flags(const QModelIndex& index) const
{
	Qt::ItemFlags result = __super::flags(index);
	if (index.column() == (int)eLayerInfo_Name)
		result |= Qt::ItemIsEditable;

	return result;
}

void QLayerModel::ReloadLayers()
{
	if (0 < rowCount())
	{
		beginRemoveRows(QModelIndex(), 0, rowCount() - 1);
		m_layersCount = 0;
		endRemoveRows();
	}

	m_previews.clear();
	m_layersCount = m_pTerrainManager ? m_pTerrainManager->GetLayerCount() : 0;
	m_previews.reserve(m_layersCount);

	CImageEx preview;
	preview.Allocate(LAYER_PREVIEW_SIZE, LAYER_PREVIEW_SIZE);

	for (int i = 0; i < m_layersCount; ++i)
	{
		preview.Fill(128);

		CLayer* pLayer = GetIEditorImpl()->GetTerrainManager()->GetLayer(i);
		CImageEx& previewImage = pLayer->GetTexturePreviewImage();

		if (previewImage.IsValid())
			CImageUtil::ScaleToFit(previewImage, preview);

		preview.SwapRedAndBlue();

		QImage img = QImage((const uchar*)preview.GetData(), LAYER_PREVIEW_SIZE, LAYER_PREVIEW_SIZE, QImage::Format_ARGB32);
		m_previews.push_back(QIcon(QPixmap().fromImage(img)));
	}

	if (0 < m_layersCount)
	{
		beginInsertRows(QModelIndex(), 0, rowCount() - 1);
		endInsertRows();
	}
}

int QLayerModel::rowCount(const QModelIndex& parent) const
{
	if (!parent.isValid() && m_pTerrainManager)
		return m_layersCount;
	return 0;
}

int QLayerModel::columnCount(const QModelIndex& parent) const
{
	if (!parent.isValid())
		return s_attributeCount;

	return 0;
}

QVariant QLayerModel::data(const QModelIndex& index, int role) const
{
	int row = index.row();
	int col = index.column();
	int count = min(m_previews.size(), m_pTerrainManager->GetLayerCount());

	if (!m_pTerrainManager || count <= row && 0 <= row)
		return QVariant();

	switch (role)
	{
	case Qt::EditRole:
	case Qt::DisplayRole:
		if (Private_TerrainLayerListView::s_attributeCount > col)
		{
			CLayer* pLayer = m_pTerrainManager->GetLayer(row);
			if (!pLayer)
				break;

			switch ((LayerInfo)col)
			{
			case eLayerInfo_Name:
				return QString(pLayer->GetLayerName().GetBuffer());
			case eLayerInfo_MinHeight:
				return pLayer->GetLayerStart();
			case eLayerInfo_MaxHeight:
				return pLayer->GetLayerEnd();
			case eLayerInfo_MinAngle:
				return pLayer->GetLayerMinSlopeAngle();
			case eLayerInfo_MaxAngle:
				return pLayer->GetLayerMaxSlopeAngle();
			case eLayerInfo_Texture:
				return QString(pLayer->GetTextureFilename().c_str());
			case eLayerInfo_Material:
				return QString(PathUtil::GetFileName(pLayer->GetMaterialName()).c_str());
			}
		}
		break;

	case Qt::DecorationRole:
		if (Private_TerrainLayerListView::s_attributeCount > col)
		{
			CLayer* pLayer = m_pTerrainManager->GetLayer(row);
			if (!pLayer)
				break;

			switch (col)
			{
			case eLayerInfo_Name:
				return m_previews[row];
			case eLayerInfo_FilterColor:
				return QColor::fromRgbF(pLayer->GetLayerFilterColor().r, pLayer->GetLayerFilterColor().g, pLayer->GetLayerFilterColor().b, pLayer->GetLayerFilterColor().a);
			}
		}
		break;

	case Qt::SizeHintRole:
		return QSize(LAYER_PREVIEW_SIZE, LAYER_PREVIEW_SIZE + 2);
	}

	return QVariant();
}

bool QLayerModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	int row = index.row();
	int col = index.column();

	if (!m_pTerrainManager || m_pTerrainManager->GetLayerCount() <= row && 0 <= row && value.isValid())
		return false;

	switch (role)
	{
	case Qt::EditRole:
		CLayer* pLayer = m_pTerrainManager->GetLayer(row);
		QString newName(value.toString());
		QString oldName(pLayer->GetLayerName().GetBuffer());

		if (newName != oldName && !newName.isEmpty())
		{
			CUndo undo("Texture Layer Change");
			CTerrainTextureDialog::StoreLayersUndo();
			pLayer->SetLayerName(newName.toUtf8().constData());
			return true;
		}
	}

	return false;
}

QVariant QLayerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && Private_TerrainLayerListView::s_attributeCount > section)
	{
		switch (role)
		{
		case Qt::DisplayRole:
			return Private_TerrainLayerListView::s_LayerAttributes[section].GetName();

		case Attributes::s_attributeMenuPathRole:
			if (section != (int)eLayerInfo_Name)
				return "";
		}
	}

	return QVariant();
}
}

QTerrainLayerView::QTerrainLayerView(CTerrainManager* pTerrainManager)
	: m_pModel(nullptr)
	, m_pTerrainManager(pTerrainManager)
	, m_selecting(false)
{
	setSelectionMode(QAbstractItemView::SingleSelection);
	setIconSize(QSize(LAYER_PREVIEW_SIZE, LAYER_PREVIEW_SIZE + 2));

	if (pTerrainManager)
	{
		pTerrainManager->signalSelectedLayerChanged.Connect(this, &QTerrainLayerView::SelectedLayerChanged);
		pTerrainManager->signalLayersChanged.Connect(this, &QTerrainLayerView::LayersChanged);
		m_pModel = new Private_TerrainLayerListView::QLayerModel(this, pTerrainManager);
		setModel(m_pModel);
	}
	setItemDelegate(new QAdvancedItemDelegate(this));
}

QTerrainLayerView::~QTerrainLayerView()
{
	if (m_pTerrainManager)
	{
		m_pTerrainManager->signalSelectedLayerChanged.DisconnectObject(this);
		m_pTerrainManager->signalLayersChanged.DisconnectObject(this);
	}
}

void QTerrainLayerView::selectRow(int row)
{
	if (selectionModel() && model())
	{
		selectionModel()->select(model()->index(row, 0), QItemSelectionModel::ClearAndSelect);
		update();
	}
}

void QTerrainLayerView::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
	RECURSION_GUARD(m_selecting);

	if (m_pTerrainManager && 0 != selected.size())
	{
		int selectedIndex = selected.front().top();
		m_pTerrainManager->SelectLayer(selectedIndex);
	}

	__super::selectionChanged(selected, deselected);
}

void QTerrainLayerView::SelectedLayerChanged(CLayer* pLayer)
{
	RECURSION_GUARD(m_selecting);

	if (m_pTerrainManager)
	{
		int layerCount = m_pTerrainManager->GetLayerCount();
		for (int i = 0; i < layerCount; ++i)
		{
			if (pLayer == m_pTerrainManager->GetLayer(i))
			{
				selectRow(i);
				return;
			}
		}
	}
}

void QTerrainLayerView::customEvent(QEvent* event)
{
	if (event->type() == SandboxEvent::Command)
	{
		if (m_pTerrainManager)
		{
			CommandEvent* commandEvent = static_cast<CommandEvent*>(event);
			const string& command = commandEvent->GetCommand();
			if (command == "general.delete")
			{
				commandEvent->setAccepted(true);
				GetIEditorImpl()->ExecuteCommand("terrain.delete_layer");
			}
		}
	}
	else
	{
		__super::customEvent(event);
	}
}

void QTerrainLayerView::LayersChanged()
{
	RECURSION_GUARD(m_selecting);

	if (m_pTerrainManager && 0 == selectedIndexes().size())
	{
		int layerCount = m_pTerrainManager->GetLayerCount();
		for (int i = 0; i < layerCount; ++i)
		{
			if (m_pTerrainManager->GetLayer(i)->IsSelected())
			{
				selectRow(i);
				return;
			}
		}
	}
}

void QTerrainLayerView::mousePressEvent(QMouseEvent* event)
{
	__super::mousePressEvent(event);

	if (Qt::RightButton == event->button())
	{
		ICommandManager* pManager = GetIEditorImpl()->GetICommandManager();
		if (pManager && m_pTerrainManager)
		{
			QMenu* pLayerMenu = new QMenu();
			QList<QAction*> actions;
			actions.push_back(pManager->GetAction("terrain.create_layer"));

			if (m_pTerrainManager->GetSelectedLayer())
			{
				actions.push_back(pManager->GetAction("terrain.delete_layer"));
				actions.push_back(pManager->GetAction("terrain.duplicate_layer"));
				actions.push_back(pManager->GetAction("terrain.move_layer_up"));
				actions.push_back(pManager->GetAction("terrain.move_layer_down"));
				actions.push_back(pManager->GetAction("terrain.flood_layer"));
			}

			pLayerMenu->addActions(actions);
			pLayerMenu->popup(QCursor::pos());
		}
	}
}

