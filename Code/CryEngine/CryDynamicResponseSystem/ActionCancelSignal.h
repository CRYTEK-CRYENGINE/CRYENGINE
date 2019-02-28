// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************

   the cancel signal processing action class

   /************************************************************************/

#pragma once

#include <CryDynamicResponseSystem/IDynamicResponseAction.h>
#include "ResponseManager.h"

namespace CryDRS
{
	class CResponseActor;

class CActionCancelSignal final : public DRS::IResponseAction
{
public:
	CActionCancelSignal() : m_bOnAllActors(true) {}
	CActionCancelSignal(const CHashedString& signalName) : m_signalName(signalName), m_bOnAllActors(true) {}

	//////////////////////////////////////////////////////////
	// IResponseAction implementation
	virtual DRS::IResponseActionInstanceUniquePtr Execute(DRS::IResponseInstance* pResponseInstance) override;
	virtual string                                GetVerboseInfo() const override { return m_signalName.GetText() + "'"; }
	virtual void                                  Serialize(Serialization::IArchive& ar) override;
	virtual const char*                           GetType() const override        { return "Cancel Signal"; }
	//////////////////////////////////////////////////////////

private:
	CHashedString m_signalName;
	bool          m_bOnAllActors;
};

class CActionCancelSignalInstance final : public DRS::IResponseActionInstance
{
public:
	CActionCancelSignalInstance(const CHashedString& signalName, CResponseActor* pSender = nullptr, DRS::SignalInstanceId instanceToSkip = DRS::s_InvalidSignalId);

	//////////////////////////////////////////////////////////
	// IResponseActionInstance implementation
	virtual eCurrentState Update() override;
	virtual void          Cancel() override;
	//////////////////////////////////////////////////////////

private:
	SSignal m_signal;
};
}
