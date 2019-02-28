// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ScriptGraphCompiler.h"

#include <CrySchematyc2/Env/EnvTypeId.h>
#include <CrySchematyc2/Runtime/RuntimeParams.h>
#include <CrySchematyc2/Runtime/ScratchPad.h>
#include <CrySchematyc2/Script/IScriptGraph.h>

#include "Lib.h"

namespace Schematyc2
{
	struct SCompiledNode
	{
		inline SCompiledNode(const SGUID& _guid)
			: guid(_guid)
			, pCallback(nullptr)
		{}

		SGUID                              guid;
		RuntimeNodeCallbackPtr             pCallback;
		std::vector<SRuntimeNodeAttribute> attributes;
		std::vector<uint32>                inputs;
		std::vector<uint32>                outputs;
	};

	typedef std::vector<SCompiledNode> CompiledNodes;

	uint32 FindCompiledNode(const CompiledNodes& nodes, const SGUID& guid)
	{
		for(uint32 nodeIdx = 0, nodeCount = nodes.size(); nodeIdx < nodeCount; ++ nodeIdx)
		{
			if(nodes[nodeIdx].guid == guid)
			{
				return nodeIdx;
			}
		}
		return s_invalidIdx;
	}

	struct SCompiledSequenceOp
	{
		inline SCompiledSequenceOp()
			: nodeIdx(s_invalidIdx)
		{}

		uint32                   nodeIdx;
		SRuntimeActivationParams activationParams;
		std::vector<uint32>      outputs;
	};

	typedef std::vector<SCompiledSequenceOp> CompiledSequenceOps;
	
	struct SCompiledFunction
	{
		typedef std::vector<SRuntimeFunctionParam> Params;

		Params              inputs;
		Params              outputs;
		CompiledNodes       nodes;
		CompiledSequenceOps sequenceOps;
		CScratchPad         scratchPad;
	};

	struct SScriptGraphCompilerContext
	{
		inline SScriptGraphCompilerContext(const ILibClass& _libClass, const IScriptGraphExtension& _graph, SCompiledFunction& _compiledFunction)
			: libClass(_libClass)
			, graph(_graph)
			, compiledFunction(_compiledFunction)
		{}

		void Error(const char* szError) // #SchematycTODO : Add support for variadic parameters!!!
		{
			errors.push_back(szError);
		}

		const ILibClass&             libClass;
		const IScriptGraphExtension& graph;
		SCompiledFunction&           compiledFunction;
		std::vector<string>          errors;
	};

	class CScriptGraphNodeCompiler : public IScriptGraphNodeCompiler
	{
	public:

		CScriptGraphNodeCompiler(SScriptGraphCompilerContext& context, const IScriptGraphNode& graphNode, SCompiledNode& compiledNode)
			: m_context(context)
			, m_graphNode(graphNode)
			, m_compiledNode(compiledNode)
		{}

		// IScriptGraphNodeCompiler

		virtual const ILibClass& GetLibClass() const override
		{
			return m_context.libClass;
		}

		virtual bool BindCallback(RuntimeNodeCallbackPtr pCallback) override
		{
			if(m_compiledNode.pCallback)
			{
				// Error : Callback already bound!!!
				return false;
			}

			m_compiledNode.pCallback = pCallback;
			return true;
		}

		virtual bool BindAnyAttribute(uint32 attributeId, const IAny& value) override
		{
			for(const SRuntimeNodeAttribute& attribute : m_compiledNode.attributes)
			{
				if(attribute.id == attributeId)
				{
					// Error : Attribute already bound!!!
					return false;
				}
			}

			m_compiledNode.attributes.push_back(SRuntimeNodeAttribute(attributeId, m_context.compiledFunction.scratchPad.PushBack(value)));
			return true;
		}

		virtual bool BindAnyInput(uint32 inputIdx, const IAny& value) override
		{
			// #SchematycTODO : Validate inputIdx!!!
			// #SchematycTODO : Make sure port has correct flags set.

			if(m_compiledNode.inputs[inputIdx] != s_invalidIdx)
			{
				// Error : Input already bound!!!
				return false;
			}

			uint32 pos = s_invalidIdx;
			bool   bError = false;
			auto visitGraphLink = [this, &value, &pos, &bError] (const IScriptGraphLink& graphLink) -> EVisitStatus
			{
				const IScriptGraphNode* pGraphNode = nullptr;
				uint32                  outputIdx;
				if(m_context.graph.GetLinkSrc(graphLink, pGraphNode, outputIdx))
				{
					const uint32 nodeIdx = FindCompiledNode(m_context.compiledFunction.nodes, pGraphNode->GetGUID());
					if(nodeIdx == s_invalidIdx)
					{
						// Error : Failed to find compiled node!!!
						bError = true;
					}
					else
					{
						const SCompiledNode& compiledNode = m_context.compiledFunction.nodes[nodeIdx];
						const uint32&        outputPos = compiledNode.outputs[outputIdx];
						if(outputPos == s_invalidIdx)
						{
							// Error : Failed to find node output!!!
							bError = true;
						}
						else
						{
							const IAny* pOutputValue = m_context.compiledFunction.scratchPad[outputPos];
							if(pOutputValue->GetTypeInfo().GetTypeId() != value.GetTypeInfo().GetTypeId())
							{
								// Error : Type mismatch!!!
								bError = true;
							}
							else
							{
								pos = outputPos;
							}
						}
					}
				}
				return EVisitStatus::End;
			};
			m_context.graph.VisitInputLinks(ScriptGraphLinkConstVisitor::FromLambdaFunction(visitGraphLink), m_graphNode.GetGUID(), m_graphNode.GetInputName(inputIdx));

			if(bError)
			{
				return false;
			}

			if(pos == s_invalidIdx)
			{
				pos = m_context.compiledFunction.scratchPad.PushBack(value);
			}
			m_compiledNode.inputs[inputIdx] = pos;
			return true;
		}

		virtual bool BindAnyOutput(uint32 outputIdx, const IAny& value) override
		{
			// #SchematycTODO : Validate inputIdx!!!
			// #SchematycTODO : Make sure port has correct flags set.

			if(m_compiledNode.outputs[outputIdx] != s_invalidIdx)
			{
				// Error : Output already bound!!!
				return false;
			}

			bool bLinkedOutput = false;
			auto visitGraphLink = [this, &bLinkedOutput] (const IScriptGraphLink& graphLink) -> EVisitStatus
			{
				bLinkedOutput = true;
				return EVisitStatus::Stop;
			};
			m_context.graph.VisitOutputLinks(ScriptGraphLinkConstVisitor::FromLambdaFunction(visitGraphLink), m_graphNode.GetGUID(), m_graphNode.GetOutputName(outputIdx));

			const uint32 pos = bLinkedOutput ? m_context.compiledFunction.scratchPad.PushBack(value) : s_invalidIdx;
			m_compiledNode.outputs[outputIdx] = pos;
			return true;
		}

		virtual bool BindAnyFunctionInput(uint32 inputIdx, const IAny& value) override
		{
			return false;
		}

		virtual bool BindAnyFunctionOutput(uint32 ouputIdx, const IAny& value) override
		{
			return false;
		}

		// ~IScriptGraphNodeCompiler

	private:

		SScriptGraphCompilerContext& m_context;
		const IScriptGraphNode&      m_graphNode;
		SCompiledNode&               m_compiledNode;
	};

	void CScriptGraphCompiler::CompileGraph(CLibClass& libClass, const IScriptGraphExtension& graph, CRuntimeFunction& runtimeFunction)
	{
		auto visitGraphNode = [this, &libClass, &graph, &runtimeFunction] (const IScriptGraphNode& graphNode) -> EVisitStatus
		{
			for(size_t outputIdx = 0, outputCount = graphNode.GetOutputCount(); outputIdx < outputCount; ++ outputIdx)
			{
				if((graphNode.GetOutputFlags(outputIdx) & EScriptGraphPortFlags::BeginSequence) != 0)
				{
					CompileFunction(libClass, graph, runtimeFunction, graphNode, outputIdx);
				}
			}
			return EVisitStatus::Continue;
		};
		graph.VisitNodes(ScriptGraphNodeConstVisitor::FromLambdaFunction(visitGraphNode));
	}

	void CScriptGraphCompiler::CompileFunction(CLibClass& libClass, const IScriptGraphExtension& graph, CRuntimeFunction& runtimeFunction, const IScriptGraphNode& graphNode, size_t outputIdx)
	{
		SCompiledFunction compiledFunction;
		compiledFunction.nodes.reserve(graph.GetNodeCount());
		compiledFunction.sequenceOps.reserve(1024);
		compiledFunction.scratchPad.Reserve(0x1000);

		SScriptGraphCompilerContext context(libClass, graph, compiledFunction);
		CompileFunctionForwardsRecursive(context, graphNode, SRuntimeActivationParams(ERuntimeActivationFlags::Output, outputIdx));

		if(context.errors.empty())
		{
			OptimizeFunction(compiledFunction);
			FinalizeFunction(compiledFunction, runtimeFunction);
		}
		else
		{
			for(const string& error : context.errors)
			{
				SCHEMATYC2_COMPILER_ERROR(error.c_str());
			}
		}
	}

	uint32 CScriptGraphCompiler::CompileFunctionForwardsRecursive(SScriptGraphCompilerContext& context, const IScriptGraphNode& graphNode, const SRuntimeActivationParams& activationParams)
	{
		uint32 inPos = context.compiledFunction.sequenceOps.size();
		context.compiledFunction.sequenceOps.push_back(SCompiledSequenceOp());

		SCompiledSequenceOp& sequenceOp = context.compiledFunction.sequenceOps.back();
		sequenceOp.activationParams = activationParams;

		const uint32 outputCount = graphNode.GetOutputCount();
		sequenceOp.outputs.resize(outputCount, s_invalidIdx);
		
		inPos              = CompileFunctionBackwardsRecursive(context, graphNode, inPos);
		sequenceOp.nodeIdx = CompileNode(context, graphNode);

		// #SchematycTODO : Make sure node compiled successfully!
		// #SchematycTODO : We need a special case for activation of outputs! When this happens we should only trace the activated output.

		const bool bTerminate = ((activationParams.flags & ERuntimeActivationFlags::Input) != 0) && ((graphNode.GetInputFlags(activationParams.portIdx) & EScriptGraphPortFlags::EndSequence) != 0);
		if(!bTerminate)
		{
			// #SchematycTODO : Make sure that we haven't been here before! If we have the 'EndSequence' flag should be set.
			
			const SGUID nodeGUID = graphNode.GetGUID();
			for(uint32 outputIdx = 0; outputIdx < outputCount; ++ outputIdx)
			{
				if((graphNode.GetOutputFlags(outputIdx) & EScriptGraphPortFlags::Execute) != 0)
				{
					auto visitGraphLink = [this, &context, &sequenceOp, outputIdx] (const IScriptGraphLink& graphLink) -> EVisitStatus
					{
						const IScriptGraphNode* pGraphNode = nullptr;
						uint32                  inputIdx;
						if(context.graph.GetLinkDst(graphLink, pGraphNode, inputIdx))
						{
							const uint32 pos = CompileFunctionForwardsRecursive(context, *pGraphNode, SRuntimeActivationParams(ERuntimeActivationFlags::Input, inputIdx));
							sequenceOp.outputs[outputIdx] = pos;
						}
						return EVisitStatus::End;
					};
					context.graph.VisitOutputLinks(ScriptGraphLinkConstVisitor::FromLambdaFunction(visitGraphLink), nodeGUID, graphNode.GetOutputName(outputIdx));
				}
			}
		}

		return inPos;
	}

	uint32 CScriptGraphCompiler::CompileFunctionBackwardsRecursive(SScriptGraphCompilerContext& context, const IScriptGraphNode& graphNode, uint32 outPos)
	{
		uint32      inPos = outPos;
		const SGUID nodeGUID = graphNode.GetGUID();
		for(uint32 inputIdx = 0, inputCount = graphNode.GetInputCount(); inputIdx < inputCount; ++ inputIdx)
		{
			if((graphNode.GetInputFlags(inputIdx) & EScriptGraphPortFlags::Data) != 0)
			{
				auto visitGraphLink = [this, &context, &inPos] (const IScriptGraphLink& graphLink) -> EVisitStatus
				{
					const IScriptGraphNode* pGraphNode = nullptr;
					uint32                  outputIdx;
					if(context.graph.GetLinkSrc(graphLink, pGraphNode, outputIdx))
					{
						const EScriptGraphPortFlags outputFlags = pGraphNode->GetOutputFlags(outputIdx);
						if(((outputFlags & EScriptGraphPortFlags::Data) != 0) && ((outputFlags & EScriptGraphPortFlags::Pull) != 0))
						{
							const uint32 pos = context.compiledFunction.sequenceOps.size();
							context.compiledFunction.sequenceOps.push_back(SCompiledSequenceOp());

							SCompiledSequenceOp& sequenceOp = context.compiledFunction.sequenceOps.back();
							sequenceOp.activationParams = SRuntimeActivationParams(ERuntimeActivationFlags::Output, outputIdx);

							sequenceOp.outputs.resize(pGraphNode->GetOutputCount(), s_invalidIdx);
							sequenceOp.outputs[outputIdx] = inPos;

							inPos              = CompileFunctionBackwardsRecursive(context, *pGraphNode, pos);
							sequenceOp.nodeIdx = CompileNode(context, *pGraphNode);
						}
					}
					return EVisitStatus::End;
				};
				context.graph.VisitInputLinks(ScriptGraphLinkConstVisitor::FromLambdaFunction(visitGraphLink), nodeGUID, graphNode.GetInputName(inputIdx));
			}
		}
		return inPos;
	}

	uint32 CScriptGraphCompiler::CompileNode(SScriptGraphCompilerContext& context, const IScriptGraphNode& graphNode)
	{
		const SGUID nodeGUID = graphNode.GetGUID();
		uint32      nodeIdx = FindCompiledNode(context.compiledFunction.nodes, nodeGUID);
		if(nodeIdx != s_invalidIdx)
		{
			return nodeIdx;
		}

		nodeIdx = context.compiledFunction.nodes.size();
		context.compiledFunction.nodes.push_back(SCompiledNode(nodeGUID));

		SCompiledNode& compiledNode = context.compiledFunction.nodes.back();
		compiledNode.inputs.resize(graphNode.GetInputCount(), s_invalidIdx);
		compiledNode.outputs.resize(graphNode.GetOutputCount(), s_invalidIdx);
		
		CScriptGraphNodeCompiler nodeCompiler(context, graphNode, compiledNode);
		graphNode.Compile_New(nodeCompiler);

		if(!compiledNode.pCallback)
		{
			context.Error("No callback was specified for node!");
		}

		return nodeIdx;
	}

	void CScriptGraphCompiler::OptimizeFunction(SCompiledFunction& function)
	{
		if(!function.sequenceOps.empty())
		{
			std::vector<uint32> indices;
			IndexFunctionRecursive(function, indices, 0);
			CompiledSequenceOps optimizedSequenceOps;
			optimizedSequenceOps.push_back(function.sequenceOps.front());
			for(uint32& idx : indices)
			{
				optimizedSequenceOps.push_back(function.sequenceOps[idx]);
			}
			optimizedSequenceOps.shrink_to_fit();
			std::swap(function.sequenceOps, optimizedSequenceOps);
		}
	}

	void CScriptGraphCompiler::IndexFunctionRecursive(SCompiledFunction& function, std::vector<uint32>& indices, uint32 pos)
	{
		SCompiledSequenceOp& sequenceOp = function.sequenceOps[pos];
		for(uint32& output : sequenceOp.outputs)
		{
			const uint32 outputPos = output;
			if(outputPos != s_invalidIdx)
			{
				if(std::find(indices.begin(), indices.end(), outputPos) == indices.end())
				{
					indices.push_back(outputPos);
					output = indices.size();
					IndexFunctionRecursive(function, indices, outputPos);
				}
			}
		}
	}

	void CScriptGraphCompiler::FinalizeFunction(const SCompiledFunction& compiledFunction, CRuntimeFunction& runtimeFunction)
	{
		std::vector<uint32> nodeLinks;
		nodeLinks.reserve(compiledFunction.nodes.size());

		for(const SCompiledNode& compiledNode : compiledFunction.nodes)
		{
			const uint32 attributeCount = compiledNode.attributes.size();
			const uint32 inputCount = compiledNode.inputs.size();
			const uint32 outputCount = compiledNode.outputs.size();
			const uint32 pos = runtimeFunction.AddNode(compiledNode.guid, compiledNode.pCallback, attributeCount, inputCount, outputCount);
			nodeLinks.push_back(pos);

			CRuntimeNode* pRuntimeNode = runtimeFunction.GetNode(pos);
			if(attributeCount)
			{
				SRuntimeNodeAttribute* pRuntimeNodeAttributes = pRuntimeNode->GetAttributes();
				for(uint32 attributeIdx = 0; attributeIdx < attributeCount; ++ attributeIdx)
				{
					pRuntimeNodeAttributes[attributeIdx] = compiledNode.attributes[attributeIdx];
				}
			}
			if(inputCount)
			{
				uint32* pRuntimeNodeInputs = pRuntimeNode->GetInputs();
				for(uint32 inputIdx = 0; inputIdx < inputCount; ++ inputIdx)
				{
					pRuntimeNodeInputs[inputIdx] = compiledNode.inputs[inputIdx];
				}
			}
			if(outputCount)
			{
				uint32* pRuntimeNodeOutputs = pRuntimeNode->GetOutputs();
				for(uint32 outputIdx = 0; outputIdx < outputCount; ++ outputIdx)
				{
					pRuntimeNodeOutputs[outputIdx] = compiledNode.outputs[outputIdx];
				}
			}
		}

		std::vector<uint32> sequenceOpLinks;
		sequenceOpLinks.reserve(compiledFunction.sequenceOps.size());

		for(const SCompiledSequenceOp& compiledSequenceOp : compiledFunction.sequenceOps)
		{
			const uint32 pos = runtimeFunction.AddSequenceOp(nodeLinks[compiledSequenceOp.nodeIdx], compiledSequenceOp.activationParams, compiledSequenceOp.outputs.size());
			sequenceOpLinks.push_back(pos);
		}

		for(uint32 sequenceOpIdx = 0, sequenceOpCount = compiledFunction.sequenceOps.size(); sequenceOpIdx < sequenceOpCount; ++ sequenceOpIdx)
		{
			const SCompiledSequenceOp& compiledSequenceOp = compiledFunction.sequenceOps[sequenceOpIdx];
			uint32*                    pRuntimeSequenceOpOutputs = runtimeFunction.GetSequenceOp(sequenceOpLinks[sequenceOpIdx])->GetOutputs();
			for(uint32 outputIdx = 0, outputCount = compiledSequenceOp.outputs.size(); outputIdx < outputCount; ++ outputIdx)
			{
				const uint32 outputPos = compiledSequenceOp.outputs[outputIdx];
				if(outputPos != s_invalidIdx)
				{
					pRuntimeSequenceOpOutputs[outputIdx] = sequenceOpLinks[outputPos];
				}
				else
				{
					pRuntimeSequenceOpOutputs[outputIdx] = s_invalidIdx;
				}
			}
		}

		runtimeFunction.GetScratchPad() = compiledFunction.scratchPad;
	}
}
