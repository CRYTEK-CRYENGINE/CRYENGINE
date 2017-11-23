// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QTViewPane.h>
#include <CrySandbox/CryInterop.h>

namespace Schematyc2
{
	class CBrowserWidget;
	class CDetailWidget;
	class CDocGraphWidget;
	class CLogWidget;
	class CPreviewWidget;

	class CMainWindow : public CDockableWindow, public IEditorNotifyListener, public IUriEventListener
	{
		Q_OBJECT

	public:

		CMainWindow();

		// IPane
		virtual const char* GetPaneTitle() const override;
		// ~IPane

		// IEditorNotifyListener
		void OnEditorNotifyEvent(EEditorNotifyEvent event) override;
		//~IEditorNotifyListener

		// IUriEventListener
		virtual void OnUriReceived(const char* szUri) override;
		//~IUriEventListener

		void Serialize(Serialization::IArchive& archive);

	protected slots:

		void OnToolbarSaveAll();

	private:

		void DockWidget(QWidget* pWidget, const char* szName, Qt::DockWidgetArea area);
		
		void OnBrowserSelection(IScriptElement* pScriptElement);

	private:

		CBrowserWidget*  m_pBrowser;
		CDetailWidget*   m_pDetail;
		CDocGraphWidget* m_pGraph;
		CLogWidget*      m_pLog;
		CPreviewWidget*  m_pPreview;

		TemplateUtils::CConnectionScope m_connectionScope;
	};
}