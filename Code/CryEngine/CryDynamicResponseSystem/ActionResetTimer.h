// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************

   this action resets a counter to the current time, so that it can be used in TimeSince Conditions later on

   /************************************************************************/

#pragma once

#include <CryDynamicResponseSystem/IDynamicResponseAction.h>
#include "Variable.h"

namespace CryDRS
{
class CVariableCollection;

class CActionResetTimerVariable final : public IVariableUsingBase, public DRS::IResponseAction
{
public:
	CActionResetTimerVariable() = default;
	CActionResetTimerVariable(const CHashedString& collection, const CHashedString& variableName) {}

	//////////////////////////////////////////////////////////
	// IResponseAction implementation
	virtual DRS::IResponseActionInstanceUniquePtr Execute(DRS::IResponseInstance* pResponseInstance) override;
	virtual string                                GetVerboseInfo() const override;
	virtual void                                  Serialize(Serialization::IArchive& ar) override;
	virtual const char*                           GetType() const override { return "ResetTimer"; }
	//////////////////////////////////////////////////////////

private:
};
} // namespace CryDRS
