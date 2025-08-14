#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <cmath>
#include <iostream>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <ctime>
#include <iomanip>

class StackAllocator {
public:
   explicit StackAllocator(size_t size)
        : m_totalSize(size), m_currentOffset(0){
        m_startPtr=static_cast<char*>(malloc(size));
        if(!m_startPtr){
            throw std::bad_alloc();
        }    
    }
    ~StackAllocator(){
        free(m_startPtr);
    }
    StackAllocator(const StackAllocator&) = delete;
    StackAllocator& operator=(const StackAllocator&) = delete;

    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)){
        size_t adjustment = alignAdjustment(m_startPtr+m_currentOffset,alignment);
        if(m_currentOffset+adjustment+size>m_totalSize){
            throw std::bad_alloc();
        }
        void* alignedPtr = m_startPtr+m_currentOffset+adjustment;
        m_currentOffset+=adjustment+size;
        return alignedPtr;
    }

    void reset(){
        m_currentOffset=0;
    }

    size_t used() const {
        return m_currentOffset;
    }

    size_t capacity() const {
        return m_totalSize;
    }

    void* getStart(){
        return m_startPtr;
    }

    static size_t alignAdjustment(const void* ptr, size_t alignment){
        size_t mask = alignment-1;
        size_t misalignment = reinterpret_cast<size_t>(ptr) & mask;
        return misalignment ? alignment - misalignment : 0;
    }

private:
    char* m_startPtr;
    size_t m_totalSize;
    size_t m_currentOffset;
};

struct alignas(32) Particle {
    float position[2];
    float velocity[2];
    float color[4];
    float size;
    float life;
    float rotation;
    float rotationSpeed;
};

class ParticleSystem{
public:
    ParticleSystem(size_t maxParticles)
        : allocator_(calculateMemoryRequirement(maxParticles)),
          maxParticles_(maxParticles),
          activeParticles_(0){
            std::cout<<"粒子系统初始化：最大粒子数 = "<<maxParticles 
                     <<",分配内存 = " << (allocator_.capacity() / 1024.0f) << " KB\n";
          }
    void emit(size_t count, float x,float y){
        if(activeParticles_ +count > maxParticles_){
            count = maxParticles_ - activeParticles_;
            if(count ==0) return;

            for(size_t i =0;i<count;i++){
                Particle* p= static_cast<Particle*>(allocator_.allocate(sizeof(Particle), alignof(Particle)));
                p->position[0]=x+randomFloat(-5.0f, 5.0f);
                p->position[1]=y+randomFloat(-5.0f,5.0f);
                p->velocity[0] = randomFloat(-50.0f, 50.0f);
                p->velocity[1] = randomFloat(-100.0f, -50.0f);
                p->color[0] = randomFloat(0.7f, 1.0f); // R
                p->color[1] = randomFloat(0.2f, 0.5f); // G
                p->color[2] = randomFloat(0.1f, 0.3f); // B
                p->color[3] = 1.0f;                   // A
                p->size = randomFloat(2.0f, 8.0f);
                p->life = randomFloat(1.0f, 3.0f);
                p->rotation = randomFloat(0.0f, 360.0f);
                p->rotationSpeed = randomFloat(-180.0f, 180.0f);
                activeParticles_++;
            }
        }
    }

    void update(float deltaTime){
        Particle* current = getPaticleArray();
        size_t remaining = activeParticles_;
        while(remaining--){
            if (current->life<=0.0f){
                current++;
                continue;
            }
            // 更新位置
            current->position[0] += current->velocity[0] * deltaTime;
            current->position[1] += current->velocity[1] * deltaTime;
            // 更新旋转
            current->rotation += current->rotationSpeed * deltaTime;
            // 应用重力
            current->velocity[1] += 98.1f * deltaTime;
            // 减少寿命
            current->life -= deltaTime;
            // 淡出效果
            current->color[3] = std::min(1.0f, current->life);
            // 移动到下一个粒子
            current++;
        }
    }

    void newFrame(){
        if (activeParticles_ == 0 ){
            allocator_.reset();
            return;
        }
        size_t index=0;
        std::vector<Particle> tempPaticles;
        Particle* src = getPaticleArray();
        for(size_t i=0;i<activeParticles_;i++){
            if(src[i].life>0.0f){
                tempPaticles[index++] = src[i];
            }
        }

        allocator_.reset();
        activeParticles_=0;
        for(auto& p : tempPaticles){
            Particle* newParticle = static_cast<Particle*>(allocator_.allocate(sizeof(Particle), alignof(Particle)));
            *newParticle = p;
            activeParticles_++;
        }
    }

    Particle* getPaticleArray() {
        return static_cast<Particle*>(allocator_.getStart());
    }
    size_t activeParticles() const { return activeParticles_; }
    size_t maxParticles() const { return maxParticles_; }
    size_t usedMemory() const { return allocator_.used(); }
    size_t totalMemory() const { return allocator_.capacity(); }
private:
    size_t calculateMemoryRequirement(size_t maxParticles){
        const size_t particleSize = sizeof(Particle);
        const size_t aligment = alignof(Particle);
        return maxParticles * (particleSize + aligment -1);
    }

    float randomFloat(float min, float max){
        return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX/(max-min)));
    }

    StackAllocator allocator_;
    size_t maxParticles_;
    size_t activeParticles_;
};

void runPerformanceTest() {
    const size_t MAX_PARTICLES = 100000;
    const int FRAMES = 100;
    const float FIXED_DELTA_TIME = 1.0f/60.0f;
    ParticleSystem system(MAX_PARTICLES);

    std::cout<<"\n性能测试开始("<<FRAMES << " 帧，最大粒子数:"<< MAX_PARTICLES<<")\n";
    clock_t start =clock();
    for(int i=0;i<FRAMES;++i){
        if(i%5==0){
            system.emit(500,500.0f,300.0f);
        }
        system.update(FIXED_DELTA_TIME);
        system.newFrame();
        if(i%10){
            std::cout << "帧 " << std::setw(3) << i 
            << ": 活跃粒子 = " << std::setw(6) << system.activeParticles()
            << ", 使用内存 = " << std::setw(6) << (system.usedMemory() / 1024) 
            << " KB\n";
        }
    }
    clock_t end = clock();
    double duration = static_cast<double>(end- start) / CLOCKS_PER_SEC;

    std::cout << "\n测试完成\n";
    std::cout << "总时间: " << duration << " 秒\n";
    std::cout << "平均帧时间: " << (duration * 1000 / FRAMES) << " 毫秒\n";
    std::cout << "平均每帧粒子数: " << (MAX_PARTICLES * 0.7) << "\n";
    std::cout << "总内存分配: " << (system.totalMemory() / 1024) << " KB\n";
}

int main() {
    StackAllocator allocator(1024 * 1024);
    int* arr = static_cast<int*>(allocator.allocate(10 * sizeof(int),16));
    struct alignas(64) BigStruct {
        double data[8];
    };
    BigStruct* big = static_cast<BigStruct*>(allocator.allocate(sizeof(BigStruct) ,64));
    
    allocator.reset();

    runPerformanceTest();

    return 0;
}
