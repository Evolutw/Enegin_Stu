#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

class LockFreeCounter {
public:
    LockFreeCounter(int init =0) : count_(init){}
    void increment(int amount = 1){
        count_.fetch_add(amount,std::memory_order_relaxed);
    }
    void decrement(int amount = 1){
        count_.fetch_sub(amount,std::memory_order_relaxed);
    }
    int get() const {
        return count_.load(std::memory_order_acquire);
    }
    void reset() {
        count_.store(0,std::memory_order_relaxed);
    }

private:
    std::atomic<int> count_;
};
void test_counter() {
    LockFreeCounter counter;
    const int num_threads = 100;
    const int increments_per_thread = 1000;
    std::vector<std::thread> threads;
    for(int i=0;i<num_threads;++i){
        threads.emplace_back([&counter]{
            for(int j=0;j<increments_per_thread;++j){
                counter.increment();
            }
        });
    }
    for(auto& t : threads) {
        t.join();
    }
    std::cout << "Expected: " << num_threads * increments_per_thread << "\n";
    std::cout << "Actual  : " << counter.get() << "\n";
}
int main() {
    test_counter();
    return 0;
}