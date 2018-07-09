// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Script/GraphNodes/ScriptGraphNodeBase.h"

#include <CrySchematyc2/Runtime/IRuntime.h>

namespace Schematyc2
{
	class CScriptGraphNodeFactory;

	class CScriptGraphBeginNode : public CScriptGraphNodeBase
	{
	private:

		struct EOutputIdx
		{
			enum : uint32
			{
				Out = 0
			};
		}; 

	public:

		CScriptGraphBeginNode(); // #SchematycTODO : Do we really need a default constructor?
		CScriptGraphBeginNode(const SGUID& guid);
		CScriptGraphBeginNode(const SGUID& guid, const Vec2& pos);

		// IScriptGraphNode
		virtual SGUID GetTypeGUID() const override;
		virtual EScriptGraphColor GetColor() const override;
		virtual void Refresh(const SScriptRefreshParams& params) override;
		virtual void Serialize(Serialization::IArchive& archive) override;
		virtual void RemapGUIDs(IGUIDRemapper& guidRemapper) override;
		virtual void Compile_New(IScriptGraphNodeCompiler& compiler) const override;
		// ~IScriptGraphNode

		static void RegisterCreator(CScriptGraphNodeFactory& factory);

	private:

		static SRuntimeResult Execute(IObject* pObject, const SRuntimeActivationParams& activationParams, CRuntimeNodeData& data);

	public:

		static const SGUID s_typeGUID;
	};
}
