#include <stdio.h>
#include <coprthr.h>
#include <coprthr_cc.h>
#include <coprthr_thread.h>
#include <coprthr_mpi.h>

//#include <e-lib.h>

#define ECORES 16    /// Not great - CORECOUNT is defined as the same thing in ringTopo16.c

int main(int argc, char** argv)
{

    int width, height;
    int * greyVals;     /// allocated after the width and height are know
    char txt[10];
    int i, j;
    int * pGreyVals;

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

    greyVals = malloc(width * height * sizeof(int));

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

    for(i = 0; i < 4 * width; i++)
    {
        printf("%d, ", greyVals[i]);
    }
    printf("\n");


    /// Open the co processor
	int dd = coprthr_dopen(COPRTHR_DEVICE_E32,COPRTHR_O_THREAD);
    printf("epiphany dev:%i\n", dd);
	if (dd<0)
	{
        printf("Device open failed.\n");
        exit(0);
    }


    /// tidy up
    free(greyVals);
    exit(1);

}


