// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SequenceToolbar.h"

#include "CryIcon.h"

#include "TrackViewPlugin.h"
#include "TrackViewComponentsManager.h"
#include "Controls/SequenceTabWidget.h"

#include <QMouseEvent>

class CAddSelectedEntitiesContextMenu : public QMenu
{
public:
	bool event(QEvent* pEvent) override
	{
		const int32 type = pEvent->type();
		if ((type == QEvent::Close) && (m_isTimeToHide == false))
		{
			return false;
		}

		if (type == QEvent::MouseButtonPress)
		{
			QMouseEvent* pMouseEvent = static_cast<QMouseEvent*>(pEvent);

			const QPointF localPos = pMouseEvent->localPos();
			const QRectF boundingBox = QRectF(QPointF(), size());

			if (boundingBox.contains(localPos) == false)
			{
				m_isTimeToHide = true;
			}
		}

		return QMenu::event(pEvent);
	}

	virtual void setVisible(bool visible) override
	{
		if (visible)
		{
			QMenu::setVisible(true);
		}
		else if (m_isTimeToHide)
		{
			QMenu::setVisible(false);
		}
	}

private:
	bool m_isTimeToHide = false;
};

CTrackViewSequenceToolbar::CTrackViewSequenceToolbar(CTrackViewCore* pTrackViewCore)
	: CTrackViewCoreComponent(pTrackViewCore, false)
{
	setMovable(true);
	setWindowTitle(QString("Sequence Controls"));

	QAction* pActionAddSelectedEntities = addAction(CryIcon("icons:Trackview/Add_Selected_Entities_To_Sequence.ico"), "Add Selected Entities", this, SLOT(OnAddSelectedEntities()));
	{
		auto pActionWidgetAddSelectedEntities = widgetForAction(pActionAddSelectedEntities);
		pActionWidgetAddSelectedEntities->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(pActionWidgetAddSelectedEntities, &QWidget::customContextMenuRequested, this, &CTrackViewSequenceToolbar::OnAddSelectedEntitiesContextMenu);
	}

	addAction(CryIcon("icons:General/Options.ico"), "Sequence Properties", this, SLOT(OnShowProperties()));
	addSeparator();
	addAction(GetIEditor()->GetICommandManager()->GetAction("general.zoom_in"));
	addAction(GetIEditor()->GetICommandManager()->GetAction("general.zoom_out"));
}

void CTrackViewSequenceToolbar::OnAddSelectedEntitiesContextMenu()
{
	CTrackViewSequence* pSequence = CTrackViewPlugin::GetSequenceManager()->GetSequenceByIndex(0);
	if (pSequence == nullptr)
	{
		return;
	}

	IAnimSequence* pAnimSequence = pSequence->GetIAnimSequence();
	if (pAnimSequence == nullptr)
	{
		return;
	}

	IAnimNode* pFakeEntityNode = pAnimSequence->CreateNode(eAnimNodeType_Entity);
	if (pFakeEntityNode == nullptr)
	{
		return;
	}

	std::unique_ptr<QMenu> pContextMenu(new CAddSelectedEntitiesContextMenu);

	size_t paramCount = pFakeEntityNode->GetParamCount();
	for (size_t i = 0; i < paramCount; i++)
	{
		CAnimParamType paramType = pFakeEntityNode->GetParamType(i);
		QAction* pAction = pContextMenu->addAction(pFakeEntityNode->GetParamName(paramType));

		QObject::connect(pAction, &QAction::toggled, [this, paramType](bool checked)
		{
			if (checked)
			{
				GetTrackViewCore()->AddAnimEntityDefaultTrackType(paramType);
			}
			else
			{
				GetTrackViewCore()->RemoveAnimEntityDefaultTrackType(paramType);
			}
		});

		pAction->setCheckable(true);
		pAction->setChecked(GetTrackViewCore()->IsContainsAnimEntityDefaultTrackType(paramType));
	}

	pAnimSequence->RemoveNode(pFakeEntityNode);
	pContextMenu->exec(QCursor::pos());
}

void CTrackViewSequenceToolbar::OnAddSelectedEntities()
{
	GetTrackViewCore()->OnAddSelectedEntities();
}

void CTrackViewSequenceToolbar::OnShowProperties()
{
	if (CTrackViewComponentsManager* pManager = GetTrackViewCore()->GetComponentsManager())
	{
		if (CTrackViewSequenceTabWidget* pTabWidget = pManager->GetTrackViewSequenceTabWidget())
		{
			if (CTrackViewSequence* pSequence = pTabWidget->GetActiveSequence())
			{
				pTabWidget->ShowSequenceProperties(pSequence);
			}
		}
	}
}
