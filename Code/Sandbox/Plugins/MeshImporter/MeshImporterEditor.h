// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DialogCommon.h"

// EditorCommon
#include <AssetSystem/AssetEditor.h>

#include <vector>

namespace MeshImporter
{

class CBaseDialog;

} // namespace MeshImporter

class QWidget;

//! Main frame of the mesh importer.
class CEditorAdapter : public CAssetEditor, public MeshImporter::IDialogHost
{
public:
	CEditorAdapter(std::unique_ptr<MeshImporter::CBaseDialog> pDialog, QWidget* pParent = nullptr);

	// IDialogHost implementation.
	virtual void Host_AddMenu(const char* menu) override;
	virtual void Host_AddToMenu(const char* menu, const char* command) override;

	// CDockableEditor implementation.

	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override;

protected:

	// CAssetEditor implementation.
	virtual bool OnOpenAsset(CAsset* pAsset) override;
	virtual bool OnSaveAsset(CEditableAsset& editAsset) override;
	virtual bool OnAboutToCloseAsset(string& reason) const override;
	virtual void OnCloseAsset() override;

	// CEditor implementation.
	virtual bool OnSave() override;
	virtual bool OnSaveAs() override;

	virtual void customEvent(QEvent* pEvent) override;
private:
	std::unique_ptr<MeshImporter::CBaseDialog> m_pDialog;
};

