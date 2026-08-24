/**
 * @file PluginJsonParser.cpp
 * @brief Implementation of plugin.json parser
 *
 * Uses a minimal recursive-descent JSON parser to avoid external dependencies.
 */

#include "PluginJsonParser.h"

#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>

// ============================================================================
// Minimal JSON Parser (internal)
// ============================================================================

namespace {

/**
 * @class JsonValue
 * @brief Minimal JSON value representation
 */
class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolVal = false;
    double numVal = 0.0;
    std::string strVal;
    std::vector<JsonValue> arrVal;
    std::map<std::string, JsonValue> objVal;
};

/**
 * @class JsonParser
 * @brief Recursive-descent JSON parser
 */
class JsonParser {
public:
    JsonParser(const std::string& text) : text_(text), pos_(0) {}

    bool parse(JsonValue& out, std::string& error) {
        skipWhitespace();
        if (!parseValue(out, error)) return false;
        skipWhitespace();
        if (pos_ < text_.size()) {
            error = "Unexpected trailing characters at position " + std::to_string(pos_);
            return false;
        }
        return true;
    }

private:
    const std::string& text_;
    size_t pos_;

    void skipWhitespace() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            pos_++;
        }
    }

    bool parseValue(JsonValue& out, std::string& error) {
        skipWhitespace();
        if (pos_ >= text_.size()) {
            error = "Unexpected end of input";
            return false;
        }
        char c = text_[pos_];
        if (c == '{') return parseObject(out, error);
        if (c == '[') return parseArray(out, error);
        if (c == '"') return parseString(out, error);
        if (c == 't' || c == 'f') return parseBool(out, error);
        if (c == 'n') return parseNull(out, error);
        return parseNumber(out, error);
    }

    bool parseObject(JsonValue& out, std::string& error) {
        out.type = JsonValue::Type::Object;
        pos_++; // skip '{'
        skipWhitespace();
        if (pos_ < text_.size() && text_[pos_] == '}') {
            pos_++;
            return true;
        }
        while (true) {
            skipWhitespace();
            if (pos_ >= text_.size() || text_[pos_] != '"') {
                error = "Expected string key at position " + std::to_string(pos_);
                return false;
            }
            JsonValue key;
            if (!parseString(key, error)) return false;
            skipWhitespace();
            if (pos_ >= text_.size() || text_[pos_] != ':') {
                error = "Expected ':' at position " + std::to_string(pos_);
                return false;
            }
            pos_++; // skip ':'
            JsonValue val;
            if (!parseValue(val, error)) return false;
            out.objVal[key.strVal] = val;
            skipWhitespace();
            if (pos_ >= text_.size()) {
                error = "Unexpected end of input in object";
                return false;
            }
            if (text_[pos_] == ',') { pos_++; continue; }
            if (text_[pos_] == '}') { pos_++; break; }
            error = "Expected ',' or '}' at position " + std::to_string(pos_);
            return false;
        }
        return true;
    }

    bool parseArray(JsonValue& out, std::string& error) {
        out.type = JsonValue::Type::Array;
        pos_++; // skip '['
        skipWhitespace();
        if (pos_ < text_.size() && text_[pos_] == ']') {
            pos_++;
            return true;
        }
        while (true) {
            JsonValue val;
            if (!parseValue(val, error)) return false;
            out.arrVal.push_back(val);
            skipWhitespace();
            if (pos_ >= text_.size()) {
                error = "Unexpected end of input in array";
                return false;
            }
            if (text_[pos_] == ',') { pos_++; continue; }
            if (text_[pos_] == ']') { pos_++; break; }
            error = "Expected ',' or ']' at position " + std::to_string(pos_);
            return false;
        }
        return true;
    }

    bool parseString(JsonValue& out, std::string& error) {
        out.type = JsonValue::Type::String;
        pos_++; // skip opening '"'
        std::string result;
        while (pos_ < text_.size()) {
            char c = text_[pos_];
            if (c == '"') { pos_++; out.strVal = result; return true; }
            if (c == '\\') {
                pos_++;
                if (pos_ >= text_.size()) { error = "Unterminated escape"; return false; }
                char esc = text_[pos_];
                switch (esc) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'u': {
                        if (pos_ + 4 >= text_.size()) { error = "Invalid unicode escape"; return false; }
                        // Simple: skip 4 hex chars, output '?'
                        pos_ += 4;
                        result += '?';
                        break;
                    }
                    default: result += esc; break;
                }
                pos_++;
            } else {
                result += c;
                pos_++;
            }
        }
        error = "Unterminated string";
        return false;
    }

    bool parseNumber(JsonValue& out, std::string& error) {
        out.type = JsonValue::Type::Number;
        size_t start = pos_;
        if (pos_ < text_.size() && (text_[pos_] == '-' || text_[pos_] == '+')) pos_++;
        while (pos_ < text_.size() && (std::isdigit(static_cast<unsigned char>(text_[pos_])) ||
               text_[pos_] == '.' || text_[pos_] == 'e' || text_[pos_] == 'E' ||
               text_[pos_] == '+' || text_[pos_] == '-')) {
            pos_++;
        }
        std::string numStr = text_.substr(start, pos_ - start);
        try {
            out.numVal = std::stod(numStr);
            out.strVal = numStr; // keep string form for int params
        } catch (...) {
            error = "Invalid number: " + numStr;
            return false;
        }
        return true;
    }

    bool parseBool(JsonValue& out, std::string& error) {
        if (text_.compare(pos_, 4, "true") == 0) {
            out.type = JsonValue::Type::Bool;
            out.boolVal = true;
            pos_ += 4;
            return true;
        }
        if (text_.compare(pos_, 5, "false") == 0) {
            out.type = JsonValue::Type::Bool;
            out.boolVal = false;
            pos_ += 5;
            return true;
        }
        error = "Invalid boolean at position " + std::to_string(pos_);
        return false;
    }

    bool parseNull(JsonValue& out, std::string& error) {
        if (text_.compare(pos_, 4, "null") == 0) {
            out.type = JsonValue::Type::Null;
            pos_ += 4;
            return true;
        }
        error = "Invalid null at position " + std::to_string(pos_);
        return false;
    }
};

// ============================================================================
// Helper: extract string from JsonValue
// ============================================================================

std::string getString(const JsonValue& v, const std::string& key, const std::string& def = "") {
    auto it = v.objVal.find(key);
    if (it == v.objVal.end()) return def;
    if (it->second.type == JsonValue::Type::String) return it->second.strVal;
    if (it->second.type == JsonValue::Type::Number) return it->second.strVal;
    return def;
}

bool getBool(const JsonValue& v, const std::string& key, bool def = false) {
    auto it = v.objVal.find(key);
    if (it == v.objVal.end()) return def;
    if (it->second.type == JsonValue::Type::Bool) return it->second.boolVal;
    return def;
}

int getInt(const JsonValue& v, const std::string& key, int def = 0) {
    auto it = v.objVal.find(key);
    if (it == v.objVal.end()) return def;
    if (it->second.type == JsonValue::Type::Number) return static_cast<int>(it->second.numVal);
    return def;
}

} // anonymous namespace

// ============================================================================
// PluginDescriptor Methods
// ============================================================================

const PluginParameter* PluginDescriptor::findParameter(const std::string& name) const {
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (parameters[i].name == name) return &parameters[i];
    }
    return nullptr;
}

bool PluginDescriptor::validateRequired(const std::map<std::string, std::string>& args,
                                        std::vector<std::string>& missing) const {
    missing.clear();
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (parameters[i].required) {
            if (args.find(parameters[i].name) == args.end()) {
                missing.push_back(parameters[i].name);
            }
        }
    }
    return missing.empty();
}

// ============================================================================
// PluginJsonParser Methods
// ============================================================================

bool PluginJsonParser::parseString(const std::string& jsonStr,
                                   PluginDescriptor& desc,
                                   std::string& error) {
    JsonParser parser(jsonStr);
    JsonValue root;
    if (!parser.parse(root, error)) return false;
    if (root.type != JsonValue::Type::Object) {
        error = "Root JSON value must be an object";
        return false;
    }

    // Required fields
    desc.name = getString(root, "name");
    if (desc.name.empty()) {
        error = "Missing required field: name";
        return false;
    }

    desc.library = getString(root, "library");
    if (desc.library.empty()) {
        error = "Missing required field: library";
        return false;
    }

    desc.className = getString(root, "class");
    if (desc.className.empty()) {
        error = "Missing required field: class";
        return false;
    }

    // Optional fields
    desc.version       = getString(root, "version", "0.0.0");
    desc.author        = getString(root, "author", "unknown");
    desc.description   = getString(root, "description", "");
    desc.category      = getString(root, "category", "sdg-calc");
    desc.minCliVersion = getString(root, "min_cli_version", "0.0.0");

    // Parse parameters array
    auto paramsIt = root.objVal.find("parameters");
    if (paramsIt != root.objVal.end() && paramsIt->second.type == JsonValue::Type::Array) {
        for (const auto& paramVal : paramsIt->second.arrVal) {
            if (paramVal.type != JsonValue::Type::Object) continue;
            PluginParameter param;
            param.name         = getString(paramVal, "name");
            param.shortName    = getString(paramVal, "short");
            param.type         = getString(paramVal, "type", "string");
            param.required     = getBool(paramVal, "required", false);
            param.defaultValue = getString(paramVal, "default");
            param.description  = getString(paramVal, "description");
            if (!param.name.empty()) {
                desc.parameters.push_back(param);
            }
        }
    }

    // Parse inputs array
    auto inputsIt = root.objVal.find("inputs");
    if (inputsIt != root.objVal.end() && inputsIt->second.type == JsonValue::Type::Array) {
        for (const auto& inputVal : inputsIt->second.arrVal) {
            if (inputVal.type != JsonValue::Type::Object) continue;
            PluginInput input;
            input.format      = getString(inputVal, "format");
            input.dtype       = getString(inputVal, "dtype");
            input.bands       = getInt(inputVal, "bands", 1);
            input.description = getString(inputVal, "description");
            desc.inputs.push_back(input);
        }
    }

    // Parse outputs array
    auto outputsIt = root.objVal.find("outputs");
    if (outputsIt != root.objVal.end() && outputsIt->second.type == JsonValue::Type::Array) {
        for (const auto& outputVal : outputsIt->second.arrVal) {
            if (outputVal.type != JsonValue::Type::Object) continue;
            PluginOutput output;
            output.format      = getString(outputVal, "format");
            output.description = getString(outputVal, "description");
            desc.outputs.push_back(output);
        }
    }

    return true;
}

bool PluginJsonParser::parseFile(const std::string& filePath,
                                 PluginDescriptor& desc,
                                 std::string& error) {
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        error = "Cannot open file: " + filePath;
        return false;
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    return parseString(ss.str(), desc, error);
}
