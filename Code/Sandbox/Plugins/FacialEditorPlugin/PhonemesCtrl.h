// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PhonemesCtrl_h__
#define __PhonemesCtrl_h__
#pragma once

#include <CryMath/ISplines.h>

struct IPhonemeLibrary;
struct SPhonemeInfo;

// Notify event sent when phoneme is being modified.
#define PHONEMECTRLN_CHANGE        (0x0001)
// Notify event sent just before when phoneme is modified.
#define PHONEMECTRLN_BEFORE_CHANGE (0x0002)
// Notify event sent just before when phoneme is modified.
#define PHONEMECTRLN_PREVIEW       (0x0003)
// The user has selected "clear all phonemes" from context menu
#define PHONEMECTRLN_CLEAR         (0x0004)
// The user wants to bake out the phonemes to curves.
#define PHONEMECTRLN_BAKE          (0x0005)

class IPhonemeUndoContext
{
public:
	//////////////////////////////////////////////////////////////////////////
	struct Phoneme
	{
		char sPhoneme[4]; // Phoneme name.

		// Start time and the length of the phoneme.
		int   time0;
		int   time1;
		float intensity;

		//////////////////////////////////////////////////////////////////////////
		bool bSelected;
		bool bActive;

		Phoneme() { bSelected = false; bActive = false; * sPhoneme = 0; time0 = time1 = 0; intensity = 1.0f; }
	};
	//////////////////////////////////////////////////////////////////////////

	virtual void SetPhonemes(const std::vector<std::vector<Phoneme>>& phonemes) = 0;
	virtual void GetPhonemes(std::vector<std::vector<Phoneme>>& phonemes) = 0;
	virtual void OnPhonemeChangesUnOrRedone() = 0;
};

//////////////////////////////////////////////////////////////////////////
// Spline control.
//////////////////////////////////////////////////////////////////////////
class CPhonemesCtrl : public CWnd, public IPhonemeUndoContext
{
public:
	DECLARE_DYNAMIC(CPhonemesCtrl)

	//////////////////////////////////////////////////////////////////////////
	struct Word
	{
		CString text; // word itself.
		// Start time and the length of the word.
		int     time0;
		int     time1;

		//////////////////////////////////////////////////////////////////////////
		bool bSelected;
		bool bActive;

		Word() { bSelected = false; bActive = false; time0 = time1 = 0; }
	};
	//////////////////////////////////////////////////////////////////////////

	CPhonemesCtrl();
	virtual ~CPhonemesCtrl();

	BOOL Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID);

	//Phoneme.
	void        InsertPhoneme(CPoint point, int phonemeId);
	void        RenamePhoneme(int sentenceIndex, int index, int phonemeId);
	void        TrackPoint(CPoint point);
	void        RemovePhoneme(int sentenceIndex, int phonemeId);
	void        ClearAllPhonemes();
	void        BakeLipsynchCurves();
	void        StartTracking();
	void        StopTracking();

	void        SetTimeMarker(float fTime);

	const char* GetMouseOverPhoneme();

	int         AddSentence();
	void        DeleteSentence(int sentenceIndex);
	int         GetSentenceCount();
	void        SetSentenceStartTime(int sentenceIndex, float startTime);
	void        SetSentenceEndTime(int sentenceIndex, float endTime);

	int         GetPhonemeCount(int sentenceIndex)   { return (int)m_sentences[sentenceIndex].phonemes.size(); }
	Phoneme&    GetPhoneme(int sentenceIndex, int i) { return m_sentences[sentenceIndex].phonemes[i]; }
	void        RemoveAllPhonemes(int sentenceIndex) { m_sentences[sentenceIndex].phonemes.clear(); };
	void        AddPhoneme(int sentenceIndex, Phoneme& ph);

	void        AddWord(int sentenceIndex, const Word& w);
	int         GetWordCount(int sentenceIndex)   { return m_sentences[sentenceIndex].words.size(); }
	Word&       GetWord(int sentenceIndex, int i) { return m_sentences[sentenceIndex].words[i]; }
	void        RemoveAllWords(int sentenceIndex) { m_sentences[sentenceIndex].words.clear(); }

	void        UpdatePhonemeLengths();

	void        UpdateCurrentActivePhoneme();

	void        SetZoom(float fZoom);
	void        SetScrollOffset(float fOrigin);
	float       GetZoom() const         { return m_fZoom; }
	float       GetScrollOffset() const { return m_fOrigin; }

	// IPhonemeUndoContext
	virtual void SetPhonemes(const std::vector<std::vector<Phoneme>>& phonemes);
	virtual void GetPhonemes(std::vector<std::vector<Phoneme>>& phonemes);
	virtual void OnPhonemeChangesUnOrRedone();

protected:
	enum EditMode
	{
		NothingMode = 0,
		SelectMode,
		TrackingMode,
	};
	enum EHitCode
	{
		HIT_NOTHING,
		HIT_PHONEME,
		HIT_EDGE_LEFT,
		HIT_EDGE_RIGHT
	};

	DECLARE_MESSAGE_MAP()

	virtual void PostNcDestroy();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu);

	// Drawing functions
	void                DrawPhonemes(CDC* pDC);
	void                DrawWords(CDC* pDC);
	void                DrawTimeMarker(CDC* pDC);

	EHitCode            HitTest(CPoint point);

	int                 TimeToClient(int time);
	int                 ClientToTime(int x);

	void                ClearSelection();

	void                SendNotifyEvent(int nEvent);

	void                AddPhonemes(CMenu& menu, int nBaseId);
	IPhonemeLibrary*    GetPhonemeLib();

	void                SetPhonemeTime(int sentenceIndex, int index, int t0, int t1);
	std::pair<int, int> PhonemeFromTime(int time);
	std::pair<int, int> WordFromTime(int time);

	void                StoreUndo();

private:
	struct Sentence
	{
		Sentence() : startTime(0.0f), endTime(0.0f) {}
		float                startTime;
		float                endTime;
		std::vector<Phoneme> phonemes;
		std::vector<Word>    words;
	};

	std::vector<Sentence> m_sentences;

	CRect                 m_rcClipRect;
	CRect                 m_rcPhonemes;
	CRect                 m_rcWords;
	CRect                 m_rcClient;
	CRect                 m_TimeUpdateRect;

	CPoint                m_LButtonDown;
	CPoint                m_hitPoint;
	EHitCode              m_hitCode;
	int                   m_nHitPhoneme;
	int                   m_nHitSentence;

	string                m_mouseOverPhoneme;

	float                 m_fTimeMarker;

	EditMode              m_editMode;

	float                 m_fZoom;
	float                 m_fOrigin;

	CToolTipCtrl          m_tooltip;
	CBitmap               m_offscreenBitmap;

	std::set<HMENU>       m_phonemePopupMenus;
};

#endif // __PhonemesCtrl_h__

