// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <CryThreading/IJobManager.h>
#include <CryString/CompileTimeString.h>
#include <CrySTL/utility.h>
#include <CrySTL/type_traits.h>
#include <CryCore/StlUtils.h>
#include <CryCore/SmallFunction.h>
#include <tuple>

namespace JobManager {
namespace Detail {

//This type def is necessary post VS 15.9 due to a bug that would otherwise report "error C2066: cast to function type is illegal"
using DefaultLambdaFunctionSig = void();

///////////////////////////////////////////////////////////////////////////////
// Utilities to extract tuple
template<typename F, typename Tuple, size_t ... I>
void apply_impl(F&& f, Tuple&& tup, stl::index_sequence<I ...> )
{
	f(std::get<I>(std::forward<Tuple>(tup)) ...);
}

//! Invokes the callable object on tuple, treating the tuple as parameters
template<typename F, typename Tuple>
void apply(F&& f, Tuple&& tup)
{
	apply_impl(std::forward<F>(f), std::forward<Tuple>(tup), stl::make_index_sequence<std::tuple_size<typename std::remove_reference<Tuple>::type>::value> {});
}

template<std::size_t Size, std::size_t Align>
struct SSizeIsAligned
{
	static_assert((Align & (Align - 1)) == 0, "Invalid alignment size");
	constexpr static bool value = ((Size & (Align - 1)) == 0);
};

//! Job functor for class member / free functions
//! Takes the instance of function pointer at compile time for optimized performance and memory usage.
//! Stores job data on creation, and passes data to the wrapped function when called.
template<
	typename FuncPtrType, /*Can be pointer to free or member functions*/
	FuncPtrType Func
	>
struct SJobFunction;

//! Specialization for free functions
template<typename R, typename ... T, R(* Func)(T ...)>
struct CRY_ALIGN(128) SJobFunction<R (*)(T ...), Func>
{
	template<typename ... U>
	struct SAcceptsParams
	{
		static constexpr bool value = sizeof ... (T) == sizeof ... (U) && stl::conjunction<std::is_convertible<U, T> ...>::value;
	};

	template<typename ... U>
	SJobFunction(U&& ... args) : m_Params(std::forward<U>(args)...)
	{
		static_assert(SAcceptsParams<U...>::value, "Incompatible parameters passed to the job");
	}

	//! Stores the serialized parameters.
	CRY_ALIGN(16) std::tuple<typename std::remove_reference<T>::type ...> m_Params;

	//! Calls the underlying function and discards the return value (if any)
	void operator()()
	{
		JobManager::Detail::apply(Func, m_Params);
	} 
};

//! Specialization for member functions
template<class C, typename R, typename ... T, R(C::* Func)(T ...)>
struct CRY_ALIGN(128) SJobFunction<R (C::*)(T ...), Func>
{
	template<typename ... U>
	struct SAcceptsParams
	{
		static constexpr bool value = sizeof ... (T) == sizeof ... (U) && stl::conjunction<std::is_convertible<U, T> ...>::value;
	};

	template<typename ... U>
	SJobFunction(U&& ... args) : m_Params(std::forward<U>(args)...)
	{
		static_assert(SAcceptsParams<U...>::value, "Incompatible parameters passed to the job");
	}

	C* m_pInstance = nullptr;

	//! Stores the serialized parameters.
	CRY_ALIGN(16) std::tuple<typename std::remove_reference<T>::type ...> m_Params;

	//! Calls the underlying function and discards the return value (if any)
	void operator()()
	{
		CRY_ASSERT_MESSAGE(m_pInstance != nullptr, "Missing this pointer to jobmanager call");
		JobManager::Detail::apply([this](const typename std::remove_reference<T>::type& ... t) { (m_pInstance->*Func)(t ...); }, m_Params);
	} 
};

//! Job functor for lambdas
//! Specifies lambda signature at compile time.
//! On creation, binds all job data with the lambda so that it is callable without any parameter.
template<typename Signature = ::JobManager::Detail::DefaultLambdaFunctionSig>
struct SJobLambdaFunction;

template<typename R, typename ... T>
struct SJobLambdaFunction<R(T ...)>
{
	template<typename F, typename ... U>
	struct SAcceptsParams
	{
		static constexpr bool value = sizeof ... (T) == sizeof ... (U) && stl::conjunction<std::is_convertible<U, T> ...>::value;
	};

	template<typename F, typename ... U>
	SJobLambdaFunction(F&& function, U&& ... args)
		: m_Function([function, args ...] //copy intended
			{
				function(args ...);
			})
	{
		static_assert(SAcceptsParams<F, U...>::value, "Incompatible parameters passed to the job");
	}

	//! Calls the underlying function and discards the return value (if any)
	void operator()()
	{
		m_Function();
	}

	CRY_ALIGN(16) SmallFunction<void(), 128U> m_Function;
};

//! Generic Job interface.
//! Accepts job data on creation and stores them into the associated function object.
//! Responsible for invoking the associated function.
template<class TFuncType, const char* Name = nullptr>
class CRY_ALIGN(128) CGenericJob: public CJobBase
{
public:

	template<typename TargetType, typename ... Types>
	static constexpr bool AppearsInAnyOf()
	{
		return stl::disjunction< std::is_same<TargetType, Types>... >::value;
	}

	//! Generic constructor when the job name is bound at compile time.
	//! Takes any parameter and constructs the underlying functor.
	//! The SFINAE check AppearsInAnyOf is essential so that implicitly copy-constructing from another non-const
	//! CGenericJob object would not go in here, but into the deleted copy constructor.
	template<typename ... T, 
		const char* TName = Name, // Dependent non-type parameter is necessary for mutual exclusive SFINAE to work
		typename = typename std::enable_if<
			TName != nullptr &&
			!AppearsInAnyOf<CGenericJob, typename std::decay<T>::type...>()
		>::type
	>
	CGenericJob(T... t) // by value because we are going to store them anyway
	{
		static_assert(TFuncType::template SAcceptsParams<T ...>::value, "Incompatible parameters passed to the job");

		// Creates the function object in the buffer, which at later point gets bulk-copied to the job system
		// without exposing the concrete type information. For that we use a fix sized buffer and placement new.
		// The data are destructed after invocation, see void Invoke(void* p)
		new(&m_Storage)TFuncType(std::move(t)...);
		MEMORY_RW_REORDERING_BARRIER;
		m_JobDelegator.SetParamDataSize(sizeof(CGenericJob::m_Storage));
		m_JobDelegator.SetJobParamData((void*)&m_Storage);
		m_JobDelegator.SetDelegator(Invoke);
		/*Safe, since GetJobHandle itself is thread safe and should always return the same result for a job */
		SetJobProgramData(gEnv->GetJobManager()->GetJobHandle(Name, Invoke));
	}

	//! Generic constructor when the job name is not available at compile time.
	//! Takes a job name string and any further parameter to construct the underlying functor.
	//! Does not check AppearsInAnyOf as the first parameter prevents the function from taking priority 
	//! over the copy constructor.
	template<typename ... T,
		const char* TName = Name, // Dependent non-type parameter is necessary for mutual exclusive SFINAE to work
		typename = typename std::enable_if<TName == nullptr>::type
	>
	CGenericJob(const char* szName, T... t)
	{
		static_assert(TFuncType::template SAcceptsParams<T ...>::value, "Incompatible parameters passed to the job");
		new(&m_Storage)TFuncType(std::move(t)...);
		MEMORY_RW_REORDERING_BARRIER;
		m_JobDelegator.SetParamDataSize(sizeof(CGenericJob::m_Storage));
		m_JobDelegator.SetJobParamData((void*)&m_Storage);
		m_JobDelegator.SetDelegator(Invoke);
		SetJobProgramData(gEnv->GetJobManager()->GetJobHandle(szName, Invoke));
	}

	~CGenericJob() = default;

	//! A job represents a unique task under the name and cannot be copied
	CGenericJob(const CGenericJob&) = delete;

	//! Move constructor. This is necessary for supporting initialization from single argument in the form of T a = b;
	CGenericJob(CGenericJob&& other) noexcept
		: CJobBase(std::move(other))
		, m_Storage(other.m_Storage)
	{
		memset(&other.m_Storage, 0, sizeof(other.m_Storage));
		m_JobDelegator.SetJobParamData((void*)&m_Storage);
	}
	CGenericJob& operator= (const CGenericJob&) = delete;
	CGenericJob& operator= (CGenericJob&&) = delete;

	using CJobBase::Run;

	ILINE void Run(unsigned int priority)
	{
		SetPriorityLevel(priority);
		Run();
	}

	ILINE void Run(unsigned int priority, JobManager::SJobState* __restrict pJobState)
	{
		SetPriorityLevel(priority);
		RegisterJobState(pJobState);
		Run();
	}

	//Detector for TFuncType::m_pInstance
	template<class T> using HasPInstanceDetector = decltype(std::declval<T>().m_pInstance);

	template<class C>
	void SetClassInstance(C* pClassInstance)
	{
		static_assert(stl::is_detected<HasPInstanceDetector, TFuncType>::value, "SetClassInstance is unsupported for free-function jobs");
		reinterpret_cast<TFuncType*>(&m_Storage)->m_pInstance = pClassInstance;
	}

	void ForceUpdateOfProfilingDataIndex()
	{
		this->m_JobDelegator.ForceUpdateOfProfilingDataIndex();
	}

	void SetPriorityLevel(unsigned int nPriorityLevel)
	{
		this->m_JobDelegator.SetPriorityLevel(nPriorityLevel);
	}

	void SetBlocking()
	{
		this->m_JobDelegator.SetBlocking();
	}

	/* overloaded new/delete operator to ensure aligned memory when doing a heap allocation */
	void* operator new(size_t nSize)        { return CryModuleMemalign(nSize, 128); }
	void  operator delete(void* pPtr)       { CryModuleMemalignFree(pPtr); }
	void* operator new(size_t, void* where) { return where; }

private:

	/* The invoker function is responsible to de-serialize the parameters passed into a job. */
	static void Invoke(void* p)
	{
		TFuncType* pFunc = reinterpret_cast<TFuncType*>(p);
		(*pFunc)();
		pFunc->~TFuncType();
	}

private:

	typedef typename std::aligned_storage<sizeof(TFuncType), 128>::type StorageType;
	StorageType m_Storage;
	static_assert(sizeof(StorageType) <= SInfoBlock::scAvailParamSize, "Too large parameter stack for the job function.");
	static_assert(SSizeIsAligned<sizeof(StorageType), 16U>::value, "Size must be factor of 16"); 
};
} // namespace Detail
} // namespace JobManager

//! Defines a job class to hold a free function or member function callback.
#define DECLARE_JOB(NAME, TYPE, FUNCTION)                                                                             \
	typedef ::JobManager::Detail::CGenericJob<::JobManager::Detail::SJobFunction<decltype(& FUNCTION), (& FUNCTION)>, \
												COMPILE_TIME_STRING(NAME)>                                            \
	  TYPE;

//! Defines a job class to hold a lambda callback.
//!
//! Sets the job to accept a lambda without parameter, equivalent to void().
//! You can specify void() as the last parameter but it is optional.
//! DECLARE_LAMBDA_JOB("Example", ExampleJob);
//!
//! The signature can be specified and the lambda parameters must match the signature.
//! DECLARE_LAMBDA_JOB("Example2", ExampleJob2, void(int));
//!
//! The usage:
//! ExampleJob job = [] { /* stuff */ };
#define DECLARE_LAMBDA_JOB(NAME, TYPE, ...)                                                          \
	typedef ::JobManager::Detail::CGenericJob<::JobManager::Detail::SJobLambdaFunction<__VA_ARGS__>, \
												COMPILE_TIME_STRING(NAME)>                           \
	  TYPE;
