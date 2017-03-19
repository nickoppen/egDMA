#include <coprthr.h>
#include <coprthr_mpi.h>

#include "esyscall.h"

#include <host_stdio.h>

#include "egdma.h"

void __entry k_scan(pass_args * args)
{
    host_printf("entry\n");
    coprthr_barrier(0);
    int gid = coprthr_corenum();
    host_printf("%d: entry\n", gid);
    int greyDistribution[GREYLEVELS];
    int * A;
    int * B;
    int processingA = -1;   /// TRUE
    int localSize = 2048; // =()

    e_dma_desc_t dmaDesc;

    void * baseAddr = coprthr_tls_sbrk(0);   /// remember the base address for reset
//    A = (int *)coprthr_tls_sbrk(localSize * sizeof(int));
//    B = (int *)coprthr_tls_sbrk(localSize * sizeof(int));

    coprthr_barrier(0);
    for (localSize = 0; localSize < GREYLEVELS; localSize++)
        greyDistribution[localSize] = gid;
    coprthr_barrier(0);

    /// write back the results synchronously because there is nothing else to do
    host_printf("%d: writing to result array\n", gid);
    e_dma_copy((args->g_result) + (gid * GREYLEVELS * sizeof(int)), (void*)greyDistribution, GREYLEVELS * sizeof(int));

    /// tidy up
    coprthr_tls_brk(baseAddr);
}
