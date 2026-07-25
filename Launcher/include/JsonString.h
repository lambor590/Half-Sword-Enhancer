#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace hse {
    enum class JsonStringError : std::uint8_t { MissingField, InvalidJson };

    struct JsonStringField {
        std::string value;
        std::size_t keyOffset = 0;
        std::size_t endOffset = 0;
    };

    /// Finds and decodes the next JSON string field with the requested key at or after startOffset.
    [[nodiscard]] std::expected<JsonStringField, JsonStringError> FindJsonStringField(
        std::string_view json, std::string_view fieldName, std::size_t startOffset = 0
    );
    [[nodiscard]] std::expected<std::string_view, JsonStringError> FindJsonObjectByStringField(
        std::string_view json, std::string_view fieldName, std::string_view fieldValue
    );
}
