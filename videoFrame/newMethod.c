// tutorial01.c
// Code based on a tutorial by Martin Bohme (boehme@inb.uni-luebeckREMOVETHIS.de)
// Tested on Gentoo, CVS version 5/01/07 compiled with GCC 4.1.1
// With updates from https://github.com/chelyaev/ffmpeg-tutorial
// Updates tested on:
// LAVC 54.59.100, LAVF 54.29.104, LSWS 2.1.101
// on GCC 4.7.2 in Debian February 2015
//
// Updates tested on:
// Mac OS X 10.11.6
// Apple LLVM version 8.0.0 (clang-800.0.38)
//
// A small sample program that shows how to use libavformat and libavcodec to read video from a file.
//
// Use
//
// $ gcc -o tutorial01 tutorial01.c -lavutil -lavformat -lavcodec -lswscale -lz -lm
//
// to build (assuming libavutil/libavformat/libavcodec/libswscale are correctly installed your system).
//
// Run using
//
// $ tutorial01 myvideofile.mpg
//
// to write the first five frames from "myvideofile.mpg" to disk in PPM format.

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

#include <stdio.h>
#include<math.h>

// compatibility with newer API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
	FILE *pFile;
	char szFilename[32];
	int y;

	// Open file.
	sprintf(szFilename, "frame%d.ppm", iFrame);
	pFile = fopen(szFilename, "wb");
	if (pFile == NULL) {
		return;
	}

	// Write header.
	fprintf(pFile, "P6\n%d %d\n255\n", width, height);

	// Write pixel data.
	for (y = 0; y < height; y++) {
		fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
	}

	// Close file.
	fclose(pFile);
}

int main(int argc, char *argv[]) {
	// Initalizing these to NULL prevents segfaults!
	AVFormatContext *pFormatCtx = NULL;
	int i, videoStream;
	AVCodecContext *pCodecCtxOrig = NULL;
	AVCodecContext *pCodecCtx = NULL;
	AVCodec *pCodec = NULL;
	AVFrame *pFrame = NULL;
	AVFrame *pFrameRGB = NULL;
	int numBytes;
	uint8_t *buffer = NULL;
	AVPacket packet;
	int frameFinished;
	double seconds;
	struct SwsContext *sws_ctx = NULL;

	if (argc < 2) {
		printf("Please provide a movie file\n");
		return -1;
	}


	seconds = atof((argv[2]));
	printf("seconds is %lf\n", seconds);

	// Register all formats and codecs.
	av_register_all();

	// Open video file.
	if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0) {
		return -1; // Couldn't open file.
	}

	// Retrieve stream information.
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
		return -1; // Couldn't find stream information.
	}

	// Dump information about file onto standard error.
	av_dump_format(pFormatCtx, 0, argv[1], 0);

	// Find the first video stream.
	videoStream = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++) {
		if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}
	}
	if (videoStream == -1) {
		return -1; // Didn't find a video stream.
	}

	// Get a pointer to the codec context for the video stream.
	pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;
	// Find the decoder for the video stream.
	pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
	if (pCodec == NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1; // Codec not found.
	}
	// Copy context.
	pCodecCtx = avcodec_alloc_context3(pCodec);
	if (avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
		fprintf(stderr, "Couldn't copy codec context");
		return -1; // Error copying codec context.
	}

	// Open codec.
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		return -1; // Could not open codec.
	}

	// Allocate video frame.
	pFrame = av_frame_alloc();

	// Allocate an AVFrame structure.
	pFrameRGB = av_frame_alloc();
	if (pFrameRGB == NULL) {
		return -1;
	}

	// Determine required buffer size and allocate buffer.
	//numBytes=avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height); // Deprecated.
	numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, 1);
	buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));

	// Assign appropriate parts of buffer to image planes in pFrameRGB Note that pFrameRGB is an AVFrame, but AVFrame is a superset of AVPicture
	//avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height); // Deprecated.
	av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, 1);

	// Initialize SWS context for software scaling.
	sws_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);

	// Read frames and save first five frames to disk.

	int stream_index = videoStream;
	int64_t seek_target= av_rescale_q(seconds * AV_TIME_BASE, AV_TIME_BASE_Q, pFormatCtx->streams[stream_index]->time_base);
	int64_t backbuff = av_rescale_q(1 * AV_TIME_BASE, AV_TIME_BASE_Q, pFormatCtx->streams[stream_index]->time_base);
	printf("seektime is %d, seek_target is %d, backbuff is %d\n", seek_target - backbuff + (int64_t)pFormatCtx->start_time / 1000, seek_target, backbuff);
	// printf("start_time, %lf\n", (double)pFormatCtx->start_time);
	 int ret = av_seek_frame(pFormatCtx, stream_index, seek_target - backbuff + (int64_t)pFormatCtx->start_time / 1000, AVSEEK_FLAG_BACKWARD);
	// int ret = av_seek_frame(pFormatCtx, -1, seconds * AV_TIME_BASE , AVSEEK_FLAG_ANY);
    if (ret < 0) {
        fprintf(stderr, "av_seek_frame failed");
        return 0;
    }

		int64_t key_frame_timestamp = 0;
		int64_t timestamp_per_frame = 0;
		int64_t frame_timestamp = 0;
		while (av_read_frame(pFormatCtx, &packet) >= 0) {
	    if (packet.stream_index == videoStream) {
					printf("1packet pts %d， flags %d, seek_target %d\n", packet.pts, packet.flags, seek_target);
	        if(packet.flags==1){
	            if(packet.pts>seek_target){
	                break;
	            }
	            key_frame_timestamp = packet.pts;
	        }
	        timestamp_per_frame = packet.pts - frame_timestamp;
	        frame_timestamp = packet.pts;
	    }
	    av_packet_unref(&packet);
	}

	if(key_frame_timestamp == pFormatCtx->streams[stream_index]->start_time){
    return -1;
	}


	int ret1 = av_seek_frame(pFormatCtx,
              stream_index,
              key_frame_timestamp- 2*timestamp_per_frame,
              AVSEEK_FLAG_BACKWARD);
	printf("the second seek, %d", ret1);

	i = 0;
	while (av_read_frame(pFormatCtx, &packet) >= 0) {
		// Is this a packet from the video stream?
		if (packet.stream_index == videoStream) {
			// Decode video frame
			// avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
			printf("2packet pts %d\n", packet.pts);
			int ret = avcodec_send_packet(pCodecCtx, &packet);
        if (ret < 0) {
            fprintf(stderr, "avcodec_send_packet");
            return -1;
        }
        ret = avcodec_receive_frame(pCodecCtx, pFrame);

			// Did we get a video frame?
			if (ret == 0 ) {
				// Convert the image from its native format to RGB.
				// sws_scale(sws_ctx, (uint8_t const * const *) pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);

				// Save the frame to disk.

				if (pFrame->pts >= seek_target) {

                sws_scale(sws_ctx, (uint8_t const * const *) pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
                //buffer为目标
                break;
            }
			}
		}

		// Free the packet that was allocated by av_read_frame.
		//av_free_packet(&packet); // Deprecated.
	}
	SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, ++i);
	av_packet_unref(&packet);


	// Free the RGB image.
	av_free(buffer);
	av_frame_free(&pFrameRGB);

	// Free the YUV frame.
	av_frame_free(&pFrame);

	// Close the codecs.
	avcodec_close(pCodecCtx);
	avcodec_close(pCodecCtxOrig);

	// Close the video file.
	avformat_close_input(&pFormatCtx);

	return 0;
}
