// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************

   The dynamic Response instance is responsible to execute one response with all the matches segments. many of these can run in parallel. responsible for updating the running action-instances

   /************************************************************************/

#pragma once

#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
#include "ResponseSystem.h"
#include "ResponseSegment.h"

namespace CryDRS
{
class CVariableCollection;
typedef std::shared_ptr<CVariableCollection> VariableCollectionSharedPtr;

//this class handles the execution of a ongoing Response
class CResponseInstance final : public DRS::IResponseInstance
{
public:
	CResponseInstance(SSignal& signal, CResponse* pResponse);
	virtual ~CResponseInstance() override;

	void Execute();
	bool Update();
	void Cancel();

	//////////////////////////////////////////////////////////
	// IResponseInstance implementation
	virtual CResponseActor*                   GetCurrentActor() const override                             { return m_pActiveActor; }
	virtual void                              SetCurrentActor(DRS::IResponseActor* pNewResponder) override { m_pActiveActor = static_cast<CResponseActor*>(pNewResponder); }
	virtual CResponseActor* const             GetOriginalSender() const override                           { return m_pSender; }
	virtual const CHashedString&              GetSignalName() const override                               { return m_signalName; }
	virtual const DRS::SignalInstanceId       GetSignalInstanceId() const override                         { return m_id; }
	virtual DRS::IVariableCollectionSharedPtr GetContextVariables() const override                         { return std::static_pointer_cast<DRS::IVariableCollection>(m_pSignalContext); }
	//////////////////////////////////////////////////////////

	void                        SetCurrentActor(CResponseActor* pNewResponder) { m_pActiveActor = pNewResponder; }
	CResponse*                  GetResponse()                                  { return m_pResponse; }
	VariableCollectionSharedPtr GetContextVariablesImpl() const                { return m_pSignalContext; }
	CResponseSegment*           GetCurrentSegment()                            { return m_pCurrentlyExecutedSegment; }

private:
	void ExecuteSegment(CResponseSegment* pSegment);

	typedef std::vector<DRS::IResponseActionInstanceUniquePtr> ActionInstanceList;

	CResponseActor* const       m_pSender;
	CResponseActor*             m_pActiveActor;
	VariableCollectionSharedPtr m_pSignalContext;
	CResponseSegment*           m_pCurrentlyExecutedSegment;
	ActionInstanceList          m_activeActions;
	const CHashedString         m_signalName;
	const DRS::SignalInstanceId m_id;
	CResponse*                  m_pResponse;
};
}  //namespace CryDRS
