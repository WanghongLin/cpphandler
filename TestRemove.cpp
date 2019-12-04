#include <iostream>
#include "Handler.h"

void callback_function(const cpphandler::Message& m) {
    std::cout << __PRETTY_FUNCTION__ << std::endl;
}

void f3(const std::string& tag)
{
    std::cout << __PRETTY_FUNCTION__ << ' ' << tag << std::endl;
}

int main(int argc, char* argv[]) {
    auto ht = cpphandler::HandlerThread::New("test_thread");
    ht->start();

    auto cb_function = cpphandler::Handler::Callback::New(callback_function);

    auto handler = cpphandler::Handler::New(ht->getLooper(), cb_function);

    auto r1 = cpphandler::make_runnable([](std::string tag) {
        std::cout << tag << std::endl;
    }, "Runnable 1");
    auto r2 = cpphandler::make_runnable([](char c) {
        std::cout << "Runnable 2 " << c << std::endl;
    }, 'c');
    auto r3 = cpphandler::make_runnable(f3, "Runnable 3");

    cpphandler::Message::obtain(handler, r3).sendToTarget();
    handler->postDelay(r1, 1_s);
    handler->postDelay(r2, 2_s);

    handler->removeCallbacks(r1);

    std::this_thread::sleep_for(std::chrono::seconds(4));
    ht->quit();

    return 0;
}
