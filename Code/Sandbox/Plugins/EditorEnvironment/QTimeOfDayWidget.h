// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include <IEditor.h>
#include "QPropertyTree/ContextList.h"

struct SCurveEditorContent;
struct STODParameterGroup;

enum ETODParamType
{
	eFloatType,
	eColorType
};

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

class CCurveEditor;
class QLineEdit;
class QPropertyTree;
class QSplitter;
class CTimeEditControl;

class QTimeOfDayWidget : public QWidget, public IEditorNotifyListener
{
	Q_OBJECT

	friend struct STODParameter;

public:
	QTimeOfDayWidget();
	~QTimeOfDayWidget();

	void        CheckParameterChanged(STODParameter& param, const Vec3& newValue);

	void        SetPersonalizationState(const QVariantMap& state);
	QVariantMap GetPersonalizationState() const;

	void        Refresh();
	void        OnIdleUpdate();

	void        UpdateCurveContent();

signals:
	void SignalContentChanged();

private slots:
	void CurveEditTimeChanged();
	void OnPropertySelected();
	void OnCopyCurveContent();
	void OnPasteCurveContent();

private:
	class CContentChangedUndoCommand;
	class CUndoConstPropTreeCommand;

	void UndoBegin();
	void UndoEnd();
	void CurrentTimeEdited(); // called when user enters new value in m_currentTimeEdit
	void OnSplineEditing();
	void OnEditorNotifyEvent(EEditorNotifyEvent event) override;
	void SetTODTime(const float fTime);

	void UndoConstantProperties();

	void CreateUi();
	void CreatePropertyTrees(QSplitter* pParent);
	void CreateCurveEditor(QSplitter* pParent);
	void LoadPropertiesTrees();

	void UpdateValues();
	void UpdateSelectedParamId();
	void UpdateVarPropTree();
	void UpdateCurrentTimeEdit();
	void UpdateCurveTime();
	void UpdateConstPropTree();

	bool eventFilter(QObject* obj, QEvent* event);

	bool                                         m_bIsPlaying;
	bool                                         m_bIsEditing;
	STODParameterGroupSet                        m_groups;
	std::unique_ptr<Serialization::CContextList> m_pContextList;
	QPropertyTree*                               m_propertyTreeVar;

	CTimeEditControl*                            m_currentTimeEdit;
	CTimeEditControl*                            m_startTimeEdit;
	CTimeEditControl*                            m_endTimeEdit;
	QLineEdit*                                   m_playSpeedEdit;
	CCurveEditor*                                m_curveEdit;
	std::unique_ptr<SCurveEditorContent>         m_curveContent;

	int                         m_selectedParamId;
	CContentChangedUndoCommand* m_pUndoCommand;

	float                       m_fAnimTimeSecondsIn24h;

	QSplitter*                  m_splitterBetweenTrees;
	QPropertyTree*              m_propertyTreeConst;
};
