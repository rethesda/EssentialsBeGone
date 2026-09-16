#include "Hooks.h"

#include "Manager.h"

namespace Hooks
{
	template <std::size_t N>
	struct CalculateEssentialProtected
	{
		static void thunk(RE::Actor* a_actor)
		{
			func(a_actor);

			if (!a_actor->IsPlayerRef()) {
				if (bool essential = a_actor->boolFlags.any(RE::Actor::BOOL_FLAGS::kEssential); essential || a_actor->boolFlags.any(RE::Actor::BOOL_FLAGS::kProtected)) {
					Manager::GetSingleton()->DisableEssentialStatus(a_actor, essential);
				}
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	void Install()
	{
		REL::Relocation<std::uintptr_t> target_0{ RELOCATION_ID(36356, 37347), OFFSET(0x15A, 0x293) }; // Actor::Process (can't just hook KillImpl because aliases unregister on death or smth)
		stl::write_thunk_call<CalculateEssentialProtected<0>>(target_0.address());
		
		REL::Relocation<std::uintptr_t> target_1{ RELOCATION_ID(36872, 37896), OFFSET(0xA1, 0xA9) }; // Actor::KillImpl
		stl::write_thunk_call<CalculateEssentialProtected<1>>(target_1.address());
	}
}
