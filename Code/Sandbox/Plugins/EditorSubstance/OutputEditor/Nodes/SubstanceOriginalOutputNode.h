// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SubstanceOutputNodeBase.h"

namespace EditorSubstance
{
	namespace OutputEditor
	{


		class COriginalOutputNode : public CSubstanceOutputNodeBase
		{
		public:
			COriginalOutputNode(const SSubstanceOutput& output, CryGraphEditor::CNodeGraphViewModel& viewModel);
			virtual bool CanDelete() const override { return true; }

			virtual const char* GetStyleId() const override
			{
				return "Node::Output";
			}

			virtual ESubstanceGraphNodeType GetNodeType() const override { return eOutput; }
		};

	}
}
