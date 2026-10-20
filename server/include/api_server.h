// =============================================================================
// api_server.h — Embedded HTTP REST API Server
// =============================================================================
#pragma once
#include "core/pipeline.h"
#include "core/superscalar_pipeline.h"
#include "core/vliw_pipeline.h"
#include "core/simulator_engine.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <atomic>

namespace rvsim {

class APIServer {
public:
    explicit APIServer(int port = 8080);
    ~APIServer();

    // Start serving (blocks until stop() called)
    void start();
    void stop();

    bool is_running() const { return running_; }

private:
    int port_;
    std::atomic<bool> running_{false};
    httplib::Server   svr_;
    std::unique_ptr<rv32i::SimulatorEngine> pipeline_;
    
    // Store current loaded hex program so we can reload it when mode changes
    std::string current_hex_program_;

    // ---------- route handlers ----------
    void setup_routes();

    // Serve the embedded frontend HTML
    void handle_root(const httplib::Request&, httplib::Response& res);

    // POST /api/load  { "builtin": "fibonacci" } or { "hex": "..." }
    void handle_load(const httplib::Request& req, httplib::Response& res);

    // POST /api/config { "predictor": "gshare", "forwarding": true, ... }
    void handle_config(const httplib::Request& req, httplib::Response& res);

    // POST /api/mode { "mode": "scalar"|"superscalar"|"vliw", "policy": "io_io" }
    void handle_mode(const httplib::Request& req, httplib::Response& res);

    // POST /api/step  { "cycles": 1 }
    void handle_step(const httplib::Request& req, httplib::Response& res);

    // POST /api/run   { "max_cycles": 100000 }
    void handle_run(const httplib::Request& req, httplib::Response& res);

    // POST /api/reset
    void handle_reset(const httplib::Request& req, httplib::Response& res);

    // GET /api/state
    void handle_state(const httplib::Request& req, httplib::Response& res);

    // GET /api/programs
    void handle_programs(const httplib::Request& req, httplib::Response& res);

    // Helper: serialize PipelineState to JSON response
    static void json_response(httplib::Response& res, const nlohmann::json& j,
                              int status = 200);
    static void error_response(httplib::Response& res, const std::string& msg,
                               int status = 400);
};

} // namespace rvsim
