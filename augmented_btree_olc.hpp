#pragma once

#include <sched.h>

#include <atomic>
#include <cassert>
#include <cstring>
#include <mutex>

#include <sys/mman.h>

#include "index.hpp"
#include "x86-instructions.h"

static const uint64_t pageSize = Config::Nodesize;

enum class PageType : uint8_t { BTreeInner = 1, BTreeLeaf = 2 };

struct OptLock {
    std::atomic<uint64_t> typeVersionLockObsolete{0b100};

    OptLock() {}
    OptLock(const OptLock &other) : typeVersionLockObsolete(other.typeVersionLockObsolete.load()){}

    bool isLocked(uint64_t version) {
        return ((version & 0b10) == 0b10);
    }

    uint64_t readLockOrRestart(bool& needRestart) {
        uint64_t version;
        version = typeVersionLockObsolete.load();
        if (isLocked(version) || isObsolete(version)) {
            // _mm_pause();
            needRestart = true;
        }
        return version;
    }

    void writeLockOrRestart(bool& needRestart) {
        uint64_t version;
        version = readLockOrRestart(needRestart);
        if (needRestart) return;

        upgradeToWriteLockOrRestart(version, needRestart);
        if (needRestart) return;
    }

    void upgradeToWriteLockOrRestart(uint64_t& version, bool& needRestart) {
        if (typeVersionLockObsolete.compare_exchange_strong(version, version + 0b10)) {
            version = version + 0b10;
        } else {
            // _mm_pause();
            needRestart = true;
        }
    }

    void writeUnlock() {
        typeVersionLockObsolete.fetch_add(0b10);
    }

    bool isObsolete(uint64_t version) {
        return (version & 1) == 1;
    }

    void checkOrRestart(uint64_t startRead, bool& needRestart) const {
        readUnlockOrRestart(startRead, needRestart);
    }

    void readUnlockOrRestart(uint64_t startRead, bool& needRestart) const {
        needRestart = (startRead != typeVersionLockObsolete.load());
    }

    void writeUnlockObsolete() {
        typeVersionLockObsolete.fetch_add(0b11);
    }
};

struct NodeBase : public OptLock {
    PageType type;
    uint16_t count;

    NodeBase() {}
    NodeBase(const NodeBase &other) : OptLock(other), type(other.type), count(other.count){}
    virtual void dummy() = 0;

    template <typename x86_inst>
    void place_code(x86_inst& inst, unsigned char* code_space, unsigned long& position){
        uint16_t size = inst.get_size();
        unsigned char* encoded_inst = (unsigned char*)(&inst);
        for(unsigned int i=0; i<size; ++i){
            code_space[position++] = encoded_inst[i];
        }
    }
};

template <class Key, class Payload>
struct BTreeLeaf : public NodeBase {
    struct Entry {
        Key k;
        Payload p;
    };

    static const PageType typeMarker = PageType::BTreeLeaf;    
    static const uint64_t maxEntries = (pageSize - sizeof(NodeBase)) / (sizeof(Key) + sizeof(Payload));

    Key keys[maxEntries];
    Payload payloads[maxEntries];

    BTreeLeaf (const BTreeLeaf<Key, Payload> &other) : NodeBase(other) {
        for (auto i = 0U; i <= other.count; i++){
            keys[i] = other.keys[i];
            payloads[i] = other.payloads[i];
        }
    }

    BTreeLeaf() {
        count = 0;
        type = typeMarker;
    }

    bool isFull() { return count == maxEntries; };

    unsigned lowerBound(Key k) {
        unsigned lower = 0;
        unsigned upper = count;
        do {
            unsigned mid = ((upper - lower) / 2) + lower;
            if (k < keys[mid]) {
                upper = mid;
            } else if (k > keys[mid]) {
                lower = mid + 1;
            } else {
                return mid;
            }
        } while (lower < upper);
        return lower;
    }

    unsigned lowerBoundLinear(const Key key) {
        auto iter = 0U;
        while (keys[iter] < key && keys[iter] > 0 && iter < count) {
            iter++;
        }
        return iter;
    }

    void insert(Key k, Payload p) {
        assert(count < maxEntries);
        if (count) {
            unsigned pos = lowerBound(k);
            if ((pos < count) && (keys[pos] == k)) {
                // Upsert
                payloads[pos] = p;
                return;
            }
            memmove(keys + pos + 1, keys + pos, sizeof(Key) * (count - pos));
            memmove(payloads + pos + 1, payloads + pos, sizeof(Payload) * (count - pos));
            keys[pos] = k;
            payloads[pos] = p;
        } else {
            keys[0] = k;
            payloads[0] = p;
        }
        count++;
    }

    unsigned long to_machine_code_x86(unsigned char* code_space, bool linear_search, unsigned long& current_address, unsigned char* return_address, uint16_t lower = 0, uint16_t upper = 0){
        unsigned long starting_address = current_address;

        if(linear_search){
            for(auto i = 0; i<count; ++i){
                // move payload into register RDX
                x86_mov_r64_imm64 mov_payload(RDX, payloads[i]);
                place_code(mov_payload, code_space, current_address);

                if(keys[i] > INT32_MAX){

                    // move compare key into register RCX
                    x86_mov_r64_imm64 mov_key(RCX, keys[i]);
                    place_code(mov_key, code_space, current_address);

                    // compare registers RDI and RCX
                    x86_cmp_r64_r64 cmp(RDI, RCX);
                    place_code(cmp, code_space, current_address);

                } else {
                    x86_cmp_r64_imm32 cmp(RDI, (int32_t) keys[i]);
                    place_code(cmp, code_space, current_address);
                }

                x86_mov_m8_imm8 mov_success(RSI, 1);
                place_code(mov_success, code_space, current_address);

                // jump to code jumping to return code
                x86_jcc_rel32 je(JE, (uint32_t)(((uint64_t) return_address) - (uint64_t)&code_space[current_address]) - sizeof(x86_jcc_rel32));
                place_code(je, code_space, current_address);

            }

            x86_mov_m8_imm8 mov_failure(RSI, 0);
            place_code(mov_failure, code_space, current_address);            

            // not found, return nothing
            x86_optional_failure_full fail;
            place_code(fail, code_space, current_address);

        } else {
            if(lower >= upper){

                if(keys[lower] > INT32_MAX){
                    // move compare key into register RCX
                    x86_mov_r64_imm64 mov_key(RCX, keys[lower]);
                    place_code(mov_key, code_space, current_address);

                    // compare registers RDI and RCX
                    x86_cmp_r64_r64 cmp(RDI, RCX);
                    place_code(cmp, code_space, current_address);

                } else {
                    // compare registers RDI and Immediate Value
                    x86_cmp_r64_imm32 cmp(RDI, (int32_t) keys[lower]);
                    place_code(cmp, code_space, current_address);
                }

                // jump to code jumping to return code
                x86_jcc_rel32 jne(JNE, sizeof(x86_mov_r64_imm64) + sizeof(x86_mov_m8_imm8) + sizeof(x86_jmp_rel32));
                place_code(jne, code_space, current_address);

                // move payload into register RDX
                x86_mov_r64_imm64 mov_payload(RDX, payloads[lower]);
                place_code(mov_payload, code_space, current_address);

                x86_mov_m8_imm8 mov_success(RSI, 1);
                place_code(mov_success, code_space, current_address);

                // jump to code returning value
                x86_jmp_rel32 jmp((uint32_t)(((uint64_t) return_address) - (uint64_t)&code_space[current_address]) - sizeof(x86_jmp_rel32));
                place_code(jmp, code_space, current_address);

                x86_mov_m8_imm8 mov_failure(RSI, 0);
                place_code(mov_failure, code_space, current_address);   

                // not found, return nothing
                x86_optional_failure_full fail;
                place_code(fail, code_space, current_address);
            } else {
                uint16_t mid = ((upper - lower) / 2) + lower;

                if(keys[mid] > INT32_MAX){
                    // move compare key into register RCX
                    x86_mov_r64_imm64 mov_key(RCX, keys[mid]);
                    place_code(mov_key, code_space, current_address);

                    // compare registers RDI and RCX
                    x86_cmp_r64_r64 cmp(RDI, RCX);
                    place_code(cmp, code_space, current_address);

                } else {
                    // compare registers RDI and Immediate Value
                    x86_cmp_r64_imm32 cmp(RDI, (int32_t) keys[mid]);
                    place_code(cmp, code_space, current_address);
                }

                // save the address for the conditional branch to be written to later
                unsigned long jcc_address = current_address;
                current_address += sizeof(x86_jcc_rel32);

                // recursively write code for one part of the binary search
                unsigned long jump_address = to_machine_code_x86(code_space, linear_search, current_address, return_address, lower, mid);

                x86_jcc_rel32 ja(JA, current_address - sizeof(x86_jcc_rel32) - jcc_address);
                place_code(ja, code_space, jcc_address);

                // recursively write code for the other part of the binary search
                to_machine_code_x86(code_space, linear_search, current_address, return_address, mid+1, upper);
            }
        }
        return starting_address;
    }

    unsigned int code_size_x86(bool linear_search, uint16_t lower = 0, uint16_t upper = 0){
        unsigned int size = 0;
        if(linear_search){
            for(auto i = 0; i<count; ++i){
                size += sizeof(x86_mov_r64_imm64);
                if(keys[i] > INT32_MAX){
                    size += sizeof(x86_mov_r64_imm64);
                    size += sizeof(x86_cmp_r64_r64);
                } else{
                    size += sizeof(x86_cmp_r64_imm32);
                }
                size += sizeof(x86_mov_m8_imm8);
                size += sizeof(x86_jcc_rel32);
            }
            
            size += sizeof(x86_mov_m8_imm8);
            size += sizeof(x86_optional_failure_full);
        } else {
            if(lower >= upper){
                if(keys[lower] > INT32_MAX){
                    size += sizeof(x86_mov_r64_imm64);
                    size += sizeof(x86_cmp_r64_r64);
                } else{
                    size += sizeof(x86_cmp_r64_imm32);
                }
                size += sizeof(x86_jcc_rel32);
                size += sizeof(x86_mov_r64_imm64);
                size += sizeof(x86_mov_m8_imm8);
                size += sizeof(x86_jmp_rel32);

                size += sizeof(x86_mov_m8_imm8);
                size += sizeof(x86_optional_failure_full);
            } else {
                uint16_t mid = ((upper - lower) / 2) + lower;

                if(keys[mid] > INT32_MAX){
                    size += sizeof(x86_mov_r64_imm64);
                    size += sizeof(x86_cmp_r64_r64);
                } else{
                    size += sizeof(x86_cmp_r64_imm32);
                }

                size += code_size_x86(linear_search, lower, mid);

                size += sizeof(x86_jcc_rel32);

                size += code_size_x86(linear_search, mid+1, upper);
            }
        }
        return size;
    }

    BTreeLeaf* split(Key& sep) {
        BTreeLeaf* newLeaf = new BTreeLeaf();
        newLeaf->count = count - (count / 2);
        count = count - newLeaf->count;
        memcpy(newLeaf->keys, keys + count, sizeof(Key) * newLeaf->count);
        memcpy(newLeaf->payloads, payloads + count, sizeof(Payload) * newLeaf->count);
        sep = keys[count - 1];
        return newLeaf;
    }

    void dummy(){}
};

template <class Key>
struct BTreeInner : public NodeBase {
    static const PageType typeMarker = PageType::BTreeInner;
    static const uint64_t maxEntries = (pageSize - sizeof(NodeBase)) / (sizeof(Key) + sizeof(NodeBase*));
    NodeBase* children[maxEntries];
    Key keys[maxEntries];

    BTreeInner (const BTreeInner<Key> &other) : NodeBase(other) {
        for (auto i = 0U; i <= other.count; i++){
            keys[i] = other.keys[i];
        }
    }

    BTreeInner() {
        count = 0;
        type = typeMarker;
    }

    ~BTreeInner(){
        for (auto i = 0U; i <= count; i++) {
            delete children[i];
        }
    }

    bool isFull() { return count == (maxEntries - 1); };

    NodeBase *get_pointer(const size_t index) {
        assert(index <= maxEntries - 1);
        return children[index];
    }

    void update_pointer(NodeBase *node, const size_t index) {
        assert(index <= maxEntries - 1);
        children[index] = node;
    }

    unsigned lowerBound(Key k) {
        unsigned lower = 0;
        unsigned upper = count;
        do {
            unsigned mid = ((upper - lower) / 2) + lower;
            if (k < keys[mid]) {
                upper = mid;
            } else if (k > keys[mid]) {
                lower = mid + 1;
            } else {
                return mid;
            }
        } while (lower < upper);
        return lower;
    }

    unsigned lowerBoundLinear(const Key key) {
        auto iter = 0U;
        while (keys[iter] < key && keys[iter] > 0 && iter < count) {
            iter++;
        }
        return iter;
    }

    BTreeInner* split(Key& sep) {
        BTreeInner* newInner = new BTreeInner();
        newInner->count = count - (count / 2);
        count = count - newInner->count - 1;
        sep = keys[count];
        memcpy(newInner->keys, keys + count + 1, sizeof(Key) * (newInner->count + 1));
        memcpy(newInner->children, children + count + 1, sizeof(NodeBase*) * (newInner->count + 1));
        return newInner;
    }

    void insert(Key k, NodeBase* child) {
        assert(count < maxEntries - 1);
        unsigned pos = lowerBound(k);
        memmove(keys + pos + 1, keys + pos, sizeof(Key) * (count - pos + 1));
        memmove(children + pos + 1, children + pos, sizeof(NodeBase*) * (count - pos + 1));
        keys[pos] = k;
        children[pos] = child;
        std::swap(children[pos], children[pos + 1]);
        count++;
    }

    unsigned long to_machine_code_x86(unsigned char* code_space, bool linear_search, unsigned long& current_address, unsigned char* return_address, int16_t code_layer = -1, uint16_t lower = 0, uint16_t upper = 0){
        unsigned long starting_address = current_address;

        if(linear_search){
            unsigned long search_address = current_address;
            unsigned long jump_address;
            for(auto i = 0; i<count; ++i){
                if(keys[i] > INT32_MAX){
                    current_address += sizeof(x86_mov_r64_imm64) + sizeof(x86_cmp_r64_r64);
                } else {
                    current_address += sizeof(x86_cmp_r64_imm32);
                }
                if(code_layer == 0){
                    current_address += sizeof(x86_mov_r64_imm64) + sizeof(x86_jcc_rel32);
                } else {
                    current_address += sizeof(x86_jcc_rel32);
                }
            }
            if(code_layer == 0){
                current_address += sizeof(x86_mov_r64_imm64);
                jump_address = current_address;
                current_address += sizeof(x86_mov_m8_imm8) + sizeof(x86_optional_success);
            } else {
                current_address += sizeof(x86_jmp_rel32);
            }

            unsigned char* children_addresses[count + 1];

            if(code_layer != 0){
                for(auto i = 0; i<=count; ++i){
                    if (children[i]->type == PageType::BTreeInner) {
                        BTreeInner<Key>* inner = static_cast<BTreeInner<Key>*>(children[i]);
                        children_addresses[i] = &code_space[inner->to_machine_code_x86(code_space, linear_search, current_address, return_address, code_layer - 1)];
                    } else {
                        BTreeLeaf<Key, Value>* leaf = static_cast<BTreeLeaf<Key, Value>*>(children[i]);
                        children_addresses[i] = &code_space[leaf->to_machine_code_x86(code_space, linear_search, current_address, return_address)];
                    }    
                }
            }

            for(auto i = 0; i<count; ++i){
                // move children address into register RBX

                if(keys[i] > INT32_MAX){

                    // move compare key into register RCX
                    x86_mov_r64_imm64 mov_key(RCX, keys[i]);
                    place_code(mov_key, code_space, search_address);

                    // compare registers RDI and RCX
                    x86_cmp_r64_r64 cmp(RDI, RCX);
                    place_code(cmp, code_space, search_address);

                } else {
                    // compare registers RDI and Immediate Value
                    x86_cmp_r64_imm32 cmp(RDI, (int32_t) keys[i]);
                    place_code(cmp, code_space, search_address);
                }

                if(code_layer == 0){
                    x86_mov_r64_imm64 mov_child(RAX, ((uint64_t) this->children[i]));
                    place_code(mov_child, code_space, search_address);
                    x86_jcc_rel32 jbe(JBE, (uint32_t)(((uint64_t) &code_space[jump_address]) - (uint64_t)&code_space[search_address]) - sizeof(x86_jcc_rel32));
                    place_code(jbe, code_space, search_address);
                } else {
                    // jump to child address in RBX
                    x86_jcc_rel32 jbe(JBE, (uint32_t)(((uint64_t) children_addresses[i]) - (uint64_t)&code_space[search_address]) - sizeof(x86_jcc_rel32));
                    place_code(jbe, code_space, search_address);
                }

            }

            if(code_layer == 0){
                x86_mov_r64_imm64 mov_child(RAX, ((uint64_t) this->children[count]));
                place_code(mov_child, code_space, search_address);

                x86_mov_m8_imm8 mov_success(RSI, 2);
                place_code(mov_success, code_space, search_address);

                x86_optional_success ret;
                place_code(ret, code_space, search_address);
            } else {
                x86_jmp_rel32 jmp((uint32_t)(((uint64_t) children_addresses[count]) - (uint64_t)&code_space[search_address]) - sizeof(x86_jmp_rel32));
                place_code(jmp, code_space, search_address);
            }

        } else {
            if(lower >= upper){
                if(code_layer == 0){
                    x86_mov_r64_imm64 mov_child(RAX, ((uint64_t) this->children[lower]));
                    place_code(mov_child, code_space, current_address);

                    x86_mov_m8_imm8 mov_success(RSI, 2);
                    place_code(mov_success, code_space, current_address);

                    x86_optional_success ret;
                    place_code(ret, code_space, current_address);
                } else {
                    if (children[lower]->type == PageType::BTreeInner) {
                        BTreeInner<Key>* inner = static_cast<BTreeInner<Key>*>(children[lower]);
                        inner->to_machine_code_x86(code_space, linear_search, current_address, return_address, code_layer - 1, 0, inner->count);
                    } else {
                        BTreeLeaf<Key, Value>* leaf = static_cast<BTreeLeaf<Key, Value>*>(children[lower]);
                        leaf->to_machine_code_x86(code_space, linear_search, current_address, return_address, 0, leaf->count);
                    }
                }
            } else {
                uint16_t mid = ((upper - lower) / 2) + lower;

                if(keys[mid] > INT32_MAX){

                    // move compare key into register RCX
                    x86_mov_r64_imm64 mov_key(RCX, keys[mid]);
                    place_code(mov_key, code_space, current_address);

                    // compare registers RDI and RCX
                    x86_cmp_r64_r64 cmp(RDI, RCX);
                    place_code(cmp, code_space, current_address);

                } else {
                    // compare registers RDI and Immediate Value
                    x86_cmp_r64_imm32 cmp(RDI, (int32_t) keys[mid]);
                    place_code(cmp, code_space, current_address);
                }

                unsigned long ja_address = current_address;
                current_address += sizeof(x86_jcc_rel32);

                unsigned long lower_address = to_machine_code_x86(code_space, linear_search, current_address, return_address, code_layer, lower, mid);
                unsigned long upper_address = to_machine_code_x86(code_space, linear_search, current_address, return_address, code_layer, mid + 1, upper);

                x86_jcc_rel32 ja(JA, (uint32_t)(((uint64_t) &code_space[upper_address]) - (uint64_t)&code_space[ja_address]) - sizeof(x86_jcc_rel32));
                place_code(ja, code_space, ja_address);

            }
        }
        return starting_address;
    }

    unsigned int code_size_x86(bool linear_search, int16_t code_layer = -1, uint16_t lower = 0, uint16_t upper = 0){
        unsigned int size = 0;

        if(linear_search){

            if(code_layer != 0){
                for(auto i = 0; i<=count; ++i){
                    if (children[i]->type == PageType::BTreeInner) {
                        BTreeInner<Key>* inner = static_cast<BTreeInner<Key>*>(children[i]);
                        size += inner->code_size_x86(linear_search, code_layer-1);
                    } else {
                        BTreeLeaf<Key, Value>* leaf = static_cast<BTreeLeaf<Key, Value>*>(children[i]);
                        size += leaf->code_size_x86(linear_search);
                    }    
                }
            }

            for(auto i = 0; i<count; ++i){

                if(keys[i] > INT32_MAX){
                    size += sizeof(x86_mov_r64_imm64) + sizeof(x86_cmp_r64_r64);
                } else {
                    size += sizeof(x86_cmp_r64_imm32);
                }

                if(code_layer == 0){
                    size += sizeof(x86_mov_r64_imm64);
                    size += sizeof(x86_jcc_rel32);
                } else {
                    size += sizeof(x86_jcc_rel32);
                }


            }
            
            if(code_layer == 0){
                size += sizeof(x86_mov_r64_imm64);
                size += sizeof(x86_mov_m8_imm8);
                size += sizeof(x86_optional_success);
            } else {
                size += sizeof(x86_jmp_rel32);
            }

        } else {
            if(lower >= upper){
                if(code_layer == 0){
                    size += sizeof(x86_mov_r64_imm64);
                    size += sizeof(x86_mov_m8_imm8);
                    size += sizeof(x86_optional_success);
                } else {
                    if (children[lower]->type == PageType::BTreeInner) {
                        BTreeInner<Key>* inner = static_cast<BTreeInner<Key>*>(children[lower]);
                        size += inner->code_size_x86(linear_search, code_layer -1, 0, inner->count);
                    } else {
                        BTreeLeaf<Key, Value>* leaf = static_cast<BTreeLeaf<Key, Value>*>(children[lower]);
                        size += leaf->code_size_x86(linear_search, 0, leaf->count);
                    }
                }
            } else {
                uint16_t mid = ((upper - lower) / 2) + lower;

                if(keys[mid] > INT32_MAX){
                    size += sizeof(x86_mov_r64_imm64) + sizeof(x86_cmp_r64_r64);
                } else {
                    size += sizeof(x86_cmp_r64_imm32);
                }
                size += sizeof(x86_jcc_rel32);

                size += code_size_x86(linear_search, code_layer, lower, mid);
                size += code_size_x86(linear_search, code_layer, mid + 1, upper);
            }
        }
        return size;
    }

    void dummy(){}
};

template <class Key, class Value>
struct BTreeOLC {
    std::atomic<NodeBase*> root;
    std::mutex code_generation_lock;
    std::atomic<unsigned int> insert_read_num{0};
    std::atomic<unsigned int> insert_write_num{0};
    std::atomic<unsigned int> code_read_num{0};
    std::atomic<unsigned int> write_num{0};
    std::atomic<bool> code_writing{false};
    std::atomic<bool> code_generating{false};
    std::atomic<bool> insert_writing{false};
    unsigned char* code = nullptr;
    unsigned long code_size;
    std::optional<unsigned long> (*get_code_func)(unsigned long, unsigned char&);
    BTreeOLC* insert_tree = nullptr;
    unsigned int size;
    unsigned int code_tree_size = 100;
    unsigned int max_code_depth = -1;

    BTreeOLC(bool insert_t = true) {
        root = new BTreeLeaf<Key, Value>();
        if(insert_t){
            insert_tree = new BTreeOLC<Key, Value>(false);
        }
        size = 0;
    }

    ~BTreeOLC (){
        delete root;
        if(this->insert_tree){
            delete insert_tree;
        }
        if(this->code){
            munmap(this->code, this->code_size);
        }
    }
    
    void makeRoot(Key k, NodeBase* leftChild, NodeBase* rightChild) {
        auto inner = new BTreeInner<Key>();
        inner->count = 1;
        inner->keys[0] = k;
        inner->children[0] = leftChild;
        inner->children[1] = rightChild;
        root = inner;
    }
    
    void yield(int count) {
        if (count > 3)
            sched_yield();
        // else
        //     _mm_pause();
    }

    void put(const Key k, Value v, const bool linear_search = false, const bool dynamic = true) {
        restart:
        if(!code_writing){
            this->write_num++;
            if(code_writing){
                this->write_num--;
                goto restart;
            }
            this->put_data(k,v);
            if(!code_generating && dynamic && this->size > 2*code_tree_size && this->insert_tree){
                this->gen_machine_code(linear_search);
            }
            this->write_num--;
        } else {
            this->insert_write_num++;
            if(insert_writing || !this->insert_tree){
                this->insert_write_num--;
                goto restart;
            }
            this->insert_tree->put(k, v, linear_search, false);
            this->insert_write_num--;
        }
        
    }

    void put_data(const Key k, Value v) {
        int restartCount = 0;
    restart:
        if (restartCount++)
            yield(restartCount);
        bool needRestart = false;

        // Current node
        NodeBase* node = root;
        uint64_t versionNode = node->readLockOrRestart(needRestart);
        if (needRestart || (node != root)) goto restart;

        // Parent of current node
        BTreeInner<Key>* parent = nullptr;
        uint64_t versionParent;

        while (node->type == PageType::BTreeInner) {
            auto inner = static_cast<BTreeInner<Key>*>(node);

            // Split eagerly if full
            if (inner->isFull()) {
                // Lock
                if (parent) {
                    parent->upgradeToWriteLockOrRestart(versionParent, needRestart);
                    if (needRestart) goto restart;
                }
                node->upgradeToWriteLockOrRestart(versionNode, needRestart);
                if (needRestart) {
                    if (parent)
                        parent->writeUnlock();
                    goto restart;
                }
                if (!parent && (node != root)) {  // there's a new parent
                    node->writeUnlock();
                    goto restart;
                }
                // Split
                Key sep;
                BTreeInner<Key>* newInner = inner->split(sep);
                if (parent)
                    parent->insert(sep, newInner);
                else
                    makeRoot(sep, inner, newInner);
                // Unlock and restart
                node->writeUnlock();
                if (parent)
                    parent->writeUnlock();
                goto restart;
            }

            if (parent) {
                parent->readUnlockOrRestart(versionParent, needRestart);
                if (needRestart) goto restart;
            }

            parent = inner;
            versionParent = versionNode;

            node = inner->children[inner->lowerBound(k)];
            inner->checkOrRestart(versionNode, needRestart);
            if (needRestart) goto restart;
            versionNode = node->readLockOrRestart(needRestart);
            if (needRestart) goto restart;
        }

        auto leaf = static_cast<BTreeLeaf<Key, Value>*>(node);

        // Split leaf if full
        if (leaf->count == leaf->maxEntries) {
            // Lock
            if (parent) {
                parent->upgradeToWriteLockOrRestart(versionParent, needRestart);
                if (needRestart) goto restart;
            }
            node->upgradeToWriteLockOrRestart(versionNode, needRestart);
            if (needRestart) {
                if (parent) parent->writeUnlock();
                goto restart;
            }
            if (!parent && (node != root)) {  // there's a new parent
                node->writeUnlock();
                goto restart;
            }
            // Split
            Key sep;
            BTreeLeaf<Key, Value>* newLeaf = leaf->split(sep);
            if (parent)
                parent->insert(sep, newLeaf);
            else
                makeRoot(sep, leaf, newLeaf);
            // Unlock and restart
            node->writeUnlock();
            if (parent)
                parent->writeUnlock();
            goto restart;
        } else {
            // only lock leaf node
            node->upgradeToWriteLockOrRestart(versionNode, needRestart);
            if (needRestart) goto restart;
            if (parent) {
                parent->readUnlockOrRestart(versionParent, needRestart);
                if (needRestart) {
                    node->writeUnlock();
                    goto restart;
                }
            }
            leaf->insert(k, v);
            node->writeUnlock();

            this->size++;
            return;  // success
        }
    }

    std::optional<Value> get(const Key k, const bool linear_search = false) {
        restart:
        std::optional<Value> v;
        if(this->code && !code_writing){
            this->code_read_num++;
            if(code_writing){
                this->code_read_num--;
                goto restart;
            }
            unsigned char found = 15;
            v = this->get_code_func(k, found);
            this->code_read_num--;
            if(found == 1){
                if(v.has_value()){
                    return v;
                }
            } else if (found == 2){
                if(v.has_value()){
                    NodeBase* node = (NodeBase*) v.value();
                    v = this->get_data(k, linear_search, node);
                    if(v.has_value()){
                        return v;
                    }
                }
            }
        }
        v = this->get_data(k,linear_search);
        if(v.has_value()){
            return v;
        }
        if(this->insert_tree){
            this->insert_read_num++;
            if(insert_writing){
                this->insert_read_num--;
                goto restart;
            }
            v = this->insert_tree->get(k, linear_search);
            this->insert_read_num--;
            if(!v.has_value()){
                goto restart;
            }
        }
        if(v.has_value()){
            return v;
        }
        return std::nullopt;
    }

    std::optional<Value> get_data(const Key k, const bool linear_search = false, NodeBase* start_node = nullptr) {
        int restartCount = 0;
    restart:
        if (restartCount++)
            yield(restartCount);
        bool needRestart = false;

        NodeBase* node = root;
        if(start_node) node = start_node;
        uint64_t versionNode = node->readLockOrRestart(needRestart);
        if (needRestart || ((node != root) && !start_node)) goto restart;

        // Parent of current node
        BTreeInner<Key>* parent = nullptr;
        uint64_t versionParent = 0;

        while (node->type == PageType::BTreeInner) {
            auto inner = static_cast<BTreeInner<Key>*>(node);

            if (parent) {
                parent->readUnlockOrRestart(versionParent, needRestart);
                if (needRestart) goto restart;
            }

            parent = inner;
            versionParent = versionNode;
            
            // Increase the number of node accesses
            unsigned a;
            if (linear_search){
                a = inner->lowerBoundLinear(k);
            } else {
                a = inner->lowerBound(k);
            }
            node = inner->children[a];
            inner->checkOrRestart(versionNode, needRestart);
            if (needRestart) goto restart;
            versionNode = node->readLockOrRestart(needRestart);
            if (needRestart) goto restart;
        }

        BTreeLeaf<Key, Value>* leaf = static_cast<BTreeLeaf<Key, Value>*>(node);
        unsigned pos;
        if (linear_search) {
            pos = leaf->lowerBoundLinear(k);
        } else {
            pos = leaf->lowerBound(k);
        }
        std::optional<Value> result = std::nullopt;
        if ((pos < leaf->count) && (leaf->keys[pos] == k)) {
            result = leaf->payloads[pos];
        }
        if (parent) {
            parent->readUnlockOrRestart(versionParent, needRestart);
            if (needRestart) goto restart;
        }
        node->readUnlockOrRestart(versionNode, needRestart);
        if (needRestart) goto restart;
        
        // Increase the number of node accesses
        return result;
    }

    std::optional<Value> get_code(const Key k) {
        if(this->code){
            unsigned char found = false;
            std::optional<Value> v = this->get_code_func(k, found);
            if(found == 2){
                v = this->get_data(k, true, (NodeBase*) v.value());
            }
            return v;
        }
    }

    NodeBase *get_root() const {
        return root;
    }


    void set_root(NodeBase *new_root, bool delete_old_root = true){
        if (delete_old_root){
            delete root;
        }
        root = new_root;
    }

    void gen_machine_code(const bool linear_search = true){
        if(this->code_generation_lock.try_lock())
        {
            this->code_generating = true;
            this->code_writing = true;
            while(this->write_num > 1);
            while(this->code_read_num > 0);

            munmap(this->code, this->code_size);

            this->code = nullptr;

            this->gen_machine_code_x86(linear_search);

            this->code_tree_size = this->size;

            this->code_writing = false;

            this->insert_writing = true;
            while(this->insert_read_num > 0);
            while(this->insert_write_num > 0);

            this->move_insert_to_main();

            this->insert_writing = false;
            this->code_generating = false;
            this->code_generation_lock.unlock();
        }
    }

    void gen_machine_code_x86(const bool linear_search = true){

        this->code_size = sizeof(x86_full_bplus_ret);
        this->code_size += get_code_size_x86(linear_search);


        void* temp_code = mmap(0, this->code_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);
        if (temp_code == MAP_FAILED) {
            perror("Error mmapping the file");
            exit(EXIT_FAILURE);
        }

        this->code = (unsigned char*) temp_code;

        if (mprotect((void*)this->code, this->code_size, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
            perror("mprotect: ");
            while (1);
        }

        if(this->root){
            NodeBase* node = this->root;
            unsigned long pos = 0;

            unsigned long return_address_pos = this->code_size - sizeof(x86_full_bplus_ret);
            unsigned char* return_address = &code[return_address_pos];

            x86_full_bplus_ret bplus_ret;

            if (node->type == PageType::BTreeInner) {
                BTreeInner<Key>* inner = static_cast<BTreeInner<Key>*>(node);
                inner->to_machine_code_x86(code, linear_search, pos, return_address, this->max_code_depth, 0, inner->count);
                inner->place_code(bplus_ret, code, return_address_pos);
            } else {
                BTreeLeaf<Key, Value>* leaf = static_cast<BTreeLeaf<Key, Value>*>(node);
                leaf->to_machine_code_x86(code, linear_search, pos, return_address, 0, leaf->count);
                leaf->place_code(bplus_ret, code, return_address_pos);
            }
        }


        this->get_code_func = (std::optional<unsigned long> (*)(unsigned long, unsigned char&)) this->code;

    }

    void move_insert_to_main(){
        std::vector<std::pair<Key, Value>> insert_key_value;
        this->insert_tree->get_all_key_value_pairs(insert_key_value);

        for (std::pair<Key, Value> kv : insert_key_value){
            this->put_data(kv.first, kv.second);
        }
        
        delete this->insert_tree;

        this->insert_tree = new BTreeOLC<Key, Value>(false);
    }

    unsigned long get_code_size_x86(const bool linear_search){
        if(this->root){
            NodeBase* node = this->root;
            if (node->type == PageType::BTreeInner) {
                BTreeInner<Key>* inner = static_cast<BTreeInner<Key>*>(node);
                return inner->code_size_x86(linear_search, this->max_code_depth,0, inner->count);
            } else {
                BTreeLeaf<Key, Value>* leaf = static_cast<BTreeLeaf<Key, Value>*>(node);
                return leaf->code_size_x86(linear_search, 0, leaf->count);
            }
        }
    }

    void get_all_key_value_pairs(std::vector<std::pair<Key, Value>>& result, NodeBase* node = nullptr) {
        if (!node) node = root;
        if (node->type == PageType::BTreeInner) {
            BTreeInner<Key>* inner = static_cast<BTreeInner<Key>*>(node);
            for (size_t i = 0; i <= inner->count; ++i) {
                get_all_key_value_pairs(result, inner->children[i]);
            }
        } else {
            BTreeLeaf<Key, Value>* leaf = static_cast<BTreeLeaf<Key, Value>*>(node);
            for (size_t i = 0; i < leaf->count; ++i) {
                result.emplace_back(leaf->keys[i], leaf->payloads[i]);
            }
        }
    }
};