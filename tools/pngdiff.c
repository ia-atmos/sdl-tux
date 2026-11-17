// Compares two screenshots of the same size: how many pixels differ by
// more than a tolerance in some channel, and, optionally, an image of the
// differences (the second image, darkened, with the differing pixels in
// red).
//
//      pngdiff a.png b.png [tolerance [diff.png]]
//
// With PNGDIFF_SHOW=n, it also prints the first n differing pixels.
//
// Prints "<differing pixels> <total> <percent>", and exits with 0 if no
// pixel differs, 1 otherwise, 2 on error.
//
//      cc -O2 tools/pngdiff.c -lSDL2 -lSDL2_image -o out/pngdiff

#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

static SDL_Surface* load (const char* path) {
    SDL_Surface* raw = IMG_Load(path);
    if (raw == NULL) {
        fprintf(stderr, "pngdiff : %s\n", IMG_GetError());
        return NULL;
    }
    SDL_Surface* sfc = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(raw);
    return sfc;
}

int main (int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: pngdiff a.png b.png [tolerance [diff.png]]\n");
        return 2;
    }
    int tol = (argc > 3) ? atoi(argv[3]) : 0;
    SDL_Surface* a = load(argv[1]);
    SDL_Surface* b = load(argv[2]);
    if (a == NULL || b == NULL) return 2;
    if (a->w != b->w || a->h != b->h) {
        printf("size %dx%d %dx%d\n", a->w, a->h, b->w, b->h);
        return 1;
    }
    long n = 0;
    for (int y=0; y<a->h; y++) {
        Uint8* pa = (Uint8*) a->pixels + y*a->pitch;
        Uint8* pb = (Uint8*) b->pixels + y*b->pitch;
        for (int x=0; x<a->w; x++, pa+=4, pb+=4) {
            int d = 0;
            for (int c=0; c<3; c++) {
                int e = abs(pa[c] - pb[c]);
                if (e > d) d = e;
            }
            if (d > tol) {
                if (getenv("PNGDIFF_SHOW") && n < atoi(getenv("PNGDIFF_SHOW"))) {
                    printf("(%d,%d) %d,%d,%d %d,%d,%d\n", x, y, pa[0], pa[1], pa[2], pb[0], pb[1], pb[2]);
                }
                n++;
                pb[0] = 255; pb[1] = 0; pb[2] = 0;
            } else {
                pb[0] /= 3; pb[1] /= 3; pb[2] /= 3;
            }
            pb[3] = 255;
        }
    }
    long total = (long) a->w * a->h;
    printf("%ld %ld %.2f%%\n", n, total, 100.0 * n / total);
    if (argc > 4) {
        IMG_SavePNG(b, argv[4]);
    }
    return n ? 1 : 0;
}
