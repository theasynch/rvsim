// =============================================================================
// main.cpp — RVSim Entry Point
// =============================================================================
//
// This is what the user double-clicks. It:
//  1. Starts the embedded HTTP server on localhost:8080
//  2. Opens the user's default browser to that URL
//  3. Blocks until the user closes the window or presses Ctrl+C
// =============================================================================
#include "api_server.h"
#include <iostream>
#include <thread>
#include <chrono>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <shellapi.h>
#elif defined(__APPLE__)
  #include <cstdlib>
#else
  #include <cstdlib>
#endif

static void open_browser(const std::string& url) {
#ifdef _WIN32
    // ShellExecuteA opens the default browser
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    std::system(("open " + url).c_str());
#else
    std::system(("xdg-open " + url + " &").c_str());
#endif
}

int main() {
    constexpr int PORT = 8080;
    const std::string URL = "http://localhost:" + std::to_string(PORT);

    std::cout << R"(
  ____  _    __  _____ _           
 |  _ \| |  / / / ____(_)          
 | |_) | | / / | (___  _ _ __ ___  
 |  _ <| |/ /   \___ \| | '_ ` _ \ 
 | |_) |   /    ____) | | | | | | |
 |____/|_|/_\  |_____/|_|_| |_| |_|
                                    
  RISC-V Pipeline Simulator v1.0
  ================================
)" << "\n";

    std::cout << "[RVSim] Starting server at " << URL << "\n";
    std::cout << "[RVSim] Opening browser...\n\n";
    std::cout << "[RVSim] Press Ctrl+C to quit.\n\n";

    rvsim::APIServer server(PORT);

    // Open browser after a short delay (give server time to bind)
    std::thread browser_thread([&URL]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        open_browser(URL);
    });
    browser_thread.detach();

    server.start(); // Blocks until Ctrl+C
    return 0;
}
