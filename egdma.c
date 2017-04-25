#include <stdio.h>
#include <time.h>
#include <coprthr.h>
#include <coprthr_cc.h>
#include <coprthr_thread.h>
#include <coprthr_mpi.h>

#include "egdma.h"
#define USAGE {printf("Usage: %s [<inputTextFile>] [-o <outputTextFile>\n", argv[0]); exit (-1);}
#define FILEERR { printf("Something wrong with the input greyscale file: %s...\n", argv[1]); exit(-1); }



int main(int argc, char** argv)
{

    int width, height;                                  /// the dimensions of the image
    uint8_t * greyVals;                                 /// allocated after the width and height are known
    uint8_t * equalGrey;                                /// the equalised grey values
    uint8_t * pGreyVals;                                /// an index into the greyVals table
    char txt[10];                                       /// text input buffer
    int newVal;                                         /// integer input buffer
    int i, j, k;

    size_t sizeInBytes;                                 /// the number of grey values in the image
    size_t szImageBuffer;                               /// the size of the image buffer (always divisible by 8 i.e. one D_WORD)
    unsigned int coreResults[ECORES * GREYLEVELS];      /// the results calculated but the cores
    unsigned int combinedResults[GREYLEVELS] = { 0 };   /// the combinded core results (set all to zero)
    unsigned int cdf_image[GREYLEVELS] = { 0 };         /// the cumulative distribution of grey levels in the image
    unsigned int cdf_ideal[GREYLEVELS] = { 0 };         /// the ideal (evenly distributed) grey levels
    unsigned int idealFreq;                             /// the ideal number of pixels of each grey level
    uint8_t      map[GREYLEVELS] = { 0 };               /// the translation map of existing grey levels (the index) to the ideal level (the value)
    size_t sizeOfMap;
//    int debug[1024];
    uint8_t test;

    coprthr_mem_t eGreyVals;
    coprthr_mem_t eCoreResults;
    coprthr_mem_t eMap;

/// Read in the grey image information as a text file
/// The first two lines are the dimensions of the image
/// followed by the comma separated grey scale values.
/// Image lines are separated by a ;

    FILE * greyFile;
    int outFileArg = 0;      /// which command line arguement contain the string for the output file

    switch (argc)
    {
    case 1:     /// stdin and stdout
        printf("argc=%d, 0=%s, 1=%s\n", argc, argv[0], argv[1]);
        greyFile =  stdin;
        outFileArg = 0;
    break;
    case 2:     /// in from file out to stdout
        printf("argc=%d, 0=%s, 1=%s\n", argc, argv[0], argv[1]);
        greyFile = fopen(argv[1], "r");
        if(!greyFile)
            FILEERR;
        outFileArg = 0;
    break;
    case 3:         /// in from stdin out to file (with -o on the cmd lne)
        printf("argc=%d, 0=%s, 1=%s, 2=%s\n", argc, argv[0], argv[1], argv[2]);
        greyFile =  stdin;
        if (strcmp(argv[1], "-o") == 0)
            outFileArg = 2;
        else
            USAGE;
    break;
    case 4:         /// in from file out to file
        printf("argc=%d, 0=%s, 1=%s, 2=%s, 3=%s\n", argc, argv[0], argv[1], argv[2], argv[3]);
        greyFile = fopen(argv[1], "r");
        if(!greyFile)
        {
            printf("Something wrong with the input greyscale file: %s...\n", argv[1]);
            exit(-1);
        }
        if (strcmp(argv[2], "-o") == 0)
            outFileArg = 3;
        else
            USAGE;
    break;
    default:
        USAGE;
    }

    fscanf(greyFile, "%s %d", txt, &width);
    fscanf(greyFile, "%s %d", txt, &height);

    /// Allocate space to store the grey scale information
    sizeInBytes = width * height * sizeof(uint8_t);
    szImageBuffer = sizeInBytes;
    if (sizeInBytes % 8)            /// if the modulus is non-zero
        szImageBuffer += 8;         /// add another 8 bytes
    greyVals = malloc(szImageBuffer);
//    debugGrey = malloc(sizeInBytes);

    /// read in the grey scale values
    fscanf(greyFile, "%s [", txt);
    pGreyVals = greyVals;
    for(i=0; i < height; i++)
    {
        for(j=0; j < width - 1; j++)
        {
            fscanf(greyFile, " %d,", &newVal);   /// read the new value in as an int
            *pGreyVals = newVal & 0xFFFF;        /// stip off all but the last byte
            pGreyVals++;
        }
        fscanf(greyFile, " %d;", &newVal);
        *pGreyVals = newVal & 0xFFFF;        /// stip off all but the last byte
        pGreyVals++;
    }
    close(greyFile);

#if TIMEIT == 1
    clock_t hostTime = clock();

    for(i=0;i<height*width;i++)
        ++combinedResults[greyVals[i]];
    hostTime = clock() - hostTime;

///   reset combinedResults for the epiphany results
    for(i=0;i<GREYLEVELS;i++)
    {
        combinedResults[i] = 0; /// reset
    }

    clock_t eTime = clock();        /// start the epiphany clock
#endif // TIMEIT

    /// Open the co processor
    int dd = coprthr_dopen(COPRTHR_DEVICE_E32,COPRTHR_O_THREAD);
	if (dd<0)
	{
        printf("Device open failed.\n");
        exit(0);
    }

    eGreyVals = coprthr_dmalloc(dd, szImageBuffer, 0);
    coprthr_dwrite(dd, eGreyVals, 0, (void*)greyVals, szImageBuffer, COPRTHR_E_WAIT);

    eCoreResults = coprthr_dmalloc(dd, (ECORES * GREYLEVELS * sizeof(int)), 0); /// Output only

    scan_args s_args;
    s_args.width = width;
    s_args.height = height;
    s_args.szImageBuffer = szImageBuffer;
    s_args.g_result = (void*)coprthr_memptr(eCoreResults, 0);
    s_args.g_greyVals = (void*)coprthr_memptr(eGreyVals, 0);
//    s_args.debug = debug;

	coprthr_program_t prg = coprthr_cc_read_bin("./egdmaScan.e32", 0);
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
            k++;
        }
    }

#if TIMEIT == 1
    eTime = clock() - eTime ;
    printf("\nThe scan on the host took: %ld milliseconds. The Epiphany took: %ld milliseconds\n", hostTime, eTime);
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

#if TIMEIT == 1

    uint8_t cpGrayVals[szImageBuffer];                                 /// copy the image for timing
    for (j=0;j<szImageBuffer;j++)
        cpGrayVals[j] = greyVals[j];

    hostTime = clock();
    for (j=0;j<height*width;j++)
        cpGrayVals[j] = map[cpGrayVals[j]];
    hostTime = clock() - hostTime;

    eTime = clock();
#endif

    sizeOfMap = GREYLEVELS * sizeof(uint8_t);
    eMap = coprthr_dmalloc(dd, sizeOfMap, 0);
    coprthr_dwrite(dd, eMap, 0, (void*)map, sizeOfMap, COPRTHR_E_WAIT);

    map_args m_args;
    m_args.width = width;
    m_args.height = height;
    m_args.szImageBuffer = szImageBuffer;
    m_args.g_map = (void*)coprthr_memptr(eMap, 0);
    m_args.g_greyVals = (void*)coprthr_memptr(eGreyVals, 0);

	prg = coprthr_cc_read_bin("./egdmaMap.e32", 0);            /// still needed
    krn = coprthr_getsym(prg, "k_map");
//    coprthr_event_t ev = coprthr_dexec(dd, ECORES, krn, (void*)&m_args, 0);
    coprthr_mpiexec(dd, ECORES, krn, &m_args, sizeof(m_args), 0);

    coprthr_dwait(dd);
    equalGrey = malloc(szImageBuffer);
    coprthr_dread(dd, eGreyVals, 0, (void*)equalGrey, szImageBuffer, COPRTHR_E_WAIT);


#if TIMEIT == 1
    eTime = clock() - eTime;
    printf("\nThe map on the host took: %ld milliseconds. The Epiphany took: %ld milliseconds\n", hostTime, eTime);
#endif // TIMEIT

/// Output the equalised grey values into a new file

    if(outFileArg)
    {
        greyFile = fopen(argv[outFileArg], "w");

        if(!greyFile)
        {
            printf("Something wrong with the input grey file...\n");
            exit(-1);
        }
    }
    else
        greyFile = stdout;

    k = 0;
    fprintf(greyFile, "width %d\nheight %d\nimage [", width, height);
    for(i=0;i<height; i++)
    {
        for(j=0;j<width - 1;j++)
            fprintf(greyFile, "%u, ", equalGrey[k++]);
        if (i < (height-1))
            fprintf(greyFile, "%u;\n", equalGrey[k++]);
        else
            fprintf(greyFile, "%u]", equalGrey[k++]);
    }
    fflush(greyFile);
    close(greyFile);

    /// tidy up
tidyUpAndExit:
    coprthr_dfree(dd, eMap);
    coprthr_dfree(dd, eGreyVals);
    coprthr_dclose(dd);
    free(greyVals);
    exit(1);

}


