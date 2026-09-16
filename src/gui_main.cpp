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