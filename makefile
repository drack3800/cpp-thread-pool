TP_SRC_FILES := ut_starter.cpp ThreadPoolTest.cpp
PQ_SRC_FILES := ut_starter.cpp ThreadSafePriorityQueueTest.cpp
HEADER_FILES := ThreadPool.hpp ThreadSafePriorityQueue.hpp
COMPILE_OPTIONS := -g -std=c++11 -O3
LINKER_OPTIONS := -lboost_system -l boost_thread -l boost_unit_test_framework

all: test-thread-pool test-priority-queue
test-thread-pool: $(TP_SRC_FILES) $(HEADER_FILES)
	g++ $(COMPILE_OPTIONS) -o $@ $(TP_SRC_FILES) $(LINKER_OPTIONS)

test-priority-queue: $(PQ_SRC_FILES) $(HEADER_FILES)
	g++ $(COMPILE_OPTIONS) -o $@ $(PQ_SRC_FILES) $(LINKER_OPTIONS)
