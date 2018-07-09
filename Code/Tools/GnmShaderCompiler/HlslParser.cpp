// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "HlslParser.inl"

namespace HlslParser
{

SParserState::SParserState(SProgram& target, EShaderType shaderType)
	: result(target)
{
	InitTables(shaderType);

	SType invalidType;
	invalidType.base = kBaseTypeInvalid;
	invalidType.vecDimOrTypeId = 0;
	invalidType.matDimOrSamples = 0;
	invalidType.modifier = kTypeModifierNone;
	invalidType.arrDim = 0;
	result.userDefinedTypes.emplace_back(invalidType);

	SExpression invalidExpr;
	invalidExpr.op = SExpression::kOpInvalid;
	invalidExpr.operand[0] = invalidExpr.operand[1] = invalidExpr.operand[2] = 0;
	result.expressions.emplace_back(invalidExpr);

	SStatement invalidStat;
	invalidStat.exprIdOrVarId = 0;
	invalidStat.statementId[0] = invalidStat.statementId[1] = invalidStat.statementId[2] = 0;
	invalidStat.type = (HlslParser::SStatement::EType)-1;
	result.statements.emplace_back(invalidStat);
}

uint16_t SParserState::AddType(SType& type)
{
	const uint16_t typeId = (uint16_t)result.userDefinedTypes.size();
	result.userDefinedTypes.emplace_back(type);
	return typeId;
}

bool SParserState::AddTypedef(STypeName& item)
{
	if (typeNames.find(item.name))
	{
		return false;
	}

	typeNames.add(item.name, (uint32_t)result.userDefinedTypes.size());
	result.userDefinedTypes.emplace_back(item);
	return true;
}

bool SParserState::AddStruct(SStruct& item)
{
	if (!item.name.empty() && typeNames.find(item.name))
	{
		return false;
	}
	else
	{
		typeNames.add(item.name, (uint32_t)result.userDefinedTypes.size());
	}
	result.userDefinedTypes.emplace_back(item);
	return true;
}

bool SParserState::AddVariable(SVariableDeclaration& item, uint32_t initializer)
{
	const uint32_t varId = (uint32_t)result.variables.size();
	if (uint32_t* const pVar = variableNames.find(item.name))
	{
		const SScope* const pParentScope = scopes.empty() ? nullptr : &scopes.back();
		if (pParentScope && *pVar < pParentScope->scopeStart)
		{
			// Re-declaration of the same name in a new scope, allowed.
			// We just update the reference in the current table.
			*pVar = varId;
		}
		else
		{
			// Re-declaration in the same scope, not allowed.
			return false;
		}
	}
	else
	{
		// New name in the current scope.
		variableNames.add(item.name, varId);
	}
	if (initializer)
	{
		assert(item.initializer == 0);
		item.initializer = initializer;
	}
	item.scope = 0U;
	result.variables.emplace_back(item);
	return true;
}

bool SParserState::AddBuffer(SBuffer& item)
{
	// Introduce all names into the parent scope
	for (auto& item : item.members)
	{
		if (!AddVariable(item, 0U))
		{
			return false;
		}
	}
	return true;
}

void SParserState::AddFunction(SFunctionDeclaration& item)
{
	SProgram::TOverloadSet* pSet = nullptr;
	if (uint32_t* const pOther = functionNames.find(item.name))
	{
		pSet = &result.overloads.at(*pOther);
		for (auto& index : * pSet)
		{
			auto& other = result.objects[index];
			const SFunctionDeclaration& decl = boost::get<SFunctionDeclaration>(other);

			// Test that the function declarations have the same signature.
			// It's possible to forward-declare the same function any number of times.
			if (decl.arguments.size() != item.arguments.size()) continue;
			if (!result.IsSameType(decl.returnType, item.returnType, kTypeModifierConst)) continue;
			bool bSameSignature = true;
			for (size_t i = 0; i < decl.arguments.size(); ++i)
			{
				if (!result.IsSameType(decl.arguments[i].type, item.arguments[i].type, kTypeModifierConst))
				{
					bSameSignature = false;
					break;
				}
			}
			if (bSameSignature)
			{
				// This is the same function, there is nothing to do here.
				return;
			}
		}
	}
	if (!pSet)
	{
		const uint32_t setIndex = (uint32_t)result.overloads.size();
		result.overloads.emplace_back();
		pSet = &result.overloads.back();
		functionNames.add(item.name, setIndex);
	}

	const uint32_t myIndex = (uint32_t)result.objects.size();
	pSet->emplace_back(myIndex);
}

uint32_t SParserState::AddExpr(SExpression::EOperator op, uint32_t operand0, uint32_t operand1, uint32_t operand2)
{
	const uint32_t exprId = (uint32_t)result.expressions.size();
	result.expressions.emplace_back();
	SExpression& expr = result.expressions.back();

	expr.op = op;
	expr.operand[0] = operand0;
	expr.operand[1] = operand1;
	expr.operand[2] = operand2;
	return exprId;
}

uint32_t SParserState::AddTypedExpr(SExpression::EOperator op, uint32_t operand0, const SType& type)
{
	const uint32_t exprId = (uint32_t)result.expressions.size();
	result.expressions.emplace_back();
	SExpression& expr = result.expressions.back();

	expr.op = op;
	expr.operand[0] = operand0;
	::memcpy(expr.operand + 1, &type, sizeof(SType));
	return exprId;
}

uint32_t SParserState::AddDotExpr(uint32_t lhs, const std::string& rhs, bool bArrow)
{
	// Note: This code is actually wrong since we don't know the type of LHS.
	// To do this correctly, we would need to do type propagation in the expressions (but this requires a lot of compiler infra for type promotion/conversion and overload resolution).
	// So for the time being, we will just make assumption that anything that looks like a swizzle or a "member-intrinsic" of a texture object is exactly that.
	const uint32_t exprId = (uint32_t)result.expressions.size();
	result.expressions.emplace_back();
	SExpression& expr = result.expressions.back();

	// Try swizzles
	std::vector<uint32_t> swizzles;
	auto begin = rhs.begin();
	auto end = rhs.end();
	if (!bArrow && qi::parse(begin, end, qi::repeat(1, 4)[this->swizzles], swizzles) && (begin == end))
	{
		uint32_t bitOr = 0;
		for (auto item : swizzles)
		{
			bitOr |= item & kSwizzleCategoryMask;
		}
		if (bitOr == kSwizzleCategoryXYZW || bitOr == kSwizzleCategoryRGBA || bitOr == kSwizzleCategory_MXX || bitOr == kSwizzleCategory_XX)
		{
			// Assume this is swizzles
			while (swizzles.size() != 4)
			{
				swizzles.push_back(kSwizzleNone);
			}
			expr.op = SExpression::kOpSwizzle;
			expr.operand[0] = lhs;
			static_assert(sizeof(ESwizzle) * 4 == sizeof(expr.operand[1]), "Bad operand size");
			ESwizzle packed[4];
			for (int i = 0; i < 4; ++i)
			{
				packed[i] = static_cast<ESwizzle>(swizzles[i]);
			}
			::memcpy(expr.operand + 1, packed, sizeof(packed));
			expr.operand[2] = 0;
			return exprId;
		}
	}

	// Try known intrinsics
	const uint32_t* pIntrinsic = objectMembers.find(rhs);
	if (!bArrow && pIntrinsic)
	{
		expr.op = SExpression::kOpMemberIntrinsic;
		expr.operand[0] = lhs;
		expr.operand[1] = static_cast<EMemberIntrinsic>(*pIntrinsic);
		expr.operand[2] = 0;
		return exprId;
	}

	// Assume this is a member variable access
	expr.op = bArrow ? SExpression::kOpIndirectAccess : SExpression::kOpStructureAccess;
	expr.operand[0] = lhs;
	expr.operand[1] = (uint32_t)result.constants.size();
	expr.operand[2] = 0;

	// Store the member as a string literal (for now)
	// TODO: We need the type-info for the UDT here, and assign the member-index instead.
	result.constants.emplace_back(SStringLiteral { rhs });
	return exprId;
}

uint32_t SParserState::AddListExpr(SExpression::EOperator op, uint32_t operand1, const std::vector<uint32_t>& elem)
{
	// List must be created before the current node, as vector storage may change
	const uint32_t listId = MakeExpressionList(0U, elem);

	const uint32_t exprId = (uint32_t)result.expressions.size();
	result.expressions.emplace_back();
	SExpression& expr = result.expressions.back();

	expr.op = op;
	expr.operand[0] = listId;
	expr.operand[1] = operand1;
	expr.operand[2] = 0;
	return exprId;
}

uint32_t SParserState::AddTypedListExpr(SExpression::EOperator op, const SType& type, const std::vector<uint32_t>& elem)
{
	return AddTypedExpr(op, MakeExpressionList(0U, elem), type);
}

uint32_t SParserState::AddConstExpr(SProgram::TLiteral lit)
{
	const uint32_t exprId = (uint32_t)result.expressions.size();
	result.expressions.emplace_back();
	SExpression& expr = result.expressions.back();

	const uint32_t constId = (uint32_t)result.constants.size();
	result.constants.emplace_back(lit);

	expr.op = SExpression::kTerminalConstant;
	expr.operand[0] = constId;
	expr.operand[1] = expr.operand[2] = 0;
	return exprId;
}

uint32_t SParserState::AddStatement(SStatement::EType type, uint32_t exprOrVar, uint32_t stat0, uint32_t stat1, uint32_t stat2)
{
	const uint32_t statId = (uint32_t)result.statements.size();
	result.statements.emplace_back();
	SStatement& stat = result.statements.back();

	stat.type = type;
	stat.exprIdOrVarId = exprOrVar;
	stat.statementId[0] = stat0;
	stat.statementId[1] = stat1;
	stat.statementId[2] = stat2;
	stat.attr.type = SAttribute::kAttributeNone;
	stat.attr.value = 0;
	return statId;
}

uint32_t SParserState::AddStatementAttr(SStatement::EType type, boost::optional<SAttribute> attr, uint32_t exprOrVar, uint32_t stat0, uint32_t stat1, uint32_t stat2)
{
	const uint32_t statId = AddStatement(type, exprOrVar, stat0, stat1, stat2);
	if (attr)
	{
		SStatement& stat = result.statements.back();
		stat.attr = attr.get();
	}
	return statId;
}

uint32_t SParserState::AddVariableStatement(SVariableDeclaration& decl, uint32_t initializer)
{
	const uint32_t varId = (uint32_t)result.variables.size();
	if (!AddVariable(decl, initializer))
	{
		return 0U;
	}
	return AddStatement(SStatement::kVariable, varId, 0U, 0U, 0U);
}

uint32_t SParserState::AddStatementBlock(std::vector<uint32_t> stat)
{
	if (stat.empty())
	{
		stat.push_back(AddStatement(SStatement::kEmpty, 0U, 0U, 0U, 0U));
	}

	uint32_t prev = 0;
	for (uint32_t i = (uint32_t)stat.size(); i != 0; --i)
	{
		const uint32_t last = i - 1;
		uint32_t first = last;
		for (uint32_t j = i - 1; j != 0 && stat[j] == stat[j - 1] + 1; --i, --j, --first)
			;                                                                                // Find contiguous parts
		prev = AddStatement(SStatement::kBlock, 0U, stat[first], stat[last], prev);
	}

	return prev;
}

uint32_t SParserState::AppendVariableStatement(SVariableDeclaration& decl, uint32_t initializer)
{
	const uint32_t varId = (uint32_t)result.variables.size();
	if (!AddVariable(decl, initializer))
	{
		return 0U;
	}

	// Note: This is called during a multi-declaration, where another variable is declared in the same statement.
	// Unfortunately, we have no good way to represent this in the existing structure, so we instead promote the main statement to a block.
	SStatement& stat = result.statements.back();
	if (stat.type == SStatement::kVariable)
	{
		// Make a block out of the main and this multi declaration
		AddStatement(SStatement::kVariable, varId, 0U, 0U, 0U);
		const uint32_t statId = (uint32_t)result.statements.size();
		return AddStatement(SStatement::kBlock, 0U, statId - 2, statId - 1, 0xFFFFFFFFU);
	}
	else
	{
		// Continue a previous block of multi declarations
		assert(stat.type == SStatement::kBlock);
		stat.statementId[1] += 1;
		const uint32_t statId = AddStatement(SStatement::kVariable, varId, 0U, 0U, 0U);
		std::swap(result.statements.at(statId), result.statements.at(statId - 1));
		return statId;
	}
}

void SParserState::BeginScope()
{
	scopes.emplace_back();
	SScope& scope = scopes.back();
	scope.previousTable = variableNames;
	scope.scopeStart = (uint32_t)result.variables.size();
	scope.firstStatId = (uint32_t)result.statements.size();
}

void SParserState::EndScope()
{
	SScope& scope = scopes.back();
	const uint32_t scopeId = scope.firstStatId != result.statements.size() ? (uint32_t)result.statements.size() - 1U : 0xFFFFFFFFU;
	for (uint32_t varId = scope.scopeStart; varId < result.variables.size(); ++varId)
	{
		if (result.variables[varId].scope == 0U)
		{
			result.variables[varId].scope = scopeId;
		}
	}
	variableNames = std::move(scope.previousTable);
	scopes.pop_back();
}

uint32_t SParserState::GetLastRegisteredTypeId()
{
	return (uint32_t)result.userDefinedTypes.size() - 1;
}

uint16_t SParserState::RecurseApplyArrayTail(SType& target, uint16_t arrayDim)
{
	if (target.base == kBaseTypeMultiDim)
	{
		SType* const pNext = boost::get<SType>(&result.userDefinedTypes[target.vecDimOrTypeId]);
		if (pNext)
		{
			arrayDim = RecurseApplyArrayTail(*pNext, arrayDim);
		}
	}
	std::swap(target.arrDim, arrayDim);
	return arrayDim;
}

void SParserState::ApplyArrayTail(SType& target, uint16_t arrayDim)
{
	if (target.arrDim == 0)
	{
		// The type is not an array yet, store inline
		target.arrDim = arrayDim;
	}
	else
	{
		// Array-of-array can't be represented in one SType.
		// Create an intermediate "user-defined" type to represent it.
		// Note that we have to reverse the array order, as the first-parsed item is the outer dimension.
		uint16_t typeId = AddType(target);
		target.base = kBaseTypeMultiDim;
		target.vecDimOrTypeId = typeId;
		target.matDimOrSamples = 0U;
		target.arrDim = RecurseApplyArrayTail(target, arrayDim);
	}
}

bool SParserState::ApplyArrayTailConst(SType& target, uint32_t exprId)
{
	// An array dimension was supplied from a variable name.
	// We must look up the dimension in the already parsed state here.
	double exprVal;
	if (EvalExpression(exprId, exprVal) && exprVal > 0.0 && exprVal < (1U << 16))
	{
		ApplyArrayTail(target, static_cast<uint16_t>(exprVal));
		return true;
	}
	return false;
}

bool SParserState::EvalExpression(uint32_t exprId, double& out)
{
	// TODO: Take care of actual underlying type and a lot more operators.
	// We treat everything here as double, which may be inaccurate.
	// Type propagation support will be needed to do this properly.
	auto& expr = result.expressions.at(exprId);
	switch (expr.op)
	{
	case SExpression::kTerminalVariable:
		return EvalExpression(result.variables.at(expr.operand[0]).initializer, out);
	case SExpression::kTerminalConstant:
		{
			if (auto* pFloat = boost::get<SFloatLiteral>(&result.constants.at(expr.operand[0])))
			{
				out = pFloat->value;
				return true;
			}
			else if (auto* pInt = boost::get<SIntLiteral>(&result.constants.at(expr.operand[0])))
			{
				out = static_cast<double>(pInt->value); // Lossy!
				return true;
			}
			else if (auto* pBool = boost::get<SBoolLiteral>(&result.constants.at(expr.operand[0])))
			{
				out = pBool->value ? 1.0 : 0.0;
				return true;
			}
		}
		return false;
	case SExpression::kOpPlus:
	case SExpression::kOpSubExpression:
		return EvalExpression(expr.operand[0], out);
	case SExpression::kOpMinus:
		if (!EvalExpression(expr.operand[0], out))
		{
			return false;
		}
		out = -out;
		return true;
	case SExpression::kOpAdd:
		{
			double lhs, rhs;
			EvalExpression(expr.operand[0], lhs);
			EvalExpression(expr.operand[1], rhs);
			out = lhs + rhs;
		}
		return true;
	case SExpression::kOpSubtract:
		{
			double lhs, rhs;
			EvalExpression(expr.operand[0], lhs);
			EvalExpression(expr.operand[1], rhs);
			out = lhs - rhs;
		}
		return true;
	case SExpression::kOpMultiply:
		{
			double lhs, rhs;
			EvalExpression(expr.operand[0], lhs);
			EvalExpression(expr.operand[1], rhs);
			out = lhs * rhs;
		}
		return true;
	case SExpression::kOpDivide:
		{
			double lhs, rhs;
			EvalExpression(expr.operand[0], lhs);
			EvalExpression(expr.operand[1], rhs);
			out = lhs / rhs;
		}
		return true;
	case SExpression::kOpModulo:
		{
			double lhs, rhs;
			EvalExpression(expr.operand[0], lhs);
			EvalExpression(expr.operand[1], rhs);
			lhs += 0.5f;
			rhs += 0.5f;
			out = (double)((uint64_t)lhs % (uint64_t)rhs);
		}
		return true;
	case SExpression::kOpShiftLeft:
		{
			double lhs, rhs;
			EvalExpression(expr.operand[0], lhs);
			EvalExpression(expr.operand[1], rhs);
			lhs += 0.5f;
			rhs += 0.5f;
			out = (double)((uint64_t)lhs >> (uint64_t)rhs);
		}
		return true;
	case SExpression::kOpShiftRight:
		{
			double lhs, rhs;
			EvalExpression(expr.operand[0], lhs);
			EvalExpression(expr.operand[1], rhs);
			lhs += 0.5f;
			rhs += 0.5f;
			out = (double)((uint64_t)lhs << (uint64_t)rhs);
		}
		return true;
	}
	out = std::numeric_limits<double>::quiet_NaN();
	return false;
}

uint32_t SParserState::MakeExpressionList(uint32_t self, const std::vector<uint32_t>& list)
{
	SExpression* pPrev = &result.expressions[self];
	uint32_t exprId = self ? self : list.empty() ? 0U : list.front();
	for (uint32_t item : list)
	{
		SExpression* pExpr = &result.expressions[item];
		assert(pPrev->operand[2] == 0 || pPrev == &result.expressions.front() && "Each expression can be linked only once");
		if (pExpr->op >= SExpression::kListIndirection)
		{
			assert(pExpr->op != SExpression::kListIndirection && "Attempt to link a helper node");

			// Link the previous node to a new helper node.
			// Note: Need to do this BEFORE we actually mutate the collection since it invalidates all the pointers we have.
			pPrev->operand[2] = (uint32_t)result.expressions.size();

			// If this was supposed to be the first item in the list, we now need to return this helper node
			if (item == exprId)
			{
				exprId = pPrev->operand[2];
			}

			// Insert helper node
			result.expressions.emplace_back();
			SExpression& helper = result.expressions.back();
			helper.op = SExpression::kListIndirection;
			helper.operand[0] = item;
			helper.operand[1] = helper.operand[2] = 0;

			// Need to link with the helper node now
			pExpr = &helper;
		}
		else
		{
			// This is a typical expression, we can link it directly.
			pPrev->operand[2] = item;
		}
		pPrev = pExpr;
	}

	// The list must be terminated
	assert(pPrev->operand[2] == 0 || list.empty());
	return exprId;
}

void SParserState::GetExpressionList(const std::vector<SExpression>& expressions, uint32_t item, std::vector<const SExpression*>& result)
{
	while (item)
	{
		const SExpression* pExpr = &expressions[item];
		item = pExpr->operand[2];
		if (pExpr->op == SExpression::kListIndirection)
		{
			pExpr = &expressions[pExpr->operand[0]];
		}
		result.push_back(pExpr);
	}
}

uint32_t SParserState::PackNumThreads(uint32_t x, uint32_t y, uint32_t z)
{
	x = x - 1U;
	y = (y - 1U) << 12;
	z = (z - 1U) << 24;
	return x + y + z;
}

ESystemValue SParserState::MakeSystemValue(uint32_t tag, char digit)
{
	if (tag & kSystemValueWithDigits)
	{
		const uint32_t maxIndex = (tag & kSystemValueMaxDigitMask) >> kSystemValueMaxDigitShift;
		const uint32_t actualIndex = digit - '0';
		if (actualIndex > maxIndex)
		{
			return kSystemValueMax;
		}
		tag = (tag & ~(kSystemValueMaxDigitMask | kSystemValueWithDigits)) + actualIndex;
	}
	return static_cast<ESystemValue>(tag);
}

inline std::string SParserState::MakeErrorContext(TIterator pos, TIterator begin, TIterator end)
{
	uint32_t offset = 0;
	TIterator lineBegin = begin;
	while (begin != pos)
	{
		const char c = *begin++;
		if (c == '\r' || c == '\n')
		{
			lineBegin = begin;
			offset = 0;
		}
		else
		{
			++offset;
		}
	}
	TIterator lineEnd = pos;
	while (pos != end)
	{
		const char c = *++pos;
		if (c == '\r' || c == '\n')
		{
			lineEnd = pos;
			break;
		}
	}

	std::string result = "error (Context): ";
	result += std::string(lineBegin, lineEnd);
	result += "\nerror (Context): ";
	result += std::string(offset, ' ');
	result += "^ here\n";
	return result;
}

SProgram SProgram::Create(const char* szFile, EShaderType shaderType, bool bDebug)
{
	SProgram program;
	try
	{
		std::ifstream file(szFile);
		if (file.is_open())
		{
			file.unsetf(std::ios::skipws);
			TIterator end, begin(file);
			TIterator originalBegin = begin;
			SParserState state(program, shaderType);
			SParserState::SGrammar grammar(state, bDebug);
			SWhitespaceGrammar whiteSpace;

			const bool bParsed = phrase_parse(begin, end, grammar, whiteSpace, program.objects);
			if (!bParsed)
			{
				program.errorMessage = state.errorMessage.str();
			}
			else if (begin != end)
			{
				program.errorMessage = "error (HlslParser): Unspecified syntaxis error\n";
				program.errorMessage += state.MakeErrorContext(begin, originalBegin, end);
			}
			for (auto& object : program.objects)
			{
				if (auto *pVarDecl = boost::get<SVariableDeclaration>(&object))
				{
					pVarDecl->scope = 0U; // Global variable will not have been touched by scope logic, clear them now.
				}
			}
		}
		else
		{
			program.errorMessage = "error (I/O): Cannot open file: ";
			program.errorMessage += szFile;
		}
	}
	catch (std::exception& ex)
	{
		program.errorMessage = "error (HlslParser): Parse exception: ";
		program.errorMessage += ex.what();
	}
	return program;
}

SType SProgram::ExpandType(const SType& type, std::vector<uint32_t>& arrayDims) const
{
	const SType* pItem = &type;
	uint8_t modifiers = 0;
	while (pItem->base == kBaseTypeMultiDim || pItem->base == kBaseTypeUserDefined)
	{
		modifiers |= pItem->modifier;
		const TTypeElement& nextType = userDefinedTypes[pItem->vecDimOrTypeId];
		const SType* pNext = boost::get<SType>(&nextType);
		if (!pNext)
		{
			const STypeName* pTypeDef = boost::get<STypeName>(&nextType);
			if (!pTypeDef)
			{
				break;       // This is a struct, we can't go further.
			}
			pNext = &pTypeDef->type;
		}
		if (pItem->arrDim)
		{
			arrayDims.push_back(pItem->arrDim);
		}
		pItem = pNext;
	}
	SType result = *pItem;
	result.modifier |= modifiers;
	if (result.arrDim)
	{
		arrayDims.push_back(result.arrDim);
		result.arrDim = 0;
	}
	return result;
}

bool SProgram::IsSameType(const SType& type1, const SType& type2, uint8_t typeModifiers) const
{
	std::vector<uint32_t> array1, array2;
	SType baseType1 = ExpandType(type1, array1);
	SType baseType2 = ExpandType(type2, array2);
	baseType1.modifier &= ~typeModifiers;
	baseType2.modifier &= ~typeModifiers;
	const bool bSameBase = ::memcmp(&baseType1, &baseType2, sizeof(SType)) == 0;
	return bSameBase && (array1 == array2);
}

std::vector<const SExpression*> SProgram::GetExpressionList(uint32_t listOperand) const
{
	std::vector<const SExpression*> result;
	SParserState::GetExpressionList(expressions, listOperand, result);
	return result;
}

SType SExpression::GetType() const
{
	static_assert(sizeof(operand[1]) + sizeof(operand[2]) == sizeof(SType), "Bad type or operand size");
	SType result;
	::memcpy(&result, operand + 1, sizeof(SType));
	return result;
}

void SExpression::GetSwizzle(ESwizzle swizzle[4]) const
{
	static_assert(sizeof(operand[1]) == sizeof(ESwizzle) * 4, "Bad swizzle or operand size");
	::memcpy(swizzle, operand + 1, sizeof(ESwizzle) * 4);
}

SWhitespaceGrammar::SWhitespaceGrammar()
	: SWhitespaceGrammar::base_type(start, "whitespace")
{
	start %=
	  omit[
	    +((repo::confix("/*", "*/")[*(char_ - "*/")]
	       | repo::confix("//", eol)[*(char_ - eol)]
	       | +space
	       )
	      )];
	start.name("whitespace");
}

SParserState::SGrammar::SGrammar(SParserState& state, bool bDebug)
	: SGrammar::base_type(program)
{
	InitBaseGrammar(state);
	InitTypeGrammar(state);
	InitOpGrammar(state);
	InitStatGrammar(state);
	InitVarGrammar(state);
	InitHelpers(state, bDebug);
}

void SParserState::SGrammar::InitHelpers(SParserState& state, bool bDebug)
{
	if (bDebug)
	{
		debug(identifier);
		debug(floatLiteral);
		debug(intLiteral);
		debug(stringLiteral);
		debug(boolLiteral);
		debug(typeBase);
		debug(typeRecurse);
		debug(typeBuiltIn);
		debug(typeUserDefined);
		debug(typeVector);
		debug(typeMatrix);
		debug(typeLegacyVector);
		debug(typeLegacyMatrix);
		debug(typeObject0);
		debug(typeObject1);
		debug(typeObject2);
		debug(typeBuiltInBase);
		debug(typeDim);
		debug(typeModifiers);
		debug(typeArrayTail);
		debug(typeFull);
		debug(opPrimary);
		debug(opSpecial);
		debug(opUnary);
		debug(opMulDivMod);
		debug(opAddSub);
		debug(opShift);
		debug(opCompareOther);
		debug(opCompareHelper);
		debug(opCompareEq);
		debug(opBitwiseAnd);
		debug(opBitwiseXor);
		debug(opBitwiseOr);
		debug(opLogicalAnd);
		debug(opLogicalOr);
		debug(opTernary);
		debug(opAssign);
		debug(opAssignHelper);
		debug(opComma);
		debug(typeDef);
		debug(typeDefMatch);
		debug(semantic);
		debug(semanticSystemValue);
		debug(semanticPackOffset);
		debug(semanticRegisterSpec);
		debug(semanticSwizzle);
		debug(semanticRegisterType);
		debug(annotation);
		debug(annotationOne);
		debug(annotationStringType);
		debug(varDeclaration);
		debug(varDeclarationHelper);
		debug(varMultiDeclaration);
		debug(varStorageClass);
		debug(varInterpolationModifier);
		debug(varInputModifier);
		debug(structRegistered);
		debug(structHelper);
		debug(bufferRegistered);
		debug(bufferHelper);
		debug(funcRegistered);
		debug(funcHelper);
		debug(funcAttribute);
		debug(funcModifiers);
		debug(funcPackNumThreads);
		debug(statement);
		debug(statementHelper);
		debug(statementVar);
		debug(statementSwitchAttr);
		debug(statementIfAttr);
		debug(statementLoopAttr);
		debug(program);
	}

	// Catch parse errors
	on_error<fail>(
	  program,
	  state.errorMessage
	  << "error (HlslParser): expected \""
	  << _4
	  << "\""
	  << std::endl
	  << phx::bind(&SParserState::MakeErrorContext, phx::ref(state), _3, _1, _2)
	  << std::endl);
}

}
