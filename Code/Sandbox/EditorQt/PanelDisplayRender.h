// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/IConsole.h>
#include <QScrollableBox.h>
#include <QtViewPane.h>

struct ICVar;
class CViewport;

class CPanelDisplayRender : public CDockableWidgetT<QScrollableBox>, public IAutoEditorNotifyListener
{
	Q_OBJECT
	Q_INTERFACES(IPane)

public:
	CPanelDisplayRender(QWidget* parent = nullptr, CViewport* viewport = nullptr);
	~CPanelDisplayRender();

	void        Serialize(Serialization::IArchive& ar);
	QSize       sizeHint() const override;
	const char* GetPaneTitle() const { return "Display Settings"; }

	void        OnCVarChangedCallback();

protected:
	void SetupCallbacks();
	void RemoveCallbacks();

	//////////////////////////////////////////////////////////////////////////
	// IAutoEditorNotifyListener
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);
	//////////////////////////////////////////////////////////////////////////

	void OnDisplayAll();
	void OnDisplayNone();
	void OnDisplayInvert();

	void OnHideObjectsAll();
	void OnHideObjectsNone();
	void OnHideObjectsInvert();

	void RegisterChangeCallback(const char* szVariableName, ConsoleVarFunc fnCallbackFunction);
	void UnregisterChangeCallback(const char* szVariableName);

	void SetObjectHideMask(uint32 mask);

	void SerializeStereo(Serialization::IArchive& ar);

	void showEvent(QShowEvent* e) override;

protected:
	class QPropertyTreeLegacy*               m_propertyTree;
	std::unordered_map<ICVar*, uint64> m_varCallbackMap;
	CViewport*                         m_pViewport;
};
