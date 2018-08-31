//
// ActorTest.cc
//
// Copyright © 2018 Couchbase. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "Async.hh"
#include "Actor.hh"
#include "LiteCoreTest.hh"

using namespace litecore::actor;


static Retained<AsyncProvider<string>> aProvider, bProvider;

static Async<string> provideA() {
    return aProvider;
}

static Async<string> provideB() {
    return bProvider;
}

static Async<string> provideSum() {
    string a, b;
    BEGIN_ASYNC_RETURNING(string)
    asyncCall(a, provideA());
    asyncCall(b, provideB());
    return a + b;
    END_ASYNC()
}


static Async<string> provideSumPlus() {
    string a;
    BEGIN_ASYNC_RETURNING(string)
    asyncCall(a, provideSum());
    return a + "!";
    END_ASYNC()
}


static Async<string> provideImmediately() {
    BEGIN_ASYNC_RETURNING(string)
    return "immediately";
    END_ASYNC()
}


static Async<int> provideLoop() {
    string n;
    int sum = 0;
    int i = 0;
    BEGIN_ASYNC_RETURNING(int)
    for (i = 0; i < 10; i++) {
        asyncCall(n, provideSum());
        //fprintf(stderr, "n=%f, i=%d, sum=%f\n", n, i, sum);
        sum += n.size() * i;
    }
    return sum;
    END_ASYNC()
}


static string provideNothingResult;

static void provideNothing() {
    string a, b;
    BEGIN_ASYNC()
    asyncCall(a, provideA());
    asyncCall(b, provideB());
    provideNothingResult = a + b;
    END_ASYNC()
}


class TestActor : public Actor {
public:

    Async<string> provideSum() {
        string a, b;
        BEGIN_ASYNC_RETURNING(string)
        asyncCall(a, provideA());
        asyncCall(b, provideB());
        return a + b;
        END_ASYNC()
    }
};



TEST_CASE("Async", "[Async]") {
    aProvider = Async<string>::provider();
    bProvider = Async<string>::provider();
    {
        Async<string> sum = provideSum();
        REQUIRE(!sum.ready());
        aProvider->setResult("hi");
        REQUIRE(!sum.ready());
        bProvider->setResult(" there");
        REQUIRE(sum.ready());
        REQUIRE(sum.result() == "hi there");
    }
    aProvider = bProvider = nullptr;
    CHECK(AsyncContext::gInstanceCount == 0);
}


TEST_CASE("Async, other order", "[Async]") {
    aProvider = Async<string>::provider();
    bProvider = Async<string>::provider();
    {
        Async<string> sum = provideSum();
        REQUIRE(!sum.ready());
        bProvider->setResult(" there");    // this time provideB() finishes first
        REQUIRE(!sum.ready());
        aProvider->setResult("hi");
        REQUIRE(sum.ready());
        REQUIRE(sum.result() == "hi there");
    }
    aProvider = bProvider = nullptr;
    CHECK(AsyncContext::gInstanceCount == 0);
}


TEST_CASE("AsyncWaiter", "[Async]") {
    aProvider = Async<string>::provider();
    bProvider = Async<string>::provider();
    {
        Async<string> sum = provideSum();
        string result;
        sum.wait([&](string s) {
            result = s;
        });
        REQUIRE(!sum.ready());
        REQUIRE(result == "");
        aProvider->setResult("hi");
        REQUIRE(!sum.ready());
        REQUIRE(result == "");
        bProvider->setResult(" there");
        REQUIRE(sum.ready());
        REQUIRE(result == "hi there");
    }
    aProvider = bProvider = nullptr;
    CHECK(AsyncContext::gInstanceCount == 0);
}


TEST_CASE("Async, 2 levels", "[Async]") {
    aProvider = Async<string>::provider();
    bProvider = Async<string>::provider();
    {
        Async<string> sum = provideSumPlus();
        REQUIRE(!sum.ready());
        aProvider->setResult("hi");
        REQUIRE(!sum.ready());
        bProvider->setResult(" there");
        REQUIRE(sum.ready());
        REQUIRE(sum.result() == "hi there!");
    }
    aProvider = bProvider = nullptr;
    CHECK(AsyncContext::gInstanceCount == 0);
}


TEST_CASE("Async, loop", "[Async]") {
    aProvider = Async<string>::provider();
    bProvider = Async<string>::provider();
    {
        Async<int> sum = provideLoop();
        for (int i = 1; i <= 10; i++) {
            REQUIRE(!sum.ready());
            aProvider->setResult("hi");
            REQUIRE(!sum.ready());
            aProvider = Async<string>::provider();
            bProvider->setResult(" there");
            bProvider = Async<string>::provider();
        }
        REQUIRE(sum.ready());
        REQUIRE(sum.result() == 360);
    }
    aProvider = bProvider = nullptr;
    CHECK(AsyncContext::gInstanceCount == 0);
}


TEST_CASE("Async, immediately", "[Async]") {
    {
        Async<string> im = provideImmediately();
        REQUIRE(im.ready());
        REQUIRE(im.result() == "immediately");
    }
    CHECK(AsyncContext::gInstanceCount == 0);
}


TEST_CASE("Async void fn", "[Async]") {
    aProvider = Async<string>::provider();
    bProvider = Async<string>::provider();
    provideNothingResult = "";
    {
        provideNothing();
        REQUIRE(provideNothingResult == "");
        aProvider->setResult("hi");
        REQUIRE(provideNothingResult == "");
        bProvider->setResult(" there");
        REQUIRE(provideNothingResult == "hi there");
    }
    aProvider = bProvider = nullptr;
    CHECK(AsyncContext::gInstanceCount == 0);
}
