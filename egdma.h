#ifndef EGDMA_H_INCLUDED
#define EGDMA_H_INCLUDED

#define ECORES 16    /// Not great - Should be a function call
#define LASTCORENUM 15  /// the last core does the remainder at the end of the file
#define GREYLEVELS 256
#define TIMEIT 1

typedef struct
{
    int width;      /// the width of the original picture
    int height;     /// the height of the original picture
    void * g_greyVals;
    void * g_result;
    int * debug;
} pass_args;

#endif // EGDMA_H_INCLUDED
