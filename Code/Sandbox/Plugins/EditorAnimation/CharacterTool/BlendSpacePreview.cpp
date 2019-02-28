// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "BlendSpacePreview.h"
#include "CharacterDocument.h"
#include "Expected.h"
#include <IEditor.h>
#include <CryAnimation/ICryAnimation.h>
#include <QDoubleSpinBox>
#include <QBoxLayout>
#include <QToolBar>
#include <QSlider>
#include <QLabel>
#include <QAction>
#include "Serialization.h"
#include "QViewport.h"
#include "QViewportSettings.h"
#include <CryIcon.h>

namespace CharacterTool
{

enum { SLIDER_MAX_VALUE = 10000 };

BlendSpacePreview::BlendSpacePreview(QWidget* parent, CharacterDocument* document)
	: QWidget(parent)
	, m_document(document)
	, m_layout(0)
{
	setMinimumHeight(200);

	m_layout = new QBoxLayout(QBoxLayout::TopToBottom);
	QToolBar* toolbar = new QToolBar();
	toolbar->setIconSize(QSize(16, 16));
	toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	{
		m_actionShowGrid = toolbar->addAction(CryIcon("icons:common/animation_grid.ico"), "Show Grid");
		m_actionShowGrid->setCheckable(true);
		m_actionShowGrid->setChecked(true);
		EXPECTED(connect(toolbar->addAction("Reset View"), SIGNAL(triggered()), this, SLOT(OnResetView())));

		QWidget* spacer = new QWidget();
		spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		toolbar->addWidget(spacer);

		m_characterScaleSlider = new QSlider(Qt::Horizontal, this);
		m_characterScaleSlider->setMinimum(0);
		m_characterScaleSlider->setMaximum(SLIDER_MAX_VALUE);
		m_characterScaleSlider->setValue(SLIDER_MAX_VALUE * 0.5f);
		m_characterScaleSlider->setMaximumWidth(200);
		toolbar->addWidget(m_characterScaleSlider);

		m_layout->addWidget(toolbar, 0);
	}
	m_layout->setContentsMargins(0, 0, 0, 0);
	m_viewport = new QViewport(gEnv, this);
	SViewportSettings settings = m_viewport->GetSettings();
	settings.grid.showGrid = false;
	m_viewport->SetSettings(settings);
	m_layout->addWidget(m_viewport, 1);
	setLayout(m_layout);

	EXPECTED(connect(m_viewport, SIGNAL(SignalRender(const SRenderContext &)), this, SLOT(OnRender(const SRenderContext &))));
}

void BlendSpacePreview::Serialize(Serialization::IArchive& ar)
{
	ar(*m_viewport, "viewport");

	float characterScale = float(m_characterScaleSlider->value()) / SLIDER_MAX_VALUE;
	ar(characterScale, "characterScale");
	if (ar.isInput())
		m_characterScaleSlider->setValue(int(characterScale * SLIDER_MAX_VALUE));

	bool showGrid = m_actionShowGrid->isChecked();
	ar(showGrid, "showGrid");
	if (ar.isInput())
		m_actionShowGrid->setChecked(showGrid);
}

void BlendSpacePreview::OnRender(const SRenderContext& context)
{
	SParametricSampler* sampler = 0;
	ICharacterInstance* character = m_document->CompressedCharacter();

	if (character)
	{
		ISkeletonAnim& skeletonAnim = *character->GetISkeletonAnim();
		int layer = 0;
		CAnimation* animation = &skeletonAnim.GetAnimFromFIFO(layer, 0);
		sampler = animation ? animation->GetParametricSampler() : 0;
	}

	if (sampler)
	{
		ICharacterManager* characterManager = GetIEditor()->GetSystem()->GetIAnimationSystem();

		float characterScale = float(m_characterScaleSlider->value()) / SLIDER_MAX_VALUE;
		unsigned int debugFlags = 4;
		if (m_actionShowGrid->isChecked())
			debugFlags |= 3;
		characterManager->RenderBlendSpace(*context.passInfo, character, characterScale, debugFlags);
	}
	else
	{
		IRenderAuxText::Draw2dLabel(20, 30, 2, ColorF(0xffffffff), false, "No BlendSpace selected.");
	}
}

void BlendSpacePreview::OnResetView()
{
	m_viewport->ResetCamera();
	m_characterScaleSlider->setValue(SLIDER_MAX_VALUE * 0.5f);
}

void BlendSpacePreview::IdleUpdate()
{
	ICharacterManager* characterManager = GetIEditor()->GetSystem()->GetIAnimationSystem();
	const char* loadedCharacterFilename = m_document->LoadedCharacterFilename();
	if (loadedCharacterFilename[0] != '\0' && !characterManager->HasDebugInstancesCreated(loadedCharacterFilename))
		characterManager->CreateDebugInstances(loadedCharacterFilename);

	m_viewport->Update();
}

}

