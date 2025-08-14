#include <vector>
#include <queue>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <stdexcept>
#include <cstddef>

class Character {
public:
    virtual ~Character() = default;
    virtual void Update(float deltatime) = 0;
    virtual void Reset() {};

    uint32_t GetID() const {return id;}
protected:
    friend class CharacterPool;
    uint32_t id = 0;
    bool isActive = false;
};
class CharacterPool {
    public:
        using Slot = std::aligned_storage_t<256,32>;

        explicit CharacterPool(size_t capacity = 5000) :
            maxCapacity(capacity),
            activeCount(0)
            {
                characterStorage.resize(maxCapacity);
                for(size_t i=0;i<maxCapacity;++i){
                    freeIndices.push(i);
                }
            }
        template<typename T, typename... Args>
        T* Create(Args&&... args){
            static_assert(std::is_base_of_v<Character,T>,"T must inherit from Character");
            if (freeIndices.empty()){
                if(activeCount>=maxCapacity){
                    HandlePoolFull();
                    return nullptr;
                }
            }
            size_t index = freeIndices.front();
            freeIndices.pop();
            Slot* slot = &characterStorage[index];
            T* obj = new(slot) T(std::forward<Args>(args)...);
            obj->id = GenerateID();
            obj->isActive = true;
            activeCharacters[index] = obj;
            activeCount++;
            return obj;
        }
        void Destroy(Character* character) {
            if(!character || !character->isActive) return;
            auto it = activeCharacters.find(GetIndex(character));
            if(it == activeCharacters.end()) return;
            character->~Character();
            character->isActive = false;
            freeIndices.push(it->first);
            activeCharacters.erase(it);
            activeCount--;
        }
        void UpdateAll(float deltaTime) {
            for(auto& [index,character] : activeCharacters) {
                if(character->isActive) {
                    character->Update(deltaTime);
                }
            }
        }
        size_t GetActiveCount() const {return activeCount;}
    private:
        size_t GetIndex(Character* character) const {
            uintptr_t charAddr = reinterpret_cast<uintptr_t>(character);
            uintptr_t baseAddr = reinterpret_cast<uintptr_t>(characterStorage.data());
            uintptr_t offset = charAddr - baseAddr;
            return static_cast<size_t>(offset/ sizeof(Slot));
        }        
        void HandlePoolFull() {
            throw std::runtime_error("Character pool capacity exceeded!");
        }
        uint32_t GenerateID() {
            static uint32_t nextID = 1;
            return nextID++;
        }
    private:
        std::vector<Slot> characterStorage;
        std::queue<size_t> freeIndices;
        std::unordered_map<size_t, Character*> activeCharacters;
        const size_t maxCapacity;
        size_t activeCount;
};
class NPC : public Character {
    public:
        NPC(int level, const std::string& name) : level(level),name(name){}
        void Update(float deltaTime) override {

        }
        void Reset() override {
            level =1;
            name = "Unnamed";
        }
private:
    int level;
    std::string name;
};
class Player : public Character {
public:
    explicit Player(uint32_t playerID) : playerID(playerID) {}
    
    void Update(float deltaTime) override {

    }
private:
    uint32_t playerID;
};
int main(){
    CharacterPool pool(5000);
    Player* p1 = pool.Create<Player>(1001);
    NPC* npc1 = pool.Create<NPC>(5,"Warrior");
    NPC* npc2 = pool.Create<NPC>(3,"Archer");
    for(int frame =0;frame<1000;++frame){
        pool.UpdateAll(0.016f);
        if(frame%100 == 0 && npc1){
            pool.Destroy(npc1);
            npc1=nullptr;
            NPC* newNPC = pool.Create<NPC>(7,"mage");
        }
    }
    if(p1) pool.Destroy(p1);
    if(npc2) pool.Destroy(npc2);
}