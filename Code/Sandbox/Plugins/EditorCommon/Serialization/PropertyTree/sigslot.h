// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// sigslot.h: Signal/Slot classes
// 
// Written by Sarah Thompson (sarah@telergy.com) 2002.
//
// License: Public domain. You are free to use this code however you like, with the proviso that
//          the author takes on no responsibility or liability for any use.
//
// QUICK DOCUMENTATION 
//		
//				(see also the full documentation at http://sigslot.sourceforge.net/)
//
//		#define switches
//			SIGSLOT_PURE_ISO			- Define this to force ISO C++ compliance. This also disables
//										  all of the thread safety support on platforms where it is 
//										  available.
//
//			SIGSLOT_USE_POSIX_THREADS	- Force use of Posix threads when using a C++ compiler other than
//										  gcc on a platform that supports Posix threads. (When using gcc,
//										  this is the default - use SIGSLOT_PURE_ISO to disable this if 
//										  necessary)
//
//			SIGSLOT_DEFAULT_MT_POLICY	- Where thread support is enabled, this defaults to multi_threaded_global.
//										  Otherwise, the default is single_threaded. #define this yourself to
//										  override the default. In pure ISO mode, anything other than
//										  single_threaded will cause a compiler error.
//
//		PLATFORM NOTES
//
//			Win32						- On Win32, the WIN32 symbol must be #defined. Most mainstream
//										  compilers do this by default, but you may need to define it
//										  yourself if your build environment is less standard. This causes
//										  the Win32 thread support to be compiled in and used automatically.
//
//			Unix/Linux/BSD, etc.		- If you're using gcc, it is assumed that you have Posix threads
//										  available, so they are used automatically. You can override this
//										  (as under Windows) with the SIGSLOT_PURE_ISO switch. If you're using
//										  something other than gcc but still want to use Posix threads, you
//										  need to #define SIGSLOT_USE_POSIX_THREADS.
//
//			ISO C++						- If none of the supported platforms are detected, or if
//										  SIGSLOT_PURE_ISO is defined, all multithreading support is turned off,
//										  along with any code that might cause a pure ISO C++ environment to
//										  complain. Before you ask, gcc -ansi -pedantic won't compile this 
//										  library, but gcc -ansi is fine. Pedantic mode seems to throw a lot of
//										  errors that aren't really there. If you feel like investigating this,
//										  please contact the author.
//
//		
//		THREADING MODES
//
//			single_threaded				- Your program is assumed to be single threaded from the point of view
//										  of signal/slot usage (i.e. all objects using signals and slots are
//										  created and destroyed from a single thread). Behaviour if objects are
//										  destroyed concurrently is undefined (i.e. you'll get the occasional
//										  segmentation fault/memory exception).
//
//			multi_threaded_global		- Your program is assumed to be multi threaded. Objects using signals and
//										  slots can be safely created and destroyed from any thread, even when
//										  connections exist. In multi_threaded_global mode, this is achieved by a
//										  single global mutex (actually a critical section on Windows because they
//										  are faster). This option uses less OS resources, but results in more
//										  opportunities for contention, possibly resulting in more context switches
//										  than are strictly necessary.
//
//			multi_threaded_local		- Behaviour in this mode is essentially the same as multi_threaded_global,
//										  except that each signal, and each object that inherits has_slots, all 
//										  have their own mutex/critical section. In practice, this means that
//										  mutex collisions (and hence context switches) only happen if they are
//										  absolutely essential. However, on some platforms, creating a lot of 
//										  mutexes can slow down the whole OS, so use this option with care.
//
//		USING THE LIBRARY
//
//			See the full documentation at http://sigslot.sourceforge.net/
//
//

#ifndef SIGSLOT_H__
#define SIGSLOT_H__

#ifndef NULL
# define NULL 0
#endif

#include <set>
#include <list>

#ifdef emit
#undef emit
#endif

namespace sigslot {

	class has_slots;

	class _connection_base0
	{
	public:
		virtual ~_connection_base0() {}
		virtual has_slots* getdest() const = 0;
		virtual void emit() = 0;
		virtual _connection_base0* clone() = 0;
		virtual _connection_base0* duplicate(has_slots* pnewdest) = 0;
	};

	template<class arg1_type>
	class _connection_base1
	{
	public:
		virtual ~_connection_base1() {}
		virtual has_slots* getdest() const = 0;
		virtual void emit(arg1_type) = 0;
		virtual _connection_base1<arg1_type>* clone() = 0;
		virtual _connection_base1<arg1_type>* duplicate(has_slots* pnewdest) = 0;
	};

	template<class arg1_type, class arg2_type>
	class _connection_base2
	{
	public:
		virtual ~_connection_base2() {}
		virtual has_slots* getdest() const = 0;
		virtual void emit(arg1_type, arg2_type) = 0;
		virtual _connection_base2<arg1_type, arg2_type>* clone() = 0;
		virtual _connection_base2<arg1_type, arg2_type>* duplicate(has_slots* pnewdest) = 0;
	};

	template<class arg1_type, class arg2_type, class arg3_type>
	class _connection_base3
	{
	public:
		virtual ~_connection_base3() {}
		virtual has_slots* getdest() const = 0;
		virtual void emit(arg1_type, arg2_type, arg3_type) = 0;
		virtual _connection_base3<arg1_type, arg2_type, arg3_type>* clone() = 0;
		virtual _connection_base3<arg1_type, arg2_type, arg3_type>* duplicate(has_slots* pnewdest) = 0;
	};

	template<class arg1_type, class arg2_type, class arg3_type, class arg4_type>
	class _connection_base4
	{
	public:
		virtual ~_connection_base4() {}
		virtual has_slots* getdest() const = 0;
		virtual void emit(arg1_type, arg2_type, arg3_type, arg4_type) = 0;
		virtual _connection_base4<arg1_type, arg2_type, arg3_type, arg4_type>* clone() = 0;
		virtual _connection_base4<arg1_type, arg2_type, arg3_type, arg4_type>* duplicate(has_slots* pnewdest) = 0;
	};

	template<class arg1_type, class arg2_type, class arg3_type, class arg4_type,
	class arg5_type>
	class _connection_base5
	{
	public:
		virtual ~_connection_base5() {}
		virtual has_slots* getdest() const = 0;
		virtual void emit(arg1_type, arg2_type, arg3_type, arg4_type, 
			arg5_type) = 0;
		virtual _connection_base5<arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type>* clone() = 0;
		virtual _connection_base5<arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type>* duplicate(has_slots* pnewdest) = 0;
	};

	template<class arg1_type, class arg2_type, class arg3_type, class arg4_type,
	class arg5_type, class arg6_type>
	class _connection_base6
	{
	public:
		virtual ~_connection_base6() {}
		virtual has_slots* getdest() const = 0;
		virtual void emit(arg1_type, arg2_type, arg3_type, arg4_type, arg5_type,
			arg6_type) = 0;
		virtual _connection_base6<arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type, arg6_type>* clone() = 0;
		virtual _connection_base6<arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type, arg6_type>* duplicate(has_slots* pnewdest) = 0;
	};

	template<class arg1_type, class arg2_type, class arg3_type, class arg4_type,
	class arg5_type, class arg6_type, class arg7_type>
	class _connection_base7
	{
	public:
		virtual ~_connection_base7() {}
		virtual has_slots* getdest() const = 0;
		virtual void emit(arg1_type, arg2_type, arg3_type, arg4_type, arg5_type,
			arg6_type, arg7_type) = 0;
		virtual _connection_base7<arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type, arg6_type, arg7_type>* clone() = 0;
		virtual _connection_base7<arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type, arg6_type, arg7_type>* duplicate(has_slots* pnewdest) = 0;
	};

	template<class arg1_type, class arg2_type, class arg3_type, class arg4_type,
	class arg5_type, class arg6_type, class arg7_type, class arg8_type>
	class _connection_base8
	{
	public:
		virtual ~_connection_base8() {}
		virtual has_slots* getdest() const = 0;
		virtual void emit(arg1_type, arg2_type, arg3_type, arg4_type, arg5_type,
			arg6_type, arg7_type, arg8_type) = 0;
		virtual _connection_base8<arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type, arg6_type, arg7_type, arg8_type>* clone() = 0;
		virtual _connection_base8<arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type, arg6_type, arg7_type, arg8_type>* duplicate(has_slots* pnewdest) = 0;
	};

	class _signal_base
	{
	public:
		virtual ~_signal_base() {}
		virtual void slot_disconnect(has_slots* pslot) = 0;
		virtual void slot_duplicate(const has_slots* poldslot, has_slots* pnewslot) = 0;
	};

	class has_slots
	{
	private:
		typedef std::set<_signal_base *> sender_set;
		typedef sender_set::const_iterator const_iterator;

	public:
		has_slots()
		{
			;
		}

		has_slots(const has_slots& hs)
		{
			const_iterator it = hs.m_senders.begin();
			const_iterator itEnd = hs.m_senders.end();

			while(it != itEnd)
			{
				(*it)->slot_duplicate(&hs, this);
				m_senders.insert(*it);
				++it;
			}
		} 

		void signal_connect(_signal_base* sender)
		{
			m_senders.insert(sender);
		}

		void signal_disconnect(_signal_base* sender)
		{
			m_senders.erase(sender);
		}

		virtual ~has_slots()
		{
			disconnect_all();
		}

		void disconnect_all()
		{
			const_iterator it = m_senders.begin();
			const_iterator itEnd = m_senders.end();

			while(m_senders.begin() != m_senders.end())
			{
				_signal_base* p = *m_senders.begin();
				m_senders.erase(m_senders.begin());
				p->slot_disconnect(this);
			}
		}

	private:
		sender_set m_senders;
	};

	class _signal_base0 : public _signal_base
	{
	public:
		typedef std::list<_connection_base0 *>  connections_list;

		_signal_base0()
		{
			;
		}

		_signal_base0(const _signal_base0& s)
			: _signal_base(s)
		{
			connections_list::const_iterator it = s.m_connected_slots.begin();
			connections_list::const_iterator itEnd = s.m_connected_slots.end();

			while(it != itEnd)
			{
				(*it)->getdest()->signal_connect(this);
				m_connected_slots.push_back((*it)->clone());

				++it;
			}
		}

		~_signal_base0()
		{
			disconnect_all();
		}

		void disconnect_all()
		{
			connections_list::const_iterator it = m_connected_slots.begin();
			connections_list::const_iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				(*it)->getdest()->signal_disconnect(this);
				delete *it;

				++it;
			}

			m_connected_slots.erase(m_connected_slots.begin(), m_connected_slots.end());
		}

		void disconnect(has_slots* pclass)
		{
			connections_list::iterator it = m_connected_slots.begin();
			connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == pclass)
				{
					delete *it;
					m_connected_slots.erase(it);
					pclass->signal_disconnect(this);
					return;
				}

				++it;
			}
		}

		void slot_disconnect(has_slots* pslot)
		{
			connections_list::iterator it = m_connected_slots.begin();
			connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == pslot)
				{
					delete *it; //
					it = m_connected_slots.erase(it);
				}
				else
					++it;
			}
		}

		void slot_duplicate(const has_slots* oldtarget, has_slots* newtarget)
		{
			connections_list::iterator it = m_connected_slots.begin();
			connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == oldtarget)
				{
					m_connected_slots.push_back((*it)->duplicate(newtarget));
				}

				++it;
			}
		}

	protected:
		connections_list m_connected_slots;   
	};

	template<class arg1_type>
	class _signal_base1 : public _signal_base
	{
	public:
		typedef std::list<_connection_base1<arg1_type> *>  connections_list;

		_signal_base1()
		{
			;
		}

		_signal_base1(const _signal_base1<arg1_type>& s)
			: _signal_base(s)
		{
			typename connections_list::const_iterator it = s.m_connected_slots.begin();
			typename connections_list::const_iterator itEnd = s.m_connected_slots.end();

			while(it != itEnd)
			{
				(*it)->getdest()->signal_connect(this);
				m_connected_slots.push_back((*it)->clone());

				++it;
			}
		}

		void slot_duplicate(const has_slots* oldtarget, has_slots* newtarget)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == oldtarget)
				{
					m_connected_slots.push_back((*it)->duplicate(newtarget));
				}

				++it;
			}
		}

		~_signal_base1()
		{
			disconnect_all();
		}

		void disconnect_all()
		{
			typename connections_list::const_iterator it = m_connected_slots.begin();
			typename connections_list::const_iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				(*it)->getdest()->signal_disconnect(this);
				delete *it;

				++it;
			}

			m_connected_slots.erase(m_connected_slots.begin(), m_connected_slots.end());
		}

		void disconnect(has_slots* pclass)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == pclass)
				{
					delete *it;
					m_connected_slots.erase(it);
					pclass->signal_disconnect(this);
					return;
				}

				++it;
			}
		}

		void slot_disconnect(has_slots* pslot)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == pslot)
				{
					delete *it; //
					it = m_connected_slots.erase(it);
				}
				else
					++it;
			}
		}


	protected:
		connections_list m_connected_slots;   
	};

	template<class arg1_type, class arg2_type>
	class _signal_base2 : public _signal_base
	{
	public:
		typedef std::list<_connection_base2<arg1_type, arg2_type> *>
			connections_list;

		_signal_base2()
		{
			;
		}

		_signal_base2(const _signal_base2<arg1_type, arg2_type>& s)
			: _signal_base(s)
		{
			typename connections_list::const_iterator it = s.m_connected_slots.begin();
			typename connections_list::const_iterator itEnd = s.m_connected_slots.end();

			while(it != itEnd)
			{
				(*it)->getdest()->signal_connect(this);
				m_connected_slots.push_back((*it)->clone());

				++it;
			}
		}

		void slot_duplicate(const has_slots* oldtarget, has_slots* newtarget)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == oldtarget)
				{
					m_connected_slots.push_back((*it)->duplicate(newtarget));
				}

				++it;
			}
		}

		~_signal_base2()
		{
			disconnect_all();
		}

		void disconnect_all()
		{
			typename connections_list::const_iterator it = m_connected_slots.begin();
			typename connections_list::const_iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				(*it)->getdest()->signal_disconnect(this);
				delete *it;

				++it;
			}

			m_connected_slots.erase(m_connected_slots.begin(), m_connected_slots.end());
		}

		void disconnect(has_slots* pclass)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == pclass)
				{
					delete *it;
					m_connected_slots.erase(it);
					pclass->signal_disconnect(this);
					return;
				}

				++it;
			}
		}

		void slot_disconnect(has_slots* pslot)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == pslot)
				{
					delete *it; //
					it = m_connected_slots.erase(it);
				}
				else
					++it;
			}
		}

	protected:
		connections_list m_connected_slots;   
	};

	template<class arg1_type, class arg2_type, class arg3_type>
	class _signal_base3 : public _signal_base
	{
	public:
		typedef std::list<_connection_base3<arg1_type, arg2_type, arg3_type> *>
			connections_list;

		_signal_base3()
		{
			;
		}

		_signal_base3(const _signal_base3<arg1_type, arg2_type, arg3_type>& s)
			: _signal_base(s)
		{
			typename connections_list::const_iterator it = s.m_connected_slots.begin();
			typename connections_list::const_iterator itEnd = s.m_connected_slots.end();

			while(it != itEnd)
			{
				(*it)->getdest()->signal_connect(this);
				m_connected_slots.push_back((*it)->clone());

				++it;
			}
		}

		void slot_duplicate(const has_slots* oldtarget, has_slots* newtarget)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == oldtarget)
				{
					m_connected_slots.push_back((*it)->duplicate(newtarget));
				}

				++it;
			}
		}

		~_signal_base3()
		{
			disconnect_all();
		}

		void disconnect_all()
		{
			typename connections_list::const_iterator it = m_connected_slots.begin();
			typename connections_list::const_iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				(*it)->getdest()->signal_disconnect(this);
				delete *it;

				++it;
			}

			m_connected_slots.erase(m_connected_slots.begin(), m_connected_slots.end());
		}

		void disconnect(has_slots* pclass)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == pclass)
				{
					delete *it;
					m_connected_slots.erase(it);
					pclass->signal_disconnect(this);
					return;
				}

				++it;
			}
		}

		void slot_disconnect(has_slots* pslot)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == pslot)
				{
					delete *it; //
					it = m_connected_slots.erase(it);
				}
				else 
					++it;
			}
		}

	protected:
		connections_list m_connected_slots;   
	};

	template<class arg1_type, class arg2_type, class arg3_type, class arg4_type>
	class _signal_base4 : public _signal_base
	{
	public:
		typedef std::list<_connection_base4<arg1_type, arg2_type, arg3_type,
			arg4_type> *>  connections_list;

		_signal_base4()
		{
			;
		}

		_signal_base4(const _signal_base4<arg1_type, arg2_type, arg3_type, arg4_type>& s)
			: _signal_base(s)
		{
			typename connections_list::const_iterator it = s.m_connected_slots.begin();
			typename connections_list::const_iterator itEnd = s.m_connected_slots.end();

			while(it != itEnd)
			{
				(*it)->getdest()->signal_connect(this);
				m_connected_slots.push_back((*it)->clone());

				++it;
			}
		}

		void slot_duplicate(const has_slots* oldtarget, has_slots* newtarget)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == oldtarget)
				{
					m_connected_slots.push_back((*it)->duplicate(newtarget));
				}

				++it;
			}
		}

		~_signal_base4()
		{
			disconnect_all();
		}

		void disconnect_all()
		{
			typename connections_list::const_iterator it = m_connected_slots.begin();
			typename connections_list::const_iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				(*it)->getdest()->signal_disconnect(this);
				delete *it;

				++it;
			}

			m_connected_slots.erase(m_connected_slots.begin(), m_connected_slots.end());
		}

		void disconnect(has_slots* pclass)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == pclass)
				{
					delete *it;
					m_connected_slots.erase(it);
					pclass->signal_disconnect(this);
					return;
				}

				++it;
			}
		}

		void slot_disconnect(has_slots* pslot)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == pslot)
				{
					delete *it; //
					it = m_connected_slots.erase(it);
				}
				else
					++it;
			}
		}

	protected:
		connections_list m_connected_slots;   
	};

	template<class arg1_type, class arg2_type, class arg3_type, class arg4_type,
	class arg5_type>
	class _signal_base5 : public _signal_base
	{
	public:
		typedef std::list<_connection_base5<arg1_type, arg2_type, arg3_type,
			arg4_type, arg5_type> *>  connections_list;

		_signal_base5()
		{
			;
		}

		_signal_base5(const _signal_base5<arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type>& s)
			: _signal_base(s)
		{
			typename connections_list::const_iterator it = s.m_connected_slots.begin();
			typename connections_list::const_iterator itEnd = s.m_connected_slots.end();

			while(it != itEnd)
			{
				(*it)->getdest()->signal_connect(this);
				m_connected_slots.push_back((*it)->clone());

				++it;
			}
		}

		void slot_duplicate(const has_slots* oldtarget, has_slots* newtarget)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == oldtarget)
				{
					m_connected_slots.push_back((*it)->duplicate(newtarget));
				}

				++it;
			}
		}

		~_signal_base5()
		{
			disconnect_all();
		}

		void disconnect_all()
		{
			typename connections_list::const_iterator it = m_connected_slots.begin();
			typename connections_list::const_iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				(*it)->getdest()->signal_disconnect(this);
				delete *it;

				++it;
			}

			m_connected_slots.erase(m_connected_slots.begin(), m_connected_slots.end());
		}

		void disconnect(has_slots* pclass)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == pclass)
				{
					delete *it;
					m_connected_slots.erase(it);
					pclass->signal_disconnect(this);
					return;
				}

				++it;
			}
		}

		void slot_disconnect(has_slots* pslot)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == pslot)
				{
					delete *it; //
					it = m_connected_slots.erase(it);
				}
				else
					++it;
			}
		}

	protected:
		connections_list m_connected_slots;   
	};

	template<class arg1_type, class arg2_type, class arg3_type, class arg4_type,
	class arg5_type, class arg6_type>
	class _signal_base6 : public _signal_base
	{
	public:
		typedef std::list<_connection_base6<arg1_type, arg2_type, arg3_type, 
			arg4_type, arg5_type, arg6_type> *>  connections_list;

		_signal_base6()
		{
			;
		}

		_signal_base6(const _signal_base6<arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type, arg6_type>& s)
			: _signal_base(s)
		{
			typename connections_list::const_iterator it = s.m_connected_slots.begin();
			typename connections_list::const_iterator itEnd = s.m_connected_slots.end();

			while(it != itEnd)
			{
				(*it)->getdest()->signal_connect(this);
				m_connected_slots.push_back((*it)->clone());

				++it;
			}
		}

		void slot_duplicate(const has_slots* oldtarget, has_slots* newtarget)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == oldtarget)
				{
					m_connected_slots.push_back((*it)->duplicate(newtarget));
				}

				++it;
			}
		}

		~_signal_base6()
		{
			disconnect_all();
		}

		void disconnect_all()
		{
			typename connections_list::const_iterator it = m_connected_slots.begin();
			typename connections_list::const_iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				(*it)->getdest()->signal_disconnect(this);
				delete *it;

				++it;
			}

			m_connected_slots.erase(m_connected_slots.begin(), m_connected_slots.end());
		}

		void disconnect(has_slots* pclass)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == pclass)
				{
					delete *it;
					m_connected_slots.erase(it);
					pclass->signal_disconnect(this);
					return;
				}

				++it;
			}
		}

		void slot_disconnect(has_slots* pslot)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == pslot)
				{
					delete *it; //
					it = m_connected_slots.erase(it);
				}
				else
					++it;
			}
		}

	protected:
		connections_list m_connected_slots;   
	};

	template<class arg1_type, class arg2_type, class arg3_type, class arg4_type,
	class arg5_type, class arg6_type, class arg7_type>
	class _signal_base7 : public _signal_base
	{
	public:
		typedef std::list<_connection_base7<arg1_type, arg2_type, arg3_type, 
			arg4_type, arg5_type, arg6_type, arg7_type> *>  connections_list;

		_signal_base7()
		{
			;
		}

		_signal_base7(const _signal_base7<arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type, arg6_type, arg7_type>& s)
			: _signal_base(s)
		{
			typename connections_list::const_iterator it = s.m_connected_slots.begin();
			typename connections_list::const_iterator itEnd = s.m_connected_slots.end();

			while(it != itEnd)
			{
				(*it)->getdest()->signal_connect(this);
				m_connected_slots.push_back((*it)->clone());

				++it;
			}
		}

		void slot_duplicate(const has_slots* oldtarget, has_slots* newtarget)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == oldtarget)
				{
					m_connected_slots.push_back((*it)->duplicate(newtarget));
				}

				++it;
			}
		}

		~_signal_base7()
		{
			disconnect_all();
		}

		void disconnect_all()
		{
			typename connections_list::const_iterator it = m_connected_slots.begin();
			typename connections_list::const_iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				(*it)->getdest()->signal_disconnect(this);
				delete *it;

				++it;
			}

			m_connected_slots.erase(m_connected_slots.begin(), m_connected_slots.end());
		}

		void disconnect(has_slots* pclass)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == pclass)
				{
					delete *it;
					m_connected_slots.erase(it);
					pclass->signal_disconnect(this);
					return;
				}

				++it;
			}
		}

		void slot_disconnect(has_slots* pslot)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == pslot)
				{
					delete *it; //
					it = m_connected_slots.erase(it);
				}
				else
					++it;
			}
		}

	protected:
		connections_list m_connected_slots;   
	};

	template<class arg1_type, class arg2_type, class arg3_type, class arg4_type,
	class arg5_type, class arg6_type, class arg7_type, class arg8_type>
	class _signal_base8 : public _signal_base
	{
	public:
		typedef std::list<_connection_base8<arg1_type, arg2_type, arg3_type, 
			arg4_type, arg5_type, arg6_type, arg7_type, arg8_type> *>
			connections_list;

		_signal_base8()
		{
			;
		}

		_signal_base8(const _signal_base8<arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type, arg6_type, arg7_type, arg8_type>& s)
			: _signal_base(s)
		{
			typename connections_list::const_iterator it = s.m_connected_slots.begin();
			typename connections_list::const_iterator itEnd = s.m_connected_slots.end();

			while(it != itEnd)
			{
				(*it)->getdest()->signal_connect(this);
				m_connected_slots.push_back((*it)->clone());

				++it;
			}
		}

		void slot_duplicate(const has_slots* oldtarget, has_slots* newtarget)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == oldtarget)
				{
					m_connected_slots.push_back((*it)->duplicate(newtarget));
				}

				++it;
			}
		}

		~_signal_base8()
		{
			disconnect_all();
		}

		void disconnect_all()
		{
			typename connections_list::const_iterator it = m_connected_slots.begin();
			typename connections_list::const_iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				(*it)->getdest()->signal_disconnect(this);
				delete *it;

				++it;
			}

			m_connected_slots.erase(m_connected_slots.begin(), m_connected_slots.end());
		}

		void disconnect(has_slots* pclass)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == pclass)
				{
					delete *it;
					m_connected_slots.erase(it);
					pclass->signal_disconnect(this);
					return;
				}

				++it;
			}
		}

		void slot_disconnect(has_slots* pslot)
		{
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				if((*it)->getdest() == pslot)
				{
					delete *it; //
					it = m_connected_slots.erase(it);
				}
				else
					++it;
			}
		}

	protected:
		connections_list m_connected_slots;   
	};


	template<class dest_type>
	class _connection0 : public _connection_base0
	{
	public:
		_connection0()
		{
			m_pobject = NULL;
			m_pmemfun = NULL;
		}

		_connection0(dest_type* pobject, void (dest_type::*pmemfun)())
		{
			m_pobject = pobject;
			m_pmemfun = pmemfun;
		}

		virtual _connection_base0* clone()
		{
			return new _connection0<dest_type>(*this);
		}

		virtual _connection_base0* duplicate(has_slots* pnewdest)
		{
			return new _connection0<dest_type>((dest_type *)pnewdest, m_pmemfun);
		}

		virtual void emit()
		{
			(m_pobject->*m_pmemfun)();
		}

		virtual has_slots* getdest() const
		{
			return m_pobject;
		}

	private:
		dest_type* m_pobject;
		void (dest_type::* m_pmemfun)();
	};

	template<class dest_type, class arg1_type>
	class _connection1 : public _connection_base1<arg1_type>
	{
	public:
		_connection1()
		{
			m_pobject = NULL;
			m_pmemfun = NULL;
		}

		_connection1(dest_type* pobject, void (dest_type::*pmemfun)(arg1_type))
		{
			m_pobject = pobject;
			m_pmemfun = pmemfun;
		}

		virtual _connection_base1<arg1_type>* clone()
		{
			return new _connection1<dest_type, arg1_type>(*this);
		}

		virtual _connection_base1<arg1_type>* duplicate(has_slots* pnewdest)
		{
			return new _connection1<dest_type, arg1_type>((dest_type *)pnewdest, m_pmemfun);
		}

		virtual void emit(arg1_type a1)
		{
			(m_pobject->*m_pmemfun)(a1);
		}

		virtual has_slots* getdest() const
		{
			return m_pobject;
		}

	private:
		dest_type* m_pobject;
		void (dest_type::* m_pmemfun)(arg1_type);
	};

	template<class dest_type, class arg1_type, class arg2_type>
	class _connection2 : public _connection_base2<arg1_type, arg2_type>
	{
	public:
		_connection2()
		{
			m_pobject = NULL;
			m_pmemfun = NULL;
		}

		_connection2(dest_type* pobject, void (dest_type::*pmemfun)(arg1_type,
			arg2_type))
		{
			m_pobject = pobject;
			m_pmemfun = pmemfun;
		}

		virtual _connection_base2<arg1_type, arg2_type>* clone()
		{
			return new _connection2<dest_type, arg1_type, arg2_type>(*this);
		}

		virtual _connection_base2<arg1_type, arg2_type>* duplicate(has_slots* pnewdest)
		{
			return new _connection2<dest_type, arg1_type, arg2_type>((dest_type *)pnewdest, m_pmemfun);
		}

		virtual void emit(arg1_type a1, arg2_type a2)
		{
			(m_pobject->*m_pmemfun)(a1, a2);
		}

		virtual has_slots* getdest() const
		{
			return m_pobject;
		}

	private:
		dest_type* m_pobject;
		void (dest_type::* m_pmemfun)(arg1_type, arg2_type);
	};

	template<class dest_type, class arg1_type, class arg2_type, class arg3_type>
	class _connection3 : public _connection_base3<arg1_type, arg2_type, arg3_type>
	{
	public:
		_connection3()
		{
			m_pobject = NULL;
			m_pmemfun = NULL;
		}

		_connection3(dest_type* pobject, void (dest_type::*pmemfun)(arg1_type,
			arg2_type, arg3_type))
		{
			m_pobject = pobject;
			m_pmemfun = pmemfun;
		}

		virtual _connection_base3<arg1_type, arg2_type, arg3_type>* clone()
		{
			return new _connection3<dest_type, arg1_type, arg2_type, arg3_type>(*this);
		}

		virtual _connection_base3<arg1_type, arg2_type, arg3_type>* duplicate(has_slots* pnewdest)
		{
			return new _connection3<dest_type, arg1_type, arg2_type, arg3_type>((dest_type *)pnewdest, m_pmemfun);
		}

		virtual void emit(arg1_type a1, arg2_type a2, arg3_type a3)
		{
			(m_pobject->*m_pmemfun)(a1, a2, a3);
		}

		virtual has_slots* getdest() const
		{
			return m_pobject;
		}

	private:
		dest_type* m_pobject;
		void (dest_type::* m_pmemfun)(arg1_type, arg2_type, arg3_type);
	};

	template<class dest_type, class arg1_type, class arg2_type, class arg3_type,
	class arg4_type>
	class _connection4 : public _connection_base4<arg1_type, arg2_type,
		arg3_type, arg4_type>
	{
	public:
		_connection4()
		{
			m_pobject = NULL;
			m_pmemfun = NULL;
		}

		_connection4(dest_type* pobject, void (dest_type::*pmemfun)(arg1_type,
			arg2_type, arg3_type, arg4_type))
		{
			m_pobject = pobject;
			m_pmemfun = pmemfun;
		}

		virtual _connection_base4<arg1_type, arg2_type, arg3_type, arg4_type>* clone()
		{
			return new _connection4<dest_type, arg1_type, arg2_type, arg3_type, arg4_type>(*this);
		}

		virtual _connection_base4<arg1_type, arg2_type, arg3_type, arg4_type>* duplicate(has_slots* pnewdest)
		{
			return new _connection4<dest_type, arg1_type, arg2_type, arg3_type, arg4_type>((dest_type *)pnewdest, m_pmemfun);
		}

		virtual void emit(arg1_type a1, arg2_type a2, arg3_type a3, 
			arg4_type a4)
		{
			(m_pobject->*m_pmemfun)(a1, a2, a3, a4);
		}

		virtual has_slots* getdest() const
		{
			return m_pobject;
		}

	private:
		dest_type* m_pobject;
		void (dest_type::* m_pmemfun)(arg1_type, arg2_type, arg3_type,
			arg4_type);
	};

	template<class dest_type, class arg1_type, class arg2_type, class arg3_type,
	class arg4_type, class arg5_type>
	class _connection5 : public _connection_base5<arg1_type, arg2_type,
		arg3_type, arg4_type, arg5_type>
	{
	public:
		_connection5()
		{
			m_pobject = NULL;
			m_pmemfun = NULL;
		}

		_connection5(dest_type* pobject, void (dest_type::*pmemfun)(arg1_type,
			arg2_type, arg3_type, arg4_type, arg5_type))
		{
			m_pobject = pobject;
			m_pmemfun = pmemfun;
		}

		virtual _connection_base5<arg1_type, arg2_type, arg3_type, arg4_type, 
			arg5_type>* clone()
		{
			return new _connection5<dest_type, arg1_type, arg2_type, arg3_type, arg4_type, 
				arg5_type>(*this);
		}

		virtual _connection_base5<arg1_type, arg2_type, arg3_type, arg4_type, 
			arg5_type>* duplicate(has_slots* pnewdest)
		{
			return new _connection5<dest_type, arg1_type, arg2_type, arg3_type, arg4_type, 
				arg5_type>((dest_type *)pnewdest, m_pmemfun);
		}

		virtual void emit(arg1_type a1, arg2_type a2, arg3_type a3, arg4_type a4,
			arg5_type a5)
		{
			(m_pobject->*m_pmemfun)(a1, a2, a3, a4, a5);
		}

		virtual has_slots* getdest() const
		{
			return m_pobject;
		}

	private:
		dest_type* m_pobject;
		void (dest_type::* m_pmemfun)(arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type);
	};

	template<class dest_type, class arg1_type, class arg2_type, class arg3_type,
	class arg4_type, class arg5_type, class arg6_type>
	class _connection6 : public _connection_base6<arg1_type, arg2_type,
		arg3_type, arg4_type, arg5_type, arg6_type>
	{
	public:
		_connection6()
		{
			m_pobject = NULL;
			m_pmemfun = NULL;
		}

		_connection6(dest_type* pobject, void (dest_type::*pmemfun)(arg1_type,
			arg2_type, arg3_type, arg4_type, arg5_type, arg6_type))
		{
			m_pobject = pobject;
			m_pmemfun = pmemfun;
		}

		virtual _connection_base6<arg1_type, arg2_type, arg3_type, arg4_type, 
			arg5_type, arg6_type>* clone()
		{
			return new _connection6<dest_type, arg1_type, arg2_type, arg3_type, arg4_type, 
				arg5_type, arg6_type>(*this);
		}

		virtual _connection_base6<arg1_type, arg2_type, arg3_type, arg4_type, 
			arg5_type, arg6_type>* duplicate(has_slots* pnewdest)
		{
			return new _connection6<dest_type, arg1_type, arg2_type, arg3_type, arg4_type, 
				arg5_type, arg6_type>((dest_type *)pnewdest, m_pmemfun);
		}

		virtual void emit(arg1_type a1, arg2_type a2, arg3_type a3, arg4_type a4,
			arg5_type a5, arg6_type a6)
		{
			(m_pobject->*m_pmemfun)(a1, a2, a3, a4, a5, a6);
		}

		virtual has_slots* getdest() const
		{
			return m_pobject;
		}

	private:
		dest_type* m_pobject;
		void (dest_type::* m_pmemfun)(arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type, arg6_type);
	};

	template<class dest_type, class arg1_type, class arg2_type, class arg3_type,
	class arg4_type, class arg5_type, class arg6_type, class arg7_type>
	class _connection7 : public _connection_base7<arg1_type, arg2_type,
		arg3_type, arg4_type, arg5_type, arg6_type, arg7_type>
	{
	public:
		_connection7()
		{
			m_pobject = NULL;
			m_pmemfun = NULL;
		}

		_connection7(dest_type* pobject, void (dest_type::*pmemfun)(arg1_type,
			arg2_type, arg3_type, arg4_type, arg5_type, arg6_type, arg7_type))
		{
			m_pobject = pobject;
			m_pmemfun = pmemfun;
		}

		virtual _connection_base7<arg1_type, arg2_type, arg3_type, arg4_type, 
			arg5_type, arg6_type, arg7_type>* clone()
		{
			return new _connection7<dest_type, arg1_type, arg2_type, arg3_type, arg4_type, 
				arg5_type, arg6_type, arg7_type>(*this);
		}

		virtual _connection_base7<arg1_type, arg2_type, arg3_type, arg4_type, 
			arg5_type, arg6_type, arg7_type>* duplicate(has_slots* pnewdest)
		{
			return new _connection7<dest_type, arg1_type, arg2_type, arg3_type, arg4_type, 
				arg5_type, arg6_type, arg7_type>((dest_type *)pnewdest, m_pmemfun);
		}

		virtual void emit(arg1_type a1, arg2_type a2, arg3_type a3, arg4_type a4,
			arg5_type a5, arg6_type a6, arg7_type a7)
		{
			(m_pobject->*m_pmemfun)(a1, a2, a3, a4, a5, a6, a7);
		}

		virtual has_slots* getdest() const
		{
			return m_pobject;
		}

	private:
		dest_type* m_pobject;
		void (dest_type::* m_pmemfun)(arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type, arg6_type, arg7_type);
	};

	template<class dest_type, class arg1_type, class arg2_type, class arg3_type,
	class arg4_type, class arg5_type, class arg6_type, class arg7_type, 
	class arg8_type>
	class _connection8 : public _connection_base8<arg1_type, arg2_type,
		arg3_type, arg4_type, arg5_type, arg6_type, arg7_type, arg8_type>
	{
	public:
		_connection8()
		{
			m_pobject = NULL;
			m_pmemfun = NULL;
		}

		_connection8(dest_type* pobject, void (dest_type::*pmemfun)(arg1_type,
			arg2_type, arg3_type, arg4_type, arg5_type, arg6_type, 
			arg7_type, arg8_type))
		{
			m_pobject = pobject;
			m_pmemfun = pmemfun;
		}

		virtual _connection_base8<arg1_type, arg2_type, arg3_type, arg4_type, 
			arg5_type, arg6_type, arg7_type, arg8_type>* clone()
		{
			return new _connection8<dest_type, arg1_type, arg2_type, arg3_type, arg4_type, 
				arg5_type, arg6_type, arg7_type, arg8_type>(*this);
		}

		virtual _connection_base8<arg1_type, arg2_type, arg3_type, arg4_type, 
			arg5_type, arg6_type, arg7_type, arg8_type>* duplicate(has_slots* pnewdest)
		{
			return new _connection8<dest_type, arg1_type, arg2_type, arg3_type, arg4_type, 
				arg5_type, arg6_type, arg7_type, arg8_type>((dest_type *)pnewdest, m_pmemfun);
		}

		virtual void emit(arg1_type a1, arg2_type a2, arg3_type a3, arg4_type a4,
			arg5_type a5, arg6_type a6, arg7_type a7, arg8_type a8)
		{
			(m_pobject->*m_pmemfun)(a1, a2, a3, a4, a5, a6, a7, a8);
		}

		virtual has_slots* getdest() const
		{
			return m_pobject;
		}

	private:
		dest_type* m_pobject;
		void (dest_type::* m_pmemfun)(arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type, arg6_type, arg7_type, arg8_type);
	};

	class signal0 : public _signal_base0
	{
	public:
		signal0()
		{
			;
		}

		signal0(const signal0& s)
			: _signal_base0(s)
		{
			;
		}

		template<class desttype>
			void connect(desttype* pclass, void (desttype::*pmemfun)())
		{
			_connection0<desttype>* conn = 
				new _connection0<desttype>(pclass, pmemfun);
			m_connected_slots.push_back(conn);
			pclass->signal_connect(this);
		}

		void emit()
		{
			connections_list::const_iterator itNext, it = m_connected_slots.begin();
			connections_list::const_iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				itNext = it;
				++itNext;

				(*it)->emit();

				it = itNext;
			}
		}

		void operator()()
		{
			connections_list::const_iterator itNext, it = m_connected_slots.begin();
			connections_list::const_iterator itEnd = m_connected_slots.end();

			while(it != itEnd)
			{
				itNext = it;
				++itNext;

				(*it)->emit();

				it = itNext;
			}
		}
	};

	template<class arg1_type>
	class signal1 : public _signal_base1<arg1_type>
	{
	public:
		signal1()
		{
			;
		}

		signal1(const signal1<arg1_type>& s)
			: _signal_base1<arg1_type>(s)
		{
			;
		}

		template<class desttype>
			void connect(desttype* pclass, void (desttype::*pmemfun)(arg1_type))
		{
			_connection1<desttype, arg1_type>* conn = 
				new _connection1<desttype, arg1_type>(pclass, pmemfun);
			_signal_base1<arg1_type>::m_connected_slots.push_back(conn);
			pclass->signal_connect(this);
		}

		void emit(arg1_type a1)
		{
			typename _signal_base1<arg1_type>::connections_list::const_iterator itNext, it = _signal_base1<arg1_type>::m_connected_slots.begin();
			typename _signal_base1<arg1_type>::connections_list::const_iterator itEnd = _signal_base1<arg1_type>::m_connected_slots.end();

			while(it != itEnd)
			{
				itNext = it;
				++itNext;

				(*it)->emit(a1);

				it = itNext;
			}
		}

		void operator()(arg1_type a1)
		{
			typename _signal_base1<arg1_type>::connections_list::const_iterator itNext, it = _signal_base1<arg1_type>::m_connected_slots.begin();
			typename _signal_base1<arg1_type>::connections_list::const_iterator itEnd = _signal_base1<arg1_type>::m_connected_slots.end();

			while(it != itEnd)
			{
				itNext = it;
				++itNext;

				(*it)->emit(a1);

				it = itNext;
			}
		}
	};

	template<class arg1_type, class arg2_type>
	class signal2 : public _signal_base2<arg1_type, arg2_type>
	{
	public:
		signal2()
		{
			;
		}

		signal2(const signal2<arg1_type, arg2_type>& s)
			: _signal_base2<arg1_type, arg2_type>(s)
		{
			;
		}

		template<class desttype>
			void connect(desttype* pclass, void (desttype::*pmemfun)(arg1_type,
			arg2_type))
		{
			_connection2<desttype, arg1_type, arg2_type>* conn = new
				_connection2<desttype, arg1_type, arg2_type>(pclass, pmemfun);
			_signal_base2<arg1_type, arg2_type>::m_connected_slots.push_back(conn);
			pclass->signal_connect(this);
		}

		void emit(arg1_type a1, arg2_type a2)
		{
			typename _signal_base2<arg1_type, arg2_type>::connections_list::const_iterator itNext, it = _signal_base2<arg1_type, arg2_type>::m_connected_slots.begin();
			typename _signal_base2<arg1_type, arg2_type>::connections_list::const_iterator itEnd = _signal_base2<arg1_type, arg2_type>::m_connected_slots.end();

			while(it != itEnd)
			{
				itNext = it;
				++itNext;

				(*it)->emit(a1, a2);

				it = itNext;
			}
		}

		void operator()(arg1_type a1, arg2_type a2)
		{
			typename _signal_base2<arg1_type, arg2_type>::connections_list::const_iterator itNext, it = _signal_base2<arg1_type, arg2_type>::m_connected_slots.begin();
			typename _signal_base2<arg1_type, arg2_type>::connections_list::const_iterator itEnd = _signal_base2<arg1_type, arg2_type>::m_connected_slots.end();

			while(it != itEnd)
			{
				itNext = it;
				++itNext;

				(*it)->emit(a1, a2);

				it = itNext;
			}
		}
	};

	template<class arg1_type, class arg2_type, class arg3_type>
	class signal3 : public _signal_base3<arg1_type, arg2_type, arg3_type>
	{
	public:
		signal3()
		{
			;
		}

		signal3(const signal3<arg1_type, arg2_type, arg3_type>& s)
			: _signal_base3<arg1_type, arg2_type, arg3_type>(s)
		{
			;
		}

		template<class desttype>
			void connect(desttype* pclass, void (desttype::*pmemfun)(arg1_type,
			arg2_type, arg3_type))
		{
			_connection3<desttype, arg1_type, arg2_type, arg3_type>* conn = 
				new _connection3<desttype, arg1_type, arg2_type, arg3_type>(pclass,
				pmemfun);
			_signal_base3<arg1_type, arg2_type, arg3_type>::m_connected_slots.push_back(conn);
			pclass->signal_connect(this);
		}

		void emit(arg1_type a1, arg2_type a2, arg3_type a3)
		{
			typename _signal_base3<arg1_type, arg2_type, arg3_type>::connections_list::const_iterator itNext, it = _signal_base3<arg1_type, arg2_type, arg3_type>::m_connected_slots.begin();
			typename _signal_base3<arg1_type, arg2_type, arg3_type>::connections_list::const_iterator itEnd = _signal_base3<arg1_type, arg2_type, arg3_type>::m_connected_slots.end();

			while(it != itEnd)
			{
				itNext = it;
				++itNext;

				(*it)->emit(a1, a2, a3);

				it = itNext;
			}
		}

		void operator()(arg1_type a1, arg2_type a2, arg3_type a3)
		{
			typename _signal_base3<arg1_type, arg2_type, arg3_type>::connections_list::const_iterator itNext, it = this->m_connected_slots.begin();
			typename _signal_base3<arg1_type, arg2_type, arg3_type>::connections_list::const_iterator itEnd = this->m_connected_slots.end();

			while(it != itEnd)
			{
				itNext = it;
				++itNext;

				(*it)->emit(a1, a2, a3);

				it = itNext;
			}
		}
	};

	template<class arg1_type, class arg2_type, class arg3_type, class arg4_type>
	class signal4 : public _signal_base4<arg1_type, arg2_type, arg3_type,
		arg4_type>
	{
	public:
		signal4()
		{
			;
		}

		signal4(const signal4<arg1_type, arg2_type, arg3_type, arg4_type>& s)
			: _signal_base4<arg1_type, arg2_type, arg3_type, arg4_type>(s)
		{
			;
		}

		template<class desttype>
			void connect(desttype* pclass, void (desttype::*pmemfun)(arg1_type,
			arg2_type, arg3_type, arg4_type))
		{
			_connection4<desttype, arg1_type, arg2_type, arg3_type, arg4_type>*
				conn = new _connection4<desttype, arg1_type, arg2_type, arg3_type,
				arg4_type>(pclass, pmemfun);
			this->m_connected_slots.push_back(conn);
			pclass->signal_connect(this);
		}

		void emit(arg1_type a1, arg2_type a2, arg3_type a3, arg4_type a4)
		{
			typename _signal_base4<arg1_type, arg2_type, arg3_type, arg4_type>::connections_list::const_iterator itNext, it = this->m_connected_slots.begin();
			typename _signal_base4<arg1_type, arg2_type, arg3_type, arg4_type>::connections_list::const_iterator itEnd = this->m_connected_slots.end();

			while(it != itEnd)
			{
				itNext = it;
				++itNext;

				(*it)->emit(a1, a2, a3, a4);

				it = itNext;
			}
		}

		void operator()(arg1_type a1, arg2_type a2, arg3_type a3, arg4_type a4)
		{
			typename _signal_base4<arg1_type, arg2_type, arg3_type, arg4_type>::connections_list::const_iterator itNext, it = this->m_connected_slots.begin();
			typename _signal_base4<arg1_type, arg2_type, arg3_type, arg4_type>::connections_list::const_iterator itEnd = this->m_connected_slots.end();

			while(it != itEnd)
			{
				itNext = it;
				++itNext;

				(*it)->emit(a1, a2, a3, a4);

				it = itNext;
			}
		}
	};

	template<class arg1_type, class arg2_type, class arg3_type, class arg4_type,
	class arg5_type>
	class signal5 : public _signal_base5<arg1_type, arg2_type, arg3_type,
		arg4_type, arg5_type>
	{
	public:
		signal5()
		{
			;
		}

		signal5(const signal5<arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type>& s)
			: _signal_base5<arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type>(s)
		{
			;
		}

		template<class desttype>
			void connect(desttype* pclass, void (desttype::*pmemfun)(arg1_type,
			arg2_type, arg3_type, arg4_type, arg5_type))
		{
			_connection5<desttype, arg1_type, arg2_type, arg3_type, arg4_type,
				arg5_type>* conn = new _connection5<desttype, arg1_type, arg2_type,
				arg3_type, arg4_type, arg5_type>(pclass, pmemfun);
			this->m_connected_slots.push_back(conn);
			pclass->signal_connect(this);
		}

		void emit(arg1_type a1, arg2_type a2, arg3_type a3, arg4_type a4,
			arg5_type a5)
		{
			typename _signal_base5<arg1_type, arg2_type, arg3_type, arg4_type, arg5_type>::connections_list::const_iterator itNext, it = this->m_connected_slots.begin();
			typename _signal_base5<arg1_type, arg2_type, arg3_type, arg4_type, arg5_type>::connections_list::const_iterator itEnd = this->m_connected_slots.end();

			while(it != itEnd)
			{
				itNext = it;
				++itNext;

				(*it)->emit(a1, a2, a3, a4, a5);

				it = itNext;
			}
		}

		void operator()(arg1_type a1, arg2_type a2, arg3_type a3, arg4_type a4,
			arg5_type a5)
		{
			typename _signal_base5<arg1_type, arg2_type, arg3_type, arg4_type, arg5_type>::connections_list::const_iterator itNext, it = this->m_connected_slots.begin();
			typename _signal_base5<arg1_type, arg2_type, arg3_type, arg4_type, arg5_type>::connections_list::const_iterator itEnd = this->m_connected_slots.end();

			while(it != itEnd)
			{
				itNext = it;
				++itNext;

				(*it)->emit(a1, a2, a3, a4, a5);

				it = itNext;
			}
		}
	};


	template<class arg1_type, class arg2_type, class arg3_type, class arg4_type,
	class arg5_type, class arg6_type>
	class signal6 : public _signal_base6<arg1_type, arg2_type, arg3_type,
		arg4_type, arg5_type, arg6_type>
	{
	public:
		signal6()
		{
			;
		}

		signal6(const signal6<arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type, arg6_type>& s)
			: _signal_base6<arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type, arg6_type>(s)
		{
			;
		}

		template<class desttype>
			void connect(desttype* pclass, void (desttype::*pmemfun)(arg1_type,
			arg2_type, arg3_type, arg4_type, arg5_type, arg6_type))
		{
			_connection6<desttype, arg1_type, arg2_type, arg3_type, arg4_type,
				arg5_type, arg6_type>* conn = 
				new _connection6<desttype, arg1_type, arg2_type, arg3_type,
				arg4_type, arg5_type, arg6_type>(pclass, pmemfun);
			this->m_connected_slots.push_back(conn);
			pclass->signal_connect(this);
		}

		void emit(arg1_type a1, arg2_type a2, arg3_type a3, arg4_type a4,
			arg5_type a5, arg6_type a6)
		{
			typename _signal_base6<arg1_type, arg2_type, arg3_type, arg4_type, arg5_type, arg6_type>::connections_list::const_iterator itNext, it = this->m_connected_slots.begin();
			typename _signal_base6<arg1_type, arg2_type, arg3_type, arg4_type, arg5_type, arg6_type>::connections_list::const_iterator itEnd = this->m_connected_slots.end();

			while(it != itEnd)
			{
				itNext = it;
				++itNext;

				(*it)->emit(a1, a2, a3, a4, a5, a6);

				it = itNext;
			}
		}

		void operator()(arg1_type a1, arg2_type a2, arg3_type a3, arg4_type a4,
			arg5_type a5, arg6_type a6)
		{
			typename _signal_base6<arg1_type, arg2_type, arg3_type, arg4_type, arg5_type, arg6_type>::connections_list::const_iterator itNext, it = this->m_connected_slots.begin();
			typename _signal_base6<arg1_type, arg2_type, arg3_type, arg4_type, arg5_type, arg6_type>::connections_list::const_iterator itEnd = this->m_connected_slots.end();

			while(it != itEnd)
			{
				itNext = it;
				++itNext;

				(*it)->emit(a1, a2, a3, a4, a5, a6);

				it = itNext;
			}
		}
	};

	template<class arg1_type, class arg2_type, class arg3_type, class arg4_type,
	class arg5_type, class arg6_type, class arg7_type>
	class signal7 : public _signal_base7<arg1_type, arg2_type, arg3_type,
		arg4_type, arg5_type, arg6_type, arg7_type>
	{
	public:
		signal7()
		{
			;
		}

		signal7(const signal7<arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type, arg6_type, arg7_type>& s)
			: _signal_base7<arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type, arg6_type, arg7_type>(s)
		{
			;
		}

		template<class desttype>
			void connect(desttype* pclass, void (desttype::*pmemfun)(arg1_type,
			arg2_type, arg3_type, arg4_type, arg5_type, arg6_type, 
			arg7_type))
		{
			_connection7<desttype, arg1_type, arg2_type, arg3_type, arg4_type,
				arg5_type, arg6_type, arg7_type>* conn = 
				new _connection7<desttype, arg1_type, arg2_type, arg3_type,
				arg4_type, arg5_type, arg6_type, arg7_type>(pclass, pmemfun);
			this->m_connected_slots.push_back(conn);
			pclass->signal_connect(this);
		}

		void emit(arg1_type a1, arg2_type a2, arg3_type a3, arg4_type a4,
			arg5_type a5, arg6_type a6, arg7_type a7)
		{
			typename _signal_base7<arg1_type, arg2_type, arg3_type, arg4_type, arg5_type, arg6_type, arg7_type>::connections_list::const_iterator itNext, it = this->m_connected_slots.begin();
			typename _signal_base7<arg1_type, arg2_type, arg3_type, arg4_type, arg5_type, arg6_type, arg7_type>::connections_list::const_iterator itEnd = this->m_connected_slots.end();

			while(it != itEnd)
			{
				itNext = it;
				++itNext;

				(*it)->emit(a1, a2, a3, a4, a5, a6, a7);

				it = itNext;
			}
		}

		void operator()(arg1_type a1, arg2_type a2, arg3_type a3, arg4_type a4,
			arg5_type a5, arg6_type a6, arg7_type a7)
		{
			typename _signal_base7<arg1_type, arg2_type, arg3_type, arg4_type, arg5_type, arg6_type, arg7_type>::connections_list::const_iterator itNext, it = this->m_connected_slots.begin();
			typename _signal_base7<arg1_type, arg2_type, arg3_type, arg4_type, arg5_type, arg6_type, arg7_type>::connections_list::const_iterator itEnd = this->m_connected_slots.end();

			while(it != itEnd)
			{
				itNext = it;
				++itNext;

				(*it)->emit(a1, a2, a3, a4, a5, a6, a7);

				it = itNext;
			}
		}
	};

	template<class arg1_type, class arg2_type, class arg3_type, class arg4_type,
	class arg5_type, class arg6_type, class arg7_type, class arg8_type>
	class signal8 : public _signal_base8<arg1_type, arg2_type, arg3_type,
		arg4_type, arg5_type, arg6_type, arg7_type, arg8_type>
	{
	public:
		signal8()
		{
			;
		}

		signal8(const signal8<arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type, arg6_type, arg7_type, arg8_type>& s)
			: _signal_base8<arg1_type, arg2_type, arg3_type, arg4_type,
			arg5_type, arg6_type, arg7_type, arg8_type>(s)
		{
			;
		}

		template<class desttype>
			void connect(desttype* pclass, void (desttype::*pmemfun)(arg1_type,
			arg2_type, arg3_type, arg4_type, arg5_type, arg6_type, 
			arg7_type, arg8_type))
		{
			_connection8<desttype, arg1_type, arg2_type, arg3_type, arg4_type,
				arg5_type, arg6_type, arg7_type, arg8_type>* conn = 
				new _connection8<desttype, arg1_type, arg2_type, arg3_type,
				arg4_type, arg5_type, arg6_type, arg7_type, 
				arg8_type>(pclass, pmemfun);
			this->m_connected_slots.push_back(conn);
			pclass->signal_connect(this);
		}

		void emit(arg1_type a1, arg2_type a2, arg3_type a3, arg4_type a4,
			arg5_type a5, arg6_type a6, arg7_type a7, arg8_type a8)
		{
			typename _signal_base8<arg1_type, arg2_type, arg3_type, arg4_type, arg5_type, arg6_type, arg7_type, arg8_type>::connections_list::const_iterator itNext, it = this->m_connected_slots.begin();
			typename _signal_base8<arg1_type, arg2_type, arg3_type, arg4_type, arg5_type, arg6_type, arg7_type, arg8_type>::connections_list::const_iterator itEnd = this->m_connected_slots.end();

			while(it != itEnd)
			{
				itNext = it;
				++itNext;

				(*it)->emit(a1, a2, a3, a4, a5, a6, a7, a8);

				it = itNext;
			}
		}

		void operator()(arg1_type a1, arg2_type a2, arg3_type a3, arg4_type a4,
			arg5_type a5, arg6_type a6, arg7_type a7, arg8_type a8)
		{
			typename _signal_base8<arg1_type, arg2_type, arg3_type, arg4_type, arg5_type, arg6_type, arg7_type, arg8_type>::connections_list::const_iterator itNext, it = this->m_connected_slots.begin();
			typename _signal_base8<arg1_type, arg2_type, arg3_type, arg4_type, arg5_type, arg6_type, arg7_type, arg8_type>::connections_list::const_iterator itEnd = this->m_connected_slots.end();

			while(it != itEnd)
			{
				itNext = it;
				++itNext;

				(*it)->emit(a1, a2, a3, a4, a5, a6, a7, a8);

				it = itNext;
			}
		}
	};

}; // namespace sigslot

namespace ww{
	using sigslot::has_slots;
	using sigslot::signal0;
	using sigslot::signal1;
	using sigslot::signal2;
	using sigslot::signal3;
	using sigslot::signal4;
	using sigslot::signal5;
	using sigslot::signal6;
	using sigslot::signal7;
	using sigslot::signal8;
};

#endif // SIGSLOT_H__


