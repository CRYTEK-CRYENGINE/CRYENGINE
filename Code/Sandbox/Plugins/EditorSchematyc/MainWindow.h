// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IEditor.h>

#include <LogWidget.h>

#include <CrySandbox/CryInterop.h>
#include <CrySerialization/Forward.h>
#include <IPlugin.h>

#include <AssetSystem/AssetEditor.h>

#include <CrySchematyc/Utils/ScopedConnection.h>

#include "ObjectModel.h"

class CInspectorLegacy;
class QAction;
class QCommandAction;
class QVBoxLayout;

namespace Schematyc {

// Forward declare structures.
struct SScriptGraphViewSelection;
struct SScriptBrowserSelection;

// Forward declare interfaces.
struct IDetailItem;

// Forward declare classes.
class CEnvBrowserWidget;
class CPreviewWidget;
class CScriptBrowserModel;
class CScriptBrowserWidget;
class CSourceControlManager;

}

namespace CrySchematycEditor {

class CObjectModel;
class CComponentsWidget;
class CGraphsWidget;
class CGraphViewWidget;
class CVariablesWidget;

class CAbstractObjectStructureModelItem;

struct IDetailItem;

class CMainWindow : public CAssetEditor, public IEditorNotifyListener
{
	Q_OBJECT

public:
	CMainWindow();
	~CMainWindow();

	// CEditor
	virtual const char* GetEditorName() const override { return "Schematyc Editor"; }
	virtual void        SetLayout(const QVariantMap& state) override;
	virtual QVariantMap GetLayout() const override;
	// ~CEditor

	// IEditorNotifyListener
	void OnEditorNotifyEvent(EEditorNotifyEvent event) override;
	//~IEditorNotifyListener

	void    Serialize(Serialization::IArchive& archive);
	void    Show(const CryGUID& elementGUID, const CryGUID& detailGUID = CryGUID());

	bool    SaveUndo(XmlNodeRef& output) const;
	bool    RestoreUndo(const XmlNodeRef& input);

	CAsset* GetAsset() const { return m_pAsset; }

protected:

	// CAssetEditor
	virtual bool OnOpenAsset(CAsset* pAsset) override;
	virtual bool OnSaveAsset(CEditableAsset& editAsset) override;
	virtual bool OnAboutToCloseAsset(string& reason) const override;
	virtual void OnCloseAsset() override;
	virtual void OnInitialize() override;
	virtual void OnCreateDefaultLayout(CDockableContainer* pSender, QWidget* pAssetBrowser) override;
	// ~CAssetEditor

	void SaveState();
	void LoadState();

Q_SIGNALS:
	void SignalReleasingModel(CObjectModel* pModel);
	void SignalOpenedModel(CObjectModel* pModel);

protected Q_SLOTS:
	void OnCompileAll();
	void OnRefreshEnv();

	void OnLogWidgetDestruction(QObject* pObject);
	void OnPreviewWidgetDestruction(QObject* pObject);
	void OnScriptBrowserWidgetDestruction(QObject* pObject);
	void OnInspectorWidgetDestruction(QObject* pObject);
	void OnGraphViewWidgetDestruction(QObject* pObject);

private:
	void                             RegisterActions();
	void                             InitMenu();

	bool                     OnUndo();
	bool                     OnRedo();
	bool                     OnCopy();
	bool                     OnCut();
	bool                     OnPaste();
	bool                     OnDelete();

	void                             ConfigureLogs();
	void                             LoadSettings();
	void                             SaveSettings();

	void                             OnScriptBrowserSelection(const Schematyc::SScriptBrowserSelection& selection);

	void                             ClearLog();

	void                             ShowLogSettings();
	void                             ShowPreviewSettings();

	Schematyc::CLogWidget*           CreateLogWidget();
	Schematyc::CPreviewWidget*       CreatePreviewWidget();
	Schematyc::CScriptBrowserWidget* CreateScriptBrowserWidget();
	CInspectorLegacy*                CreateInspectorWidget();
	CGraphViewWidget*                CreateGraphViewWidget();

private:
	CAsset*                          m_pAsset;
	Schematyc::IScript*              m_pScript;

	Schematyc::CScriptBrowserWidget* m_pScriptBrowser;
	Schematyc::CScriptBrowserModel*  m_pModel;

	Schematyc::CLogWidget*           m_pLog;
	Schematyc::SLogSettings          m_logSettings;

	Schematyc::CPreviewWidget*       m_pPreview;
	Schematyc::CConnectionScope      m_connectionScope;

	QAction*                         m_pCompileAllMenuAction;
	QAction*                         m_pRefreshEnvironmentMenuAction;

	QCommandAction*                  m_pClearLogToolbarAction;
	QCommandAction*                  m_pShowLogSettingsToolbarAction;
	QCommandAction*                  m_pShowPreviewSettingsToolbarAction;

	CInspectorLegacy*                m_pInspector;
	CGraphViewWidget*                m_pGraphView;
};

} // Schematyc
