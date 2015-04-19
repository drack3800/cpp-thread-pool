#include <vector>
#include <memory>
#include <iostream>
#include <mutex>
#include <atomic>


#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>

template <class T, class Cmp = std::less<T>>
class ThreadSafePriorityQueue {
private:
    const float MAX_LOCKS_NODES_RATIO = 1.5;

    Cmp _comp;
    std::vector<std::shared_ptr<T>> _heap;

    std::vector<boost::detail::spinlock> _node_locks;

    // This variable is needed for correct exclusive locks for pop and push.
    // Push increments this variable in the beginning and decrements it before returning.
    // Pop works in the reverse order. Dec at start, Inc before returning.
    std::atomic<int> _push_pop_balance;
    boost::mutex _balance_mutex;
    boost::condition_variable _balance_event;

    bool _is_stopped;

    // Parent of index i is (i - 1) / 2.
    size_t parent(size_t nodei) {
        return nodei == 0 ? -1 : (nodei - 1) / 2;
    }

    // Childs of index i are 2i + 1 and 2i + 2.
    size_t lch(size_t nodei) {
        int retval = 2 * nodei + 1;
        return retval < this->_heap.size() ? retval : -1;
    }

    size_t rch(size_t nodei) {
        int retval = 2 * nodei + 2;
        return retval < this->_heap.size() ? retval : -1;
    }

    // This function is called only when spinlock of nodei is locked.
    void heapify_down(size_t nodei) {
        using unique_for_spinlock = boost::unique_lock<boost::detail::spinlock>;

        // _node_locks[nodei].try_lock();
        // When parent is locked it is guaranteed that childs will not be changed.
        unique_for_spinlock ulk_node, ulk_left, ulk_right;
        ulk_node = unique_for_spinlock(_node_locks[nodei], boost::adopt_lock);
        while (lch(nodei) != -1) {
            ulk_node.unlock();
            std::lock_guard<boost::mutex> lk(_balance_mutex);
            ulk_node.lock();
            if (lch(nodei) == -1)
                break;
            ulk_left = unique_for_spinlock(_node_locks[lch(nodei)], boost::defer_lock);
            if (rch(nodei) == -1) {
                ulk_left.lock();
                // If rchi == -1 then we are at the (bottom - 1) level of the heap => return.
                if (_comp(*_heap[lch(nodei)], *_heap[nodei]))
                    std::swap(_heap[nodei], _heap[lch(nodei)]);
                return;
            } else {
                ulk_right = unique_for_spinlock(_node_locks[rch(nodei)], boost::defer_lock);
                boost::lock(ulk_left, ulk_right);
                int index_of_min = _comp(*_heap[lch(nodei)], *_heap[rch(nodei)]) ? lch(nodei) : rch(nodei);
                if (_comp(*_heap[index_of_min], *_heap[nodei])) {
                    // Swap and continue.
                    std::swap(_heap[nodei], _heap[index_of_min]);
                    if (index_of_min == lch(nodei)) {
                        // We go to the left subtree.
                        ulk_right.unlock();
                        ulk_node = std::move(ulk_left);
                    } else {
                        // We go to the right subtree.
                        ulk_left.unlock();
                        ulk_node = std::move(ulk_right);
                    }
                    nodei = index_of_min;
                } else {
                    return;
                }
            }
        }
    }

    void heapify_up(size_t nodei) {
        while (parent(nodei) != -1) {
            if (_comp(*_heap[nodei], *_heap[parent(nodei)])) {
                std::swap(_heap[nodei], _heap[parent(nodei)]);
                nodei = parent(nodei);
            } else {
                return;
            }
        }
    }

    std::shared_ptr<T> _internal_pop(bool wait) {
        boost::unique_lock<boost::mutex> balance_lk(_balance_mutex);
       
        _balance_event.wait(balance_lk,
            [this, wait] {
            return (!wait || (this->_heap.size() != 0 && this->_push_pop_balance <= 0) || _is_stopped == true);
        });
        // No pushes can execute alongside below lines.
        if (this->_heap.size() == 0)
            return nullptr;
        
        _push_pop_balance--;

        std::shared_ptr<T> retval = _heap.front();
        if (_heap.size() == 1) {
            _heap.pop_back();
        } else {
            std::swap(_heap.front(), _heap.back());
            _heap.pop_back();
            if (_heap.size() * MAX_LOCKS_NODES_RATIO < _node_locks.size())
                _node_locks.resize(int(_heap.size() * MAX_LOCKS_NODES_RATIO));
            // Root's spinlock must be locked before lk destructing, otherwise race condition might happen.
            _node_locks[0].lock();
            balance_lk.unlock();
            heapify_down(0);
        }

        _push_pop_balance++;
        _balance_event.notify_one();
        return std::move(retval);
    }

public:
    ThreadSafePriorityQueue() : _is_stopped(false), _push_pop_balance(0) { }
    ~ThreadSafePriorityQueue() { }

    std::shared_ptr<T> try_pop() {
        return _internal_pop(false);
    }

    // Returns nullptr if and only if the stop() method was called.
    std::shared_ptr<T> wait_and_pop() {
        return _internal_pop(true);
    }

    void push(std::shared_ptr<T> object) {
        boost::unique_lock<boost::mutex> balance_lk(_balance_mutex);
        _balance_event.wait(balance_lk, [this]{ return this->_push_pop_balance >= 0; });
        _push_pop_balance++;

        _heap.push_back(object);
        _node_locks.emplace_back();
        heapify_up(_heap.size() - 1);

        _push_pop_balance--;
        _balance_event.notify_one();
    }

    void push(const T& object) {
        push(std::make_shared<T>(object));
    }

    bool empty() {
        return _heap.size() == 0;
    }

    size_t size() {
        return _heap.size();
    }
  
    void stop() {
        _is_stopped = true;
        _push_pop_balance = 0;
        _balance_event.notify_all();
    }

};
