// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  Created:     5/6/2002 by Timur.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "Controls/ColorCtrl.h"
#include "Util/Variable.h"

//! All possible property types.
enum PropertyType
{
	ePropertyInvalid = 0,
	ePropertyTable   = 1,
	ePropertyBool    = 2,
	ePropertyInt,
	ePropertyFloat,
	ePropertyVector2,
	ePropertyVector,
	ePropertyVector4,
	ePropertyString,
	ePropertyColor,
	ePropertyAngle,
	ePropertyFloatCurve,
	ePropertyColorCurve,
	ePropertyFile,
	ePropertyTexture,
	ePropertyAnimation,
	ePropertyModel,
	ePropertySelection,
	ePropertyList,
	ePropertyShader,
	ePropertyMaterial,
	ePropertyAiBehavior,
	ePropertyAiAnchor,
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	ePropertyAiCharacter,
#endif
	ePropertyAiPFPropertiesList,
	ePropertyAiEntityClasses,
	ePropertyAiTerritory,
	ePropertyAiWave,
	ePropertyEquip,
	ePropertyReverbPreset,
	ePropertyLocalString,
	ePropertySOClass,
	ePropertySOClasses,
	ePropertySOState,
	ePropertySOStates,
	ePropertySOStatePattern,
	ePropertySOAction,
	ePropertySOHelper,
	ePropertySONavHelper,
	ePropertySOAnimHelper,
	ePropertySOEvent,
	ePropertySOTemplate,
	ePropertyCustomAction,
	ePropertyGameToken,
	ePropertySequence,
	ePropertyMissionObj,
	ePropertyUser,
	ePropertySequenceId,
	ePropertyFlare,
	ePropertyParticleName,
	ePropertyGeomCache,
	ePropertyAudioTrigger,
	ePropertyAudioSwitch,
	ePropertyAudioSwitchState,
	ePropertyAudioRTPC,
	ePropertyAudioEnvironment,
	ePropertyAudioPreloadRequest,
	ePropertyDynamicResponseSignal,
};

// forward declarations.
class CNumberCtrl;
class CPropertyCtrl;
class CInPlaceEdit;
class CInPlaceComboBox;
class CFillSliderCtrl;
class CSplineCtrl;
class CColorGradientCtrl;
class CSliderCtrlEx;
class CInPlaceColorButton;
struct IVariable;

class CInPlaceButton;
class CInPlaceCheckBox;

/** Item of CPropertyCtrl.
    Every property item reflects value of single XmlNode.
 */
class PLUGIN_API CPropertyItem : public _i_reference_target_t
{
public:
	typedef std::vector<string>        TDValues;
	typedef std::map<string, TDValues> TDValuesContainer;

protected:
private:

public:
	// Variables.
	// Constructors.
	CPropertyItem(CPropertyCtrl* pCtrl);
	virtual ~CPropertyItem();

	//! Set xml node to this property item.
	virtual void SetXmlNode(XmlNodeRef& node);

	//! Set variable.
	virtual void SetVariable(IVariable* var);

	//! Get Variable.
	IVariable* GetVariable() const { return m_pVariable; }

	/** Get type of property item.
	 */
	virtual int GetType() { return m_type; }

	/** Get name of property item.
	 */
	virtual CString GetName() const { return m_name; };

	/** Set name of property item.
	 */
	virtual void SetName(const char* sName) { m_name = sName; };

	/** Called when item becomes selected.
	 */
	virtual void SetSelected(bool selected);

	/** Get if item is selected.
	 */
	bool IsSelected() const { return m_bSelected; };

	/** Get if item is currently expanded.
	 */
	bool IsExpanded() const { return m_bExpanded; };

	/** Get if item can be expanded (Have children).
	 */
	bool IsExpandable() const { return m_bExpandable; };

	/** Check if item cannot be category.
	 */
	bool IsNotCategory() const { return m_bNoCategory; };

	/** Check if item must be bold.
	 */
	bool IsBold() const;

	/** Check if item must be disabled.
	 */
	bool IsDisabled() const;

	/** Check if item must be drawn.
	 */
	bool IsInvisible() const;

	/** Get height of this item.
	 */
	virtual int GetHeight();

	/** Called by PropertyCtrl to draw value of this item.
	 */
	virtual void DrawValue(CDC* dc, CRect rect);

	/** Called by PropertyCtrl when item selected to creare in place editing control.
	 */
	virtual void CreateInPlaceControl(CWnd* pWndParent, CRect& rect);
	/** Called by PropertyCtrl when item deselected to destroy in place editing control.
	 */
	virtual void DestroyInPlaceControl(bool bRecursive = false);

	virtual void CreateControls(CWnd* pWndParent, CRect& textRect, CRect& ctrlRect);

	/** Move in place control to new position.
	 */
	virtual void MoveInPlaceControl(const CRect& rect);

	// Enable/Disables all controls
	virtual void EnableControls(bool bEnable);

	/** Set Focus to inplace control.
	 */
	virtual void SetFocus();

	/** Set data from InPlace control to Item value.
	 */
	virtual void SetData(CWnd* pWndInPlaceControl) {};

	//////////////////////////////////////////////////////////////////////////
	// Mouse notifications.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnLButtonDown(UINT nFlags, CPoint point);
	virtual void OnRButtonDown(UINT nFlags, CPoint point) {};
	virtual void OnLButtonDblClk(UINT nFlags, CPoint point);
	virtual void OnMouseWheel(UINT nFlags, short zDelta, CPoint point);

	/** Changes value of item.
	 */
	virtual void SetValue(const char* sValue, bool bRecordUndo = true, bool bForceModified = false);

	/** Returns current value of property item.
	 */
	virtual const char* GetValue() const;

	/** Get Item's XML node.
	 */
	XmlNodeRef& GetXmlNode() { return m_node; };

	//////////////////////////////////////////////////////////////////////////
	//! Get description of this item.
	CString GetTip() const;

	//! Return image index of this property.
	int GetImage() const { return m_image; };

	//! Return true if this property item is modified.
	bool IsModified() const { return m_modified; }

	bool HasDefaultValue(bool bChildren = false) const;

	/** Get script default value of property item.
	 */
	virtual bool HasScriptDefault() const { return m_strScriptDefault != m_strNoScriptDefault; };

	/** Get script default value of property item.
	 */
	virtual CString GetScriptDefault() const { return m_strScriptDefault; };

	/** Set script default value of property item.
	 */
	virtual void SetScriptDefault(const char* sScriptDefault) { m_strScriptDefault = sScriptDefault; };

	/** Set script default value of property item.
	 */
	virtual void ClearScriptDefault() { m_strScriptDefault = m_strNoScriptDefault; };

	//////////////////////////////////////////////////////////////////////////
	// Childs.
	//////////////////////////////////////////////////////////////////////////
	//! Expand child nodes.
	virtual void SetExpanded(bool expanded);

	//! Reload Value from Xml Node (hierarchicaly reload children also).
	virtual void ReloadValues();

	//! Get number of child nodes.
	int            GetChildCount() const     { return m_childs.size(); };
	//! Get Child by id.
	CPropertyItem* GetChild(int index) const { return m_childs[index]; };

	//! Parent of this item.
	CPropertyItem* GetParent() const { return m_parent; };

	//! Add Child item.
	void AddChild(CPropertyItem* item);

	//! Delete child item.
	void RemoveChild(CPropertyItem* item);

	//! Delete all child items
	void RemoveAllChildren();

	//! Find item that reference specified property.
	CPropertyItem*         FindItemByVar(IVariable* pVar);
	//! Get full name, including names of all parents.
	virtual CString        GetFullName() const;
	//! Find item by full specified item.
	CPropertyItem*         FindItemByFullName(const CString& name);

	void                   ReceiveFromControl();

	CFillSliderCtrl* const GetFillSlider() const { return m_cFillSlider; }
	void                   EnableNotifyWithoutValueChange(bool bFlag);

protected:
	//////////////////////////////////////////////////////////////////////////
	// Private methods.
	//////////////////////////////////////////////////////////////////////////
	void SendToControl();
	void CheckControlActiveColor();
	void RepositionWindow(CWnd* pWnd, CRect rc);

	void OnChildChanged(CPropertyItem* child);

	void OnEditChanged();
	void OnNumberCtrlUpdate(CNumberCtrl* ctrl);
	void OnFillSliderCtrlUpdate(CSliderCtrlEx* ctrl);
	void OnNumberCtrlBeginUpdate(CNumberCtrl* ctrl) {};
	void OnNumberCtrlEndUpdate(CNumberCtrl* ctrl)   {};
	void OnSplineCtrlUpdate(CSplineCtrl* ctrl);

	void OnComboSelection();

	void OnCheckBoxButton();
	void OnColorBrowseButton();
	void OnColorChange(COLORREF col);
	void OnFileBrowseButton();
	void OnTextureEditButton();
	void OnPsdEditButton();
	void OnAnimationApplyButton();
	void OnEditDeprecatedProperty();
	void OnMaterialBrowseButton();
	void OnMaterialPickSelectedButton();
	void OnSequenceBrowseButton();
	void OnSequenceIdBrowseButton();
	void OnMissionObjButton();
	void OnUserBrowseButton();
	void OnLocalStringBrowseButton();
	void OnExpandButton();

	void OnResourceSelectorButton();

	void RegisterCtrl(CWnd* pCtrl);

	void ParseXmlNode(bool bRecursive = true);

	//! String to color.
	COLORREF StringToColor(const CString& value);
	//! String to boolean.
	bool     GetBoolValue();

	//! Convert variable value to value string.
	void    VarToValue();
	CString GetDrawValue();

	//! Convert from value to variable.
	void ValueToVar();

	//! Release used variable.
	void      ReleaseVariable();
	//! Callback called when variable change.
	void      OnVariableChange(IVariable* var);

	TDValues* GetEnumValues(const char* strPropertyName);

	void      AddChildrenForPFProperties();
	void      AddChildrenForAIEntityClasses();
	//void      PopulateAITerritoriesList();
	//void      PopulateAIWavesList();

private:
	CString      m_name;
	PropertyType m_type;

	CString      m_value;

	//////////////////////////////////////////////////////////////////////////
	// Flags for this property item.
	//////////////////////////////////////////////////////////////////////////
	//! True if item selected.
	unsigned int m_bSelected           : 1;
	//! True if item currently expanded
	unsigned int m_bExpanded           : 1;
	//! True if item can be expanded
	unsigned int m_bExpandable         : 1;
	//! True if children can be edited in parent
	unsigned int m_bEditChildren       : 1;
	//! True if children displayed in parent field.
	unsigned int m_bShowChildren       : 1;
	//! True if item can not be category.
	unsigned int m_bNoCategory         : 1;
	//! If tru ignore update that comes from childs.
	unsigned int m_bIgnoreChildsUpdate : 1;
	//! If can move in place controls.
	unsigned int m_bMoveControls       : 1;
	//! True if item modified.
	unsigned int m_modified            : 1;
	//! True if list-box is not to be sorted.
	unsigned int m_bUnsorted           : 1;

	bool         m_bForceModified;

	// Used for number controls.
	float m_rangeMin;
	float m_rangeMax;
	float m_step;
	bool  m_bHardMin, m_bHardMax;   // Values really limited by this range.
	int   m_nHeight;

	// Xml node.
	XmlNodeRef m_node;

	//! Pointer to the variable for this item.
	_smart_ptr<IVariable> m_pVariable;

	//////////////////////////////////////////////////////////////////////////
	// InPlace controls.
	CColorCtrl<CStatic>* m_pStaticText;
	CNumberCtrl*         m_cNumber;
	CNumberCtrl*         m_cNumber1;
	CNumberCtrl*         m_cNumber2;
	CNumberCtrl*         m_cNumber3;
	CFillSliderCtrl*     m_cFillSlider;
	CInPlaceEdit*        m_cEdit;
	CSplineCtrl*         m_cSpline;
	CColorGradientCtrl*  m_cColorSpline;
	CInPlaceComboBox*    m_cCombo;
	CInPlaceButton*      m_cButton;
	CInPlaceButton*      m_cButton2;
	CInPlaceButton*      m_cButton3;
	CInPlaceButton*      m_cExpandButton;
	CInPlaceButton*      m_cButton4;
	CInPlaceButton*      m_cButton5;
	CInPlaceCheckBox*    m_cCheckBox;
	CInPlaceColorButton* m_cColorButton;
	//////////////////////////////////////////////////////////////////////////
	std::vector<CWnd*>   m_controls;

	//! Owner property control.
	CPropertyCtrl* m_propertyCtrl;

	//! Parent item.
	CPropertyItem* m_parent;

	// Enum.
	IVarEnumListPtr         m_enumList;
	CUIEnumsDatabase_SEnum* m_pEnumDBItem;

	CString                 m_tip;
	int                     m_image;

	float                   m_valueMultiplier;

	// Childs.
	typedef std::vector<_smart_ptr<CPropertyItem>> Childs;
	Childs m_childs;

	//////////////////////////////////////////////////////////////////////////
	friend class CPropertyCtrlEx;
	int         m_nCategoryPageId;

	CWnd*       m_pControlsHostWnd;
	CRect       m_rcText;
	CRect       m_rcControl;

	static HDWP s_HDWP;
	CString     m_strNoScriptDefault;
	CString     m_strScriptDefault;
};

typedef _smart_ptr<CPropertyItem> CPropertyItemPtr;

