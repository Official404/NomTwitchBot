#ifndef __JSONVALUE_H__
#define __JSONVALUE_H__
#include <string>
#include <map>
#include <vector>
#include <memory>
#include "../Logging/ImGuiLog.h"


namespace NomBotCore {
	struct JsonValue {
		enum class Type {
			Null,
			Boolean,
			Number,
			String,
			Array,
			Object
		};
		Type type;
		std::string stringValue;
		double numberValue;
		bool boolValue;
		std::vector<std::shared_ptr<JsonValue>> arrayValues;
		std::map<std::string, std::shared_ptr<JsonValue>> objectValues;
		JsonValue() : type(Type::Null), numberValue(0), boolValue(false) {}

		void DumpToLog(const std::string& prefix = "") const {
			switch (type) {
			case Type::Null:
				ImGuiLogManager::AddLog("JsonParser", prefix + "null", LogSeverity::Info);
				break;
			case Type::Boolean:
				ImGuiLogManager::AddLog("JsonParser", prefix + (boolValue ? "true" : "false"), LogSeverity::Info);
				break;
			case Type::Number:
				ImGuiLogManager::AddLog("JsonParser", prefix + std::to_string(numberValue), LogSeverity::Info);
				break;
			case Type::String:
				ImGuiLogManager::AddLog("JsonParser", prefix + "\"" + stringValue + "\"", LogSeverity::Info);
				break;
			case Type::Object:
				ImGuiLogManager::AddLog("JsonParser", prefix + "{", LogSeverity::Info);
				for (const auto& kv : objectValues) {
					ImGuiLogManager::AddLog("JsonParser", prefix + "  \"" + kv.first + "\":", LogSeverity::Info);
					if (kv.second) kv.second->DumpToLog(prefix + "    ");
				}
				ImGuiLogManager::AddLog("JsonParser", prefix + "}", LogSeverity::Info);
				break;
			case Type::Array:
				ImGuiLogManager::AddLog("JsonParser", prefix + "[", LogSeverity::Info);
				for (const auto& v : arrayValues) {
					if (v) v->DumpToLog(prefix + "  ");
				}
				ImGuiLogManager::AddLog("JsonParser", prefix + "]", LogSeverity::Info);
				break;
			}
		}
	};
}

#endif