#include <coprthr.h>
#include <coprthr_mpi.h>

#include "esyscall.h"

#include <host_stdio.h>

#include "egdma.h"

void __entry k_scan(pass_args * args)
{
    int gid = coprthr_corenum();
    int greyDistribution[GREYLEVELS];
    int i;
    int * A;
    int * B;
    int processingA = -1;   /// TRUE
    unsigned int imageSize = args->height * args->width;
    e_dma_desc_t dmaDesc;

    register uintptr_t sp_val;
    __asm__ __volatile__(
       "mov %[sp_val], sp"
       : [sp_val] "=r" (sp_val)
    );

    /// how much space do we have
    void * baseAddr = coprthr_tls_sbrk(0);   /// begining of free space
    uintptr_t baseLowOrder = (int)baseAddr & 0x0000FFFF;
    unsigned int localSize = sp_val - baseLowOrder - 0x40;     /// bytes   /// leave 64 bytes as a buffer
    localSize = 4096; // pending testing
    unsigned int frameSize = localSize / 2; /// bytes
    A = (int *)coprthr_tls_sbrk(frameSize);   /// 1st buffer
    B = (int *)coprthr_tls_sbrk(frameSize);   /// 2nd buffer

    /// how much data do we have to process
    unsigned int band = imageSize/ ECORES;
    if (gid == LASTCORENUM)
        band += imageSize % ECORES;
    unsigned int frames = (band * sizeof(int)) / frameSize;

    /// where in the global buffer does it come from
    int * startLoc = args->g_greyVals + (gid * band);

    e_dma_set_desc(E_DMA_0, E_DMA_DWORD | E_DMA_ENABLE | E_DMA_MASTER, 0, sizeof(int), sizeof(int), frameSize, 0, 0, 0, (void*)startLoc, (void*)A, &dmaDesc);

    /// write back the results synchronously because there is nothing else to do
    host_printf("%d: sp=%x lowOrder=%x space=%x imagesSize=%d frameSize=%d band=%d frames=%d startloc=%x\n", gid, sp_val, baseLowOrder, (sp_val - baseLowOrder), imageSize, frameSize, band, frames, startLoc);


    for (i = 0; i < GREYLEVELS; i++)
        greyDistribution[i] = A[i];
    e_dma_copy((args->g_result) + (gid * GREYLEVELS * sizeof(int)), (void*)greyDistribution, GREYLEVELS * sizeof(int));

    /// tidy up
    coprthr_tls_brk(baseAddr);
}
