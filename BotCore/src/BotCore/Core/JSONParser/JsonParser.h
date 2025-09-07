#ifndef __JSONPARSER_H__
#define __JSONPARSER_H__

#include "JsonValue.h"
#include <string>
#include <memory>

namespace NomBotCore {
	class JsonParser {
	public:
		static std::shared_ptr<JsonValue> Parse(const std::string& jsonString, size_t& pos);

	private:
		static std::string ParseJsonString(const std::string& jsonString, size_t& pos);
		static double ParseJsonNumber(const std::string& jsonString, size_t& pos);
		static std::shared_ptr<JsonValue> ParseJsonObject(const std::string& jsonString, size_t& pos);
		static std::shared_ptr<JsonValue> ParseJsonArray(const std::string& jsonString, size_t& pos);
		static bool ParseJsonBoolean(const std::string& jsonString, size_t& pos);
		static std::shared_ptr<NomBotCore::JsonValue> ParseJsonNull(const std::string& jsonString, size_t& pos);
		static bool IsValidJsonRoot(const std::string& jsonString);
	};
}

#endif