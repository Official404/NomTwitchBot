#ifndef	__CHANNELPOINTREDEMPTION_H__
#define __CHANNELPOINTREDEMPTION_H__
#include <BotCore/TwitchAPI/ChannelPointRewardRedemption.h>
#include <BotCore/Core/Logging/ImGuiLog.h>

namespace NomTwitchBot {
	class ChannelPointRewardRedemption {
	public:
		static void ProcessRedemption(const NomBotCore::ChannelPointRewardRedemption& redemption);
	};
} // namespace NomTwitchBot

#endif