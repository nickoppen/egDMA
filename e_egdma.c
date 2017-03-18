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
    int localSize = 2; // =()

    e_dma_desc_t dmaDesc;

    void * baseAddr = coprthr_tls_sbrk(0);   /// remember the base address for reset
    A = (int *)coprthr_tls_sbrk(localSize * sizeof(int));
    B = (int *)coprthr_tls_sbrk(localSize * sizeof(int));

    host_printf("%d: writing to result array\n", gid);
    for (localSize = 0; localSize < GREYLEVELS; localSize++)
        greyDistribution[localSize] = gid;

    /// write back the results synchronously because there is nothing else to do
    e_dma_copy((args->g_result) + (gid * GREYLEVELS * sizeof(int)), (void*)greyDistribution, GREYLEVELS * sizeof(int));

    /// tidy up
    coprthr_tls_brk(baseAddr);
}
