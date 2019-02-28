// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include <IEditor.h>

struct ITimeOfDay;
struct SCurveEditorContent;
struct SAnimTime;

#include <CrySerialization/Forward.h>
#include "QPropertyTree/ContextList.h"

enum ETODParamType
{
	eFloatType,
	eColorType
};

struct STODParameterGroup;
struct STODParameter
{
public:
	STODParameter();

	void          SetName(const string& name)           { m_ParamName = name; }
	const char*   GetName() const                       { return m_ParamName.c_str(); }

	void          SetLabel(string labelName)            { m_LabelName = labelName; }
	const char*   GetLabel() const                      { return m_LabelName.c_str(); }

	void          SetGroupName(const string& groupName) { m_GroupName = groupName; }
	const char*   GetGroupName() const                  { return m_GroupName.c_str(); }

	void          SetTODParamType(ETODParamType type)   { m_Type = type; }
	ETODParamType GetTODParamType() const               { return m_Type; }

	void          SetParamID(int id)                    { m_ID = id; }
	int           GetParamID() const                    { return m_ID; }

	void          SetValue(const Vec3& paramValue)      { m_value = paramValue; }
	Vec3          GetValue() const                      { return m_value; }

private:
	Vec3          m_value;
	int           m_ID;

	ETODParamType m_Type;

	string        m_ParamName;
	string        m_LabelName;
	string        m_GroupName;

public:
	STODParameterGroup* m_pGroup;
	int                 m_IDWithinGroup;
};

bool Serialize(Serialization::IArchive& ar, STODParameter& param, const char* name, const char* label);

struct STODParameterGroup
{
	void Serialize(Serialization::IArchive& ar);

	int                        m_id;
	std::string                m_name;
	std::vector<STODParameter> m_Params;
};

struct STODParameterGroupSet
{
	typedef std::vector<STODParameterGroup> TPropertyGroupMap;
	TPropertyGroupMap           m_propertyGroups;

	std::vector<STODParameter*> m_params; // paramID to pointer

	void                        Serialize(Serialization::IArchive& ar);
};

class QPropertyTree;
class QViewport;
class QTimeEdit;
class QSlider;
class QCheckBox;
class CCurveEditor;
//class QDoubleSpinBox;
class QLineEdit;
class CTimeEditControl;

class PropertyRow;
class QTimeOfDayWidget : public QWidget, public IEditorNotifyListener
{
	Q_OBJECT

	friend struct STODParameter;

public:
	QTimeOfDayWidget();
	~QTimeOfDayWidget();

	void CheckParameterChanged(STODParameter& param, const Vec3& newValue);

	void Refresh();
	void OnIdleUpdate();

	void UpdateCurveContent();

signals:
	void SignalContentChanged();

private slots:
	void CurveEditTimeChanged();
	void CurrentTimeEdited(); // called when user enters new value in m_currentTimeEdit

	void OnForceSkyUpdateChk();
	void OnPropertySelected();
	void UndoBegin();
	void UndoEnd();
	void OnSplineEditing();

	void OnCopyCurveContent();
	void OnPasteCurveContent();
protected:
	class CParamSelectionChangedUndoCommand;
	class CContentChangedUndoCommand;

	void         SetTODTime(const float fTime);

	void         CreateUi();
	void         LoadPropertiesTree();

	void         UpdateValues();
	void         UpdateSelectedParamId();
	void         UpdateProperties();
	void         UpdateCurrentTimeEdit();
	void         UpdateCurveTime();

	bool         eventFilter(QObject* obj, QEvent* event);

	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) override;

protected:
	bool                                         m_bIsPlaying;
	bool                                         m_bIsEditing;
	STODParameterGroupSet                        m_groups;
	std::unique_ptr<Serialization::CContextList> m_pContextList;
	QPropertyTree*                               m_propertyTree;
	QCheckBox*                                   m_forceSkyUpdateBtn;
	CTimeEditControl*                            m_currentTimeEdit;
	CTimeEditControl*                            m_startTimeEdit;
	CTimeEditControl*                            m_endTimeEdit;
	QLineEdit*                                   m_playSpeedEdit;
	CCurveEditor*                                m_curveEdit;
	std::unique_ptr<SCurveEditorContent>         m_curveContent;

	int                         m_selectedParamId;
	CContentChangedUndoCommand* m_pUndoCommand;

	float                       m_fAnimTimeSecondsIn24h;
};

