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

#include <cstdio>
#include <iostream>
#include "Handler.h"

int main(int argc, char* argv[])
{
    auto ht = cpphandler::HandlerThread::New("test_callback_lambada");
    ht->start();

    auto cb_lambda = [](const cpphandler::Message& msg) {
        std::clog << __PRETTY_FUNCTION__ << static_cast<std::string>(msg) << std::endl;
    };

    auto cb = cpphandler::Handler::Callback::New(cb_lambda);

    auto handler = cpphandler::Handler::New(ht->getLooper(), cb);

    cpphandler::Message::obtain(handler, 245).sendToTarget();

    std::this_thread::sleep_for(std::chrono::seconds(2));

    ht->quit();
    ht->join();

    return EXIT_SUCCESS;
}
