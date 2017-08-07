#ifndef TO_GLSL_INSTRUCTION_H
#define TO_GLSL_INSTRUCTION_H

#include "internal_includes/structs.h"

void TranslateInstruction(HLSLCrossCompilerContext* psContext, Instruction* psInst, Instruction* psNextInst);

//For each MOV temp, immediate; check to see if the next instruction
//using that temp has an integer opcode. If so then the immediate value
//is flaged as having an integer encoding.
void MarkIntegerImmediates(HLSLCrossCompilerContext* psContext, uint32_t phase);

void SetDataTypes(HLSLCrossCompilerContext* psContext, Instruction* psInst, const int32_t i32InstCount, SHADER_VARIABLE_TYPE* aeCommonTempVecType);

#endif
