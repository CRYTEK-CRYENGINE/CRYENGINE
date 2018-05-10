// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SequencerKeyPropertiesDlg_h__
#define __SequencerKeyPropertiesDlg_h__
#pragma once

#if _MSC_VER > 1000
	#pragma once
#endif

#include "Controls\PropertyCtrl.h"
#include "Controls/NumberCtrl.h"
#include "SequencerUtils.h"
#include "SequencerDopeSheetBase.h"
#include "SequencerSequence.h"


class CSequencerKeyPropertiesDlg;

//////////////////////////////////////////////////////////////////////////
class CSequencerKeyUIControls : public CObject, public _i_reference_target_t
{
	DECLARE_DYNAMIC(CSequencerKeyUIControls)
public:
	typedef CSequencerUtils::SelectedKey  SelectedKey;
	typedef CSequencerUtils::SelectedKeys SelectedKeys;

	CSequencerKeyUIControls() { m_pVarBlock = new CVarBlock; };

	void SetKeyPropertiesDlg(CSequencerKeyPropertiesDlg* pDlg) { m_pKeyPropertiesDlg = pDlg; }

	// Return internal variable block.
	CVarBlock* GetVarBlock() const { return m_pVarBlock; }

	//////////////////////////////////////////////////////////////////////////
	// Callbacks that must be implemented in derived class
	//////////////////////////////////////////////////////////////////////////
	// Returns true if specified animation track type is supported by this UI.
	virtual bool SupportTrackType(ESequencerParamType type) const = 0;

	// Called when UI variable changes.
	void CreateVars()
	{
		m_pVarBlock->DeleteAllVariables();
		OnCreateVars();
	}

	// Called when user changes selected keys.
	// Return true if control update UI values
	virtual bool OnKeySelectionChange(SelectedKeys& keys) = 0;

	// Called when UI variable changes.
	virtual void OnUIChange(IVariable* pVar, SelectedKeys& keys) = 0;

	void         OnInternalVariableChange(IVariable* pVar);

protected:

	virtual void OnCreateVars() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Helper functions.
	//////////////////////////////////////////////////////////////////////////
	template<class T, class U>
	static void SyncValue(CSmartVariable<T>& var, U& value, bool bCopyToUI, IVariable* pSrcVar = NULL)
	{
		if (bCopyToUI)
			var = value;
		else
		{
			if (!pSrcVar || pSrcVar == var.GetVar())
				value = var;
		}
	}
	void AddVariable(CVariableBase& varArray, IVariable& var, const char* varName, unsigned char dataType = IVariable::DT_SIMPLE)
	{
		if (varName)
			var.SetName(varName);
		var.SetDataType(dataType);
		var.AddOnSetCallback(functor(*this, &CSequencerKeyUIControls::OnInternalVariableChange));
		varArray.AddVariable(&var);
	}
	//////////////////////////////////////////////////////////////////////////
	void AddVariable(CVariableBase& var, const char* varName, unsigned char dataType = IVariable::DT_SIMPLE)
	{
		if (varName)
			var.SetName(varName);
		var.SetDataType(dataType);
		var.AddOnSetCallback(functor(*this, &CSequencerKeyUIControls::OnInternalVariableChange));
		m_pVarBlock->AddVariable(&var);
	}

	void RefreshSequencerKeys();

protected:
	_smart_ptr<CVarBlock>       m_pVarBlock;
	CSequencerKeyPropertiesDlg* m_pKeyPropertiesDlg;
};

//////////////////////////////////////////////////////////////////////////
class CSequencerTrackPropsDlg : public CDialog
{
public:
	CSequencerTrackPropsDlg();   // standard constructor

	void SetSequence(CSequencerSequence* pSequence);
	bool OnKeySelectionChange(CSequencerUtils::SelectedKeys& keys);
	void ReloadKey();

	// Dialog Data
	enum { IDD = IDD_TV_TRACK_PROPERTIES };

protected:
	void         SetCurrKey(int nkey);

	virtual BOOL OnInitDialog();
	virtual void OnOK()     {};
	virtual void OnCancel() {};
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	afx_msg void OnDeltaposPrevnext(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnUpdateTime();
	afx_msg void OnDestroy();

	DECLARE_MESSAGE_MAP()

	CSpinButtonCtrl m_keySpinBtn;
	CStatic                     m_keynum;

	_smart_ptr<CSequencerTrack> m_track;
	int                         m_key;
	CNumberCtrl                 m_time;
};

//////////////////////////////////////////////////////////////////////////
class SequencerKeys;
class CSequencerKeyPropertiesDlg : public CDialog, public ISequencerEventsListener
{
public:
	CSequencerKeyPropertiesDlg(bool overrideConstruct = false);

	enum { IDD = IDD_PANEL_PROPERTIES };

	void SetKeysCtrl(CSequencerDopeSheetBase* pKeysCtrl)
	{
		m_keysCtrl = pKeysCtrl;
		if (m_keysCtrl)
			m_keysCtrl->SetKeyPropertiesDlg(this);
	}

	void                SetSequence(CSequencerSequence* pSequence);
	CSequencerSequence* GetSequence() { return m_pSequence; };

	void                PopulateVariables();
	void                PopulateVariables(CPropertyCtrl& propCtrl);

	//////////////////////////////////////////////////////////////////////////
	// ISequencerEventsListener interface implementation
	//////////////////////////////////////////////////////////////////////////
	virtual void OnKeySelectionChange();
	//////////////////////////////////////////////////////////////////////////

	void CreateAllVars();

	void SetVarValue(const char* varName, const char* varValue);

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy);

	virtual BOOL OnInitDialog();
	//virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnOK()     {};
	virtual void OnCancel() {};

	//////////////////////////////////////////////////////////////////////////
	void OnVarChange(IVariable* pVar);

	void AddVars(CSequencerKeyUIControls* pUI);
	void ReloadValues();

protected:
	std::vector<_smart_ptr<CSequencerKeyUIControls>> m_keyControls;

	_smart_ptr<CVarBlock>                            m_pVarBlock;
	_smart_ptr<CSequencerSequence>                   m_pSequence;

	CPropertyCtrl           m_wndProps;
	CSequencerTrackPropsDlg m_wndTrackProps;
	//CTcbPreviewCtrl m_wndTcbPreview;

	CSequencerDopeSheetBase* m_keysCtrl;

	CSequencerTrack*         m_pLastTrackSelected;
};

#endif //__SequencerKeyPropertiesDlg_h__

