#include "ahocorasick.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

// ANSI Terminal Colors
constexpr const char* RED_BOLD = "\033[1;31m";
constexpr const char* RESET    = "\033[0m";

std::string to_lower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c){ return std::tolower(c); });
    return str;
}

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " -p <pattern> [-p <pattern>...] [-f <pattern_file>] [target_file] [--json] [-i]\n";
}

int main(int argc, char* argv[]) {
    std::vector<std::string> patterns;
    std::string target_file_path;
    std::string pattern_file_path;
    bool json_output = false;
    bool ignore_case = false;

    // Minimal CLI Parser
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-p" && i + 1 < argc) {
            patterns.push_back(argv[++i]);
        } else if (arg == "-f" && i + 1 < argc) {
            pattern_file_path = argv[++i];
        } else if (arg == "--json") {
            json_output = true;
        } else if (arg == "-i" || arg == "--ignore-case") {
            ignore_case = true;
        } else if (arg[0] != '-') {
            target_file_path = arg;
        }
    }

    // Load pattern file if supplied
    if (!pattern_file_path.empty()) {
        std::ifstream pf(pattern_file_path);
        if (!pf.is_open()) {
            std::cerr << "Error opening pattern file: " << pattern_file_path << "\n";
            return 1;
        }
        std::string line;
        while (std::getline(pf, line)) {
            if (!line.empty()) patterns.push_back(line);
        }
    }

    if (patterns.empty()) {
        print_usage(argv[0]);
        return 1;
    }

    // Apply Case-Insensitivity Mapping
    std::vector<std::string> search_patterns = patterns;
    if (ignore_case) {
        for (auto& p : search_patterns) p = to_lower(p);
    }

    // Construct Automaton
    AhoCorasick ac(search_patterns);

    // Stream Setup (File vs Standard Input)
    std::ifstream file_stream;
    std::istream* input_stream = &std::cin;

    if (!target_file_path.empty()) {
        file_stream.open(target_file_path);
        if (!file_stream.is_open()) {
            std::cerr << "Error opening log file: " << target_file_path << "\n";
            return 1;
        }
        input_stream = &file_stream;
    }

    std::string line;
    std::size_t line_num = 1;

    while (std::getline(*input_stream, line)) {
        std::string search_text = ignore_case ? to_lower(line) : line;
        auto matches = ac.find_matches(search_text);

        if (!matches.empty()) {
            if (json_output) {
                for (const auto& m : matches) {
                    std::cout << "{\"line\": " << line_num 
                              << ", \"pattern\": \"" << patterns[m.pattern_idx] 
                              << "\", \"start\": " << m.start 
                              << ", \"end\": " << m.end 
                              << ", \"text\": \"" << line << "\"}\n";
                }
            } else {
                // Highlight matches in right-to-left order to maintain correct string index offsets
                std::string highlighted = line;
                std::sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) {
                    return a.start > b.start;
                });

                for (const auto& m : matches) {
                    if (m.end <= highlighted.length()) {
                        highlighted.insert(m.end, RESET);
                        highlighted.insert(m.start, RED_BOLD);
                    }
                }
                std::cout << line_num << ": " << highlighted << "\n";
            }
        }
        line_num++;
    }

    return 0;
}