#include "termacs/application.hpp"
#include "internal.hpp"
#include "termacs/tvision_backend.hpp"

#include <chrono>
#include <mutex>

namespace termacs {

static long long nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

struct Application::Impl {
    std::unique_ptr<Backend> backend;
    std::unique_ptr<Core>    core;
    CellBuffer               frame;
    bool                     running = false;
    int                      exitCode = 0;

    std::mutex                          mtx;
    std::vector<std::function<void()>>  tasks;

    struct Timer { TimerId id; int interval; long long nextDue; std::function<void()> cb; };
    std::vector<Timer> timers;
    TimerId            nextTimer = 1;
};

Application::Application() : Application(makeTerminalBackend()) {}

Application::Application(std::unique_ptr<Backend> backend) : d_(std::make_unique<Impl>()) {
    d_->backend = std::move(backend);
    d_->core = std::make_unique<Core>();
    d_->core->backend = d_->backend.get();
}

Application::~Application() = default;

void         Application::setTheme(Theme t) { d_->core->theme = std::move(t); }
const Theme& Application::theme() const     { return d_->core->theme; }
Core&        Application::core()            { return *d_->core; }
Backend&     Application::backend()         { return *d_->backend; }

Window Application::createWindow(const std::string& title) {
    auto [id, w] = d_->core->create<WindowNode>();
    w->title = title;
    d_->core->windows.push_back(id);
    return Window(d_->core.get(), id);
}

const CellBuffer& Application::render() {
    d_->core->render(d_->frame);
    return d_->frame;
}

void Application::feed(const InputEvent& ev) {
    if (ev.kind == InputKind::Key) d_->core->dispatchKey(ev.key);
    else if (ev.kind == InputKind::Mouse) d_->core->dispatchMouse(ev.mouse);
}

void Application::postToUiThread(std::function<void()> fn) {
    {
        std::lock_guard<std::mutex> lk(d_->mtx);
        d_->tasks.push_back(std::move(fn));
    }
    d_->backend->wake();
}

TimerId Application::startTimer(int intervalMs, std::function<void()> cb) {
    TimerId id = d_->nextTimer++;
    d_->timers.push_back({id, intervalMs, nowMs() + intervalMs, std::move(cb)});
    return id;
}
void Application::stopTimer(TimerId id) {
    auto& t = d_->timers;
    t.erase(std::remove_if(t.begin(), t.end(), [&](const Impl::Timer& x) { return x.id == id; }), t.end());
}

void Application::quit(int code) {
    d_->exitCode = code;
    d_->running = false;
    d_->backend->wake();
}

int Application::run() {
    d_->running = true;
    while (d_->running) {
        // 1. drain posted UI-thread tasks
        std::vector<std::function<void()>> tasks;
        { std::lock_guard<std::mutex> lk(d_->mtx); tasks.swap(d_->tasks); }
        for (auto& fn : tasks) fn();

        // 2. fire due timers, compute next wake timeout
        long long now = nowMs();
        int timeout = 1000;
        for (auto& tm : d_->timers) {
            if (now >= tm.nextDue) { tm.cb(); tm.nextDue = now + tm.interval; }
            timeout = std::min<long long>(timeout, std::max<long long>(0, tm.nextDue - now));
        }
        if (!d_->running) break;

        // 3. render the frame
        d_->core->render(d_->frame);
        d_->backend->present(d_->frame);

        // 4. wait for input (interrupted by wake() / quit())
        InputEvent ev;
        if (d_->backend->pollInput(ev, timeout)) {
            if (ev.kind == InputKind::Key) d_->core->dispatchKey(ev.key);
            else if (ev.kind == InputKind::Mouse) d_->core->dispatchMouse(ev.mouse);
            // Resize: next render uses backend->size()
        }
    }
    return d_->exitCode;
}

// ---- Dialogs -----------------------------------------------------------------
void Dialogs::info(Window owner, const std::string& message) {
    (void)owner;
    Core& core = app_->core();
    auto [id, d] = core.create<DialogNode>();
    d->title = "Information";
    d->message = message;
    d->buttons = {"OK"};
    d->onClose = [](int, std::string) {};
    core.popups.push_back(id);
}
void Dialogs::confirm(Window owner, const std::string& message, std::function<void(bool)> onResult) {
    (void)owner;
    Core& core = app_->core();
    auto [id, d] = core.create<DialogNode>();
    d->title = "Confirm";
    d->message = message;
    d->buttons = {"Yes", "No"};
    d->focusBtn = 0;
    d->onClose = [onResult](int btn, std::string) { if (onResult) onResult(btn == 0); };
    core.popups.push_back(id);
}
void Dialogs::prompt(Window owner, const std::string& message, std::function<void(bool, std::string)> onResult) {
    (void)owner;
    Core& core = app_->core();
    auto [id, d] = core.create<DialogNode>();
    d->title = "Input";
    d->message = message;
    d->hasInput = true;
    d->buttons = {"OK", "Cancel"};
    d->onClose = [onResult](int btn, std::string in) { if (onResult) onResult(btn == 0, in); };
    core.popups.push_back(id);
}

} // namespace termacs
