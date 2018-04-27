// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef VariableCollection_h
#define VariableCollection_h

#pragma once

#ifndef _RELEASE
	#define DEBUG_VARIABLE_COLLECTION
#endif //_RELEASE

#if !defined(_RELEASE) && CRY_PLATFORM_WINDOWS
	#define USING_VARIABLE_COLLECTION_SERIALIZATION
	#define USING_VARIABLE_COLLECTION_XML_DESCRIPTION_CREATION
#endif

#ifdef DEBUG_VARIABLE_COLLECTION
	#include <CryAISystem/IAISystem.h>
	#include <CryAISystem/IAIDebugRenderer.h>
#endif // DEBUG_VARIABLE_COLLECTION

#include <CryNetwork/ISerialize.h>
#include <CryString/StringUtils.h>

#if defined(USING_VARIABLE_COLLECTION_SERIALIZATION)
	#include <CrySerialization/IArchive.h>
	#include <CrySerialization/STL.h>
	#include <CrySerialization/StringList.h>
#endif

namespace Variables
{
typedef uint32 VariableID;

class Collection
{
#ifdef DEBUG_VARIABLE_COLLECTION
	friend struct DebugHelper;
#endif // DEBUG_VARIABLE_COLLECTION

public:
	Collection()
		: m_changed(false)
	{
	}

	bool GetVariable(const VariableID& variableID, bool* value) const
	{
		VariableMap::const_iterator it = m_variableMap.find(variableID);

		if (it != m_variableMap.end())
		{
			const Variable& variable = it->second;
			*value = variable.value;

			return true;
		}

		return false;
	}

	bool SetVariable(const VariableID& variableID, bool value)
	{
		Variable& variable = stl::map_insert_or_get(m_variableMap, variableID, Variable());
		if (variable.value != value)
		{
			variable.value = value;
			m_changed = true;

			CTimeValue now = gEnv->pTimer->GetFrameStartTime();

#ifdef DEBUG_VARIABLE_COLLECTION
			m_history.push_front(VariableChangeEvent(now, variableID, value));
			while (m_history.size() > 25)
				m_history.pop_back();
#endif  // DEBUG_VARIABLE_COLLECTION
		}

		return false;
	}

	void ResetChanged(bool state = false)
	{
		m_changed = state;
	}

	bool Changed() const
	{
		return m_changed;
	}

	void Swap(Collection& other)
	{
		std::swap(m_changed, other.m_changed);
		m_variableMap.swap(other.m_variableMap);

#ifdef DEBUG_VARIABLE_COLLECTION
		m_history.swap(other.m_history);
#endif // DEBUG_VARIABLE_COLLECTION
	}

	void Serialize(TSerialize ser)
	{
		ser.Value("m_variables", m_variableMap);

		if (ser.IsReading())
		{
			m_changed = true;   // Force the selection tree to be re-evaluated
		}
	}

	size_t GetVariablesAmount() const
	{
		return m_variableMap.size();
	}

private:
	struct Variable
	{
		Variable()
			: value(false)
		{
		}

		bool value;

		void Serialize(TSerialize ser)
		{
			ser.Value("value", value);
		}
	};

	typedef std::unordered_map<VariableID, Variable, stl::hash_uint32> VariableMap;
	VariableMap m_variableMap;

	bool        m_changed;

#ifdef DEBUG_VARIABLE_COLLECTION
	struct VariableChangeEvent
	{
		VariableChangeEvent(const CTimeValue& _when, const VariableID _variableID, bool _value)
			: when(_when)
			, variableID(_variableID)
			, value(_value)
		{
		}

		CTimeValue when;
		VariableID variableID;
		bool       value;
	};

	typedef std::deque<VariableChangeEvent> History;
	History m_history;
#endif // DEBUG_VARIABLE_COLLECTION
};

struct Description
{
	Description() {};

	Description(const char* _name)
		: name(_name)
	{
	}

#if defined(USING_VARIABLE_COLLECTION_SERIALIZATION)
	void Serialize(Serialization::IArchive& archive)
	{
		archive(name, "name", "^<");
		if (name.empty())
			archive.error(name, "Name must be specified");
	}

	bool operator<(const Description& rhs) const
	{
		return (name < rhs.name);
	}
#endif

	string name;
};

typedef std::map<VariableID, Description> VariableDescriptions;

static VariableID GetVariableID(const char* name)
{
	return CryStringUtils::CalculateHashLowerCase(name);
}

class Declarations
{
public:
	Declarations()
	{
	}

	bool LoadFromXML(const XmlNodeRef& rootNode, const char* fileName)
	{
		const int childCount = rootNode->getChildCount();
		for (int i = 0; i < childCount; ++i)
		{
			XmlNodeRef childNode = rootNode->getChild(i);
			if (!stricmp(childNode->getTag(), "Variable"))
			{
				const char* variableName = 0;
				if (childNode->haveAttr("name"))
					childNode->getAttr("name", &variableName);
				else
				{
					gEnv->pLog->LogWarning("Missing 'name' attribute for tag '%s' in file '%s' at line %d.", childNode->getTag(), fileName, childNode->getLine());
					return false;
				}

				bool defaultValue = false;
				if (childNode->haveAttr("default"))
				{
					const char* value = 0;
					childNode->getAttr("default", &value);

					if (!stricmp(value, "true"))
						defaultValue = true;
					else if (!stricmp(value, "false"))
						defaultValue = false;
					else
					{
						gEnv->pLog->LogWarning("Invalid variable value '%s' for variable '%s' in file '%s' at line %d.", value, variableName, fileName, childNode->getLine());
						return false;
					}
				}

				VariableID variableID = GetVariableID(variableName);
				std::pair<VariableDescriptions::iterator, bool> iresult = m_descriptions.insert(
				  VariableDescriptions::value_type(variableID, Description(variableName)));

				if (!iresult.second)
				{
					if (!stricmp(iresult.first->second.name, variableName))
					{
						gEnv->pLog->LogWarning("Duplicate variable declaration '%s' in file '%s' at line %d.", variableName, fileName, childNode->getLine());
					}
					else
					{
						gEnv->pLog->LogWarning("Hash collision for variable declaration '%s' in file '%s' at line %d.", variableName, fileName, childNode->getLine());
					}

					return false;
				}

				m_defaults.SetVariable(variableID, defaultValue);
				m_defaults.ResetChanged(true);
			}
			else
			{
				gEnv->pLog->LogWarning("Unexpected tag '%s' in file '%s' at line %d. 'Variable' expected.", childNode->getTag(), fileName, childNode->getLine());
				return false;
			}
		}

		return true;
	}

	bool IsDeclared(const VariableID& variableID) const
	{
		return m_descriptions.find(variableID) != m_descriptions.end();
	}

	const Description& GetVariableDescription(const VariableID& variableID) const
	{
		VariableDescriptions::const_iterator it = m_descriptions.find(variableID);
		if (it != m_descriptions.end())
		{
			const Description& description = it->second;
			return description;
		}

		static Description empty("<invalid>");
		return empty;
	}

	const VariableDescriptions& GetVariableDescriptions() const
	{
		return m_descriptions;
	}

	void GetVariablesNames(const char** variableNamesBuffer, const size_t maxSize, size_t& actualSize) const
	{
		const size_t totalVariablesAmount = m_descriptions.size();
		if (maxSize < totalVariablesAmount)
		{
			gEnv->pLog->LogWarning("Only %" PRISIZE_T " can be inserted into the buffer while %" PRISIZE_T " are currently present.", (totalVariablesAmount - maxSize), totalVariablesAmount);
		}

		VariableDescriptions::const_iterator it = m_descriptions.begin();
		VariableDescriptions::const_iterator end = m_descriptions.end();
		for (; it != end; ++it)
		{
			if (actualSize < maxSize)
			{
				const Description& description = it->second;
				variableNamesBuffer[actualSize++] = description.name.c_str();
			}
		}
	}

	const Collection& GetDefaults() const
	{
		return m_defaults;
	}

#if defined(USING_VARIABLE_COLLECTION_XML_DESCRIPTION_CREATION)
	XmlNodeRef CreateXmlDescription()
	{
		if (m_descriptions.size() == 0)
			return XmlNodeRef();

		XmlNodeRef variablesXml = GetISystem()->CreateXmlNode("Variables");
		for (VariableDescriptions::const_iterator it = m_descriptions.begin(), end = m_descriptions.end(); it != end; ++it)
		{
			const Description& description = it->second;

			XmlNodeRef variableXml = GetISystem()->CreateXmlNode("Variable");
			variableXml->setAttr("name", description.name);

			variablesXml->addChild(variableXml);
		}

		return variablesXml;
	}
#endif

#if defined(USING_VARIABLE_COLLECTION_SERIALIZATION)
	void Serialize(Serialization::IArchive& archive)
	{
		archive(m_descriptions, "variableDescriptions", "^[<>]");
	}
#endif

private:
	VariableDescriptions m_descriptions;
	Collection           m_defaults;
};

class SimpleLexer
{
public:
	SimpleLexer(const char* buffer)
		: m_buffer(buffer)
		, m_obuffer(buffer)
	{
	}

	int peek(string* ident = 0)
	{
		const char* obuffer = m_buffer;
		string oident(m_ident);
		int tok = operator()();
		m_buffer = obuffer;
		if (ident)
			ident->swap(m_ident);
		m_ident.swap(oident);

		return tok;
	}

	const char* ident() const
	{
		return m_ident.c_str();
	}

	const char* buf() const
	{
		return m_obuffer;
	}

	int operator()()
	{
		while (unsigned char ch = *m_buffer++)
		{
			unsigned char next = *m_buffer;

			switch (ch)
			{
			case '(':
			case ')':
				return (int)ch;
			case '=':
				if (next == '=')
				{
					++m_buffer;
					return '==';
				}
				return ch;
			case '!':
				if (next == '=')
				{
					++m_buffer;
					return '!=';
				}
				return ch;
			default:
				{
					if (isalpha(ch) || (ch == '_'))
					{
						m_ident.clear();
						m_ident.push_back(ch);
						while ((ch = *m_buffer) && (isalnum(ch) || (ch == '.') || (ch == '_')))
						{
							++m_buffer;
							m_ident.push_back(ch);
						}

						// keywords
						if (!stricmp(m_ident.c_str(), "or"))
							return 'or';
						else if (!stricmp(m_ident.c_str(), "and"))
							return 'and';
						else if (!stricmp(m_ident.c_str(), "xor"))
							return 'xor';
						else if (!stricmp(m_ident.c_str(), "true"))
							return 'true';
						else if (!stricmp(m_ident.c_str(), "false"))
							return 'fals';
						return 'var';
					}
					else if (isspace(ch))
						continue;

					return ch;
				}
			}
		}

		return 0;
	}

private:
	const char* m_buffer;
	const char* m_obuffer;
	string      m_ident;
};

/*
   Simple logical expression for the variable collections.
   The expression is pre-compiled and stored as a vector of byte-code ops.
 */
class Expression
{
	struct ExpressionOperator;

public:
	Expression()
		: m_rootID(-1)
	{
	}

	Expression(const char* expression, const Declarations& declarations)
	{
		Reset(expression, declarations);
	}

	void Reset(const char* expression, const Declarations& declarations)
	{
		m_rootID = Parse(expression, declarations);
	}

	bool Evaluate(const Collection& collection) const
	{
		if (Valid())
			return EvaluateOp(collection, m_expressionOps[m_rootID]);

		return false;
	}

	bool Valid() const
	{
		return m_rootID >= 0;
	}

private:
	int AddOp(const ExpressionOperator& op)
	{
		m_expressionOps.push_back(op);
		return (int)m_expressionOps.size() - 1;
	}

	int ParseLogical(SimpleLexer& lex, int tok, const Declarations& declarations)
	{
		int leftID = ParseCompOp(lex, tok, declarations);
		if (leftID == -1)
			return -1;

		while ((tok = lex.peek()) && ((tok == 'and') || (tok == 'or') || (tok == 'xor')))
		{
			lex();

			int rightID = ParseCompOp(lex, lex(), declarations);
			if (rightID == -1)
				return -1;

			switch (tok)
			{
			case 'or':
				leftID = AddOp(ExpressionOperator(ExpressionOperator::Or, leftID, rightID));
				break;
			case 'and':
				leftID = AddOp(ExpressionOperator(ExpressionOperator::And, leftID, rightID));
				break;
			case 'xor':
				leftID = AddOp(ExpressionOperator(ExpressionOperator::Xor, leftID, rightID));
				break;
			}
		}

		return leftID;
	}

	int ParseCompOp(SimpleLexer& lex, int tok, const Declarations& declarations)
	{
		int leftID = ParseUnary(lex, tok, declarations);
		if (leftID == -1)
			return -1;

		tok = lex.peek();
		if ((tok == '==') || (tok == '!='))
		{
			lex();

			int rightID = ParseUnary(lex, lex(), declarations);
			if (rightID == -1)
				return -1;

			switch (tok)
			{
			case '==':
				return AddOp(ExpressionOperator(ExpressionOperator::Equal, leftID, rightID));
			case '!=':
				return AddOp(ExpressionOperator(ExpressionOperator::NotEqual, leftID, rightID));
			}
		}

		return leftID;
	}

	int ParseUnary(SimpleLexer& lex, int tok, const Declarations& declarations)
	{
		if (tok == '!')
		{
			int opID = ParseValue(lex, lex(), declarations);
			if (opID != -1)
				return AddOp(ExpressionOperator(ExpressionOperator::Not, opID, -1));
			return -1;
		}

		return ParseValue(lex, tok, declarations);
	}

	int ParseValue(SimpleLexer& lex, int tok, const Declarations& declarations)
	{
		if (tok == '(')
		{
			int opID = ParseLogical(lex, lex(), declarations);
			if ((opID == -1) || (lex() != ')'))
				return -1;

			return opID;
		}
		else if (tok == 'true')
			return AddOp(ExpressionOperator(ExpressionOperator::Constant, true));
		else if (tok == 'fals')
			return AddOp(ExpressionOperator(ExpressionOperator::Constant, false));
		else if (tok == 'var')
		{
			if (VariableID variableID = GetVariableID(lex.ident()))
				return AddOp(ExpressionOperator(ExpressionOperator::Variable, variableID));
			else
			{
				gEnv->pLog->LogWarning("Unknown variable '%s' used in expression '%s'...", lex.ident(), lex.buf());
			}
		}

		return -1;
	}

	int Parse(const char* expression, const Declarations& declarations)
	{
		SimpleLexer lex = SimpleLexer(expression);
		return ParseLogical(lex, lex(), declarations);
	}

	bool EvaluateOp(const Collection& collection, const struct ExpressionOperator& op) const
	{
		switch (op.opType)
		{
		case ExpressionOperator::Variable:
			{
				bool value = false;
				collection.GetVariable(op.variableID, &value);

				return value;
			}
		case ExpressionOperator::Constant:
			return op.value;
		case ExpressionOperator::Or:
			return EvaluateOp(collection, m_expressionOps[op.operandLeft]) || EvaluateOp(collection, m_expressionOps[op.operandRight]);
		case ExpressionOperator::And:
			return EvaluateOp(collection, m_expressionOps[op.operandLeft]) && EvaluateOp(collection, m_expressionOps[op.operandRight]);
		case ExpressionOperator::Xor:
			return EvaluateOp(collection, m_expressionOps[op.operandLeft]) ^ EvaluateOp(collection, m_expressionOps[op.operandRight]);
		case ExpressionOperator::Not:
			return !EvaluateOp(collection, m_expressionOps[op.operandLeft]);
		case ExpressionOperator::Equal:
			return EvaluateOp(collection, m_expressionOps[op.operandLeft]) == EvaluateOp(collection, m_expressionOps[op.operandRight]);
		case ExpressionOperator::NotEqual:
			return EvaluateOp(collection, m_expressionOps[op.operandLeft]) != EvaluateOp(collection, m_expressionOps[op.operandRight]);
		}

		return false;
	}

	struct ExpressionOperator
	{
		enum Type
		{
			Not = 1,
			And,
			Or,
			Xor,
			Equal,
			NotEqual,
			Constant,
			Variable,
		};

		ExpressionOperator()
			: value(false)
		{
		}

		ExpressionOperator(int type, int left, int right)
			: value(false)
			, opType((Type)type)
			, operandLeft((uint8)left)
			, operandRight((uint8)right)
		{
		}

		ExpressionOperator(int type, uint32 varID)
			: variableID(varID)
			, value(false)
			, opType((Type)type)
		{
		}

		ExpressionOperator(int type, bool val)
			: value(val)
			, opType((Type)type)
		{
		}

		uint32 variableID;
		bool   value;

		uint8  opType;
		uint8  operandLeft;
		uint8  operandRight;
	};

	typedef std::vector<ExpressionOperator> ExpressionOps;
	ExpressionOps m_expressionOps;
	int           m_rootID;
};

DECLARE_SHARED_POINTERS(Expression)

/*
   A simple mechanism to flip variable values based on signal fire and conditions.
   A signal is mapped to an expression which will evaluate to the value of the variable,
   shall the signal be fired.
 */
struct SignalHandle
{
	Expression valueExpr;
	VariableID variableID;

#ifdef USING_VARIABLE_COLLECTION_SERIALIZATION
	void Serialize(Serialization::IArchive& archive)
	{
		Declarations* declarations = archive.context<Declarations>();
		if (!declarations)
			return;

		archive(signalName, "signalName", "^On event");

		Serialization::StringList variableNameList;
		variableNameList.push_back("");

		const VariableDescriptions& variableDescriptions = declarations->GetVariableDescriptions();
		for (VariableDescriptions::const_iterator it = variableDescriptions.begin(), end = variableDescriptions.end(); it != end; ++it)
			variableNameList.push_back(it->second.name);

		Serialization::StringListValue variableNameListValue(variableNameList, variableName);
		archive(variableNameListValue, "variableName", "^Switch variable");
		variableName = variableNameListValue.c_str();

		archive(value, "valueString", "^(True/False)");

		if (archive.isInput())
		{
			variableID = GetVariableID(variableName);
			valueExpr = Expression(value ? "true" : "false", *declarations);
		}

		if (signalName.empty())
			archive.error(signalName, "Signal must be specified");

		if (variableName.empty())
			archive.error(*this, "Variable must be selected");
	}

	bool operator<(const SignalHandle& rhs) const
	{
		return (signalName < rhs.signalName);
	}
#endif

#if defined(USING_VARIABLE_COLLECTION_SERIALIZATION) || defined(USING_VARIABLE_COLLECTION_XML_DESCRIPTION_CREATION)
	string signalName;
	string variableName;
	bool   value;
#endif
};

typedef std::multimap<uint32, SignalHandle> SignalHandles;

class SignalHandler
{
public:
	bool LoadFromXML(const Declarations& declarations, const XmlNodeRef& rootNode, const char* fileName)
	{
		const int childCount = rootNode->getChildCount();
		for (int i = 0; i < childCount; ++i)
		{
			XmlNodeRef childNode = rootNode->getChild(i);
			if (!stricmp(childNode->getTag(), "Signal"))
			{
				const char* signalName = 0;
				if (childNode->haveAttr("name"))
					childNode->getAttr("name", &signalName);
				else
				{
					gEnv->pLog->LogWarning("Missing 'name' attribute for tag '%s' in file '%s' at line %d.", childNode->getTag(), fileName, childNode->getLine());
					return false;
				}

				const char* variableName = 0;
				if (childNode->haveAttr("variable"))
					childNode->getAttr("variable", &variableName);
				else
				{
					gEnv->pLog->LogWarning("Missing 'variable' attribute for tag '%s' in file '%s' at line %d.", childNode->getTag(), fileName, childNode->getLine());
					return false;
				}

				const char* value = 0;
				if (childNode->haveAttr("value"))
					childNode->getAttr("value", &value);
				else
				{
					gEnv->pLog->LogWarning("Missing 'value' attribute for tag '%s' in file '%s' at line %d.", childNode->getTag(), fileName, childNode->getLine());
					return false;
				}

				VariableID variableID = GetVariableID(variableName);
				if (declarations.IsDeclared(variableID))
				{
					SignalHandles::iterator it = m_signalHandles.insert(SignalHandles::value_type(CCrc32::Compute(signalName), SignalHandle()));

					SignalHandle& signalHandle = it->second;

					signalHandle.valueExpr = Expression(value, declarations);
					signalHandle.variableID = variableID;

#if defined(USING_VARIABLE_COLLECTION_SERIALIZATION) || defined(USING_VARIABLE_COLLECTION_XML_DESCRIPTION_CREATION)
					signalHandle.signalName = signalName;
					signalHandle.variableName = variableName;
					signalHandle.value = signalHandle.valueExpr.Evaluate(Variables::Collection());
#endif

					if (!signalHandle.valueExpr.Valid())
					{
						gEnv->pLog->LogWarning("Failed to compile expression '%s' in file '%s' at line %d.", value, fileName, childNode->getLine());
						return false;
					}
				}
				else
				{
					gEnv->pLog->LogWarning("Unknown variable '%s' used for signal variable in file '%s' at line '%d'.", variableName, fileName, childNode->getLine());
					return false;
				}
			}
			else
			{
				gEnv->pLog->LogWarning("Unexpected tag '%s' in file '%s' at line %d. 'Signal' expected.", childNode->getTag(), fileName, childNode->getLine());
				return false;
			}
		}

		return true;
	}

	bool ProcessSignal(uint32 signalCRC, Collection& collection) const
	{
		SignalHandles::const_iterator it = m_signalHandles.find(signalCRC);
		if (it == m_signalHandles.end())
			return false;

		while ((it != m_signalHandles.end()) && (it->first == signalCRC))
		{
			const SignalHandle& signalHandle = it->second;
			collection.SetVariable(signalHandle.variableID, signalHandle.valueExpr.Evaluate(collection));
			++it;
		}

		return true;
	}

#ifdef USING_VARIABLE_COLLECTION_SERIALIZATION
	void Serialize(Serialization::IArchive& archive)
	{
		archive(m_signalHandles, "signalHandles", "^[<>]");
	}
#endif

#if defined(USING_VARIABLE_COLLECTION_XML_DESCRIPTION_CREATION)
	XmlNodeRef CreateXmlDescription()
	{
		if (m_signalHandles.size() == 0)
			return XmlNodeRef();

		XmlNodeRef signalVariablesXml = GetISystem()->CreateXmlNode("SignalVariables");
		for (SignalHandles::const_iterator it = m_signalHandles.begin(), end = m_signalHandles.end(); it != end; ++it)
		{
			const SignalHandle& signalHandle = it->second;

			XmlNodeRef variableXml = GetISystem()->CreateXmlNode("Signal");
			variableXml->setAttr("name", signalHandle.signalName);
			variableXml->setAttr("variable", signalHandle.variableName);
			variableXml->setAttr("value", signalHandle.value ? "true" : "false");

			signalVariablesXml->addChild(variableXml);
		}

		return signalVariablesXml;
	}
#endif

private:
	SignalHandles m_signalHandles;
};

#ifdef DEBUG_VARIABLE_COLLECTION
struct DebugHelper
{
	static void DumpToJSON(string* resultString, const Collection& collection, const Declarations& declarations)
	{
		resultString->reserve(512);
		resultString->clear();

		resultString->append("[");

		bool first = true;

		for (Collection::VariableMap::const_iterator it = collection.m_variableMap.begin(), end = collection.m_variableMap.end(); it != end; ++it)
		{
			const VariableID& variableID = it->first;
			bool value = it->second.value;
			const Description& description = declarations.GetVariableDescription(variableID);

			if (first)
			{
				first = false;
			}
			else
			{
				resultString->append(",");
			}

			resultString->append("\n{ \"variable\" : \"");
			resultString->append(description.name.c_str());
			resultString->append("\", \"value\" : ");
			resultString->append(value ? "true" : "false");
			resultString->append(" }");
		}
		resultString->append("\n]\n");
	}

	struct DebugVariable
	{
		const char* name;
		bool        value;

		bool operator<(const DebugVariable& other) const
		{
			return strcmp(name, other.name) < 0;
		}
	};

	static void DebugDraw(bool history, const Collection& collection, const Declarations& declarations)
	{
		float minY = 370.0f;
		float maxY = -FLT_MAX;

		float x = 900.0f;
		float y = minY;
		const float fontSize = 1.25f;
		const float lineHeight = 11.5f * fontSize;
		const float columnWidth = 145.0f;

		std::vector<DebugVariable> sorted;

		for (Collection::VariableMap::const_iterator it = collection.m_variableMap.begin(), end = collection.m_variableMap.end(); it != end; ++it)
		{
			const VariableID& variableID = it->first;
			bool value = it->second.value;

			const Description& description = declarations.GetVariableDescription(variableID);

			sorted.resize(sorted.size() + 1);
			sorted.back().name = description.name.c_str();
			sorted.back().value = value;
		}

		std::sort(sorted.begin(), sorted.end());

		ColorB trueColor(Col_SlateBlue, 1.0f);
		ColorB falseColor(Col_DarkGray, 1.0f);

		stack_string text;

		if (sorted.empty())
			return;

		gEnv->pAISystem->GetAIDebugRenderer()->Draw2dLabel(x, y, fontSize, Col_Yellow, false, "Variables");
		y += lineHeight;

		uint32 variableCount = sorted.size();
		for (uint32 i = 0; i < variableCount; ++i)
		{
			gEnv->pAISystem->GetAIDebugRenderer()->Draw2dLabel(x, y, fontSize, sorted[i].value ? trueColor : falseColor, false, "%s", sorted[i].name);
			y += lineHeight;

			if (y > maxY)
				maxY = y;

			if (y + lineHeight > 760.0f)
			{
				y = minY;
				x += columnWidth;
			}
		}

		if (history)
		{
			y = minY + lineHeight;
			CTimeValue now = gEnv->pTimer->GetFrameStartTime();

			Collection::History::const_iterator it = collection.m_history.begin();
			Collection::History::const_iterator end = collection.m_history.end();

			for (; it != end; ++it)
			{
				const Collection::VariableChangeEvent& changeEvent = *it;
				float alpha = 1.0f - (now - changeEvent.when).GetSeconds() / 10.0f;
				if (alpha > 0.01f)
				{
					alpha = clamp_tpl(alpha, 0.33f, 1.0f);
					const Description& description = declarations.GetVariableDescription(changeEvent.variableID);

					trueColor.a = (uint8)(alpha * 255.5f);
					falseColor.a = (uint8)(alpha * 255.5f);

					text = description.name;
					gEnv->pAISystem->GetAIDebugRenderer()->Draw2dLabel(x + columnWidth + 2.0f, y, fontSize, changeEvent.value ? trueColor : falseColor, false, "%s", text.c_str());

					y += lineHeight;
				}
			}
		}
	}

	static void CollectDebugVariables(const Collection& collection, const Declarations& declarations, DynArray<DebugVariable> outVariables)
	{
		outVariables.clear();

		for (Collection::VariableMap::const_iterator it = collection.m_variableMap.begin(), end = collection.m_variableMap.end(); it != end; ++it)
		{
			const VariableID& variableID = it->first;
			bool value = it->second.value;

			const Description& description = declarations.GetVariableDescription(variableID);

			DebugVariable variable;
			variable.name = description.name.c_str();
			variable.value = value;
			outVariables.push_back(variable);
		}

		std::sort(outVariables.begin(), outVariables.end());
	}
};
#endif // DEBUG_VARIABLE_COLLECTION
}

#if defined(USING_VARIABLE_COLLECTION_SERIALIZATION)

namespace std
{
inline bool Serialize(Serialization::IArchive& archive, std::pair<Variables::VariableID, Variables::Description>& pair, const char* name, const char* label)
{
	if (!archive(pair.second, name, label))
		return false;

	pair.first = Variables::GetVariableID(pair.second.name);
	return true;
}

inline bool Serialize(Serialization::IArchive& archive, std::pair<uint32, Variables::SignalHandle>& pair, const char* name, const char* label)
{
	if (!archive(pair.second, name, label))
		return false;

	pair.first = CCrc32::Compute(pair.second.signalName);
	return true;
}
}

namespace Variables
{
template<class T>
bool less_than_second(const T& a, const T& b)
{
	return (a.second < b.second);
};

inline bool Serialize(Serialization::IArchive& archive, VariableDescriptions& variableDescriptions, const char* name, const char* label)
{
	typedef std::pair<Variables::VariableID, Variables::Description> VariableIdDescriptionPair;
	typedef std::vector<VariableIdDescriptionPair>                   DescriptorVector;
	DescriptorVector tempDescriptionsVector;

	if (archive.isOutput())
	{
		tempDescriptionsVector.assign(variableDescriptions.begin(), variableDescriptions.end());
		std::sort(tempDescriptionsVector.begin(), tempDescriptionsVector.end(), less_than_second<VariableIdDescriptionPair> );
	}

	if (!archive(tempDescriptionsVector, name, label))
		return false;

	if (archive.isInput())
	{
		variableDescriptions.clear();
		variableDescriptions.insert(tempDescriptionsVector.begin(), tempDescriptionsVector.end());
	}

	return true;
}

inline bool Serialize(Serialization::IArchive& archive, SignalHandles& signalHandles, const char* name, const char* label)
{
	typedef std::pair<uint32, Variables::SignalHandle> KeySignalHandlePair;
	typedef std::vector<KeySignalHandlePair>           SignalHandleVector;
	SignalHandleVector tempSignalHandleVector;

	if (archive.isOutput())
	{
		tempSignalHandleVector.assign(signalHandles.begin(), signalHandles.end());
		std::sort(tempSignalHandleVector.begin(), tempSignalHandleVector.end(), less_than_second<KeySignalHandlePair> );
	}

	archive(tempSignalHandleVector, name, label);

	if (archive.isInput())
	{
		signalHandles.clear();
		signalHandles.insert(tempSignalHandleVector.begin(), tempSignalHandleVector.end());
	}

	return true;
}
}

#endif

#endif // VariableCollection_h
