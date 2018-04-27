// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Runtime/IRuntime.h>

#include "Script/GraphNodes/ScriptGraphNodeBase.h"

namespace Schematyc2
{
	class CScriptGraphNodeFactory;

	class CScriptGraphGetNode : public CScriptGraphNodeBase
	{
	private:

		struct EOutputIdx
		{
			enum : uint32
			{
				Value = 0
			};
		};

		struct EAttributeId
		{
			enum : uint32
			{
				Params
			};
		};

	public:

		CScriptGraphGetNode(const SGUID& guid);
		CScriptGraphGetNode(const SGUID& guid, const Vec2& pos, const SGUID& refGUID);

		// IScriptGraphNode
		virtual SGUID GetTypeGUID() const override;
		virtual SGUID GetRefGUID() const override;
		virtual EScriptGraphColor GetColor() const override;
		virtual void Refresh(const SScriptRefreshParams& params) override;
		virtual void Serialize(Serialization::IArchive& archive) override;
		virtual void RemapGUIDs(IGUIDRemapper& guidRemapper) override;
		virtual void Compile_New(IScriptGraphNodeCompiler& compiler) const override;
		// ~IScriptGraphNode

		static void RegisterCreator(CScriptGraphNodeFactory& factory);

	private:

		void SerializeBasicInfo(Serialization::IArchive& archive);
		void Validate(Serialization::IArchive& archive);

		static SRuntimeResult Execute(IObject* pObject, const SRuntimeActivationParams& activationParams, CRuntimeNodeData& data);

	public:

		static const SGUID s_typeGUID;

	private:

		SGUID m_refGUID;
	};
}
