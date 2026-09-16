#pragma once

#include "NND_API.h"

class Manager :
	public REX::TSingleton<Manager>,
	public RE::BSTEventSink<RE::TESDeathEvent>
{
public:
	enum class NPC_STATE
	{
		kDisabled,
		kProtected,
		kVunerable
	};

	void Register();
	void RequestAPI();
	void LoadSettings();
	void BuildExclusionList();
	void DisableEssentialStatus(RE::Actor* a_actor, bool a_essential);

private:
	static RE::TESNPC*                         GetActorBase(RE::Actor* a_actor);
	bool                                       IsExcluded(RE::Actor* a_actor) const;
	static std::optional<RE::QUEST_DATA::Type> GetQuestType(RE::Actor* a_actor, bool a_essential);
	std::string                                GetActorName(const RE::TESObjectREFRPtr& a_actor) const;
	void                                       ShowMessage(bool a_showMessage, const std::string& a_message, const std::string& a_notification, const RE::TESObjectREFRPtr& a_actor) const;

	RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>* a_eventSource) override;

	[[nodiscard]] NPC_STATE GetGeneralNPCState() const { return static_cast<NPC_STATE>(generalNPCState.GetValue()); }
	[[nodiscard]] NPC_STATE GetFollowerNPCState() const { return static_cast<NPC_STATE>(followerNPCState.GetValue()); }
	[[nodiscard]] NPC_STATE GetSideQuestNPCState() const { return static_cast<NPC_STATE>(sideQuestNPCState.GetValue()); }

	[[nodiscard]] const std::string& GetMessageVIP() const { return stl::get_setting_ref(messageVIP); }
	[[nodiscard]] const std::string& GetMessageSideQuest() const { return stl::get_setting_ref(messageSideQuest); }
	[[nodiscard]] const std::string& GetNotificationVIP() const { return stl::get_setting_ref(notificationVIP); }
	[[nodiscard]] const std::string& GetNotificationSideQuest() const { return stl::get_setting_ref(notificationSideQuest); }

	// members
	static constexpr auto path = R"(Data\SKSE\Plugins\po3_EssentialsBeGone.ini)"sv;

	NND_API::IVNND2* _nnd{ nullptr };

	REX::TIniSetting<std::uint32_t> generalNPCState{ "Settings", "iGeneralNPCState", std::to_underlying(NPC_STATE::kProtected) };
	REX::TIniSetting<std::uint32_t> sideQuestNPCState{ "Settings", "iSideQuestNPCState", std::to_underlying(NPC_STATE::kProtected) };
	REX::TIniSetting<std::uint32_t> followerNPCState{ "Settings", "iFollowerNPCState", std::to_underlying(NPC_STATE::kDisabled) };

	REX::TIniSetting<bool> enableMessageBoxVIP{ "Settings", "bShowMessageboxVIP", true };
	REX::TIniSetting<bool> enableMessageBoxSideQuest{ "Settings", "bShowMessageboxSideQuest", true };
	REX::TIniSetting<bool> enableCameraShake{ "Settings", "bEnableCameraShake", true };

	REX::TIniSettingA<std::string> npcExclusions{ "Settings", "sNPCExcludeList", ""s, ","sv };

	REX::TIniSetting<std::string> messageVIP{ "Messages", "sMessageboxVIP",
		"With [npc]'s death, the thread of prophecy is severed. Restore a saved game to restore the weave of fate, or persist in the doomed world you have created." };
	REX::TIniSetting<std::string> messageSideQuest{ "Messages", "sMessageboxSideQuest",
		"With [npc]'s death, the thread of prophecy is damaged. Restore a saved game to repair the weave of fate, or persist in the doomed world you have created." };
	REX::TIniSetting<std::string> notificationVIP{ "Messages", "sNotificationVIP",
		"Your actions have broken the thread of prophecy..." };
	REX::TIniSetting<std::string> notificationSideQuest{ "Messages", "sNotificationSideQuest",
		"Your actions have caused a shift throughout the weave of fate..." };

	Set<RE::FormID>                                 excludedNPCs;
	ConcurrentMap<RE::FormID, RE::QUEST_DATA::Type> questNPCs;
};
