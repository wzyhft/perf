#pragma once
#include <atomic>

template <class T, uint32_t CNT>
class FixedSizeSPSCQueue
{
public:
    // todo: round to power of 2
    static_assert(CNT && !(CNT & (CNT - 1)), "CNT must be a power of 2");

    T *alloc()
    {
        if (free_write_cnt == 0)
        {
            uint32_t rd_idx = ((std::atomic<uint32_t> *)&read_idx)->load(std::memory_order_consume);
            free_write_cnt = (rd_idx - write_idx + CNT - 1) % CNT;
            if (free_write_cnt == 0) [[unlikely]]
                return nullptr;
        }
        return &blk[write_idx].data;
    }

    void push()
    {
        ((std::atomic<bool> *)&blk[write_idx].available)->store(true, std::memory_order_release);
        write_idx = (write_idx + 1) % CNT;
        free_write_cnt--;
    }

    template <typename Writer>
    bool tryPush(Writer writer)
    {
        T *p = alloc();
        if (!p)
            return false;
        writer(p);
        push();
        return true;
    }

    template <typename Writer>
    void blockPush(Writer writer)
    {
        while (!tryPush(writer))
            ;
    }

    T *front()
    {
        auto &cur_blk = blk[read_idx];
        if (!((std::atomic<bool> *)&cur_blk.available)->load(std::memory_order_acquire))
            return nullptr;
        return &cur_blk.data;
    }

    void pop()
    {
        blk[read_idx].available = false;
        ((std::atomic<uint32_t> *)&read_idx)->store((read_idx + 1) % CNT, std::memory_order_release);
    }

    template <typename Reader>
    bool tryPop(Reader reader)
    {
        T *v = front();
        if (!v)
            return false;
        reader(v);
        pop();
        return true;
    }

private:
    struct alignas(64) Block
    {
        // available will be updated by both write and read thread
        bool available = false;
        T data;
    } blk[CNT] = {};

    // used only by writing thread
    alignas(128) uint32_t write_idx = 0;
    uint32_t free_write_cnt = CNT - 1;

    alignas(128) uint32_t read_idx = 0;
};