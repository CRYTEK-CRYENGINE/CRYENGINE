#ifndef DECODE_H
#define DECODE_H

#include "internal_includes/structs.h"

Shader* DecodeDXBC(uint32_t* data, unsigned int flags, GLLang language);

//You don't need to call this directly because DecodeDXBC
//will call DecodeDX9BC if the shader looks
//like it is SM1/2/3.
Shader* DecodeDX9BC(const uint32_t* pui32Tokens);

void UpdateDeclarationReferences(Shader* psShader, Declaration* psDeclaration);
void UpdateInstructionReferences(Shader* psShader, Instruction* psInstruction);

#endif
