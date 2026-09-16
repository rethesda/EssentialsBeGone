#pragma once

#define WIN32_LEAN_AND_MEAN

#include "RE/Skyrim.h"
#include "REX/REX.h"
#include "SKSE/SKSE.h"

#include <boost/unordered/concurrent_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include "ClibUtil/editorID.hpp"

using namespace clib_util;
using namespace RE::literals;
using namespace std::literals;

template <class K, class D, class H = boost::hash<K>, class KEqual = std::equal_to<K>>
using ConcurrentMap = boost::concurrent_flat_map<K, D, H, KEqual>;

template <class K, class H = boost::hash<K>, class KEqual = std::equal_to<K>>
using Set = boost::unordered_flat_set<K, H, KEqual>;

namespace stl
{
	template <class F, size_t offset, class T>
	void write_vfunc()
	{
		REL::Relocation<std::uintptr_t> vtbl{ F::VTABLE[offset] };
		T::func = vtbl.write_vfunc(T::idx, T::thunk);
	}

	template <class F, class T>
	void write_vfunc()
	{
		write_vfunc<F, 0, T>();
	}

	template <class T>
	void write_thunk_call(std::uintptr_t a_src)
	{
		auto& trampoline = REL::GetTrampoline();
		T::func = trampoline.write_call<5>(a_src, T::thunk);
	}

	template <class T>
	T& get_setting_ref(REX::TSetting<T>& a_setting)
	{
		return static_cast<T&>(a_setting);
	}

	template <class T>
	const T& get_setting_ref(const REX::TSetting<T>& a_setting)
	{
		return static_cast<const T&>(a_setting);
	}
}

#include "Version.h"

#ifdef SKYRIM_AE
#	define OFFSET(se, ae) ae
#else
#	define OFFSET(se, ae) se
#endif
