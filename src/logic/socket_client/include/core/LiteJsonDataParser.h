#pragma once

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace socket_dc::lite_json_data {

using Point2D = std::array<double, 2>;
using Point3D = std::array<double, 3>;
using Matrix4D = std::array<std::array<double, 4>, 4>;

struct JsonValue {
    enum class Type {
        Null,
        String,
        Number,
        Boolean,
        Array,
        Object
    };

    Type type = Type::Null;
    std::string stringValue;
    double numberValue = 0.0;
    bool boolValue = false;
    std::vector<JsonValue> arrayValues;
    std::unordered_map<std::string, JsonValue> objectValues;

    static JsonValue makeString(std::string value) {
        JsonValue result;
        result.type = Type::String;
        result.stringValue = std::move(value);
        return result;
    }

    static JsonValue makeNumber(double value) {
        JsonValue result;
        result.type = Type::Number;
        result.numberValue = value;
        return result;
    }

    static JsonValue makeBoolean(bool value) {
        JsonValue result;
        result.type = Type::Boolean;
        result.boolValue = value;
        return result;
    }

    static JsonValue makeArray() {
        JsonValue result;
        result.type = Type::Array;
        return result;
    }

    static JsonValue makeArray(std::initializer_list<JsonValue> values) {
        JsonValue result;
        result.type = Type::Array;
        result.arrayValues.assign(values.begin(), values.end());
        return result;
    }

    static JsonValue makeObject() {
        JsonValue result;
        result.type = Type::Object;
        return result;
    }

    static JsonValue makeObject(std::initializer_list<std::pair<std::string, JsonValue>> values) {
        JsonValue result;
        result.type = Type::Object;
        for (const auto& entry : values) {
            result.objectValues[entry.first] = entry.second;
        }
        return result;
    }

    bool isObject() const {
        return type == Type::Object;
    }

    bool isArray() const {
        return type == Type::Array;
    }

    bool isString() const {
        return type == Type::String;
    }

    bool isNumber() const {
        return type == Type::Number;
    }

    bool isBoolean() const {
        return type == Type::Boolean;
    }
};

namespace detail {

inline bool isWhitespace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

inline bool hasOnlyTrailingWhitespace(const char* cursor) {
    while (*cursor != '\0') {
        if (!isWhitespace(*cursor)) {
            return false;
        }
        ++cursor;
    }
    return true;
}

inline bool parseIntString(const std::string& input, int& value) {
    char* end = nullptr;
    errno = 0;
    const auto parsed = std::strtoll(input.c_str(), &end, 10);
    if (end == input.c_str() || errno != 0 || !hasOnlyTrailingWhitespace(end)) {
        return false;
    }
    if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

inline bool parseDoubleString(const std::string& input, double& value) {
    char* end = nullptr;
    errno = 0;
    const auto parsed = std::strtod(input.c_str(), &end);
    if (end == input.c_str() || errno != 0 || !hasOnlyTrailingWhitespace(end) || !std::isfinite(parsed)) {
        return false;
    }
    value = parsed;
    return true;
}

inline std::string numberToString(double value) {
    std::ostringstream stream;
    stream << std::setprecision(std::numeric_limits<double>::digits10 + 1) << value;
    auto result = stream.str();
    const auto exponentPos = result.find_first_of("eE");
    if (exponentPos != std::string::npos) {
        return result;
    }
    const auto dotPos = result.find('.');
    if (dotPos == std::string::npos) {
        return result;
    }
    while (!result.empty() && result.back() == '0') {
        result.pop_back();
    }
    if (!result.empty() && result.back() == '.') {
        result.pop_back();
    }
    return result;
}

inline std::string escapeString(std::string_view input) {
    std::string result;
    result.reserve(input.size() + 8);
    for (const auto ch : input) {
        switch (ch) {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20U) {
                std::ostringstream stream;
                stream << "\\u" << std::uppercase << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<int>(static_cast<unsigned char>(ch));
                result += stream.str();
            } else {
                result.push_back(ch);
            }
            break;
        }
    }
    return result;
}

inline std::string dumpJsonValueImpl(const JsonValue& value) {
    switch (value.type) {
    case JsonValue::Type::Null:
        return "null";
    case JsonValue::Type::String:
        return '"' + escapeString(value.stringValue) + '"';
    case JsonValue::Type::Number:
        return numberToString(value.numberValue);
    case JsonValue::Type::Boolean:
        return value.boolValue ? "true" : "false";
    case JsonValue::Type::Array: {
        std::string result = "[";
        for (std::size_t index = 0; index < value.arrayValues.size(); ++index) {
            if (index != 0) {
                result.push_back(',');
            }
            result += dumpJsonValueImpl(value.arrayValues[index]);
        }
        result.push_back(']');
        return result;
    }
    case JsonValue::Type::Object: {
        std::vector<std::string> keys;
        keys.reserve(value.objectValues.size());
        for (const auto& entry : value.objectValues) {
            keys.push_back(entry.first);
        }
        std::sort(keys.begin(), keys.end());

        std::string result = "{";
        for (std::size_t index = 0; index < keys.size(); ++index) {
            if (index != 0) {
                result.push_back(',');
            }
            result.push_back('"');
            result += escapeString(keys[index]);
            result += "\":";
            result += dumpJsonValueImpl(value.objectValues.at(keys[index]));
        }
        result.push_back('}');
        return result;
    }
    }
    return "null";
}

inline void appendUtf8(std::string& output, std::uint32_t codePoint) {
    if (codePoint <= 0x7FU) {
        output.push_back(static_cast<char>(codePoint));
        return;
    }
    if (codePoint <= 0x7FFU) {
        output.push_back(static_cast<char>(0xC0U | ((codePoint >> 6U) & 0x1FU)));
        output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        return;
    }
    if (codePoint <= 0xFFFFU) {
        output.push_back(static_cast<char>(0xE0U | ((codePoint >> 12U) & 0x0FU)));
        output.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        return;
    }
    output.push_back(static_cast<char>(0xF0U | ((codePoint >> 18U) & 0x07U)));
    output.push_back(static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3FU)));
    output.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
    output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
}

class Parser {
public:
    explicit Parser(std::string_view input) : current_(input.data()), end_(input.data() + input.size()) {}

    JsonValue parseDocument() {
        skipWhitespace();
        const auto value = parseValue();
        skipWhitespace();
        if (failed_ || current_ != end_) {
            failed_ = true;
            return JsonValue();
        }
        return value;
    }

    bool failed() const {
        return failed_;
    }

private:
    void skipWhitespace() {
        while (current_ != end_ && isWhitespace(*current_)) {
            ++current_;
        }
    }

    bool consume(char expected) {
        if (current_ == end_ || *current_ != expected) {
            return false;
        }
        ++current_;
        return true;
    }

    JsonValue fail() {
        failed_ = true;
        return JsonValue();
    }

    JsonValue parseValue() {
        if (current_ == end_) {
            return fail();
        }

        switch (*current_) {
        case '{':
            return parseObject();
        case '[':
            return parseArray();
        case '"':
            return parseString();
        case 't':
            return parseLiteral("true", JsonValue::makeBoolean(true));
        case 'f':
            return parseLiteral("false", JsonValue::makeBoolean(false));
        case 'n':
            return parseLiteral("null", JsonValue());
        default:
            if (*current_ == '-' || (*current_ >= '0' && *current_ <= '9')) {
                return parseNumber();
            }
            return fail();
        }
    }

    JsonValue parseLiteral(const char* literal, const JsonValue& value) {
        const char* cursor = literal;
        while (*cursor != '\0') {
            if (current_ == end_ || *current_ != *cursor) {
                return fail();
            }
            ++current_;
            ++cursor;
        }
        return value;
    }

    JsonValue parseObject() {
        if (!consume('{')) {
            return fail();
        }

        auto object = JsonValue::makeObject();
        skipWhitespace();
        if (consume('}')) {
            return object;
        }

        while (true) {
            if (current_ == end_ || *current_ != '"') {
                return fail();
            }
            const auto key = parseString();
            if (failed_) {
                return JsonValue();
            }

            skipWhitespace();
            if (!consume(':')) {
                return fail();
            }

            skipWhitespace();
            object.objectValues[key.stringValue] = parseValue();
            if (failed_) {
                return JsonValue();
            }

            skipWhitespace();
            if (consume('}')) {
                return object;
            }
            if (!consume(',')) {
                return fail();
            }
            skipWhitespace();
        }
    }

    JsonValue parseArray() {
        if (!consume('[')) {
            return fail();
        }

        auto array = JsonValue::makeArray();
        skipWhitespace();
        if (consume(']')) {
            return array;
        }

        while (true) {
            array.arrayValues.push_back(parseValue());
            if (failed_) {
                return JsonValue();
            }

            skipWhitespace();
            if (consume(']')) {
                return array;
            }
            if (!consume(',')) {
                return fail();
            }
            skipWhitespace();
        }
    }

    bool parseUnicodeEscape(std::string& result) {
        auto hexValue = [](char ch) -> int {
            if (ch >= '0' && ch <= '9') {
                return ch - '0';
            }
            if (ch >= 'a' && ch <= 'f') {
                return 10 + (ch - 'a');
            }
            if (ch >= 'A' && ch <= 'F') {
                return 10 + (ch - 'A');
            }
            return -1;
        };

        std::uint32_t codePoint = 0;
        for (int index = 0; index < 4; ++index) {
            if (current_ == end_) {
                return false;
            }
            const auto value = hexValue(*current_++);
            if (value < 0) {
                return false;
            }
            codePoint = (codePoint << 4U) | static_cast<std::uint32_t>(value);
        }

        if (codePoint >= 0xD800U && codePoint <= 0xDBFFU) {
            if (end_ - current_ < 6 || current_[0] != '\\' || current_[1] != 'u') {
                return false;
            }
            current_ += 2;
            std::uint32_t lowSurrogate = 0;
            for (int index = 0; index < 4; ++index) {
                const auto value = hexValue(*current_++);
                if (value < 0) {
                    return false;
                }
                lowSurrogate = (lowSurrogate << 4U) | static_cast<std::uint32_t>(value);
            }
            if (lowSurrogate < 0xDC00U || lowSurrogate > 0xDFFFU) {
                return false;
            }
            codePoint = 0x10000U + ((codePoint - 0xD800U) << 10U) + (lowSurrogate - 0xDC00U);
        }

        appendUtf8(result, codePoint);
        return true;
    }

    JsonValue parseString() {
        if (!consume('"')) {
            return fail();
        }

        std::string result;
        while (current_ != end_) {
            const auto ch = *current_++;
            if (ch == '"') {
                return JsonValue::makeString(std::move(result));
            }
            if (static_cast<unsigned char>(ch) < 0x20U) {
                return fail();
            }
            if (ch != '\\') {
                result.push_back(ch);
                continue;
            }

            if (current_ == end_) {
                return fail();
            }

            switch (*current_++) {
            case '"':
                result.push_back('"');
                break;
            case '\\':
                result.push_back('\\');
                break;
            case '/':
                result.push_back('/');
                break;
            case 'b':
                result.push_back('\b');
                break;
            case 'f':
                result.push_back('\f');
                break;
            case 'n':
                result.push_back('\n');
                break;
            case 'r':
                result.push_back('\r');
                break;
            case 't':
                result.push_back('\t');
                break;
            case 'u':
                if (!parseUnicodeEscape(result)) {
                    return fail();
                }
                break;
            default:
                return fail();
            }
        }

        return fail();
    }

    JsonValue parseNumber() {
        const auto start = current_;
        if (*current_ == '-') {
            ++current_;
            if (current_ == end_) {
                return fail();
            }
        }

        if (*current_ == '0') {
            ++current_;
        } else {
            if (*current_ < '1' || *current_ > '9') {
                return fail();
            }
            while (current_ != end_ && *current_ >= '0' && *current_ <= '9') {
                ++current_;
            }
        }

        if (current_ != end_ && *current_ == '.') {
            ++current_;
            if (current_ == end_ || *current_ < '0' || *current_ > '9') {
                return fail();
            }
            while (current_ != end_ && *current_ >= '0' && *current_ <= '9') {
                ++current_;
            }
        }

        if (current_ != end_ && (*current_ == 'e' || *current_ == 'E')) {
            ++current_;
            if (current_ != end_ && (*current_ == '+' || *current_ == '-')) {
                ++current_;
            }
            if (current_ == end_ || *current_ < '0' || *current_ > '9') {
                return fail();
            }
            while (current_ != end_ && *current_ >= '0' && *current_ <= '9') {
                ++current_;
            }
        }

        const std::string token(start, current_);
        double value = 0.0;
        if (!parseDoubleString(token, value)) {
            return fail();
        }
        return JsonValue::makeNumber(value);
    }

    const char* current_;
    const char* end_;
    bool failed_ = false;
};

inline JsonValue parseJsonOrLiteral(const std::string& input) {
    Parser parser(input);
    const auto parsed = parser.parseDocument();
    if (parser.failed()) {
        return JsonValue::makeString(input);
    }
    return parsed;
}

inline const JsonValue* resolvePath(const JsonValue& root, std::string_view path) {
    if (path.empty()) {
        return &root;
    }

    const JsonValue* current = &root;
    std::size_t segmentStart = 0;
    while (segmentStart <= path.size()) {
        const auto separator = path.find('.', segmentStart);
        const auto segmentLength = separator == std::string_view::npos ? path.size() - segmentStart : separator - segmentStart;
        if (segmentLength == 0 || !current->isObject()) {
            return nullptr;
        }

        const auto key = std::string(path.substr(segmentStart, segmentLength));
        const auto found = current->objectValues.find(key);
        if (found == current->objectValues.end()) {
            return nullptr;
        }
        current = &found->second;

        if (separator == std::string_view::npos) {
            return current;
        }
        segmentStart = separator + 1;
    }

    return current;
}

inline std::string toString(const JsonValue& value, const std::string& defaultValue) {
    if (value.isString()) {
        return value.stringValue;
    }
    if (value.isBoolean()) {
        return value.boolValue ? "true" : "false";
    }
    if (value.isNumber()) {
        return numberToString(value.numberValue);
    }
    return defaultValue;
}

inline int toInt(const JsonValue& value, int defaultValue) {
    if (value.isNumber()) {
        if (!std::isfinite(value.numberValue) || std::floor(value.numberValue) != value.numberValue) {
            return defaultValue;
        }
        if (value.numberValue < static_cast<double>(std::numeric_limits<int>::min()) ||
            value.numberValue > static_cast<double>(std::numeric_limits<int>::max())) {
            return defaultValue;
        }
        return static_cast<int>(value.numberValue);
    }
    if (value.isString()) {
        int parsed = defaultValue;
        return parseIntString(value.stringValue, parsed) ? parsed : defaultValue;
    }
    return defaultValue;
}

inline double toDouble(const JsonValue& value, double defaultValue) {
    if (value.isNumber()) {
        return std::isfinite(value.numberValue) ? value.numberValue : defaultValue;
    }
    if (value.isString()) {
        double parsed = defaultValue;
        return parseDoubleString(value.stringValue, parsed) ? parsed : defaultValue;
    }
    return defaultValue;
}

inline bool toBool(const JsonValue& value, bool defaultValue) {
    if (value.isBoolean()) {
        return value.boolValue;
    }
    if (value.isString()) {
        if (value.stringValue == "true") {
            return true;
        }
        if (value.stringValue == "false") {
            return false;
        }
    }
    return defaultValue;
}

inline std::vector<std::string> toStringVector(const JsonValue& value) {
    if (!value.isArray()) {
        return {};
    }

    std::vector<std::string> result;
    result.reserve(value.arrayValues.size());
    for (const auto& item : value.arrayValues) {
        const auto parsed = toString(item, std::string());
        if (parsed.empty() && !(item.isString() && item.stringValue.empty())) {
            return {};
        }
        result.push_back(parsed);
    }
    return result;
}

inline std::vector<double> toDoubleVector(const JsonValue& value) {
    if (!value.isArray()) {
        return {};
    }

    std::vector<double> result;
    result.reserve(value.arrayValues.size());
    for (const auto& item : value.arrayValues) {
        const auto parsed = toDouble(item, std::numeric_limits<double>::quiet_NaN());
        if (!std::isfinite(parsed)) {
            return {};
        }
        result.push_back(parsed);
    }
    return result;
}

template <std::size_t Dimension>
inline std::array<double, Dimension> toPoint(const JsonValue& value, bool& ok) {
    std::array<double, Dimension> result{};
    ok = false;
    if (!value.isArray() || value.arrayValues.size() != Dimension) {
        return result;
    }
    for (std::size_t index = 0; index < Dimension; ++index) {
        const auto parsed = toDouble(value.arrayValues[index], std::numeric_limits<double>::quiet_NaN());
        if (!std::isfinite(parsed)) {
            return result;
        }
        result[index] = parsed;
    }
    ok = true;
    return result;
}

template <std::size_t Dimension>
inline std::vector<std::array<double, Dimension>> toPointVector(const JsonValue& value) {
    if (!value.isArray()) {
        return {};
    }

    std::vector<std::array<double, Dimension>> result;
    result.reserve(value.arrayValues.size());
    for (const auto& item : value.arrayValues) {
        bool ok = false;
        const auto point = toPoint<Dimension>(item, ok);
        if (!ok) {
            return {};
        }
        result.push_back(point);
    }
    return result;
}

inline Matrix4D toMatrix4D(const JsonValue& value, const Matrix4D& defaultValue) {
    Matrix4D result = defaultValue;
    if (!value.isArray() || value.arrayValues.size() != 4) {
        return defaultValue;
    }
    for (std::size_t row = 0; row < 4; ++row) {
        bool ok = false;
        const auto parsed = toPoint<4>(value.arrayValues[row], ok);
        if (!ok) {
            return defaultValue;
        }
        result[row] = parsed;
    }
    return result;
}

template <typename Converter, typename Result>
inline Result fromPath(const JsonValue& root, std::string_view path, Converter converter, Result defaultValue) {
    const auto* selected = resolvePath(root, path);
    if (selected == nullptr) {
        return defaultValue;
    }
    return converter(*selected, defaultValue);
}

}  // namespace detail

inline JsonValue parseJsonValue(const std::string& input) {
    return detail::parseJsonOrLiteral(input);
}

inline std::string dumpJsonValue(const JsonValue& value) {
    return detail::dumpJsonValueImpl(value);
}

inline std::string parseString(const std::string& input, const std::string& defaultValue = "") {
    return detail::toString(detail::parseJsonOrLiteral(input), defaultValue);
}

inline int parseInt(const std::string& input, int defaultValue = 0) {
    return detail::toInt(detail::parseJsonOrLiteral(input), defaultValue);
}

inline double parseDouble(const std::string& input, double defaultValue = 0.0) {
    return detail::toDouble(detail::parseJsonOrLiteral(input), defaultValue);
}

inline bool parseBool(const std::string& input, bool defaultValue = false) {
    return detail::toBool(detail::parseJsonOrLiteral(input), defaultValue);
}

inline std::vector<std::string> parseStringVector(const std::string& input) {
    return detail::toStringVector(detail::parseJsonOrLiteral(input));
}

inline std::vector<double> parseDoubleVector(const std::string& input) {
    return detail::toDoubleVector(detail::parseJsonOrLiteral(input));
}

inline std::vector<Point3D> parsePoints3DVector(const std::string& input) {
    return detail::toPointVector<3>(detail::parseJsonOrLiteral(input));
}

inline std::vector<Point2D> parsePoints2DVector(const std::string& input) {
    return detail::toPointVector<2>(detail::parseJsonOrLiteral(input));
}

inline Matrix4D parseMatrix4D(const std::string& input, const Matrix4D& defaultValue = Matrix4D{}) {
    return detail::toMatrix4D(detail::parseJsonOrLiteral(input), defaultValue);
}

inline std::string getString(const JsonValue& root, std::string_view path, const std::string& defaultValue = "") {
    return detail::fromPath(root, path, detail::toString, defaultValue);
}

inline std::string getString(const std::string& input, std::string_view path, const std::string& defaultValue = "") {
    return getString(detail::parseJsonOrLiteral(input), path, defaultValue);
}

inline int getInt(const JsonValue& root, std::string_view path, int defaultValue = 0) {
    return detail::fromPath(root, path, detail::toInt, defaultValue);
}

inline int getInt(const std::string& input, std::string_view path, int defaultValue = 0) {
    return getInt(detail::parseJsonOrLiteral(input), path, defaultValue);
}

inline double getDouble(const JsonValue& root, std::string_view path, double defaultValue = 0.0) {
    return detail::fromPath(root, path, detail::toDouble, defaultValue);
}

inline double getDouble(const std::string& input, std::string_view path, double defaultValue = 0.0) {
    return getDouble(detail::parseJsonOrLiteral(input), path, defaultValue);
}

inline bool getBool(const JsonValue& root, std::string_view path, bool defaultValue = false) {
    return detail::fromPath(root, path, detail::toBool, defaultValue);
}

inline bool getBool(const std::string& input, std::string_view path, bool defaultValue = false) {
    return getBool(detail::parseJsonOrLiteral(input), path, defaultValue);
}

inline std::vector<std::string> getStringVector(const JsonValue& root, std::string_view path) {
    return detail::fromPath(root, path, [](const JsonValue& value, std::vector<std::string>) {
        return detail::toStringVector(value);
    }, std::vector<std::string>{});
}

inline std::vector<std::string> getStringVector(const std::string& input, std::string_view path) {
    return getStringVector(detail::parseJsonOrLiteral(input), path);
}

inline std::vector<double> getDoubleVector(const JsonValue& root, std::string_view path) {
    return detail::fromPath(root, path, [](const JsonValue& value, std::vector<double>) {
        return detail::toDoubleVector(value);
    }, std::vector<double>{});
}

inline std::vector<double> getDoubleVector(const std::string& input, std::string_view path) {
    return getDoubleVector(detail::parseJsonOrLiteral(input), path);
}

inline std::vector<Point3D> getPoints3DVector(const JsonValue& root, std::string_view path) {
    return detail::fromPath(root, path, [](const JsonValue& value, std::vector<Point3D>) {
        return detail::toPointVector<3>(value);
    }, std::vector<Point3D>{});
}

inline std::vector<Point3D> getPoints3DVector(const std::string& input, std::string_view path) {
    return getPoints3DVector(detail::parseJsonOrLiteral(input), path);
}

inline std::vector<Point2D> getPoints2DVector(const JsonValue& root, std::string_view path) {
    return detail::fromPath(root, path, [](const JsonValue& value, std::vector<Point2D>) {
        return detail::toPointVector<2>(value);
    }, std::vector<Point2D>{});
}

inline std::vector<Point2D> getPoints2DVector(const std::string& input, std::string_view path) {
    return getPoints2DVector(detail::parseJsonOrLiteral(input), path);
}

inline Matrix4D getMatrix4D(const JsonValue& root, std::string_view path, const Matrix4D& defaultValue = Matrix4D{}) {
    return detail::fromPath(root, path, detail::toMatrix4D, defaultValue);
}

inline Matrix4D getMatrix4D(const std::string& input, std::string_view path, const Matrix4D& defaultValue = Matrix4D{}) {
    return getMatrix4D(detail::parseJsonOrLiteral(input), path, defaultValue);
}

}  // namespace socket_dc::lite_json_data
