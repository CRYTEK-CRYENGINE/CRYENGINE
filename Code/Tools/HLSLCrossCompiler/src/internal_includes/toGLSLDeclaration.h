#ifndef TO_GLSL_DECLARATION_H
#define TO_GLSL_DECLARATION_H

#include "internal_includes/structs.h"

void TranslateDeclaration(HLSLCrossCompilerContext* psContext, const Declaration* psDecl);

char* GetDeclaredInputName(const HLSLCrossCompilerContext* psContext, const SHADER_TYPE eShaderType, const Operand* psOperand);
char* GetDeclaredPatchConstantName(const HLSLCrossCompilerContext* psContext, const SHADER_TYPE eShaderType, const Operand* psOperand);
char* GetDeclaredOutputName(const HLSLCrossCompilerContext* psContext, const SHADER_TYPE eShaderType, const Operand* psOperand, int* stream);

//Hull shaders have multiple phases.
//Each phase has its own temps.
//Convert to global temps for GLSL.
void ConsolidateHullTempVars(Shader* psShader);

#endif
