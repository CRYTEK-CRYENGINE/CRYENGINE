// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************

   Sets a new GameObject as the active Responder for this response
   All following actions are then executed on/by the new responder-object

   /************************************************************************/

#pragma once

#include <CryDynamicResponseSystem/IDynamicResponseAction.h>
#include <memory>
#include "Variable.h"

namespace CryDRS
{
class CVariableCollection;

class CActionSetActor final : public DRS::IResponseAction
{
public:
	CActionSetActor() : m_newResponderName() {}
	CActionSetActor(const CHashedString& responderName, CVariableCollection* pUsedCollection) : m_newResponderName(responderName) {}

	//////////////////////////////////////////////////////////
	// IResponseAction implementation
	virtual DRS::IResponseActionInstanceUniquePtr Execute(DRS::IResponseInstance* pResponseInstance) override;
	virtual string                                GetVerboseInfo() const override { return m_newResponderName.GetText() + "'"; }
	virtual void                                  Serialize(Serialization::IArchive& ar) override;
	virtual const char*                           GetType() const override        { return "Set Actor"; }
	//////////////////////////////////////////////////////////

private:
	CHashedString m_newResponderName;
};

class CActionSetActorByVariable final : public DRS::IResponseAction, public IVariableUsingBase
{
public:
	//////////////////////////////////////////////////////////
	// IResponseAction implementation
	virtual DRS::IResponseActionInstanceUniquePtr Execute(DRS::IResponseInstance* pResponseInstance) override;
	virtual string                                GetVerboseInfo() const override { return "Variable to receive the actor name from: " + GetVariableVerboseName(); }
	virtual void                                  Serialize(Serialization::IArchive& ar) override;
	virtual const char*                           GetType() const override        { return "Set Actor by variable"; }
	//////////////////////////////////////////////////////////
};
} //namespace CryDRS
