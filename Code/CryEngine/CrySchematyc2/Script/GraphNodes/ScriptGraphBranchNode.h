// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Script/GraphNodes/ScriptGraphNodeBase.h"

#include <CrySchematyc2/Runtime/IRuntime.h>

namespace Schematyc2
{
	class CScriptGraphNodeFactory;

	class CScriptGraphBranchNode : public CScriptGraphNodeBase
	{
	private:

		struct EInputIdx
		{
			enum : uint32
			{
				In = 0,
				Value
			};
		};

		struct EOutputIdx
		{
			enum : uint32
			{
				True = 0,
				False
			};
		}; 

	public:

		CScriptGraphBranchNode(); // #SchematycTODO : Do we really need a default constructor?
		CScriptGraphBranchNode(const SGUID& guid);
		CScriptGraphBranchNode(const SGUID& guid, const Vec2& pos);

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

		bool m_bValue;


		static const SGUID s_typeGUID;
	};
}
