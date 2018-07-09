// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek, 2014.
// -------------------------------------------------------------------------
//  File name:   QViewportHeader.h
//  Version:     v1.00
//  Created:     15/10/2014 by timur.
//  Description: Viewport header widget
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ViewportHeader_h__
#define __ViewportHeader_h__
#pragma once

#include <QFrame>
#include <QMenu>
#include <QDialog>

class QToolButton;
class QLabel;
class QLineEdit;
class QSpinBox;
class CPopupMenuItem;
class CLevelEditorViewport;
class QPushButton;
class QViewportHeader;
class CameraSpeedPopup;

//////////////////////////////////////////////////////////////////////////

class QTerrainSnappingMenu : public QMenu
{
	Q_OBJECT
public:
	QTerrainSnappingMenu(QToolButton* pToolButton, QViewportHeader* pViewportHeader);

protected:
	void     mouseReleaseEvent(QMouseEvent* pEvent);
	QAction* AddCommandAction(const char* szCommand, QViewportHeader* pViewportHeader);

protected Q_SLOTS:
	void RefreshParentToolButton();

private:
	QToolButton* m_pParentToolButton;
};

//////////////////////////////////////////////////////////////////////////
class QViewportHeader : public QFrame, public IEditorNotifyListener
{
	Q_OBJECT

public:
	QViewportHeader(CLevelEditorViewport* pViewport);
	virtual ~QViewportHeader();

	void UpdateCameraName();

	enum ESearchResultHandling
	{
		ESRH_HIDE_OTHERS = 0,
		ESRH_FREEZE_OTHERS,
		ESRH_JUST_SELECT,
	};

	enum ESearchMode
	{
		ESM_BY_NAME = 0,
		ESM_BY_TYPE,
		ESM_BY_ASSET,
	};

	void                       AddFOVMenus(CPopupMenuItem& menu, Functor1<float> setFOVFunc, const std::vector<string>& customPresets);
	void                       AddAspectRatioMenus(CPopupMenuItem& menu, Functor2<unsigned int, unsigned int> setAspectRatioFunc, const std::vector<string>& customPresets);
	void                       AddResolutionMenus(CPopupMenuItem& menu, Functor2<int, int> resizeFunc, const std::vector<string>& customPresets);
	static void                UpdateCustomPresets(const string& text, std::vector<string>& custompresets);

	static void                LoadCustomPresets(const char* keyName, std::vector<string>& outCustompresets);
	static void                SaveCustomPresets(const char* keyName, const std::vector<string>& custompresets);

	const std::vector<string>& GetCustomResolutionPresets() const  { return m_customResPresets; }
	const std::vector<string>& GetCustomFOVPresets() const         { return m_customFOVPresets; }
	const std::vector<string>& GetCustomAspectRatioPresets() const { return m_customAspectRatioPresets; }

	void                       OnMenuFOVCustom();
	void                       OnMenuResolutionCustom();
	void                       OnCycleDisplayInfo(QAction* cycleDisplayInfo);

	void                       OnMenuResolutionCustomClear();
	void                       SavePersonalization();

Q_SIGNALS:
	void cameraSpeedChanged(float speed, int increments);

protected:
	virtual void  OnEditorNotifyEvent(EEditorNotifyEvent event);
	virtual QSize sizeHint() const;
	//virtual QSize minimumSizeHint() const { return sizeHint(); }

private:
	void         LoadPersonalization();
	QToolButton* AddToolButtonForCommand(const char* szCommand, QLayout* pLayout);
	void         UnhideUnfreezeAll();
	void         SearchByType(const std::vector<string>& terms);
	void         SearchByName(const std::vector<string>& terms);
	void         SearchByAsset(const std::vector<string>& terms);
	void         UpdateSearchOptionsText();
	void         InputNamesToSearchFromSelection();

	void         SetViewportFOV(float fov);
	float        GetViewportFOV();

	void         OnMenuViewSelected(struct IViewPaneClass* viewClass);

	friend class QViewportTitleMenu;
private:
	QToolButton* m_titleBtn;
	QToolButton* m_toggleHelpersBtn;
	QToolButton* m_displayOptions;

	// Snapping
	QToolButton*          m_pivotSnapping;
	QToolButton*          m_vertexSnapping;
	QString               m_moduleName;

	QLabel*               m_searchOptionsText;
	QLineEdit*            m_viewportSearch;
	ESearchMode           m_searchMode;
	ESearchResultHandling m_searchResultHandling;
	bool                  m_bOR;

	CLevelEditorViewport*            m_viewport;

	static const int      MAX_NUM_CUSTOM_PRESETS = 10;
	std::vector<string>   m_customResPresets;
	std::vector<string>   m_customFOVPresets;
	std::vector<string>   m_customAspectRatioPresets;

	// current tool info and exit button
	QLabel*      m_currentToolText;
	QToolButton* m_currentToolExit;

	string       m_viewPaneClass;
	uint64       m_displayInfoFuncIdx;
};

#endif //__ViewportHeader_h__

