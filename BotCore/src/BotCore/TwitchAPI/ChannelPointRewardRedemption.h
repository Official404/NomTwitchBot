#ifndef __CHANNELPOINTREWARDREDEMPTION_H__
#define __CHANNELPOINTREWARDREDEMPTION_H__
#include <string>

namespace NomBotCore {
	struct ChannelPointRewardRedemption {
		std::string id;
		std::string user_id;
		std::string user_login;
		std::string user_name;
		std::string channel_id;
		std::string channel_login;
		std::string channel_name;
		std::string redeemed_at; // ISO 8601 format
		std::string reward_id;
		std::string reward_title;
		std::string reward_cost;
		std::string prompt;
		std::string status; // "UNFULFILLED", "FULFILLED", "CANCELED"
		std::string user_input; // Optional, may be empty
	};
}

#endif