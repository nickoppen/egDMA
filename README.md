# egDMA
Examples of Parallella DMA operations.

This program takes a text file of gray values from an image and performs histogram equalisation on it, producing another text file. There are two versions, one using DMA and one using memcpy. To use DMA, compile the source with the precompiler variable UseDMA set. Leave it unset to use memcpy.

## Usage:

Create a text file of your uncompressed grayscale image using gray2text.x in the grayConvert repository. The csv files in this repository are examples of converted images.

```
$./egdma <text file> -o <output text file>
```

Reconvert the image to the equalised verions using text2gray.x in the grayConvert repository.
