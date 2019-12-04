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
// Created by Wanghong Lin on 2019/11/26.
//

#include <cstdio>
#include <iostream>
#include "Handler.h"

using namespace cpphandler;

struct PrivateData {
    char c;
    int i;
    double d;
    std::string s;
};

struct CallbackFunctor {
    void operator()(const Message& m) {
        std::cout << __PRETTY_FUNCTION__ << (std::string)(m) << std::endl;
        if (m.what == 123) {
            auto privateData = static_cast<PrivateData*>(m.obj);
            std::cout << privateData->c << ' ' << privateData->i << ' ' << privateData->d << ' '
                      << privateData->s << std::endl;
            delete privateData;
        }
    }
};

int main(int argc, char* argv[])
{
    auto ht = HandlerThread::New("test_callback_functor");
    ht->start();
    auto cb_functor = Handler::Callback::New(CallbackFunctor());

    auto handler = Handler::New(ht->getLooper(), cb_functor);

    Message::obtain(handler, 123, 0, 0, new PrivateData {'c', 123, 3.14159, "a string"}).sendToTarget();

    std::this_thread::sleep_for(std::chrono::seconds(1));
    ht->quit();
    ht->join();

    return EXIT_SUCCESS;
}
