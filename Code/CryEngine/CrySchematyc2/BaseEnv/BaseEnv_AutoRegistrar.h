// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseEnv/BaseEnv_Prerequisites.h"

#include <CryCore/StaticInstanceList.h>

#define SCHEMATYC2_GAME_ENV_AUTO_REGISTER(function) static SchematycBaseEnv::CAutoRegistrar PP_JOIN_XY(schematycGameEnvAutoRegistrar, __COUNTER__)(function);

namespace SchematycBaseEnv
{
	typedef void (*AutoRegistrarFunctionPtr)(); 

	class CAutoRegistrar : public CStaticInstanceList<CAutoRegistrar>
	{
	public:

		CAutoRegistrar(AutoRegistrarFunctionPtr pFunction);

		static void Process();

	private:

		AutoRegistrarFunctionPtr m_pFunction;
	};
}