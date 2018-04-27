// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Viewport.h"
#include <QBoxLayout>
#include <Cry3DEngine/CGF/CryHeaders.h> // MAX_STATOBJ_LODS_NUM
#include <QViewportSettings.h>
#include <CryCore/StlUtils.h>

////////////////////////////////////////////////////

CSplitViewport::CSplitViewport(QWidget* parent)
	: QWidget(parent)
	, m_pLayout(nullptr)
	, m_bSplit(false)
	, m_splitDirection(eSplitDirection_Horizontal)
{
	setContentsMargins(0, 0, 0, 0);
	m_pSecondaryViewport = new QViewport(gEnv, parent);
	connect(m_pSecondaryViewport, SIGNAL(SignalCameraMoved(const QuatT &)), this, SLOT(OnCameraMoved(const QuatT &)));

	m_pPrimaryViewport = new QViewport(gEnv, parent);
	connect(m_pPrimaryViewport, SIGNAL(SignalCameraMoved(const QuatT &)), this, SLOT(OnCameraMoved(const QuatT &)));

	ResetLayout();
}

void CSplitViewport::SetSplit(bool bSplit)
{
	if (m_bSplit != bSplit)
	{
		m_bSplit = bSplit;
		ResetLayout();
	}
}

void CSplitViewport::SetSplitDirection(ESplitDirection splitDirection)
{
	if (m_splitDirection != splitDirection)
	{
		m_splitDirection = splitDirection;
		ResetLayout();
	}
}

void CSplitViewport::FixLayout()
{
	SetSplit(!m_bSplit);
	SetSplit(!m_bSplit);
}

void CSplitViewport::ResetLayout()
{
	if (m_pLayout)
	{
		delete m_pLayout;
		m_pLayout = nullptr;
	}

	const QBoxLayout::Direction layoutDir =
	  m_splitDirection == eSplitDirection_Horizontal
	  ? QBoxLayout::BottomToTop
	  : QBoxLayout::RightToLeft;

	m_pLayout = new QBoxLayout(layoutDir);
	setLayout(m_pLayout);
	m_pLayout->setContentsMargins(0, 0, 0, 0);
	if (m_bSplit)
	{
		m_pLayout->addWidget(m_pSecondaryViewport, 1);
	}
	m_pSecondaryViewport->setVisible(m_bSplit);
	m_pLayout->addWidget(m_pPrimaryViewport, 1);
}

void CSplitViewport::OnCameraMoved(const QuatT& qt)
{
	if (m_pPrimaryViewport)
	{
		SViewportState state = m_pPrimaryViewport->GetState();
		state.cameraTarget = qt;
		m_pPrimaryViewport->SetState(state);
	}

	if (m_pSecondaryViewport)
	{
		SViewportState state = m_pPrimaryViewport->GetState();
		state.cameraTarget = qt;
		m_pSecondaryViewport->SetState(state);
	}
}

