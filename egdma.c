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

    int width, height;                                  /// the dimensions of the image
    int * greyVals;                                     /// allocated after the width and height are know
    int * equalGrey;                                    /// the equalised grey values
    int * pGreyVals;                                    /// an index into the greyVals table
    char txt[10];                                       /// text input buffer
    int i, j, k;

    size_t sizeInBytes;
    unsigned int coreResults[ECORES * GREYLEVELS];      /// the results calculated but the cores
    unsigned int combinedResults[GREYLEVELS] = { 0 };   /// the combinded core results (set all to zero)
    unsigned int cdf_image[GREYLEVELS] = { 0 };         /// the cumulative distribution of grey levels in the image
    unsigned int cdf_ideal[GREYLEVELS] = { 0 };         /// the ideal (evenly distributed) grey levels
    unsigned int idealFreq;                             /// the ideal number of pixels of each grey level
    unsigned int map[GREYLEVELS] = { 0 };               /// the translation map of existing grey levels (the index) to the ideal level (the value)
    unsigned int mapcpy[GREYLEVELS] = { 0 };//testing
    size_t sizeOfMap;
    int debug[1024];

    coprthr_mem_t eGreyVals;
    coprthr_mem_t eCoreResults;
    coprthr_mem_t eMap;

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
    close(greyFile);

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

//    for(i=0;i<GREYLEVELS;i++)
//    {
//        printf("%d\t", combinedResults[i]);
//        combinedResults[i] = 0; /// reset
//    }
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

goto  calcCumFreq;      /// testing
    eCoreResults = coprthr_dmalloc(dd, (ECORES * GREYLEVELS * sizeof(int)), 0); /// Output only

    scan_args s_args;
    s_args.width = width;
    s_args.height = height;
    s_args.g_result = (void*)coprthr_memptr(eCoreResults, 0);
    s_args.g_greyVals = (void*)coprthr_memptr(eGreyVals, 0);
//    s_args.debug = debug;

	coprthr_program_t prg = coprthr_cc_read_bin("./egdma.e32", 0);
    coprthr_sym_t krn = coprthr_getsym(prg, "k_scan");
//    coprthr_event_t ev = coprthr_dexec(dd, ECORES, krn, (void*)&s_args, 0);
    coprthr_mpiexec(dd, ECORES, krn, &s_args, sizeof(s_args), 0);

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
//    printf("\ngrey\t");
//    for(j=0;j<GREYLEVELS;j++)
//        printf("%d\t", j);
//    printf("\neFreq\t");
//    for(j=0;j<GREYLEVELS;j++)
//        printf("%u\t", combinedResults[j]);
//    printf("\n");

#if TIMEIT == 1
    eTime = clock() - eTime ;
    printf("The scan on the host took: %ld milliseconds. The Epiphany took: %ld milliseconds\n", hostTime, eTime);
#endif // TIMEIT

calcCumFreq:
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
    for(j=0;j<GREYLEVELS;j++)
    {
        while ((cdf_image[j] > cdf_ideal[i]) && (i < 256))
            i++;
        map[j] = i;
    }

    printf("\ngrey\t");
    for(j=0;j<GREYLEVELS;j++)
        printf("%d\t", j);
    printf("\nimage\t");
    for(j=0;j<GREYLEVELS;j++)
        printf("%u\t", cdf_image[j]);
    printf("\nideal\t");
    for(j=0;j<GREYLEVELS;j++)
        printf("%u\t", cdf_ideal[j]);
    printf("\nmap\t");
    for(j=0;j<GREYLEVELS;j++)
        printf("%u\t", map[j]);
    printf("\n");
    printf("\n"); fflush(stdout);

//goto tidyUpAndExit;

    sizeOfMap = GREYLEVELS * sizeof(int);
    printf("malloc %d\t", (int)sizeOfMap); fflush(stdout);
    eMap = coprthr_dmalloc(dd, sizeOfMap, 0);
    printf("dwrite\t"); fflush(stdout);
    coprthr_dwrite(dd, eMap, 0, (void*)map, sizeOfMap, COPRTHR_E_WAIT);

    printf("adding params\t"); fflush(stdout);
    map_args m_args;
    m_args.width = width;
    m_args.height = height;
    m_args.g_map = (void*)coprthr_memptr(eMap, 0);
    m_args.g_greyVals = (void*)coprthr_memptr(eGreyVals, 0);

	prg = coprthr_cc_read_bin("./egdma.e32", 0);
printf("getsym from prog: 0x%x\n", (unsigned int)prg); fflush(stdout);
    krn = coprthr_getsym(prg, "k_map");
//    coprthr_event_t ev = coprthr_dexec(dd, ECORES, krn, (void*)&m_args, 0);
printf("calling dd:%u\t cores:%d\tkrn:0x%x\tmap: 0x%x\tsize:%d\n", dd, ECORES, (unsigned int)krn, (unsigned int)m_args.g_map, sizeof(m_args)); fflush(stdout);
    coprthr_mpiexec(dd, ECORES, krn, &m_args, sizeof(m_args), 0);

printf("waiting map\t"); fflush(stdout);
    coprthr_dwait(dd);
    equalGrey = malloc(sizeInBytes);
printf("readling\t"); fflush(stdout);
    coprthr_dread(dd, eGreyVals, 0, (void*)equalGrey, sizeInBytes, COPRTHR_E_WAIT);

    printf("\nnew\t");
    for(j=0;j<GREYLEVELS;j++)
        printf("%u, ", equalGrey[j]);
    printf("\n");   fflush(stdout);


    /// tidy up
tidyUpAndExit:
    coprthr_dfree(dd, eGreyVals);
    coprthr_dfree(dd, eMap);
    coprthr_dclose(dd);
    free(greyVals);
    exit(1);

}


