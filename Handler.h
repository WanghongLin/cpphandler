//
// Copyright 2019 Wanghong Lin
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//         http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
//         limitations under the License.
//
// Created by Wanghong Lin on 2019/11/25.
//

#ifndef CPPHANDLER_HANDLER_H
#define CPPHANDLER_HANDLER_H

#include <iostream>
#include <thread>
#include <deque>
#include <functional>
#include <chrono>
#include <algorithm>
#include <sstream>

namespace cpphandler {
    namespace Clock {
        inline std::chrono::steady_clock::time_point min() {
            return std::chrono::steady_clock::time_point::min();
        }

        inline std::chrono::steady_clock::time_point now() {
            return std::chrono::steady_clock::now();
        }

        using TP = std::chrono::steady_clock::time_point;
        using Duration = std::chrono::steady_clock::duration;
    }

    /**
     * Runnable is a wrapper around any C++ callable, it could be one of the following:
     * <ul>
     * <li>regular function</li>
     * <li>lambda</li>
     * <li>functor, a class with operator() implementation</li>
     * <ul>
     *
     * It's the actual execute target for a message
     */
    struct Runnable {
        template<typename FUNC, typename...ARGS>
        explicit Runnable(const FUNC& func, const ARGS& ...args) /*: _func(std::bind(func, args...))*/ {
            auto binder = std::bind(func, args...);
            _func = binder;
            _func_target = _func.target<decltype(binder)>();
        }

        Runnable() : _func_target{nullptr} {}

        void run() const {
            if (_func) _func();
        }

        /**
         * Check if target is empty or not
         * @return true if this runnable has a target to run, otherwise false
         */
        inline bool canRun() const { return _func != nullptr; }

        /**
         * Check if two runnable is equal, which means has the same target
         * @param lhs first
         * @param rhs seconds
         * @return true if two runnable has the same target, otherwise false
         */
        friend bool operator==(const Runnable& lhs, const Runnable& rhs) {
            return lhs._func_target == rhs._func_target;
        }

    private:
        std::function<void(void)> _func;

        /**
         * Generic pointer to hold function target, which can be used for == comparision
         */
        void *_func_target;
    };

    /**
     * Create a runnable from a function, lamba or functor with varidic arguments
     * @tparam FUNC the function, lamba or functor
     * @tparam ARGS the arguments
     * @param f the function, lamba or functor
     * @param args the arguments
     * @return a runnable can be used by message or handler
     */
    template<typename FUNC, typename...ARGS>
    Runnable make_runnable(FUNC&& f, ARGS&& ...args) {
        return Runnable(std::forward<FUNC>(f), std::forward<ARGS>(args)...);
    }

    /**
     * A simple implementation of Android/Java (Handler/HandlerThread/Message/MessageQueue/Looper) API in C++<br/>
     * All the class names and method names are keep the same with Android/Java API
     */
    class Handler;

    /**
     * A message is the basic entity for async communication between threads, a sender thread construct a message
     * and put into a <b>MessageQueue</b>, the receiver threads take message from queue and consume the message
     */
    struct Message {
        static Message obtain(const std::shared_ptr<Handler>& handler, const Runnable& r) {
            return Message(handler, r);
        }

        static Message obtain(const std::shared_ptr<Handler>& handler = nullptr,
                              int what = 0, int arg1 = 0, int arg2 = 0, void *obj = nullptr) {
            auto m = Message();
            m.target = handler;
            m.what = what;
            m.arg1 = arg1;
            m.arg2 = arg2;
            m.obj = obj;
            return m;
        }

        Message(std::shared_ptr<Handler> h, Runnable cb, void *token = nullptr) :
                target(std::move(h)), runnable(std::move(cb)), obj(token) {
        }

        Message() = default;

        /**
         * Send this message to the handler target
         */
        void sendToTarget();

        bool isNullMessage() { return nullptr == target; }

        explicit operator std::string() const {
            std::stringstream ss;
            ss << "what = " << what
               << ", arg1 = " << arg1
               << ", arg2 = " << arg2
               << ", obj = " << obj
               << ", when = " << when.time_since_epoch().count();
            return ss.str();
        }

        friend bool operator==(const Message& lhs, const Message& rhs) {
            return lhs.what == rhs.what && lhs.arg1 == rhs.arg1 &&
                   lhs.arg2 == rhs.arg2 && lhs.obj == rhs.obj;
        }

        std::shared_ptr<Handler> target;
        Runnable runnable;
        Clock::TP when;

        int what{};
        int arg1{};
        int arg2{};
        void *obj{};
    };

    /**
     * A queue to hold the message, only one message queue can exists for a thread
     */
    struct MessageQueue {

        Message next() {
            if (_quit) return Message();
            Clock::Duration d(0);

            for (;;) {
                std::unique_lock<std::mutex> lck(_mutex);
                if (_deque.empty()) {
                    _cv.wait(lck);
                } else {
                    _cv.wait_for(lck, d);
                }
                if (_quit) {
                    lck.unlock();
                    return Message();
                }

                if (!_deque.empty()) {
                    auto m = _deque.front();
                    d = m.when - Clock::now();
                    if (d.count() <= 0) {
                        _deque.pop_front();
                        lck.unlock();
                        return m;
                    }
                }

                lck.unlock();
            }

            return Message();
        }

        bool enqueue(Message& m, const Clock::TP& tp) {
            std::lock_guard<std::mutex> lck_guard(_mutex);
            m.when = tp;

            if (tp == Clock::min()) {
                _deque.push_front(m);
            } else {
                std::deque<Message>::const_iterator it;
                for (it = _deque.begin(); it != _deque.end(); ++it) {
                    if ((*it).when > tp) {
                        break;
                    }
                }
                _deque.insert(it, m);
            }

            _cv.notify_all();
            return true;
        }

        bool hasMessages(const std::shared_ptr<Handler>& handler, int what, void *obj) {
            std::lock_guard<std::mutex> lck_guard(_mutex);
            return std::find_if(_deque.begin(), _deque.end(), [handler, what, obj](const Message& m) -> bool {
                return m.target == handler && m.what == what && m.obj == obj;
            }) != _deque.end();
        }

        void removeMessages(const std::shared_ptr<Handler>& handler, int what, void *obj) {
            std::lock_guard<std::mutex> lck_guard(_mutex);
            auto it = std::remove_if(_deque.begin(), _deque.end(), [handler, what, obj](const Message& m) -> bool {
                return m.target == handler && m.what == what && m.obj == obj;
            });
            _deque.erase(it, _deque.end());
        }

        void removeMessages(const std::shared_ptr<Handler>& handler, const Runnable& runnable, void *token) {
            std::lock_guard<std::mutex> lck_guard(_mutex);
            auto it = std::remove_if(_deque.begin(), _deque.end(),
                                     [handler, runnable, token](const Message& m) -> bool {
                                         return m.target == handler && m.runnable == runnable && m.obj == token;
                                     });
            _deque.erase(it, _deque.end());
        }

        void removeMessages(const std::shared_ptr<Handler>& handler, void *token) {
            std::lock_guard<std::mutex> lck_guard(_mutex);
            auto it = std::remove_if(_deque.begin(), _deque.end(), [handler, token](const Message& m) -> bool {
                if (token == nullptr) {
                    return m.target == handler;
                } else {
                    return m.target == handler && m.obj == token;
                }
            });
            _deque.erase(it, _deque.end());
        }

        void setQuit(bool quit) {
            this->_quit = quit;
            _cv.notify_all();
        }

    private:
        std::deque<Message> _deque;
        std::mutex _mutex;
        std::condition_variable _cv;
        bool _quit = false;
    };

    struct Looper {
        static void prepare() {
            if (_looper) {
                throw std::runtime_error("Only one Looper may be created per thread");
            }
            _looper = std::make_shared<Looper>();
        }

        static void loop();

        static std::shared_ptr<Looper> myLooper() {
            return _looper;
        }

        bool enqueue(Message& m, const Clock::TP& tp) {
            return _mq.enqueue(m, tp);
        }

        void quit() {
            _mq.setQuit(true);
        }

        MessageQueue& getMessageQueue() { return _mq; }

    private:
        MessageQueue _mq;
        static thread_local std::shared_ptr<Looper> _looper;
    };

    thread_local std::shared_ptr<Looper> Looper::_looper{nullptr};

    struct Handler {
        struct Callback {
            template<typename FUNC>
            static Callback New(FUNC&& f) {
                return Callback(f);
            }

            template<typename FUNC>
            explicit Callback(FUNC f) : _callback(f) {}

            Callback() = default;

            void call(const Message& m) {
                if (_callback) _callback(m);
            }

            inline bool canCall() const {
                return _callback != nullptr;
            }

        private:
            std::function<void(const Message&)> _callback;
        };

        static std::shared_ptr<Handler> New(const std::shared_ptr<Looper>& l, const Callback& cb) {
            auto h = std::make_shared<Handler>(l, cb);
            h->_sharedSelf = h;
            return h;
        }

        explicit Handler(std::shared_ptr<Looper> l = nullptr,
                         Callback callback = Callback()) :
                _looper(std::move(l)), _callback(std::move(callback)) {}

        void dispatchMessage(const Message& m) {
            if (m.runnable.canRun()) {
                m.runnable.run();
            } else {
                if (_callback.canCall()) {
                    _callback.call(m);
                    return;
                }

                handleMessage(m);
            }
        }

        /**
         * override this in derived class to receive messages
         * @param m the message to handle
         */
        virtual void handleMessage(const Message& m) {

        }

        /**
         * Push the message with the current time stamp to the end of message queue after all pending message
         * @param msg message to push
         * @return true if add successfully, otherwise false
         */
        bool sendMessage(Message& msg) {
            return _looper->enqueue(msg, Clock::now());
        }

        /**
         * Place a message at the front of queue, will execute immediately at next event cycle
         * @param msg the message to send
         * @return true if add successfully, otherwise false
         */
        bool sendMessageAtFrontOfQueue(Message& msg) {
            return _looper->enqueue(msg, Clock::min());
        }

        /**
         * Send a message at specific time, the message will scheduled to execute at specific time
         * @param msg message to send
         * @param upTimeMillis uptime representation in milliseconds
         * @return true if schedule successfully, otherwise false
         */
        bool sendMessageAtTime(Message& msg, long upTimeMillis) {
            return _looper->enqueue(msg, Clock::TP(std::chrono::milliseconds(upTimeMillis)));
        }

        /**
         * Send a message to execute with a delay relative to current uptime
         * @param msg message to send
         * @param delayMillis delay time in milliseconds
         * @return true if successfully send, otherwise false
         */
        bool sendMessageDelayed(Message& msg, long delayMillis) {
            return _looper->enqueue(msg, Clock::now() + std::chrono::milliseconds(delayMillis));
        }

        template<typename Rep, typename Period>
        bool sendMessageDelayed(Message& msg, const std::chrono::duration<Rep, Period>& d) {
            return _looper->enqueue(msg, Clock::now() + d);
        };

        bool sendEmptyMessage(int what) {
            auto m = Message::obtain(_sharedSelf, what);
            return sendMessage(m);
        }

        bool sendEmptyMessageAtTime(int what, long upTimeMillis) {
            auto m = Message::obtain(_sharedSelf, what);
            return sendMessageAtTime(m, upTimeMillis);
        }

        bool sendEmptyMessageDelayed(int what, long delayMillis) {
            auto m = Message::obtain(_sharedSelf, what);
            return sendMessageDelayed(m, delayMillis);
        }

        template<typename Rep, typename Period>
        bool sendEmptyMessageDelayed(int what, const std::chrono::duration<Rep, Period>& d) {
            auto m = Message::obtain(_sharedSelf, what);
            return sendMessageDelayed(m, d);
        };

        bool post(const Runnable& r) {
            Message m(_sharedSelf, r);
            return sendMessage(m);
        }

        bool postAtFrontOfQueue(const Runnable& r) {
            Message m(_sharedSelf, r);
            return sendMessageAtFrontOfQueue(m);
        }

        bool postAtTime(const Runnable& r, long upTimeMillis) {
            Message m(_sharedSelf, r);
            return sendMessageAtTime(m, upTimeMillis);
        }

        bool postDelay(const Runnable& r, long delayMillis, void *token = nullptr) {
            Message m(_sharedSelf, r, token);
            return sendMessageDelayed(m, delayMillis);
        }

        template<typename Rep, typename Period>
        bool postDelay(const Runnable& r, const std::chrono::duration<Rep, Period>& d, void *token = nullptr) {
            Message m(_sharedSelf, r, token);
            return sendMessageDelayed(m, d);
        };

        bool hasMessages(int what, void *obj = nullptr) {
            return _looper->getMessageQueue().hasMessages(_sharedSelf, what, obj);
        }

        void removeCallbacks(const Runnable& runnable, void *token = nullptr) {
            _looper->getMessageQueue().removeMessages(_sharedSelf, runnable, token);
        }

        void removeMessages(int what, void *obj = nullptr) {
            _looper->getMessageQueue().removeMessages(_sharedSelf, what, obj);
        }

        void removeCallbacksAndMessages(void *token = nullptr) {
            _looper->getMessageQueue().removeMessages(_sharedSelf, token);
        }

        std::shared_ptr<Looper> getLooper() { return _looper; }

    private:
        std::shared_ptr<Looper> _looper;
        std::shared_ptr<Handler> _sharedSelf;
        Callback _callback;
    };

    struct HandlerThread {
        static std::unique_ptr<HandlerThread> New(const std::string& name) {
            return std::unique_ptr<HandlerThread>(new HandlerThread(name));
        }

        explicit HandlerThread(std::string name) : _name(std::move(name)) {}

        void start() {
            auto lck = new std::unique_lock<std::mutex>(_mutex);
            const auto self = this;
            _t = std::thread([self, lck]() {

                Looper::prepare();

                self->_looper = Looper::myLooper();
                lck->unlock();

                Looper::loop();
            });
        }

        void join() { if (_t.joinable()) _t.join(); }

        std::shared_ptr<Looper> getLooper() {
            std::lock_guard<std::mutex> lck_guard(_mutex);
            return _looper;
        }

        size_t getThreadId() {
            return std::hash<std::thread::id>{}(_t.get_id());
        }

        void quit() {
            if (_looper) _looper->quit();
        }

        void quitSafely() {
            if (_looper) _looper->quit();
        }

        ~HandlerThread() {
            if (_t.joinable()) _t.join();
        }

    private:
        std::shared_ptr<Looper> _looper;
        std::string _name;
        std::mutex _mutex;
        std::thread _t;
    };

    void Message::sendToTarget() {
        if (target) {
            target->sendMessage(*this);
        } else {
            throw std::runtime_error("No target found to send");
        }
    }

    void Looper::loop() {
        auto me = myLooper();
        auto& mq = me->_mq;
        for (;;) {
            auto m = mq.next();
            if (m.isNullMessage()) {
                return;
            }
            m.target->dispatchMessage(m);
        }
    }
}

/**
 * A duration of day suffix
 * @param n number of day
 * @return duration
 */
std::chrono::hours operator "" _d(unsigned long long n) {
    return std::chrono::hours(24 * n);
}

/**
 * A duration of hour suffix
 * @param n number of hours
 * @return duration
 */
std::chrono::hours operator "" _h(unsigned long long n) {
    return std::chrono::hours(n);
}

/**
 * A duration of minutes suffix
 * @param n number of minutes
 * @return duration
 */
std::chrono::minutes operator "" _min(unsigned long long n) {
    return std::chrono::minutes(n);
}

/**
 * A duration of seconds suffix
 * @param n seconds
 * @return duration
 */
std::chrono::seconds operator "" _s(unsigned long long n) {
    return std::chrono::seconds(n);
}

/**
 * A duration of milliseconds
 * @param n number of milli seconds
 * @return duration
 */
std::chrono::milliseconds operator "" _ms(unsigned long long n) {
    return std::chrono::milliseconds(n);
}

/**
 * A duration of microseconds
 * @param n number of micro seconds
 * @return duration
 */
std::chrono::microseconds operator "" _us(unsigned long long n) {
    return std::chrono::microseconds(n);
}

/**
 * A duration of nanoseconds
 * @param n number of nanoseconds
 * @return duration
 */
std::chrono::nanoseconds operator "" _ns(unsigned long long n) {
    return std::chrono::nanoseconds(n);
}

#endif //CPPHANDLER_HANDLER_H
