/**
 * Mozart++ Template Library
 * Licensed under MIT License
 * Copyright (c) 2020 Covariant Institute
 * Website: https://covariant.cn/
 * Github:  https://github.com/covariant-institute/
 */

#include <queue>
#include <memory>
#include <thread>
#include <typeindex>
#include <mutex>
#include <atomic>
#include <mozart++/core>

using mpp::function;
using mpp::function_alias;
using mpp::function_parser;
using mpp::typelist;
using mpp::throw_ex;

/**
 * Inner container class, storing event handler
 */
class handler_container {
private:
    size_t _args_count = 0;
    std::type_index _args_info;
    std::shared_ptr<char> _handler;

public:
    template <typename Handler>
    explicit handler_container(Handler &&handler)
        :_args_info(typeid(void)) {
        // handler-dependent types
        using wrapper_type = decltype(mpp::make_function(handler));
        using arg_types = typename function_parser<wrapper_type>::decayed_arg_types;

        // generate the handler wrapper dynamically according to
        // the callback type, so we can pass varied and arbitrary
        // count of arguments to trigger the event handler.
        auto *fn = new wrapper_type(mpp::make_function(handler));

        // store argument info for call-time type check.
        _args_count = typelist::size<arg_types>::value;
        _args_info = typeid(arg_types);

        // use std::shared_ptr to manage the allocated memory
        // (char *) and (void *) are known as universal pointers.
        _handler = std::shared_ptr<char>(
            // wrapper function itself
            reinterpret_cast<char *>(fn),

            // wrapper function deleter
            [](char *ptr) {
                delete reinterpret_cast<wrapper_type *>(ptr);
            }
        );
    }

    template <typename F>
    function_alias<F> *callable_ptr() {
        using callee_arg_types = typename function_parser<function_alias<F>>::decayed_arg_types;

        // When callee didn't pass any argument,
        // we only need to check _arg_count.
        // Avoid typeid() call as much as possible.
        //
        // Note that:
        // This branch condition is always a constexpr, so don't
        // worry about the branch overhead.
        //
        if (mpp::typelist::empty_v<callee_arg_types>) {
            if (_args_count == 0) {
                return reinterpret_cast<function_alias<F> *>(_handler.get());
            }
        } else {
            if (_args_info == typeid(callee_arg_types)) {
                return reinterpret_cast<function_alias<F> *>(_handler.get());
            }
        }

        // Otherwise, return nothing when type mismatch
        return nullptr;
    }
};

class handler {
private:
    using remote_fn_t = mpp::function_alias<void()>;

    std::atomic_bool _quited{false};
    std::mutex _loop_mutex;
    std::mutex _queue_mutex;
    std::queue<std::unique_ptr<remote_fn_t>> _pending_remote_calls;
    std::unordered_map<std::string, std::vector<std::pair<std::thread::id, handler_container>>> _events;

private:
    void push_remote_call(std::unique_ptr<remote_fn_t> remote_fn) {
        std::lock_guard<std::mutex> lg(_queue_mutex);
        _pending_remote_calls.push(std::move(remote_fn));
    }

    std::unique_ptr<remote_fn_t> pop_remote_call() {
        std::lock_guard<std::mutex> lg(_queue_mutex);
        auto remote_fn = std::move(_pending_remote_calls.front());
        _pending_remote_calls.pop();
        return std::move(remote_fn);
    }

public:
    handler() = default;

    virtual ~handler() = default;

    handler(const handler &) = default;

    /**
     * Register an event with handler.
     *
     * @tparam Handler Type of the handler
     * @param name Event name
     * @param handler Event handler
     */
    template <typename Handler>
    void on(const std::string &name, Handler handler) {
        _events[name].push_back(std::make_pair(
            std::this_thread::get_id(),
            handler_container(handler)));
    }

    /**
     * Clear all handlers registered to event.
     *
     * @param name Event name
     */
    void unregister_event(const std::string &name) {
        _events.erase(name);
    }

    /**
     * Call all event handlers associated with event name.
     *
     * @tparam Args Argument types
     * @param name Event name
     * @param args Event handler arguments
     */
    template <typename ...Args>
    void emit(const std::string &name, Args &&...args) {
        auto it = _events.find(name);
        if (it == _events.end()) {
            return;
        }

        for (auto &&fn : it->second) {
            auto *handler = fn.second.callable_ptr<void(Args...)>();
            if (handler == nullptr) {
                throw_ex<mpp::runtime_error>("Invalid call to event handler: mismatched argument list");
            }

            if (fn.first == std::this_thread::get_id()) {
                (*handler)(std::forward<Args>(args)...);
            } else {
                push_remote_call(std::make_unique<remote_fn_t>(
                    [=]() mutable {
                        (*handler)(std::forward<Args>(args)...);
                    }
                ));
            }
        }
    }

    void loop() {
        std::unique_lock<std::mutex> guard(_loop_mutex);
        while (true) {
            while (_pending_remote_calls.empty() && !_quited) {
                std::this_thread::yield();
            }
            if (_quited) {
                break;
            }
            auto remote_fn = std::move(pop_remote_call());
            (*remote_fn)();
        }
    }

    void quit() {
        _quited = true;
    }
};

void thread_run(handler *&H) {
    handler h;
    H = &h;
    h.on("key-enter", []() {
        printf("thread: Enter pressed\n");
    });
    h.on("key", [](int ch) {
        printf("thread: pressed %c\n", ch);
    });
    printf("MessageQueue started\n");
    h.loop();
    printf("MessageQueue exited\n");
}

int main() {
    handler *H;
    std::thread thread(thread_run, std::ref(H));
    int ch = 0;
    while ((ch = getchar()) != 'q') {
        if (ch == '\n') {
            H->emit("key-enter");
        } else {
            H->emit("key", ch);
        }
    }
    H->quit();
    thread.join();
}
