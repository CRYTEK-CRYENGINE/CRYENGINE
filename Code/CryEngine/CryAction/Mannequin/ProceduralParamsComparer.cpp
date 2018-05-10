// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryExtension/ClassWeaver.h>
#include <ICryMannequin.h>
#include <Mannequin/Serialization.h>
#include <CrySerialization/IArchiveHost.h>

class CProceduralParamsComparerDefault
	: public IProceduralParamsComparer
{
public:
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IProceduralParamsComparer)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CProceduralParamsComparerDefault, "ProceduralParamsComparerDefault", "fc53bd92-4853-4faa-ab0f-b42b24e55b3e"_cry_guid)

	virtual ~CProceduralParamsComparerDefault() {}

	virtual bool Equal(const IProceduralParams& lhs, const IProceduralParams& rhs) const override
	{
		return Serialization::CompareBinary(lhs, rhs);
	}
};

CRYREGISTER_CLASS(CProceduralParamsComparerDefault);
