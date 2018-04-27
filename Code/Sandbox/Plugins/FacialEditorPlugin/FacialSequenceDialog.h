// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FacialSequenceDialog_h__
#define __FacialSequenceDialog_h__
#pragma once

#include "Dialogs/ToolbarDialog.h"
#include "FacialSlidersCtrl.h"
#include "Controls/SplitterCtrl.h"
#include "Controls/SplineCtrlEx.h"
#include "Controls/TimelineCtrl.h"
#include "Audio/WaveGraphCtrl.h"
#include "PhonemesCtrl.h"

//////////////////////////////////////////////////////////////////////////
class CFacialChannelTreeCtrl : public CXTTreeCtrl
{
	DECLARE_DYNAMIC(CFacialChannelTreeCtrl)
public:
	CFacialChannelTreeCtrl();
	~CFacialChannelTreeCtrl();

	void                Reload();

	IFacialAnimChannel* GetSelectedChannel();

	void                SetContext(CFacialEdContext* pContext);
	void                SelectChannel(IFacialAnimChannel* pChannel, bool bExclusive);

	std::vector<string> m_selectedChannelIDs;

protected:
	DECLARE_MESSAGE_MAP()

private:
	CImageList                               m_imageList;
	std::map<IFacialAnimChannel*, HTREEITEM> m_itemsMap;
	CFacialEdContext*                        m_pContext;
};

//////////////////////////////////////////////////////////////////////////
class CFacialSequenceDialog : public CCustomFrameWnd, public IFacialEdListener, public ISplineSet, public IEditorNotifyListener
{
	DECLARE_DYNAMIC(CFacialSequenceDialog)
	friend class CSequenceDialogDropTarget;
public:
	CFacialSequenceDialog();
	~CFacialSequenceDialog();

	void                            SetContext(CFacialEdContext* pContext);
	void                            LipSync(int waveformIndex);
	void                            RemoveSoundEntry(int waveformIndex);
	void                            ClearAllSoundEntries();
	IFacialAnimChannel*             GetLipSyncGroup();
	IFacialAnimChannel*             GetBakedLipSyncGroup();
	bool                            DoPhonemeExtraction(CString wavFile, CString strText, struct IFacialAnimSequence* pSeq, int soundEntryIndex);
	_smart_ptr<IFacialAnimSequence> CreateSequenceWithSelectedExpressions();

	// ISplineSet implementation
	virtual ISplineInterpolator* GetSplineFromID(const string& id);
	virtual string               GetIDFromSpline(ISplineInterpolator* pSpline);
	virtual int                  GetSplineCount() const;
	virtual int                  GetKeyCountAtTime(float time, float threshold) const;

	// IEditorNotifyListener implementation
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);

	void         SetSmoothingSigma(float smoothingSigma)           { m_fSmoothingSigma = smoothingSigma; UpdateSmoothSplineSigma(); }
	float        GetSmoothingSigma() const                         { return m_fSmoothingSigma; }
	void         SetNoiseThreshold(float noiseThreshold)           { m_fNoiseThreshold = noiseThreshold; UpdateRemoveNoiseSigma(); }
	float        GetNoiseThreshold() const                         { return m_fNoiseThreshold; }
	void         SetKeyCleanupThreshold(float keyCleanupThreshold) { m_fKeyCleanupThreshold = keyCleanupThreshold; UpdateKeyCleanupErrorMax(); }
	float        GetKeyCleanupThreshold() const                    { return m_fKeyCleanupThreshold; }

	void         LoadSequenceSound(const string& filename);
	void         KeyAllSplines();
	void         SelectAllKeys();

protected:
	virtual void OnOK()     {};
	virtual void OnCancel() {};

	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	void         Update();

	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy);

	afx_msg void OnTreeRClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTreeSelChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTreeItemExpanded(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnAddSelectedEffector();
	afx_msg void OnNewFolder();
	afx_msg void OnRenameFolder();
	afx_msg void OnRemoveSelected();
	afx_msg void OnTimelineChangeStart(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTimelineChangeEnd(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTimelineChange(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTimelineDeletePressed(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnWaveCtrlTimeChange(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSplineScrollZoom(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSplineTimeChange(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnWaveScrollZoom(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnWaveRClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnBeginMoveWaveform(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnMoveWaveforms(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnResetWaveformChanges(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEndWaveformChanges(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSplineChange(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSplineBeginChange(NMHDR* pNMHDR, LRESULT* pResult);

	afx_msg void OnPhonemesBeforeChange(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnPhonemesChange(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnPhonemesPreview(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnPhonemesClear(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnPhonemesBake(NMHDR* pNMHDR, LRESULT* pResult);

	afx_msg void OnSplineCmd(UINT cmd);
	afx_msg void OnSplineCmdUpdateUI(CCmdUI* pCmdUI);

	afx_msg void OnPlay();
	afx_msg void OnPlayUpdate(CCmdUI* pCmdUI);
	afx_msg void OnStop();
	afx_msg void OnSequenceProperties();
	afx_msg void OnSpeedChange();
	afx_msg void OnFrameChange();

	afx_msg void OnZeroAll();
	afx_msg void OnKeyAll();
	afx_msg void OnSkeletonAnim();
	afx_msg void OnCameraAnim();
	afx_msg void OnOverlapSounds();
	afx_msg void OnControlAmplitude();
	afx_msg void OnSmoothSplineSigmaChange();
	afx_msg void OnKeyCleanupErrorMaxChange();
	afx_msg void OnRemoveNoiseSigmaChange();

	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDestroy();

	int          OnCreateControl(LPCREATECONTROLSTRUCT lpCreateControl);

	//////////////////////////////////////////////////////////////////////////
	void SelectChannel(IFacialAnimChannel* pChannel, bool bExclusive);
	void SelectChannels(int numChannels, IFacialAnimChannel** ppChannels, bool bExclusive = true);
	void OnSelectionChange();
	void SyncZoom();
	void ReloadPhonemeCtrl();

	void OnTimeChanged();
	void OnSoundChanged();
	void AddPhonemeStrengthChannel();
	void AddVertexDragChannel();
	void AddBalanceChannel();
	void AddCategoryBalanceChannel();
	void AddLipsyncCategoryStrengthChannel();
	void AddProceduralStrengthChannel();
	void CleanupKeys();
	void SmoothKeys();
	void RemoveNoiseFromKeys();
	void AddLayerToChannel();
	void DeleteLayerOfChannel();
	void CollapseLayersForChannel();
	void InsertShapeIntoChannel();
	void InsertVisimeSeriesIntoChannel();

protected:
	virtual void        OnFacialEdEvent(EFacialEdEvent event, IFacialEffector* pEffector, int nChannelCount, IFacialAnimChannel** ppChannels);

	IFacialAnimChannel* GetChannelFromSpline(ISplineInterpolator* pSpline);
	void                DisplayPlaybackSpeedInToolbar();
	float               ReadPlaybackSpeedFromToolbar();
	float               ReadFrameFromToolbar();
	float               ReadSmoothSigmaFromToolbar();
	float               ReadKeyCleanupErrorMaxFromToolbar();
	float               ReadRemoveNoiseSigmaFromToolbar();
	void                UpdateSkeletonAnimationStatus();
	void                UpdateCameraAnimationStatus();
	void                UpdateOverlapSoundStatus();
	void                UpdateControlAmplitudeStatus();
	void                UpdateSmoothSplineSigma();
	void                UpdateKeyCleanupErrorMax();
	void                UpdateRemoveNoiseSigma();

	void                ReloadWaveCtrlSounds();

private:
	CSplitterCtrl          m_splitWnd;
	CSplitterCtrl          m_splitWnd2;

	CFacialChannelTreeCtrl m_channelsCtrl;
	CSplineCtrlEx          m_splineCtrl;
	CTimelineCtrl          m_timelineCtrl;
	IFacialAnimChannel*    m_pCurrent;
	CXTPControlEdit*       m_pTimeEdit;
	CXTPControlEdit*       m_pFrameEdit;
	CXTPControlEdit*       m_pSpeedEdit;
	CXTPControlButton*     m_pAnimSkeletonButton;
	CXTPControlButton*     m_pAnimCameraButton;
	CXTPControlButton*     m_pOverlapSoundsButton;
	CXTPControlButton*     m_pControlAmplitudeButton;
	CXTPControlEdit*       m_pSmoothValueEdit;
	CXTPControlEdit*       m_pCleanupKeysValueEdit;
	CXTPControlEdit*       m_pRemoveNoiseValueEdit;
	CWaveGraphCtrl         m_waveCtrl;
	CPhonemesCtrl          m_phonemesCtrl;

	CFacialEdContext*      m_pContext;
	bool                   m_bIgnoreSplineChangeEvents;
	COleDropTarget*        m_pDropTarget;

	float                  m_fSmoothingSigma;
	float                  m_fNoiseThreshold;
	float                  m_fKeyCleanupThreshold;
};

#endif // __FacialSequenceDialog_h__

