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
    localSize = 8192; // pending testing
    unsigned int frameSizeBytes = localSize / 2;                /// bytes       /// a frame is the smallest processing chunk
    unsigned int frameSizeInts = frameSizeBytes / sizeof(int);  /// ints        /// the number of ints in a frame
    int * A = (int *)coprthr_tls_sbrk(frameSizeBytes);          /// 1st frame
    int * B = (int *)coprthr_tls_sbrk(frameSizeBytes);          /// 2nd frame
    int processingA = (gid % 2);                                /// odd numbered cores start with frame A while transferring data into B; odd numbered cores start on B

    /// how much data do we have to process
    unsigned int band = imageSize / ECORES;                     /// split the image up int 16 "bands" (or horizontal strips)
    if (gid == LASTCORENUM)
        band += imageSize % ECORES;                             /// if it is not exactly divisible by 16 then add the remainder onto the workload for core 15
    unsigned int frames = band / frameSizeInts;
    unsigned int tailEndInts = band % frameSizeInts;

    /// where in the global buffer does it come from
    void * startLocA = args->g_greyVals + (gid * band * sizeof(int));
    void * startLocB = startLocA + frameSizeBytes;

    /// debug
///    host_printf("%d sp=0x%x lowOrder=0x%x space=0x%x imagesSize=%d frameSize=%d band=%d frames=%d startloc=0x%x\n", gid, sp_val, baseLowOrder, (sp_val - baseLowOrder), imageSize, frameSizeBytes, band, frames, startLocA);
    host_printf("%d\t\tspace=0x%x\timagesSize=%d\tframeSize=%d\tband=%d\tframes=%d\tstartlocA=%u\tstartlocB=%u\n", gid, localSize, imageSize, frameSizeBytes, band, frames, startLocA, startLocB);

    e_dma_desc_t dmaDesc;
    if(processingA)
    {
        /// !!!! check that there are is at least one full frame otherwise transfer tailEndInts !!!!
        e_dma_set_desc(E_DMA_0,                                     /// channel
                        E_DMA_WORD | E_DMA_ENABLE | E_DMA_MASTER,   /// config
                        0,                                          /// next descriptor (there isn't one)
                        sizeof(int),                                /// inner stride source
                        sizeof(int),                                /// inner stride destination
                        frameSizeInts,                              /// inner count of data items to transfer (one bufferful of ints)
                        1,                                          /// outer count (1 we are doing 1D dma)
                        0,                                          /// outer stride source N/A in 1D dma
                        0,                                          /// outer stride destination N/A in 1D dma
                        startLocA,                                  /// starting location source
                        (void*)A,                                   /// starting location destination
                        &dmaDesc);                                  /// dma descriptor
    host_printf("%d\t%d\tA\t0x%x\n", gid, 99, startLocA);
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
                        startLocB,                                  /// starting location source
                        (void*)B,                                   /// starting location destination
                        &dmaDesc);                                  /// dma descriptor

    host_printf("%d\t%d\tB\t0x%x\n", gid, 99, startLocB);
        e_dma_start(&dmaDesc, E_DMA_1);
    }
//    goto rubbish;

    while(frames--)
    {
        if(processingA)
        {
            e_dma_wait(E_DMA_0);        /// wait for the transfer to A is complete

            if(frames)      /// i.e. there are  more full frames to transfer
            {
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
                                startLocB,                                  /// starting location source
                                (void*)B,                                   /// starting location destination
                                &dmaDesc);                                  /// dma descriptor
            }
            else            /// there is only the tail end left
            {
                if(tailEndInts)         /// make sure that there is a tail end
                    e_dma_set_desc(E_DMA_1,
                                    E_DMA_WORD | E_DMA_ENABLE | E_DMA_MASTER,
                                    0,
                                    sizeof(int),
                                    sizeof(int),
                                    tailEndInts,                                /// the rest of the image
                                    1,
                                    0,
                                    0,
                                    startLocB,
                                    (void*)B,
                                    &dmaDesc);
            }

    host_printf("%d\t%d\tB\t0x%x 0x%x\n", gid, frames, startLocB, dmaDesc.count);
            e_dma_start(&dmaDesc, E_DMA_1);

            for(i=0; i<frameSizeInts; i++)
                ++greyDistribution[A[i]];

            startLocA += localSize;
            processingA = 0;
        }
        else
        {
            e_dma_wait(E_DMA_1);

            if(frames)      /// i.e. there are more full frames to transfer
            {
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
                                startLocA,                                  /// starting location source
                                (void*)A,                                   /// starting location destination
                                &dmaDesc);                                  /// dma descriptor
            }
            else
            {
                if(tailEndInts)
                    e_dma_set_desc(E_DMA_0,
                                    E_DMA_WORD | E_DMA_ENABLE | E_DMA_MASTER,
                                    0,
                                    sizeof(int),
                                    sizeof(int),
                                    tailEndInts,                                /// the rest of the image
                                    1,
                                    0,
                                    0,
                                    startLocA,
                                    (void*)A,
                                    &dmaDesc);
            }



    host_printf("%d\t%d\tA\t0x%x 0x%x\n", gid, frames, startLocA, dmaDesc.count);
            e_dma_start(&dmaDesc, E_DMA_0);

            for(i=0; i<frameSizeInts; i++)
                ++greyDistribution[B[i]];

            startLocB += localSize;
            processingA = -1;
        }
    }

    if(tailEndInts)
    {
        if(processingA)
        {
            e_dma_wait(E_DMA_0);            /// wait for the transfer to A is complete
            for(i=0; i<tailEndInts; i++)    /// scan the remaining data
                ++greyDistribution[A[i]];
        }
        else
        {
            e_dma_wait(E_DMA_1);
            for(i=0; i<tailEndInts; i++)
                ++greyDistribution[B[i]];
        }
    }

    host_printf("%d\t\tspace=0x%x\timagesSize=%d\tframeSize=%d\tband=%d\tframes=%d\tstartlocA=%u\tstartlocB=%u\n", gid, localSize, imageSize, frameSizeBytes, band, frames, startLocA, startLocB);

    /// write back the results synchronously because there is nothing else to do
    e_dma_copy((args->g_result) + (gid * GREYLEVELS * sizeof(int)), (void*)greyDistribution, GREYLEVELS * sizeof(int));

//rubbish:
//    if(processingA)
//    {
//        e_dma_wait(E_DMA_0);
//        for (i=0;i<GREYLEVELS; i++)
//            greyDistribution[i] = A[i];
//    }
//    else
//    {
//        e_dma_wait(E_DMA_1);
//        for (i=0;i<GREYLEVELS; i++)
//            greyDistribution[i] = B[i];
//    }
//    e_dma_copy((args->g_result) + (gid * GREYLEVELS * sizeof(int)), (void*)greyDistribution, GREYLEVELS * sizeof(int));

    /// tidy up
    coprthr_tls_brk(baseAddr);
}

