// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SubstanceOutputNodeBase.h"
#include "SubstanceCommon.h"

namespace EditorSubstance
{
	namespace OutputEditor
	{

		class CSubstanceNodeContentWidget;
		class CSubstanceOutPinItem;
		class COriginalOutputNode;

		struct ConnectionInfo
		{
			CSubstanceOutPinItem* pin;
			COriginalOutputNode* node;
		};

		class CVirtualOutputNode : public CSubstanceOutputNodeBase
		{
		public:
			CVirtualOutputNode(const SSubstanceOutput& output, CryGraphEditor::CNodeGraphViewModel& viewModel);
			virtual bool CanDelete() const override { return true; }

			virtual const char* GetStyleId() const override { return "Node::Input"; }

			SSubstanceOutput* GetOutput() { return &m_pOutput; }

			virtual void Serialize(Serialization::IArchive& ar);

			virtual void UpdatePinState();
			virtual void PropagateNetworkToOutput();

			virtual ESubstanceGraphNodeType GetNodeType() const override { return eInput; }
		protected:
			virtual ConnectionInfo GetPinConnectionInfo(int index);
			virtual void HandleIndividualConnection(int pinIndex);

		};


	}
}
