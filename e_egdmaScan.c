#include <coprthr.h>
#include <coprthr_mpi.h>
#include "timer.h"
#include "esyscall.h"
#include <host_stdio.h>

#include "egdma.h"

uint8_t * bebug;

#define UseDMA
#ifdef UseDMA
int epip_callback(int coreId, int something);
unsigned localRow, localCol;
e_mutex_t mtx;

void __attribute__((interrupt)) int_isr()
{
    host_printf("Unlocking on: %i \(%u, %u\)\n", coprthr_corenum(), localRow, localCol);
    e_mutex_unlock(localRow, localCol, &mtx);
}

void __entry k_scan(scan_args * args)
{
#ifdef TIMEEPIP
    unsigned int clkStartTicks, waitStartTicks, clkStopTicks, waitStopTicks, totalClockTicks;
    unsigned int totalWaitTicks = 0;
    STARTCLOCK0(clkStartTicks);
#endif // TIMEIT

    unsigned int gid = coprthr_corenum();
    unsigned int grayDistribution[GRAYLEVELS] = { 0 };   /// all elements set to 0
    unsigned int i;
    uint8_t debug;

    e_coreid_t coreId = e_get_coreid();
    e_coords_from_coreid(coreId, &localRow, &localCol);
    e_mutex_init(localRow, localCol, &mtx, NULL);


    register uintptr_t sp_val;      /// Thanks jar
    __asm__ __volatile__(
       "mov %[sp_val], sp"
       : [sp_val] "=r" (sp_val)
    );

    /// how much space do we have
    void * baseAddr = coprthr_tls_sbrk(0);                      /// begining of free space
    uintptr_t baseLowOrder = (int)baseAddr & 0x0000FFFF;
    unsigned int localSize = sp_val - baseLowOrder - 0x200;     /// amount of free space available in bytes   /// leave 512 bytes as a buffer
//    unsigned int localSize = 0x2000;
    unsigned int workArea = (localSize / 2);                    /// split the avalable memory into two chunks
                 workArea -= (workArea % 8);                    /// and make them divisible by 8
    uint8_t * A = (int *)coprthr_tls_sbrk(workArea);            /// 1st chunks
    uint8_t * B = (int *)coprthr_tls_sbrk(workArea);            /// 2nd chunks
    uint8_t * beingTransferred;                                 /// A or B
    uint8_t * beingProcessed;                                   /// B or A
    e_dma_id_t  currentChannel = E_DMA_0;                                 /// E_DMA_0 or E_DMA_1  --  just use DMA_0 as a test
    int processingA = (gid % 2);                                /// odd numbered cores start with frame A while transferring data into B; odd numbered cores start on B
    e_dma_desc_t dmaDesc;

//    int COP2_TRX_FLAGS;


    /// how much data do we have to process
    unsigned int imageSize = args->szImageBuffer;
    unsigned int band = imageSize / ECORES;                     /// split the image up int 16 "bands" (or horizontal strips)
    if (gid == LASTCORENUM)                                     /// =========================== this needs checking ==========================================
        band += imageSize % ECORES;                             /// if it is not exactly divisible by 16 then add the remainder onto the workload for core 15
    unsigned int workUnits = band / workArea;
    unsigned int tailEnds = band % workArea;
    unsigned int trxCount;                                      /// the number of DWORDS to transfer every DMA call

    /// where in the global buffer does it come from
    void * startLoc = args->g_grayVals + (gid * band * sizeof(uint8_t));

    if(processingA)
    {
        beingTransferred = A;                                   /// transfer first frame to A ready for processing
// testing        currentChannel = E_DMA_0;                               /// on E_DMA_0
    }
    else
    {
        beingTransferred = B;
// testing        currentChannel = E_DMA_1;
    }

    if(workUnits)
        trxCount = workArea / 8;                                /// there are 8 values per DWORD
    else
        trxCount = tailEnds / 8;

//host_printf("Core:%d, localSize: %u, workArea: %u, imageSize: %u, band: %u, wornUnits: %u, tailEnds; %u, startLoc: 0x%x, trxCount: %u, 0x%x, A: 0x%x, B:0x%x\n", gid, localSize, workArea, imageSize, band, workUnits, tailEnds, startLoc, trxCount, dmaDesc.count, A, B);
    /// Set up the interrupt handlers
//    e_irq_attach(E_DMA1_INT, int_isr);
//    e_irq_mask(E_DMA1_INT, E_FALSE);
    e_irq_attach(E_DMA0_INT, int_isr);
    e_irq_mask(E_DMA0_INT, E_FALSE);
    e_irq_global_mask(E_FALSE);

    e_dma_set_desc(currentChannel,                              /// channel
                    E_DMA_DWORD | E_DMA_ENABLE | E_DMA_MASTER | E_DMA_IRQEN,  /// config
                    0x0,                                        /// next descriptor (there isn't one)
                    8,                                          /// inner stride source (sizeof(DWORD))
                    8,                                          /// inner stride destination
                    trxCount,                                   /// inner count of data items to transfer (one row)
                    1,                                          /// outer count (1 we are doing 1D dma)
                    0,                                          /// outer stride source N/A in 1D dma
                    0,                                          /// outer stride destination N/A in 1D dma
                    startLoc,                                   /// starting location source
                    (void*)beingTransferred,                    /// starting location destination
                    &dmaDesc);                                  /// dma descriptor

bebug = beingTransferred;
//    e_dma_start(&dmaDesc, currentChannel);
//phalt();
    bebug = beingTransferred;
    memcpy(beingTransferred, startLoc, workArea);   /// copy the first block

///phalt();

    while(workUnits--)
    {
//host_printf("Core:%d, iteration: %u, startLoc: 0x%x, src: 0x%x, dst: 0x%x\n", gid, workUnits+1, startLoc, dmaDesc.src_addr, dmaDesc.dst_addr);
#ifdef TIMEEPIP
        STARTCLOCK1(waitStartTicks);
#endif // TIMEIT
        /// wait for the current transfer to complete
//        e_dma_wait(currentChannel);                           /// e_dma_wait does not idle - it is a wait loop
        e_mutex_lock(localRow, localCol, &mtx);

#ifdef TIMEEPIP
        STOPCLOCK1(waitStopTicks);
        totalWaitTicks += (waitStartTicks - waitStopTicks);
#endif // TIMEIT
//bebug = beingTransferred;
//phalt();
//host_printf("Core:%d first 8, last two: %u, %u, %u, %u, %u, %u, %u, %u, %u, %u\n", gid, beingTransferred[0], beingTransferred[1], beingTransferred[2], beingTransferred[3], beingTransferred[4], beingTransferred[5], beingTransferred[6], beingTransferred[7], beingTransferred[workArea - 2], beingTransferred[workArea - 1]);

        if(processingA)
        {
            beingTransferred = B;                               /// transfer next frame to B
// testing            currentChannel = E_DMA_1;                           /// on E_DMA_1
            beingProcessed = A;
        }
        else
        {
            beingTransferred = A;
// testing            currentChannel = E_DMA_0;
            beingProcessed = B;
        }

        if(workUnits == 0)                                      /// i.e. there are  more full frames to transfer
            trxCount = tailEnds / 8;                            /// there is only the tail end left

        startLoc += workArea;                                   /// transfer the next frame

        /// Warning: channel should be idle before making changes to the descriptor
        dmaDesc.count = 0x00010000 | trxCount;                  /// outer loop count = 1 (high order) inner loop count = trxCount (low order)
        dmaDesc.src_addr = startLoc;
        dmaDesc.dst_addr = (void*)beingTransferred;

        e_dma_start(&dmaDesc, currentChannel);

        for(i=0; i<workArea; i++)                               /// only the last frame will have tailEndInts to process and that is done below
            ++grayDistribution[beingProcessed[i]];

        processingA = !processingA;                             /// swap buffers and channels
    }
host_printf("Core:%d, tailends: %u\n", gid, tailEnds);

#ifdef TIMEEPIP
        STARTCLOCK1(waitStartTicks);                            /// e_dma_wait does not idle - it is a wait loop
#endif // TIMEIT
        /// wait for the last transfer to complete
//        e_dma_wait(currentChannel);
        e_mutex_lock(localRow, localCol, &mtx);
#ifdef TIMEEPIP
        STOPCLOCK1(waitStopTicks);
        totalWaitTicks += (waitStartTicks - waitStopTicks);
#endif // TIMEIT
    if(tailEnds)
    {
        for(i=0; i<tailEnds; i++)                               /// scan the remaining data
            ++grayDistribution[beingTransferred[i]];            /// the last transferred frame
    }

#ifdef TIMEEPIP
        STARTCLOCK1(waitStartTicks);
#endif // TIMEIT
    /// write back the results synchronously because there is nothing else to do
    e_dma_copy((args->g_result) + (gid * GRAYLEVELS * sizeof(int)), (void*)grayDistribution, GRAYLEVELS * sizeof(int));
#ifdef TIMEEPIP
        STOPCLOCK1(waitStopTicks);
        totalWaitTicks += (waitStartTicks - waitStopTicks);
#endif // TIMEIT

tidyUpAndExit:
    /// tidy up
    e_mutex_unlock(localRow, localCol, &mtx);
    coprthr_tls_brk(baseAddr);
    /// put back the interrupt masks where the came from
    e_irq_mask(E_DMA0_INT, E_TRUE);
//    e_irq_mask(E_DMA1_INT, E_TRUE);
    e_irq_global_mask(E_TRUE);



#ifdef TIMEEPIP
    STOPCLOCK0(clkStopTicks);
    totalClockTicks = (clkStartTicks - clkStopTicks);
    host_printf("Scan\tDMA\t%d\tTotal Ticks:\t%u\tworking ticks:\t%u\twaiting ticks:\t%u\t(%0.2f%%)\n", gid, totalClockTicks, (totalClockTicks - totalWaitTicks), totalWaitTicks, ((float)totalWaitTicks) / ((float)totalClockTicks) * 100.0);
#endif // TIMEIT
}
#else
void __entry k_scan(scan_args * args)
{
#ifdef TIMEEPIP
    unsigned int clkStartTicks, waitStartTicks, clkStopTicks, waitStopTicks, totalClockTicks;
    unsigned int totalWaitTicks = 0;
    STARTCLOCK0(clkStartTicks);
#endif // TIMEIT

    unsigned int gid = coprthr_corenum();
    unsigned int grayDistribution[GRAYLEVELS] = { 0 };   /// all elements set to 0
    unsigned int i;
    uint8_t debug;

    register uintptr_t sp_val;      /// Thanks jar
    __asm__ __volatile__(
       "mov %[sp_val], sp"
       : [sp_val] "=r" (sp_val)
    );

    /// how much space do we have
    void * baseAddr = coprthr_tls_sbrk(0);                      /// begining of free space
    uintptr_t baseLowOrder = (int)baseAddr & 0x0000FFFF;
    unsigned int localSize = sp_val - baseLowOrder - 0x200;      /// amount of free space available in bytes   /// leave 512 bytes as a buffer
    uint8_t * A = (int *)coprthr_tls_sbrk(localSize);          /// 1st chunks

    /// how much data do we have to process
    unsigned int imageSize = args->szImageBuffer;
    unsigned int band = imageSize / ECORES;                     /// split the image up int 16 "bands" (or horizontal strips)
    if (gid == LASTCORENUM)                                     /// =========================== this needs checking ==========================================
        band += imageSize % ECORES;                             /// if it is not exactly divisible by 16 then add the remainder onto the workload for core 15
    unsigned int workUnits = band / localSize;
    unsigned int tailEnds = band % localSize;

    /// where in the global buffer does it come from
    void * startLoc = args->g_grayVals + (gid * band * sizeof(uint8_t));


    while(workUnits--)
    {
#ifdef TIMEEPIP
        STARTCLOCK1(waitStartTicks);  /// e_dma_wait does not idle - it is a wait loop
#endif // TIMEIT
        memcpy(A, startLoc, localSize);
#ifdef TIMEEPIP
        STOPCLOCK1(waitStopTicks);
        totalWaitTicks += (waitStartTicks - waitStopTicks);
#endif // TIMEIT

        startLoc += localSize;           /// transfer the next frame

        for(i=0; i<localSize; i++)          /// only the last frame will have tailEndInts to process and that is done below
            ++grayDistribution[A[i]];

    }

    if(tailEnds)
    {
        memcpy(A, startLoc, tailEnds);                         /// wait for the last transfer to complete
        for(i=0; i<tailEnds; i++)                           /// scan the remaining data
            ++grayDistribution[A[i]];        /// the last transferred frame
    }

    /// write back the results synchronously because there is nothing else to do
    memcpy((args->g_result) + (gid * GRAYLEVELS * sizeof(int)), (void*)grayDistribution, GRAYLEVELS * sizeof(int));

tidyUpAndExit:
    /// tidy up
    coprthr_tls_brk(baseAddr);

#ifdef TIMEEPIP
    STOPCLOCK0(clkStopTicks);
    totalClockTicks = (clkStartTicks - clkStopTicks);
    host_printf("Scan\tmemcpy\t%d\tTotal Ticks:\t%u\tworking ticks:\t%u\twaiting ticks:\t%u\t(%0.2f%%)\n", gid, totalClockTicks, (totalClockTicks - totalWaitTicks), totalWaitTicks, ((float)totalWaitTicks) / ((float)totalClockTicks) * 100.0);
#endif // TIMEIT
}

#endif
