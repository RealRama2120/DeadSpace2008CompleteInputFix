#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct PatternByte {
    std::uint8_t value;
    bool wildcard;
};

static bool ParsePattern(const std::string& text, std::vector<PatternByte>& pattern) {
    std::istringstream stream(text);
    std::string token;
    while (stream >> token) {
        if (token == "?" || token == "??") {
            pattern.push_back({0, true});
            continue;
        }
        if (token.size() != 2 || !std::isxdigit(static_cast<unsigned char>(token[0])) ||
            !std::isxdigit(static_cast<unsigned char>(token[1]))) {
            return false;
        }
        pattern.push_back({static_cast<std::uint8_t>(std::stoul(token, nullptr, 16)), false});
    }
    return !pattern.empty();
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: scan_byte_pattern <file> \"AA BB ?? CC\"\n";
        return 2;
    }

    std::vector<PatternByte> pattern;
    if (!ParsePattern(argv[2], pattern)) {
        std::cerr << "invalid pattern\n";
        return 2;
    }

    std::ifstream input(argv[1], std::ios::binary);
    if (!input) {
        std::cerr << "cannot open input file\n";
        return 2;
    }
    const std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};

    std::size_t matches = 0;
    if (bytes.size() >= pattern.size()) {
        for (std::size_t offset = 0; offset <= bytes.size() - pattern.size(); ++offset) {
            bool matched = true;
            for (std::size_t index = 0; index < pattern.size(); ++index) {
                if (!pattern[index].wildcard && bytes[offset + index] != pattern[index].value) {
                    matched = false;
                    break;
                }
            }
            if (matched) {
                std::cout << "0x" << std::uppercase << std::hex << std::setw(8)
                          << std::setfill('0') << offset << "\n";
                ++matches;
            }
        }
    }
    std::cerr << std::dec << matches << " match(es)\n";
    return 0;
}
