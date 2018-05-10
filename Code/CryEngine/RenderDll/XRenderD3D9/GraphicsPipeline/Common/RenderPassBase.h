// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Containers/CryArray.h>


class CGraphicsPipeline;


namespace ComputeRenderPassInputChangedDetail
{
	template <int Size, int N, typename Tuple>
	struct is_tuple_trivial
	{
		// Check that data is a POD and not a pointer
		using T = typename std::tuple_element<N, Tuple>::type;
		using S = typename std::decay<T>::type;
		static constexpr bool value = !std::is_pointer<S>::value && std::is_trivial<S>::value && is_tuple_trivial<Size, N+1, Tuple>::value;
	};
	template <int Size, typename Tuple>
	struct is_tuple_trivial<Size, Size, Tuple>
	{
		static constexpr bool value = true;
	};

// 	template <typename Tuple>
// 	static constexpr bool is_tuple_trivial_v = is_tuple_trivial<std::tuple_size<Tuple>::value, 0, Tuple>::value;
}

class CRenderPassBase
{
	static constexpr size_t nInputVarsStaticSize = 16;

public:
	void SetLabel(const char* label)  { m_label = label; }
	const char* GetLabel() const      { return m_label.c_str(); }

	CGraphicsPipeline& GetGraphicsPipeline() const;

	virtual bool InputChanged() const { return false; }
	// Take a variadic list of trivial parameters. Returns true if and only if IsDirty() is set or the serialized data is different from the input. 
	// Note: Comparison is done as with memcmp on serialized blobs. Types are ignored.
	template <typename T, typename... Ts>
	bool InputChanged(T&& arg, Ts&&... args)
	{
		return InputChanged(std::make_tuple(std::forward<T>(arg), std::forward<Ts>(args)...));
	}
	template <typename... Ts>
	bool InputChanged(std::tuple<Ts...>&& tuple)
	{
		using Tuple = std::tuple<Ts...>;
		static constexpr size_t size = sizeof(Tuple);

		// Verify all types are trivial
		static_assert(ComputeRenderPassInputChangedDetail::is_tuple_trivial<std::tuple_size<Tuple>::value, 0, Tuple>::value, "All input types must be trivial non-pointer types");

		// Check if input tuple equals the serialized one
		if (size == m_inputVars.size() * sizeof(decltype(m_inputVars)::value_type) &&
			*reinterpret_cast<const Tuple*>(m_inputVars.data()) == tuple)
		{
			// Nothing changed
			return InputChanged();
		}

		// Store new data
		m_inputVars.resize(size);
		*reinterpret_cast<Tuple*>(m_inputVars.data()) = std::move(tuple);

		return true;
	}

protected:
	string m_label;

private:
	LocalDynArray<uint8, nInputVarsStaticSize> m_inputVars;
};
