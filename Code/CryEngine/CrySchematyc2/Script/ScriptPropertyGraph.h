// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ScriptGraphBase.h"

namespace Schematyc2
{
	class CScriptPropertyGraph : public CScriptGraphBase
	{
	public:

		CScriptPropertyGraph(IScriptElement& element);

		// IScriptExtension
		virtual void Refresh_New(const SScriptRefreshParams& params) override;
		virtual void Serialize_New(Serialization::IArchive& archive) override;
		// ~IScriptExtension

		// IScriptGraph
		virtual EScriptGraphType GetType() const override;
		// ~IScriptGraph

	private:

		bool BeginNodeExists() const;
	};
}
