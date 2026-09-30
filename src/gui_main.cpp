#include "ahocorasick.hpp"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include "ahocorasick.hpp"
#include <imgui.h>

#include <chrono>
#include <random>
#include <sstream>

#ifdef ENABLE_BENCHMARK_SUITE

struct BenchmarkResult {
    std::size_t dataset_size_mb;
    std::size_t num_patterns;
    double naive_time_ms;
    double ac_time_ms;
    double speedup;
    std::size_t total_matches;
};

// Global Benchmark State
static int g_bench_file_size_mb = 10;
static int g_bench_num_patterns = 20;
static bool g_bench_running = false;
static std::vector<BenchmarkResult> g_benchmark_history;
static std::mutex g_bench_mutex;
static std::string g_bench_status = "Ready";

// Naive baseline for comparison: O(N * K * M) string::find search
size_t run_naive_multi_search(const std::vector<std::string>& lines, const std::vector<std::string>& patterns) {
    size_t matches = 0;
    for (const auto& line : lines) {
        for (const auto& pat : patterns) {
            size_t pos = line.find(pat);
            while (pos != std::string::npos) {
                matches++;
                pos = line.find(pat, pos + 1);
            }
        }
    }
    return matches;
}

// Generates synthetic log lines directly in RAM
std::vector<std::string> generate_synthetic_logs(std::size_t target_mb) {
    static const std::vector<std::string> templates = {
        "2026-09-30 10:14:02 [INFO] Server process started successfully on port 8080",
        "2026-09-30 10:14:15 [WARN] Memory utilization threshold reached: 88%",
        "2026-09-30 10:15:22 [ERROR] Database connection pool exhausted: Timeout after 3000ms",
        "2026-09-30 10:16:05 [INFO] User admin authenticated successfully from 192.168.1.50",
        "2026-09-30 10:17:40 [ERROR] 404 Endpoint Not Found: GET /api/v2/deprecated_route",
        "2026-09-30 10:18:11 [WARN] Unauthorized access attempt detected on /admin/credentials"
    };

    std::vector<std::string> lines;
    std::size_t total_bytes = 0;
    std::size_t target_bytes = target_mb * 1024 * 1024;

    std::mt19937 rng(1337); // Fixed seed for reproducible benchmarks
    std::uniform_int_distribution<std::size_t> dist(0, templates.size() - 1);

    while (total_bytes < target_bytes) {
        const std::string& template_line = templates[dist(rng)];
        lines.push_back(template_line);
        total_bytes += template_line.size() + 1;
    }
    return lines;
}

// Worker function executed asynchronously in background
void execute_gui_benchmark(int size_mb, int pattern_count) {
    g_bench_running = true;
    
    {
        std::lock_guard<std::mutex> lock(g_bench_mutex);
        g_bench_status = "Generating " + std::to_string(size_mb) + " MB synthetic log buffer in RAM...";
    }

    auto lines = generate_synthetic_logs(size_mb);

    // Build dictionary of unique patterns
    std::vector<std::string> patterns;
    static const std::vector<std::string> pool = {
        "ERROR", "WARN", "INFO", "Timeout", "Unauthorized", "Database", "404", "8080", 
        "admin", "deprecated_route", "authenticated", "credentials", "exhausted", "port", "192.168"
    };
    for (int i = 0; i < pattern_count; ++i) {
        patterns.push_back(pool[i % pool.size()] + (i >= pool.size() ? std::to_string(i) : ""));
    }

    {
        std::lock_guard<std::mutex> lock(g_bench_mutex);
        g_bench_status = "Running Naive O(N*K*M) Loop Search...";
    }

    // 1. Time Naive Search
    auto t1 = std::chrono::high_resolution_clock::now();
    size_t naive_matches = run_naive_multi_search(lines, patterns);
    auto t2 = std::chrono::high_resolution_clock::now();
    double naive_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();

    {
        std::lock_guard<std::mutex> lock(g_bench_mutex);
        g_bench_status = "Building Aho-Corasick Automaton & Scanning...";
    }

    // 2. Time Aho-Corasick Search
    auto t3 = std::chrono::high_resolution_clock::now();
    AhoCorasick ac(patterns);
    size_t ac_matches = 0;
    for (const auto& l : lines) {
        ac_matches += ac.find_matches(l).size();
    }
    auto t4 = std::chrono::high_resolution_clock::now();
    double ac_ms = std::chrono::duration<double, std::milli>(t4 - t3).count();

    double speedup = (ac_ms > 0.0) ? (naive_ms / ac_ms) : 0.0;

    {
        std::lock_guard<std::mutex> lock(g_bench_mutex);
        g_benchmark_history.push_back({
            static_cast<std::size_t>(size_mb),
            static_cast<std::size_t>(pattern_count),
            naive_ms,
            ac_ms,
            speedup,
            ac_matches
        });
        g_bench_status = "Completed successfully!";
    }

    g_bench_running = false;
}

void DrawBenchmarkWindow() {
    ImGui::Begin("Aho-Corasick Benchmark Suite");

    ImGui::Text("Algorithm Comparison: Aho-Corasick O(N+Z) vs Naive std::string::find O(N*K*M)");
    ImGui::Separator();

    ImGui::SliderInt("Log Dataset Size (MB)", &g_bench_file_size_mb, 1, 50);
    ImGui::SliderInt("Dictionary Pattern Count (K)", &g_bench_num_patterns, 1, 500);

    if (g_bench_running) {
        ImGui::BeginDisabled();
        ImGui::Button("Running Benchmark...", ImVec2(-1, 35));
        ImGui::EndDisabled();
    } else {
        if (ImGui::Button("Run Performance Comparison", ImVec2(-1, 35))) {
            std::thread bench_thread(execute_gui_benchmark, g_bench_file_size_mb, g_bench_num_patterns);
            bench_thread.detach();
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_bench_mutex);
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Status: %s", g_bench_status.c_str());
    }

    ImGui::Separator();
    ImGui::Text("Benchmark History & Results:");

    if (ImGui::BeginTable("BenchTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Size (MB)");
        ImGui::TableSetupColumn("Patterns (K)");
        ImGui::TableSetupColumn("Naive Time (ms)");
        ImGui::TableSetupColumn("Aho-Corasick Time (ms)");
        ImGui::TableSetupColumn("Speedup");
        ImGui::TableSetupColumn("Matches Found");
        ImGui::TableHeadersRow();

        std::lock_guard<std::mutex> lock(g_bench_mutex);
        for (const auto& res : g_benchmark_history) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%zu MB", res.dataset_size_mb);
            ImGui::TableSetColumnIndex(1); ImGui::Text("%zu", res.num_patterns);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f ms", res.naive_time_ms);
            ImGui::TableSetColumnIndex(3); ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%.2f ms", res.ac_time_ms);
            ImGui::TableSetColumnIndex(4); ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%.2fx Faster", res.speedup);
            ImGui::TableSetColumnIndex(5); ImGui::Text("%zu", res.total_matches);
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

#endif // ENABLE_BENCHMARK_SUITE

struct GuiMatchRecord {
    std::size_t line_number;
    std::string pattern;
    std::size_t start;
    std::size_t end;
    std::string line_text;
};

// Global App State
std::vector<std::string> g_patterns = {};
char g_new_pattern_buf[128] = "";
char g_file_path_buf[512] = "sample.txt";
bool g_ignore_case = false;

// Async Search State
std::vector<GuiMatchRecord> g_results;
std::mutex g_results_mutex;
std::atomic<bool> g_is_searching(false);
std::atomic<float> g_search_progress(0.0f);
std::atomic<std::size_t> g_total_lines(0);

void run_async_search(std::string log_file_path, std::vector<std::string> search_patterns, bool ignore_case) {
    g_is_searching = true;
    g_search_progress = 0.0f;
    g_total_lines = 0;

    {
        std::lock_guard<std::mutex> lock(g_results_mutex);
        g_results.clear();
    }

    // Build lower-case patterns if ignore_case is true
    std::vector<std::string> patterns_to_use = search_patterns;
    if (ignore_case) {
        for (auto& p : patterns_to_use) {
            std::transform(p.begin(), p.end(), p.begin(), [](unsigned char c){ return std::tolower(c); });
        }
    }

    AhoCorasick ac(patterns_to_use);

    std::ifstream file(log_file_path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        g_is_searching = false;
        return;
    }

    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string line;
    std::size_t line_num = 1;

    while (std::getline(file, line)) {
        std::string search_text = line;
        if (ignore_case) {
            std::transform(search_text.begin(), search_text.end(), search_text.begin(), [](unsigned char c){ return std::tolower(c); });
        }

        auto matches = ac.find_matches(search_text);

        if (!matches.empty()) {
            std::lock_guard<std::mutex> lock(g_results_mutex);
            for (const auto& m : matches) {
                g_results.push_back(GuiMatchRecord{
                    line_num,
                    search_patterns[m.pattern_idx], // Original pattern text
                    m.start,
                    m.end,
                    line
                });
            }
        }

        line_num++;
        g_total_lines = line_num;

        if (line_num % 1000 == 0 && file_size > 0) {
            g_search_progress = static_cast<float>(file.tellg()) / static_cast<float>(file_size);
        }
    }

    g_search_progress = 1.0f;
    g_is_searching = false;
}

int main() {
    if (!glfwInit()) return -1;

    GLFWwindow* window = glfwCreateWindow(1280, 720, "multigrep", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    #define IMGUI_DEFINE_MATH_OPERATORS
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // -------------------------------------------------------------
        // Control Panel Window
        // -------------------------------------------------------------
        ImGui::Begin("Search Control Panel");

        ImGui::InputText("Input File", g_file_path_buf, IM_ARRAYSIZE(g_file_path_buf));
        ImGui::Checkbox("Ignore Case (-i)", &g_ignore_case);

        ImGui::Separator();
        ImGui::Text("Patterns / Indicators (Aho-Corasick Dictionary):");

        ImGui::InputText("New Pattern", g_new_pattern_buf, IM_ARRAYSIZE(g_new_pattern_buf));
        ImGui::SameLine();
        if (ImGui::Button("Add") && strlen(g_new_pattern_buf) > 0) {
            g_patterns.push_back(g_new_pattern_buf);
            g_new_pattern_buf[0] = '\0';
        }

        // List existing patterns
        int to_remove = -1;
        for (int i = 0; i < static_cast<int>(g_patterns.size()); ++i) {
            ImGui::BulletText("%s", g_patterns[i].c_str());
            ImGui::SameLine();
            std::string remove_label = "Remove##" + std::to_string(i);
            if (ImGui::Button(remove_label.c_str())) {
                to_remove = i;
            }
        }
        if (to_remove != -1) {
            g_patterns.erase(g_patterns.begin() + to_remove);
        }

        ImGui::Separator();

        if (g_is_searching) {
            ImGui::ProgressBar(g_search_progress, ImVec2(-1.0f, 0.0f));
            ImGui::Text("Searching... Checked %zu lines", g_total_lines.load());
        } else {
            if (ImGui::Button("Run Aho-Corasick Search", ImVec2(-1, 40))) {
                if (!g_patterns.empty()) {
                    std::thread search_thread(run_async_search, std::string(g_file_path_buf), g_patterns, g_ignore_case);
                    search_thread.detach();
                }
            }
        }

        ImGui::End();

        // -------------------------------------------------------------
        // Results Table Window
        // -------------------------------------------------------------
        ImGui::Begin("Matched Results");

        std::lock_guard<std::mutex> lock(g_results_mutex);
        ImGui::Text("Total Matches Found: %zu", g_results.size());

        static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
        if (ImGui::BeginTable("ResultsTable", 4, flags)) {
            ImGui::TableSetupColumn("Line", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Matched Keyword", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("File Line Text", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (const auto& match : g_results) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%zu", match.line_number);

                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", match.pattern.c_str());

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%zu-%zu", match.start, match.end);

                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(match.line_text.c_str());
            }
            ImGui::EndTable();
        }
        #ifdef ENABLE_BENCHMARK_SUITE
        // Render the conditional Benchmark Suite window
        DrawBenchmarkWindow();
        #endif
        ImGui::End();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.12f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}