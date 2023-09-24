#include "RuntimePerf.h"

// Define the static members of RuntimePerf here
thread_local RuntimePerf::StagingBufferDestroyer RuntimePerf::sbc;
RuntimePerf RuntimePerf::perfSingleton;

RuntimePerf::RuntimePerf()
    : threadBuffers(), bufferMutex(), outputFp(nullptr) {}

RuntimePerf::~RuntimePerf()
{
    if (outputFp)
    {
        fclose(outputFp);
        outputFp = nullptr;
    }
}

void RuntimePerf::poll_()
{
    std::unique_lock<std::mutex> lock(bufferMutex);
    for (size_t i = 0; i < threadBuffers.size(); i++)
    {
        StagingBuffer *sb = threadBuffers[i];
        while (true)
        {
            auto header = sb->peek();
            if (!header)
            {
                // If there's no work, check if we're supposed to delete
                // the stagingBuffer
                if (sb->checkCanDelete())
                {
                    delete sb;
                    std::swap(threadBuffers[i], threadBuffers.back());
                    threadBuffers.pop_back();
                    i--;
                }
                break;
            }

            std::cout << "[PERF] id:" << header->id << ", start:" << header->start << ", end:" << header->end << ", diff:" << header->end - header->start << std::endl;

            if (outputFp)
            {
                fprintf(outputFp, "%ld %ld %ld\n", header->id, header->start, header->end);
            }

            sb->consume();
        }
    }

    if (outputFp)
    {
        fflush(outputFp);
    }
}