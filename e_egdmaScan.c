#include <coprthr.h>
#include <coprthr_mpi.h>
#if TIMEIT == EPIPHANY
#include "timer.h"
#endif // TIMEIT
#include "esyscall.h"
#include <host_stdio.h>

#include "egdma.h"

void __entry k_scan(scan_args * args)
{
#if TIMEIT == 2
    unsigned int clkStartTicks, waitStartTicks, clkStopTicks, waitStopTicks, totalClockTicks;
    unsigned int totalWaitTicks = 0;
    STARTCLOCK0(clkStartTicks);
#endif // TIMEIT

    unsigned int gid = coprthr_corenum();
    int greyDistribution[GREYLEVELS] = { 0 };   /// all elements set to 0
    int i;
    host_printf("%d: in Scan\n", gid);

    register uintptr_t sp_val;      /// Thanks jar
    __asm__ __volatile__(
       "mov %[sp_val], sp"
       : [sp_val] "=r" (sp_val)
    );

    /// how much space do we have
    void * baseAddr = coprthr_tls_sbrk(0);                      /// begining of free space
    uintptr_t baseLowOrder = (int)baseAddr & 0x0000FFFF;
    unsigned int localSize = sp_val - baseLowOrder - 0x200;      /// amount of free space available in bytes   /// leave 512 bytes as a buffer
    unsigned int workArea = (localSize / 2);                    /// split the avalable memory into two chunks
                 workArea -= (workArea % 8);                    /// and make them divisible by 8
    unsigned int * A = (int *)coprthr_tls_sbrk(workArea);          /// 1st chunks
    unsigned int * B = (int *)coprthr_tls_sbrk(workArea);          /// 2nd chunks
    unsigned int * beingTransferred;                                     /// A or B
    unsigned int * beingProcessed;                                       /// B or A
    e_dma_id_t  currentChannel;                                 /// E_DMA_0 or E_DMA_1
    int processingA = (gid % 2);                                /// odd numbered cores start with frame A while transferring data into B; odd numbered cores start on B
    e_dma_desc_t dmaDesc;

    /// how much data do we have to process
    unsigned int imageSize = args->szImageBuffer;
    unsigned int band = imageSize / ECORES;                     /// split the image up int 16 "bands" (or horizontal strips)
    if (gid == LASTCORENUM)                                     /// =========================== this needs checking ==========================================
        band += imageSize % ECORES;                             /// if it is not exactly divisible by 16 then add the remainder onto the workload for core 15
    unsigned int workUnits = band / workArea;
    unsigned int tailEnds = band % workArea;
    unsigned int trxCount;                                      /// frames or tailEndCount

    /// where in the global buffer does it come from
    void * startLoc = args->g_greyVals + (gid * band * sizeof(uint8_t));

    /// debug
    host_printf("%d\t\timagesize=%u\tband=%u\tspace=0x%x\tframeSizeBytes=%d\ttaileEnd=%u\tframes=%d\tstartloc=0x%x\tA=x0%x\tB=0x%x\n", gid, imageSize, band, localSize, workArea, tailEnds, workUnits, startLoc, A, B);

    if(processingA)
    {
        beingTransferred = A;        /// transfer first frame to A ready for processing
        currentChannel = E_DMA_0;   /// on E_DMA_0
    }
    else
    {
        beingTransferred = B;
        currentChannel = E_DMA_1;
    }

    if(workUnits)
        trxCount = workArea / 8;                                /// there are 8 values per DWORD
    else
        trxCount = tailEnds / 8;

    host_printf("%d\t%d\tto 0x%x\tfrom 0x%x\n", gid, trxCount, beingTransferred, startLoc);
    e_dma_set_desc(currentChannel,                              /// channel
                    E_DMA_DWORD | E_DMA_ENABLE | E_DMA_MASTER,  /// config
                    0,                                          /// next descriptor (there isn't one)
                    0x0008,                                     /// inner stride source (sizeof(DWORD))
                    0x0008,                                     /// inner stride destination
                    trxCount,                                   /// inner count of data items to transfer (one row)
                    1,                                          /// outer count (1 we are doing 1D dma)
                    0,                                          /// outer stride source N/A in 1D dma
                    0,                                          /// outer stride destination N/A in 1D dma
                    startLoc,                                   /// starting location source
                    (void*)beingTransferred,                    /// starting location destination
                    &dmaDesc);                                  /// dma descriptor

    e_dma_start(&dmaDesc, currentChannel);
    host_printf("%d\tstarted \n", gid);

    while(workUnits--)
    {
#if TIMEIT == 2
        STARTCLOCK1(waitStartTicks);  /// e_dma_wait does not idle - it is a wait loop
#endif // TIMEIT
        e_dma_wait(currentChannel);         /// wait for the current transfer to complete
#if TIMEIT == 2
        STOPCLOCK1(waitStopTicks);
        totalWaitTicks += (waitStartTicks - waitStopTicks);
#endif // TIMEIT

        if(processingA)
        {
            beingTransferred = B;           /// transfer next frame to B
            currentChannel = E_DMA_1;       /// on E_DMA_1
            beingProcessed = A;
        }
        else
        {
            beingTransferred = A;
            currentChannel = E_DMA_0;
            beingProcessed = B;
        }

        if(workUnits)      /// i.e. there are  more full frames to transfer
            trxCount = workArea / 8;
        else            /// there is only the tail end left
            trxCount = tailEnds / 8;

        startLoc += workArea;     /// transfer the next frame

        e_dma_set_desc(currentChannel,                              /// channel
                        E_DMA_DWORD | E_DMA_ENABLE | E_DMA_MASTER,  /// config
                        0,                                          /// next descriptor (there isn't one)
                        0x0008,                                     /// inner stride source
                        0x0008,                                     /// inner stride destination
                        trxCount,                                   /// inner count of data items to transfer (one row)
                        1,                                          /// outer count (1 we are doing 1D dma)
                        0,                                          /// outer stride source N/A in 1D dma
                        0,                                          /// outer stride destination N/A in 1D dma
                        startLoc,                                   /// starting location source
                        (void*)beingTransferred,                    /// starting location destination
                        &dmaDesc);                                  /// dma descriptor

        ///host_printf("%d\tframe=%d\t%d\tto 0x%x\tfrom 0x%x\tto 0x%x\n", gid, frames, trxCount, beingTransferred, startLoc, beingTransferred);
        e_dma_start(&dmaDesc, currentChannel);

        for(i=0; i<workArea; i++)          /// only the last frame will have tailEndInts to process and that is done below
            ++greyDistribution[beingProcessed[i]];

        processingA = !processingA;             /// swap buffers
    }

    if(tailEnds)
    {
    host_printf("%d\twaiting \n", gid);
        e_dma_wait(currentChannel);                         /// wait for the last transfer to complete
        for(i=0; i<tailEnds; i++)                        /// scan the remaining data
            ++greyDistribution[beingTransferred[i]];        /// the last transferred frame
    }

    //host_printf("%d\t\tspace=0x%x\timagesSize=%d\tframeSize=%d\tband=%d\tframes=%d\tstartloc=%x\t\n", gid, localSize, imageSize, frameSizeBytes, band, frames, startLoc);

    /// write back the results synchronously because there is nothing else to do
    //unsigned int writeTo = (args->g_result) + (gid * GREYLEVELS * sizeof(int));
    //host_printf("%d\t%x\n", gid, writeTo);
    host_printf("%d\tcopy back\n", gid);

    e_dma_copy((args->g_result) + (gid * GREYLEVELS * sizeof(int)), (void*)greyDistribution, GREYLEVELS * sizeof(int));
    //e_write((void*)&e_emem_config, 0, 0, (args->g_result) + (gid * GREYLEVELS * sizeof(int)), (void*)greyDistribution, GREYLEVELS * sizeof(int));
    host_printf("%d\texiting\n", gid);

tidyUpAndExit:
    /// tidy up
    coprthr_tls_brk(baseAddr);

#if TIMEIT == 2
    STOPCLOCK0(clkStopTicks);
    totalClockTicks = (clkStartTicks - clkStopTicks);
    host_printf("%d: Total Ticks: %u, working ticks: %u waiting ticks: %u (%0.2f%%).\n", gid, totalClockTicks, (totalClockTicks - totalWaitTicks), totalWaitTicks, ((float)(totalClockTicks - totalWaitTicks) / (float)totalClockTicks) * 100.0);
#endif // TIMEIT
}
