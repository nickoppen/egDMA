#include <coprthr.h>
#include <coprthr_mpi.h>
#if TIMEIT == EPIPHANY
#include "timer.h"
#endif // TIMEIT
#include "esyscall.h"
#include <host_stdio.h>

#include "egdma.h"
#define UseDMA

#ifdef UseDMA
void __entry k_map(map_args * args)
{
#ifdef TIMEEPIP
    unsigned int clkStartTicks, waitStartTicks, clkStopTicks, waitStopTicks, totalClockTicks;
    unsigned int totalWaitTicks = 0;
    STARTCLOCK0(clkStartTicks);
#endif // TIMEIT
    unsigned int gid = coprthr_corenum();
    unsigned int i;

    register uintptr_t sp_val;      /// Thanks jar
    __asm__ __volatile__(
       "mov %[sp_val], sp"
       : [sp_val] "=r" (sp_val)
    );

    /// how much space do we have
    uint8_t map[GRAYLEVELS];                                    /// local storage for the grey scale map
    void * baseAddr = coprthr_tls_sbrk(0);                      /// begining of free space
    uintptr_t baseLowOrder = (int)baseAddr & 0x0000FFFF;
    unsigned int localSize = sp_val - baseLowOrder - 0x200;      /// amount of free space available in bytes   /// leave 512 bytes as a buffer
//    unsigned int localSize = 0x2000;
    unsigned int workArea = localSize / 2;                      /// divide the available space into 2 work areas A and B
                 workArea -= (workArea % 8);                    /// and make them divisible by 8
    uint8_t * A = (int *)coprthr_tls_sbrk(workArea);            /// 1st work area
    uint8_t * B = (int *)coprthr_tls_sbrk(workArea);            /// 2nd work area
    uint8_t * inbound;                                          /// A or B
    uint8_t * outbound;                                         /// B or A
    int processingA;
    e_dma_desc_t dmaDescInbound, dmaDescOutbound;

    /// how much data do we have to process
    unsigned int imageSize = args->szImageBuffer;
    unsigned int band = imageSize / ECORES;                     /// split the image up int 16 "bands" (or horizontal strips)
    if (gid == LASTCORENUM)                                     /// =========================== this needs checking ==========================================
        band += imageSize % ECORES;                             /// if it is not exactly divisible by 16 then add the remainder onto the workload for core 15
    unsigned int workUnits = band / workArea;
    unsigned int tailEnds = band % workArea;
    unsigned int trxCount;                                      /// frames or tailEndCount

    /// where in the global buffer does it come from
    void * startLoc = args->g_grayVals + (gid * band * sizeof(uint8_t));

///    read in the map using memcpy (probably faster for a small amount of data)
    memcpy(map, args->g_map, GRAYLEVELS);

    /// set up the DMA dexcriptors once and then only change the destination in the loop
    e_dma_set_desc(E_DMA_1,                                     /// outbound data channel on the interuptable channel
                    E_DMA_DWORD | E_DMA_ENABLE | E_DMA_MASTER,  /// config
                    0x0,                                        /// next descriptor (there isn't one)
                    8,                                          /// inner stride source
                    8,                                          /// inner stride destination
                    workArea / 8,                               /// inner count of data items to transfer (one row)
                    1,                                          /// outer count (1 we are doing 1D dma)
                    0,                                          /// outer stride source N/A in 1D dma
                    0,                                          /// outer stride destination N/A in 1D dma
                    0x0,                                        /// starting location source will change every iteration
                    0x0,                                        /// starting location destination will also change on every iteration
                    &dmaDescOutbound);                          /// dma descriptor for outbound traffic bu use it for transferring the map for now

    e_dma_set_desc(E_DMA_0,                                     /// inbound channel
                    E_DMA_DWORD | E_DMA_ENABLE | E_DMA_MASTER,
                    0x0,
                    8,
                    8,
                    workArea / 8,
                    1,
                    0,
                    0,
                    0x0,
                    0x0,
                    &dmaDescInbound);                             /// dma descriptor for inbound traffic


    inbound = A;        /// transfer first frame to A ready for processing
    processingA = -1;            /// and start processing it first

    while(workUnits--)
    {
        dmaDescInbound.src_addr = startLoc;
        dmaDescInbound.dst_addr = (void*)inbound;

        e_dma_start(&dmaDescInbound, E_DMA_0); /// start the first inbound transfer

#ifdef TIMEEPIP
        STARTCLOCK1(waitStartTicks);  /// e_dma_wait does not idle - it is a wait loop
#endif // TIMEIT
        e_dma_wait(E_DMA_0);
#ifdef TIMEEPIP
        STOPCLOCK1(waitStopTicks);
        totalWaitTicks += (waitStartTicks - waitStopTicks);
#endif // TIMEIT

        for(i=0; i<workArea; i++)          /// only the last frame will have tailEndInts to process and that is done below
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

        dmaDescOutbound.src_addr = outbound;
        dmaDescOutbound.dst_addr = startLoc;
        e_dma_start(&dmaDescOutbound, E_DMA_1); /// start the first inbound transfer

        startLoc += workArea;     /// transfer the next frame

        processingA = !processingA;             /// swap buffers
    }

    if(tailEnds)
    {
#ifdef TIMEEPIP
        STARTCLOCK1(waitStartTicks);  /// e_dma_wait does not idle - it is a wait loop
#endif // TIMEIT
        e_dma_copy((void*)inbound, startLoc, tailEnds * sizeof(uint8_t));        /// copy int the tail end values
#ifdef TIMEEPIP
        STOPCLOCK1(waitStopTicks);
        totalWaitTicks += (waitStartTicks - waitStopTicks);
#endif // TIMEIT
//        host_printf("%d\t%u inbound to 0x%x\n", gid, tailEnds, inbound);
        for(i=0; i<tailEnds; i++)                                            /// scan the remaining data
            inbound[i] = map[inbound[i]];
#ifdef TIMEEPIP
        STARTCLOCK1(waitStartTicks);  /// e_dma_wait does not idle - it is a wait loop
#endif // TIMEIT
        e_dma_copy(startLoc, (void*)inbound, tailEnds * sizeof(uint8_t));        /// copy back the results using E_DMA_0 because it is faster and there is nothing else left to do
#ifdef TIMEEPIP
        STOPCLOCK1(waitStopTicks);
        totalWaitTicks += (waitStartTicks - waitStopTicks);
#endif // TIMEIT
//        host_printf("%d\t%u outbound from 0x%x\n", gid, tailEnds, inbound);
    }
#ifdef TIMEEPIP
        STARTCLOCK1(waitStartTicks);  /// e_dma_wait does not idle - it is a wait loop
#endif // TIMEIT
    e_dma_wait(E_DMA_1);    /// make sure the last outbound transfer on DMA_1 is complete before exiting
#ifdef TIMEEPIP
        STOPCLOCK1(waitStopTicks);
        totalWaitTicks += (waitStartTicks - waitStopTicks);
#endif // TIMEIT

//    printf("%d: exiting\n", gid);
tidyUpAndExit:
    /// tidy up
    coprthr_tls_brk(baseAddr);

#ifdef TIMEEPIP
    STOPCLOCK0(clkStopTicks);
    totalClockTicks = (clkStartTicks - clkStopTicks);
    host_printf("Map\tDMA\t%d\tTotal Ticks:\t%u\tworking ticks:\t%u\twaiting ticks:\t%u\t(%0.2f%%)\n", gid, totalClockTicks, (totalClockTicks - totalWaitTicks), totalWaitTicks, ((float)totalWaitTicks) / ((float)totalClockTicks) * 100.0);
#endif // TIMEIT
}

#else
void __entry k_map(map_args * args)
{
#ifdef TIMEEPIP
    unsigned int clkStartTicks, waitStartTicks, clkStopTicks, waitStopTicks, totalClockTicks;
    unsigned int totalWaitTicks = 0;
    STARTCLOCK0(clkStartTicks);
#endif // TIMEIT
    unsigned int gid = coprthr_corenum();
    unsigned int i;

    register uintptr_t sp_val;      /// Thanks jar
    __asm__ __volatile__(
       "mov %[sp_val], sp"
       : [sp_val] "=r" (sp_val)
    );

    /// how much space do we have
    uint8_t map[GRAYLEVELS];                               /// local storage for the grey scale map
    void * baseAddr = coprthr_tls_sbrk(0);                      /// begining of free space
    uintptr_t baseLowOrder = (int)baseAddr & 0x0000FFFF;
    unsigned int localSize = sp_val - baseLowOrder - 0x200;      /// amount of free space available in bytes   /// leave 512 bytes as a buffer
//    unsigned int localSize = 0x4000;
    uint8_t * A = (int *)coprthr_tls_sbrk(localSize);          /// 1st work area

    /// how much data do we have to process
    unsigned int imageSize = args->szImageBuffer;
    unsigned int band = imageSize / ECORES;                     /// split the image up int 16 "bands" (or horizontal strips)
    if (gid == LASTCORENUM)                                     /// =========================== this needs checking ==========================================
        band += imageSize % ECORES;                             /// if it is not exactly divisible by 16 then add the remainder onto the workload for core 15
    unsigned int workUnits = band / localSize;
    unsigned int tailEnds = band % localSize;

    /// where in the global buffer does it come from
    void * startLoc = args->g_grayVals + (gid * band * sizeof(uint8_t));

///    read in the map using memcpy (probably faster for a small amount of data)
    memcpy(map, args->g_map, GRAYLEVELS);

//host_printf("%d: localSize: 0x%x workUnits: %u tailEnds: %u\n", gid, localSize, workUnits, tailEnds);
    while(workUnits--)
    {
//host_printf("%d: startLoc: 0x%x workUnits: %u\n", gid, startLoc, workUnits);

#ifdef TIMEEPIP
        STARTCLOCK1(waitStartTicks);  /// e_dma_wait does not idle - it is a wait loop
#endif // TIMEIT
        memcpy(A, startLoc, localSize);
#ifdef TIMEEPIP
        STOPCLOCK1(waitStopTicks);
        totalWaitTicks += (waitStartTicks - waitStopTicks);
#endif // TIMEIT

        for(i=0; i<localSize; i++)          /// only the last frame will have tailEndInts to process and that is done below
            A[i] = map[A[i]];             /// replace the grey value in the image with it's mapped value

#ifdef TIMEEPIP
        STARTCLOCK1(waitStartTicks);  /// e_dma_wait does not idle - it is a wait loop
#endif // TIMEIT
        memcpy(startLoc, A, localSize);
#ifdef TIMEEPIP
        STOPCLOCK1(waitStopTicks);
        totalWaitTicks += (waitStartTicks - waitStopTicks);
#endif // TIMEIT

        startLoc += localSize;     /// transfer the next frame

    }
//host_printf("%d: startLoc: 0x%x tailEnds: %u\n", gid, startLoc, tailEnds);

    if(tailEnds)
    {
#ifdef TIMEEPIP
        STARTCLOCK1(waitStartTicks);  /// e_dma_wait does not idle - it is a wait loop
#endif // TIMEIT
        memcpy(A, startLoc, tailEnds);        /// copy int the tail end values
#ifdef TIMEEPIP
        STOPCLOCK1(waitStopTicks);
        totalWaitTicks += (waitStartTicks - waitStopTicks);
#endif // TIMEIT
        for(i=0; i<tailEnds; i++)                                            /// scan the remaining data
            A[i] = map[A[i]];
#ifdef TIMEEPIP
        STARTCLOCK1(waitStartTicks);
#endif // TIMEIT
        memcpy(startLoc, A, tailEnds);
#ifdef TIMEEPIP
        STOPCLOCK1(waitStopTicks);
        totalWaitTicks += (waitStartTicks - waitStopTicks);
#endif // TIMEIT
    }

tidyUpAndExit:
    /// tidy up
    coprthr_tls_brk(baseAddr);

#ifdef TIMEEPIP
    STOPCLOCK0(clkStopTicks);
    totalClockTicks = (clkStartTicks - clkStopTicks);
    host_printf("Map\tmemcpy\t%d\tTotal Ticks:\t%u\tworking ticks:\t%u\twaiting ticks:\t%u\t(%0.2f%%)\n", gid, totalClockTicks, (totalClockTicks - totalWaitTicks), totalWaitTicks, ((float)totalWaitTicks) / ((float)totalClockTicks) * 100.0);
#endif // TIMEIT
}
#endif  // UseDMA
