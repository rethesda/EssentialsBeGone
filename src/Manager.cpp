#include "Manager.h"

void Manager::Register()
{
	if (auto scriptEventSource = RE::ScriptEventSourceHolder::GetSingleton()) {
		scriptEventSource->AddEventSink(this);
	}
}

void Manager::RequestAPI()
{
	_nnd = static_cast<NND_API::IVNND2*>(NND_API::RequestPluginAPI());
	if (_nnd) {
		REX::INFO("NND API requested successfully");
	} else {
		REX::ERROR("Failed to request NND API");
	}
}

void Manager::LoadSettings()
{
	const auto store = REX::FIniSettingStore::GetSingleton();
	store->Init(path.data(), "");

	store->Load();
	store->Save();
}

void Manager::DisableEssentialStatus(RE::Actor* a_actor, RE::TESNPC* a_npc)
{
	if (!a_actor || !a_npc) {
		return;
	}

	const auto& npcExclusionsA = stl::get_setting_ref(npcExclusions);

	if (std::ranges::find(npcExclusionsA, editorID::get_editorID(a_npc)) != npcExclusionsA.end()) {
		return;
	}

	bool essential = a_actor->IsEssential();
	bool playerTeammate = a_actor->IsPlayerTeammate();

	if (essential || a_actor->IsProtected()) {
		if (playerTeammate) {
			DisableEssentialStatusActor(GetFollowerNPCState(), a_actor);
		} else {
			DisableEssentialStatusActor(GetGeneralNPCState(), a_actor);
		}
	}

	bool baseEssential = a_npc->IsEssential();

	if (baseEssential || a_npc->IsProtected()) {
		if (playerTeammate) {
			DisableEssentialStatusNPC(GetFollowerNPCState(), a_npc);
		} else {
			DisableEssentialStatusNPC(GetGeneralNPCState(), a_npc);
		}
	}

	if (auto xAliases = a_actor->extraList.GetByType<RE::ExtraAliasInstanceArray>()) {
		RE::BSReadLockGuard locker(xAliases->lock);
		for (auto& aliasData : xAliases->aliases) {
			if (aliasData) {
				auto quest = aliasData->quest;
				auto alias = const_cast<RE::BGSBaseAlias*>(aliasData->alias);
				if (quest && alias && quest->GetType() != RE::QUEST_DATA::Type::kNone && (alias->IsEssential() || essential || baseEssential)) {
					alias->SetEssential(false);
					alias->SetProtected(true);

					bool isVunerable = playerTeammate ? (GetFollowerNPCState() == NPC_STATE::kVunerable) : (GetGeneralNPCState() == NPC_STATE::kVunerable);

					if (isVunerable && GetSideQuestNPCState() == NPC_STATE::kVunerable) {
						switch (quest->GetType()) {
						case RE::QUEST_DATA::Type::kMiscellaneous:
						case RE::QUEST_DATA::Type::kSideQuest:
							alias->SetProtected(false);
							break;
						default:
							break;
						}
					}

					questNPCs.insert({ a_actor->GetFormID(), quest->GetType() });
				}
			}
		}
	}
}

void Manager::DisableEssentialStatusActor(NPC_STATE a_state, RE::Actor* a_actor) const
{
	switch (a_state) {
	case NPC_STATE::kDisabled:
		break;
	case NPC_STATE::kProtected:
		{
			a_actor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kEssential);
			a_actor->boolFlags.set(RE::Actor::BOOL_FLAGS::kProtected);
		}
		break;
	case NPC_STATE::kVunerable:
		{
			a_actor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kEssential);
			a_actor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kProtected);
		}
		break;
	default:
		std::unreachable();
	}
}

void Manager::DisableEssentialStatusNPC(NPC_STATE a_state, RE::TESNPC* a_npc) const
{
	switch (a_state) {
	case NPC_STATE::kDisabled:
		break;
	case NPC_STATE::kProtected:
		{
			a_npc->actorData.actorBaseFlags.set(RE::ACTOR_BASE_DATA::Flag::kProtected);
			a_npc->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kEssential);
		}
		break;
	case NPC_STATE::kVunerable:
		{
			a_npc->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kEssential);
			a_npc->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kProtected);
		}
		break;
	default:
		std::unreachable();
	}
}

std::string Manager::GetActorName(const RE::TESObjectREFRPtr& a_actor) const
{
	if (_nnd) {
		if (auto actor = a_actor->As<RE::Actor>()) {
			_nnd->RevealName(actor);
			if (auto name = _nnd->GetName(actor, NND_API::NameContext::kCrosshair); !name.empty()) {
				return { name.data(), name.size() };
			}
		}
	}

	return a_actor->GetDisplayFullName();
}

void Manager::ShowMessage(bool a_showMessage, const std::string& a_message, const std::string& a_notification, const RE::TESObjectREFRPtr& a_actor) const
{
	const auto get_message = [this](const std::string& a_template, const RE::TESObjectREFRPtr& actor) {
		std::string result = a_template;
		REX::STR::REPLACE_ALL(result, "[npc]", GetActorName(actor));
		return result;
	};

	if (a_showMessage) {
		RE::DebugMessageBox(get_message(a_message, a_actor).c_str());
	} else {
		RE::SendHUDMessage::ShowHUDMessage(get_message(a_notification, a_actor).c_str());
	}
}

RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*)
{
	if (!a_event || !a_event->actorDying) {
		return RE::BSEventNotifyControl::kContinue;
	}

	const auto& actor = a_event->actorDying;

	if (auto result = questNPCs.find(actor->GetFormID()); result != questNPCs.end()) {
		if (!a_event->dead) {
			RE::PlaySound("AMBRumbleShakeGreybeardsSD");
			if (enableCameraShake) {
				switch (result->second) {
				case RE::QUEST_DATA::Type::kMiscellaneous:
				case RE::QUEST_DATA::Type::kSideQuest:
					RE::ShakeCamera(0.125f, actor->GetPosition(), 2.0f);
					break;
				default:
					RE::ShakeCamera(0.25f, actor->GetPosition(), 2.0f);
					break;
				}
			}
		} else {
			switch (result->second) {
			case RE::QUEST_DATA::Type::kMiscellaneous:
			case RE::QUEST_DATA::Type::kSideQuest:
				ShowMessage(enableMessageBoxSideQuest, GetMessageSideQuest(), GetNotificationSideQuest(), actor);
				break;
			default:
				ShowMessage(enableMessageBoxVIP, GetMessageVIP(), GetNotificationVIP(), actor);
				break;
			}
		}
	}

	return RE::BSEventNotifyControl::kContinue;
}
