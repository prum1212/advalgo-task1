#include "ahocorasick.hpp"
#include <queue>

AhoCorasick::AhoCorasick(std::vector<std::string> patterns) 
    : patterns_(std::move(patterns)) {
    build_trie();
    build_failure_links();
}

void AhoCorasick::build_trie() {
    nodes_.emplace_back(); // NodeId 0 is always the root node

    for (std::size_t idx = 0; idx < patterns_.size(); ++idx) {
        NodeId current = 0;
        for (char ch : patterns_[idx]) {
            if (nodes_[current].children.find(ch) == nodes_[current].children.end()) {
                NodeId next_node = nodes_.size();
                nodes_.emplace_back();
                nodes_[current].children[ch] = next_node;
            }
            current = nodes_[current].children[ch];
        }
        nodes_[current].patterns.push_back(idx);
    }
}

void AhoCorasick::build_failure_links() {
    std::queue<NodeId> q;

    // Step 1: Direct children of root have failure link -> 0
    for (const auto& [ch, child_id] : nodes_[0].children) {
        nodes_[child_id].failure = 0;
        q.push(child_id);
    }

    // Step 2: BFS propagation across trie levels
    while (!q.empty()) {
        NodeId current = q.front();
        q.pop();

        for (const auto& [ch, child_id] : nodes_[current].children) {
            NodeId fail = nodes_[current].failure;

            // Follow failure chain until a transition exists for char 'ch'
            while (fail != 0 && nodes_[fail].children.find(ch) == nodes_[fail].children.end()) {
                fail = nodes_[fail].failure;
            }

            NodeId target_fail = 0;
            if (auto it = nodes_[fail].children.find(ch); it != nodes_[fail].children.end()) {
                target_fail = it->second;
            }

            nodes_[child_id].failure = target_fail;

            // Output link optimization
            if (!nodes_[target_fail].patterns.empty()) {
                nodes_[child_id].output = target_fail;
            } else {
                nodes_[child_id].output = nodes_[target_fail].output;
            }

            q.push(child_id);
        }
    }
}

void AhoCorasick::collect_matches(NodeId state, std::size_t end_offset, std::vector<Match>& matches) const {
    for (std::size_t pat_idx : nodes_[state].patterns) {
        const std::string& pat = patterns_[pat_idx];
        std::size_t start = end_offset - pat.length();
        matches.push_back(Match{pat_idx, pat, start, end_offset});
    }
}

std::vector<Match> AhoCorasick::find_matches(const std::string& text) const {
    std::vector<Match> matches;
    NodeId current_state = 0;

    for (std::size_t i = 0; i < text.length(); ++i) {
        char ch = text[i];

        // Traverse failure links if child doesn't exist
        while (current_state != 0 && nodes_[current_state].children.find(ch) == nodes_[current_state].children.end()) {
            current_state = nodes_[current_state].failure;
        }

        if (auto it = nodes_[current_state].children.find(ch); it != nodes_[current_state].children.end()) {
            current_state = it->second;
        }

        // Direct pattern matches
        collect_matches(current_state, i + 1, matches);

        // Output links for overlapping matches
        std::optional<NodeId> curr_output = nodes_[current_state].output;
        while (curr_output.has_value()) {
            collect_matches(*curr_output, i + 1, matches);
            curr_output = nodes_[*curr_output].output;
        }
    }

    return matches;
}