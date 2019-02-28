// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorFramework/Editor.h"
#include "ILevelEditor.h"

class QMenu;
class CDockableDialog;

// This is a stub class that is meant to encapsulate all editor logic for the level editor that is currently in CryEdit.
// Ultimately the level editor will be just another window and all its logic should be encapsulated, while CryEdit should retain global functionnality
// Currently the transition will only happen for the parts of the code being changed
class CLevelEditor : public CEditor, public ILevelEditor, public IAutoEditorNotifyListener
{
	Q_OBJECT
public:
	CLevelEditor();
	~CLevelEditor();

	virtual void        customEvent(QEvent* pEvent) override;

	void                OnEditorNotifyEvent(EEditorNotifyEvent event) override;
	void                CreateRecentFilesMenu(QMenu* pRecentFilesMenu);
	virtual const char* GetEditorName() const override { return "Level Editor"; }

	// snapping
	void EnableVertexSnapping(bool bEnable);
	void EnablePivotSnapping(bool bEnable);
	void EnableGridSnapping(bool bEnable);
	void EnableAngleSnapping(bool bEnable);
	void EnableScaleSnapping(bool bEnable);

	void EnableTerrainSnapping(bool bEnable);
	void EnableGeometrySnapping(bool bEnable);
	void EnableSurfaceNormalSnapping(bool bEnable);
	void EnableHelpersDisplay(bool bEnable);

	bool IsVertexSnappingEnabled() const;
	bool IsPivotSnappingEnabled() const;

	bool IsGridSnappingEnabled() const;
	bool IsAngleSnappingEnabled() const;
	bool IsScaleSnappingEnabled() const;
	bool IsTerrainSnappingEnabled() const;
	bool IsGeometrySnappingEnabled() const;
	bool IsSurfaceNormalSnappingEnabled() const;
	bool IsHelpersDisplayed() const;

	bool ConvertEditorXmlToLevelAssetType(const string& levelFolder, const char* const szName) const;

	//ILevelEditor.h interface
	virtual bool IsLevelLoaded() override;
	//end ILevelEditor.h

private:
	virtual bool IsOnlyBackend() const override { return true; }

	virtual bool OnNew() override;
	virtual bool OnOpen() override;
	virtual bool OnSave() override;
	virtual bool OnSaveAs() override;
	virtual bool OnDelete() override;
	virtual bool OnDuplicate() override;

	virtual bool OnFind() override;
	virtual bool OnCut() override;
	virtual bool OnCopy() override;
	virtual bool OnPaste() override;

	void OnCopyInternal(bool isCut = false);
	void OnToggleAssetBrowser();

	void SaveCryassetFile(const string& levelPath);

Q_SIGNALS:
	void VertexSnappingEnabled(bool bEnable);
	void PivotSnappingEnabled(bool bEnable);
	void GridSnappingEnabled(bool bEnable);
	void AngleSnappingEnabled(bool bEnable);
	void ScaleSnappingEnabled(bool bEnable);

	void TerrainSnappingEnabled(bool bEnable);
	void GeometrySnappingEnabled(bool bEnable);
	void SurfaceNormalSnappingEnabled(bool bEnable);
	void LevelLoaded(const QString& levelPath);
	void HelpersDisplayEnabled(bool bEnable);

private:
	CDockableDialog* m_findWindow;
	CDockableDialog* m_assetBrowser;
};

