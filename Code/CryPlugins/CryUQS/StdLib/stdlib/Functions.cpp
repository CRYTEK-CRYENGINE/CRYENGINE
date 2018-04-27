// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryEntitySystem/IEntitySystem.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace StdLib
	{

		void CStdLibRegistration::InstantiateFunctionFactoriesForRegistration()
		{
			{
				Client::CFunctionFactory<CFunction_Pos3AddOfs3>::SCtorParams ctorParams;

				ctorParams.szName = "std::Pos3AddOfs3";
				ctorParams.guid = "20f46e25-1522-46a0-959d-4006241792f8"_cry_guid;
				ctorParams.szDescription = "Adds an Ofs3 to a Pos3 and returns the new Pos3.";

				static const Client::CFunctionFactory<CFunction_Pos3AddOfs3> functionFactory_Pos3AddOfs3(ctorParams);
			}

			{
				Client::CFunctionFactory<CFunction_PosFromEntity>::SCtorParams ctorParams;

				ctorParams.szName = "std::PosFromEntity";
				ctorParams.guid = "c76ca7ad-02cf-440e-87ca-6e27097b9737"_cry_guid;
				ctorParams.szDescription = "Returns the position of a given entity.\nCauses an exception if no such entity exists.";

				static const Client::CFunctionFactory<CFunction_PosFromEntity> functionFactory_PosFromEntity(ctorParams);
			}
		}

		//===================================================================================
		//
		// CFunction_Pos3AddOfs3
		//
		//===================================================================================

		CFunction_Pos3AddOfs3::CFunction_Pos3AddOfs3(const SCtorContext& ctorContext)
			: CFunctionBase(ctorContext)
		{
			// nothing
		}

		Pos3 CFunction_Pos3AddOfs3::DoExecute(const SExecuteContext& executeContext, const SParams& params) const
		{
			return Pos3(params.pos.value + params.ofs.value);
		}

		//===================================================================================
		//
		// CFunction_PosFromEntity
		//
		//===================================================================================

		CFunction_PosFromEntity::CFunction_PosFromEntity(const SCtorContext& ctorContext)
			: CFunctionBase(ctorContext)
		{
			// nothing
		}

		Pos3 CFunction_PosFromEntity::DoExecute(const SExecuteContext& executeContext, const SParams& params) const
		{
			if (const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(params.entityId.value))
			{
				return Pos3(pEntity->GetPos());
			}
			else
			{
				executeContext.error.Format("could not find entity with entityId = %i", static_cast<int>(params.entityId.value));
				executeContext.bExceptionOccurred = true;
				return Pos3(Vec3Constants<float>::fVec3_Zero);
			}
		}

	}
}
