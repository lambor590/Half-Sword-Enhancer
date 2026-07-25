#include "../include/JsonString.h"

#include <array>
#include <cctype>

namespace hse {
    namespace {
        struct ParsedString {
            std::string value;
            std::size_t endOffset = 0;
        };

        [[nodiscard]] int HexDigit(char character) noexcept {
            if (character >= '0' && character <= '9') return character - '0';
            if (character >= 'a' && character <= 'f') return character - 'a' + 10;
            if (character >= 'A' && character <= 'F') return character - 'A' + 10;
            return -1;
        }

        [[nodiscard]] std::expected<std::uint16_t, JsonStringError> ParseCodeUnit(
            std::string_view json, std::size_t offset
        ) {
            if (offset + 4 > json.size()) return std::unexpected(JsonStringError::InvalidJson);
            std::uint16_t value = 0;
            for (std::size_t index = 0; index < 4; ++index) {
                const int digit = HexDigit(json[offset + index]);
                if (digit < 0) return std::unexpected(JsonStringError::InvalidJson);
                value = static_cast<std::uint16_t>((value << 4U) | static_cast<std::uint16_t>(digit));
            }
            return value;
        }

        [[nodiscard]] std::expected<std::size_t, JsonStringError> FindStringEnd(
            std::string_view json, std::size_t quoteOffset
        ) {
            if (quoteOffset >= json.size() || json[quoteOffset] != '"')
                return std::unexpected(JsonStringError::InvalidJson);

            for (std::size_t offset = quoteOffset + 1; offset < json.size(); ++offset) {
                const auto character = static_cast<unsigned char>(json[offset]);
                if (character == '"') return offset + 1;
                if (character < 0x20) return std::unexpected(JsonStringError::InvalidJson);
                if (character != '\\') continue;
                if (++offset >= json.size()) return std::unexpected(JsonStringError::InvalidJson);
                if (json[offset] == 'u') {
                    if (!ParseCodeUnit(json, offset + 1)) return std::unexpected(JsonStringError::InvalidJson);
                    offset += 4;
                } else if (std::string_view{"\"\\/bfnrt"}.find(json[offset]) == std::string_view::npos) {
                    return std::unexpected(JsonStringError::InvalidJson);
                }
            }
            return std::unexpected(JsonStringError::InvalidJson);
        }

        void AppendUtf8(std::string& output, std::uint32_t codePoint) {
            if (codePoint <= 0x7f) {
                output.push_back(static_cast<char>(codePoint));
            } else if (codePoint <= 0x7ff) {
                output.push_back(static_cast<char>(0xc0U | (codePoint >> 6U)));
                output.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
            } else if (codePoint <= 0xffff) {
                output.push_back(static_cast<char>(0xe0U | (codePoint >> 12U)));
                output.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3fU)));
                output.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
            } else {
                output.push_back(static_cast<char>(0xf0U | (codePoint >> 18U)));
                output.push_back(static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3fU)));
                output.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3fU)));
                output.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
            }
        }

        [[nodiscard]] std::expected<ParsedString, JsonStringError> ParseString(
            std::string_view json, std::size_t quoteOffset
        ) {
            if (quoteOffset >= json.size() || json[quoteOffset] != '"')
                return std::unexpected(JsonStringError::InvalidJson);
            for (std::size_t offset = quoteOffset + 1; offset < json.size(); ++offset) {
                const auto character = static_cast<unsigned char>(json[offset]);
                if (character == '"') {
                    return ParsedString{
                        .value = std::string(json.substr(quoteOffset + 1, offset - quoteOffset - 1)),
                        .endOffset = offset + 1,
                    };
                }
                if (character < 0x20) return std::unexpected(JsonStringError::InvalidJson);
                if (character == '\\') break;
            }

            ParsedString parsed;
            parsed.value.reserve(32);
            for (std::size_t offset = quoteOffset + 1; offset < json.size(); ++offset) {
                const auto character = static_cast<unsigned char>(json[offset]);
                if (character == '"') {
                    parsed.endOffset = offset + 1;
                    return parsed;
                }
                if (character < 0x20) return std::unexpected(JsonStringError::InvalidJson);
                if (character != '\\') {
                    parsed.value.push_back(static_cast<char>(character));
                    continue;
                }

                if (++offset >= json.size()) return std::unexpected(JsonStringError::InvalidJson);
                switch (json[offset]) {
                    case '"': parsed.value.push_back('"'); break;
                    case '\\': parsed.value.push_back('\\'); break;
                    case '/': parsed.value.push_back('/'); break;
                    case 'b': parsed.value.push_back('\b'); break;
                    case 'f': parsed.value.push_back('\f'); break;
                    case 'n': parsed.value.push_back('\n'); break;
                    case 'r': parsed.value.push_back('\r'); break;
                    case 't': parsed.value.push_back('\t'); break;
                    case 'u': {
                        auto first = ParseCodeUnit(json, offset + 1);
                        if (!first) return std::unexpected(first.error());
                        offset += 4;
                        std::uint32_t codePoint = *first;
                        if (codePoint >= 0xd800 && codePoint <= 0xdbff) {
                            if (offset + 6 >= json.size() || json[offset + 1] != '\\' || json[offset + 2] != 'u')
                                return std::unexpected(JsonStringError::InvalidJson);
                            auto second = ParseCodeUnit(json, offset + 3);
                            if (!second || *second < 0xdc00 || *second > 0xdfff)
                                return std::unexpected(JsonStringError::InvalidJson);
                            codePoint = 0x10000U + ((codePoint - 0xd800U) << 10U) + (*second - 0xdc00U);
                            offset += 6;
                        } else if (codePoint >= 0xdc00 && codePoint <= 0xdfff) {
                            return std::unexpected(JsonStringError::InvalidJson);
                        }
                        AppendUtf8(parsed.value, codePoint);
                        break;
                    }
                    default: return std::unexpected(JsonStringError::InvalidJson);
                }
            }
            return std::unexpected(JsonStringError::InvalidJson);
        }

        [[nodiscard]] std::size_t SkipWhitespace(std::string_view json, std::size_t offset) noexcept {
            while (offset < json.size() && std::isspace(static_cast<unsigned char>(json[offset])))
                ++offset;
            return offset;
        }

        [[nodiscard]] std::expected<std::size_t, JsonStringError> EnclosingObjectStart(
            std::string_view json, std::size_t fieldOffset
        ) {
            std::array<std::size_t, 32> objects{};
            std::size_t depth = 0;
            for (std::size_t offset = 0; offset < fieldOffset;) {
                if (json[offset] == '"') {
                    auto end = FindStringEnd(json, offset);
                    if (!end) return std::unexpected(end.error());
                    offset = *end;
                    continue;
                }
                if (json[offset] == '{') {
                    if (depth == objects.size()) return std::unexpected(JsonStringError::InvalidJson);
                    objects[depth++] = offset;
                } else if (json[offset] == '}') {
                    if (depth == 0) return std::unexpected(JsonStringError::InvalidJson);
                    --depth;
                }
                ++offset;
            }
            if (depth == 0) return std::unexpected(JsonStringError::InvalidJson);
            return objects[depth - 1];
        }

        [[nodiscard]] std::expected<std::size_t, JsonStringError> ObjectEnd(
            std::string_view json, std::size_t objectStart
        ) {
            std::size_t depth = 0;
            for (std::size_t offset = objectStart; offset < json.size();) {
                if (json[offset] == '"') {
                    auto end = FindStringEnd(json, offset);
                    if (!end) return std::unexpected(end.error());
                    offset = *end;
                    continue;
                }
                if (json[offset] == '{')
                    ++depth;
                else if (json[offset] == '}' && --depth == 0)
                    return offset + 1;
                ++offset;
            }
            return std::unexpected(JsonStringError::InvalidJson);
        }
    }

    std::expected<JsonStringField, JsonStringError> FindJsonStringField(
        std::string_view json, std::string_view fieldName, std::size_t startOffset
    ) {
        std::size_t offset = startOffset;
        while ((offset = json.find('"', offset)) != std::string_view::npos) {
            const std::size_t keyOffset = offset;
            auto keyEnd = FindStringEnd(json, offset);
            if (!keyEnd) return std::unexpected(keyEnd.error());
            const auto key = json.substr(offset + 1, *keyEnd - offset - 2);
            offset = SkipWhitespace(json, *keyEnd);
            if (offset >= json.size() || json[offset] != ':') {
                offset = *keyEnd;
                continue;
            }

            offset = SkipWhitespace(json, offset + 1);
            if (key != fieldName) continue;
            auto value = ParseString(json, offset);
            if (!value) return std::unexpected(value.error());
            return JsonStringField{
                .value = std::move(value->value), .keyOffset = keyOffset, .endOffset = value->endOffset
            };
        }
        return std::unexpected(JsonStringError::MissingField);
    }

    std::expected<std::string_view, JsonStringError> FindJsonObjectByStringField(
        std::string_view json, std::string_view fieldName, std::string_view fieldValue
    ) {
        std::size_t searchOffset = 0;
        while (searchOffset < json.size()) {
            auto field = FindJsonStringField(json, fieldName, searchOffset);
            if (!field) return std::unexpected(field.error());
            searchOffset = field->endOffset;
            if (field->value != fieldValue) continue;

            auto start = EnclosingObjectStart(json, field->keyOffset);
            if (!start) return std::unexpected(start.error());
            auto end = ObjectEnd(json, *start);
            if (!end) return std::unexpected(end.error());
            return json.substr(*start, *end - *start);
        }
        return std::unexpected(JsonStringError::MissingField);
    }
}
