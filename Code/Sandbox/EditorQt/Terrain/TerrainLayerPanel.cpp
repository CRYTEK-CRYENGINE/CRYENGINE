// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <QGridLayout>
#include <QSplitter>
#include <Serialization/QPropertyTree/QPropertyTree.h>

#include "Terrain/Layer.h"
#include "Terrain/TerrainLayerPanel.h"
#include "Terrain/TerrainLayerView.h"
#include "Terrain/TerrainEditor.h"
#include "Terrain/TerrainManager.h"
#include "TerrainTexture.h"
#include "TerrainTexturePainter.h"
#include "QT/Widgets/QEditToolButton.h"
#include "QMfcApp/qmfchost.h"
#include "CryIcon.h"

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

	QTerrainLayerView* pLayerView = new QTerrainLayerView(GetIEditorImpl()->GetTerrainManager());
	pSplitter->addWidget(pLayerView);

	if (GetIEditorImpl()->GetTerrainManager())
		GetIEditorImpl()->GetTerrainManager()->signalSelectedLayerChanged.Connect(this, &QTerrainLayerPanel::SetLayer);

	QWidget* pPainterContainer = new QWidget(this);
	QVBoxLayout* paiterLayout = new QVBoxLayout(pPainterContainer);
	paiterLayout->setContentsMargins(0, 0, 0, 0);
	pPainterContainer->setLayout(paiterLayout);
	pSplitter->addWidget(pPainterContainer);
	paiterLayout->addWidget(new QTerrainLayerButtons());

	m_pPropertyTree = new QPropertyTree();
	paiterLayout->addWidget(m_pPropertyTree);
	QObject::connect(m_pPropertyTree, &QPropertyTree::signalPushUndo, this, &QTerrainLayerPanel::UndoPush);
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

void QTerrainLayerPanel::UndoPush()
{
	CUndo layerModified("Texture Layer Change");
	CTerrainTextureDialog::StoreLayersUndo();
}

