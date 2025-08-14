#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>
#include <chrono>

const int kNumPhilosophers = 5;
class DiningPhilosophers {
public:
    DiningPhilosophers() {
        for(int i=0;i<kNumPhilosophers;++i){
            chopsticks.push_back(std::make_unique<std::mutex>());
        }
    }
    void dine(int id){
        while(true){
            think(id);
            eat(id);
        }
    }
    void think(int id) {
        std::cout << "Philosopher " << id << " is thinking...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    void eat(int id){
        if(id%2==0){
            chopsticks[id]->lock();
            chopsticks[(id+1)%kNumPhilosophers]->lock();
        } else {
            chopsticks[(id+1)%kNumPhilosophers]->lock();
            chopsticks[id]->lock();
        }
        std::cout << "Philosopher " << id << " is eating...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        chopsticks[id]->unlock();
        chopsticks[(id+1)%kNumPhilosophers]->unlock();
    }
private:
    std::vector<std::unique_ptr<std::mutex>> chopsticks;
};
int main(){
    DiningPhilosophers table;
    std::vector<std::thread> philosophers;
    for(int i=0;i<kNumPhilosophers;++i){
        philosophers.emplace_back(&DiningPhilosophers::dine,table,i);
    }
    return 0;
}