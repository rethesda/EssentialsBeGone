#include "Manager.h"

void Manager::Register()
{
	if (auto scriptEventSource = RE::ScriptEventSourceHolder::GetSingleton()) {
		scriptEventSource->AddEventSink(this);
	}

	BuildExclusionList();
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

void Manager::BuildExclusionList()
{
	const auto& npcExclusionsA = stl::get_setting_ref(npcExclusions);
	if (npcExclusionsA.empty()) {
		REX::INFO("No excluded NPCs found");
		return;
	}

	for (const auto& npcEDID : npcExclusionsA) {
		if (auto npc = RE::TESForm::LookupByEditorID<RE::TESNPC>(npcEDID)) {
			excludedNPCs.insert(npc->GetFormID());
		}
	}

	REX::INFO("Resolved {}/{} excluded NPCs", excludedNPCs.size(), npcExclusionsA.size());
}

void Manager::DisableEssentialStatus(RE::Actor* a_actor, bool a_essential)
{
	auto state = a_actor->IsPlayerTeammate() ? GetFollowerNPCState() : GetGeneralNPCState();
	if (state == NPC_STATE::kDisabled) {
		return;
	}
	
	if (IsExcluded(a_actor)) {
		return;
	}

	if (const auto questType = GetQuestType(a_actor, a_essential)) {
		if (state == NPC_STATE::kVunerable) {
			switch (*questType) {
			case RE::QUEST_DATA::Type::kMiscellaneous:
			case RE::QUEST_DATA::Type::kSideQuest:
				state = GetSideQuestNPCState() == NPC_STATE::kVunerable ? NPC_STATE::kVunerable : NPC_STATE::kProtected;
				break;
			default:
				state = NPC_STATE::kProtected;
				break;
			}
		}
		questNPCs.insert_or_assign(a_actor->GetFormID(), *questType);
	} else {
		questNPCs.erase(a_actor->GetFormID());
	}

	switch (state) {
	case NPC_STATE::kProtected:
		a_actor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kEssential);
		a_actor->boolFlags.set(RE::Actor::BOOL_FLAGS::kProtected);
		break;
	case NPC_STATE::kVunerable:
		a_actor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kEssential, RE::Actor::BOOL_FLAGS::kProtected);
		break;
	default:
		break;
	}
}

RE::TESNPC* Manager::GetActorBase(RE::Actor* a_actor)
{
	if (const auto xLevCreature = a_actor->extraList.GetByType<RE::ExtraLeveledCreature>()) {
		if (const auto original = xLevCreature->originalBase ? xLevCreature->originalBase->As<RE::TESNPC>() : nullptr) {
			return original;
		}
	}
	return a_actor->GetActorBase();
}

bool Manager::IsExcluded(RE::Actor* a_actor) const
{
	if (excludedNPCs.empty()) {
		return false;
	}
	if (auto actorbase = GetActorBase(a_actor); actorbase && excludedNPCs.contains(actorbase->GetFormID())) {
		return true;
	}
	return false;
}

std::optional<RE::QUEST_DATA::Type> Manager::GetQuestType(RE::Actor* a_actor, bool a_essential)
{
	const auto xAliases = a_actor->extraList.GetByType<RE::ExtraAliasInstanceArray>();
	if (!xAliases) {
		return std::nullopt;
	}

	RE::BSReadLockGuard locker(xAliases->lock);
	for (const auto& aliasData : xAliases->aliases) {
		if (aliasData) {
			if (const auto quest = aliasData->quest; quest && quest->GetType() != RE::QUEST_DATA::Type::kNone) {
				if (a_essential || (aliasData->alias && aliasData->alias->flags.any(RE::BGSBaseAlias::FLAGS::kEssential))) {
					return quest->GetType();
				}
			}
		}
	}

	return std::nullopt;
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
		RE::MessageBoxMenu::Create(get_message(a_message, a_actor).c_str(), nullptr, 0, 4, 10, const_cast<const char*>(*"sOk"_gs));
	} else {
		RE::SendHUDMessage::ShowHUDMessage(get_message(a_notification, a_actor).c_str());
	}
}

RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*)
{
	if (!a_event || !a_event->actorDying || a_event->actorDying->IsPlayerRef()) {
		return RE::BSEventNotifyControl::kContinue;
	}

	const auto& actor = a_event->actorDying;

	questNPCs.erase_if(actor->GetFormID(), [this, actor, dead = a_event->dead](const auto& result) {
		if (!dead) {
			RE::PlaySound("AMBRumbleShakeGreybeardsSD");
			if (enableCameraShake) {
				switch (result.second) {
				case RE::QUEST_DATA::Type::kMiscellaneous:
				case RE::QUEST_DATA::Type::kSideQuest:
					RE::ShakeCamera(0.125f, actor->GetPosition(), 2.0f);
					break;
				default:
					RE::ShakeCamera(0.25f, actor->GetPosition(), 2.0f);
					break;
				}
			}
			return false;
		} else {
			switch (result.second) {
			case RE::QUEST_DATA::Type::kMiscellaneous:
			case RE::QUEST_DATA::Type::kSideQuest:
				ShowMessage(enableMessageBoxSideQuest, GetMessageSideQuest(), GetNotificationSideQuest(), actor);
				break;
			default:
				ShowMessage(enableMessageBoxVIP, GetMessageVIP(), GetNotificationVIP(), actor);
				break;
			}
			return true;
		}
	});

	return RE::BSEventNotifyControl::kContinue;
}
