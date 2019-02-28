// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <CryCore/Platform/platform.h>
#include <CrySystem/ISystem.h>
#include <QBoxLayout>
#include <QDragEnterEvent>

#include <CryMath/Cry_Camera.h>
#include "SplitViewport.h"
#include "QViewport.h"
#include "Serialization.h"
#include "QViewportSettings.h"

#include <DragDrop.h>

namespace CharacterTool
{

QSplitViewport::QSplitViewport(QWidget* parent)
	: QWidget(parent)
	, m_isSplit(false)
{
	setContentsMargins(0, 0, 0, 0);
	m_originalViewport = new QViewport(gEnv, 0);
	m_originalViewport->setVisible(false);
	connect(m_originalViewport, SIGNAL(SignalCameraMoved(const QuatT &)), this, SLOT(OnCameraMoved(const QuatT &)));

	m_viewport = new QViewport(gEnv, 0);
	connect(m_viewport, SIGNAL(SignalCameraMoved(const QuatT &)), this, SLOT(OnCameraMoved(const QuatT &)));

	m_layout = new QBoxLayout(QBoxLayout::LeftToRight);
	setLayout(m_layout);
	m_layout->setContentsMargins(0, 0, 0, 0);
	m_layout->addWidget(m_originalViewport, 1);
	m_layout->addWidget(m_viewport, 1);

	setAcceptDrops(true);
}

void QSplitViewport::Serialize(Serialization::IArchive& ar)
{
	ar(*m_viewport, "viewport", "Viewport");
	ar(*m_originalViewport, "originalViewport", "Original Viewport");
}

void QSplitViewport::SetSplit(bool split)
{
	if (m_isSplit != split)
	{
		m_isSplit = split;

		m_layout->removeWidget(m_originalViewport);
		m_layout->removeWidget(m_viewport);

		if (m_isSplit)
			m_layout->addWidget(m_originalViewport, 1);
		m_layout->addWidget(m_viewport, 1);
		m_originalViewport->setVisible(m_isSplit);
	}
}

void QSplitViewport::FixLayout()
{
	SetSplit(!m_isSplit);
	SetSplit(!m_isSplit);
}

void QSplitViewport::OnCameraMoved(const QuatT& qt)
{
	if (m_viewport)
	{
		SViewportState state = m_viewport->GetState();
		state.cameraTarget = qt;
		m_viewport->SetState(state);
	}

	if (m_originalViewport)
	{
		SViewportState state = m_viewport->GetState();
		state.cameraTarget = qt;
		m_originalViewport->SetState(state);
	}
}

void QSplitViewport::dragEnterEvent(QDragEnterEvent* pEvent)
{
	auto pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());
	const QStringList filePaths = pDragDropData->GetFilePaths();
	if (!filePaths.empty())
	{
		pEvent->acceptProposedAction();
	}
}

void QSplitViewport::dropEvent(QDropEvent* pEvent)
{
	auto pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());
	const QStringList filePaths = pDragDropData->GetFilePaths();
	CRY_ASSERT(!filePaths.empty());
	dropFile(filePaths.first());
}

}

