#include <iostream>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <chrono>

class ReaderWriterModel {
public:
    void read(int id){
        std::shared_lock lock(mutex_);
        std::cout << "Reader " << id << " is reading...\n";
        // 模拟读取操作
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    void write(int id){
        std::unique_lock lock(mutex_);
        std::cout << "Writer " << id << " is writing...\n";
        // 模拟写入操作
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

private:
    std::shared_mutex mutex_;
};
int main(){
    ReaderWriterModel rw;
    std::vector<std::thread> threads;
    for(int i=0;i<5;i++){
        threads.emplace_back([&rw,i]{
            for(int j=0;j<3;j++){
                rw.read(i);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
    }
    for(int i=0;i<2;i++){
        threads.emplace_back([&rw,i]{
            for(int j=0;j<2;++j){
                rw.write(i);
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
            }
        });
    }
    for(auto& t : threads){
        t.join();
    }
    return 0;
}