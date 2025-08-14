#include <memory>
#include <vector>
#include <bitset>
#include <array>
#include <mutex>
#include <memory_resource>
#include <cassert>

struct ObjectHeader{
    uint32_t magicNumber;
    void* originChunk;
    size_t chunkIndex;
};

template<typename T, size_t ChunkSize = 1024>
class GameObjectPool{
public:
    GameObjectPool(){
        static_assert(ChunkSize>0,"chunksize must be postice");
        static_assert(sizeof(T)<=64-sizeof(ObjectHeader),"object size too large foe alignment");
    }
    ~GameObjectPool(){
        clear();
    }
    template<typename... Args>
    T* allocate(Args&&... args){
        std::lock_guard  lock(m_mutex);
    }
    template<size_t N, typename... Args>
    std::array<T*, N> allocateBatch(Args&&... args){
        std::array<T*, N> result;
        std::lock_guard lock(m_mutex);
        size_t fromFreeList = std::min(N,m_freeList.size());
        for(size_t i=0;i<fromFreeList;++i){
            result[i]=m_freeList[m_freeList.size()-fromFreeList+i];
            new(result[i]) T(std::forward<Args>(args)...);
        }
        m_freeList.resize(m_freeList.size()-fromFreeList);
        for(size_t i =fromFreeList;i<N;++i){
            bool allocated = false;
            for(auto& chunk : m_chunks){
                if(chunk.freeCount>0){
                    result[i]=allocateFromChunk(chunk,std::forward<Args>(args)...);
                    allocated = true;
                    break;
                }
            }
            if(!allocated){
                m_chunks.emplace_back();
                result[i]=allocateFromChunk(m_chunks.back(),std::forward<Args>(args)...);
            }
        }
        return results;
    }

    void deallocate(T* obj) {
        if(!obj) return;
        std::lock_guard lock(m_mutex);
        ObjectHeader* header = getHeader(obj);
        assert(header->magicNumber == 0xDEADBEEF && "Corrupted object");

        obj->~T;
        m_freeList.push_back(obj);
    }
 
private:
    struct MemoryChunk {
        alignas(64) std::unique_ptr<std::byte[]> memory;
        std::bitset<ChunkSize> usedFlages;
        size_t freeCount = ChunkSize;
        MemoryChunk() : memory(new std::byte[ChunkSize * getObjectSize()]) {}
    };
    std::vector<MemoryChunk> m_chunks;
    std::vector<T*> m_freeList;
    mutable std::mutex m_mutex;

    static constexpr size_t getObjectSize() {
        return sizeof(ObjectHeader) + sizeof(T);
    }

    template<typename... Args>
    T* allocateFromChunk(MemoryChunk& chunk, Args&&... args) {
        size_t index =0;
        for(;index<ChunkSize;++index){
            if(!chunk.usedFlags.test(index)){
                break;
            }
        }
        assert(index < ChunkSize && "No free slot in chunk");
        chunk.usedFlages.set(index);
        --chunk.freeCount;
        std::byte* mem = chunk.memory.get() + index * getObjectSize();

        ObjectHeader* header = reinterpret_cast<ObjectHeader*>(mem);
        header->magicNumber = 0xDEADBEEF;
        header->originChunk = &chunk;
        header->chunkIndex = index;
        T* obj = reinterpret_cast<T*>(mem+sizeof(ObjectHeader));
        return new(obj) T(std::forward<Args>(args)...);
    }

    ObjectHeader* getHeader(T* obj) const {
        return reinterpret_cast<ObjectHeader*>(reinterpret_cast<std::byte*>(obj)-sizeof(ObjectHeader));
    }
    T* getObjectFromChunk(MemoryChunk& chunk, size_t index) const {
        std::byte* mem = chunk.memory.get() + index * getObjectSize();
        return reinterpret_cast<T*>(mem+sizeof(ObjectHeader));
    }
};