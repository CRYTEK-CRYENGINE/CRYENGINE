// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Separate into multiple headers?
// #SchematycTODO : Do all of these types really need to live in public interface?

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/Any.h"
#include "CrySchematyc2/BasicTypes.h"
#include "CrySchematyc2/Runtime/RuntimeParams.h"
#include "CrySchematyc2/Runtime/ScratchPad.h"

namespace Schematyc2
{
	class  CRuntimeNodeData;
	class  CRuntimeParams;
	struct IObject;
	
	enum class ERuntimeStatus
	{
		Repeat,
		Continue,
		End,
		Break,
		Return
	};

	struct SRuntimeResult
	{
		inline SRuntimeResult(ERuntimeStatus _status = ERuntimeStatus::End, uint32 _outputIdx = s_invalidIdx)
			: status(_status)
			, outputIdx(_outputIdx)
		{}

		ERuntimeStatus status;
		uint32         outputIdx;
	};

	enum class ERuntimeActivationFlags
	{
		None   = 0,
		Input  = BIT(0),
		Output = BIT(1),
		Repeat = BIT(2)
	};

	DECLARE_ENUM_CLASS_FLAGS(ERuntimeActivationFlags)

	struct SRuntimeActivationParams
	{
		inline SRuntimeActivationParams(ERuntimeActivationFlags _flags = ERuntimeActivationFlags::None, uint32 _portIdx = s_invalidIdx)
			: flags(_flags)
			, portIdx(_portIdx)
		{}

		ERuntimeActivationFlags flags;
		uint32                  portIdx;
	};

	typedef SRuntimeResult (*RuntimeNodeCallbackPtr)(IObject* pObject, const SRuntimeActivationParams& activationParams, CRuntimeNodeData& data);

	struct SRuntimeNodeAttribute
	{
		inline SRuntimeNodeAttribute(uint32 _id = s_invalidId, uint32 _pos = s_invalidIdx)
			: id(_id)
			, pos(_pos)
		{}

		uint32 id;
		uint32 pos;
	};

	class CRuntimeNode
	{
	public:

		inline CRuntimeNode(const SGUID& guid, RuntimeNodeCallbackPtr pCallback, uint32 attributeCount, uint32 inputCount, uint32 outputCount)
			: m_guid(guid)
			, m_pCallback(pCallback)
			, m_attributeCount(attributeCount)
			, m_inputCount(inputCount)
			, m_outputCount(outputCount)
		{}

		inline SGUID GetGUID() const
		{
			return m_guid;
		}

		inline RuntimeNodeCallbackPtr GetCallback() const
		{
			return m_pCallback;
		}

		inline uint32 GetAttributeCount() const
		{
			return m_attributeCount;
		}

		inline SRuntimeNodeAttribute* GetAttributes()
		{
			return reinterpret_cast<SRuntimeNodeAttribute*>(this + 1);
		}

		inline const SRuntimeNodeAttribute* GetAttributes() const
		{
			return reinterpret_cast<const SRuntimeNodeAttribute*>(this + 1);
		}

		inline uint32 GetInputCount() const
		{
			return m_inputCount;
		}

		inline uint32* GetInputs()
		{
			return reinterpret_cast<uint32*>(GetAttributes() + m_attributeCount);
		}

		inline const uint32* GetInputs() const
		{
			return reinterpret_cast<const uint32*>(GetAttributes() + m_attributeCount);
		}

		inline uint32 GetOutputCount() const
		{
			return m_outputCount;
		}

		inline uint32* GetOutputs()
		{
			return reinterpret_cast<uint32*>(GetInputs() + m_inputCount);
		}

		inline const uint32* GetOutputs() const
		{
			return reinterpret_cast<const uint32*>(GetInputs() + m_inputCount);
		}

	private:

		SGUID                  m_guid;
		RuntimeNodeCallbackPtr m_pCallback;
		uint32                 m_attributeCount;
		uint32                 m_inputCount;
		uint32                 m_outputCount;
	};

	class CRuntimeSequenceOp
	{
	public:

		inline CRuntimeSequenceOp(uint32 nodePos, const SRuntimeActivationParams& activationParams, uint32 outputCount)
			: m_nodePos(nodePos)
			, m_activationParams(activationParams)
			, m_outputCount(outputCount)
		{}

		inline uint32 GetNodePos() const
		{
			return m_nodePos;
		}

		inline const SRuntimeActivationParams& GetActivationParams() const
		{
			return m_activationParams;
		}

		inline uint32 GetOutputCount() const // N.B. Only used during validation!
		{
			return m_outputCount;
		}

		inline uint32* GetOutputs()
		{
			return reinterpret_cast<uint32*>(this + 1);
		}

		inline const uint32* GetOutputs() const
		{
			return reinterpret_cast<const uint32*>(this + 1);
		}

		inline const CRuntimeSequenceOp* GetNext() const // N.B. Only used during validation!
		{
			return reinterpret_cast<const CRuntimeSequenceOp*>(reinterpret_cast<const uint32*>(this + 1) + m_outputCount);
		}

	private:

		uint32                   m_nodePos;
		SRuntimeActivationParams m_activationParams;
		uint32                   m_outputCount;
	};

	struct SRuntimeFunctionParam
	{
		inline SRuntimeFunctionParam(uint32 _id = s_invalidId, const EnvTypeId& _envTypeId = EnvTypeId())
			: id(_id)
			, envTypeId(_envTypeId)
		{}

		uint32    id;
		EnvTypeId envTypeId;
	};

	//typedef std::vector<SRuntimeFunctionParam> RuntimeFunctionParams;

	class CRuntimeFunction
	{
	public:

		inline CRuntimeFunction(const SGUID& guid)
			: m_guid(guid)
		{}

		inline SGUID GetGUID() const
		{
			return m_guid;
		}

		inline uint32 AddNode(const SGUID& guid, RuntimeNodeCallbackPtr pCallback, uint32 attributeCount, uint32 inputCount, uint32 outputCount)
		{
			const uint32 pos = m_nodes.size();
			const uint32 size = sizeof(CRuntimeNode) + (sizeof(SRuntimeNodeAttribute) * attributeCount) + (sizeof(uint32) * (inputCount + outputCount));
			m_nodes.resize(pos + size, 0);
			new (&m_nodes[pos]) CRuntimeNode(guid, pCallback, attributeCount, inputCount, outputCount);
			return pos;
		}

		inline CRuntimeNode* GetNode(uint32 pos)
		{
			return reinterpret_cast<CRuntimeNode*>(&m_nodes[pos]);
		}

		inline const CRuntimeNode* GetNode(uint32 pos) const
		{
			return reinterpret_cast<const CRuntimeNode*>(&m_nodes[pos]);
		}

		inline uint32 AddSequenceOp(uint32 nodeIdx, const SRuntimeActivationParams& activationParams, uint32 outputCount)
		{
			const uint32 pos = m_sequenceOps.size();
			const uint32 size = sizeof(CRuntimeSequenceOp) + (sizeof(uint32) * outputCount);
			m_sequenceOps.resize(pos + size, 0);
			new (&m_sequenceOps[pos]) CRuntimeSequenceOp(nodeIdx, activationParams, outputCount);
			return pos;
		}

		inline CRuntimeSequenceOp* GetSequenceOp(uint32 pos)
		{
			return reinterpret_cast<CRuntimeSequenceOp*>(&m_sequenceOps[pos]);
		}

		inline const CRuntimeSequenceOp* GetSequenceOp(uint32 pos) const
		{
			return reinterpret_cast<const CRuntimeSequenceOp*>(&m_sequenceOps[pos]);
		}

		inline CScratchPad& GetScratchPad()
		{
			return m_scratchPad;
		}

		inline const CScratchPad& GetScratchPad() const
		{
			return m_scratchPad;
		}

		inline bool Empty() const
		{
			return m_sequenceOps.empty();
		}

	private:

		SGUID                 m_guid;
		//RuntimeFunctionParams m_inputs;
		//RuntimeFunctionParams m_outputs;
		std::vector<uint8>    m_nodes; // What's a good size to reserve?
		std::vector<uint8>    m_sequenceOps; // Can we allocate nodes and sequence ops in a single block?
		CScratchPad           m_scratchPad;
	};

	class CRuntimeNodeData
	{
	public:

		inline CRuntimeNodeData(const CRuntimeFunction& function, const CRuntimeNode& node, const CRuntimeParams& inputs, CRuntimeParams& outputs, CScratchPad& scratchPad)
			: m_function(function)
			, m_node(node)
			, m_inputs(inputs)
			, m_outputs(outputs)
			, m_scratchPad(scratchPad)
		{}

		inline IAny* GetAttribute(uint32 attributeId)
		{
			for(const SRuntimeNodeAttribute* pAttribute = m_node.GetAttributes(), *pEndAttribute = pAttribute + m_node.GetAttributeCount(); pAttribute != pEndAttribute; ++ pAttribute)
			{
				if(pAttribute->id == attributeId)
				{
					return m_scratchPad[pAttribute->pos];
				}
			}
			return nullptr;
		}

		inline IAny* GetAttribute(uint32 attributeId, const EnvTypeId& typeId)
		{
			IAny* pAttribute = GetAttribute(attributeId);
			return pAttribute && (pAttribute->GetTypeInfo().GetTypeId().AsEnvTypeId() == typeId) ? pAttribute : nullptr;
		}

		template <typename TYPE> inline TYPE* GetAttribute(uint32 attributeId)
		{
			IAny* pAttribute = GetAttribute(attributeId, GetEnvTypeId<TYPE>());
			return pAttribute ? static_cast<TYPE*>(pAttribute->ToVoidPtr()) : nullptr;
		}

		inline uint32 GetInputCount() const
		{
			return m_node.GetInputCount();
		}

		inline const IAny* GetInput(uint32 inputIdx) const
		{
			SCHEMATYC2_SYSTEM_ASSERT(inputIdx < m_node.GetInputCount());
			if(inputIdx < m_node.GetInputCount())
			{
				const uint32 inputPos = m_node.GetInputs()[inputIdx];
				SCHEMATYC2_SYSTEM_ASSERT(inputPos != s_invalidIdx);
				if(inputPos != s_invalidIdx)
				{
					return m_scratchPad[inputPos];
				}
			}
			return nullptr;
		}

		inline const IAny* GetInput(uint32 inputIdx, const EnvTypeId& typeId) const
		{
			const IAny* pInput = GetInput(inputIdx);
			return pInput && (pInput->GetTypeInfo().GetTypeId().AsEnvTypeId() == typeId) ? pInput : nullptr;
		}

		template <typename TYPE> inline const TYPE* GetInput(uint32 inputIdx) const
		{
			const IAny* pInput = GetInput(inputIdx, GetEnvTypeId<TYPE>());
			return pInput ? static_cast<const TYPE*>(pInput->ToVoidPtr()) : nullptr;
		}

		inline uint32 GetOutputCount() const
		{
			return m_node.GetOutputCount();
		}

		inline IAny* GetOutput(uint32 outputIdx)
		{
			SCHEMATYC2_SYSTEM_ASSERT(outputIdx < m_node.GetOutputCount());
			if(outputIdx < m_node.GetOutputCount())
			{
				const uint32 outputPos = m_node.GetOutputs()[outputIdx];
				if(outputPos != s_invalidIdx)
				{
					return m_scratchPad[outputPos];
				}
			}
			return nullptr;
		}

		inline IAny* GetOutput(uint32 outputIdx, const EnvTypeId& typeId)
		{
			IAny* pOutput = GetOutput(outputIdx);
			return pOutput && (pOutput->GetTypeInfo().GetTypeId().AsEnvTypeId() == typeId) ? pOutput : nullptr;
		}

		template <typename TYPE> inline TYPE* GetOutput(uint32 outputIdx)
		{
			IAny* pOutput = GetOutput(outputIdx, GetEnvTypeId<TYPE>());
			return pOutput ? static_cast<TYPE*>(pOutput->ToVoidPtr()) : nullptr;
		}

	private:

		const CRuntimeFunction& m_function;
		const CRuntimeNode&     m_node;
		const CRuntimeParams&   m_inputs;
		CRuntimeParams&         m_outputs;
		CScratchPad&            m_scratchPad;
	};

	inline void ExecuteRuntimeFunction(const CRuntimeFunction& function, IObject* pObject, const CRuntimeParams& inputs, CRuntimeParams& outputs)
	{
		// #SchematycTODO : Verify inputs.
		if(!function.Empty())
		{
			uint32                    pos = 0;
			const CRuntimeSequenceOp* pSequenceOp = function.GetSequenceOp(pos);
			if(pSequenceOp)
			{
				CScratchPad              scratchPad = function.GetScratchPad();
				SRuntimeActivationParams activationParams = pSequenceOp->GetActivationParams();
				std::vector<uint32>      repeatSequenceOps;
				repeatSequenceOps.reserve(64);
				while(true)
				{
					const CRuntimeNode*  pNode = function.GetNode(pSequenceOp->GetNodePos());
					CRuntimeNodeData     nodeData(function, *pNode, inputs, outputs, scratchPad);
					const SRuntimeResult result = (*pNode->GetCallback())(pObject, activationParams, nodeData);
					if(result.status == ERuntimeStatus::Break)
					{
						while(!repeatSequenceOps.empty())
						{
							const uint32 nodePos = function.GetSequenceOp(repeatSequenceOps.back())->GetNodePos();
							repeatSequenceOps.pop_back();
							if(nodePos == pSequenceOp->GetNodePos())
							{
								break;
							}
						}
					}
					if(result.status == ERuntimeStatus::Repeat)
					{
						repeatSequenceOps.push_back(pos);
					}
					if(result.status != ERuntimeStatus::Return)
					{
						if(result.status != ERuntimeStatus::End)
						{
							pos = pSequenceOp->GetOutputs()[result.outputIdx];
							if(pos != s_invalidIdx)
							{
								pSequenceOp      = function.GetSequenceOp(pos);
								activationParams = pSequenceOp->GetActivationParams();
								continue;
							}
						}
						if(!repeatSequenceOps.empty())
						{
							pos = repeatSequenceOps.back();
							repeatSequenceOps.pop_back();
							pSequenceOp      = function.GetSequenceOp(pos);
							activationParams = pSequenceOp->GetActivationParams();
							activationParams.flags |= ERuntimeActivationFlags::Repeat;
						}
					}
					break;
				}
			}
		}
	}
}
