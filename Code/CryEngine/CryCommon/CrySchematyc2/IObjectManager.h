// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Think about how we can filter signals for broadcast rather then using the brute force approach.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include <CrySchematyc2/TemplateUtils/TemplateUtils_Delegate.h>

#include "CrySchematyc2/ILib.h"

namespace Schematyc2
{
	struct IObject;
	struct SObjectParams;

	typedef TemplateUtils::CDelegate<EVisitStatus (const IObject&)> IObjectVisitor;
	typedef TemplateUtils::CDelegate<EVisitStatus (const IObject&)> IObjectConstVisitor;

	struct IObjectManager
	{
		virtual ~IObjectManager() {}

		virtual IObject* CreateObject(const SObjectParams& params) = 0;
		virtual void DestroyObject(IObject* pObject) = 0;
		virtual IObject* GetObjectById(const ObjectId& objectId) = 0;
		//virtual IObject* GetObjectByEntityId(const ExplicitEntityId& entityId) = 0;
		virtual void VisitObjects(const IObjectVisitor& visitor) = 0;
		virtual void VisitObjects(const IObjectConstVisitor& visitor) const = 0;
		virtual void SendSignal(const ObjectId& objectId, const SGUID& signalGUID, const TVariantConstArray& inputs = TVariantConstArray()) = 0;
		virtual void SendSignal(const ExplicitEntityId& entityId, const SGUID& signalGUID, const TVariantConstArray& inputs = TVariantConstArray()) = 0;
		virtual void BroadcastSignal(const SGUID& signalGUID, const TVariantConstArray& inputs = TVariantConstArray()) = 0;
	};
}
