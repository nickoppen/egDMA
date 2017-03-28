#include <stdio.h>
#include <time.h>
#include <coprthr.h>
#include <coprthr_cc.h>
#include <coprthr_thread.h>
#include <coprthr_mpi.h>

#include "egdma.h"

//#include <e-lib.h>


int main(int argc, char** argv)
{

    int width, height;
    int * greyVals;     /// allocated after the width and height are know
    char txt[10];
    int i, j, k;
    int * pGreyVals;
//    int * debugGrey;
    size_t sizeInBytes;
    unsigned int coreResults[ECORES * GREYLEVELS];
    unsigned int combinedResults[GREYLEVELS] = { 0 };    /// set all to zero
    unsigned int cdf_image[GREYLEVELS] = { 0 };
    unsigned int cdf_ideal[GREYLEVELS] = { 0 };
    unsigned int idealFreq;
    unsigned int map[GREYLEVELS] = { 0 };
    int debug[1024];

    coprthr_mem_t eGreyVals;
    coprthr_mem_t eCoreResults;

/// Read in the grey image information as a text file
/// The first two lines are the dimensions of the image
/// followed by the comma separated grey scale values.
/// Image lines are separated by a ;

    FILE * greyFile;

    greyFile = fopen("./grey.csv", "r");
    if(!greyFile)
    {
        printf("Something wrong with the input grey file...\n");
        exit(-1);
    }

    fscanf(greyFile, "%s %d", txt, &width);
    fscanf(greyFile, "%s %d", txt, &height);

    /// Allocate space to store the grey scale information
    sizeInBytes = width * height * sizeof(int);
    greyVals = malloc(sizeInBytes);
//    debugGrey = malloc(sizeInBytes);

    /// read in the grey scale values
    fscanf(greyFile, "%s [", txt);
    pGreyVals = greyVals;
    for(i=0; i < height; i++)
    {
        for(j=0; j < width - 1; j++)
        {
            fscanf(greyFile, " %d,", pGreyVals++);
        }
        fscanf(greyFile, " %d;", pGreyVals++);
    }

    /// testing
//    for(j=0;j<GREYLEVELS;j++)
//        printf("%d\t", j);
//    printf("\n");

#if TIMEIT == 1
    clock_t hostTime = clock();
#endif // TIMEIT
    for(i=0;i<height*width;i++)
        ++combinedResults[greyVals[i]];
#if TIMEIT == 1
    hostTime = clock() - hostTime;
#endif // TIMEIT

    goto  skip;
    for(i=0;i<GREYLEVELS;i++)
    {
//        printf("%d\t", combinedResults[i]);
        combinedResults[i] = 0; /// reset
    }
//    printf("\ncore output\n");
    /// end testing

#if TIMEIT == 1
clock_t eTime = clock();
#endif // TIMEIT

    /// Open the co processor
	int dd = coprthr_dopen(COPRTHR_DEVICE_E32,COPRTHR_O_THREAD);
	if (dd<0)
	{
        printf("Device open failed.\n");
        exit(0);
    }

    eGreyVals = coprthr_dmalloc(dd, sizeInBytes, 0);
    coprthr_dwrite(dd, eGreyVals, 0, (void*)greyVals, sizeInBytes, COPRTHR_E_WAIT);

    eCoreResults = coprthr_dmalloc(dd, (ECORES * GREYLEVELS * sizeof(int)), 0); /// Output only

    scan_args args;
    args.width = width;
    args.height = height;
    args.g_result = (void*)coprthr_memptr(eCoreResults, 0);
    args.g_greyVals = (void*)coprthr_memptr(eGreyVals, 0);
//    args.debug = debug;

	coprthr_program_t prg = coprthr_cc_read_bin("./egdma.e32", 0);
    coprthr_sym_t krn = coprthr_getsym(prg, "k_scan");
//    coprthr_event_t ev = coprthr_dexec(dd, ECORES, krn, (void*)&args, 0);
   coprthr_mpiexec(dd, ECORES, krn, &args, sizeof(args), 0);

    coprthr_dwait(dd);
    coprthr_dread(dd, eCoreResults, 0, coreResults, ECORES * GREYLEVELS * sizeof(int), COPRTHR_E_WAIT);

    /// combind the individual counts from the cores
    k = 0;
    for(i=0;i<ECORES;i++)
    {
        for (j=0;j<GREYLEVELS;j++)
        {
            combinedResults[j] += coreResults[k];
//            printf("%d\t", coreResults[k]);
            k++;
//            if ((k) && !(k % 256))
//                printf("\n");
        }
    }

#if TIMEIT == 1
    eTime = clock() - eTime ;
    printf("The scan on the host took: %ld milliseconds. The Epiphany took: %ld milliseconds\n", hostTime, eTime);
#endif // TIMEIT

skip:
    /// calculate the image's cumulative freq and the ideal cum freq
    idealFreq = (width * height) / GREYLEVELS;      /// TODO: add the remainder to the middle of the ideal
    cdf_ideal[0] = idealFreq;
    cdf_image[0] = combinedResults[0];
    for(j=1;j<GREYLEVELS;j++)
    {
        cdf_ideal[j] = cdf_ideal[j-1] + idealFreq;
        cdf_image[j] = cdf_image[j-1] + combinedResults[j];
    }

    /// calculate the map
    i = 0;
    for(j=1;j<GREYLEVELS;j++)
    {
        while (cdf_ideal[i] > cdf_image[j])
            i++;
        map[j] = i;
    }

    printf("grey, cdf image, cdf_ideal, map\n");
    for(j=0;j<GREYLEVELS;j++)
        printf("%d\t", j);
    printf("\n");
    for(j=0;j<GREYLEVELS;j++)
        printf("%u\t", cdf_image[j]);
    printf("\n");
    for(j=0;j<GREYLEVELS;j++)
        printf("%u\t", cdf_ideal[j]);
    printf("\n");
    for(j=0;j<GREYLEVELS;j++)
        printf("%u\t", map[j]);
    printf("\n");
    printf("\n");




    /// tidy up
    coprthr_dfree(dd, eGreyVals);
    coprthr_dclose(dd);
    free(greyVals);
    exit(1);

}


