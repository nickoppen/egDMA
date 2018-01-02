# egDMA
Examples of Parallella DMA operations.

This program takes a text file of gray values from an image and performs histogram equalisation on it, producing another text file. There are two versions, one using DMA and one using memcpy. To use DMA, compile the source with the precompiler variable UseDMA set. Leave it unset to use memcpy.

If you have a gray scale image that you want to test it out on, have a look at https://github.com/nickoppen/grayConvert where I have a short program that converst 256 gray-scale images into comma separated text files for input into this program. WARNING: the behaviour is a little eratic, the DMA channels get blocked and hang.

## Usage:

Create a text file of your uncompressed grayscale image using gray2text.x in the grayConvert repository. The csv files in this repository are examples of converted images.

```
$./egdma.x <input text file> -o <output text file>
```

Reconvert the image to the equalised verions using text2gray.x in the grayConvert repository.
