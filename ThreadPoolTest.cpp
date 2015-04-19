#define BOOST_TEST_MODULE ThreadPoolTest
#ifdef _WIN32
    #define BOOST_TEST_MAIN
#endif

#include <iostream>
#include <vector>
#include <algorithm>

#include <boost/thread/shared_mutex.hpp>
#include <boost/thread.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/execution_monitor.hpp>

#include "ThreadPool.hpp"

int test_func_1() {
    return 10;
}

int test_func_2() {
    return 1;
}

int test_func_3() {
    return -1;
}

BOOST_AUTO_TEST_CASE(correct_function_calculation_without_exceptions) {
    ThreadPool<int> tp(1);
    std::vector<int> correct_results = { -1, 1, 1, 10, 10 };
    std::vector<std::shared_future<int>> futures;
    
    futures.push_back(tp.enqueue(test_func_1, 1000));
    futures.push_back(tp.enqueue(test_func_2, 999));
    futures.push_back(tp.enqueue(test_func_1, 900));
    futures.push_back(tp.enqueue(test_func_3, 997));
    futures.push_back(tp.enqueue(test_func_2, 950));
    tp.join();
    /*for (auto& future : futures) {
        try {
            std::cout << future.get() << std::endl;
        } catch (std::exception& e) {
            std::cout << e.what() << std::endl;
        }
    }*/
    std::vector<int> results(futures.size());
    for (int i = 0; i < results.size(); ++i) {
        results[i] = futures[i].get();
    }
    std::sort(results.begin(), results.end());
    for (int i = 0; i < results.size(); ++i) {
        BOOST_CHECK_EQUAL(results[i], correct_results[i]);
    }
}

BOOST_AUTO_TEST_CASE(correct_exception_pushing_in_future) {
    ThreadPool<int> tp(5);
    std::shared_future<int> f = tp.enqueue([]() -> int { throw std::runtime_error("heeeey"); });
    BOOST_CHECK_THROW(f.get(), std::runtime_error);
}
