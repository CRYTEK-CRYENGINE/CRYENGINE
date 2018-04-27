// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FacialPreviewDialog_h__
#define __FacialPreviewDialog_h__
#pragma once

#include "Dialogs/ToolbarDialog.h"
#include "FacialEdContext.h"

class CModelViewport;
class CFacialEdContext;
class CModelViewportFE;
class QMfcViewportHost;
class CVarObject;

enum EyeType
{
	EYE_LEFT,
	EYE_RIGHT
};

//////////////////////////////////////////////////////////////////////////
class CFacialPreviewDialog : public CToolbarDialog, public IFacialEdListener
{
	DECLARE_DYNAMIC(CFacialPreviewDialog)
public:
	static void RegisterViewClass();

	CFacialPreviewDialog();
	~CFacialPreviewDialog();

	void              SetContext(CFacialEdContext* pContext);
	void              RedrawPreview();

	CModelViewport*   GetViewport();
	CVarObject* GetVarObject() { return &m_vars; }

	void              SetForcedNeckRotation(const Quat& rotation);
	void              SetForcedEyeRotation(const Quat& rotation, EyeType eye);

	void              SetAnimateCamera(bool bAnimateCamera);
	bool              GetAnimateCamera() const;

protected:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnOK()     {};
	virtual void OnCancel() {};

	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);

	afx_msg void OnCenterOnHead();

private:
	virtual void OnFacialEdEvent(EFacialEdEvent event, IFacialEffector* pEffector, int nChannelCount, IFacialAnimChannel** ppChannels);

	void         OnLookIKChanged(IVariable* var);
	void         OnLookIKEyesOnlyChanged(IVariable* var);
	void         OnShowEyeVectorsChanged(IVariable* var);
	void         OnLookIKOffsetChanged(IVariable* var);
	void         OnProceduralAnimationChanged(IVariable* var);

	CModelViewportFE* m_pModelViewport;
	QMfcViewportHost* m_pViewportHost;
	CXTPToolBar       m_wndToolBar;

	CFacialEdContext* m_pContext;

	CVariable<bool>   m_bLookIK;
	CVariable<bool>   m_bLookIKEyesOnly;
	CVariable<bool>   m_bShowEyeVectors;
	CVariable<float>  m_fLookIKOffsetX;
	CVariable<float>  m_fLookIKOffsetY;
	CVariable<bool>   m_bProceduralAnimation;
	CVarObject        m_vars;
	HACCEL            m_hAccelerators;
};

#endif // __FacialPreviewDialog_h__

