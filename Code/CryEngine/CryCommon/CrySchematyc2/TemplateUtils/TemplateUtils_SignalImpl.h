// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef INCLUDING_FROM_TEMPLATE_UTILS_SIGNAL_HEADER
#error This file should only be included from TemplateUtils_Signal.h!
#endif //INCLUDING_FROM_TEMPLATE_UTILS_SIGNAL_HEADER

#define PARAM_SEPARATOR                                       PP_COMMA_IF(PARAM_COUNT)
#define TEMPLATE_PARAMS                                       typename RETURN PARAM_SEPARATOR PP_ENUM_PARAMS(PARAM_COUNT, typename PARAM)
#define SIGNATURE                                             RETURN ( PP_ENUM_PARAMS(PARAM_COUNT, PARAM) )
#define INPUT_PARAMS_ELEMENT(i, user_data)                    PARAM##i param##i	
#define INPUT_PARAMS                                          PP_COMMA_ENUM(PARAM_COUNT, INPUT_PARAMS_ELEMENT, PP_EMPTY)
#define OUTPUT_PARAMS                                         PP_ENUM_PARAMS(PARAM_COUNT, param)
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
	template <TEMPLATE_PARAMS> struct SSignalParams<SIGNATURE>
	{
		inline SSignalParams(PARAMS_ELEMENT_INPUT)
			PARAMS_ELEMENT_CONSTRUCT
		{}

		inline void Send(const CDelegate<SIGNATURE>& delegate) const
		{
			delegate(PARAMS_ELEMENT_OUTPUT);
		}

		PARAMS_ELEMENT_MEMBERS
	};

	template <TEMPLATE_PARAMS, class ELEMENT>	class CSignalQueue<SIGNATURE, void, ELEMENT>
	{
	private:

		typedef std::vector<ELEMENT> ElementVector;

	public:

		typedef typename ElementVector::const_iterator Iterator;

		inline void PushBack(INPUT_PARAMS)
		{
			m_elements.push_back(OUTPUT_PARAMS);
		}

		inline Iterator Begin() const
		{
			return m_elements.begin();
		}

		inline Iterator End() const
		{
			return m_elements.end();
		}

		inline void Clear()
		{
			m_elements.clear();
		}

	private:

		ElementVector m_elements;
	};

	template <TEMPLATE_PARAMS, typename KEY, class ELEMENT>	class CSignalQueue<SIGNATURE, KEY, ELEMENT>
	{
	private:

		typedef KEY                         Key;
		typedef std::pair<Key, ELEMENT>     KeyElementPair;
		typedef std::vector<KeyElementPair> KeyElementVector;

	public:

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

		typedef typename KeyElementVector::const_iterator Iterator;

		inline const CSelectionScope Select(const Key& key)
		{
			return CSelectionScope(*this, key);
		}

		inline Iterator Begin() const
		{
			return m_keyElements.begin();
		}

		inline Iterator End() const
		{
			return m_keyElements.end();
		}

		inline void Clear()
		{
			m_keyElements.clear();
		}

	private:

		inline void PushBack(const Key& key PARAM_SEPARATOR INPUT_PARAMS)
		{
			m_keyElements.push_back(KeyElementPair(key PARAM_SEPARATOR OUTPUT_PARAMS));
		}

		KeyElementVector m_keyElements;
	};

	template <TEMPLATE_PARAMS> class ISignal<SIGNATURE>
	{
	public:

		typedef void                              Key;
		typedef std::less<void>                   KeyCompare;
		typedef CDelegate<SIGNATURE>              Delegate;
		typedef CSignalScope<CSignal<SIGNATURE> > Scope;

		virtual ~ISignal() {}

		virtual bool Connect(const Delegate& delegate) = 0;
		virtual bool Connect(const Delegate& delegate, Scope& scope) = 0;
		virtual void Disconnect(const Delegate& delegate) = 0;
		virtual void Disconnect(Scope& scope) = 0;
	};

	template <TEMPLATE_PARAMS> class CSignal<SIGNATURE> : public ISignal<SIGNATURE>
	{
	public:

		typedef ISignal<SIGNATURE>             Interface;
		typedef typename Interface::Key        Key;
		typedef typename Interface::KeyCompare KeyCompare;
		typedef typename Interface::Delegate   Delegate;
		typedef typename Interface::Scope      Scope;

		inline CSignal()
			: m_emptySlotCount(0)
			, m_recursionDepth(0)
			, m_bDirty(false)
		{}

		inline ~CSignal()
		{
			for(typename SlotVector::iterator itSlot = m_slots.begin(), itEndSlot = m_slots.end(); itSlot != itEndSlot; ++ itSlot)
			{
				Scope* pScope = itSlot->pScope;
				if(pScope)
				{
					CRY_ASSERT((pScope->m_pSignal == this) || !pScope->m_pSignal);
					if(pScope->m_pSignal == this)
					{
						pScope->m_pSignal = nullptr;
					}
				}
			}
		}

		// ISignal

		virtual bool Connect(const Delegate& delegate)
		{
			m_slots.push_back(SSlot(delegate));
			return true;
		}

		virtual bool Connect(const Delegate& delegate, Scope& scope)
		{
			CRY_ASSERT((scope.m_pSignal == this) || !scope.m_pSignal);
			if((scope.m_pSignal == this) || !scope.m_pSignal)
			{
				Scope* pScope = reinterpret_cast<Scope*>(&reinterpret_cast<void*&>(scope)); // Temporary workaround for PS4 compilation error.
				m_slots.push_back(SSlot(delegate, pScope));
				scope.m_pSignal = this;
				return true;
			}
			return false;
		}

		virtual void Disconnect(const Delegate& delegate)
		{
			uint32 emptySlotCount = 0;
			std::replace_if(m_slots.begin(), m_slots.end(), SFindSlotsByDelegate(delegate, emptySlotCount), SSlot());
			m_bDirty         |= emptySlotCount != 0;
			m_emptySlotCount += emptySlotCount;
		}

		virtual void Disconnect(Scope& scope)
		{
			CRY_ASSERT(scope.m_pSignal == this);
			if(scope.m_pSignal == this)
			{
				uint32 emptySlotCount = 0;
				Scope* pScope = reinterpret_cast<Scope*>(&reinterpret_cast<void*&>(scope)); // Temporary workaround for PS4 compilation error.
				std::replace_if(m_slots.begin(), m_slots.end(), SFindSlotsByScope(pScope, emptySlotCount), SSlot());
				m_bDirty         |= emptySlotCount != 0;
				m_emptySlotCount += emptySlotCount;
				scope.m_pSignal = nullptr;
			}
		}

		virtual void Send(INPUT_PARAMS)
		{
			CRY_ASSERT(m_recursionDepth < SIGNAL_MAX_RECURSION_DEPTH);
			if(m_recursionDepth < SIGNAL_MAX_RECURSION_DEPTH)
			{
				Cleanup();
				++ m_recursionDepth;
				for(size_t pos = 0, size = m_slots.size(); pos < size; ++ pos)
				{
					Delegate& delegate = m_slots[pos].delegate;
					if(delegate)
					{
						delegate(OUTPUT_PARAMS);
					}
				}
				CRY_ASSERT(m_recursionDepth > 0);
				-- m_recursionDepth;
			}
		}

		template <typename ELEMENT> inline void ProcessQueue(CSignalQueue<SIGNATURE, void, ELEMENT>& queue)
		{
			CRY_ASSERT(!m_recursionDepth);
			if(!m_recursionDepth)
			{
				Cleanup();
				for(typename CSignalQueue<SIGNATURE, void, ELEMENT>::Iterator itQueueElement = queue.Begin(), itEndQueueElement = queue.End(); itQueueElement != itEndQueueElement; ++ itQueueElement)
				{
					const ELEMENT& queueElement = *itQueueElement;
					for(size_t pos = 0, size = m_slots.size(); pos < size; ++ pos)
					{
						Delegate& delegate = m_slots[pos].delegate;
						if(delegate)
						{
							queueElement.Send(delegate);
						}
					}
				}
				queue.Clear();
			}
		}

		// ~ISignal

		inline int16 GetRecursionDepth() const
		{
			return m_recursionDepth;
		}

	private:

		struct SSlot
		{
			inline SSlot(const Delegate& _delegate = Delegate(), Scope* _pScope = nullptr)
				: delegate(_delegate)
				, pScope(_pScope)
			{}

			inline bool operator == (const SSlot& rhs) const
			{
				return (delegate == rhs.delegate) && (pScope == rhs.pScope);
			}

			Delegate delegate;
			Scope*   pScope;
		};

		typedef std::vector<SSlot> SlotVector;

		struct SSlotCompare
		{
			inline bool operator () (const SSlot& lhs, const SSlot& rhs) const
			{
				return lhs.delegate < rhs.delegate;
			}
		};

		struct SFindSlotsByDelegate
		{
			inline SFindSlotsByDelegate(const Delegate& _delegate, uint32& _count)
				: delegate(_delegate)
				, count(_count)
			{}

			inline bool operator () (const SSlot& slot)
			{
				if(slot.delegate == delegate)
				{
					++ count;
					return true;
				}
				return false;
			}

			const Delegate& delegate;
			uint32&         count;
		};

		struct SFindSlotsByScope
		{
			inline SFindSlotsByScope(const Scope* _pScope, uint32& _count)
				: pScope(_pScope)
				, count(_count)
			{}

			inline bool operator () (const SSlot& slot)
			{
				if(slot.pScope == pScope)
				{
					++ count;
					return true;
				}
				return false;
			}

			const Scope* pScope;
			uint32&      count;
		};

		inline void Cleanup()
		{
			if(m_bDirty && !m_recursionDepth)
			{
				if(m_emptySlotCount)
				{
					if(m_emptySlotCount == m_slots.size())
					{
						m_slots.clear();
					}
					else
					{
						CRY_ASSERT(m_emptySlotCount);
						typename SlotVector::iterator itEndSlot = m_slots.end();
						typename SlotVector::iterator itEraseSlot = std::remove(m_slots.begin(), itEndSlot, SSlot());
						CRY_ASSERT(itEraseSlot != itEndSlot);
						if(itEraseSlot != itEndSlot)
						{
							m_slots.erase(itEraseSlot, itEndSlot);
						}
					}
				}
				m_emptySlotCount = 0;
				m_bDirty         = false;
			}
		}

		uint32     m_emptySlotCount;
		int16      m_recursionDepth;
		bool       m_bDirty;
		SlotVector m_slots;
	};

	template <TEMPLATE_PARAMS, typename KEY, class KEY_COMPARE> class ISignal<SIGNATURE, KEY, KEY_COMPARE>
	{
	public:

		typedef KEY                                                Key;
		typedef KEY_COMPARE                                        KeyCompare;
		typedef CDelegate<SIGNATURE>                               Delegate;
		typedef CSignalScope<CSignal<SIGNATURE, Key, KeyCompare> > Scope;

		class CSelectionScope
		{
		public:

			inline CSelectionScope(ISignal& signal, const Key&	key)
				: m_signal(signal)
				, m_key(key)
			{}

			inline bool Connect(const Delegate& delegate) const
			{
				return m_signal.Connect(m_key, delegate);
			}

			inline bool Connect(const Delegate& delegate, Scope& scope) const
			{
				return m_signal.Connect(m_key, delegate, scope);
			}

			inline void Disconnect(const Delegate& delegate) const
			{
				m_signal.Disconnect(m_key, delegate);
			}

			inline void Send(INPUT_PARAMS) const
			{
				m_signal.Send(m_key PARAM_SEPARATOR OUTPUT_PARAMS);
			}

		private:

			ISignal&   m_signal;
			const Key& m_key;
		};

		friend class CSelectionScope;

		virtual ~ISignal() {}

		virtual const CSelectionScope Select(const Key& key) = 0;
		virtual void Disconnect(Scope& scope) = 0;
		virtual void Send(INPUT_PARAMS) = 0;

	private:

		virtual bool Connect(const Key& key, const Delegate& delegate) = 0;
		virtual bool Connect(const Key& key, const Delegate& delegate, Scope& scope) = 0;
		virtual void Disconnect(const Key& key, const Delegate& delegate) = 0;
		virtual void Send(const Key& key PARAM_SEPARATOR INPUT_PARAMS) = 0;
	};

	template <TEMPLATE_PARAMS, typename KEY, class KEY_COMPARE> class CSignal<SIGNATURE, KEY, KEY_COMPARE> : public ISignal<SIGNATURE, KEY, KEY_COMPARE>
	{
	public:

		typedef ISignal<SIGNATURE, KEY, KEY_COMPARE> Interface;
		typedef typename Interface::Key              Key;
		typedef typename Interface::KeyCompare       KeyCompare;
		typedef typename Interface::Delegate         Delegate;
		typedef typename Interface::Scope            Scope;
		typedef typename Interface::CSelectionScope  CSelectionScope;

		inline CSignal(const KeyCompare& keyCompare = KeyCompare())
			: m_keyCompare(keyCompare)
			, m_emptySlotCount(0)
			, m_recursionDepth(0)
			, m_bDirty(false)
		{}

		inline ~CSignal()
		{
			for(typename SlotVector::iterator itSlot = m_slots.begin(), itEndSlot = m_slots.end(); itSlot != itEndSlot; ++ itSlot)
			{
				Scope* pScope = itSlot->pScope;
				if(pScope)
				{
					CRY_ASSERT((pScope->m_pSignal == this) || !pScope->m_pSignal);
					if(pScope->m_pSignal == this)
					{
						pScope->m_pSignal = nullptr;
					}
				}
			}
		}

		// ISignal

		virtual const CSelectionScope Select(const Key& key)
		{
			return CSelectionScope(*this, key);
		}

		virtual void Disconnect(Scope& scope)
		{
			CRY_ASSERT(scope.m_pSignal == this);
			if(scope.m_pSignal == this)
			{
				uint32  emptySlotCount = 0;
				Scope* pScope = reinterpret_cast<Scope*>(&reinterpret_cast<void*&>(scope)); // Temporary workaround for PS4 compilation error.
				std::replace_if(m_slots.begin(), m_slots.end(), SFindSlotsByScope(pScope, emptySlotCount), SSlot());
				m_bDirty         |= emptySlotCount != 0;
				m_emptySlotCount += emptySlotCount;
				scope.m_pSignal	= nullptr;
			}
		}

		virtual void Send(INPUT_PARAMS)
		{
			CRY_ASSERT(m_recursionDepth < SIGNAL_MAX_RECURSION_DEPTH);
			if(m_recursionDepth < SIGNAL_MAX_RECURSION_DEPTH)
			{
				Cleanup();
				++ m_recursionDepth;
				for(size_t pos = 0, size = m_slots.size(); pos < size; ++ pos)
				{
					Delegate& delegate = m_slots[pos].delegate;
					if(delegate)
					{
						delegate(OUTPUT_PARAMS);
					}
				}
				CRY_ASSERT(m_recursionDepth > 0);
				-- m_recursionDepth;
			}
		}

		// ~ISignal

		template <typename ELEMENT> inline void ProcessQueue(CSignalQueue<SIGNATURE, void, ELEMENT>& queue)
		{
			CRY_ASSERT(!m_recursionDepth);
			if(!m_recursionDepth)
			{
				Cleanup();
				for(typename CSignalQueue<SIGNATURE, void, ELEMENT>::Iterator itQueueElement = queue.Begin(), itEndQueueElement = queue.End(); itQueueElement != itEndQueueElement; ++ itQueueElement)
				{
					const ELEMENT& queueElement = *itQueueElement;
					for(size_t pos = 0, size = m_slots.size(); pos < size; ++ pos)
					{
						Delegate& delegate = m_slots[pos].delegate;
						if(delegate)
						{
							queueElement.Send(delegate);
						}
					}
				}
				queue.Clear();
			}
		}

		template <typename ELEMENT> inline void ProcessQueue(CSignalQueue<SIGNATURE, Key, ELEMENT>& queue)
		{
			CRY_ASSERT(!m_recursionDepth);
			if(!m_recursionDepth)
			{
				Cleanup();
				for(typename CSignalQueue<SIGNATURE, Key, ELEMENT>::Iterator itQueueElement = queue.Begin(), itEndQueueElement = queue.End(); itQueueElement != itEndQueueElement; ++ itQueueElement)
				{
					const Key&                    key = itQueueElement->first;
					const ELEMENT&                queueElement = itQueueElement->second;
					typename SlotVector::iterator itBeginSlot = m_slots.begin();
					typename SlotVector::iterator itEndSlot = m_slots.end();
					typename SlotVector::iterator itLowerBoundSlot = std::lower_bound(itBeginSlot, itEndSlot, key, SSlotCompare(m_keyCompare));
					for(size_t pos = itLowerBoundSlot - itBeginSlot, size = m_slots.size(); pos < size; ++ pos)
					{
						SSlot& slot = m_slots[pos];
						if(m_keyCompare(key, slot.key))
						{
							break;
						}
						else
						{
							Delegate& delegate = m_slots[pos].delegate;
							if(delegate)
							{
								queueElement.Send(delegate);
							}
						}
					}
				}
				queue.Clear();
			}
		}

		inline int16 GetRecursionDepth() const
		{
			return m_recursionDepth;
		}

	private:

		struct SSlot
		{
			inline SSlot(const Key& _key = Key(), const Delegate& _delegate = Delegate(), Scope* _pScope = nullptr)
				: key(_key)
				, delegate(_delegate)
				, pScope(_pScope)
			{}

			inline bool operator == (const SSlot& rhs) const
			{
				return (delegate == rhs.delegate) && (pScope == rhs.pScope);
			}

			Key      key;
			Delegate delegate;
			Scope*   pScope;
		};

		typedef std::vector<SSlot> SlotVector;

		struct SSlotCompare // TODO : Replace with lambda function?
		{
			inline SSlotCompare(const KeyCompare& _keyCompare = KeyCompare())
				: keyCompare(_keyCompare)
			{}

			inline bool operator () (const SSlot& slot, const Key& key) const
			{
				return keyCompare(slot.key, key);
			}

			inline bool operator () (const SSlot& lhs, const SSlot& rhs) const
			{
				return keyCompare(lhs.key, rhs.key);
			}

			const KeyCompare& keyCompare;
		};

		struct SFindSlotsByKeyAndDelegate // TODO : Replace with lambda function?
		{
			inline SFindSlotsByKeyAndDelegate(const Key& _key, const KeyCompare& _keyCompare, const Delegate& _delegate, uint32& _count)
				: key(_key)
				, keyCompare(_keyCompare)
				, delegate(_delegate)
				, count(_count)
			{}

			inline bool operator () (const SSlot& slot)
			{
				if(!keyCompare(slot.key, key) && !keyCompare(key, slot.key) && (slot.delegate == delegate))
				{
					++ count;
					return true;
				}
				return false;
			}

			const Key&        key;
			const KeyCompare& keyCompare;
			const Delegate&   delegate;
			uint32&           count;
		};

		struct SFindSlotsByDelegate // TODO : Replace with lambda function?
		{
			inline SFindSlotsByDelegate(const Delegate& _delegate, uint32& _count)
				: delegate(_delegate)
				, count(_count)
			{}

			inline bool operator () (const SSlot& slot)
			{
				if(slot.delegate == delegate)
				{
					++ count;
					return true;
				}
				return false;
			}

			const Delegate& delegate;
			uint32&         count;
		};

		struct SFindSlotsByScope // TODO : Replace with lambda function?
		{
			inline SFindSlotsByScope(const Scope* _pScope, uint32& _count)
				: pScope(_pScope)
				, count(_count)
			{}

			inline bool operator () (const SSlot& slot)
			{
				if(slot.pScope == pScope)
				{
					++ count;
					return true;
				}
				return false;
			}

			const Scope* pScope;
			uint32&      count;
		};

		virtual bool Connect(const Key& key, const Delegate& delegate)
		{
			m_bDirty = true;
			m_slots.push_back(SSlot(key, delegate));
			return true;
		}

		virtual bool Connect(const Key& key, const Delegate& delegate, Scope& scope)
		{
			CRY_ASSERT((scope.m_pSignal == this) || !scope.m_pSignal);
			if((scope.m_pSignal == this) || !scope.m_pSignal)
			{
				m_slots.push_back(SSlot(key, delegate, &scope));
				m_bDirty        = true;
				scope.m_pSignal = this;
				return true;
			}
			return false;
		}

		virtual void Disconnect(const Key& key, const Delegate& delegate)
		{
			uint32 emptySlotCount = 0;
			std::replace_if(m_slots.begin(), m_slots.end(), SFindSlotsByKeyAndDelegate(key, m_keyCompare, delegate, emptySlotCount), SSlot());
			m_bDirty         |= emptySlotCount != 0;
			m_emptySlotCount += emptySlotCount;
		}

		virtual void Send(const Key& key PARAM_SEPARATOR INPUT_PARAMS)
		{
			CRY_ASSERT(m_recursionDepth < SIGNAL_MAX_RECURSION_DEPTH);
			if(m_recursionDepth < SIGNAL_MAX_RECURSION_DEPTH)
			{
				Cleanup();
				++ m_recursionDepth;
				typename SlotVector::iterator itBeginSlot = m_slots.begin();
				typename SlotVector::iterator itEndSlot = m_slots.end();
				typename SlotVector::iterator itLowerBoundSlot = std::lower_bound(itBeginSlot, itEndSlot, key, SSlotCompare(m_keyCompare));
				for(size_t pos = itLowerBoundSlot - itBeginSlot, size = m_slots.size(); pos < size; ++ pos)
				{
					SSlot& slot = m_slots[pos];
					if(m_keyCompare(key, slot.key))
					{
						break;
					}
					else
					{
						Delegate& delegate = m_slots[pos].delegate;
						if(delegate)
						{
							delegate(OUTPUT_PARAMS);
						}
					}
				}
				CRY_ASSERT(m_recursionDepth > 0);
				-- m_recursionDepth;
			}
		}

		inline void Cleanup()
		{
			if(m_bDirty && !m_recursionDepth)
			{
				if(m_emptySlotCount)
				{
					if(m_emptySlotCount == m_slots.size())
					{
						m_slots.clear();
					}
					else
					{
						typename SlotVector::iterator itEndSlot = m_slots.end();
						typename SlotVector::iterator itEraseSlot = std::remove(m_slots.begin(), itEndSlot, SSlot());
						CRY_ASSERT(itEraseSlot != itEndSlot);
						if(itEraseSlot != itEndSlot)
						{
							m_slots.erase(itEraseSlot, itEndSlot);
						}
					}
				}
				std::sort(m_slots.begin(), m_slots.end(), SSlotCompare(m_keyCompare));
				m_emptySlotCount = 0;
				m_bDirty         = false;
			}
		}

		KeyCompare  m_keyCompare;
		uint32      m_emptySlotCount;
		int16       m_recursionDepth;
		bool        m_bDirty;
		SlotVector  m_slots;
	};
}

#undef PARAM_SEPARATOR
#undef TEMPLATE_PARAMS
#undef SIGNATURE
#undef INPUT_PARAMS_ELEMENT
#undef INPUT_PARAMS
#undef OUTPUT_PARAMS
#undef PARAMS_ELEMENT_INPUT_ELEMENT
#undef PARAMS_ELEMENT_INPUT
#undef PARAMS_ELEMENT_CONSTRUCT_ELEMENT
#undef PARAMS_ELEMENT_CONSTRUCT
#undef PARAMS_ELEMENT_OUTPUT
#undef PARAMS_ELEMENT_MEMBERS_ELEMENT
#undef PARAMS_ELEMENT_MEMBERS
