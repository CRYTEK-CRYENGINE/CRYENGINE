#pragma once

#include <functional>
#include <type_traits>

#include <CryCore/Assert/CryAssert.h>

namespace stl
{

	// Helper struct that deletes move/copy ctor/operator
	namespace copyable_moveable_base_detail 
	{
		template <bool b>
		struct move_ctor_deleter {};
		template <>
		struct move_ctor_deleter<false>
		{
			move_ctor_deleter() = default;
			move_ctor_deleter(const move_ctor_deleter&) = default;
			move_ctor_deleter &operator=(move_ctor_deleter&&) = default;
			move_ctor_deleter &operator=(const move_ctor_deleter&) = default;
			move_ctor_deleter(move_ctor_deleter&&) = delete;
		};

		template <bool b>
		struct move_assign_deleter {};
		template <>
		struct move_assign_deleter<false>
		{
			move_assign_deleter() = default;
			move_assign_deleter(move_assign_deleter&&) = default;
			move_assign_deleter(const move_assign_deleter&) = default;
			move_assign_deleter &operator=(const move_assign_deleter&) = default;
			move_assign_deleter &operator=(move_assign_deleter&&) = delete;
		};

		template <bool b>
		struct copy_ctor_deleter {};
		template <>
		struct copy_ctor_deleter<false>
		{
			copy_ctor_deleter() = default;
			copy_ctor_deleter(copy_ctor_deleter&&) = default;
			copy_ctor_deleter &operator=(copy_ctor_deleter&&) = default;
			copy_ctor_deleter &operator=(const copy_ctor_deleter&) = default;
			copy_ctor_deleter(const copy_ctor_deleter&) = delete;
		};

		template <bool b>
		struct copy_assign_deleter {};
		template <>
		struct copy_assign_deleter<false>
		{
			copy_assign_deleter() = default;
			copy_assign_deleter(copy_assign_deleter&&) = default;
			copy_assign_deleter(const copy_assign_deleter&) = default;
			copy_assign_deleter &operator=(copy_assign_deleter&&) = default;
			copy_assign_deleter &operator=(const copy_assign_deleter&) = delete;
		};
	}
	template <bool is_move_constructible, bool is_copy_constructible, bool is_move_assignable, bool is_copy_assignable>
	struct copyable_moveable_base
		: copyable_moveable_base_detail::move_ctor_deleter<is_move_constructible>,
		copyable_moveable_base_detail::copy_ctor_deleter<is_copy_constructible>,
		copyable_moveable_base_detail::move_assign_deleter<is_move_assignable>,
		copyable_moveable_base_detail::copy_assign_deleter<is_copy_assignable>
	{
		copyable_moveable_base() = default;
		copyable_moveable_base(copyable_moveable_base&&) = default;
		copyable_moveable_base(const copyable_moveable_base&) = default;
		copyable_moveable_base &operator=(copyable_moveable_base&&) = default;
		copyable_moveable_base &operator=(const copyable_moveable_base&) = default;
	};


	// C++11 implementation of optional
	//! Remove once we move to C++17

	struct nullopt_t {};
	static nullopt_t nullopt;

	struct optional_ctor_inplace {};

	namespace optional_detail {

		template <typename T>
		class optional_base_moveable_copyable
		{
			using storage_t = typename std::aligned_storage<sizeof(T), alignof(T)>::type;

		protected:
			storage_t storage;
			bool has_val{ false };

			T& val() &
			{
				if (!has_val)
				{
					// throw
					CRY_ASSERT_MESSAGE(false, "Attempting to get value but optional is empty");
				}
				return *reinterpret_cast<T*>(&storage);
			}
			T&& val() && {
				if (!has_val)
				{
					// throw
					CRY_ASSERT_MESSAGE(false, "Attempting to get value but optional is empty");
				}
				return std::move(*reinterpret_cast<T*>(&storage));
			}
			const T& val() const&
			{
				if (!has_val)
				{
					// throw
					CRY_ASSERT_MESSAGE(false, "Attempting to get value but optional is empty");
				}
				return *reinterpret_cast<const T*>(&storage);
			}

			template <typename... Ts>
			void ctor_val(Ts&&... ts)
			{
				if (has_val)
					destroy_val();
				::new (&storage) T(std::forward<Ts>(ts)...);
				has_val = true;
			}

			template <typename S>
			void assign_val(S&& s)
			{
				if (has_val)
				{
					val() = std::forward<S>(s);
				}
				else
				{
					ctor_val(std::forward<S>(s));
				}
				has_val = true;
			}

			void destroy_val() noexcept(std::is_nothrow_destructible<T>::value)
			{
				val().~T();
				has_val = false;
			}

		public:
			optional_base_moveable_copyable() = default;
			template <typename... Args>
			optional_base_moveable_copyable(optional_ctor_inplace,
				Args&&... args)
			{
				ctor_val(std::forward<Args>(args)...);
			}

			~optional_base_moveable_copyable() noexcept/*(std::is_nothrow_destructible<T>::value)*/
			{  // Blows up with internal compiler failure on vcc15
				if (has_val)
					destroy_val();
			}

		public:
			optional_base_moveable_copyable(optional_base_moveable_copyable &&other)
				noexcept(std::is_nothrow_move_constructible<T>::value)
			{
				if (other.has_val)
				{
					ctor_val(std::move(other.val()));
					other.destroy_val();
				}
			}
			optional_base_moveable_copyable &operator=(optional_base_moveable_copyable &&other)
				noexcept(std::is_nothrow_move_constructible<T>::value && std::is_nothrow_move_assignable<T>::value)
			{
				if (other.has_val)
				{
					this->assign_val(std::move(other.val()));
					other.destroy_val();
				}
				else if (has_val)
				{
					destroy_val();
				}

				return *this;
			}
			optional_base_moveable_copyable(const optional_base_moveable_copyable &other)
				noexcept(std::is_nothrow_copy_constructible<T>::value)
			{
				if (other.has_val)
				{
					ctor_val(other.val());
				}
			}
			optional_base_moveable_copyable &operator=(const optional_base_moveable_copyable &other)
				noexcept(std::is_nothrow_copy_constructible<T>::value && std::is_nothrow_copy_assignable<T>::value)
			{
				if (other.has_val)
				{
					ctor_val(other.val());
				}
				else if (has_val)
				{
					destroy_val();
				}

				return *this;
			}
		};

		template <typename T>
		struct optional_copymove_base
			: optional_base_moveable_copyable<T>,
			copyable_moveable_base<std::is_nothrow_move_constructible<T>::value, std::is_copy_constructible<T>::value, std::is_nothrow_move_assignable<T>::value, std::is_copy_assignable<T>::value>
		{
			using optional_base_moveable_copyable<T>::optional_base_moveable_copyable;
		};

		template <typename T>
		using optional_ctor_base = optional_copymove_base<T>;

	}

	template <typename T>
	class optional : public optional_detail::optional_ctor_base<T>
	{
		using Base = optional_detail::optional_ctor_base<T>;

	public:
		using type = typename std::remove_reference<typename std::remove_pointer<T>::type>::type;

	private:
		template <typename S>
		friend class optional;

		using reference = type & ;
		using const_reference = const type&;
		using pointer = type * ;
		using const_pointer = const type*;

	private:
		template <typename S = T>
		typename std::enable_if<!std::is_pointer<S>::value, const typename std::remove_reference<S>::type&>::type
			get_val() const { return val(); }
		template <typename S = T>
		typename std::enable_if<std::is_pointer<S>::value, const typename std::remove_reference<typename std::remove_pointer<S>::type>::type&>::type
			get_val() const { return *val(); }
		template <typename S = T>
		typename std::enable_if<!std::is_pointer<S>::value, typename std::remove_reference<S>::type&>::type
			get_val() { return val(); }
		template <typename S = T>
		typename std::enable_if<std::is_pointer<S>::value, typename std::remove_reference<typename std::remove_pointer<S>::type>::type&>::type
			get_val() { return *val(); }

		template <typename S = T>
		typename std::enable_if<!std::is_pointer<S>::value, const typename std::remove_reference<S>::type*>::type
			get_ref() const { return &val(); }
		template <typename S = T>
		typename std::enable_if<std::is_pointer<S>::value, const typename std::remove_reference<S>::type>::type
			get_ref() const { return val(); }
		template <typename S = T>
		typename std::enable_if<!std::is_pointer<S>::value, typename std::remove_reference<S>::type*>::type
			get_ref() { return &val(); }
		template <typename S = T>
		typename std::enable_if<std::is_pointer<S>::value, typename std::remove_reference<S>::type>::type
			get_ref() { return val(); }

		using Base::ctor_val;
		using Base::destroy_val;
		using Base::has_val;
		using Base::val;

	public:
		optional() {}
		optional(const nullopt_t &) : optional() {}

		optional(optional<T> &&) = default;
		optional(const optional<T> &) = default;
		optional &operator=(optional<T> &&) = default;
		optional &operator=(const optional<T> &) = default;

		template <typename Placeholder, typename... Args>
		optional(typename std::enable_if<std::is_same<Placeholder, optional_ctor_inplace>::value, optional_ctor_inplace>::type,
			Args&&... args)
			: Base(optional_ctor_inplace{},
				std::forward<Args>(args)...)
		{}

		template <typename S, typename =
			typename std::enable_if<std::is_constructible<T, S>::value || std::is_convertible<S, T>::value>::type
		>
			optional(optional<S> &&other) noexcept
		{
			if (other.has_val)
			{
				ctor_val(std::move(other.val()));
				other.destroy_val();
			}
		}
		template <typename S, typename =
			typename std::enable_if<std::is_constructible<T, const S&>::value || std::is_convertible<const S&, T>::value>::type
		>
			optional(const optional<S> &other)
		{
			if (other.has_val)
			{
				ctor_val(other.val());
			}
		}

		template <typename S, typename =
			typename std::enable_if<std::is_constructible<T, S>::value || std::is_convertible<S, T>::value>::type
		>
			optional(S &&v)
		{
			ctor_val(std::move(v));
		}
		template <typename S, typename =
			typename std::enable_if<std::is_constructible<T, const S&>::value || std::is_convertible<const S&, T>::value>::type
		>
			optional(const S &v)
		{
			ctor_val(v);
		}

		template <typename S, typename =
			typename std::enable_if<(std::is_constructible<T, S>::value && std::is_assignable<T, S>::value) || std::is_convertible<S, T>::value>::type
		>
			optional &operator=(S&& v)
		{
			this->assign_val(std::move(v));
			return *this;
		}
		template <typename S, typename =
			typename std::enable_if<(std::is_constructible<T, const S&>::value && std::is_assignable<T, const S&>::value) || std::is_convertible<const S&, T>::value>::type
		>
			optional &operator=(const S &v)
		{
			this->assign_val(v);
			return *this;
		}

		template <typename S, typename =
			typename std::enable_if<(std::is_constructible<T, S>::value && std::is_assignable<T, S>::value) || std::is_convertible<S, T>::value>::type
		>
			optional &operator=(optional<S> &&other) noexcept
		{
			if (other.has_val)
			{
				this->assign_val(std::move(other.val()));
				other.destroy_val();
			}
			else if (has_val)
			{
				destroy_val();
			}

			return *this;
		}
		template <typename S, typename =
			typename std::enable_if<(std::is_constructible<T, const S&>::value && std::is_assignable<T, const S&>::value) || std::is_convertible<const S&, T>::value>::type
		>
			optional &operator=(const optional<S> &other)
		{
			if (other.has_val)
			{
				this->assign_val(other.val());
			}
			else if (has_val)
			{
				destroy_val();
			}

			return *this;
		}

		optional &operator=(nullopt_t)
		{
			if (has_val)
				destroy_val();

			return *this;
		}

		~optional() noexcept {}

		template <typename ... Ts>
		void emplace(Ts&&... args)
		{
			ctor_val(std::forward<Ts>(args)...);
		}

		const typename std::remove_reference<T>::type& value() const& { return val(); }
		typename std::remove_reference<T>::type& value() & { return val(); }
		typename std::remove_reference<T>::type&& value() && { return std::move(*this).val(); }

		template <typename U>
		typename std::remove_reference<T>::type value_or(U&& default_value) const& { return has_val ? val() : std::forward<U>(default_value); }
		template <typename U>
		typename std::remove_reference<T>::type&& value_or(U&& default_value) && { return has_val ? std::move(*this).val() : std::forward<U>(default_value); }

		const_reference operator*() const { return get_val(); }
		reference operator*() { return get_val(); }

		const_pointer operator->() const { return get_ref(); }
		pointer operator->() { return get_ref(); }

		explicit operator bool() const { return has_val; }
		bool operator!() const { return !has_val; }
	};

	template <typename T>
	bool operator==(const optional<T> &opt, nullopt_t)
	{
		return !opt;
	}
	template <typename T>
	bool operator!=(const optional<T> &opt, nullopt_t)
	{
		return !!opt;
	}

	template <typename T>
	bool operator==(nullopt_t, const optional<T> &opt)
	{
		return !opt;
	}
	template <typename T>
	bool operator!=(nullopt_t, const optional<T> &opt)
	{
		return !!opt;
	}

	template <typename T>
	bool operator==(const optional<T> &opt, const T &t)
	{
		return opt.value() == t;
	}
	template <typename T>
	bool operator!=(const optional<T> &opt, const T &t)
	{
		return opt.value() != t;
	}
	template <typename T>
	bool operator==(const T &t, const optional<T> &opt)
	{
		return t == opt.value();
	}
	template <typename T>
	bool operator!=(const T &t, const optional<T> &opt)
	{
		return t != opt.value();
	}
	template <typename T>
	bool operator==(const optional<T> &opt0, const optional<T> &opt1)
	{
		if (!opt0 != !opt1)
			return false;
		if (!opt0 && !opt1)
			return true;
		return opt0.value() == opt1.value();
	}
	template <typename T>
	bool operator!=(const optional<T> &opt0, const optional<T> &opt1)
	{
		return opt0.value() != opt1.value();
	}

	template <typename T>
	bool operator<(const optional<T> &opt, const T &t)
	{
		return opt.value() < t;
	}
	template <typename T>
	bool operator>(const optional<T> &opt, const T &t)
	{
		return opt.value() > t;
	}
	template <typename T>
	bool operator<(const T &t, const optional<T> &opt)
	{
		return t < opt.value();
	}
	template <typename T>
	bool operator>(const T &t, const optional<T> &opt)
	{
		return t > opt.value();
	}
	template <typename T>
	bool operator<(const optional<T> &opt0, const optional<T> &opt1)
	{
		return opt0.value() < opt1.value();
	}
	template <typename T>
	bool operator>(const optional<T> &opt0, const optional<T> &opt1)
	{
		return opt0.value() > opt1.value();
	}

	template <typename T>
	bool operator<=(const optional<T> &opt, const T &t)
	{
		return opt.value() <= t;
	}
	template <typename T>
	bool operator>=(const optional<T> &opt, const T &t)
	{
		return opt.value() >= t;
	}
	template <typename T>
	bool operator<=(const T &t, const optional<T> &opt)
	{
		return t <= opt.value();
	}
	template <typename T>
	bool operator>=(const T &t, const optional<T> &opt)
	{
		return t >= opt.value();
	}
	template <typename T>
	bool operator<=(const optional<T> &opt0, const optional<T> &opt1)
	{
		return opt0.value() <= opt1.value();
	}
	template <typename T>
	bool operator>=(const optional<T> &opt0, const optional<T> &opt1)
	{
		return opt0.value() >= opt1.value();
	}

	template <typename T>
	T operator+(const optional<T> &opt, const T &t)
	{
		return opt.value() + t;
	}
	template <typename T>
	T operator-(const optional<T> &opt, const T &t)
	{
		return opt.value() - t;
	}
	template <typename T>
	T operator*(const optional<T> &opt, const T &t)
	{
		return opt.value() * t;
	}
	template <typename T>
	T operator/(const optional<T> &opt, const T &t)
	{
		return opt.value() / t;
	}
	template <typename T>
	T operator+=(const optional<T> &opt, const T &t)
	{
		return opt.value() += t;
	}
	template <typename T>
	T operator-=(const optional<T> &opt, const T &t)
	{
		return opt.value() -= t;
	}
	template <typename T>
	T operator*=(const optional<T> &opt, const T &t)
	{
		return opt.value() *= t;
	}
	template <typename T>
	T operator/=(const optional<T> &opt, const T &t)
	{
		return opt.value() /= t;
	}

	template <typename T>
	T operator+(const T &t, const optional<T> &opt)
	{
		return t + opt.value();
	}
	template <typename T>
	T operator-(const T &t, const optional<T> &opt)
	{
		return t - opt.value();
	}
	template <typename T>
	T operator*(const T &t, const optional<T> &opt)
	{
		return t * opt.value();
	}
	template <typename T>
	T operator/(const T &t, const optional<T> &opt)
	{
		return t / opt.value();
	}
	template <typename T>
	T operator+=(const T &t, const optional<T> &opt)
	{
		return t += opt.value();
	}
	template <typename T>
	T operator-=(const T &t, const optional<T> &opt)
	{
		return t -= opt.value();
	}
	template <typename T>
	T operator*=(const T &t, const optional<T> &opt)
	{
		return t *= opt.value();
	}
	template <typename T>
	T operator/=(const T &t, const optional<T> &opt)
	{
		return t /= opt.value();
	}

	template <typename T, typename... Ts>
	optional<T> make_optional(Ts&&... ts)
	{
		return optional<T>(T(std::forward<Ts>(ts)...));
	}

}
