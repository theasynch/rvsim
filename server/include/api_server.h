// =============================================================================
// api_server.h — Embedded HTTP REST API Server
// =============================================================================
#pragma once
#include "rtl_pipeline.h"
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
    std::unique_ptr<rv32i::RtlPipeline> pipeline_;
    rv32i::PipelineConfig config_;

    // ---------- route handlers ----------
    void setup_routes();

    // Serve the embedded frontend HTML
    void handle_root(const httplib::Request&, httplib::Response& res);

    // POST /api/load  { "hex": "..." }
    void handle_load(const httplib::Request& req, httplib::Response& res);

    // POST /api/config { "predictor": "gshare", "cache_size": 8192, ... }
    void handle_config(const httplib::Request& req, httplib::Response& res);

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
    static nlohmann::json state_to_json(const rv32i::PipelineState& s);
    static void json_response(httplib::Response& res, const nlohmann::json& j,
                              int status = 200);
    static void error_response(httplib::Response& res, const std::string& msg,
                               int status = 400);
};

} // namespace rvsim
