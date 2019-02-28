// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************

   This action will execute the specified audio trigger.

   /************************************************************************/

#pragma once

#include <CryDynamicResponseSystem/IDynamicResponseAction.h>

class CActionExecuteAudioTrigger final : public DRS::IResponseAction
{
public:
	CActionExecuteAudioTrigger() : m_bWaitToBeFinished(true) {}

	//////////////////////////////////////////////////////////
	// IResponseAction implementation
	virtual DRS::IResponseActionInstanceUniquePtr Execute(DRS::IResponseInstance* pResponseInstance) override;
	virtual string                                GetVerboseInfo() const override;
	virtual void                                  Serialize(Serialization::IArchive& ar) override;
	virtual const char*                           GetType() const override { return "Execute Audio Trigger"; }
	//////////////////////////////////////////////////////////

private:
	string m_AudioTriggerName;
	bool   m_bWaitToBeFinished;
};

//////////////////////////////////////////////////////////////////////////

class CActionExecuteAudioTriggerInstance final : public DRS::IResponseActionInstance
{
public:
	CActionExecuteAudioTriggerInstance(DRS::IResponseActor* pActor, CryAudio::ControlId audioStartTriggerID);
	virtual ~CActionExecuteAudioTriggerInstance() override;

	//////////////////////////////////////////////////////////
	// IResponseActionInstance implementation
	virtual eCurrentState Update() override;
	virtual void          Cancel() override;
	//////////////////////////////////////////////////////////

	void        SetFinished();
	static void OnAudioTriggerFinished(const CryAudio::SRequestInfo* const pAudioRequestInfo);
private:
	CryAudio::ControlId  m_audioStartTriggerID;
	DRS::IResponseActor* m_pActor;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CActionSetAudioSwitch final : public DRS::IResponseAction
{
public:
	//////////////////////////////////////////////////////////
	// IResponseAction implementation
	virtual DRS::IResponseActionInstanceUniquePtr Execute(DRS::IResponseInstance* pResponseInstance) override;
	virtual string                                GetVerboseInfo() const override;
	virtual void                                  Serialize(Serialization::IArchive& ar) override;
	virtual const char*                           GetType() const override { return "Set Audio Switch"; }
	//////////////////////////////////////////////////////////

private:
	string m_switchName;
	string m_stateName;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CActionSetAudioParameter final : public DRS::IResponseAction
{
public:
	//////////////////////////////////////////////////////////
	// IResponseAction implementation
	virtual DRS::IResponseActionInstanceUniquePtr Execute(DRS::IResponseInstance* pResponseInstance) override;
	virtual string                                GetVerboseInfo() const override;
	virtual void                                  Serialize(Serialization::IArchive& ar) override;
	virtual const char*                           GetType() const override { return "Set Audio Parameter"; }
	//////////////////////////////////////////////////////////

private:
	string m_audioParameter;
	float m_valueToSet = 1.0f;
};
