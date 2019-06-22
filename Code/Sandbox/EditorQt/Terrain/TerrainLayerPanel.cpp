// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "QT/Widgets/QEditToolButton.h"
#include "Terrain/Layer.h"
#include "Terrain/TerrainLayerPanel.h"
#include "Terrain/TerrainLayerUndoObject.h"
#include "Terrain/TerrainLayerView.h"
#include "Terrain/TerrainManager.h"
#include "TerrainTexturePainter.h"

#include <CryIcon.h>

#include <Serialization/QPropertyTreeLegacy/QPropertyTreeLegacy.h>

#include <QSplitter>
#include <QBoxLayout>

void STexturePainterSerializer::YASLI_SERIALIZE_METHOD(Serialization::IArchive& ar)
{
	textureLayerUpdate = true;

	if (pEditTool)
		pEditTool->Serialize(ar);

	if (pLayer)
		pLayer->Serialize(ar);

	textureLayerUpdate = false;
}

QTerrainLayerButtons::QTerrainLayerButtons(QWidget* parent)
	: QWidget(parent)
{
	QVBoxLayout* localLayout = new QVBoxLayout();
	setLayout(localLayout);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	localLayout->setAlignment(localLayout, Qt::AlignTop);

	AddTool(RUNTIME_CLASS(CTerrainTexturePainter), "Paint");
}

void QTerrainLayerButtons::AddTool(CRuntimeClass* pRuntimeClass, const char* text)
{
	QString icon = QString("icons:TerrainEditor/Terrain_%1.ico").arg(text);
	icon.replace(" ", "_");

	QEditToolButton* pToolButton = new QEditToolButton(nullptr);
	pToolButton->SetToolClass(pRuntimeClass);
	pToolButton->setText(text);
	pToolButton->setIcon(CryIcon(icon));
	pToolButton->setIconSize(QSize(24, 24));
	pToolButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	pToolButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
	layout()->addWidget(pToolButton);
}

QTerrainLayerPanel::QTerrainLayerPanel(QWidget* parent)
	: QEditToolPanel(parent)
{
	QVBoxLayout* localLayout = new QVBoxLayout();
	localLayout->setContentsMargins(1, 1, 1, 1);
	setLayout(localLayout);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	localLayout->setAlignment(localLayout, Qt::AlignTop);

	QSplitter* pSplitter = new QSplitter(this);
	pSplitter->setOrientation(Qt::Vertical);
	localLayout->addWidget(pSplitter);

	QTerrainLayerView* pLayerView = new QTerrainLayerView(this, GetIEditorImpl()->GetTerrainManager());
	pSplitter->addWidget(pLayerView);

	if (GetIEditorImpl()->GetTerrainManager())
		GetIEditorImpl()->GetTerrainManager()->signalSelectedLayerChanged.Connect(this, &QTerrainLayerPanel::SetLayer);

	QWidget* pPainterContainer = new QWidget(this);
	QVBoxLayout* pPainterLayout = new QVBoxLayout(pPainterContainer);
	pPainterLayout->setContentsMargins(0, 0, 0, 0);
	pPainterContainer->setLayout(pPainterLayout);
	pSplitter->addWidget(pPainterContainer);
	pPainterLayout->addWidget(new QTerrainLayerButtons());

	m_pPropertyTree = new QPropertyTreeLegacy();
	pPainterLayout->addWidget(m_pPropertyTree);
	QObject::connect(m_pPropertyTree, &QPropertyTreeLegacy::signalBeginUndo, this, &QTerrainLayerPanel::OnBeginUndo);
	QObject::connect(m_pPropertyTree, &QPropertyTreeLegacy::signalEndUndo, this, &QTerrainLayerPanel::OnEndUndo);
}

QTerrainLayerPanel::~QTerrainLayerPanel()
{
	if (GetIEditorImpl()->GetTerrainManager())
		GetIEditorImpl()->GetTerrainManager()->signalSelectedLayerChanged.DisconnectObject(this);

	if (m_layerSerializer.pEditTool)
		m_layerSerializer.pEditTool->signalPropertiesChanged.DisconnectObject(this);

	if (m_layerSerializer.pLayer)
		m_layerSerializer.pLayer->signalPropertiesChanged.DisconnectObject(this);
}

bool QTerrainLayerPanel::CanEditTool(CEditTool* pTool)
{
	if (!pTool)
		return false;

	return pTool->IsKindOf(RUNTIME_CLASS(CTerrainTexturePainter));
}

void QTerrainLayerPanel::SetTool(CEditTool* pTool)
{
	if (m_layerSerializer.pEditTool)
	{
		m_layerSerializer.pEditTool->signalPropertiesChanged.DisconnectObject(this);
		m_layerSerializer.pEditTool = nullptr;
	}

	if (pTool)
	{
		pTool->signalPropertiesChanged.Connect(this, &QTerrainLayerPanel::BrushChanged);
		m_layerSerializer.pEditTool = pTool;
		AttachProperties();
	}
	else
	{
		m_pPropertyTree->revertNoninterrupting();
	}
}

void QTerrainLayerPanel::SetLayer(CLayer* pLayer)
{
	if (m_layerSerializer.pLayer)
	{
		m_layerSerializer.pLayer->signalPropertiesChanged.DisconnectObject(this);
		m_layerSerializer.pLayer = nullptr;
	}

	if (pLayer)
	{
		pLayer->signalPropertiesChanged.Connect(this, &QTerrainLayerPanel::LayerChanged);
		m_layerSerializer.pLayer = pLayer;
		AttachProperties();
	}
	else
	{
		m_pPropertyTree->revert();
	}
}

void QTerrainLayerPanel::LayerChanged(CLayer* pLayer)
{
	if (!m_layerSerializer.textureLayerUpdate)
		m_pPropertyTree->revert();
}

void QTerrainLayerPanel::BrushChanged(CEditTool* pTool)
{
	if (!m_layerSerializer.textureLayerUpdate)
		m_pPropertyTree->revert();
}

void QTerrainLayerPanel::AttachProperties()
{
	Serialization::SStructs structs;
	structs.emplace_back(m_layerSerializer);
	m_pPropertyTree->detach();
	m_pPropertyTree->setExpandLevels(2);
	m_pPropertyTree->attach(structs);
}

void QTerrainLayerPanel::OnBeginUndo()
{
	GetIEditor()->GetIUndoManager()->Begin();
	GetIEditor()->GetIUndoManager()->RecordUndo(new CTerrainLayersPropsUndoObject);
}

void QTerrainLayerPanel::OnEndUndo(bool acceptUndo)
{
	if (acceptUndo)
	{
		GetIEditor()->GetIUndoManager()->Accept("Terrain Layers Properties");
	}
	else
	{
		GetIEditor()->GetIUndoManager()->Cancel();
	}
}
