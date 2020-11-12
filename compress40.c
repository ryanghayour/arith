/* Mateo Hadeshian and Ryan Ghayour
 * (mhades02 and rghayo01)
 * 5 March 2020
 * compress40.c
 */

#include "compress40.h"
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "uarray2.h"
#include "pnm.h"
#include "a2plain.h"
#include "a2methods.h"
#include "arith40.h"
#include "bitpack.h"

typedef void *A2;

// The chroma struct is a struct similar to a ppm struct, with a width, height,
// and denominator. It also contains an A2, for which each element contains 
// either a chroma_pixel or an int_chroma_pixel
typedef struct chroma{
    unsigned width, height, denominator;

    A2Methods_UArray2 pixels;
    const struct A2Methods_T *methods;
} *chroma;

// The int_chroma_pixel is a struct that is stored in the A2 of a chroma struct
// which contains unsigned and signed representations of a, b, c, d, pb and pr.
// This is struct that makes moving between our "Int->Float / Float->Int" and 
// bitpacking/unpacking steps more simple.
typedef struct int_chroma_pixel{
    uint64_t a, pb, pr;
    int64_t b, c, d;
} *int_chroma_pixel;


// The chroma_pixel struct contains floating point representations of y, pr,
// and pb, which is used in the transfer between the RGB <-> component and
// int <-> float steps. It is essentially a component video representation of
// the data.
typedef struct chroma_pixel{
    float y, pr, pb;

} *chroma_pixel;



void trim(Pnm_ppm image); // for compression
                          // makes height and width even

float quantizehalf(float in); // returns the input after clamped to [-.5, .5]
int quantizebcd(float in); // clamps in to [-.3, .3] then multiplies it by 50
float quantize_one(float in); // returns the input after clamped to [-1, 1]

chroma compressToChroma(Pnm_ppm src); // converts src to chroma and frees src
Pnm_ppm decompressToPpm(chroma src); // converts src to pnm_ppm and frees src
void freeChroma(chroma c); // frees all memory allocated for any chroma struct

void applyFloatToInt(int i , int j, A2 array, A2Methods_Object *elem, 
                     void *src);
void applyChromaToBit(int i, int j, A2 array, A2Methods_Object *elem,
                      void *src);

void applyBitToChroma(int i, int j, A2 src, A2Methods_Object *elem, 
                      void *picture);
void applyIntToFloat(int i , int j, A2 src, A2Methods_Object *elem, 
                     void *picture);

void printBits(A2 bits);
void applyPrint(int i, int j, A2 src, A2Methods_Object *elem, 
                     void *cl);

// Name: compress40
// Parameters: FILE pointer to ppm file
// Returns: None
// Does: Compresses a pnm_ppm image to a Comp40 bitmap file and prints the 
//          bitmap to stdout.
void compress40(FILE *input){
    A2Methods_T methods = uarray2_methods_plain;
    Pnm_ppm OG_image = Pnm_ppmread(input, methods);

    trim(OG_image);

    unsigned width = OG_image->width;
    unsigned height = OG_image->height;
    unsigned denominator = OG_image->denominator;

    chroma componentVideo = compressToChroma(OG_image); // convert to chroma
                                                        // struct (float)

    chroma unpackedBits = malloc(sizeof(struct chroma));

    unpackedBits->pixels = 
        methods->new(width / 2, height / 2, sizeof(struct int_chroma_pixel));
    unpackedBits->width = width/2;
    unpackedBits->height = height/2;
    unpackedBits->denominator = denominator;
    unpackedBits->methods = methods;

    methods->map_default(unpackedBits->pixels, 
        applyFloatToInt, componentVideo); 
    freeChroma(componentVideo);
    // conversion from component to a, b, c, d, pb, pr.

    A2 bits = methods->new(width/2, height/2, sizeof(uint64_t));

    methods->map_default(bits, applyChromaToBit, unpackedBits); // fill bits
    freeChroma(unpackedBits);

    printBits(bits);
    methods->free(&bits);
}

// Name: decompress40
// Parameters: File pointer to comp40 bitmap file
// Returns: None
// Does: Decompresses a comp40 bitmap to a pnm_ppm file, prints ppm to stdout
void decompress40(FILE *input){

    unsigned denominator = 255;
    unsigned height, width;
    int read = 
    fscanf(input, "COMP40 Compressed image format 2\n%u %u", &width, &height);
    assert(read == 2);
    int c = getc(input);
    assert(c == '\n');

    A2Methods_T methods = uarray2_methods_plain;

    height /= 2;
    width /= 2;

    A2 bits = methods->new(width, height, sizeof(uint64_t));

    ///////////////////////////////////////////////////////////////
    // fill bits from file                                       //
    ///////////////////////////////////////////////////////////////

    uint64_t word;
    uint64_t storage;
    for (unsigned i = 0; i < height; i++){
        for (unsigned j = 0; j < width; j++){
            word = 0;
            for (int k = 0; k < 4; k++){
                storage = getc(input);
                word = Bitpack_newu(word, 8, 8 * (3 - k), storage);
            }
            *(uint64_t*)methods->at(bits, j, i) = word;
        }
    }

    ////////////////////////////////////////////////////////////////

    chroma unpackedBits = malloc(sizeof(struct chroma));
    unpackedBits->pixels = 
        methods->new(width, height, sizeof(struct int_chroma_pixel)); 
    unpackedBits->methods = methods;
    unpackedBits->width = width;
    unpackedBits->height = height;
    unpackedBits->denominator = denominator;
    
    methods->map_default(bits, applyBitToChroma, unpackedBits); // unpacks bits
    methods->free(&bits);                                    // int int_chromas

    chroma componentVideo = malloc(sizeof(struct chroma));
    componentVideo->pixels = 
        methods->new(width * 2, height * 2, sizeof(struct chroma_pixel));
    componentVideo->width = width * 2;
    componentVideo->height = height * 2;
    componentVideo->denominator = denominator;
    componentVideo->methods = methods;

    methods->map_default(unpackedBits->pixels, 
    applyIntToFloat, componentVideo); // fill component
    freeChroma(unpackedBits);

    Pnm_ppm image = decompressToPpm(componentVideo); 

    Pnm_ppmwrite(stdout, image);
    Pnm_ppmfree(&image);
}

// Name: trim
// Parameters: pointer to a pnm_ppm struct
// Returns: none
// Does: if either width or height are odd, copies A2 struct into a new A2
//       with even width and height
      
void trim(Pnm_ppm image){
    
    unsigned x = image->width % 2;
    unsigned y = image->height % 2;

// If no trimming needs to be done:
    if (x == 0 && y == 0) return;

// Creating a new A2 that will contain the trimmed image
    A2 trimmed = image->methods->new(image->width - x, 
        image->height - y, 
        sizeof(struct Pnm_rgb));

// Copying data to new A2, without copying extra row/column
    for (unsigned i = 0; i < image->height - y; i++){
        for (unsigned j = 0; j < image->width - x; j++){
            *(Pnm_rgb)image->methods->at(trimmed, j, i) = 
                *(Pnm_rgb)image->methods->at(image->pixels, j, i);
        }
    }

// Updating height and witdth in struct
    image->height -= y;
    image->width -= x;

    image->methods->free(&image->pixels);
    image->pixels = trimmed;
}

// Name: compressToChroma
// Parameters: pointer to a pnm_ppm struct
// Returns: Pointer to a chroma struct containing A2 of chroma_pixels
// Does: converts unsigned r g b values to component video floats and returns
//       a struct containing those values
chroma compressToChroma(Pnm_ppm src){
    
// Here we create an instance of a chroma struct, copying basic data (not the
// inner A2 yet)

    chroma image = malloc(sizeof(struct chroma));
    image->pixels = 
    src->methods->new(src->width, src->height, sizeof(struct chroma_pixel));
    image->width = src->width;
    image->height = src->height;
    image->methods = src->methods;
    image->denominator = src->denominator;

    float red, green, blue;

// For each pixel within the A2, update the y, pb, and pr values of the chroma
// struct using the equations given the r g b and denominator values at that  
// point
    for (unsigned i = 0; i < image->width; i++){
        for (unsigned j = 0; j < image->height; j++){

            red = ((Pnm_rgb)src->methods->at(src->pixels, i, j))->red / 
            (float)src->denominator;
            green = ((Pnm_rgb)src->methods->at(src->pixels, i, j))->green / 
            (float)src->denominator;
            blue = ((Pnm_rgb)src->methods->at(src->pixels, i, j))->blue / 
            (float)src->denominator;

            red = quantize_one(red);
            blue = quantize_one(blue);
            green = quantize_one(green);

            ((chroma_pixel)image->methods->at(image->pixels, i, j))->y = 
                quantize_one(0.299*red + 0.587*green + 0.114*blue);
            

            ((chroma_pixel)image->methods->at(image->pixels, i, j))->pb = 
                quantizehalf(-0.168736*red - 0.331264*green + 0.5*blue);


            ((chroma_pixel)image->methods->at(image->pixels, i, j))->pr = 
                quantizehalf(0.5*red - 0.418688*green - 0.081312*blue);
        }
    }

    Pnm_ppmfree(&src);
    return image;
}

// Name: decompressToPpm
// Parameters: chroma struct containing floating point representation of
//             component video values
// Returns: pointer to Pnm_ppm struct
// Does: Converts component video values to rgb values in a ppm struct.
Pnm_ppm decompressToPpm(chroma src){

    Pnm_ppm image = malloc(sizeof(struct Pnm_ppm));
    image->pixels = 
        src->methods->new(src->width, src->height, sizeof(struct Pnm_rgb));
    image->width = src->width;
    image->height = src->height;
    image->methods = src->methods;
    image->denominator = src->denominator;

    float y, pb, pr;
    int red, green, blue;

    // transform to ppm format and copy into new UArray2 in chroma 
    for (unsigned i = 0; i < image->width; i++){
        for (unsigned j = 0; j < image->height; j++){
            y = ((chroma_pixel)src->methods->at(src->pixels, i, j))->y;
            pr = ((chroma_pixel)src->methods->at(src->pixels, i, j))->pr;
            pb = ((chroma_pixel)src->methods->at(src->pixels, i, j))->pb;

            red = (int)((1.0 * y + 0.0 * pb + 1.402 * pr) * src->denominator);
            if (red < 0) red = 0;
            if (red > (int)image->denominator){
                red = (int)image->denominator;
            }
            ((Pnm_rgb)image->methods->at(image->pixels, i, j))->red = red;
            

            blue = (int)((1.0 * y + 1.772 * pb + 0.0 * pr) * src->denominator);
            if (blue < 0) blue = 0;
            if (blue > (int)image->denominator){
                blue = (int)image->denominator;
            }
            ((Pnm_rgb)image->methods->at(image->pixels, i, j))->blue = blue;


            green = (int)((1.0 * y - 0.344136 * pb - 0.714136 * pr)*src->denominator);
            if (green < 0) green = 0;
            if (green > (int)image->denominator){
                green = (int)image->denominator;
            }

            ((Pnm_rgb)image->methods->at(image->pixels, i, j))->green = green;
        }
    }

    freeChroma(src);
    return image;
}

// Name: freeChroma
// Parameters: pointer to chroma struct
// Returns: none
// Does: frees data associated with chroma struct
void freeChroma(chroma c){
    if (c != NULL && c->pixels != NULL)
        c->methods->free(&(c->pixels));
    free(c);
}

// Name: applyFloatToInt
// Parameters: position ints i and j, A2 struct of int_chroma_pixels, current 
//             int_chroma_pixel element in A2, source struct of chroma_pixels         
// Returns: None
// Does: Calculates every int_chroma_pixel value based on component floats and
//      implements them into the corresponding values to the given chroma_pixel
void applyFloatToInt(int i , int j, A2 array, A2Methods_Object *elem,
                     void *src){
    (void)array;
    A2Methods_T methods = uarray2_methods_plain;

    // extract values from 2x2 block of pixels
    float y1 = ((chroma_pixel)methods->at(((chroma)src)->pixels, 
    2*i, 2*j))->y;
    float y2 = ((chroma_pixel)methods->at(((chroma)src)->pixels, 
    2*i + 1, 2*j))->y;
    float y3 = ((chroma_pixel)methods->at(((chroma)src)->pixels, 
    2*i, 2*j + 1))->y;
    float y4 = ((chroma_pixel)methods->at(((chroma)src)->pixels, 
    2*i + 1, 2*j + 1))->y;

    // convert to abcd averages.
    float a_raw = quantize_one((y4 + y3 + y2 + y1)/4.0);
    float b_raw = (y4 + y3 - y2 - y1)/4.0;
    float c_raw = (y4 - y3 + y2 - y1)/4.0;
    float d_raw = (y4 - y3 - y2 + y1)/4.0;

    // quantize b, c, and d to [-.3, .3] and multiply by 50.
    // cast abcd to ints (round down)
    uint64_t a = (int)(a_raw * 63);
    int64_t b = quantizebcd(b_raw);
    int64_t c = quantizebcd(c_raw);
    int64_t d = quantizebcd(d_raw);

    // averages pb values of block and stores in temp float
    float pbtemp = 
    (((chroma_pixel)methods->at(((chroma)src)->pixels, 
    2*i, 2*j))->pb + 
    ((chroma_pixel)methods->at(((chroma)src)->pixels, 
    2*i + 1, 2*j))->pb + 
    ((chroma_pixel)methods->at(((chroma)src)->pixels, 
    2*i, 2*j + 1))->pb + 
    ((chroma_pixel)methods->at(((chroma)src)->pixels, 
    2*i + 1, 2*j + 1))->pb)/4.0;

    float prtemp = 
    (((chroma_pixel)methods->at(((chroma)src)->pixels, 
    2*i, 2*j))->pr + 
    ((chroma_pixel)methods->at(((chroma)src)->pixels, 
    2*i + 1, 2*j))->pr + 
    ((chroma_pixel)methods->at(((chroma)src)->pixels, 
    2*i, 2*j + 1))->pr + 
    ((chroma_pixel)methods->at(((chroma)src)->pixels, 
    2*i + 1, 2*j + 1))->pr)/4.0;

    // converts temp values to ints
    uint64_t pb = Arith40_index_of_chroma(pbtemp);
    uint64_t pr = Arith40_index_of_chroma(prtemp);

    // store converted integer values
    ((int_chroma_pixel)elem)->a = a;
    ((int_chroma_pixel)elem)->b = b;
    ((int_chroma_pixel)elem)->c = c;
    ((int_chroma_pixel)elem)->d = d;
    ((int_chroma_pixel)elem)->pb = pb;
    ((int_chroma_pixel)elem)->pr = pr;
}


// Name: applyChromaToBit
// Parameters: position ints i and j, empty A2 of 32 bit words, current 32 bit
//.            word, source int_chroma struct
// Returns: None
// Does: Condenses values from each int_chroma_pixel into a 32-bit word and 
//.      puts them in the corresponding spot in an A2 array.
void applyChromaToBit(int i, int j, A2 array, A2Methods_Object *elem, 
                      void *src){

    (void)array;
    uint64_t word = 0;

    A2Methods_T methods = uarray2_methods_plain;

    // extract int values from elem
    uint64_t a = 
    ((int_chroma_pixel)methods->at(((chroma)src)->pixels, i, j))->a;
    int64_t b = 
    ((int_chroma_pixel)methods->at(((chroma)src)->pixels, i, j))->b;
    int64_t c = 
    ((int_chroma_pixel)methods->at(((chroma)src)->pixels, i, j))->c;
    int64_t d = 
    ((int_chroma_pixel)methods->at(((chroma)src)->pixels, i, j))->d;
    uint64_t pb = 
    ((int_chroma_pixel)methods->at(((chroma)src)->pixels, i, j))->pb;
    uint64_t pr = 
    ((int_chroma_pixel)methods->at(((chroma)src)->pixels, i, j))->pr;

    // pack ints into 32bit word
    word = Bitpack_newu(word, 6, 26, a);
    word = Bitpack_news(word, 6, 20, b);
    word = Bitpack_news(word, 6, 14, c);
    word = Bitpack_news(word, 6, 8, d);
    word = Bitpack_newu(word, 4, 4, pb);
    word = Bitpack_newu(word, 4, 0, pr);

    *(uint64_t*)elem = word;
}

// Name: applyBitToChroma
// Parameters: position ints i and j, source 32 bit array, current 32 bit 
//.            element, empty chroma struct of int_chroma_pixels
// Returns: None
// Does: Unpacks 32 bit words into a, b, c, d, pb, and pr values for int_chroma
//.      struct.
void applyBitToChroma(int i, int j, A2 src, A2Methods_Object *elem,
                      void *picture){
    
    (void)src;
    A2Methods_T methods = uarray2_methods_plain;

    // separate 32bit words into 6 int values
    uint64_t a_packed = Bitpack_getu(*(uint64_t*)elem, 6, 26);
    int64_t b_packed = Bitpack_gets(*(uint64_t*)elem, 6, 20);
    // The *(uint*) might need to be a *(int*)
    int64_t c_packed = Bitpack_gets(*(uint64_t*)elem, 6, 14);
    int64_t d_packed = Bitpack_gets(*(uint64_t*)elem, 6, 8);
    uint64_t pb_index = Bitpack_getu(*(uint64_t*)elem, 4, 4);
    uint64_t pr_index = Bitpack_getu(*(uint64_t*)elem, 4, 0);
    
    // store extracted values into int_chroma_pixels
    ((int_chroma_pixel)methods->at(((chroma)picture)->pixels, i, j))->a = 
    a_packed;
    ((int_chroma_pixel)methods->at(((chroma)picture)->pixels, i, j))->b = 
    b_packed;
    ((int_chroma_pixel)methods->at(((chroma)picture)->pixels, i, j))->c = 
    c_packed;
    ((int_chroma_pixel)methods->at(((chroma)picture)->pixels, i, j))->d = 
    d_packed;
    ((int_chroma_pixel)methods->at(((chroma)picture)->pixels, i, j))->pb = 
    pb_index;
    ((int_chroma_pixel)methods->at(((chroma)picture)->pixels, i, j))->pr = 
    pr_index;

}

// Name: applyIntToFloat
// Parameters: position ints i and j, Chroma struct of int_chroma_pixels, 
//.            current element in struct, empty chroma struct of chroma_pixels
// Returns: None
// Does: Converts each int value in the int_chroma_pixel to corresponding
//.      floating point values in chroma_pixel struct.
void applyIntToFloat(int i , int j, A2 src, A2Methods_Object *elem,
                     void *picture){

    (void)src;
    A2Methods_T methods = uarray2_methods_plain;

    // extreact ints from elem
    uint64_t a_packed = ((int_chroma_pixel)elem)->a;
    int64_t b_packed = ((int_chroma_pixel)elem)->b;
    int64_t c_packed = ((int_chroma_pixel)elem)->c;
    int64_t d_packed = ((int_chroma_pixel)elem)->d;
    uint64_t pb_index = ((int_chroma_pixel)elem)->pb;
    uint64_t pr_index = ((int_chroma_pixel)elem)->pr;

    // convert pb & pr to floats and clamp to [-.5, .5]
    float pr = quantizehalf(Arith40_chroma_of_index(pr_index));
    float pb = quantizehalf(Arith40_chroma_of_index(pb_index));

    // convert ints abcd to respective floats
    float a = a_packed;
    a /= 63;
    a = quantize_one(a);

    float b = b_packed;
    b /= 100;
    if (b < -.31) b = -.31;
    if (b > .31) b = .31;

    float c = c_packed;
    c /= 100;
    if (c < -.31) c = -.31;
    if (c > .31) c = .31;

    float d = d_packed;
    d /= 100;
    if (d < -.31) d = -.31;
    if (d > .31) d = .31;

    // convert floats to 4 respective pixel values
    float y1 = quantize_one(a - b - c + d);
    float y2 = quantize_one(a - b + c - d);
    float y3 = quantize_one(a + b - c - d);
    float y4 = quantize_one(a + b + c + d);

    // store computed values into chroma_pixels
    ((chroma_pixel)methods->at(((chroma)picture)->pixels,
     2 * i, 2 * j))->y = y1;
    ((chroma_pixel)methods->at(((chroma)picture)->pixels,
     2 * i + 1, 2 * j))->y = y2;
    ((chroma_pixel)methods->at(((chroma)picture)->pixels,
     2 * i, 2 * j + 1))->y = y3;
    ((chroma_pixel)methods->at(((chroma)picture)->pixels,
     2 * i + 1, 2 * j + 1))->y = y4;

    ((chroma_pixel)methods->at(((chroma)picture)->pixels,
     2 * i, 2 * j))->pb = pb;
    ((chroma_pixel)methods->at(((chroma)picture)->pixels,
     2 * i + 1, 2 * j))->pb = pb;
    ((chroma_pixel)methods->at(((chroma)picture)->pixels,
     2 * i, 2 * j + 1))->pb = pb;
    ((chroma_pixel)methods->at(((chroma)picture)->pixels,
     2 * i + 1, 2 * j + 1))->pb = pb;

    ((chroma_pixel)methods->at(((chroma)picture)->pixels,
     2 * i, 2 * j))->pr = pr;
    ((chroma_pixel)methods->at(((chroma)picture)->pixels,
     2 * i + 1, 2 * j))->pr = pr;
    ((chroma_pixel)methods->at(((chroma)picture)->pixels,
     2 * i, 2 * j + 1))->pr = pr;
    ((chroma_pixel)methods->at(((chroma)picture)->pixels,
     2 * i + 1, 2 * j + 1))->pr = pr;
}

// Name: quantize_one
// Parameters: float
// Returns: float
// Does: clamps to 0 - 1 range
float quantize_one(float in){
    if (in > 1) in = 1.0;
    if (in < 0) in = 0.0;
    return in;
}

// Name: quantizebcd
// Parameters: float
// Returns: int
// Does: clamps to -0.3 0.3 range, multiplies by 50 and converts to int
int quantizebcd(float in){
    if (in > 0.31) in = 0.31;
    if (in < -0.31) in = -0.31;
    in *= 100;
    return (int)in;
}

// Name: quantize half
// Parameters: float
// Returns: float
// Does: clamps value to -0.5 0.5 range
float quantizehalf(float in){
    if (in > 0.5) in = 0.5;
    if (in < -0.5) in = -0.5;
    return in;
}

// Name: printBits
// Parameters: A2 array of 32 bit words
// Returns: none
// Does: prints each bit to stdout by calling applyPrint on each element.
void printBits(A2 bits){
    A2Methods_T methods = uarray2_methods_plain;

    unsigned width = methods->width(bits);
    unsigned height = methods->height(bits);

    assert(width && height);

    fprintf(stdout, "COMP40 Compressed image format 2\n%u %u\n", 
        width * 2, height * 2);

    bool OK = true;
    methods->map_row_major(bits, applyPrint, &OK);
}

// Name: applyPrint
// Parameters used: current element in A2 array
// Returns: none
// Does: prints 32 bit word as 4 separate 8 bit characters in big-endian order
void applyPrint(int i, int j, A2 src, A2Methods_Object *elem, 
                     void *cl){
    (void)j;
    (void)src;
    (void)cl;
    (void)i;

    for (int k = 24; k >= 0; k = k - 8){
        uint64_t c = Bitpack_getu(*(uint64_t*)elem, 8, k);
        putchar(c);
    }
}

