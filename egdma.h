#ifndef EGDMA_H_INCLUDED
#define EGDMA_H_INCLUDED

#define ECORES 16    /// Not great - Should be a function call
#define LASTCORENUM 15  /// the last core does the remainder at the end of the file
#define GREYLEVELS 256
//#define TIMEIT 1        /// host scan
//#define TIMEIT 2      /// Epiphany scan
//#define TIMEIT 3      /// host map
//#define TIMEIT 4      /// Epiphany map
#define SHARED_RAM (0x01000000)

typedef struct
{
    int     width;           /// the width of the original picture
    int     height;          /// the height of the original picture
    size_t  szImageBuffer;   /// the size of the image buffer
    void *  g_greyVals;
    void *  g_result;
//    int * debug;
} scan_args;

typedef struct
{
    int     width;           /// the width of the original picture
    int     height;           /// the height of the original picture
    size_t  szImageBuffer;   /// the size of the image buffer
    void *  g_map;
    void *  g_greyVals;
//    void * g_mappedVals;
//    int * debug;
} map_args;

#endif // EGDMA_H_INCLUDED
