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

#include <iostream>
#include <cstdio>
#include "Handler.h"

using namespace cpphandler;

void callback_function(const Message& msg)
{
    std::clog << __PRETTY_FUNCTION__ << static_cast<std::string>(msg) << std::endl;
}

int main(int argc, char* argv[])
{
    auto ht = HandlerThread::New("test_callback_function");
    ht->start();

    auto cb_function = Handler::Callback::New(callback_function);

    Handler::New(ht->getLooper(), cb_function);
    ht->quit();
    ht->join();

    return EXIT_SUCCESS;
}
