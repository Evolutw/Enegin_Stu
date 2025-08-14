#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
class CirtualarBuffer {
public:
    explicit CirtualarBuffer(size_t size)
        : buf_(std::vector<int>(size)),
          max_size_(size),
          head_(0),
          tail_(0),
          count_(0){}
    void put(int item){
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock,[this](){return count_!=max_size_;});
        buf_[tail_]=item;
        tail_=(tail_+1)%max_size_;
        ++count_;
        not_empty_.notify_one();
    }
    int get(){
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock,[this](){return count_!=0;});
        int item=buf_[head_];
        head_=(head_+1)%max_size_;
        --count_;
        not_full_.notify_one();
        return item;
    }
private:
    std::vector<int> buf_;
    size_t head_;
    size_t tail_;
    size_t max_size_;
    size_t count_;
    std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
};
void producer(CirtualarBuffer& buffer,int items){
    for(int i=0;i<items;i++){
        buffer.put(i);
        std::cout << "Produced: " << i << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 模拟生产耗时
    }
}
void consumer(CirtualarBuffer& buffer, int items){
    for(int i=0;i<items;++i){
        int val=buffer.get();
        std::cout << "Consumed: " << val << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(150)); // 模拟消费耗时
    }
}
int main() {
    CirtualarBuffer buffer(5);
    std::thread prod_thred(producer,std::ref(buffer),10);
    std::thread cons_thred(consumer,std::ref(buffer),10);

    prod_thred.join();
    cons_thred.join();

    return 0;
}