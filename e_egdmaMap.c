#include <coprthr.h>
#include <coprthr_mpi.h>
#if TIMEIT == EPIPHANY
#include "timer.h"
#endif // TIMEIT
#include "esyscall.h"
#include <host_stdio.h>

#include "egdma.h"

void __entry k_map(map_args * args)
{
#if TIMEIT == 4
    unsigned int clkStartTicks, waitStartTicks, clkStopTicks, waitStopTicks, totalClockTicks;
    unsigned int totalWaitTicks = 0;
    STARTCLOCK0(clkStartTicks);
#endif // TIMEIT
    unsigned int gid = coprthr_corenum();
    host_printf("%d: in Map\n", gid);
    int i;

    register uintptr_t sp_val;      /// Thanks jar
    __asm__ __volatile__(
       "mov %[sp_val], sp"
       : [sp_val] "=r" (sp_val)
    );

    /// how much space do we have
    unsigned int map[GREYLEVELS];                               /// local storage for the grey scale map
    uint8_t  map8;
    void * baseAddr = coprthr_tls_sbrk(0);                      /// begining of free space
    uintptr_t baseLowOrder = (int)baseAddr & 0x0000FFFF;
    unsigned int localSize = sp_val - baseLowOrder - 0x200;      /// amount of free space available in bytes   /// leave 512 bytes as a buffer
    unsigned int frameSizeBytes = localSize / 2;                /// bytes       /// a frame is the smallest processing chunk
    unsigned int frameSizeInts = frameSizeBytes / sizeof(int);  /// ints        /// the number of ints in a frame
    unsigned int * A = (int *)coprthr_tls_sbrk(frameSizeBytes);          /// 1st frame
    unsigned int * B = (int *)coprthr_tls_sbrk(frameSizeBytes);          /// 2nd frame
    unsigned int * inbound;                                     /// A or B
    unsigned int * outbound;                                       /// B or A
    int processingA;
    e_dma_desc_t dmaDescInbound, dmaDescOutbound;

    /// how much data do we have to process
    unsigned int imageSize = (args->height) * (args->width);
    unsigned int band = imageSize / ECORES;                     /// split the image up int 16 "bands" (or horizontal strips)
    if (gid == LASTCORENUM)                                     /// =========================== this needs checking ==========================================
        band += imageSize % ECORES;                             /// if it is not exactly divisible by 16 then add the remainder onto the workload for core 15
    unsigned int frames = band / frameSizeInts;
    unsigned int tailEndInts = band % frameSizeInts;
    unsigned int trxCount;                                      /// frames or tailEndCount

    /// where in the global buffer does it come from
    void * startLoc = args->g_greyVals + (gid * band * sizeof(int));

    /// debug
    //host_printf("%d\t\timagesize=%u\tband=%u\tspace=0x%x\tframeSizeBytes=%d\tframeSizeInts=%u\ttaileEnd=%u\tframes=%d\tstartloc=0x%x\tA=x0%x\tB=0x%x\n", gid, imageSize, band, localSize, frameSizeBytes, frameSizeInts, tailEndInts, frames, startLoc, A, B);

    e_dma_set_desc(E_DMA_1,                                     /// inbound channel on fast channel
                    E_DMA_WORD | E_DMA_ENABLE | E_DMA_MASTER,   /// config
                    0,                                          /// next descriptor (there isn't one)
                    sizeof(int),                                /// inner stride source
                    sizeof(int),                                /// inner stride destination
                    GREYLEVELS,                                 /// inner count of data items to transfer (one row)
                    1,                                          /// outer count (1 we are doing 1D dma)
                    0,                                          /// outer stride source N/A in 1D dma
                    0,                                          /// outer stride destination N/A in 1D dma
                    args->g_map,                                /// starting location source
                    (void*)map,                                 /// starting location destination
                    &dmaDescOutbound);                          /// dma descriptor for outbound traffic bu use it for transferring the map for now

    //host_printf("%d\tfrom 0x%x\tto 0x%x\n", gid, args->g_map, map);
    e_dma_start(&dmaDescOutbound, E_DMA_1); /// start the first inbound transfer
    e_dma_wait(E_DMA_1);

    inbound = A;        /// transfer first frame to A ready for processing
    processingA = -1;            /// and start processing it first

    while(frames--)
    {
        e_dma_set_desc(E_DMA_0,                                     /// inbound channel on fast channel
                        E_DMA_WORD | E_DMA_ENABLE | E_DMA_MASTER,   /// config
                        0,                                          /// next descriptor (there isn't one)
                        sizeof(int),                                /// inner stride source
                        sizeof(int),                                /// inner stride destination
                        frameSizeInts,                              /// inner count of data items to transfer (one row)
                        1,                                          /// outer count (1 we are doing 1D dma)
                        0,                                          /// outer stride source N/A in 1D dma
                        0,                                          /// outer stride destination N/A in 1D dma
                        startLoc,                                   /// starting location source
                        (void*)inbound,                    /// starting location destination
                        &dmaDescInbound);                                  /// dma descriptor for inbound traffic

        ///host_printf("%d\t%d\tto 0x%x\tfrom 0x%x\n", gid, trxCount, beingTransferred, startLoc);
        e_dma_start(&dmaDescInbound, E_DMA_0); /// start the first inbound transfer
        //host_printf("%d: %u inbound to %x\n", gid, frameSizeInts, inbound);

//#if TIMEIT == 4
//        STARTCLOCK1(waitStartTicks);  /// e_dma_wait does not idle - it is a wait loop
//#endif // TIMEIT
        e_dma_wait(E_DMA_0);
//#if TIMEIT == 4
//        STOPCLOCK1(waitStopTicks);
//        totalWaitTicks += (waitStartTicks - waitStopTicks);
//#endif // TIMEIT

        for(i=0; i<frameSizeInts; i++)          /// only the last frame will have tailEndInts to process and that is done below
            inbound[i] = map[inbound[i]];             /// replace the grey value in the image with it's mapped value

        if(processingA)
        {
            inbound = B;           /// transfer next frame to B
            outbound = A;
        }
        else
        {
            inbound = A;
            outbound = B;
        }

        e_dma_wait(E_DMA_1);            /// wait til the previous copy bakc has finished before starting the next one
        e_dma_set_desc(E_DMA_1,                                     /// inbound channel on fast channel
                        E_DMA_WORD | E_DMA_ENABLE | E_DMA_MASTER,   /// config
                        0,                                          /// next descriptor (there isn't one)
                        sizeof(int),                                /// inner stride source
                        sizeof(int),                                /// inner stride destination
                        frameSizeInts,                              /// inner count of data items to transfer (one row)
                        1,                                          /// outer count (1 we are doing 1D dma)
                        0,                                          /// outer stride source N/A in 1D dma
                        0,                                          /// outer stride destination N/A in 1D dma
                        outbound,                                   /// starting location source
                        startLoc,                                     /// starting location destination
                        &dmaDescOutbound);                          /// dma descriptor for outbound traffic bu use it for transferring the map for now

        e_dma_start(&dmaDescOutbound, E_DMA_1); /// start the first inbound transfer
        //host_printf("%d\t%u\t to 0x%x\tfrom 0x%x\n", gid, frameSizeInts, outbound, startLoc);

        startLoc += frameSizeBytes;     /// transfer the next frame

        processingA = !processingA;             /// swap buffers
    }

    if(tailEndInts)
    {
        e_dma_copy((void*)inbound, startLoc, tailEndInts * sizeof(int));        /// copy int the tail end values
        //host_printf("%d\t%u inbound to 0x%x", gid, tailEndInts, inbound);
        for(i=0; i<tailEndInts; i++)                                            /// scan the remaining data
            inbound[i] = map[inbound[i]];
        e_dma_copy(startLoc, (void*)inbound, tailEndInts * sizeof(int));        /// copy back the results using E_DMA_0 because it is faster and there is nothing else left to do
        //host_printf("%d\t%u outbound from 0x%x", gid, tailEndInts, inbound);
    }
    e_dma_wait(E_DMA_1);    /// make sure the last outbound transfer on DMA_1 is complete before exiting


tidyUpAndExit:
    /// tidy up
    coprthr_tls_brk(baseAddr);

#if TIMEIT == 4
    STOPCLOCK0(clkStopTicks);
    totalClockTicks = (clkStartTicks - clkStopTicks);
    host_printf("%d: Total Ticks: %u, working ticks: %u waiting ticks: %u (%0.2f%%).\n", gid, totalClockTicks, (totalClockTicks - totalWaitTicks), totalWaitTicks, ((float)(totalClockTicks - totalWaitTicks) / (float)totalClockTicks) * 100.0);
#endif // TIMEIT
}
