// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Script/GraphNodes/ScriptGraphNodeBase.h"

#include <CrySchematyc2/Runtime/IRuntime.h>

namespace Schematyc2
{
	class CScriptGraphNodeFactory;

	class CScriptGraphCommentNode : public CScriptGraphNodeBase
	{
	public:
		static const SGUID s_typeGUID;

	public:
		CScriptGraphCommentNode(); // #SchematycTODO : Do we really need a default constructor?
		CScriptGraphCommentNode(const SGUID& guid);
		CScriptGraphCommentNode(const SGUID& guid, const Vec2& pos);

		// IScriptGraphNode
		virtual const char* GetComment() const override;
		virtual SGUID GetTypeGUID() const override;
		virtual EScriptGraphColor GetColor() const override;
		virtual void Refresh(const SScriptRefreshParams& params) override;
		virtual void Serialize(Serialization::IArchive& archive) override;
		virtual void Compile_New(IScriptGraphNodeCompiler& compiler) const override;
		// ~IScriptGraphNode

		static void RegisterCreator(CScriptGraphNodeFactory& factory);

	private:
		string m_str;  // contents of the comment
	};
}
