#ifndef AHOCORASICK_HPP
#define AHOCORASICK_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <cstddef>

using NodeId = std::size_t;

struct Match {
    std::size_t pattern_idx;
    std::string pattern;
    std::size_t start;
    std::size_t end;
};

struct Node {
    std::unordered_map<char, NodeId> children;
    NodeId failure = 0;                  // Default failure link -> root (NodeId 0)
    std::optional<NodeId> output;        // Shortcut link to nearest terminal state
    std::vector<std::size_t> patterns;   // Indices of patterns ending at this node
};

class AhoCorasick {
public:
    explicit AhoCorasick(std::vector<std::string> patterns);
    
    [[nodiscard]] std::vector<Match> find_matches(const std::string& text) const;
    [[nodiscard]] const std::vector<std::string>& get_patterns() const { return patterns_; }

private:
    std::vector<Node> nodes_;
    std::vector<std::string> patterns_;

    void build_trie();
    void build_failure_links();
    void collect_matches(NodeId state, std::size_t end_offset, std::vector<Match>& matches) const;
};

#endif // AHOCORASICK_HPP