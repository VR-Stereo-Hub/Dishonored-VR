// Fixed, bounded command ownership transport. Slots never overwrite a reader.
#pragma once
#include "core/gfx/hud_owner.h"
#include <atomic>
#include <cstddef>
namespace dvr::hudowner {
template<size_t N=16384> class CommandOwners {
    static_assert((N&(N-1))==0 && N>=8,"power of two capacity");
    struct Slot { std::atomic<uintptr_t> key{0}; Owner owner; } slots[N];
    std::atomic<unsigned> pendingCount{0};
    static constexpr uintptr_t busy=1;
    static size_t bucket(uintptr_t key) { return ((key>>2)*2654435761u)&(N-1); }
public:
    bool pending() const {return pendingCount.load(std::memory_order_relaxed)!=0;}
    bool put(uintptr_t command,const Owner& owner) {
        if(command<=busy || !owner) return false;
        for(size_t n=0;n<8;++n) {
            auto& s=slots[(bucket(command)+n)&(N-1)];
            if(s.key.load(std::memory_order_relaxed)!=command) continue;
            uintptr_t duplicate=command;
            if(s.key.compare_exchange_strong(duplicate,busy,std::memory_order_acquire)) {
                pendingCount.fetch_sub(1,std::memory_order_relaxed);
                s.key.store(0,std::memory_order_release);return false;
            }
        }
        for(size_t n=0;n<8;++n) {
            auto& s=slots[(bucket(command)+n)&(N-1)];
            if(s.key.load(std::memory_order_relaxed)!=0) continue;
            uintptr_t empty=0;
            if(!s.key.compare_exchange_strong(empty,busy,std::memory_order_acquire)) continue;
            s.owner=owner;pendingCount.fetch_add(1,std::memory_order_relaxed);
            s.key.store(command,std::memory_order_release);return true;
        }
        return false; // Native fallback, never overwrite another queued owner.
    }
    Owner take(uintptr_t command,uint32_t currentGeneration) {
        if(command<=busy || !pending()) return {};
        for(size_t n=0;n<8;++n) {
            auto& s=slots[(bucket(command)+n)&(N-1)];
            if(s.key.load(std::memory_order_relaxed)!=command) continue;
            uintptr_t expected=command;
            if(!s.key.compare_exchange_strong(expected,busy,std::memory_order_acquire)) continue;
            const Owner value=s.owner;pendingCount.fetch_sub(1,std::memory_order_relaxed);
            s.key.store(0,std::memory_order_release);
            return value.generation==currentGeneration ? value : Owner{};
        }
        return {};
    }
};
}
