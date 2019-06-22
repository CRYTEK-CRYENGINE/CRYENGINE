// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Controller.h"
#include <AssetSystem/AssetEditor.h>

#include <Cry3DEngine/ITimeOfDay.h>

class CEnvironmentEditor : public CAssetEditor
{
	Q_OBJECT
public:
	CEnvironmentEditor();

	ITimeOfDay::IPreset* GetPreset() const;
	string               GetPresetFileName() const;
	static ITimeOfDay*   GetTimeOfDay();

private:
	void RegisterActions();
	bool OnUndo() { return false; }
	bool OnRedo() { return false; }

	virtual const char*                           GetEditorName() const override        { return "Environment Editor"; }

	virtual std::unique_ptr<IAssetEditingSession> CreateEditingSession() override;
	virtual bool                                  OnOpenAsset(CAsset* pAsset) override;
	virtual void                                  OnCloseAsset() override;
	virtual void                                  OnDiscardAssetChanges(CEditableAsset& editAsset) override;
	virtual void                                  OnInitialize() override;
	virtual void                                  OnCreateDefaultLayout(CDockableContainer* pSender, QWidget* pAssetBrowser) override;

	virtual bool                                  event(QEvent* event) override;

private:
	ITimeOfDay::IPreset* m_pPreset;
	string               m_presetFileName;
	CController          m_controller;
};
