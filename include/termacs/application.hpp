// termacs — Application: owns the Core + Backend, the run loop, timers, the UI-thread
// task queue (§11.6), and dialogs (§5.7).
#pragma once
#include <functional>
#include <memory>
#include <string>
#include "backend.hpp"
#include "theme.hpp"
#include "widget.hpp"

namespace termacs {

class Core;
class Application;

using TimerId = uint64_t;

// app.dialogs().info(...) / .confirm(...) / .prompt(...)
class Dialogs {
public:
    explicit Dialogs(Application* app) : app_(app) {}
    void info(Window owner, const std::string& message);
    void confirm(Window owner, const std::string& message, std::function<void(bool)> onResult);
    void prompt(Window owner, const std::string& message, std::function<void(bool, std::string)> onResult);
private:
    Application* app_;
};

class Application {
public:
    Application();                                   // real terminal (TvisionBackend)
    explicit Application(std::unique_ptr<Backend> backend);  // injected (e.g. Headless)
    ~Application();

    void   setTheme(Theme t);
    const  Theme& theme() const;

    Window createWindow(const std::string& title);

    int    run();                                    // the event loop; returns exit code
    void   quit(int code = 0);

    // The ONLY thread-safe entry point (§11.6).
    void   postToUiThread(std::function<void()> fn);

    TimerId startTimer(int intervalMs, std::function<void()> cb);
    void    stopTimer(TimerId id);

    Dialogs& dialogs() { return dialogs_; }

    // --- testing / headless helpers ---
    const CellBuffer& render();                      // layout + draw one frame to a buffer
    void              feed(const InputEvent& ev);    // inject one input event + dispatch
    Backend&          backend();

    Core& core();                                    // internal

private:
    struct Impl;
    std::unique_ptr<Impl> d_;
    Dialogs dialogs_{this};
};

} // namespace termacs
