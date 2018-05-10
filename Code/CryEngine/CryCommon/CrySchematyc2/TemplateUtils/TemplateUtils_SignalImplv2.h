// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef INCLUDING_FROM_TEMPLATE_UTILS_SIGNAL_HEADER
#error This file should only be included from TemplateUtils_Signalv2.h!
#endif //INCLUDING_FROM_TEMPLATE_UTILS_SIGNAL_HEADER

#define PARAM_SEPARATOR                                       PP_COMMA_IF(PARAM_COUNT)
#define TEMPLATE_PARAMS                                       typename RETURN PARAM_SEPARATOR PP_ENUM_PARAMS(PARAM_COUNT, typename PARAM)
#define SIGNATURE                                             RETURN ( PP_ENUM_PARAMS(PARAM_COUNT, PARAM) )
#define INPUT_PARAMS_ELEMENT(i, user_data)                    PARAM##i param##i
#define INPUT_PARAMS                                          PP_COMMA_ENUM(PARAM_COUNT, INPUT_PARAMS_ELEMENT, PP_EMPTY)
#define OUTPUT_PARAMS                                         PP_ENUM_PARAMS(PARAM_COUNT, param)
#define CAPTURE_PARAMS                                        PP_ENUM_PARAMS(PARAM_COUNT, &param)
#define PARAMS_ELEMENT_INPUT_ELEMENT(i, user_data)            PARAM##i _param##i
#define PARAMS_ELEMENT_INPUT                                  PP_COMMA_ENUM(PARAM_COUNT, PARAMS_ELEMENT_INPUT_ELEMENT, PP_EMPTY)
#define PARAMS_ELEMENT_CONSTRUCT_PARAMS_ELEMENT(i, user_data) param##i(_param##i)
#define PARAMS_ELEMENT_CONSTRUCT_PARAMS                       PP_COMMA_ENUM(PARAM_COUNT, PARAMS_ELEMENT_CONSTRUCT_PARAMS_ELEMENT, PP_EMPTY)
#define PARAMS_ELEMENT_CONSTRUCT                              PP_COLON_IF(PARAM_COUNT) PARAMS_ELEMENT_CONSTRUCT_PARAMS
#define PARAMS_ELEMENT_OUTPUT                                 PP_ENUM_PARAMS(PARAM_COUNT, param)
#define PARAMS_ELEMENT_MEMBERS_ELEMENT(i, user_data)          PARAM##i param##i;
#define PARAMS_ELEMENT_MEMBERS                                PP_ENUM(PARAM_COUNT, PARAMS_ELEMENT_MEMBERS_ELEMENT, PP_EMPTY, PP_EMPTY)

namespace TemplateUtils
{
	/*template <TEMPLATE_PARAMS, typename VALUE> class CSignalQueuev2<SIGNATURE, void, VALUE>
	{
	private:

		typedef VALUE             Element;
		typedef DynArray<Element> Elements;

	public:

		typedef void                              Key;
		typedef VALUE                             Value;
		typedef typename Elements::const_iterator Iterator;

	public:

		inline void Reserve(size_t size)
		{
			m_elements.reserve(size);
		}

		inline void PushBack(INPUT_PARAMS)
		{
			m_elements.push_back(OUTPUT_PARAMS);
		}

		inline void Clear()
		{
			m_elements.clear();
		}

		inline Iterator begin() const
		{
			return m_elements.begin();
		}

		inline Iterator end() const
		{
			return m_elements.end();
		}

	private:

		Elements m_elements;
	};*/

	/*template <TEMPLATE_PARAMS, typename KEY, typename VALUE> class CSignalQueuev2<SIGNATURE, KEY, VALUE>
	{
	private:

		typedef std::pair<KEY, VALUE> Element;
		typedef DynArray<Element>     Elements;

	public:

		typedef KEY                               Key;
		typedef VALUE                             Value;
		typedef typename Elements::const_iterator Iterator;

		class CSelectionScope
		{
		public:

			inline CSelectionScope(CSignalQueue& signalQueue, const Key& key)
				: m_signalQueue(signalQueue)
				, m_key(key)
			{}

			inline void PushBack(INPUT_PARAMS) const
			{
				return m_signalQueue.PushBack(m_key PARAM_SEPARATOR OUTPUT_PARAMS);
			}

		private:

			CSignalQueue& m_signalQueue;
			const Key&    m_key;
		};

	public:
		
		inline void Reserve(size_t size)
		{
			m_elements.reserve(size);
		}

		inline const CSelectionScope Select(const Key& key)
		{
			return CSelectionScope(*this, key);
		}

		inline void Clear()
		{
			m_elements.clear();
		}

		inline Iterator begin() const
		{
			return m_elements.begin();
		}

		inline Iterator end() const
		{
			return m_elements.end();
		}

	private:

		inline void PushBack(const Key& key PARAM_SEPARATOR INPUT_PARAMS)
		{
			m_elements.push_back(Element(key PARAM_SEPARATOR OUTPUT_PARAMS));
		}

	private:

		Elements m_elements;
	};*/

	template <TEMPLATE_PARAMS> class CSignalv2<SIGNATURE>
	{
	public:

		typedef void                 Key;
		typedef void                 KeyCompare;
		typedef CDelegate<SIGNATURE> Delegate;

	public:

		inline void Reserve(size_t size)
		{
			return m_connections.Reserve(size);
		}

		inline void Connect(const Delegate& delegate, CConnectionScope& scope)
		{
			m_connections.Add(delegate, scope);
		}

		/*inline void Disconnect(CConnectionScope& scope)
		{
			m_connections.Remove(scope);
		}*/

		inline void Send(INPUT_PARAMS)
		{
			auto delegate = [CAPTURE_PARAMS] (const Delegate& delegate)
			{
				delegate(OUTPUT_PARAMS);
			};
			m_connections.Process(delegate);
		}

		//template <typename ELEMENT> inline void ProcessQueue(CSignalQueuev2<SIGNATURE, void, ELEMENT>& queue) {}

		inline void Cleanup()
		{
			m_connections.Cleanup();
		}

	private:

		CScopedConnectionManager<Delegate> m_connections;
	};

	template <TEMPLATE_PARAMS, typename KEY, class KEY_COMPARE> class CSignalv2<SIGNATURE, KEY, KEY_COMPARE>
	{
	public:

		typedef KEY                  Key;
		typedef KEY_COMPARE          KeyCompare;
		typedef CDelegate<SIGNATURE> Delegate;

		class CSelectionScope
		{
		public:

			inline CSelectionScope(CSignalv2& signal, const Key& key)
				: m_signal(signal)
				, m_key(key)
			{}

			inline void Connect(const Delegate& delegate, CConnectionScope& scope) const
			{
				m_signal.Connect(m_key, delegate, scope);
			}

			inline void Send(INPUT_PARAMS) const
			{
				m_signal.Send(m_key PARAM_SEPARATOR OUTPUT_PARAMS);
			}

		private:

			CSignalv2& m_signal;
			const Key& m_key;
		};

		inline CSignalv2(const KeyCompare& keyCompare = KeyCompare())
			: m_connections(keyCompare)
		{}

		inline const CSelectionScope Select(const Key& key)
		{
			return CSelectionScope(*this, key);
		}

		inline void Reserve(size_t size)
		{
			return m_connections.Reserve(size);
		}

		/*inline void Disconnect(CConnectionScope& scope)
		{
			m_connections.Remove(scope);
		}*/

		inline void Send(INPUT_PARAMS)
		{
			auto delegate = [CAPTURE_PARAMS] (const Delegate& delegate)
			{
				delegate(OUTPUT_PARAMS);
			};
			m_connections.Process(delegate);
		}

		//template <typename ELEMENT> inline void ProcessQueue(CSignalQueue<SIGNATURE, void, ELEMENT>& queue) {}

		//template <typename ELEMENT> inline void ProcessQueue(CSignalQueue<SIGNATURE, Key, ELEMENT>& queue) {}

		inline void Cleanup()
		{
			m_connections.Cleanup();
		}

	private:

		inline void Connect(const Key& key, const Delegate& delegate, CConnectionScope& scope)
		{
			m_connections.Add(key, delegate, scope);
		}

		inline void Send(const Key& key PARAM_SEPARATOR INPUT_PARAMS)
		{
			auto delegate = [CAPTURE_PARAMS] (const Delegate& delegate)
			{
				delegate(OUTPUT_PARAMS);
			};
			m_connections.Process(key, delegate);
		}

		CScopedConnectionManager<Delegate, Key, KeyCompare> m_connections;
	};
}

#undef PARAM_SEPARATOR
#undef TEMPLATE_PARAMS
#undef SIGNATURE
#undef INPUT_PARAMS_ELEMENT
#undef INPUT_PARAMS
#undef OUTPUT_PARAMS
#undef CAPTURE_PARAMS
#undef PARAMS_ELEMENT_INPUT_ELEMENT
#undef PARAMS_ELEMENT_INPUT
#undef PARAMS_ELEMENT_CONSTRUCT_PARAMS_ELEMENT
#undef PARAMS_ELEMENT_CONSTRUCT_PARAMS
#undef PARAMS_ELEMENT_CONSTRUCT
#undef PARAMS_ELEMENT_OUTPUT
#undef PARAMS_ELEMENT_MEMBERS_ELEMENT
#undef PARAMS_ELEMENT_MEMBERS