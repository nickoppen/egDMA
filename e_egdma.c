#include <coprthr.h>
#include <coprthr_mpi.h>

#include "esyscall.h"

#include <host_stdio.h>

#include "egdma.h"

void __entry k_scan(pass_args * args)
{
    int gid = coprthr_corenum();
    int greyDistribution[GREYLEVELS];
    int * A;
    int * B;
    int processingA = -1;   /// TRUE
    e_dma_desc_t dmaDesc;

    register uintptr_t sp_val;
    __asm__ __volatile__(
       "mov %[sp_val], sp"
       : [sp_val] "=r" (sp_val)
    );

    void * baseAddr = coprthr_tls_sbrk(0);   /// begining of free space
    uintptr_t baseLowOrder = (int)baseAddr & 0x0000FFFF;
    int localSize = sp_val - baseLowOrder - 0x40;       /// leave 64 bytes as a buffer

//    A = (int *)coprthr_tls_sbrk(localSize * sizeof(int));
//    B = (int *)coprthr_tls_sbrk(localSize * sizeof(int));

    for (localSize = 0; localSize < GREYLEVELS; localSize++)
        greyDistribution[localSize] = gid;
    coprthr_barrier(0);

    /// write back the results synchronously because there is nothing else to do
    host_printf("%d: sp=%x membase=%x lowOrder=%x space=%x\n", gid, sp_val, *(int*)baseAddr, baseLowOrder, (sp_val - baseLowOrder));
    e_dma_copy((args->g_result) + (gid * GREYLEVELS * sizeof(int)), (void*)greyDistribution, GREYLEVELS * sizeof(int));

    /// tidy up
    coprthr_tls_brk(baseAddr);
}

