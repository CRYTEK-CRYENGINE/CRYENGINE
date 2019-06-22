// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "AI/AIManager.h"
#include "Objects/ObjectManager.h"
#include "ViewManager.h"

#include <Commands/QCommandAction.h>
#include <Preferences/SnappingPreferences.h>
#include <Preferences/ViewportPreferences.h>
#include <QControls.h>

#include <IUndoManager.h>

#include <QComboBox>
#include <QGroupBox>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

class QSelectionMaskMenu : public QDynamicPopupMenu
{
	Q_OBJECT

protected:

	virtual void OnPopulateMenu() override
	{
		AddAction("Select All", OBJTYPE_ANY);
		AddAction("Brushes", OBJTYPE_BRUSH);
		AddAction("No Brushes", ~OBJTYPE_BRUSH);
		AddAction("Entities", OBJTYPE_ENTITY);
		AddAction("Prefabs", OBJTYPE_PREFAB);
		AddAction("Areas, Shapes", OBJTYPE_VOLUME | OBJTYPE_SHAPE | OBJTYPE_VOLUMESOLID);
		AddAction("AI Points", OBJTYPE_AIPOINT | OBJTYPE_TAGPOINT);
		AddAction("Decals", OBJTYPE_DECAL);
		AddAction("Designer Objects", OBJTYPE_SOLID);
		AddAction("No Designer Objects", ~OBJTYPE_SOLID);
	}

	void AddAction(const char* str, int value)
	{
		string command;
		command.Format("general.set_selection_mask %d", value);
		QCommandAction* action = new QCommandAction(str, command.c_str(), nullptr);
		action->setCheckable(true);
		action->setChecked(gViewportSelectionPreferences.objectSelectMask == value);
		addAction(action);
	}
};

//////////////////////////////////////////////////////////////////////////
class QSnapToGridMenu : public QDynamicPopupMenu
{
	Q_OBJECT

public:
	QSnapToGridMenu(QWidget* parent = nullptr)
		: QDynamicPopupMenu(parent)
	{
	}

protected:
	virtual void OnTrigger(QAction* action) override
	{
		float newSize = atof(action->text().toStdString().c_str());
		gSnappingPreferences.setGridSize(newSize);
	}

	virtual void OnPopulateMenu() override
	{
		double gridSize = gSnappingPreferences.gridSize();

		double startSize = 0.125;
		double size = startSize;
		for (int i = 0; i < 10; i++)
		{
			string str;
			str.Format("%g", size);
			QAction* action = addAction(str.c_str());
			if (gridSize == size)
			{
				action->setCheckable(true);
				action->setChecked(true);
			}
			size *= 2;
		}
	}
};

//////////////////////////////////////////////////////////////////////////
class QSnapToAngleMenu : public QDynamicPopupMenu
{
	Q_OBJECT

public:
	QSnapToAngleMenu(QWidget* parent = nullptr)
		: QDynamicPopupMenu(parent)
	{
	}

protected:
	virtual void OnTrigger(QAction* action) override
	{
		gSnappingPreferences.setAngleSnap(atof(action->text().toStdString().c_str()));
	}

	virtual void OnPopulateMenu() override
	{
		const float anglesArray[] = { 1, 5, 10, 15, 22.5, 30, 45, 90 };
		const int steps = CRY_ARRAY_COUNT(anglesArray);
		for (int i = 0; i < steps; i++)
		{
			string str;
			str.Format("%f", anglesArray[i]);
			//Remove unneeded 0 at the end, but keep .5 for example
			char end = str[str.size() - 1];
			int counter = 0;
			//Count all the zeros
			while (end == '0')
			{
				counter++;
				end = str[str.size() - (counter + 1)];
			}
			//If there were only zeros, the next character is a . and we want to remove that one too
			if (end == '.')
			{
				counter++;
			}
			//Finally truncate by the amount of characters we want to remove
			str.resize(str.size() - counter);
			QAction* action = addAction(str.c_str());
			if (anglesArray[i] == gSnappingPreferences.angleSnap())
			{
				action->setCheckable(true);
				action->setChecked(true);
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////
class QSnapToScaleMenu : public QDynamicPopupMenu
{
	Q_OBJECT

public:
	QSnapToScaleMenu(QWidget* parent = nullptr)
		: QDynamicPopupMenu(parent)
	{
	}

protected:
	virtual void OnTrigger(QAction* action) override
	{
		gSnappingPreferences.setScaleSnap(atof(action->text().toStdString().c_str()));
	}

	virtual void OnPopulateMenu() override
	{
		const float scalesArray[] = { 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f };
		const int steps = CRY_ARRAY_COUNT(scalesArray);
		for (int i = 0; i < steps; i++)
		{
			string str;
			str.Format("%g", scalesArray[i]);
			QAction* action = addAction(str.c_str());
			if (scalesArray[i] == gSnappingPreferences.scaleSnap())
			{
				action->setCheckable(true);
				action->setChecked(true);
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////
class QNavigationAgentTypeMenu : public QDynamicPopupMenu
{
	Q_OBJECT

public:
	QNavigationAgentTypeMenu()
	{
		m_pActionGroup = new QActionGroup(this);
		m_pActionGroup->setObjectName("VisualizeAgentTypeGroup");
		OnPopulateMenu();
	}

protected:
	QActionGroup* m_pActionGroup;

	virtual void OnTrigger(QAction* action) override
	{
		if (action->property("QCommandAction").isNull())
		{
			CAIManager* manager = GetIEditorImpl()->GetAI();
			const size_t agentTypeCount = manager->GetNavigationAgentTypeCount();

			string actionName = action->objectName().toStdString().c_str();

			for (size_t i = 0; i < agentTypeCount; ++i)
			{
				if (0 == actionName.compare(manager->GetNavigationAgentTypeName(i)))
				{
					const bool shouldBeDisplayed = gAINavigationPreferences.navigationDebugAgentType() != i || !gAINavigationPreferences.navigationDebugDisplay();
					gAINavigationPreferences.setNavigationDebugAgentType(i);
					gAINavigationPreferences.setNavigationDebugDisplay(shouldBeDisplayed);
					break;
				}
			}
		}
		action->setChecked(gAINavigationPreferences.navigationDebugDisplay());
	}

	virtual void OnPopulateMenu() override
	{
		ICommandManager* pCommandManager = GetIEditorImpl()->GetICommandManager();
		CAIManager* pManager = GetIEditorImpl()->GetAI();
		const size_t agentTypeCount = pManager->GetNavigationAgentTypeCount();

		const string actionModule = "ai";
		for (size_t i = 0; i < agentTypeCount; ++i)
		{
			const char* szAgentTypeName = pManager->GetNavigationAgentTypeName(i);

			string actionName = "debug_agent_type" + ToString(i);
			string fullActionName = actionModule + "." + actionName;

			QAction* pAction = nullptr;
			if (pCommandManager->IsRegistered(actionModule, actionName))
			{
				pAction = pCommandManager->GetAction(fullActionName);
			}
			if (pAction)
			{
				addAction(pAction);
				pAction->setText(QString("Visualize %1").arg(szAgentTypeName));
			}
			else
			{
				pAction = addAction(fullActionName.c_str());
				pAction->setObjectName(szAgentTypeName);
				pAction->setText(QString("Visualize %1").arg(szAgentTypeName));
			}
			m_pActionGroup->addAction(pAction);
			pAction->setCheckable(true);

			if (i == gAINavigationPreferences.navigationDebugAgentType())
			{
				pAction->setChecked(gAINavigationPreferences.navigationDebugDisplay());
			}
			else
			{
				pAction->setChecked(false);
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////
class QMNMRegenerationAgentTypeMenu : public QDynamicPopupMenu
{
	Q_OBJECT

public:
	QMNMRegenerationAgentTypeMenu()
	{
		OnPopulateMenu();
	}

protected:

	virtual void OnTrigger(QAction* action) override
	{
		if (action->property("QCommandAction").isNull())
		{
			CAIManager* manager = GetIEditorImpl()->GetAI();
			manager->RegenerateNavigationByTypeName(action->objectName().toStdString().c_str());
		}
	}

	virtual void OnPopulateMenu() override
	{
		ICommandManager* pCommandManager = GetIEditorImpl()->GetICommandManager();
		CAIManager* pManager = GetIEditorImpl()->GetAI();
		const size_t agentTypeCount = pManager->GetNavigationAgentTypeCount();

		QAction* pRegenerateIgnoredAction = pCommandManager->GetAction("ai.regenerate_ignored");
		if (pRegenerateIgnoredAction)
		{
			addAction(pRegenerateIgnoredAction);
			pRegenerateIgnoredAction->setObjectName("ignored");
		}

		addSeparator();

		QAction* pRegenerateAllAction = pCommandManager->GetAction("ai.regenerate_agent_type_all");
		if (pRegenerateAllAction)
		{
			addAction(pRegenerateAllAction);
			pRegenerateAllAction->setObjectName("all");
		}

		const string actionModule = "ai";
		for (size_t i = 0; i < agentTypeCount; ++i)
		{
			const char* szAgentTypeName = pManager->GetNavigationAgentTypeName(i);

			string actionName = "regenerate_agent_type_layer" + ToString(i);
			string fullActionName = actionModule + "." + actionName;

			QAction* pAction = nullptr;
			if (pCommandManager->IsRegistered(actionModule, actionName))
			{
				pAction = pCommandManager->GetAction(fullActionName);
			}
			if (pAction)
			{
				addAction(pAction);
				pAction->setText(QString("Regenerate %1").arg(szAgentTypeName));
			}
			else
			{
				pAction = addAction(fullActionName.c_str());
				pAction->setObjectName(szAgentTypeName);
				pAction->setText(QString("Regenerate %1").arg(szAgentTypeName));
			}
		}
	}
};
