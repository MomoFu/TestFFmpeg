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
#include <emscripten.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <jpeglib.h>
#include <stdio.h>
#include <math.h>

// compatibility with newer API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) ;
void saveFrame_jpg(uint8_t *pRGBBuffer, int iFrame, int width, int height); 
int main () {
    av_register_all();
    fprintf(stdout, "ffmpeg init done\n");
    return 0;
}

void saveFrame_jpg(uint8_t *pRGBBuffer, int iFrame, int width, int height)  {  
  
    struct jpeg_compress_struct cinfo;  
  
    struct jpeg_error_mgr jerr;  
  
    char szFilename[1024];  
    int row_stride;  
  
    FILE *fp;  
  
    JSAMPROW row_pointer[1];   // 一行位图  
  
    cinfo.err = jpeg_std_error(&jerr);  
  
    jpeg_create_compress(&cinfo);  
  
      
    sprintf(szFilename, "result%d.jpg", iFrame);//图片名字为视频名+号码  
    fp = fopen(szFilename, "wb");  
      
    if(fp == NULL)  
            return;  
  
    jpeg_stdio_dest(&cinfo, fp);  
  
    cinfo.image_width = width;    // 为图的宽和高，单位为像素   
    cinfo.image_height = height;  
    cinfo.input_components = 3;   // 在此为1,表示灰度图， 如果是彩色位图，则为3   
    cinfo.in_color_space = JCS_RGB; //JCS_GRAYSCALE表示灰度图，JCS_RGB表示彩色图像  
  
    jpeg_set_defaults(&cinfo);   
    jpeg_set_quality (&cinfo, 80, 1);  
  
    jpeg_start_compress(&cinfo, TRUE);  
  
      
    row_stride = cinfo.image_width * 3;//每一行的字节数,如果不是索引图,此处需要乘以3  
  
    // 对每一行进行压缩  
    while (cinfo.next_scanline < cinfo.image_height) {  
        row_pointer[0] = &(pRGBBuffer[cinfo.next_scanline * row_stride]);  
        jpeg_write_scanlines(&cinfo, row_pointer, 1);  
    }  
  
    jpeg_finish_compress(&cinfo);  
    jpeg_destroy_compress(&cinfo);  
  
    fclose(fp);  
}  


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


 //这个宏表示这个函数要作为导出的函数
EMSCRIPTEN_KEEPALIVE int seekseek( char *fileName, double timestamp) {
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


	seconds = timestamp;
	printf("seconds is %lf\n", seconds);

	// Register all formats and codecs.

	// Open video file.
	if (avformat_open_input(&pFormatCtx, fileName, NULL, NULL) != 0) {
		return -1; // Couldn't open file.
	}

	// Retrieve stream information.
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
		return -1; // Couldn't find stream information.
	}

	// Dump information about file onto standard error.
	av_dump_format(pFormatCtx, 0, fileName, 0);

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
	printf("start time is %lf, seek_target is %d , seconds is %d, AV_TIME_BASE_Q is %d,AV_TIME_BASE is %d, time_base is %d\n", (double)pFormatCtx->start_time, seek_target, seconds, AV_TIME_BASE_Q, AV_TIME_BASE, pFormatCtx->streams[stream_index]->time_base);
	// printf("start_time, %lf\n", (double)pFormatCtx->start_time);
	 int ret = av_seek_frame(pFormatCtx, stream_index, seek_target - backbuff + pFormatCtx->start_time / 1000, AVSEEK_FLAG_BACKWARD);
	// int ret = av_seek_frame(pFormatCtx, -1, seconds * AV_TIME_BASE , AVSEEK_FLAG_ANY);
    if (ret < 0) {
        fprintf(stderr, "av_seek_frame failed");
        return 0;
    }
		// avcodec_flush_buffers(pFormatCtx->streams[stream_index]->codec);
		avcodec_flush_buffers(pCodecCtx);
	int diff = 2000;
	i = 0;
	while (av_read_frame(pFormatCtx, &packet) >= 0) {
		// Is this a packet from the video stream?
		if (packet.stream_index == videoStream) {
			// Decode video frame
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

			// Did we get a video frame?
			if (frameFinished) {
				printf("packet pts, %d, %d\n", packet.pts, seek_target - packet.pts);
				// Convert the image from its native format to RGB.
				// sws_scale(sws_ctx, (uint8_t const * const *) pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);

				// Save the frame to disk.
				if (fabs(seek_target - packet.pts) < 1000 && fabs(seek_target - packet.pts) < diff) {
					sws_scale(sws_ctx, (uint8_t const * const *) pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
					printf("packet pts, %d, %d\n", packet.pts, seek_target - packet.pts);
					diff = fabs(seek_target - packet.pts);
				//	SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, ++i);
				}
				else if(packet.pts - seek_target > 2000){
					break;
				}
			}
		}

		// Free the packet that was allocated by av_read_frame.
		//av_free_packet(&packet); // Deprecated.
	}
	// SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, ++i);
	saveFrame_jpg(pFrameRGB->data[0], i, pCodecCtx->width, pCodecCtx->height);  
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



