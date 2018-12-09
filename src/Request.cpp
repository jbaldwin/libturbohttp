#include "liquid/Request.h"

#include <charconv>
#include <cstring>

namespace liquid
{

static constexpr char HTTP_SP = ' ';
static constexpr char HTTP_CR = '\r';
static constexpr char HTTP_LF = '\n';
static constexpr char HTTP_HTAB = '\t';

#define EXPECT(C, E) { if(data[++m_pos] != C) { return E; }}
#define ADVANCE() { ++m_pos; }

/**
 * Converts a char from uppercase to lowercase ascii.
 * @param c
 * @return
 */
inline auto tolower_ascii(int c) -> int
{
    if(c >= 65 && c <= 90)
    {
        return c + 32;
    }
    return c;
}

inline auto tolower_ascii2(int c) -> int
{
    return c + ((static_cast<unsigned int>(c - 'A') < 26u) << 5);
}

static constexpr int tolower_table_add[] = {
    0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
    32,32,32,32,32,32,32,32,32,32,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

inline auto tolower_asciitable_add(int c) -> int
{
    return c + tolower_table_add[c];
}

static constexpr unsigned char tolower_table[] = {
      0,
      1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
     17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
     33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
     49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64,
     97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,112,
    113,114,115,116,117,118,119,120,121,122, 91, 92, 93, 94, 95, 96,
     97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,112,
    113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,
    129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,
    145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,
    161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,
    177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,
    193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,
    209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,
    225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,
    241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};

inline auto tolower_asciitable(int c) -> int
{
    return tolower_table[c];
}

/**
 * Case insensitive comparison of two string views.
 * @param a String view A to compare.
 * @param b String view B to compare.
 * @return True if a == b with case insensitive comparison.
 */
auto string_view_icompare(std::string_view a, std::string_view b) -> bool
{
    if(a.length() != b.length())
    {
        return false;
    }

    const size_t len = a.length();
    for(size_t i = 0; i < len; ++i)
    {
//        if(std::tolower(a[i]) != std::tolower(b[i]))
//        if(tolower_ascii(a[i]) != tolower_ascii(b[i]))
//        if(tolower_ascii2(a[i]) != tolower_ascii2(b[i]))
        if(tolower_asciitable_add(a[i]) != tolower_asciitable_add(b[i]))
//        if(tolower_asciitable(a[i]) != tolower_asciitable(b[i]))
        {
            return false;
        }
    }

    return true;
}

auto is_ws(char c) -> bool
{
    switch(c)
    {
        case HTTP_SP:
        case HTTP_HTAB:
            return true;
        default:
            return false;
    }
}

auto Request::Parse(std::string& data) -> ParseResult
{
    if(data.empty())
    {
        return ParseResult::INCOMPLETE;
    }

    //size_t data_length = data.length();

    if(m_parse_state == ParseState::START)
    {
        auto result = parseMethod(data);
        if(result != ParseResult::CONTINUE)
        {
            return result;
        }
    }

    // After parsing the Method move on to parsing the URI.
    if(m_parse_state == ParseState::PARSED_METHOD)
    {
        auto result = parseUri(data);
        if(result != ParseResult::CONTINUE)
        {
            return result;
        }
    }

    if(m_parse_state == ParseState::PARSED_URI)
    {
        auto result = parseVersion(data);
        if(result != ParseResult::CONTINUE)
        {
            return result;
        }
    }

    if(m_parse_state == ParseState::PARSED_VERSION)
    {
        auto result = parseHeaders(data);
        if(result != ParseResult::CONTINUE)
        {
            return result;
        }
    }

    // There may or may not be a body
    if(m_parse_state == ParseState::PARSED_HEADERS)
    {
        auto result = parseBody(data);
        if(result != ParseResult::CONTINUE)
        {
            return result;
        }
    }

    return ParseResult::COMPLETE;
}

auto Request::parseMethod(std::string& data) -> ParseResult
{
    size_t data_length = data.length();

    switch(data[0])
    {
        // GET
        case 'G':
        {
            // do we have enough room to parse "G" "ET "?
            if(m_pos + 3 < data_length)
            {
                EXPECT('E', ParseResult::METHOD_UNKNOWN);
                EXPECT('T', ParseResult::METHOD_UNKNOWN);
                EXPECT(HTTP_SP, ParseResult::METHOD_UNKNOWN);
                m_method = Method::GET;
            }
            else
            {
                return ParseResult::INCOMPLETE;
            }
        }
            break;
        // POST
        // PUT
        // PATCH
        case 'P':
        {
            // we need to guarantee 1 character for which P method this is
            if(m_pos + 1 < data_length)
            {
                // Only advance once for this check, since its multiple branches.
                switch(data[++m_pos])
                {
                    case 'O':
                    {
                        // "PO" "ST "
                        if(m_pos + 3 < data_length)
                        {
                            EXPECT('S', ParseResult::METHOD_UNKNOWN);
                            EXPECT('T', ParseResult::METHOD_UNKNOWN);
                            EXPECT(HTTP_SP, ParseResult::METHOD_UNKNOWN);
                            m_method = Method::POST;
                        }
                        else
                        {
                            return ParseResult::INCOMPLETE;
                        }
                    }
                        break;
                    case 'U':
                    {
                        // "PU" "T "
                        if(m_pos + 2 < data_length)
                        {
                            EXPECT('T', ParseResult::METHOD_UNKNOWN);
                            EXPECT(HTTP_SP, ParseResult::METHOD_UNKNOWN);
                            m_method = Method::PUT;
                        }
                        else
                        {
                            return ParseResult::INCOMPLETE;
                        }
                    }
                        break;
                    case 'A':
                    {
                        // "PA" "TCH "
                        if(m_pos + 4 < data_length)
                        {
                            EXPECT('T', ParseResult::METHOD_UNKNOWN);
                            EXPECT('C', ParseResult::METHOD_UNKNOWN);
                            EXPECT('H', ParseResult::METHOD_UNKNOWN);
                            EXPECT(HTTP_SP, ParseResult::METHOD_UNKNOWN);
                            m_method = Method::PATCH;
                        }
                        else
                        {
                            return ParseResult::INCOMPLETE;
                        }
                    }
                        break;
                    default:
                        return ParseResult::INCOMPLETE;
                }
            }
            else
            {
                return ParseResult::INCOMPLETE;
            }

        }
            break;
        // HEAD
        case 'H':
        {
            // do we have enough room to parse "H ""EAD "?
            if(m_pos + 4 < data_length)
            {
                EXPECT('E', ParseResult::METHOD_UNKNOWN);
                EXPECT('A', ParseResult::METHOD_UNKNOWN);
                EXPECT('D', ParseResult::METHOD_UNKNOWN);
                EXPECT(HTTP_SP, ParseResult::METHOD_UNKNOWN);
                m_method = Method::HEAD;
            }
            else
            {
                return ParseResult::INCOMPLETE;
            }
        }
            break;
        // DELETE
        case 'D':
        {
            // do we have enough room to parse "D" "ELETE "?
            if(m_pos + 6 < data_length)
            {
                EXPECT('E', ParseResult::METHOD_UNKNOWN);
                EXPECT('L', ParseResult::METHOD_UNKNOWN);
                EXPECT('E', ParseResult::METHOD_UNKNOWN);
                EXPECT('T', ParseResult::METHOD_UNKNOWN);
                EXPECT('E', ParseResult::METHOD_UNKNOWN);
                EXPECT(HTTP_SP, ParseResult::METHOD_UNKNOWN);
                m_method = Method::DELETE;
            }
            else
            {
                return ParseResult::INCOMPLETE;
            }
        }
            break;
        // CONNECT
        case 'C':
        {
            // do we have enough room to parse "C" "ONNECT "?
            if(m_pos + 7 < data_length)
            {
                EXPECT('O', ParseResult::METHOD_UNKNOWN);
                EXPECT('N', ParseResult::METHOD_UNKNOWN);
                EXPECT('N', ParseResult::METHOD_UNKNOWN);
                EXPECT('E', ParseResult::METHOD_UNKNOWN);
                EXPECT('C', ParseResult::METHOD_UNKNOWN);
                EXPECT('T', ParseResult::METHOD_UNKNOWN);
                EXPECT(HTTP_SP, ParseResult::METHOD_UNKNOWN);
                m_method = Method::CONNECT;
            }
            else
            {
                return ParseResult::INCOMPLETE;
            }
        }
            break;
        // OPTIONS
        case 'O':
        {
            // do we have enough room to parse "O" "PTIONS "?
            if(m_pos + 7 < data_length)
            {
                EXPECT('P', ParseResult::METHOD_UNKNOWN);
                EXPECT('T', ParseResult::METHOD_UNKNOWN);
                EXPECT('I', ParseResult::METHOD_UNKNOWN);
                EXPECT('O', ParseResult::METHOD_UNKNOWN);
                EXPECT('N', ParseResult::METHOD_UNKNOWN);
                EXPECT('S', ParseResult::METHOD_UNKNOWN);
                EXPECT(HTTP_SP, ParseResult::METHOD_UNKNOWN);
                m_method = Method::OPTIONS;
            }
            else
            {
                return ParseResult::INCOMPLETE;
            }
        }
            break;
        // TRACE
        case 'T':
        {
            // do we have enough room to parse "T" "RACE "?

            if(m_pos + 5 < data_length)
            {
                EXPECT('R', ParseResult::METHOD_UNKNOWN);
                EXPECT('A', ParseResult::METHOD_UNKNOWN);
                EXPECT('C', ParseResult::METHOD_UNKNOWN);
                EXPECT('E', ParseResult::METHOD_UNKNOWN);
                EXPECT(HTTP_SP, ParseResult::METHOD_UNKNOWN);
                m_method = Method::TRACE;
            }
            else
            {
                return ParseResult::INCOMPLETE;
            }
        }
            break;
        default:
            return ParseResult::METHOD_UNKNOWN;
    }

    // If the parser gets this far then its successfully parsed the HTTP Method.
    m_parse_state = ParseState::PARSED_METHOD;

    return ParseResult::CONTINUE;
}

auto Request::parseUri(std::string& data) -> ParseResult
{
    ADVANCE(); // Previous parse leaves us on the HTTP_SP token before the URI, advance past it.
    size_t data_length = data.length();
    if(m_uri_start_pos == 0)
    {
        // Set the start pos once (subsequent Parse calls could have different m_pos!)
        // saved for calculating the view of the URI.
        m_uri_start_pos = m_pos;
    }

    // Advance until the next HTTP_SP is found
    while(true)
    {
        if(m_pos < data_length)
        {
            if(data[m_pos] != HTTP_SP)
            {
                // only advance if we don't find the HTTP_SP
                ADVANCE();
                continue; // while(true)
            }
            else
            {
                // We've found the end of the URI.
                break; // while(true)
            }
        }
        else
        {
            // If the end of the data is found with no HTTP_SP then more data is needed.
            return ParseResult::INCOMPLETE;
        }
    }

    // Subtract off 1 for the final HTTP_SP that was found.
    m_uri = std::string_view{&data[m_uri_start_pos], m_pos - m_uri_start_pos};

    // If the parser gets this far then its successfully parsed the URI.
    m_parse_state = ParseState::PARSED_URI;

    return ParseResult::CONTINUE;
}

auto Request::parseVersion(std::string& data) -> ParseResult
{
    size_t data_length = data.length();
    if(m_pos + 10 < data_length)
    {
        // This state expects to still be on the previous HTTP_SP so we can easily EXPECT()
        EXPECT('H', ParseResult::HTTP_VERSION_MALFORMED);
        EXPECT('T', ParseResult::HTTP_VERSION_MALFORMED);
        EXPECT('T', ParseResult::HTTP_VERSION_MALFORMED);
        EXPECT('P', ParseResult::HTTP_VERSION_MALFORMED);
        EXPECT('/', ParseResult::HTTP_VERSION_MALFORMED);
        EXPECT('1', ParseResult::HTTP_VERSION_UNKNOWN);
        EXPECT('.', ParseResult::HTTP_VERSION_MALFORMED);
        switch(data[++m_pos])
        {
            case '0':
                m_version = Version::V1_0;
                break;
            case '1':
                m_version = Version::V1_1;
                break;
            default:
                return ParseResult::HTTP_VERSION_UNKNOWN;
        }
        // We include the \r\n here Request-line to Header parse to reduce bounds checking
        EXPECT(HTTP_CR, ParseResult::HTTP_VERSION_MALFORMED);
        EXPECT(HTTP_LF, ParseResult::HTTP_VERSION_MALFORMED);
        ADVANCE(); // Next section expects to be on its starting position
    }
    else
    {
        return ParseResult::INCOMPLETE;
    }

    m_parse_state = ParseState::PARSED_VERSION;

    return ParseResult::CONTINUE;
}

auto Request::parseHeaders(std::string& data) -> ParseResult
{
    size_t data_length = data.length();
    // missing empty line or headers
    if(data_length == m_pos)
    {
        return ParseResult::INCOMPLETE;
    }

    // If there are exactly 2 characters left and its the newline,
    // this request is complete as there are no headers.
    if(
            m_pos + 2 == data_length
        &&  data[m_pos] == HTTP_CR
        &&  data[m_pos + 1] == HTTP_LF
        )
    {
        m_pos += 2; // advance twice for the consumed values.
        return ParseResult::COMPLETE;
    }

    // There must be some headers here, parse them!
    while(true)
    {
        size_t name_start = m_pos;
        size_t value_start;

        size_t name_end = name_start;
#define CHECK_FOR_COLON() { if(data[++name_end] == ':') break; }

        // lets check 8 chars in a row!
        while(name_end + 8 < data_length)
        {
            CHECK_FOR_COLON();
            CHECK_FOR_COLON();
            CHECK_FOR_COLON();
            CHECK_FOR_COLON();
            CHECK_FOR_COLON();
            CHECK_FOR_COLON();
            CHECK_FOR_COLON();
            CHECK_FOR_COLON();
        }
#undef CHECK_FOR_COLON

        // go one by one...
        while(true)
        {
            if(name_end < data_length)
            {
                if(data[name_end] == ':')
                {
                    value_start = name_end + 1;
                    break; // while(true);p
                }
                ++name_end;
            }
            else
            {
                return ParseResult::INCOMPLETE;
            }
        }

        // Walk value forwards to left trim, this is unlikely to be more than 1 HTTP_SP or HTTP_HTAB
        while(value_start < data_length && is_ws(data[value_start]))
        {
            ++value_start;
        }

        // The parser has found the name of the header, now parse for the value.
        size_t value_end = value_start;
        while(value_end + 8 < data_length)
        {
            if(data[value_end] == HTTP_CR) break;
            if(data[++value_end] == HTTP_CR) break;
            if(data[++value_end] == HTTP_CR) break;
            if(data[++value_end] == HTTP_CR) break;
            if(data[++value_end] == HTTP_CR) break;
            if(data[++value_end] == HTTP_CR) break;
            if(data[++value_end] == HTTP_CR) break;
            if(data[++value_end] == HTTP_CR) break;
            ++value_end;
        }

        // check one by one or until incomplete
        bool found_crlf = false;
        while(value_end < data_length)
        {
            if(data[value_end++] == HTTP_CR && value_end < data_length && data[value_end] == HTTP_LF)
            {
                value_end -= 2;
                found_crlf = true;
                break;
            }
        }

        if(!found_crlf)
        {
            return ParseResult::INCOMPLETE;
        }

        // Update the current position after finding the end of the header,
        // since this loop expects to be on the first char its checking ADVANCE 3 times
        m_pos = value_end + 3;

        // Walk value end backwards to right trim
        while(is_ws(data[value_end]))
        {
            --value_end;
        }

        // We are out of space :(
        if(m_header_count == 64)
        {
            return ParseResult::TOO_MANY_HEADERS;
        }

        m_headers[m_header_count] =
            {
                {&data[name_start], (name_end - name_start)},
                {&data[value_start], (value_end - value_start + 1)}
            };
        // Before continuing, check to see if any of these headers give an indication if
        // there is any body content.
        if(m_body_type == BodyType::END_OF_STREAM)
        {
            auto& [name, value] = m_headers[m_header_count];
            if(
                    string_view_icompare(name, "Transfer-Encoding")
                &&  string_view_icompare(value, "chunked")
                )
            {
                m_body_type = BodyType::CHUNKED;
            }
            else if(
                    string_view_icompare(name, "Content-Length")
                &&  value.length() > 0
                )
            {
                m_content_length = 0; // in the event from_chars fails, we'll get no body
                std::from_chars(value.data(), value.data() + value.length(), m_content_length, 10);
                m_body_type = BodyType::CONTENT_LENGTH;
            }
        }
        ++m_header_count;

        // If this header line end with CRLF then this request has no more headers.
        if(
            m_pos + 1 < data_length
            &&  data[m_pos] == HTTP_CR
            &&  data[m_pos + 1] == HTTP_LF
            )
        {
            m_pos += 2; // ADVANCE two times for the consumed values and setup for next parse stage
            m_parse_state = ParseState::PARSED_HEADERS;
            break; // while(true)
        }
    }

    return ParseResult::CONTINUE;
}

auto Request::parseBody(std::string& data) -> ParseResult
{
    size_t data_length = data.length();
    switch(m_body_type)
    {
        case BodyType::CHUNKED:
        {
            // First time through record the start of the body for the full chunked body size.
            if(!m_body.has_value())
            {
                m_body_start = m_pos;
                m_content_length = 0; // leverage this for the decoded length
                m_body.emplace(&data[m_pos], 0);
            }

            while(true)
            {
                size_t chunk_size_end = m_pos + 1;
                bool chunk_size_end_found = false;
                while(chunk_size_end + 1 < data_length)
                {
                    if(data[chunk_size_end] == HTTP_CR && data[chunk_size_end + 1] == HTTP_LF)
                    {
                        chunk_size_end_found = true;
                        break; // while(chunk_size_end + 1 < data_length)
                    }
                    ++chunk_size_end;
                }

                if(!chunk_size_end_found)
                {
                    return ParseResult::INCOMPLETE;
                }

                size_t chunk_length{0};
                std::from_chars(&data[m_pos], &data[chunk_size_end], chunk_length, 16);

                if(chunk_length != 0)
                {
                    // There is a valid chunk with some data, parse over it!
                    // (note: chunk_size_end includes 1 byte for \r\n already
                    m_pos = chunk_size_end + 1 + chunk_length;
                    if(m_pos + 2 < data_length)
                    {
                        // move the data into the correct position. this is major YIKES!
                        char* data_start = data.data() + (m_body_start + m_content_length);
                        char* chunk_start = data.data() + (chunk_size_end + 2);

                        std::memmove(data_start, chunk_start, chunk_length);
                        m_content_length += chunk_length; // Keep track of the entire size though content length.
                        m_body.emplace(&data[m_body_start], m_content_length);

                        EXPECT(HTTP_CR, ParseResult::CHUNK_MALFORMED);
                        EXPECT(HTTP_LF, ParseResult::CHUNK_MALFORMED);
                        ADVANCE(); // This chunk parser expects to be on the first byte of the chunk
                    }
                }
                else
                {
                    // If the chunk length is zero, then EXPECT the final \r\n and record the body.
                    if(chunk_size_end + 3 <= data_length) // need \n\r\n
                    {
                        m_pos = chunk_size_end + 1; // strip the \n trailing chunk_size_end
                        EXPECT(HTTP_CR, ParseResult::CHUNK_MALFORMED);
                        EXPECT(HTTP_LF, ParseResult::CHUNK_MALFORMED);
                        m_parse_state = ParseState::PARSED_BODY;
                        break; // while(true)
                    }
                    else
                    {
                        // its possible to infer the last two bytes *should* be CR LF, but technically
                        // it is incomplete without them
                        return ParseResult::INCOMPLETE;
                    }
                }
            }
        }
            break;
        case BodyType::CONTENT_LENGTH:
        {
            if(m_pos + m_content_length >= data_length)
            {
                m_body.emplace(&data[m_pos], m_content_length);
                m_parse_state = ParseState::PARSED_BODY;
            }
            else
            {
                return ParseResult::INCOMPLETE;
            }
        }
            break;
        case BodyType::END_OF_STREAM:
        {
            if (m_pos < data_length)
            {
                // just update to as much data is possible, this isn't a super reliable method
                m_body.emplace(&data[m_pos], data.length() - m_pos);
                // Do not update m_parse_state, the user might
                // call again with more data to add into the body...
            }
        }
            break;
    }

    return ParseResult::CONTINUE;
}

auto Request::Reset() -> void
{
    m_parse_state = ParseState::START;
    m_pos = 0;
    //m_method{Method::GET};
    m_uri_start_pos = 0;
    //m_uri;
    //m_version{Version::V1_1};
    m_header_count = 0;
    //m_headers;
    m_body_type = BodyType::END_OF_STREAM;
    m_content_length = 0;
    m_body_start = 0;
    m_body = std::nullopt;
}

auto Request::GetParseState() const -> ParseState
{
    return m_parse_state;
}

auto Request::GetMethod() const -> Method
{
    return m_method;
}

auto Request::GetUri() const -> std::string_view
{
    return m_uri;
}

auto Request::GetVersion() const -> Version
{
    return m_version;
}

auto Request::GetHeaderCount() const -> size_t
{
    return m_header_count;
}

auto Request::GetHeader(std::string_view name) const -> std::optional<std::string_view>
{
    for(size_t i = 0; i < m_header_count; ++i)
    {
        auto& [req_name, req_value] = m_headers[i];
        if(string_view_icompare(name, req_name))
        {
            return std::optional<std::string_view>{req_value};
        }
    }

    return std::optional<std::string_view>{};
}

auto Request::GetBody() const -> const std::optional<std::string_view>&
{
    return m_body;
}

} // namespace liquid
