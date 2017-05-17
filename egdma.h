#ifndef EGDMA_H_INCLUDED
#define EGDMA_H_INCLUDED

#define ECORES 16    /// Not great - Should be a function call
#define LASTCORENUM 15  /// the last core does the remainder at the end of the file
#define GRAYLEVELS 256
//#define TIMEHOST        /// wall clock host  v Epiphany
//#define TIMEEPIP      /// Epiphany internal

#define UseDMA

typedef struct
{
    int     width;           /// the width of the original picture
    int     height;          /// the height of the original picture
    size_t  szImageBuffer;   /// the size of the image buffer
    void *  g_grayVals;
    void *  g_result;
//    int * debug;
} scan_args;

typedef struct
{
    int     width;           /// the width of the original picture
    int     height;           /// the height of the original picture
    size_t  szImageBuffer;   /// the size of the image buffer
    void *  g_map;
    void *  g_grayVals;
//    int * debug;
} map_args;

#endif // EGDMA_H_INCLUDED
