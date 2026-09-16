#include "ahocorasick.hpp"
#include <iostream>
#include <cassert>

void test_overlapping() {
    std::vector<std::string> patterns = {"he", "she", "his", "hers"};
    AhoCorasick ac(patterns);
    auto matches = ac.find_matches("ushers");

    // Expecting matches for "she", "he", and "hers"
    assert(matches.size() == 3);
    assert(matches[0].pattern == "she");
    assert(matches[1].pattern == "he");
    assert(matches[2].pattern == "hers");
    std::cout << "[PASS] Test Overlapping Patterns\n";
}

void test_nested() {
    std::vector<std::string> patterns = {"ana", "banana"};
    AhoCorasick ac(patterns);
    auto matches = ac.find_matches("banana");

    // Matches: "banana" (0..6), "ana" (1..4), "ana" (3..6)
    assert(matches.size() == 3);
    std::cout << "[PASS] Test Nested Patterns\n";
}

int main() {
    test_overlapping();
    test_nested();
    std::cout << "All unit tests passed successfully!\n";
    return 0;
}