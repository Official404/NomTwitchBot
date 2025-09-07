#include "ChannelPointRewardRedemption.h"
#include <sstream>

namespace NomTwitchBot {
	void ChannelPointRewardRedemption::ProcessRedemption(const NomBotCore::ChannelPointRewardRedemption& redemption) {
		std::stringstream ss;
		ss << "Redemption ID: " << redemption.id << ", User: " << redemption.user_name << ", Reward: " << redemption.reward_title << ", Prompt: " << redemption.prompt << ", User Input: " << redemption.user_input << ", Status: " << redemption.status;
		NomBotCore::ImGuiLogManager::AddLog("ChannelPointRedemption", ss.str(), NomBotCore::LogSeverity::Info);
	}
} // namespace NomTwitchBot