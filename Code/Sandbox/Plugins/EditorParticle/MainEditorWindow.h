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
class QPropertyTree;
class QToolBar;

class CInspector;
class CCurveEditorPanel;

namespace pfx2
{
	struct IParticleEffectPfx2;
}

namespace CryParticleEditor {

class CEffectAssetModel;
class CEffectAssetTabs;
class CEffectAssetWidget;

class CParticleEditor : public CAssetEditor, public IEditorNotifyListener
{
	Q_OBJECT

public:
	CParticleEditor();

	// CEditor
	virtual const char* GetEditorName() const override { return "Particle Editor"; };
	virtual void        SetLayout(const QVariantMap& state) override;
	virtual QVariantMap GetLayout() const override;
	// ~CEditor

	// IEditorNotifyListener
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) {}
	//~

	void Serialize(Serialization::IArchive& archive);

protected:
	// CAssetEditor
	virtual bool OnOpenAsset(CAsset* pAsset) override;
	virtual bool OnSaveAsset(CEditableAsset& editAsset) override;
	virtual void OnDiscardAssetChanges() override;
	virtual bool OnAboutToCloseAsset(string& reason) const override;
	virtual void OnCloseAsset() override;
	// ~CAssetEditor

	// CEditor
	virtual void CreateDefaultLayout(CDockableContainer* pSender) override;
	// ~CEditor

	void                       AssignToEntity(CBaseObject* pObject, const string& newAssetName);
	bool                       AssetSaveDialog(string* pOutputName);

	void                       OnShowEffectOptions();

private:
	void         InitMenu();
	void         InitToolbar(QVBoxLayout* pWindowLayout);
	void RegisterDockingWidgets();

	virtual bool OnUndo() override;
	virtual bool OnRedo() override;

protected Q_SLOTS:
	void SaveEffect(CEditableAsset& editAsset);
	void OnReloadEffect();
	void OnImportPfx1();
	void OnLoadFromSelectedEntity();
	void OnApplyToSelectedEntity();

	void OnEffectOptionsChanged();

	void OnNewComponent();

private:
	CEffectAssetWidget* CreateEffectAssetWidget();

private:
	std::shared_ptr<CEffectAssetModel> m_pEffectAssetModel;

	//
	QToolBar*           m_pEffectToolBar;
	CInspector*         m_pInspector;

	QAction*            m_pReloadEffectMenuAction;
	QAction*            m_pShowEffectOptionsMenuAction;
};

}

