// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Models/ParticleNodeTreeModel.h"

#include <IEditor.h>
#include <QDrag>

#include <CryCore/Platform/platform.h>
#include <CrySerialization/IArchive.h>
#include <CryParticleSystem/IParticles.h>

#include <AssetSystem/AssetEditor.h>

#include <NodeGraph/ICryGraphEditor.h>
#include <NodeGraph/NodeGraphView.h>

class QContainer;

class QModelIndex;
class QDockWidget;
class QPropertyTreeLegacy;
class QToolBar;

class CCurveEditorPanel;

namespace pfx2
{
struct IParticleEffect;
}

namespace CryParticleEditor
{

class CEffectAssetModel;
class CEffectAssetTabs;
class CEffectAssetWidget;

class CParticleEditor : public CAssetEditor, public IEditorNotifyListener
{
	Q_OBJECT

public:
	CParticleEditor();

	// CEditor
	virtual const char* GetEditorName() const override { return "Particle Editor"; }
	// ~CEditor

	// IEditorNotifyListener
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) {}
	//~

	void Serialize(Serialization::IArchive& archive);

protected:
	// CAssetEditor
	virtual bool                                  OnOpenAsset(CAsset* pAsset) override;
	virtual void                                  OnDiscardAssetChanges(CEditableAsset& editAsset) override;
	virtual bool                                  OnAboutToCloseAsset(string& reason) const override;
	virtual void                                  OnCloseAsset() override;
	virtual std::unique_ptr<IAssetEditingSession> CreateEditingSession() override;
	virtual void                                  OnCreateDefaultLayout(CDockableContainer* pSender, QWidget* pAssetBrowser) override;
	virtual void                                  OnInitialize() override;
	// ~CAssetEditor

	void AssignToEntity(CBaseObject* pObject, const string& newAssetName);
	bool AssetSaveDialog(string* pOutputName);

	void OnShowEffectOptions();

private:
	void         RegisterActions();
	void         InitMenu();

	bool OnReload();
	bool OnImport();

	bool OnCopy() { return false; }
	bool OnPaste() { return false; }
	bool OnDelete() { return false; }
	bool OnUndo();
	bool OnRedo();

protected Q_SLOTS:
	void OnLoadFromSelectedEntity();
	void OnApplyToSelectedEntity();

	void OnEffectOptionsChanged();

	void OnNewComponent();

private:
	std::unique_ptr<CEffectAssetModel> m_pEffectAssetModel;

	//
	CInspectorLegacy* m_pInspector;

	QAction*          m_pReloadEffectMenuAction;
	QAction*          m_pShowEffectOptionsMenuAction;
};

}
