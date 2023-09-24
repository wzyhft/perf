#pragma once

#include "PerfCommon.h"
#include "FixedSizeSPSCQueue.h"
#include <cstring>
#include <iostream>
#include <sys/syscall.h>
#include <unistd.h>
#include <mutex>
#include <vector>

class RuntimePerf
{
public:
  using MsgHeader = TracePoint;

  static inline MsgHeader *reserveAlloc()
  {
    if (stagingBuffer == nullptr)
      perfSingleton.ensureStagingBufferAllocated();

    // NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage)
    return stagingBuffer->reserveProducerSpace();
  }

  static inline void finishAlloc() { stagingBuffer->finishReservation(); }

  static void preallocate() { perfSingleton.ensureStagingBufferAllocated(); }

  static void poll() { perfSingleton.poll_(); }

  static void setThreadName(const char *name)
  {
    perfSingleton.ensureStagingBufferAllocated();
    stagingBuffer->setName(name);
  }

private:
  // Forward Declarations
  class StagingBuffer;
  class StagingBufferDestroyer;

  // Storage for per thread tracepoints
  inline static __thread StagingBuffer *stagingBuffer = nullptr;

  // Destroys the __thread StagingBuffer upon its own destruction, which
  // is synchronized with thread death
  static thread_local StagingBufferDestroyer sbc;

  // Singleton RuntimePerf that manages the thread-local structures and
  // background output thread.
  static RuntimePerf perfSingleton;

  RuntimePerf();

  ~RuntimePerf();

  void poll_();

  /**
   * Allocates thread-local structures if they weren't already allocated.
   * This is used by the generated C++ code to ensure it has space to
   * store pedning tracepoints and by the user if they wish to
   * preallocate the data structures on thread creation.
   */
  inline void ensureStagingBufferAllocated()
  {
    if (stagingBuffer == nullptr)
    {
      std::unique_lock<std::mutex> guard(bufferMutex);
      // Unlocked for the expensive StagingBuffer allocation
      guard.unlock();
      stagingBuffer = new StagingBuffer();
      guard.lock();

      threadBuffers.push_back(stagingBuffer);
    }
  }

  // Globally the thread-local stagingBuffers
  std::vector<StagingBuffer *> threadBuffers;

  // Protects reads and writes to threadBuffers
  std::mutex bufferMutex;

  FILE *outputFp;

  //=========================

  class StagingBuffer
  {
  public:
    inline TracePoint *reserveProducerSpace() { return spscq.alloc(); }

    inline void finishReservation() { spscq.push(); }

    TracePoint *peek() { return spscq.front(); }

    inline void consume() { spscq.pop(); }

    void setName(const char *name_) { strncpy(name, name_, sizeof(name) - 1); }

    const char *getName() { return name; }

    /**
     * Returns true if it's safe for the compression thread to delete
     * the StagingBuffer and remove it from the global vector.
     *
     * \return
     *      true if its safe to delete the StagingBuffer
     */
    bool checkCanDelete() { return shouldDeallocate; }

    StagingBuffer()
        : spscq(), shouldDeallocate(false)
    {
      // Empty function, but causes the C++ runtime to instantiate the
      // sbc thread_local (see documentation in function).
      sbc.stagingBufferCreated();

      uint32_t tid = static_cast<pid_t>(::syscall(SYS_gettid));
      snprintf(name, sizeof(name), "%d", tid);
    }

    ~StagingBuffer() {}

  private:
    FixedSizeSPSCQueue<TracePoint, 4096> spscq;

    bool shouldDeallocate;

    char name[16] = {0};

    friend StagingBufferDestroyer;
  };

  // This class is intended to be instantiated as a C++ thread_local to
  // synchronize marking the thread local stagingBuffer for deletion with
  // thread death.
  //
  // The reason why this class exists rather than wrapping the stagingBuffer
  // in a unique_ptr or declaring the stagingBuffer itself to be thread_local
  // is because of performance. Dereferencing the former costs 10 ns and the
  // latter allocates large amounts of resources for every thread that is
  // created, which is wasteful for threads that do not use this.
  class StagingBufferDestroyer
  {
  public:
    explicit StagingBufferDestroyer() {}

    // Weird C++ hack; C++ thread_local are instantiated upon first use
    // thus the StagingBuffer has to invoke this function in order
    // to instantiate this object.
    void stagingBufferCreated() {}

    virtual ~StagingBufferDestroyer()
    {
      if (stagingBuffer != nullptr)
      {
        stagingBuffer->shouldDeallocate = true;
        stagingBuffer = nullptr;
      }
    }
  };

}; // RuntimePerf
