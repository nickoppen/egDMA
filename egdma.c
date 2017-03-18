#include <stdio.h>
#include <coprthr.h>
#include <coprthr_cc.h>
#include <coprthr_thread.h>
#include <coprthr_mpi.h>

#include "egdma.h"

//#include <e-lib.h>

#define ECORES 16    /// Not great - Should be a function call

int main(int argc, char** argv)
{

    int width, height;
    int * greyVals;     /// allocated after the width and height are know
    char txt[10];
    int i, j;
    int * pGreyVals;
    size_t sizeInBytes;
    int coreResults[ECORES * GREYLEVELS];
    int combinedResults[GREYLEVELS];
    int debug[1024];

    coprthr_mem_t eGreyVals;
//    coprthr_mem_t eCoreResults;

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
//    printf("%s = %d\n", txt, width);
    fscanf(greyFile, "%s %d", txt, &height);
//    printf("%s = %d\n", txt, height);

    /// Allocate space to store the grey scale information
    sizeInBytes = width * height * sizeof(int);
    greyVals = malloc(sizeInBytes);

    /// read in the grey scale values

    fscanf(greyFile, "%s [", txt);
//    printf("%s \n", txt);

    pGreyVals = greyVals;
    for(i=0; i < height; i++)
    {
        for(j=0; j < width - 1; j++)
        {
            fscanf(greyFile, " %d,", pGreyVals++);
//            printf("%d ", inVal);
        }
        fscanf(greyFile, " %d;", pGreyVals++);
//        printf("%d;\n", inVal);
    }

//    for(i = 0; i < 4 * width; i++)
//    {
//        printf("%d, ", greyVals[i]);
//    }
//    printf("\n");


    /// Open the co processor
	int dd = coprthr_dopen(COPRTHR_DEVICE_E32,COPRTHR_O_THREAD);
    printf("epiphany dev:%i\n", dd);
	if (dd<0)
	{
        printf("Device open failed.\n");
        exit(0);
    }

    eGreyVals = coprthr_dmalloc(dd, sizeInBytes, 0);
    printf("writing gry levels to device\n");
    coprthr_dwrite(dd, eGreyVals, 0, (void*)greyVals, sizeInBytes, 0);
//    eCoreResults = coprthr_dmalloc(dd, (ECORES * GREYLEVELS * sizeof(int), 0); /// Output only

    pass_args args;
    args.width = width;
    args.height = height;
    args.g_result = (void*)coprthr_dmalloc(dd, (ECORES * GREYLEVELS * sizeof(int)), 0); /// Output only
//    args.debug = debug;

	coprthr_program_t prg = coprthr_cc_read_bin("./egdma.e32", 0);
    coprthr_sym_t krn = coprthr_getsym(prg, "k_scan");
    printf("calling scan\n");
    coprthr_event_t ev = coprthr_dexec(dd, ECORES, krn, (void*)(&args), COPRTHR_E_WAIT);

        printf("retrieving resutls\n");
    coprthr_dread(dd, args.g_result, 0, (void*)coreResults, ECORES * GREYLEVELS * sizeof(int), 0);

//    for(i=0;i<ECORES;i++)
//    {
        for (j=0;j<GREYLEVELS*4;j++)
            printf("%d, ", coreResults[i*GREYLEVELS + j]);
        printf("\n");
//    }

    /// tidy up
    coprthr_dfree(dd, eGreyVals);
    coprthr_dclose(dd);
    free(greyVals);
    exit(1);

}


