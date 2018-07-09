// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QCollapsibleFrame.h>

class CInspectorWidgetCreator
{
public:
	struct SQueuedWidgetInfo
	{
		// Number of objects that have requested adding this widget
		uint64 usageCount;
		uint64 id;
		std::function<QWidget*(const SQueuedWidgetInfo& queuedWidget)> createWidgetCallback;
		std::unique_ptr<QCollapsibleFrame> collapsibleFrames;

		CObjectPropertyWidget::TSerializationFunc serializationFunc;
	};

	CInspectorWidgetCreator()
		: m_personalizationState(GetIEditor()->GetPersonalizationManager()->GetState("ObjectInspectorWidgetCreator"))
	{
	}

	// Called when we're done creating widgets for a specific object
	// This signifies us only keeping what is shared between m_currentScopeQueuedWidgets and m_queuedWidgets
	// This is done since multi-selection behavior currently expects that we only show widgets that the objects have in common.
	void EndScope()
	{
		m_scopeCount++;

		// Remove any widget that was not specified in the last scope
		m_queuedWidgets.erase(std::remove_if(m_queuedWidgets.begin(), m_queuedWidgets.end(), [this](const SQueuedWidgetInfo& widgetInfo) -> bool
		{
			return m_scopeCount != widgetInfo.usageCount;
		}), m_queuedWidgets.end());
	}

	// Helper to add a property tree contained inside a collapsible frame to the inspector's scrollable box area
	template <typename TObjectType>
	void AddPropertyTree(const char* szTitle, std::function<void(TObjectType* pObject, Serialization::IArchive& ar, bool bMultiEdit)> serializationFunc, bool bCollapsedByDefault = false)
	{
		uint64 id = stl::hash_strcmp<const char*>()(szTitle);

		// Look if another object has already queued this widget
		auto it = std::lower_bound(m_queuedWidgets.begin(), m_queuedWidgets.end(), id, [](const SQueuedWidgetInfo& widgetInfo, const uint64 widgetId) -> bool { return widgetInfo.id < widgetId; });
		if (it != m_queuedWidgets.end() && it->id == id)
		{
			// Duplicate, skip
			it->usageCount++;
			return;
		}

		auto serializationFuncWrapper = [serializationFunc](CBaseObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
		{
			serializationFunc(static_cast<TObjectType*>(pObject), ar, bMultiEdit);
		};

		AddPropertyTreeInternal(id, szTitle, stl::make_unique<QCollapsibleFrame>(szTitle, nullptr), serializationFuncWrapper, bCollapsedByDefault);
	}

	void AddPropertyTree(uint64 id, const char* szTitle, std::unique_ptr<QCollapsibleFrame>&& pFrame, CObjectPropertyWidget::TSerializationFunc serializationFunc, bool bCollapsedByDefault = false)
	{
		// Look if another object has already queued this widget
		auto it = std::lower_bound(m_queuedWidgets.begin(), m_queuedWidgets.end(), id, [](const SQueuedWidgetInfo& widgetInfo, const uint64 widgetId) -> bool { return widgetInfo.id < widgetId; });
		if (it != m_queuedWidgets.end() && it->id == id)
		{
			// Duplicate, skip
			it->usageCount++;
			return;
		}

		AddPropertyTreeInternal(id, szTitle, std::move(pFrame), serializationFunc, bCollapsedByDefault);
	}

	void AddWidget(uint64 id, std::function<QWidget*(const SQueuedWidgetInfo& queuedWidget)> widgetCreationFunc)
	{
		// Look if another object has already queued this widget
		auto it = std::lower_bound(m_queuedWidgets.begin(), m_queuedWidgets.end(), id, [](const SQueuedWidgetInfo& widgetInfo, const uint64 widgetId) -> bool { return widgetInfo.id < widgetId; });
		if (it != m_queuedWidgets.end() && it->id == id)
		{
			// Duplicate, skip
			it->usageCount++;
			return;
		}

		auto upperBoundIt = std::upper_bound(m_queuedWidgets.begin(), m_queuedWidgets.end(), id, [](const uint64 widgetId, const SQueuedWidgetInfo& widgetInfo) -> bool { return widgetId < widgetInfo.id; });
		m_queuedWidgets.emplace(upperBoundIt, SQueuedWidgetInfo{ 1, id, widgetCreationFunc });
	}

	// Called when we're done queuing all inspector widgets, and want to finally add them to the inspector
	void AddWidgetsToInspector(CInspector& inspector)
	{
		// Process widgets queued from the callback above
		for (SQueuedWidgetInfo& queuedWidget : m_queuedWidgets)
		{
			// Release ownership to the frames, as it is now up to the parent object to delete them
			queuedWidget.collapsibleFrames.release();

			CRY_ASSERT(queuedWidget.createWidgetCallback);

			inspector.AddWidget(queuedWidget.createWidgetCallback(queuedWidget));
		}
	}

protected:
	// Adds a collapsible frame to the inspector's scrollable box area
	void AddPropertyTreeInternal(uint64 id, const char* szTitle, std::unique_ptr<QCollapsibleFrame>&& pFrame, CObjectPropertyWidget::TSerializationFunc serializationFunc, bool bCollapsedByDefault = false)
	{
		string title = szTitle;
		QCollapsibleFrame* pCollapsibleFrame = pFrame.get();

		// We defer creation of the widget itself until AddWidgetsToInspector is called
		auto createWidgetCallback = [this, pCollapsibleFrame, bCollapsedByDefault, title](const SQueuedWidgetInfo& queuedWidget) -> QWidget*
		{
			bool bCollapsed = bCollapsedByDefault;

			// Override default collapsed setting with user's personalized state
			auto personalizationIt = m_personalizationState.find(title.c_str());
			if (personalizationIt != m_personalizationState.end())
			{
				bCollapsed = personalizationIt->toBool();
			}

			pCollapsibleFrame->SetCollapsed(bCollapsed);
			pCollapsibleFrame->SetCollapsedStateChangeCallback([title](bool bCollapsed)
			{
				QVariantMap personalizationState = GetIEditor()->GetPersonalizationManager()->GetState("ObjectInspectorWidgetCreator");

				personalizationState.insert(title.c_str(), bCollapsed);

				GetIEditor()->GetPersonalizationManager()->SetState("ObjectInspectorWidgetCreator", personalizationState);
			});

			// Create the frame widget
			pCollapsibleFrame->SetWidget(new CObjectPropertyWidget(queuedWidget.serializationFunc));
			return pCollapsibleFrame;
		};

		auto upperBoundIt = std::upper_bound(m_queuedWidgets.begin(), m_queuedWidgets.end(), id, [](const uint64 widgetId, const SQueuedWidgetInfo& widgetInfo) -> bool { return widgetId < widgetInfo.id; });
		m_queuedWidgets.emplace(upperBoundIt, SQueuedWidgetInfo{ 1, id, createWidgetCallback, std::move(pFrame), serializationFunc });
	}

private:
	// Number of scopes, AKA number of objects that have tried to add widgets
	uint64 m_scopeCount = 0;
	// Vector containing widgets that are queued for adding to the inspector
	// This is sorted by widget id, in order to allow for binary search when adding elements to prevent duplicates
	std::vector<SQueuedWidgetInfo> m_queuedWidgets;
	QVariantMap     m_personalizationState;
};

