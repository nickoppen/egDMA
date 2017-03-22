#include <coprthr.h>
#include <coprthr_mpi.h>

#include "esyscall.h"

#include <host_stdio.h>

#include "egdma.h"

void __entry k_scan(pass_args * args)
{
    unsigned int gid = coprthr_corenum();
    int greyDistribution[GREYLEVELS] = { 0 };   /// all elements set to 0
    int i;
    unsigned int imageSize = (args->height) * (args->width);

    register uintptr_t sp_val;      /// Thanks jar
    __asm__ __volatile__(
       "mov %[sp_val], sp"
       : [sp_val] "=r" (sp_val)
    );

    /// how much space do we have
    void * baseAddr = coprthr_tls_sbrk(0);                      /// begining of free space
    uintptr_t baseLowOrder = (int)baseAddr & 0x0000FFFF;
    unsigned int localSize = sp_val - baseLowOrder - 0x40;      /// amount of free space available in bytes   /// leave 64 bytes as a buffer
    localSize = 4096; // pending testing
    unsigned int frameSizeBytes = localSize / 2;                /// bytes       /// a frame is the smallest processing chunk
    unsigned int frameSizeInts = frameSizeBytes / sizeof(int);  /// ints        /// the number of ints in a frame
    int * A = (int *)coprthr_tls_sbrk(frameSizeBytes);          /// 1st frame
    int * B = (int *)coprthr_tls_sbrk(frameSizeBytes);          /// 2nd frame
    int processingA = (gid % 2);                                /// even numbered cores start with frame A while transferring data into B; odd numbered cores start on B

    /// how much data do we have to process
    unsigned int band = imageSize / ECORES;                     /// split the image up int 16 "bands" (or horizontal strips)
    if (gid == LASTCORENUM)
        band += imageSize % ECORES;                             /// if it is not exactly divisible by 16 then add the remainder onto the workload for core 15
    unsigned int frames = band / frameSizeInts;

    /// where in the global buffer does it come from
    int * startLocA = args->g_greyVals + (gid * band * sizeof(int));
    int * startLocB = startLocA + frameSizeBytes;

    /// debug
///    host_printf("%d: sp=%x lowOrder=%x space=%x imagesSize=%d frameSize=%d band=%d frames=%d startloc=%x\n", gid, sp_val, baseLowOrder, (sp_val - baseLowOrder), imageSize, frameSizeBytes, band, frames, startLocA);
    host_printf("%d: space=%x imagesSize=%d frameSize=%d band=%d frames=%d startlocA=%x startlocB=%x\n", gid, localSize, imageSize, frameSizeBytes, band, frames, startLocA, startLocB);

    e_dma_desc_t dmaDesc;
    if(processingA)
    {
        e_dma_set_desc(E_DMA_0,                                     /// channel
                        E_DMA_WORD | E_DMA_ENABLE | E_DMA_MASTER,   /// config
                        0,                                          /// next descriptor (there isn't one)
                        sizeof(int),                                /// inner stride source
                        sizeof(int),                                /// inner stride destination
                        frameSizeInts,                              /// inner count of data items to transfer (one bufferful of ints)
                        1,                                          /// outer count (1 we are doing 1D dma)
                        0,                                          /// outer stride source N/A in 1D dma
                        0,                                          /// outer stride destination N/A in 1D dma
                        (void*)startLocA,                           /// starting location source
                        (void*)A,                                   /// starting location destination
                        &dmaDesc);                                  /// dma descriptor
//    host_printf("%d: first Transfer to A from startLocA=%x\n", gid, startLocA);
        e_dma_start(&dmaDesc, E_DMA_0);
    }
    else
    {
        e_dma_set_desc(E_DMA_1,                                     /// channel
                        E_DMA_WORD | E_DMA_ENABLE | E_DMA_MASTER,   /// config
                        0,                                          /// next descriptor (there isn't one)
                        sizeof(int),                                /// inner stride source
                        sizeof(int),                                /// inner stride destination
                        frameSizeInts,                              /// inner count of data items to transfer (one row)
                        1,                                          /// outer count (1 we are doing 1D dma)
                        0,                                          /// outer stride source N/A in 1D dma
                        0,                                          /// outer stride destination N/A in 1D dma
                        (void*)startLocB,                           /// starting location source
                        (void*)B,                                   /// starting location destination
                        &dmaDesc);                                  /// dma descriptor

//    host_printf("%d: first Transfer to B from startLocB=%x\n", gid, startLocB);
        e_dma_start(&dmaDesc, E_DMA_1);
    }

    while(frames--)
    {
        host_printf("%d: startlocA=%x startlocB=%x\n", gid, startLocA, startLocB);
        if(processingA)
        {
            e_dma_wait(E_DMA_0);

            /// Start the transfer to B before starting processing on A
            e_dma_set_desc(E_DMA_1,                                     /// channel
                            E_DMA_WORD | E_DMA_ENABLE | E_DMA_MASTER,   /// config
                            0,                                          /// next descriptor (there isn't one)
                            sizeof(int),                                /// inner stride source
                            sizeof(int),                                /// inner stride destination
                            frameSizeInts,                              /// inner count of data items to transfer (one row)
                            1,                                          /// outer count (1 we are doing 1D dma)
                            0,                                          /// outer stride source N/A in 1D dma
                            0,                                          /// outer stride destination N/A in 1D dma
                            (void*)startLocB,                           /// starting location source
                            (void*)B,                                   /// starting location destination
                            &dmaDesc);                                  /// dma descriptor

//    host_printf("%d: transferring to B from startLocB=%x\n", gid, startLocB);
            e_dma_start(&dmaDesc, E_DMA_1);

            for(i=0; i<frameSizeInts;i++)
                ++greyDistribution[A[i]];

            startLocA += localSize;
            processingA = 0;
        }
        else
        {
            e_dma_wait(E_DMA_1);

            /// start the transfer of A before processing B
            e_dma_set_desc(E_DMA_0,                                     /// channel
                            E_DMA_WORD | E_DMA_ENABLE | E_DMA_MASTER,   /// config
                            0,                                          /// next descriptor (there isn't one)
                            sizeof(int),                                /// inner stride source
                            sizeof(int),                                /// inner stride destination
                            frameSizeInts,                              /// inner count of data items to transfer (one row)
                            1,                                          /// outer count (1 we are doing 1D dma)
                            0,                                          /// outer stride source N/A in 1D dma
                            0,                                          /// outer stride destination N/A in 1D dma
                            (void*)startLocA,                           /// starting location source
                            (void*)A,                                   /// starting location destination
                            &dmaDesc);                                  /// dma descriptor

//    host_printf("%d: transferring to A from startLocA=%x\n", gid, startLocA);
            e_dma_start(&dmaDesc, E_DMA_0);

            for(i=0; i<frameSizeInts;i++)
                ++greyDistribution[B[i]];

            startLocB += localSize;
            processingA = -1;
        }
    }

    /// write back the results synchronously because there is nothing else to do
    e_dma_copy((args->g_result) + (gid * GREYLEVELS * sizeof(int)), (void*)greyDistribution, GREYLEVELS * sizeof(int));

    /// tidy up
    coprthr_tls_brk(baseAddr);
}

