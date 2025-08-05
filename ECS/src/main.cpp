#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>

const size_t MAX_ENTITIES = 100000;
const size_t COMPONENT_POOL_SIZE = 1024 * 1024;

enum class UnitState {
    IDLE,
    MOVING,
    ATTACKING,
    DEAD
};

enum class DamageType {
    PHYSICAL,
    MAGIC,
    TRUE_DAMAGE
};

class IComponentPool{
public:
    virtual ~IComponentPool() = default;
    virtual void remove(size_t entity) = 0;     
};

template<typename T>
class ComponentPool : public IComponentPool{
private:
    struct Block {
        T data;
        bool active;
    };
    Block* blocks;
    size_t capacity;
    size_t count;
public:
    ComponentPool() {
        capacity = MAX_ENTITIES;
        blocks = static_cast<Block*>(malloc(capacity * sizeof(Block)));
        for( size_t i =0;i<capacity;++i){
            blocks[i].active = false;
        }
        count =0;
    }
    ~ComponentPool() {free(blocks)};
    T* assign(size_t entity){
        if(entity>=capacity) return nullptr;
        if(blocks[entity].active) return nullptr;
        blocks[entity].active = true;
        blocks[entity].data=T();
        count++;
        return &blocks[entity].data;
    }
    void remove(size_t entity) override {
        if(entity>=capacity || !blocks[entity].active) return;
        blocks[entity].active = false;
        count--;
    }
    T* get(size_t entity){
        return (entity<capacity&&blocks[entity].active) ? &blocks[entity].data : nullptr;
    }
    size_t size() const {return count;}
};

struct Transform {
    float x, y, z;
    Transform() : x(0), y(0), z(0) {}
};

struct CombatStats {
    int health;
    int maxHealth;
    int attack;
    int defense;
    float attackRange;
    float attackSpeed;
    float attackCooldown;
    DamageType damageType;
    UnitState state;

    CombatStats()
        : health(100), maxHealth(100), attack(10), defense(5),
          attackRange(5.0f), attackSpeed(1.0f), attackCooldown(0),
          damageType(DamageType::PHYSICAL), state(UnitState::IDLE) {}
};

struct Movement {
    float velocity;
    float direction;
    float moveRange;
    size_t targetEntity;

    Movement()
        : velocity(0), direction(0), moveRange(20.0f),
          targetEntity(-1) {}
};

struct StatusEffects {
    bool poisoned;
    bool stunned;
    bool burning;
    float effectDuration;

    StatusEffects()
        : poisoned(false), stunned(false), burning(false),
          effectDuration(0) {}
};

class ComponentManager {
private:
    std::unordered_map<size_t,IComponentPool*> componentPools;
public:
    template<typename T>
    void registerComponent(){
        size_t typeID = typeid(T).hash_code();
        if(componentPools.find(typeID) == componentPools.end()){
            componentPools[typeID] = new ComponentPool<T>();
        }
    }
    template<typename T>
    ComponentPool<T>* getPool(){
        size_t typeID = typeid(T).hash_code();
        auto it = componentPools.find(typeID);
        return (it!=componentPools.end()) ? static_cast<ComponentPool<T>*>(it->second) : nullptr;
    }
    template<typename T>
    T* assignComponent(size_t entity){
        if(auto pool = getPool<T>()){
            return pool->assign(entity);
        }
        return nullptr;
    }
    template<typename T>
    void removeComponent(size_t entity){
        if(auto pool = getPool<T>()){
            pool->remove(entity);
        }
    }
    void removeAllComponents(size_t entity){
        for(auto& pair : componentPools){
            pair.second->remove(entity);
        }
    }
    ~ComponentManager() {
        for(auto& pair : componentPools){
            delete pair.second;
        }
    }
};

class EntityManager {
private:
    std::vector<size_t> available;
    size_t livingCount;

public:
    EntityManager() : livingCount(0) {
        available.reserve(MAX_ENTITIES);
        for (size_t i = MAX_ENTITIES; i > 0; --i) {
            available.push_back(i - 1);
        }
    }

    size_t create() {
        if (available.empty()) return -1;
        size_t id = available.back();
        available.pop_back();
        livingCount++;
        return id;
    }

    void destroy(size_t entity) {
        available.push_back(entity);
        livingCount--;
    }

    size_t count() const { return livingCount; }
};

class CombatSystem {
private:
    ComponentManager* components;
    EntityManager* entities;
    
    int calculateDamage(int attack, int defense, DamageType type) {
        switch(type) {
            case DamageType::PHYSICAL:
                return std::max(1, attack - defense/2);
            case DamageType::MAGIC:
                return attack + rand() % (attack/2 + 1);
            case DamageType::TRUE_DAMAGE:
                return attack;
            default:
                return attack;
        }
    }

    bool inAttackRange(size_t attacker, size_t target) {
        auto transformPool = components->getPool<Transform>();
        Transform* t1 = transformPool->get(attacker);
        Transform* t2 = transformPool->get(target);
        auto combatPool = components->getPool<CombatStats>();
        CombatStats* stats = combatPool->get(attacker);

        if (!t1 || !t2 || !stats) return false;

        float dx = t1->x - t2->x;
        float dy = t1->y - t2->y;
        float distance = std::sqrt(dx*dx + dy*dy);

        return distance <= stats->attackRange;
    }

public:
    CombatSystem(ComponentManager* cm, EntityManager* em)
        : components(cm), entities(em) {}

    void update(float deltaTime) {
        auto combatPool = components->getPool<CombatStats>();
        auto transformPool = components->getPool<Transform>();
        auto movementPool = components->getPool<Movement>();
        auto statusPool = components->getPool<StatusEffects>();

        for (size_t i = 0; i < MAX_ENTITIES; ++i) {
            CombatStats* stats = combatPool->get(i);
            StatusEffects* status = statusPool->get(i);

            if (stats && stats->state != UnitState::DEAD && status) {
                // 状态效果持续伤害
                if (status->poisoned) {
                    stats->health -= 1;
                    status->effectDuration -= deltaTime;
                    if (status->effectDuration <= 0) status->poisoned = false;
                }

                if (status->burning) {
                    stats->health -= 2;
                    status->effectDuration -= deltaTime;
                    if (status->effectDuration <= 0) status->burning = false;
                }

                if (status->stunned) {
                    status->effectDuration -= deltaTime;
                    if (status->effectDuration <= 0) status->stunned = false;
                }

                // 检查死亡
                if (stats->health <= 0) {
                    stats->state = UnitState::DEAD;
                    stats->health = 0;
                }
            }
        }

        // 处理攻击逻辑
        for (size_t i = 0; i < MAX_ENTITIES; ++i) {
            CombatStats* attackerStats = combatPool->get(i);
            Movement* movement = movementPool->get(i);
            StatusEffects* status = statusPool ? statusPool->get(i) : nullptr;

            if (!attackerStats || attackerStats->state == UnitState::DEAD) continue;
            if (status && status->stunned) continue;

            // 更新攻击冷却
            attackerStats->attackCooldown -= deltaTime;

            // 检查攻击状态
            if (attackerStats->state == UnitState::ATTACKING) {
                if (movement && movement->targetEntity != -1) {
                    CombatStats* targetStats = combatPool->get(movement->targetEntity);

                    // 检查目标是否有效
                    if (!targetStats || targetStats->state == UnitState::DEAD) {
                        attackerStats->state = UnitState::IDLE;
                        movement->targetEntity = -1;
                        continue;
                    }

                    // 检查是否在攻击范围内
                    if (inAttackRange(i, movement->targetEntity)) {
                        movement->velocity = 0; // 停止移动

                        // 执行攻击
                        if (attackerStats->attackCooldown <= 0) {
                            int damage = calculateDamage(
                                attackerStats->attack,
                                targetStats->defense,
                                attackerStats->damageType
                            );

                            targetStats->health -= damage;
                            attackerStats->attackCooldown = 1.0f / attackerStats->attackSpeed;

                            // 30%几率附加状态效果
                            if (rand() % 100 < 30) {
                                if (StatusEffects* targetStatus = statusPool->get(movement->targetEntity)) {
                                    switch(rand() % 3) {
                                        case 0:
                                            targetStatus->poisoned = true;
                                            targetStatus->effectDuration = 3.0f;
                                            break;
                                        case 1:
                                            targetStatus->stunned = true;
                                            targetStatus->effectDuration = 1.0f;
                                            break;
                                        case 2:
                                            targetStatus->burning = true;
                                            targetStatus->effectDuration = 4.0f;
                                            break;
                                    }
                                }
                            }
                        }
                    } else {
                        // 不在攻击范围内，向目标移动
                        attackerStats->state = UnitState::MOVING;
                        Transform* targetTransform = transformPool->get(movement->targetEntity);
                        Transform* selfTransform = transformPool->get(i);

                        if (targetTransform && selfTransform) {
                            float dx = targetTransform->x - selfTransform->x;
                            float dy = targetTransform->y - selfTransform->y;
                            movement->direction = std::atan2(dy, dx);
                            movement->velocity = 2.0f; // 移动速度
                        }
                    }
                } else {
                    attackerStats->state = UnitState::IDLE;
                }
            }
        }
    }
};

class MovementSystem {
private:
    ComponentManager* components;

public:
    MovementSystem(ComponentManager* cm) : components(cm) {}

    void update(float deltaTime) {
        auto transformPool = components->getPool<Transform>();
        auto movementPool = components->getPool<Movement>();
        auto combatPool = components->getPool<CombatStats>();

        for (size_t i = 0; i < MAX_ENTITIES; ++i) {
            Transform* transform = transformPool->get(i);
            Movement* movement = movementPool->get(i);
            CombatStats* combat = combatPool->get(i);

            if (transform && movement && combat && combat->state != UnitState::DEAD) {
                // 移动逻辑
                if (movement->velocity > 0 && combat->state == UnitState::MOVING) {
                    transform->x += movement->velocity * std::cos(movement->direction) * deltaTime;
                    transform->y += movement->velocity * std::sin(movement->direction) * deltaTime;
                }
            }
        }
    }
};

class AISystem {
private:
    ComponentManager* components;
    EntityManager* entities;

public:
    AISystem(ComponentManager* cm, EntityManager* em)
        : components(cm), entities(em) {}

    void update() {
        auto combatPool = components->getPool<CombatStats>();
        auto movementPool = components->getPool<Movement>();
        auto transformPool = components->getPool<Transform>();

        for (size_t i = 0; i < MAX_ENTITIES; ++i) {
            CombatStats* stats = combatPool->get(i);
            Movement* movement = movementPool->get(i);

            if (!stats || stats->state == UnitState::DEAD) continue;

            // 空闲状态单位寻找目标
            if (stats->state == UnitState::IDLE) {
                // 随机选择目标
                size_t target = rand() % MAX_ENTITIES;
                CombatStats* targetStats = combatPool->get(target);

                // 验证目标有效性
                if (targetStats && targetStats->state != UnitState::DEAD && target != i) {
                    movement->targetEntity = target;
                    stats->state = UnitState::ATTACKING;
                }
            }
        }
    }
};

class CleanupSystem {
private:
    ComponentManager* components;
    EntityManager* entities;

public: 
    CleanupSystem(ComponentManager* cm,EntityManager* em) : components(cm),entities(em) {}
    void update(){
        auto combatPool = components->getPool<CombatStats>();
        for(size_t i=0;i<MAX_ENTITIES;++i){
            CombatStats* stats = combatPool->get(i);
            if(stats && stats->state == UnitState::DEAD){
                components->removeAllComponents(i);
                entities->destroy(i);
            }
        }

    }
};

class BattleSimulation {
private:
    EntityManager entities;
    ComponentManager components;
    CombatSystem combat;
    MovementSystem movement;
    AISystem ai;
    CleanupSystem cleanup;

public:
    BattleSimulation()
        : combat(&components, &entities),
          movement(&components),
          ai(&components, &entities),
          cleanup(&components, &entities)
    {
        components.registerComponent<Transform>();
        components.registerComponent<CombatStats>();
        components.registerComponent<Movement>();
        components.registerComponent<StatusEffects>();
    }

    void spawnUnit() {
        size_t entity = entities.create();
        if (entity == -1) return;

        components.getPool<Transform>()->assign(entity);
        components.getPool<CombatStats>()->assign(entity);
        components.getPool<Movement>()->assign(entity);
        components.getPool<StatusEffects>()->assign(entity);

        // 随机化单位属性
        CombatStats* stats = components.getPool<CombatStats>()->get(entity);
        if (stats) {
            stats->health = 80 + rand() % 40;
            stats->maxHealth = stats->health;
            stats->attack = 5 + rand() % 10;
            stats->defense = 3 + rand() % 7;
            stats->attackSpeed = 0.5f + (rand() % 100) / 100.0f;

            // 随机伤害类型
            stats->damageType = static_cast<DamageType>(rand() % 3);
        }
    }

    void spawnUnits(size_t count) {
        for (size_t i = 0; i < count && entities.count() < MAX_ENTITIES; ++i) {
            spawnUnit();
        }
    }

    void simulateBattle(float deltaTime) {
        ai.update();
        combat.update(deltaTime);
        movement.update(deltaTime);
        cleanup.update();
    }

    size_t unitCount() const { return entities.count(); }

    void printBattleStatus() {
        auto combatPool = components.getPool<CombatStats>();
        size_t alive = 0, attacking = 0, moving = 0;

        for (size_t i = 0; i < MAX_ENTITIES; ++i) {
            if (CombatStats* stats = combatPool->get(i)) {
                if (stats->state != UnitState::DEAD) {
                    alive++;
                    if (stats->state == UnitState::ATTACKING) attacking++;
                    if (stats->state == UnitState::MOVING) moving++;
                }
            }
        }

        std::cout << "Units: " << alive << " | "
                  << "Attacking: " << attacking << " | "
                  << "Moving: " << moving << std::endl;
    }
};

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    BattleSimulation battle;

    battle.spawnUnits(100000);
    std::cout << "Units spawned: " << battle.unitCount() << std::endl;

    const float deltaTime = 0.016f; // 60 FPS
    for (int i = 0; i < 1000; ++i) {
        battle.simulateBattle(deltaTime);

        if (i % 60 == 0) { // 每秒显示一次
            std::cout << "Frame " << i << " - ";
            battle.printBattleStatus();
        }

        if (battle.unitCount() < 100) break;
    }

    std::cout << "Battle simulation completed" << std::endl;
    return 0;
}