#include "nompch.h"
#include "JsonParser.h"
#include "JsonValue.h"
#include "../Logging/ImGuiLog.h"

namespace NomBotCore {
	std::shared_ptr<JsonValue> JsonParser::Parse(const std::string& jsonString, size_t& pos) {
		if (!IsValidJsonRoot(jsonString))
			return nullptr;
		while (pos < jsonString.size() && isspace(jsonString[pos])) ++pos;
		if (pos >= jsonString.size()) {
			ImGuiLogManager::AddLog("JsonParser", "Empty JSON string.", LogSeverity::Error);
			return nullptr;
		}
		if (jsonString[pos] == '"') {
			std::string strValue = ParseJsonString(jsonString, pos);
			auto jsonValue = std::make_shared<JsonValue>();
			jsonValue->type = JsonValue::Type::String;
			jsonValue->stringValue = strValue;
			return jsonValue;
		}
		if (jsonString[pos] == '{') {
			return ParseJsonObject(jsonString, pos);
		}
		if (jsonString[pos] == '[') {
			return ParseJsonArray(jsonString, pos);
		}
		if (isdigit(jsonString[pos]) || jsonString[pos] == '-' || jsonString[pos] == '+') {
			double numValue = ParseJsonNumber(jsonString, pos);
			auto jsonValue = std::make_shared<JsonValue>();
			jsonValue->type = JsonValue::Type::Number;
			jsonValue->numberValue = numValue;
			return jsonValue;
		}
		if (jsonString.compare(pos, 4, "true") == 0 || jsonString.compare(pos, 5, "false") == 0) {
			bool boolValue = ParseJsonBoolean(jsonString, pos);
			auto jsonValue = std::make_shared<JsonValue>();
			jsonValue->type = JsonValue::Type::Boolean;
			jsonValue->boolValue = boolValue;
			return jsonValue;
		}
		if (jsonString.compare(pos, 4, "null") == 0) {
			ParseJsonNull(jsonString, pos);
			auto jsonValue = std::make_shared<JsonValue>();
			jsonValue->type = JsonValue::Type::Null;
			return jsonValue;
		}
	}

	std::string JsonParser::ParseJsonString(const std::string& jsonString, size_t& pos)
	{
		if (jsonString[pos] != '"') {
			ImGuiLogManager::AddLog("JsonParser", "Expected '\"' at the beginning of JSON string.", LogSeverity::Error);
			return "";
		}
		++pos;
		std::string result;
		while (pos < jsonString.size()) {
			char c = jsonString[pos++];
			if (c == '"') {
				return result;
			}
			else if (c == '\\') {
				if (pos >= jsonString.size()) {
					throw std::runtime_error("Invalid escape sequence in JSON string.");
				}
				char nextChar = jsonString[pos++];
				switch (nextChar) {
				case '"': result += '"'; break;
				case '\\': result += '\\'; break;
				case '/': result += '/'; break;
				case 'b': result += '\b'; break;
				case 'f': result += '\f'; break;
				case 'n': result += '\n'; break;
				case 'r': result += '\r'; break;
				case 't': result += '\t'; break;
				case 'u':
					// Unicode escape sequence handling can be added here
					ImGuiLogManager::AddLog("JsonParser", "Unicode escape sequences are not supported in this implementation.", LogSeverity::Warning);
					break;
				default:
					ImGuiLogManager::AddLog("JsonParser", std::string("Invalid escape character: \\") + nextChar, LogSeverity::Error);
					return "";
				}
			}
			else {
				result += c;
			}
		}
		throw std::runtime_error("Unterminated string in JSON input.");
	}

	double JsonParser::ParseJsonNumber(const std::string& jsonString, size_t& pos)
	{
		size_t start = pos;
		while (pos < jsonString.size() && (isdigit(jsonString[pos]) || jsonString[pos] == '-' || jsonString[pos] == '+' || jsonString[pos] == '.' || jsonString[pos] == 'e' || jsonString[pos] == 'E')) {
			++pos;
		}
		if (start == pos) {
			ImGuiLogManager::AddLog("JsonParser", "Invalid JSON number format.", LogSeverity::Error);
			return 0;
		}
		try {
			double number = std::stod(jsonString.substr(start, pos - start));
			return number;
		}
		catch (const std::exception& e) {
			ImGuiLogManager::AddLog("JsonParser", std::string("Error parsing JSON number: ") + e.what(), LogSeverity::Error);
			return 0;
		}
	}

	std::shared_ptr<NomBotCore::JsonValue> JsonParser::ParseJsonObject(const std::string& jsonString, size_t& pos)
	{
		if (jsonString[pos] != '{') {
			ImGuiLogManager::AddLog("JsonParser", "Expected '{' at the beginning of JSON object.", LogSeverity::Error);
			return nullptr;
		}
		++pos;
		auto jsonValue = std::make_shared<JsonValue>();
		jsonValue->type = JsonValue::Type::Object;
		while (pos < jsonString.size()) {
			while (pos < jsonString.size() && isspace(jsonString[pos])) ++pos;
			if (pos < jsonString.size() && jsonString[pos] == '}') {
				++pos;
				return jsonValue;
			}
			std::string key = ParseJsonString(jsonString, pos);
			while (pos < jsonString.size() && isspace(jsonString[pos])) ++pos;
			if (pos >= jsonString.size() || jsonString[pos] != ':') {
				ImGuiLogManager::AddLog("JsonParser", "Expected ':' after key in JSON object.", LogSeverity::Error);
				return nullptr;
			}
			++pos;
			while (pos < jsonString.size() && isspace(jsonString[pos])) ++pos;
			std::shared_ptr<JsonValue> value = Parse(jsonString, pos);
			if (!value) {
				ImGuiLogManager::AddLog("JsonParser", "Failed to parse value in JSON object.", LogSeverity::Error);
				return nullptr;
			}
			// After parsing value
			jsonValue->objectValues[key] = value;
			while (pos < jsonString.size() && isspace(jsonString[pos])) ++pos;
			if (pos < jsonString.size() && jsonString[pos] == ',') {
				++pos;
				continue; // Parse next key-value pair
			}
			else if (pos < jsonString.size() && jsonString[pos] == '}') {
				++pos;
				return jsonValue; // End of object
			}
			else {
				ImGuiLogManager::AddLog("JsonParser", std::string("Expected ',' or '}' in JSON object, found: '") + jsonString[pos] + "'", LogSeverity::Error);
				return nullptr;
			}
		}
		throw std::runtime_error("Unterminated JSON object.");
	}

	std::shared_ptr<NomBotCore::JsonValue> JsonParser::ParseJsonArray(const std::string& jsonString, size_t& pos)
	{
		if (jsonString[pos] != '[') {
			ImGuiLogManager::AddLog("JsonParser", "Expected '[' at the beginning of JSON array.", LogSeverity::Error);
			return nullptr;
		}
		++pos;
		auto jsonValue = std::make_shared<JsonValue>();
		jsonValue->type = JsonValue::Type::Array;
		while (pos < jsonString.size()) {
			while (pos < jsonString.size() && isspace(jsonString[pos])) ++pos;
			if (pos < jsonString.size() && jsonString[pos] == ']') {
				++pos;
				return jsonValue;
			}
			std::shared_ptr<JsonValue> element = Parse(jsonString, pos);
			if (!element) {
				ImGuiLogManager::AddLog("JsonParser", "Failed to parse element in JSON array.", LogSeverity::Error);
				return nullptr;
			}
			jsonValue->arrayValues.push_back(element);
			while (pos < jsonString.size() && isspace(jsonString[pos])) ++pos;
			if (pos < jsonString.size() && jsonString[pos] == ',') {
				++pos;
				continue;
			}
			else if (pos < jsonString.size() && jsonString[pos] == ']') {
				++pos;
				return jsonValue;
			}
			else {
				ImGuiLogManager::AddLog("JsonParser", "Expected ',' or ']' in JSON array.", LogSeverity::Error);
				return nullptr;
			}
		}
		throw std::runtime_error("Unterminated JSON array.");
	}

	bool JsonParser::ParseJsonBoolean(const std::string& jsonString, size_t& pos)
	{
		if (jsonString.compare(pos, 4, "true") == 0) {
			pos += 4;
			return true;
		}
		else if (jsonString.compare(pos, 5, "false") == 0) {
			pos += 5;
			return false;
		}
		else {
			ImGuiLogManager::AddLog("JsonParser", "Invalid JSON boolean value.", LogSeverity::Error);
			return false;
		}
	}

	std::shared_ptr<NomBotCore::JsonValue> JsonParser::ParseJsonNull(const std::string& jsonString, size_t& pos)
	{
		if (jsonString.compare(pos, 4, "null") == 0) {
			pos += 4;
			auto jsonValue = std::make_shared<JsonValue>();
			jsonValue->type = JsonValue::Type::Null;
			return jsonValue;
		}
		else {
			ImGuiLogManager::AddLog("JsonParser", "Invalid JSON null value.", LogSeverity::Error);
			return nullptr;
		}
	}

	bool JsonParser::IsValidJsonRoot(const std::string& jsonString)
	{
		size_t pos = 0;
		while (pos < jsonString.size() && isspace(jsonString[pos])) ++pos;
		if (pos >= jsonString.size()) return false;
		return jsonString[pos] == '{' || jsonString[pos] == '[';
	}
}