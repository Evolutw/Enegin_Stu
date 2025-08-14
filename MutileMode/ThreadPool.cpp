#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <vector>
#include <future>

class ThreadPool {
public:
    ThreadPool(size_t num_threads) : stop(false){
        for(size_t i=0;i<num_threads;++i){
            workers.emplace_back([this]{
                while(true){
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        condition.wait(lock,[this]{
                            return stop || !tasks.empty();
                        });
                        if(stop && tasks.empty()) return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task;
                }
            });
        }
    }
    template<class F,class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>{
            using return_type = typename std::result_of<F(Args...)>::type;

            auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );

            std::future<return_type>
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                if(stop){
                    throw std::runtime_error("enqueue on stopped ThreadPool");
                }
                tasks.emplace([task](){(*task)();});
            }
            condition.notify_one();
            return res;
        }
        ~ThreadPool(){
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                stop=true;
            }
            condition.notify_all();
            for(std::thread &worker : workers){
                worker.join();
            }
        }
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};
int main(){
    ThreadPool pool(4);
    std::vector<std::future<int>> results;
    for(int i=0;i<8;i++){
        results.emplace_back(
            pool.enqueue([i]{
                std::cout << "Task " << i << " started\n";
                std::this_thread::sleep_for(std::chrono::seconds(1));
                std::cout << "Task " << i << " finished\n";
                return i*i;
            })
        );
    }
    for(auto&& result : results) {
        std::cout << "Result: " << result.get() << std::endl;
    }
    return 0;
}