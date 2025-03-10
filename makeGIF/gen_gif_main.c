#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <emscripten.h>

#include <libavcodec/avcodec.h>

#include "gen_gif.h"
#include "log.h"

#define INBUF_SIZE (10<<20)
#define OUTBUF_SIZE (1<<20)

void Ffmpeglog(int l, char* t) {
    if (l <= 32) {
        fprintf(stdout, "%d\t%s\n", l, t);
    }
}

int main () {
    // av_register_all();
    fprintf(stdout, "ffmpeg init done\n");
    return 0;
}

EMSCRIPTEN_KEEPALIVE int makeGIF(char *filename, char *outfilename)
{
    FILE *f, *outfile;
    uint8_t* data, *outData;
    size_t   data_size;
    int outSize;

    set_log_callback();
    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }

    data = malloc(INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);
    /* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
    memset(data + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);

    outData = malloc(OUTBUF_SIZE);

    /* read raw data from the input file */
    data_size = fread(data, 1, INBUF_SIZE, f); // read 1 byte every time, ret = how many time, INBUF_SIZE = max time
    if (!data_size)
        exit(1);

    gen_gif(5, 0, data, data_size, outData, OUTBUF_SIZE, &outSize);

    outfile = fopen(outfilename, "wb");
    if (!outfile) {
        fprintf(stderr, "open file fail.%s", outfilename);
        exit(1);
    }
    fwrite(outData, 1, outSize, outfile);
    fclose(outfile);
    free(outData);
    free(data);
    fclose(f);
    return 0;
}

