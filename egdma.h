#ifndef EGDMA_H_INCLUDED
#define EGDMA_H_INCLUDED

#define GREYLEVELS 256

typedef struct
{
    int width;      /// the width of the original picture
    int height;     /// the height of the original picture
    void * g_greyVals;
    void * g_result;
    int * debug;
} pass_args;

#endif // EGDMA_H_INCLUDED
