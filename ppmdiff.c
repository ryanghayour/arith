/* ppmdiff.c
 * Ryan Ghayour (rghayo1) and Mateo Hadeshian (mhades02)
 * 
 * help from Jack Burns and Max Mitchell
 * for testing
 * calculates the difference between two images
 */


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "assert.h"
#include "a2methods.h"
#include "a2plain.h"
#include "a2blocked.h"
#include "pnm.h"

#define TRUE 0
#define FALSE 1

#define SET_METHODS(METHODS, MAP, WHAT) do {                    \
        methods = (METHODS);                                    \
        assert(methods != NULL);                                \
                                                                \
} while (0)


/********** function prototypes **********/
void apply_root_mean_sqr_diff(int col, int row, A2Methods_UArray2 array2,
                              A2Methods_Object *ptr, void *cl);

/********** structs definitions **********/
typedef struct sumPpm {

    Pnm_ppm ppm;
    double sum;
    double otherDenom;

} *sumPpm;




int main(int argc, char *argv[])
{
    /* default to UArray2 methods */
    A2Methods_T methods = uarray2_methods_plain; 
    assert(methods);

    SET_METHODS(uarray2_methods_plain, map_row_major, 
                    "row-major");

    FILE *fp1 = NULL;
    FILE *fp2 = NULL;

    Pnm_ppm ppm1;
    Pnm_ppm ppm2;
    int stdinFree = TRUE;

    if (argc != 3) {
        fprintf(stderr, "ppmdiff: invalid use. %s takes 2 PPM images \
as arguments\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    for (int i = 1; i < argc; i++) {

        if (strcmp(argv[i], "-") == 0 && stdinFree) {
            fp1 = stdin;
            stdinFree = FALSE;
        }
        else if (fp2 == NULL) {
            fp2 = fopen(argv[i], "r");
            if (fp2 == NULL) {
                fprintf(stderr, "Failed opening one or more files.\n");
                assert(0);
            }
        }
        else if (fp1 == NULL) {
            fp1 = fopen(argv[i], "r");
            if (fp1 == NULL) {
                fprintf(stderr, "Failed opening one or more files.\n");
                assert(0);
            }
        }
    }
 
    ppm2 = Pnm_ppmread(fp2, methods);
    ppm1 = Pnm_ppmread(fp1, methods);


    int ppm1Width = ppm1->width;
    int ppm1Height = ppm1->height;

    int ppm2Width = ppm2->width;
    int ppm2Height = ppm2->height;

    if (abs(ppm1Width - ppm2Width) > 1 || 
        abs(ppm1Height - ppm2Height) > 1) {
        fprintf(stderr, "Files more than 1 pixel different \
in width/height.\n");
        printf("1.0\n");
        exit(EXIT_FAILURE);
    }

    struct sumPpm *total = malloc(sizeof(*total));
    assert(total != NULL);
    total->sum = 0;

    double diff = 0;

    if (ppm1Width <= ppm2Width || ppm1Height <= ppm2Height ) {
        total->ppm = ppm2;
        total->otherDenom = ppm1->denominator;
        methods->map_default(ppm1->pixels, apply_root_mean_sqr_diff, total);
        diff = total->sum / (double)(3.0 * ppm1Width * ppm1Height);

    }
    else {
        total->ppm = ppm1;
        total->otherDenom = ppm2->denominator;
        methods->map_default(ppm2->pixels, apply_root_mean_sqr_diff, total);
        diff = total->sum / (double)(3.0 * ppm2Width * ppm2Height);
    }

    double sq = sqrt(diff);

    printf("%lf\n", sq);


    free(total);
    Pnm_ppmfree(&ppm1);
    Pnm_ppmfree(&ppm2);

    //STUB: check what happens here when STDIN is used
    if (fp1 != NULL) {
        fclose(fp1);
    }
    if (fp2 != NULL) {
        fclose(fp2);
    }

    exit(EXIT_SUCCESS);
}



void apply_root_mean_sqr_diff(int col, int row, A2Methods_UArray2 array2,
                              A2Methods_Object *ptr, void *cl)
{
    (void) array2;

    struct sumPpm *total = cl;
    Pnm_ppm ppm = total->ppm;
    A2Methods_UArray2 pixels = ppm->pixels;
    Pnm_rgb pix1 = (Pnm_rgb)ptr;
    Pnm_rgb pix2 = (Pnm_rgb)(ppm->methods->at(pixels, col, row));

    double denom1 = total->otherDenom;
    double denom2 = ppm->denominator;

    double redDiff = (((double)pix1->red) / denom1) - (((double)pix2->red) / denom2);
    double blueDiff = (((double)pix1->blue) / denom1) - (((double)pix2->blue) / denom2);
    double greenDiff = (((double)pix1->green) / denom1) - (((double)pix2->green) / denom2);

    double redPow = redDiff * redDiff;
    double bluePow = blueDiff * blueDiff;
    double greenPow = greenDiff * greenDiff;

    total->sum += (redPow + bluePow + greenPow);
}
