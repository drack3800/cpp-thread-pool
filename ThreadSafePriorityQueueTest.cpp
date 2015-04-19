#define BOOST_TEST_MODULE ThreadSafeQueue Test

#include <boost/test/unit_test.hpp>
#include "ThreadSafePriorityQueue.hpp"

#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>
#include <random>

void concurrentPush150randomNumbersInQueue(ThreadSafePriorityQueue<int>& q) {
    std::default_random_engine generator;
    std::vector<std::thread> v;
    for (int i = 0; i < 50; ++i) {
        v.emplace_back([&q, &generator]{
            q.push(generator());
            q.push(generator());
            q.push(generator());
        });
    }
    for (int i = 0; i < 50; ++i) {
        v[i].join();
    }
    return;
}

BOOST_AUTO_TEST_CASE(correct_push_test) {
    ThreadSafePriorityQueue<int> q;
    q.push(5);
    BOOST_CHECK(q.size() == 1);
    BOOST_CHECK(q.empty() == false);
    auto el = q.try_pop();
    BOOST_REQUIRE(el != nullptr);
    BOOST_CHECK(*el == 5);
    
    q.push(2);
    q.push(1);
    BOOST_CHECK(q.size() == 2);
    BOOST_CHECK(*q.try_pop() == 1);
    BOOST_CHECK(q.size() == 1);
    BOOST_CHECK(*q.try_pop() == 2);
    BOOST_CHECK(q.size() == 0);
    BOOST_CHECK(q.empty());
}

BOOST_AUTO_TEST_CASE(correct_concurrent_push_test) {
    ThreadSafePriorityQueue<int> q;
    std::vector<std::thread> threads;
    std::default_random_engine generator;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&q, &generator]{
            for (int i = 0; i < 1000; ++i) {
                q.push(generator());
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
}

BOOST_AUTO_TEST_CASE(correct_pop_test) {
    ThreadSafePriorityQueue<std::string> q;
    // correct order: aaa, aaa, aab, aba, baa
    q.push("aaa");
    q.push("aba");
    q.push("aab");
    q.push("baa");
    q.push("aaa");

    BOOST_CHECK(*q.try_pop() == "aaa");
    BOOST_CHECK(*q.try_pop() == "aaa");
    BOOST_CHECK(*q.try_pop() == "aab");
    BOOST_CHECK(*q.try_pop() == "aba");
    BOOST_CHECK(*q.try_pop() == "baa");

    BOOST_CHECK(q.try_pop() == nullptr);

}


BOOST_AUTO_TEST_CASE(try_pop_and_wait_and_pop_equivalence_test_without_concurrency) {
    // Just copy the same test for try_pop method with according replacing.

    ThreadSafePriorityQueue<std::string> q;
    // correct order: aaa, aaa, aab, aba, baa
    q.push("aaa");
    q.push("aba");
    q.push("aab");
    q.push("baa");
    q.push("aaa");

    BOOST_CHECK(*q.wait_and_pop() == "aaa");
    BOOST_CHECK(*q.wait_and_pop() == "aaa");
    BOOST_CHECK(*q.wait_and_pop() == "aab");
    BOOST_CHECK(*q.wait_and_pop() == "aba");
    BOOST_CHECK(*q.wait_and_pop() == "baa");

}


BOOST_AUTO_TEST_CASE(concurrent_push_test) {
    ThreadSafePriorityQueue<int> q;
    concurrentPush150randomNumbersInQueue(q);
    auto min = q.try_pop();
    while (!q.empty()) {
        auto next = q.try_pop();
        BOOST_CHECK(*min <= *next);
        min = next;
    }
}

BOOST_AUTO_TEST_CASE(concurrent_pop_test) {
    ThreadSafePriorityQueue<int> q;
    int N = 10000;
    for (int i = 0; i < N; ++i) {
        q.push(N - i);
    }
    std::vector<std::thread> threads;
    std::atomic<int> amount(0);
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&]{
            for (int j = 0; j < N / 10; ++j) {
                auto top = q.try_pop();
                if (top)
                    amount++;
            }
        });
    }
    for (int i = 0; i < 10; ++i) {
        threads[i].join();
    }
    BOOST_CHECK(amount == N);
}

BOOST_AUTO_TEST_CASE(stop_correct_notifies_all_threads) {
    ThreadSafePriorityQueue<int> q;
    std::vector<std::thread> threads(5);
    for (int i = 0; i < 5; ++i) {
        threads[i] = std::thread([&q]{ q.wait_and_pop(); });
    }
    q.stop();
    for (auto &thread : threads) {
        thread.join();
    }
}
