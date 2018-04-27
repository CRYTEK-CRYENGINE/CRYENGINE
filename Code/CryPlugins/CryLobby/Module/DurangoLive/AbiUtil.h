// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#define CX_TYPEDEF(Interface_) \
  typedef __ ## Interface_ ## PublicNonVirtuals Interface_

#define CX_TYPEDEF_F(Interface_)                                \
  typedef __ ## Interface_ ## PublicNonVirtuals     Interface_; \
  typedef __ ## Interface_ ## Factory Interface_ ## Factory

#define CX_TYPEDEF_S(Interface_)                                \
  typedef __ ## Interface_ ## PublicNonVirtuals     Interface_; \
  typedef __ ## Interface_ ## Statics Interface_ ## Statics

#define CX_TYPEDEF_FS(Interface_)                               \
  typedef __ ## Interface_ ## PublicNonVirtuals     Interface_; \
  typedef __ ## Interface_ ## Factory Interface_ ## Factory;    \
  typedef __ ## Interface_ ## Statics Interface_ ## Statics

//
template<typename Ty>
struct InterfaceInfo
{
};

// Probably need to redo these again to build the different info structs out of the ABI compound type mapping to
// account for the possibility of multiple statics types per runtimetype

#define USING_RTC_BLOCK(RuntimeClass_, Interface_)                           \
  static LPCWSTR Name;                                                       \
  typedef     RuntimeClass_                            RuntimeClass;         \
  typedef     Interface_                               Interface;            \
  typedef     InterfaceInfo<RuntimeClass_>             Info;                 \
  typedef ::Microsoft::WRL::ComPtr<Interface_>         InterfacePtr;         \
  typedef ::Microsoft::WRL::ComPtr<IActivationFactory> ActivationFactoryPtr; \
  static ActivationFactoryPtr ActivationFactory_;                            \
  static HRESULT ActivationFactoryInitializationResult_;                     \
  static std::once_flag ActivationFactoryInitializationFlag_;                \
  static PFNGETACTIVATIONFACTORY ActivationFactoryFunc_

#define USING_RTC_F_BLOCK(Interface_)                            \
  typedef     Interface_ ## Factory         Factory;             \
  typedef ::Microsoft::WRL::ComPtr<Factory> InterfaceFactoryPtr; \
  static InterfaceFactoryPtr InterfaceFactory_;                  \
  static HRESULT InterfaceFactoryInitializationResult_;          \
  static std::once_flag InterfaceFactoryInitializationFlag_

#define USING_RTC_S_BLOCK(Interface_)                            \
  typedef     Interface_ ## Statics         Statics;             \
  typedef ::Microsoft::WRL::ComPtr<Statics> InterfaceStaticsPtr; \
  static InterfaceStaticsPtr InterfaceStatics_;                  \
  static HRESULT InterfaceStaticsInitializationResult_;          \
  static std::once_flag InterfaceStaticsInitializationFlag_

#define USING_RTC_F_INFO(RuntimeClass_)                       \
  template<>                                                  \
  struct InterfaceInfo<InterfaceInfo<RuntimeClass_>::Factory> \
  {                                                           \
    typedef     InterfaceInfo<RuntimeClass_> Info;            \
  }

#define USING_RTC_S_INFO(RuntimeClass_)                       \
  template<>                                                  \
  struct InterfaceInfo<InterfaceInfo<RuntimeClass_>::Statics> \
  {                                                           \
    typedef     InterfaceInfo<RuntimeClass_> Info;            \
  }

#define USING_RTC_B_INFO(RuntimeClass_, Interface_) \
  template<>                                        \
  struct InterfaceInfo<Interface_>                  \
  {                                                 \
    typedef     InterfaceInfo<RuntimeClass_> Info;  \
  }

#define USING_RTC(RuntimeClass_, Interface_)    \
  template<>                                    \
  struct InterfaceInfo<RuntimeClass_>           \
  {                                             \
    USING_RTC_BLOCK(RuntimeClass_, Interface_); \
  }

#define USING_RTC_F(RuntimeClass_, Interface_)  \
  template<>                                    \
  struct InterfaceInfo<RuntimeClass_>           \
  {                                             \
    USING_RTC_BLOCK(RuntimeClass_, Interface_); \
    USING_RTC_F_BLOCK(Interface_);              \
  };                                            \
                                                \
  USING_RTC_F_INFO(RuntimeClass_)

#define USING_RTC_S(RuntimeClass_, Interface_)  \
  template<>                                    \
  struct InterfaceInfo<RuntimeClass_>           \
  {                                             \
    USING_RTC_BLOCK(RuntimeClass_, Interface_); \
    USING_RTC_S_BLOCK(Interface_);              \
  };                                            \
                                                \
  USING_RTC_S_INFO(RuntimeClass_)

#define USING_RTC_FS(RuntimeClass_, Interface_) \
  template<>                                    \
  struct InterfaceInfo<RuntimeClass_>           \
  {                                             \
    USING_RTC_BLOCK(RuntimeClass_, Interface_); \
    USING_RTC_F_BLOCK(Interface_);              \
    USING_RTC_S_BLOCK(Interface_);              \
  };                                            \
                                                \
  USING_RTC_F_INFO(RuntimeClass_);              \
  USING_RTC_S_INFO(RuntimeClass_)

#define USING_RTC_B(RuntimeClass_, Interface_) \
  USING_RTC(RuntimeClass_, Interface_);        \
  USING_RTC_B_INFO(RuntimeClass_, Interface_)

#define USING_RTC_F_B(RuntimeClass_, Interface_) \
  USING_RTC_F(RuntimeClass_, Interface_);        \
  USING_RTC_B_INFO(RuntimeClass_, Interface_)

#define USING_RTC_S_B(RuntimeClass_, Interface_) \
  USING_RTC_S(RuntimeClass_, Interface_);        \
  USING_RTC_B_INFO(RuntimeClass_, Interface_)

#define USING_RTC_FS_B(RuntimeClass_, Interface_) \
  USING_RTC_FS(RuntimeClass_, Interface_);        \
  USING_RTC_B_INFO(RuntimeClass_, Interface_)

#define RTC_NAME_STD(RuntimeClass_)                                                                    \
  InterfaceInfo<RuntimeClass_>::ActivationFactoryPtr InterfaceInfo<RuntimeClass_>::ActivationFactory_; \
  HRESULT InterfaceInfo<RuntimeClass_ >::ActivationFactoryInitializationResult_ = S_OK;                \
  std::once_flag InterfaceInfo<RuntimeClass_ >::ActivationFactoryInitializationFlag_;                  \
  PFNGETACTIVATIONFACTORY InterfaceInfo<RuntimeClass_ >::ActivationFactoryFunc_ = &ABI::Windows::Foundation::GetActivationFactory

#define RTC_NAME_F_ONLY(RuntimeClass_)                                                               \
  InterfaceInfo<RuntimeClass_>::InterfaceFactoryPtr InterfaceInfo<RuntimeClass_>::InterfaceFactory_; \
  HRESULT InterfaceInfo<RuntimeClass_ >::InterfaceFactoryInitializationResult_ = S_OK;               \
  std::once_flag InterfaceInfo<RuntimeClass_ >::InterfaceFactoryInitializationFlag_

#define RTC_NAME_S_ONLY(RuntimeClass_)                                                               \
  InterfaceInfo<RuntimeClass_>::InterfaceStaticsPtr InterfaceInfo<RuntimeClass_>::InterfaceStatics_; \
  HRESULT InterfaceInfo<RuntimeClass_ >::InterfaceStaticsInitializationResult_ = S_OK;               \
  std::once_flag InterfaceInfo<RuntimeClass_ >::InterfaceStaticsInitializationFlag_

#define RTC_NAME(RuntimeClass_, Name_)                \
  LPCWSTR InterfaceInfo<RuntimeClass_>::Name = Name_; \
  RTC_NAME_STD(RuntimeClass_)

#define RTC_NAME_F(RuntimeClass_, Name_)              \
  LPCWSTR InterfaceInfo<RuntimeClass_>::Name = Name_; \
  RTC_NAME_STD(RuntimeClass_);                        \
  RTC_NAME_F_ONLY(RuntimeClass_)

#define RTC_NAME_S(RuntimeClass_, Name_)              \
  LPCWSTR InterfaceInfo<RuntimeClass_>::Name = Name_; \
  RTC_NAME_STD(RuntimeClass_);                        \
  RTC_NAME_S_ONLY(RuntimeClass_)

#define RTC_NAME_FS(RuntimeClass_, Name_)             \
  LPCWSTR InterfaceInfo<RuntimeClass_>::Name = Name_; \
  RTC_NAME_STD(RuntimeClass_);                        \
  RTC_NAME_F_ONLY(RuntimeClass_);                     \
  RTC_NAME_S_ONLY(RuntimeClass_)

struct Activate
{
	template<typename Ty>
	inline static HRESULT Instance(Microsoft::WRL::ComPtr<Ty>* obj)
	{
		return Instance<typename InterfaceInfo<Ty>::Info::RuntimeClass>(obj->ReleaseAndGetAddressOf());
	}

	template<typename Ty>
	static HRESULT Instance(Microsoft::WRL::Details::ComPtrRef<Microsoft::WRL::ComPtr<Ty>> obj)
	{
		return Instance<typename InterfaceInfo<Ty>::Info::RuntimeClass>(obj.ReleaseAndGetAddressOf());
	}

	template<typename Ty>
	static HRESULT Instance(typename InterfaceInfo<Ty>::Info::Interface** obj)
	{
		std::call_once(InterfaceInfo<Ty>::ActivationFactoryInitializationFlag_, []()
		{
			InterfaceInfo<Ty>::ActivationFactoryInitializationResult_ = InterfaceInfo<Ty>::ActivationFactoryFunc_(Microsoft::WRL::Wrappers::HStringReference(InterfaceInfo<Ty>::Name).Get(), &InterfaceInfo<Ty>::ActivationFactory_);
		});
		if (FAILED(InterfaceInfo<Ty>::ActivationFactoryInitializationResult_))
			return InterfaceInfo<Ty>::ActivationFactoryInitializationResult_;

		return InterfaceInfo<Ty>::ActivationFactory_->ActivateInstance(reinterpret_cast<IInspectable**>(obj));
	}
};

struct Activation
{
	template<typename Ty>
	inline static HRESULT Factory(Microsoft::WRL::ComPtr<Ty>* fact)
	{
		return Factory<typename InterfaceInfo<Ty>::Info::RuntimeClass>(fact->ReleaseAndGetAddressOf());
	}

	template<typename Ty>
	inline static HRESULT Factory(Microsoft::WRL::Details::ComPtrRef<Microsoft::WRL::ComPtr<Ty>> fact)
	{
		return Factory<typename InterfaceInfo<Ty>::Info::RuntimeClass>(fact.ReleaseAndGetAddressOf());
	}

	template<typename Ty>
	static HRESULT Factory(typename InterfaceInfo<Ty>::Info::Factory** fact)
	{
		std::call_once(InterfaceInfo<Ty>::Info::ActivationFactoryInitializationFlag_, []()
		{
			InterfaceInfo<Ty>::Info::ActivationFactoryInitializationResult_ = InterfaceInfo<Ty>::Info::ActivationFactoryFunc_(Microsoft::WRL::Wrappers::HStringReference(InterfaceInfo<Ty>::Info::Name).Get(), &InterfaceInfo<Ty>::Info::ActivationFactory_);
		});
		if (FAILED(InterfaceInfo<Ty>::Info::ActivationFactoryInitializationResult_))
			return InterfaceInfo<Ty>::Info::ActivationFactoryInitializationResult_;
		std::call_once(InterfaceInfo<Ty>::Info::InterfaceFactoryInitializationFlag_, []()
		{
			InterfaceInfo<Ty>::Info::InterfaceFactoryInitializationResult_ = InterfaceInfo<Ty>::Info::ActivationFactory_.As(&InterfaceInfo<Ty>::Info::InterfaceFactory_);
		});
		if (FAILED(InterfaceInfo<Ty>::Info::InterfaceFactoryInitializationResult_))
			return InterfaceInfo<Ty>::Info::InterfaceFactoryInitializationResult_;

		return InterfaceInfo<Ty>::Info::InterfaceFactory_.CopyTo(fact);
	}

	template<typename Ty>
	static typename InterfaceInfo<Ty>::InterfaceFactoryPtr Factory()
	{
		InterfaceInfo<Ty>::InterfaceFactoryPtr ret;
		Factory(&ret);

		return InterfaceInfo<Ty>::InterfaceFactory_;
	}
};

struct Interface
{
public:
	template<typename Ty>
	inline static HRESULT Statics(Microsoft::WRL::ComPtr<Ty>* statics)
	{
		return Statics<typename InterfaceInfo<Ty>::Info::RuntimeClass>(statics->ReleaseAndGetAddressOf());
	}

	template<typename Ty>
	inline static HRESULT Statics(Microsoft::WRL::Details::ComPtrRef<Microsoft::WRL::ComPtr<Ty>> statics)
	{
		return Statics<typename InterfaceInfo<Ty>::Info::RuntimeClass>(statics.ReleaseAndGetAddressOf());
	}

	template<typename Ty>
	static HRESULT Statics(typename InterfaceInfo<Ty>::Info::Statics** statics)
	{
		std::call_once(InterfaceInfo<Ty>::Info::ActivationFactoryInitializationFlag_, []()
		{
			InterfaceInfo<Ty>::Info::ActivationFactoryInitializationResult_ = InterfaceInfo<Ty>::Info::ActivationFactoryFunc_(Microsoft::WRL::Wrappers::HStringReference(InterfaceInfo<Ty>::Name).Get(), &InterfaceInfo<Ty>::Info::ActivationFactory_);
		});
		if (FAILED(InterfaceInfo<Ty>::Info::ActivationFactoryInitializationResult_))
			return InterfaceInfo<Ty>::Info::ActivationFactoryInitializationResult_;
		std::call_once(InterfaceInfo<Ty>::Info::InterfaceStaticsInitializationFlag_, []()
		{
			InterfaceInfo<Ty>::Info::InterfaceStaticsInitializationResult_ = InterfaceInfo<Ty>::Info::ActivationFactory_.As(&InterfaceInfo<Ty>::Info::InterfaceStatics_);
		});
		if (FAILED(InterfaceInfo<Ty>::Info::InterfaceStaticsInitializationResult_))
			return InterfaceInfo<Ty>::Info::InterfaceStaticsInitializationResult_;

		return InterfaceInfo<Ty>::Info::InterfaceStatics_.CopyTo(statics);
	}

	template<typename Ty>
	static typename InterfaceInfo<Ty>::Info::InterfaceStaticsPtr Statics()
	{
		InterfaceInfo<Ty>::Info::InterfaceStaticsPtr ret;
		Statics(&ret);
		return ret;
	}
};

namespace ABI {

namespace Collections {

namespace details {

template<typename T>
class TypedVectorHelper
{
public:
	typedef std::vector<::Microsoft::WRL::ComPtr<T>> Vector_T;
	static inline HRESULT CopyTo(typename Vector_T::value_type const& itr, T* out) noexcept
	{
		return itr.CopyTo(out);
	}
	static inline bool IsEqual(typename Vector_T::value_type const& itr, T value) noexcept
	{
		return itr.Get() == value;
	}
	static inline void Cleanup(Vector_T& items) noexcept
	{
	}
};

template<>
class TypedVectorHelper<HSTRING>
{
public:
	typedef std::vector<HSTRING> Vector_T;

	// Disable warning pertaining to returning out param from potential failed call.
	// We're also returning the failure state, so it's the caller's problem.
#pragma warning(push)
#pragma warning(disable:6103)
	static inline HRESULT CopyTo(Vector_T::value_type const& itr, HSTRING* out) noexcept
	{
		if (!out)
			return E_INVALIDARG;

		return ::WindowsDuplicateString(itr, out);
	}
#pragma warning(pop)

	static inline bool IsEqual(Vector_T::value_type const& itr, HSTRING value) noexcept
	{
		int res = 0;
		HRESULT hr = ::WindowsCompareStringOrdinal(value, itr, &res);
		if (FAILED(hr))
			return false;

		return !res;
	}
	static inline void Cleanup(Vector_T& items) noexcept
	{
		for (auto& item : items)
		{
			::WindowsDeleteString(item);
		}
	}
};

}   // } namespace details

template<typename T>
class TypedVectorIterator : public ::Microsoft::WRL::RuntimeClass<::ABI::Windows::Foundation::Collections::IIterator<T>>
{
	InspectableClass(z_get_rc_name_impl(), BaseTrust)
private:
	typedef typename ::ABI::Windows::Foundation::Internal::GetAbiType<typename ::ABI::Windows::Foundation::Collections::IIterator<T>::T_complex>::type     T_abi;
	typedef typename ::ABI::Windows::Foundation::Internal::GetLogicalType<typename ::ABI::Windows::Foundation::Collections::IIterator<T>::T_complex>::type T_logical;
	typedef typename details::TypedVectorHelper<T_abi>::Vector_T::iterator                                                                                 Iterator_T;

private:
	Iterator_T Itr_;
	Iterator_T End_;

public:
	TypedVectorIterator(Iterator_T itr, Iterator_T end) noexcept : Itr_(itr), End_(end)
	{
	}

	virtual /* propget */ HRESULT STDMETHODCALLTYPE get_Current(_Out_ T_abi* current) noexcept override
	{
		if (!current)
			return E_INVALIDARG;

		if (Itr_ == End_)
			return E_BOUNDS;

		return details::TypedVectorHelper<T>::CopyTo(*Itr_, current);
	}

	virtual /* propget */ HRESULT STDMETHODCALLTYPE get_HasCurrent(_Out_ boolean* hasCurrent) noexcept override
	{
		if (!hasCurrent)
			return E_INVALIDARG;

		*hasCurrent = Itr_ != End_;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE MoveNext(_Out_ boolean* hasCurrent) noexcept override
	{
		if (!hasCurrent)
			return E_INVALIDARG;

		if (Itr_ != End_)
			++Itr_;

		*hasCurrent = (Itr_ != End_);
		return S_OK;
	}
};

template<typename T>
class TypedVectorView : public ::Microsoft::WRL::RuntimeClass<::ABI::Windows::Foundation::Collections::IVectorView<T>, ::ABI::Windows::Foundation::Collections::IIterable<T>>
{
	InspectableClass(::ABI::Windows::Foundation::Collections::IVectorView<T>::z_get_rc_name_impl(), BaseTrust)
private:
	typedef typename ::ABI::Windows::Foundation::Internal::GetAbiType<typename ::ABI::Windows::Foundation::Collections::IVectorView<T>::T_complex>::type     T_abi;
	typedef typename ::ABI::Windows::Foundation::Internal::GetLogicalType<typename ::ABI::Windows::Foundation::Collections::IVectorView<T>::T_complex>::type T_logical;
	typedef typename details::TypedVectorHelper<T_abi>::Vector_T                                                                                             Vector_T;

private:
	Vector_T Items_;

public:
	explicit TypedVectorView(Vector_T const& items) noexcept : Items_(items.size())
	{
		Items_.resize(0);
		for (auto& item : items)
		{
			Items_.emplace_back(nullptr);
			details::TypedVectorHelper<T>::CopyTo(item, &Items_.back());
		}
	}

	~TypedVectorView() noexcept
	{
		details::TypedVectorHelper<T>::Cleanup(Items_);
	}

	// IVectorView<T>
	virtual HRESULT STDMETHODCALLTYPE GetAt(_In_ unsigned index, _Out_ T_abi* item) noexcept override
	{
		if (!(index < Items_.size()))
			return E_BOUNDS;

		if (!item)
			return E_INVALIDARG;

		return details::TypedVectorHelper<T>::CopyTo(Items_[index], item);
	}

	virtual /* propget */ HRESULT STDMETHODCALLTYPE get_Size(_Out_ unsigned* size) noexcept override
	{
		if (!size)
			return E_INVALIDARG;

		*size = static_cast<unsigned>(Items_.size());
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE IndexOf(_In_opt_ T_abi value, _Out_ unsigned* index, _Out_ boolean* found) noexcept override
	{
		if (!index || !found)
			return E_INVALIDARG;

		*found = false;
		for (*index = 0; *index < Items_.size(); ++(*index))
		{
			if (details::TypedVectorHelper<T>::IsEqual(Items_[*index], value))
			{
				*found = true;
				break;
			}
		}
		return S_OK;
	}

	// IIterable<T>
	virtual HRESULT STDMETHODCALLTYPE First(_Outptr_result_maybenull_::ABI::Windows::Foundation::Collections::IIterator<T_logical>** first) noexcept override
	{
		if (!first)
			return E_INVALIDARG;

		::Microsoft::WRL::ComPtr<TypedVectorIterator<T>> itr = ::Microsoft::WRL::Make<TypedVectorIterator<T>>(Items_.begin(), Items_.end());
		return itr.CopyTo(first);
	}
};

namespace details{

template<typename T>
struct storage_type_helper
{
	typedef T T_storage;
};

template<typename T>
struct storage_type_helper<T*>
{
	typedef ::Microsoft::WRL::ComPtr<T> T_storage;
};

template<>
struct storage_type_helper<HSTRING>
{
	typedef std::wstring T_storage;
};

template<typename T>
struct std_conversion_type;

template<typename T>
struct std_conversion_type<ABI::Windows::Foundation::Collections::IIterable<T>>
{
	typedef ABI::Windows::Foundation::Collections::IIterable<T>                     T_iterable;
	typedef typename ABI::Windows::Foundation::Collections::IIterable<T>::T_complex T_complex;
	typedef typename Windows::Foundation::Internal::GetAbiType<T_complex>::type     T_abi;
	typedef typename Windows::Foundation::Internal::GetLogicalType<T_complex>::type T_logical;
	typedef ABI::Windows::Foundation::Collections::IIterator<T>                     T_iterator;
};

// Need HSTRING and integral versions
template<typename T>
struct std_conversion_type<ABI::Windows::Foundation::Collections::IVectorView<T>>
{
	typedef typename ABI::Windows::Foundation::Collections::IIterable<T> T_iterable;
	typedef typename std_conversion_type<T_iterable>::T_complex          T_complex;
	typedef typename std_conversion_type<T_iterable>::T_abi              T_abi;
	typedef typename std_conversion_type<T_iterable>::T_logical          T_logical;
	typedef typename std_conversion_type<T_iterable>::T_iterator         T_iterator;

	typedef ::Microsoft::WRL::ComPtr<T_iterator>                         T_iterator_ptr;
	typedef ::Microsoft::WRL::ComPtr<T_iterable>                         T_iterable_ptr;

	typedef typename storage_type_helper<T_abi>::T_storage               T_abi_storage;

	typedef std::vector<T_abi_storage>                                   T_return;

	static inline void emplace(T_return& container, T_abi_storage&& item)
	{
		container.emplace_back(item);
	}
};

template<>
struct std_conversion_type<ABI::Windows::Foundation::Collections::IVectorView<HSTRING>>
{
	typedef ABI::Windows::Foundation::Collections::IIterable<HSTRING> T_iterable;
	typedef std_conversion_type<T_iterable>::T_complex                T_complex;
	typedef std_conversion_type<T_iterable>::T_abi                    T_abi;
	typedef std_conversion_type<T_iterable>::T_logical                T_logical;
	typedef std_conversion_type<T_iterable>::T_iterator               T_iterator;

	typedef ::Microsoft::WRL::ComPtr<T_iterator>                      T_iterator_ptr;
	typedef ::Microsoft::WRL::ComPtr<T_iterable>                      T_iterable_ptr;

	typedef HSTRING                                                   T_abi_storage;

	typedef std::vector<storage_type_helper<HSTRING>::T_storage>      T_return;

	static inline void emplace(T_return& container, T_abi_storage&& item)
	{
		container.emplace_back(::WindowsGetStringRawBuffer(item, nullptr));
		::WindowsDeleteString(item);
	}
};
template<typename T>
struct std_conversion_type<ABI::Windows::Foundation::Collections::IVector<T>> : public std_conversion_type<ABI::Windows::Foundation::Collections::IVectorView<T>>
{
};

#if _MSC_VER > 1700
template<typename T, typename U>
struct std_conversion_type<ABI::Windows::Foundation::Collections::IMapView<T, U>>
{
	typedef typename ABI::Windows::Foundation::Collections::IKeyValuePair<T, U>     T_kvp;
	typedef typename ABI::Windows::Foundation::Collections::IIterable<T_kvp*>       T_iterable;
	typedef typename std_conversion_type<T_iterable>::T_complex                     T_complex;
	typedef typename std_conversion_type<T_iterable>::T_abi                         T_abi;
	typedef typename std_conversion_type<T_iterable>::T_logical                     T_logical;
	typedef typename std_conversion_type<T_iterable>::T_iterator                    T_iterator;

	typedef ::Microsoft::WRL::ComPtr<T_iterator>                                    T_iterator_ptr;
	typedef ::Microsoft::WRL::ComPtr<T_iterable>                                    T_iterable_ptr;

	typedef typename T_kvp::K_complex                                               K_complex;
	typedef typename T_kvp::V_complex                                               V_complex;

	typedef typename Windows::Foundation::Internal::GetAbiType<K_complex>::type     K_abi;
	typedef typename Windows::Foundation::Internal::GetLogicalType<K_complex>::type K_logical;
	typedef typename Windows::Foundation::Internal::GetAbiType<V_complex>::type     V_abi;
	typedef typename Windows::Foundation::Internal::GetLogicalType<V_complex>::type V_logical;

	typedef typename storage_type_helper<T_abi>::T_storage                          T_abi_storage;
	typedef typename storage_type_helper<K_abi>::T_storage                          K_abi_storage;
	typedef typename storage_type_helper<V_abi>::T_storage                          V_abi_storage;

	typedef std::map<K_abi_storage, V_abi_storage>                                  T_return;

	static inline void emplace(T_return& container, T_abi_storage&& item)
	{
		K_abi_storage k;
		item->get_Key(k.GetAddressOf());
		V_abi_storage v;
		item->get_Value(v.GetAddressOf());
		container.emplace(std::move(k), std::move(v));
	}
};

template<typename T, typename U>
struct std_conversion_type<ABI::Windows::Foundation::Collections::IMap<T, U>> : public std_conversion_type<ABI::Windows::Foundation::Collections::IMapView<T, U>>
{
};
#endif

}   // }namespace details

template<typename T>
typename details::std_conversion_type<T>::T_return make_std_container(T* collection)
{
	if (!collection)
		return details::std_conversion_type<T>::T_return();

	unsigned int itemCt;
	collection->get_Size(&itemCt);

	details::std_conversion_type<T>::T_iterator_ptr iterator;
	{
		details::std_conversion_type<T>::T_iterable_ptr iterable;
		HRESULT hr = collection->QueryInterface(__uuidof(details::std_conversion_type<T>::T_iterable), &iterable);
		if (FAILED(hr))
			return details::std_conversion_type<T>::T_return();

		hr = iterable->First(&iterator);
		if (FAILED(hr))
			return details::std_conversion_type<T>::T_return();
	}

	details::std_conversion_type<T>::T_return std_container;
	{
		boolean hasCurrent = false;
		HRESULT hr = iterator->get_HasCurrent(&hasCurrent);
		if (FAILED(hr))
			return std_container;
		while (hasCurrent)
		{
			typename details::std_conversion_type<T>::T_abi_storage item;
			iterator->get_Current(&item);

			details::std_conversion_type<T>::emplace(std_container, std::move(item));

			hr = iterator->MoveNext(&hasCurrent);
			if (FAILED(hr))
				break;
		}
	}

	return std_container;
}
} // } namespace Collections

namespace Concurrency {

namespace details
{

template<typename T>
struct pointed_at;

template<typename T>
struct pointed_at<T*>
{
	typedef typename T type;
};

}

namespace details
{

template<typename T>
struct task_from_async_helper;

template<>
struct task_from_async_helper<ABI::Windows::Foundation::IAsyncAction>
{
	typedef HRESULT                                                return_T;
	typedef ABI::Windows::Foundation::IAsyncAction                 async_T;
	typedef ABI::Windows::Foundation::IAsyncActionCompletedHandler handler_T;
	static inline HRESULT completed(async_T* op, ABI::Windows::Foundation::AsyncStatus s, concurrency::task_completion_event<return_T> comp)
	{
		UNREFERENCED_PARAMETER(s);
		HRESULT const hr = [&]()
		{
			::Microsoft::WRL::ComPtr<IAsyncInfo> info;
			HRESULT comHr = op->QueryInterface(__uuidof(IAsyncInfo), &info);
			HRESULT hr = S_OK;

			comHr = info->get_ErrorCode(&hr);
			if (FAILED(comHr))
				return comHr;

			return hr;
		} ();
		comp.set(hr);
		return S_OK;
	}
	static inline void set_completed_failure(HRESULT hr, concurrency::task_completion_event<return_T> comp)
	{
		comp.set(hr);
	}
};

template<typename T>
struct task_from_async_helper<ABI::Windows::Foundation::IAsyncActionWithProgress<T>>
{
	typedef HRESULT                                                               return_T;
	typedef ABI::Windows::Foundation::IAsyncActionWithProgress<T>                 async_T;
	typedef ABI::Windows::Foundation::IAsyncActionWithProgressCompletedHandler<T> handler_T;
	static inline HRESULT completed(async_T* op, ABI::Windows::Foundation::AsyncStatus s, concurrency::task_completion_event<return_T> comp)
	{
		UNREFERENCED_PARAMETER(s);
		HRESULT const hr = [&]()
		{
			::Microsoft::WRL::ComPtr<IAsyncInfo> info;
			HRESULT comHr = op->QueryInterface(__uuidof(IAsyncInfo), &info);
			HRESULT hr = S_OK;

			comHr = info->get_ErrorCode(&hr);

			return hr;
		} ();

		comp.set(hr);
		return S_OK;
	}
	static inline void set_completed_failure(HRESULT hr, concurrency::task_completion_event<return_T> comp)
	{
		comp.set(hr);
	}
};

// Disabled because HString isn't copyable and a task which does not properly complete and release would leak
//  template<>
//  struct task_from_async_helper< ABI::Windows::Foundation::IAsyncOperation<HSTRING> >
//  {
//  public:
//    typedef std::tuple< HRESULT, HSTRING > return_T;
//    typedef ABI::Windows::Foundation::IAsyncOperation<HSTRING> async_T;
//    typedef ABI::Windows::Foundation::IAsyncOperationCompletedHandler<HSTRING> handler_T;
//    static inline HRESULT completed(async_T* op, ABI::Windows::Foundation::AsyncStatus s, concurrency::task_completion_event<return_T> comp)
//    {
//      UNREFERENCED_PARAMETER(s);
//      HSTRING result;
//      HRESULT hr = op->GetResults(&result);
//
//      comp.set(std::make_tuple(std::move(hr), std::move(result)));
//      return S_OK;
//    }
//    static inline void set_completed_failure(HRESULT hr, concurrency::task_completion_event<return_T> comp)
//    {
//      comp.set(std::make_tuple(std::move(hr), nullptr));
//    }
//  };

template<typename T>
struct task_from_async_helper<ABI::Windows::Foundation::IAsyncOperation<T*>>
{
private:
	typedef ::Microsoft::WRL::ComPtr<
	    typename details::pointed_at<typename ABI::Windows::Foundation::Internal::GetAbiType<typename ABI::Windows::Foundation::IAsyncOperation<T*>::TResult_complex>::type>::type> comptr_T;
public:
	typedef std::tuple<HRESULT, comptr_T>                                 return_T;
	typedef ABI::Windows::Foundation::IAsyncOperation<T*>                 async_T;
	typedef ABI::Windows::Foundation::IAsyncOperationCompletedHandler<T*> handler_T;
	static inline HRESULT completed(async_T* op, ABI::Windows::Foundation::AsyncStatus s, concurrency::task_completion_event<return_T> comp)
	{
		comptr_T result;
		HRESULT hr = op->GetResults(&result);
		if (SUCCEEDED(hr) && s == AsyncStatus::Error)
			hr = E_FAIL;

		comp.set(std::make_tuple(std::move(hr), std::move(result)));
		return S_OK;
	}
	static inline void set_completed_failure(HRESULT hr, concurrency::task_completion_event<return_T> comp)
	{
		comp.set(std::make_tuple(std::move(hr), comptr_T()));
	}
};

template<typename T>
struct task_from_async_helper<ABI::Windows::Foundation::IAsyncOperation<T>>
{
public:
	typedef std::tuple<HRESULT, T>                                       return_T;
	typedef ABI::Windows::Foundation::IAsyncOperation<T>                 async_T;
	typedef ABI::Windows::Foundation::IAsyncOperationCompletedHandler<T> handler_T;
	static inline HRESULT completed(async_T* op, ABI::Windows::Foundation::AsyncStatus s, concurrency::task_completion_event<return_T> comp)
	{
		T result;
		HRESULT hr = op->GetResults(&result);
		if (SUCCEEDED(hr) && s == AsyncStatus::Error)
			hr = E_FAIL;

		comp.set(std::make_tuple(std::move(hr), std::move(result)));
		return S_OK;
	}
	static inline void set_completed_failure(HRESULT hr, concurrency::task_completion_event<return_T> comp)
	{
		comp.set(std::make_tuple(std::move(hr), T()));
	}
};

template<typename T, typename U>
struct task_from_async_helper<ABI::Windows::Foundation::IAsyncOperationWithProgress<T, U>>
{
private:
	typedef ::Microsoft::WRL::ComPtr<
	    typename details::pointed_at<typename ABI::Windows::Foundation::Internal::GetAbiType<typename ABI::Windows::Foundation::IAsyncOperationWithProgress<T, U>::TResult_complex>::type>::type> comptr_T;
public:
	typedef std::tuple<HRESULT, comptr_T>                                               return_T;
	typedef ABI::Windows::Foundation::IAsyncOperationWithProgress<T, U>                 async_T;
	typedef ABI::Windows::Foundation::IAsyncOperationWithProgressCompletedHandler<T, U> handler_T;
	static inline HRESULT completed(async_T* op, ABI::Windows::Foundation::AsyncStatus s, concurrency::task_completion_event<return_T> comp)
	{
		ComPtr<typename details::pointed_at<typename ABI::Windows::Foundation::Internal::GetAbiType<typename ABI::Windows::Foundation::IAsyncOperationWithProgress<T, U>::TResult_complex>::type>::type> result;
		HRESULT hr = op->GetResults(&result);
		if (SUCCEEDED(hr) && s == AsyncStatus::Error)
			hr = E_FAIL;

		comp.set(std::make_tuple(std::move(hr), std::move(result)));
		return S_OK;
	}
	static inline void set_completed_failure(HRESULT hr, concurrency::task_completion_event<return_T> comp)
	{
		comp.set(std::make_tuple(std::move(hr), comptr_T()));
	}
};

}

template<typename T>
concurrency::task<typename details::task_from_async_helper<T>::return_T> task_from_async(::Microsoft::WRL::ComPtr<T>& op)
{
	concurrency::task_completion_event<typename details::task_from_async_helper<T>::return_T> comp;

	HRESULT hr = op->put_Completed(::Microsoft::WRL::Callback<typename details::task_from_async_helper<T>::handler_T>(
	                                 [comp](typename details::task_from_async_helper<T>::async_T* op, ABI::Windows::Foundation::AsyncStatus s)
			{
				return details::task_from_async_helper<T>::completed(op, s, comp);
	    }).Get());
	if (FAILED(hr))
		details::task_from_async_helper<T>::set_completed_failure(hr, comp);

	return concurrency::create_task(comp);
}

template<typename T>
concurrency::task<typename details::task_from_async_helper<T>::return_T> task_from_async(T* op)
{
	concurrency::task_completion_event<typename details::task_from_async_helper<T>::return_T> comp;

	HRESULT hr = op->put_Completed(::Microsoft::WRL::Callback<typename details::task_from_async_helper<T>::handler_T>(
	                                 [comp](typename details::task_from_async_helper<T>::async_T* op, ABI::Windows::Foundation::AsyncStatus s)
			{
				return details::task_from_async_helper<T>::completed(op, s, comp);
	    }).Get());
	if (FAILED(hr))
		details::task_from_async_helper<T>::set_completed_failure(hr, comp);

	return concurrency::create_task(comp);
}

}
}
