/* Mateo Hadeshian and Ryan Ghayour
 * (mhades02 and rghayo01)
 * 5 March 2020
 * bitpack.c
 */

#include "bitpack.h"
#include <math.h>
#include <stdio.h>

// Name: fitsu
// Parameters: uint64 word, width
// Returns: true/false
// Does: decides if unsigned int will fit in declared width
bool Bitpack_fitsu(uint64_t n, unsigned width){

    uint64_t max = 1;
    max = max << width;

    return (n < max);
}

// Name: fitss
// Parameters: int64 word, width
// Returns: true/false
// Does: decides if signed word will fit in desired width
bool Bitpack_fitss(int64_t n, unsigned width){

    int64_t max = 1;
    max = max << (width - 1);

       return (n < max && n >= -max);//NO POW
}

// Name: getu
// Parameters: uint64 word, unsigned width and lsb
// Returns: uint64 word
// Does: returns bits at (64 - lsb - width) to (64 - lsb) as uint
uint64_t Bitpack_getu(uint64_t word, unsigned width, unsigned lsb){
    uint64_t output = word;

    output = output << (64 - lsb - width);
    output = output >> (64 - width);
    
    return output;
}

// Name: gets
// Parameters: uint64 word, unsigned width and lsb
// Returns: signed int
// Does: returns bits at (64 - lsb - width) to (64 - lsb) as signed int
int64_t Bitpack_gets(uint64_t word, unsigned width, unsigned lsb){

     int64_t output = word;
     
     output = output << (64 - lsb - width);

     output = output >> (64 - width);
     
     return output;
 }

// Name: newu
// Parameters: uint64_t word, unsigned width, unsigned lsb, uint64_t value
// Returns: uint64 word
// Does: replaces the bits from (64-lsb-width) to (64-lsb) to unsigned value
uint64_t Bitpack_newu(uint64_t word, 
                      unsigned width, 
                      unsigned lsb, 
                      uint64_t value){

    uint64_t mask = value;
    mask = mask << lsb;

    uint64_t left = word;
    left = left >> (lsb + width);
    left = left << (lsb + width);

    uint64_t right = word;
    right = right << (64 - lsb);
    right = right >> (64 - lsb);

    uint64_t output = (left | right);

    return (output | mask);
}

// Name: newu
// Parameters: uint64_t word, unsigned width, unsigned lsb, int64_t value
// Returns: uint64 word
// Does: replaces the bits from (64 - lsb - width) to (64-lsb) to signed value
uint64_t Bitpack_news(uint64_t word,
                      unsigned width, 
                      unsigned lsb,  
                      int64_t value){

    int64_t mask = value;
    mask = mask << lsb;

    uint64_t left = word;
    left = left >> (lsb + width);
    left = left << (lsb + width);

    uint64_t right = word;
    right = right << (64 - lsb);
    right = right >> (64 - lsb);

    uint64_t output = (left | right);

    uint64_t finalMask = 0;
    finalMask = finalMask | mask;
    
    finalMask = finalMask << (64 - width - lsb);
    finalMask = finalMask >> (64 - width - lsb);

    return (output | finalMask);
}

