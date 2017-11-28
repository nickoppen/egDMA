#include <stdio.h>
#include <time.h>
#include <coprthr.h>
#include <coprthr_cc.h>
#include <coprthr_thread.h>
#include <coprthr_mpi.h>

#include "egdma.h"
#define USAGE {printf("Usage: %s [<inputTextFile>] [-o <outputTextFile>\n", argv[0]); exit (-1);}
#define FILEERR { printf("Something wrong with the input grayscale file: %s...\n", argv[1]); exit(-1); }

int epip_callback(int coreId, int i);
USRCALL(epip_callback, 1);
int epip_callback(int coreId, int i)
{
    printf("call back received from: %i", coreId);

}

int main(int argc, char** argv)
{

    int width, height;                                  /// the dimensions of the image
    uint8_t * grayVals;                                 /// allocated after the width and height are known
    uint8_t * equalGray;                                /// the equalised gray values
    uint8_t * pGreyVals;                                /// an index into the grayVals table
    char txt[10];                                       /// text input buffer
    int newVal;                                         /// integer input buffer
    int i, j, k;

    size_t sizeInBytes;                                 /// the number of gray values in the image
    size_t szImageBuffer;                               /// the size of the image buffer (always divisible by 8 i.e. one D_WORD)
    unsigned int coreResults[ECORES * GRAYLEVELS];      /// the results calculated but the cores
    unsigned int combinedResults[GRAYLEVELS] = { 0 };   /// the combinded core results (set all to zero)
    unsigned int cdf_image[GRAYLEVELS] = { 0 };         /// the cumulative distribution of gray levels in the image
    unsigned int cdf_ideal[GRAYLEVELS] = { 0 };         /// the ideal (evenly distributed) gray levels
    unsigned int idealFreq;                             /// the ideal number of pixels of each gray level
    uint8_t      map[GRAYLEVELS] = { 0 };               /// the translation map of existing gray levels (the index) to the ideal level (the value)
    size_t sizeOfMap;
//    int debug[1024];
    uint8_t test;

    coprthr_mem_t eGrayVals;
    coprthr_mem_t eCoreResults;
    coprthr_mem_t eMap;

/// Read in the gray image information as a text file
/// The first two lines are the dimensions of the image
/// followed by the comma separated gray scale values.
/// Image lines are separated by a ;

    FILE * grayFile;
    int outFileArg = 0;      /// which command line arguement contain the string for the output file

    switch (argc)
    {
    case 1:     /// stdin and stdout
//        printf("argc=%d, 0=%s, 1=%s\n", argc, argv[0], argv[1]);
        grayFile =  stdin;
        outFileArg = 0;
    break;
    case 2:     /// in from file out to stdout
//        printf("argc=%d, 0=%s, 1=%s\n", argc, argv[0], argv[1]);
        grayFile = fopen(argv[1], "r");
        if(!grayFile)
            FILEERR;
        outFileArg = 0;
    break;
    case 3:         /// in from stdin out to file (with -o on the cmd lne)
//        printf("argc=%d, 0=%s, 1=%s, 2=%s\n", argc, argv[0], argv[1], argv[2]);
        grayFile =  stdin;
        if (strcmp(argv[1], "-o") == 0)
            outFileArg = 2;
        else
            USAGE;
    break;
    case 4:         /// in from file out to file
//        printf("argc=%d, 0=%s, 1=%s, 2=%s, 3=%s\n", argc, argv[0], argv[1], argv[2], argv[3]);
        grayFile = fopen(argv[1], "r");
        if(!grayFile)
        {
            printf("Something wrong with the input grayscale file: %s...\n", argv[1]);
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
printf("reading\n");
    fscanf(grayFile, "%s %d", txt, &width);
    fscanf(grayFile, "%s %d", txt, &height);

    /// Allocate space to store the gray scale information
    sizeInBytes = width * height * sizeof(uint8_t);
    szImageBuffer = sizeInBytes;
    if (sizeInBytes % 8)
        szImageBuffer += (8 - (sizeInBytes % 8));  /// make the buffer a number dividible by 8

    /// THIS IS THE NEW BIT
    /// you used to have to declare and allocate local space using malloc: grayVals = malloc(szImageBuffer);
    /// now you allocate the space in shared memory (after opening the device):
    int dd = coprthr_dopen(COPRTHR_DEVICE_E32,COPRTHR_O_THREAD);
	if (dd<0)
	{
        printf("Device open failed.\n");
        exit(0);
    }
    eGrayVals = coprthr_dmalloc(dd, szImageBuffer, 0);
    /// and cast a local pointer directly into shared mem where you can treat it like local memory
    pGreyVals = (u_int8_t*)coprthr_memptr(eGrayVals, 0);
    /// CONTINUE ON AS BEFORE

    /// read in the gray scale values
    fscanf(grayFile, "%s [", txt);
    for(i=0; i < height; i++)
    {
        for(j=0; j < width - 1; j++)
        {
            fscanf(grayFile, " %d,", &newVal);   /// read the new value in as an int
            *pGreyVals = newVal & 0xFFFF;        /// stip off all but the last byte
            pGreyVals++;
        }
        fscanf(grayFile, " %d;", &newVal);
        *pGreyVals = newVal & 0xFFFF;        /// stip off all but the last byte
        pGreyVals++;
    }
    close(grayFile);

//for (i=szImageBuffer-16;i<szImageBuffer;i++)
//    printf("%u, ", grayVals[i]);
printf("\nsize: %u, buffersize: %u\n", sizeInBytes, szImageBuffer);

#ifdef TIMEHOST
    clock_t hostTime = clock();

    for(i=0;i<height*width;i++)
        ++combinedResults[grayVals[i]];
    hostTime = clock() - hostTime;

///   reset combinedResults for the epiphany results
    for(i=0;i<GRAYLEVELS;i++)
    {
        combinedResults[i] = 0; /// reset
    }

    clock_t eTime = clock();        /// start the epiphany clock
#endif // TIMEIT
printf("acc set up\n");

/// You don't have to call drite because the data is already in shared mem
///    coprthr_dwrite(dd, eGrayVals, 0, (void*)grayVals, szImageBuffer, COPRTHR_E_WAIT);

    eCoreResults = coprthr_dmalloc(dd, (ECORES * GRAYLEVELS * sizeof(int)), 0); /// Output only

    coprthr_mem_t argMemScan = coprthr_dmalloc(dd, sizeof(scan_args), 0); /// allocate shares arguement memory
    scan_args * ps_Args = (scan_args*)coprthr_memptr(argMemScan, 0);
    ps_Args->width = width;
    ps_Args->height = height;
    ps_Args->szImageBuffer = szImageBuffer;
    ps_Args->g_result = (void*)coprthr_memptr(eCoreResults, 0);
    ps_Args->g_grayVals = pGreyVals; /// pGreyVals has already been massed into the correct form
//    s_args.debug = debug;

	coprthr_program_t prg = coprthr_cc_read_bin("./egdmaScan.e32", 0);
    coprthr_kernel_t krn = coprthr_getsym(prg, "k_scan");

printf("calling scan\n");
    coprthr_dexec(dd, ECORES, krn, (void*)&argMemScan, 0);
//    coprthr_mpiexec(dd, ECORES, krn, &s_args, sizeof(s_args), 0);

    coprthr_dwait(dd);
    coprthr_dread(dd, eCoreResults, 0, coreResults, ECORES * GRAYLEVELS * sizeof(int), COPRTHR_E_WAIT);
printf("combining\n");

    /// combind the individual counts from the cores
    k = 0;
    for(i=0;i<ECORES;i++)
    {
        for (j=0;j<GRAYLEVELS;j++)
        {
            combinedResults[j] += coreResults[k];
            k++;
        }
    }

#ifdef TIMEHOST
    eTime = clock() - eTime ;
    #ifdef UseDMA
    printf("Scan\tDMA\thost\t%ld\tEpiphany\t%ld\n", hostTime, eTime);
    #else
    printf("Scan\tmemcpy\thost\t%ld\tEpiphany\t%ld\n", hostTime, eTime);
    #endif // UseDMA
#endif // TIMEIT

calcCumFreq:
    /// calculate the image's cumulative freq and the ideal cum freq
    idealFreq = (width * height) / GRAYLEVELS;      /// TODO: add the remainder to the middle of the ideal
    cdf_ideal[0] = idealFreq;
    cdf_image[0] = combinedResults[0];
    for(j=1;j<GRAYLEVELS;j++)
    {
        cdf_ideal[j] = cdf_ideal[j-1] + idealFreq;
        cdf_image[j] = cdf_image[j-1] + combinedResults[j];
    }

    /// calculate the map
    i = 0;
    for(j=0;j<GRAYLEVELS;j++)
    {
        while ((cdf_image[j] > cdf_ideal[i]) && (i < 256))
            i++;
        map[j] = i;
    }

//    for(j=0;j<GRAYLEVELS; j++)
//        printf("%u ", combinedResults[j]);
//    printf("\n");
//    for(j=0;j<GRAYLEVELS; j++)
//        printf("%u ", map[j]);
//    printf("\n");
    printf("mapping\n");

#ifdef TIMEHOST

    uint8_t cpGrayVals[szImageBuffer];                                 /// copy the image for timing
    for (j=0;j<szImageBuffer;j++)
        cpGrayVals[j] = grayVals[j];

    hostTime = clock();
    for (j=0;j<height*width;j++)
        cpGrayVals[j] = map[cpGrayVals[j]];
    hostTime = clock() - hostTime;

    eTime = clock();
#endif

    sizeOfMap = GRAYLEVELS * sizeof(uint8_t);
    eMap = coprthr_dmalloc(dd, sizeOfMap, 0);
    coprthr_dwrite(dd, eMap, 0, (void*)map, sizeOfMap, COPRTHR_E_WAIT);

    coprthr_mem_t argMemMap = coprthr_dmalloc(dd, sizeof(map_args), 0); /// allocate shares arguement memory
    map_args * p_MapArgs = (map_args*)coprthr_memptr(argMemMap, 0);

    p_MapArgs->width = width;
    p_MapArgs->height = height;
    p_MapArgs->szImageBuffer = szImageBuffer;
    p_MapArgs->g_map = (void*)coprthr_memptr(eMap, 0);
    p_MapArgs->g_grayVals = (void*)coprthr_memptr(eGrayVals, 0);

	prg = coprthr_cc_read_bin("./egdmaMap.e32", 0);            /// still needed
    krn = coprthr_getsym(prg, "k_map");
    coprthr_dexec(dd, ECORES, krn, (void*)&argMemMap, 0);

    coprthr_dwait(dd);
    equalGray = malloc(szImageBuffer);
    coprthr_dread(dd, eGrayVals, 0, (void*)equalGray, szImageBuffer, COPRTHR_E_WAIT);


#ifdef TIMEHOST
    eTime = clock() - eTime;
    #ifdef UseDMA
    printf("Map\thost\t%ld\tDMA\tEpiphany\t%ld\n", hostTime, eTime);
    #else
    printf("Map\tmemcpy\thost\t%ld\tEpiphany\t%ld\n", hostTime, eTime);
    #endif // UseDMA
#endif // TIMEIT

/// Output the equalised gray values into a new file

    if(outFileArg)
    {
        grayFile = fopen(argv[outFileArg], "w");

        if(!grayFile)
        {
            printf("%s failed to open the otuput file: %s...\n", argv[0], argv[outFileArg]);
            exit(-1);
        }
    }
    else
        grayFile = stdout;

    k = 0;
    fprintf(grayFile, "width %d\nheight %d\nimage [", width, height);
    for(i=0;i<height; i++)
    {
        for(j=0;j<width - 1;j++)
            fprintf(grayFile, "%u, ", equalGray[k++]);
        if (i < (height-1))
            fprintf(grayFile, "%u;\n", equalGray[k++]);
        else
            fprintf(grayFile, "%u]", equalGray[k++]);
    }
    fflush(grayFile);
    close(grayFile);

    /// tidy up
tidyUpAndExit:
    coprthr_dfree(dd, eMap);
    coprthr_dfree(dd, eGrayVals);
    coprthr_dfree(dd, argMemScan);
    coprthr_dfree(dd, argMemMap);
    coprthr_dclose(dd);
    free(grayVals);
    exit(1);

}


