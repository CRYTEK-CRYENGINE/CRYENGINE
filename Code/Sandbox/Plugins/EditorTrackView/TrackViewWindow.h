// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Header File.
// Copyright (C), Crytek, 1999-2015.

#pragma once

#include "EditorFramework/Editor.h"

#include <CryExtension/CryGUID.h>
#include <QtViewPane.h>

#include "TrackViewCore.h"

class CTrackViewCore;

class CTrackViewWindow : public CDockableEditor
{
	Q_OBJECT

public:
	CTrackViewWindow(QWidget* pParent = nullptr);
	~CTrackViewWindow();

	static std::vector<CTrackViewWindow*> GetTrackViewWindows() { return ms_trackViewWindows; }

	// CEditor
	virtual const char* GetEditorName() const override { return "Track View"; };

	virtual void        SetLayout(const QVariantMap& state) override;
	virtual QVariantMap GetLayout() const override;
	// ~CEditor
	
private:

	void         InitMenu();

	virtual bool OnNew() override;
	virtual bool OnOpen() override;
	virtual bool OnClose() override;
	virtual bool OnUndo() override;
	virtual bool OnRedo() override;
	virtual bool OnCopy() override;
	virtual bool OnCut() override;
	virtual bool OnPaste() override;
	virtual bool OnDelete() override;
	virtual bool OnDuplicate() override;
	virtual bool OnZoomIn() override;
	virtual bool OnZoomOut() override;

	void         OnExportSequence();
	void         OnImportSequence();
	void         OnUnitsChanged(SAnimTime::EDisplayMode mode);
	void         OnRender();
	void         OnDeleteSequence();
	void         OnSequenceProperties();
	void         OnNewEvent();
	void         OnShowAllEvents();
	void         OnLightAnimationSet();

	virtual void keyPressEvent(QKeyEvent* e) override;

	CTrackViewCore*                       m_pTrackViewCore;
	static std::vector<CTrackViewWindow*> ms_trackViewWindows;
};

