// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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
#include <CryAISystem/BehaviorTree/SerializationSupport.h>
#include <CryString/StringUtils.h>
#include <CryAISystem/ISignal.h>
#include <CrySerialization/SerializationUtils.h>

#if defined(USING_VARIABLE_COLLECTION_SERIALIZATION)
	#include <CrySerialization/IArchive.h>
	#include <CrySerialization/STL.h>
	#include <CrySerialization/StringList.h>
#endif

namespace
{
	template<typename T>
	void ConvertMapToVectorOfValues(const T& map, std::vector<typename T::mapped_type>& vec)
	{
		std::transform(map.begin(), map.end(),
			std::back_inserter(vec),
			[](const std::pair<typename T::key_type, typename T::mapped_type> &p) {
			return p.second;
		});
	}
}

namespace Variables
{
typedef uint32 VariableID;

template <typename T>
std::vector<size_t> GetIndicesOfDuplicatedEntries(const T& container)
{
	std::vector<size_t> duplicatesEntries;

	for (size_t i = 0; i < container.size(); ++i)
	{
		for (size_t j = i + 1; j < container.size(); ++j)
		{
			if (container[i] == container[j])
			{
				duplicatesEntries.push_back(j);
			}
		}
	}
	return duplicatesEntries;
}

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
	Description() {}

	Description(const char* _name)
		: name(_name)
	{
	}

#if defined(USING_VARIABLE_COLLECTION_SERIALIZATION)
	void Serialize(Serialization::IArchive& archive)
	{
		archive(name, "name", "^<Name");
		archive.doc("Variable name");

		if (name.empty())
		{
			archive.error(name, SerializationUtils::Messages::ErrorEmptyValue("Name"));
		}
			
	}

	bool operator<(const Description& rhs) const
	{
		return (name < rhs.name);
	}

	bool operator==(const Description& rhs) const
	{
		return (name == rhs.name);
	}

	const string& SerializeToString() const
	{
		return name;
	}
#endif

	string name;
};

typedef std::unordered_map<VariableID, Description> VariableDescriptions;
typedef std::vector<Description>          DescriptionVector;

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
					gEnv->pLog->LogWarning("(%d) [File='%s'] Missing 'name' attribute for tag '%s'", childNode->getLine(), fileName, childNode->getTag());
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
						gEnv->pLog->LogWarning("(%d) [File='%s'] Invalid variable value '%s' for variable '%s'", childNode->getLine(), fileName, value, variableName);
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
						gEnv->pLog->LogWarning("(%d) [File='%s'] Duplicate variable declaration '%s'", childNode->getLine(), fileName, variableName);
					}
					else
					{
						gEnv->pLog->LogWarning("(%d) [File='%s'] Hash collision for variable declaration '%s'", childNode->getLine(), fileName, variableName);
					}

					return false;
				}

				m_defaults.SetVariable(variableID, defaultValue);
				m_defaults.ResetChanged(true);
			}
			else
			{
				gEnv->pLog->LogWarning("(%d) [File='%s'] Unexpected tag '%s'", childNode->getLine(), fileName, childNode->getTag());
				return false;
			}
		}

#if defined(USING_VARIABLE_COLLECTION_SERIALIZATION)
		ConvertMapToVectorOfValues(m_descriptions, m_descriptionVector);
#endif//USING_VARIABLE_COLLECTION_SERIALIZATION

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

#if defined(USING_VARIABLE_COLLECTION_SERIALIZATION)
	const DescriptionVector& GetVariableDescriptionVector() const
	{
		return m_descriptionVector;
	}
#endif

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
		archive(m_descriptionVector, "variableDescriptions", "^[<>]");

		if (archive.isInput() || archive.isEdit())
		{
			m_descriptions.clear();

			for (const Description& desc : m_descriptionVector)
			{
				m_descriptions.insert(VariableDescriptions::value_type(GetVariableID(desc.name), desc));
			}
		}

		const std::vector<size_t> duplicatedIndices = GetIndicesOfDuplicatedEntries(m_descriptionVector);
		for (const size_t i : duplicatedIndices)
		{
			archive.error(m_descriptionVector[i].name, SerializationUtils::Messages::ErrorDuplicatedValue("Variable name", m_descriptionVector[i].name));
		}
	}

#endif

private:
	VariableDescriptions m_descriptions;
#if defined(USING_VARIABLE_COLLECTION_SERIALIZATION)
	DescriptionVector    m_descriptionVector;
#endif
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

#ifdef USING_VARIABLE_COLLECTION_SERIALIZATION
	bool operator==(const Expression& rhs) const
	{
		return m_rootID == rhs.m_rootID && m_expressionOps == rhs.m_expressionOps;
	}
#endif

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
			const VariableID variableID = GetVariableID(lex.ident());
			if (variableID && declarations.IsDeclared(variableID))
			{
				return AddOp(ExpressionOperator(ExpressionOperator::Variable, variableID));
			}
			else
			{
				gEnv->pLog->LogError("Unknown variable '%s' used in expression '%s'...", lex.ident(), lex.buf());
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

#ifdef USING_VARIABLE_COLLECTION_SERIALIZATION
		bool operator==(const ExpressionOperator& rhs) const
		{
			return variableID == rhs.variableID &&
			       value == rhs.value &&
			       opType == rhs.opType &&
			       operandLeft == rhs.operandLeft &&
			       operandRight == rhs.operandRight;
		}
#endif

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

struct Event;
typedef std::vector<Event> Events;

struct Event
{
	Event()
	{
	};

	Event(const char* _name)
		: name(_name)
	{
	}

#if defined(USING_VARIABLE_COLLECTION_SERIALIZATION)
	void Serialize(Serialization::IArchive& archive)
	{
		archive(name, "name", "^<Name");
		archive.doc("Event name");

		if (name.empty())
		{
			archive.error(name, SerializationUtils::Messages::ErrorEmptyValue("Name"));
		}
	}

	const string& SerializeToString() const
	{
		return name;
	}

	bool operator<(const Event& rhs) const
	{
		return (name < rhs.name);
	}

	bool operator==(const Event& rhs) const
	{
		return (name == rhs.name);
	}
#endif

	string name;
};

class EventsDeclaration
{
public:
	EventsDeclaration()
	{
		LoadBuiltInEvents();
	}

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	// Has to be virtual so that allocation and deallocation of memory happens in the same module
	// Otherwise crashes since allocation happens in the IA module and deallocation in Sandbox
	// Using a private implementation would also solve the issue but CryCore does not have any .cpp files
	//! Declares event as a Game Event. Does not watch out for duplicates. Call IsDeclared before to check if game event already exists
	//! \param newEventName Name of the new Game Event to declare
	virtual void DeclareGameEvent(const char* newEventName)
	{
		const Event tempEvent = Event(newEventName);
		m_eventsGame.insert(
			std::upper_bound(
				m_eventsGame.begin(),
				m_eventsGame.end(),
				tempEvent),
			tempEvent
		);
	}
#endif //USING_BEHAVIOR_TREE_SERIALIZATION

	bool LoadFromXML(const XmlNodeRef& rootNode, const char* fileName)
	{
		m_eventsGame.clear();

		if (XmlNodeRef eventsTagNode = rootNode->findChild("GameEvents"))
		{
			const int count = eventsTagNode->getChildCount();

			for (int i = 0; i < count; ++i)
			{
				XmlNodeRef childNode = eventsTagNode->getChild(i);
				if (!stricmp(childNode->getTag(), "Event"))
				{
					const char* eventName = 0;
					if (childNode->haveAttr("name"))
					{
						childNode->getAttr("name", &eventName);
					}
					else
					{
						gEnv->pLog->LogWarning("(%d) [File='%s'] Missing 'name' attribute for tag '%s'", childNode->getLine(), fileName, childNode->getTag());
						return false;
					}

					m_eventsGame.emplace_back(eventName);
				}
				else
				{
					gEnv->pLog->LogWarning("(%d) [File='%s'] Unexpected tag '%s'. Tag 'Event' expected.", childNode->getLine(), fileName, childNode->getTag());
					return false;
				}
			}
		}

		return true;
	}

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	const Events GetEventsWithFlags() const
	{
		Events events;

		if (m_eventsFlags.test(BehaviorTree::NodeSerializationHints::EEventsFlags_CryEngine))
		{
			events.reserve(m_eventsCryEngine.size());
			events.insert(events.end(), m_eventsCryEngine.begin(), m_eventsCryEngine.end());
		}

		if (m_eventsFlags.test(BehaviorTree::NodeSerializationHints::EEventsFlags_GameSDK))
		{
			events.reserve(events.size() + m_eventsGameSDK.size());
			events.insert(events.end(), m_eventsGameSDK.begin(), m_eventsGameSDK.end());
		}

		if (m_eventsFlags.test(BehaviorTree::NodeSerializationHints::EEventsFlags_Deprecated))
		{
			events.reserve(events.size() + m_eventsDeprecated.size());
			events.insert(events.end(), m_eventsDeprecated.begin(), m_eventsDeprecated.end());
		}

		events.reserve(events.size() + m_eventsGame.size());
		events.insert(events.end(), m_eventsGame.begin(), m_eventsGame.end());

		return events;
	}
#endif //USING_BEHAVIOR_TREE_SERIALIZATION

	const Events& GetGameEvents() const
	{
		return m_eventsGame;
	}

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	//! Checks if the given eventName already exists in the list of events (CryEngine, GameSDK, Deprecated and Game)
	//! \param eventName Name of the Event to check 
	//! \param isLoadingFromEditor If True looks for the eventName in those lists for which the flag is enabled. Otherwise ignores the flags and searches for the event in all lists
	//! \return True if the event is declared. False otherwise.
	bool IsDeclared(const char* eventName, const bool isLoadingFromEditor = true) const
	{
		const Event tempEvent = Event(eventName);

		bool isDeclared = std::find(m_eventsGame.begin(), m_eventsGame.end(), tempEvent) != m_eventsGame.end();

		if (!isDeclared && (!isLoadingFromEditor || m_eventsFlags.test(BehaviorTree::NodeSerializationHints::EEventsFlags_CryEngine)))
		{
			isDeclared = std::find(m_eventsCryEngine.begin(), m_eventsCryEngine.end(), tempEvent) != m_eventsCryEngine.end();
		}

		if (!isDeclared && (!isLoadingFromEditor || m_eventsFlags.test(BehaviorTree::NodeSerializationHints::EEventsFlags_GameSDK)))
		{
			isDeclared = std::find(m_eventsGameSDK.begin(), m_eventsGameSDK.end(), tempEvent) != m_eventsGameSDK.end();
		}

		if (!isDeclared && (!isLoadingFromEditor || m_eventsFlags.test(BehaviorTree::NodeSerializationHints::EEventsFlags_Deprecated)))
		{
			isDeclared = std::find(m_eventsDeprecated.begin(), m_eventsDeprecated.end(), tempEvent) != m_eventsDeprecated.end();
		}

		return isDeclared;
	}
#endif // USING_BEHAVIOR_TREE_SERIALIZATION

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	void Serialize(Serialization::IArchive& archive)
	{
		const BehaviorTree::NodeSerializationHints* pHintsContext = archive.context<BehaviorTree::NodeSerializationHints>();

		IF_LIKELY(pHintsContext)
		{
			SetEventsFlags(pHintsContext->eventsFlags);
		}

		if (m_eventsFlags.test(BehaviorTree::NodeSerializationHints::EEventsFlags_CryEngine))
		{
			SerializeEventsList(archive, "eventsCryEngine", "!CryEngine Events", m_eventsCryEngine);
		}

		if (m_eventsFlags.test(BehaviorTree::NodeSerializationHints::EEventsFlags_GameSDK))
		{
			SerializeEventsList(archive, "eventsGameSDK", "!GameSDK Events", m_eventsGameSDK);
		}

		if (m_eventsFlags.test(BehaviorTree::NodeSerializationHints::EEventsFlags_Deprecated))
		{
			SerializeEventsList<Events>(archive, "eventsDeprecated", "!Deprecated Events", m_eventsDeprecated);
		}

		SerializeEventsList(archive, "gameEvents", "+Game Events", m_eventsGame);

		LookForEventGamesDuplicates(archive, m_eventsCryEngine, "CryEngine");
		LookForEventGamesDuplicates(archive, m_eventsGameSDK, "GameSDK");
		LookForEventGamesDuplicates(archive, m_eventsDeprecated, "Deprecated");

	}

	void LookForEventGamesDuplicates(Serialization::IArchive& archive, const Events& events, const char* eventsCategory)
	{
		Events::const_iterator itEvents = events.begin();
		Events::const_iterator itEventsGame = m_eventsGame.begin();

		bool eventsCryEngineDone = itEvents == events.end();
		bool eventsGameDone = itEventsGame == m_eventsGame.end();

		while (!eventsCryEngineDone && !eventsGameDone)
		{
			if (!eventsCryEngineDone && !eventsGameDone)
			{
				const Event& eventCryEngine = *itEvents;
				const Event& eventGame = *itEventsGame;

				if (eventCryEngine < eventGame)
				{
					++itEvents;
					eventsCryEngineDone = itEvents == events.end();
				}
				else if (eventGame < eventCryEngine)
				{
					++itEventsGame;
					eventsGameDone = itEventsGame == m_eventsGame.end();
				}
				else
				{
					stack_string errorMessage;
					errorMessage.Format("Event with name '%s' already exists as a %s event. Consider renaming the event or removing it and using the built-in event with the same name by enabling %s events under the section Built-in events in the View menu.", itEventsGame->name, eventsCategory, eventsCategory);
					archive.error(itEventsGame->name, SerializationUtils::Messages::ErrorInvalidValueWithReason("Name", itEventsGame->name, errorMessage));

					++itEvents;
					++itEventsGame;
					eventsCryEngineDone = itEvents == events.end();
					eventsGameDone = itEventsGame == m_eventsGame.end();
				}
			}
		}
	}


#endif

#if defined(USING_VARIABLE_COLLECTION_XML_DESCRIPTION_CREATION)
	XmlNodeRef CreateXmlDescription()
	{
		if (m_eventsGame.size() == 0)
		{
			return XmlNodeRef();
		}

		XmlNodeRef eventsXml = GetISystem()->CreateXmlNode("Events");
		XmlNodeRef gameEventsXml = GetISystem()->CreateXmlNode("GameEvents");

		// Only write game defined events. Built in events are stored in CE code
		for (Events::const_iterator it = m_eventsGame.begin(), end = m_eventsGame.end(); it != end; ++it)
		{
			XmlNodeRef variableXml = GetISystem()->CreateXmlNode("Event");
			variableXml->setAttr("name", it->name);

			gameEventsXml->addChild(variableXml);
		}
		eventsXml->addChild(gameEventsXml);

		return eventsXml;
	}
#endif

#if defined(USING_BEHAVIOR_TREE_SERIALIZATION)
	const BehaviorTree::NodeSerializationHints::EventsFlags& GetEventsFlags() const
	{
		return m_eventsFlags;
	}

	void SetEventsFlags(const BehaviorTree::NodeSerializationHints::EventsFlags& eventsFlags)
	{
		m_eventsFlags = eventsFlags;
	}
#endif // USING_BEHAVIOR_TREE_SERIALIZATION

private:
	void LoadBuiltInEvents()
	{
		const size_t signalDescriptionsCount = gEnv->pAISystem->GetSignalManager()->GetSignalDescriptionsCount();
		for (size_t i = 0; i < signalDescriptionsCount; ++i)
		{
			const AISignals::ISignalDescription& signalDesc = gEnv->pAISystem->GetSignalManager()->GetSignalDescription(i);

			switch (signalDesc.GetTag())
			{
			case AISignals::ESignalTag::CRY_ENGINE:
				m_eventsCryEngine.emplace_back(signalDesc.GetName());
				break;
			case AISignals::ESignalTag::GAME_SDK:
				m_eventsGameSDK.emplace_back(signalDesc.GetName());
				break;
			case AISignals::ESignalTag::DEPRECATED:
				m_eventsDeprecated.emplace_back(signalDesc.GetName());
				break;
			case AISignals::ESignalTag::GAME:
			case AISignals::ESignalTag::NONE:
				// Nothing
				break;
			default:
				CRY_ASSERT_MESSAGE(false, "Missing case label for Signal Tag");
			}
		}
	}

#if defined (USING_BEHAVIOR_TREE_SERIALIZATION) || defined(USING_VARIABLE_COLLECTION_SERIALIZATION)
	template <class T>
	void SerializeEventsList(Serialization::IArchive& archive, const char* szKey, const char* szLabel, T& events)
	{
		archive(events, szKey, szLabel);

		const std::vector<size_t> duplicatedIndices = GetIndicesOfDuplicatedEntries(events);
		for (const size_t i : duplicatedIndices)
		{
			archive.error(events[i].name, SerializationUtils::Messages::ErrorDuplicatedValue("Event name", events[i].name));
		}
	}
#endif
private:
	Events m_eventsCryEngine;
	Events m_eventsGameSDK;
	Events m_eventsDeprecated;
	Events m_eventsGame;

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	BehaviorTree::NodeSerializationHints::EventsFlags m_eventsFlags;
#endif //USING_BEHAVIOR_TREE_SERIALIZATION
};

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
		// Serialize Events

		const EventsDeclaration* eventsDeclaration = archive.context<EventsDeclaration>();
		IF_LIKELY (!eventsDeclaration)
			return;

		BehaviorTree::SerializeContainerAsSortedStringList(archive, "signalName", "^On event", eventsDeclaration->GetEventsWithFlags(), "Event", signalName);
		archive.doc("Event that triggers the change in the value of the Variable");

		// Serialize Variables

		const Declarations* pDeclarations = archive.context<Declarations>();
		if (!pDeclarations)
			return;

		BehaviorTree::SerializeContainerAsSortedStringList(
			archive, 
			"variableName", 
			"^Switch variable", 
			pDeclarations->GetVariableDescriptionVector(),
			"Switch variable", 
			variableName
		);
		archive.doc("Variable that gets changed to the specified value when the Event is triggered");

		// Serialize bool switch
		archive(value, "valueString", "^Value(True/False)");
		archive.doc("Value of the variable after the Event has been triggered");

		if (archive.isInput())
		{
			variableID = GetVariableID(variableName);
			valueExpr = Expression(value ? "true" : "false", *pDeclarations);
		}
	}

	bool operator<(const SignalHandle& rhs) const
	{
		return (signalName < rhs.signalName);
	}

	inline bool operator==(const SignalHandle& rhs) const
	{
		return signalName == rhs.signalName && variableID == rhs.variableID && valueExpr == rhs.valueExpr;
	}
#endif

#if defined(USING_VARIABLE_COLLECTION_SERIALIZATION) || defined(USING_VARIABLE_COLLECTION_XML_DESCRIPTION_CREATION)
	string signalName;
	string variableName;
	bool   value;
#endif
};

typedef std::multimap<uint32, SignalHandle>        SignalHandles;
typedef std::vector<SignalHandle>                  SignalHandleVector;

class SignalHandler
{
public:
	bool LoadFromXML(const Declarations& variableDeclaration, EventsDeclaration& eventsDeclaration, const XmlNodeRef& rootNode, const char* fileName, const bool isLoadingFromEditor)
	{
		const int childCount = rootNode->getChildCount();
		for (int i = 0; i < childCount; ++i)
		{
			XmlNodeRef childNode = rootNode->getChild(i);
			if (!stricmp(childNode->getTag(), "Signal"))
			{
				// Event
				const char* signalName = 0;
				if (childNode->haveAttr("name"))
				{
					childNode->getAttr("name", &signalName);
				}
				else
				{
					gEnv->pLog->LogWarning("(%d) [File='%s'] Missing 'name' attribute for tag '%s'", childNode->getLine(), fileName, childNode->getTag());
					return false;
				}

#if defined(USING_BEHAVIOR_TREE_SERIALIZATION)
				// Automatically declare game signals
				if (!eventsDeclaration.IsDeclared(signalName, isLoadingFromEditor))
				{
					if (isLoadingFromEditor)
					{
						eventsDeclaration.DeclareGameEvent(signalName);
						gEnv->pLog->LogWarning("(%d) [File='%s'] Unknown event '%s' used in Event Handle. Event will be declared automatically", childNode->getLine(), fileName, signalName);
					}
					else
					{
						gEnv->pLog->LogError("(%d) [File='%s'] Unknown event '%s' used in Event Handle.", childNode->getLine(), fileName, signalName);
						return false;
					}
				}
#endif // USING_BEHAVIOR_TREE_SERIALIZATION

				// Bool value
				const char* value = 0;
				if (childNode->haveAttr("value"))
					childNode->getAttr("value", &value);
				else
				{
					gEnv->pLog->LogWarning("(%d) [File='%s'] Missing 'value' attribute for tag '%s'", childNode->getLine(), fileName, childNode->getTag());
					return false;
				}

				// Variable
				const char* variableName = 0;
				if (childNode->haveAttr("variable"))
				{
					childNode->getAttr("variable", &variableName);
				}
				else
				{
					gEnv->pLog->LogWarning("(%d) [File='%s'] Missing 'variable' attribute for tag '%s'", childNode->getLine(), fileName, childNode->getTag());
					return false;
				}

				VariableID variableID = GetVariableID(variableName);
				SignalHandles::iterator it = m_signalHandles.insert(SignalHandles::value_type(CCrc32::Compute(signalName), SignalHandle()));

				SignalHandle& signalHandle = it->second;

				signalHandle.valueExpr = Expression(value, variableDeclaration);
				signalHandle.variableID = variableID;

				if (variableDeclaration.IsDeclared(variableID))
				{
#if defined(USING_VARIABLE_COLLECTION_SERIALIZATION) || defined(USING_VARIABLE_COLLECTION_XML_DESCRIPTION_CREATION)
					signalHandle.signalName = signalName;
					signalHandle.variableName = variableName;
					signalHandle.value = signalHandle.valueExpr.Evaluate(Variables::Collection());
#endif
					if (!signalHandle.valueExpr.Valid())
					{
						gEnv->pLog->LogWarning("(%d) [File='%s'] Failed to compile expression '%s'", childNode->getLine(), fileName, value);
					}
				}
				else
				{
					gEnv->pLog->LogWarning("(%d) [File='%s'] Unknown variable '%s' used for Event Handle", childNode->getLine(), fileName, variableName);
				}

			}
			else
			{
				gEnv->pLog->LogWarning("(%d) [File='%s'] Unexpected tag '%s'. Tag 'Signal' expected.", childNode->getLine(), fileName, childNode->getTag());
				return false;
			}
		}

#if defined(USING_VARIABLE_COLLECTION_SERIALIZATION)
		ConvertMapToVectorOfValues(m_signalHandles, m_signalHandleVector);
#endif//USING_VARIABLE_COLLECTION_SERIALIZATION

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
		archive(m_signalHandleVector, "signalHandles", "^[<>]");

		if (archive.isInput() || archive.isEdit())
		{
			m_signalHandles.clear();

			for (const SignalHandle& signalHandle : m_signalHandleVector)
			{
				m_signalHandles.insert(SignalHandles::value_type(CCrc32::Compute(signalHandle.signalName), signalHandle));
			}
		}

		const std::vector<size_t> duplicatedIndices = GetIndicesOfDuplicatedEntries(m_signalHandleVector);
		for (const size_t i : duplicatedIndices)
		{
			archive.error(m_signalHandleVector[i].signalName, SerializationUtils::Messages::ErrorDuplicatedValue("Event handle", m_signalHandleVector[i].signalName));
		}
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

	const SignalHandleVector& GetSignalHandleVector() const
	{
		return m_signalHandleVector;
	}

#endif

	const SignalHandles& GetSignalHandles() const
	{
		return m_signalHandles;
	}

private:
	SignalHandles       m_signalHandles;
#if defined(USING_VARIABLE_COLLECTION_SERIALIZATION)
	SignalHandleVector  m_signalHandleVector;
#endif
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

#endif
