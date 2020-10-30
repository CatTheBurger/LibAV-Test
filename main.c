/*
 * Copyright (c) 2001 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * video encoding with libavcodec API example
 *
 * @example encode_video.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>

static void encode(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt,
                   FILE *outfile)
{
    int ret;

    /* send the frame to the encoder */
    if (frame)
        printf("Send frame %3"PRId64"\n", frame->pts);

    ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending a frame for encoding\n");
        exit(1);
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            exit(1);
        }

        printf("Write packet %3"PRId64" (size=%5d)\n", pkt->pts, pkt->size);
        fwrite(pkt->data, 1, pkt->size, outfile);
        av_packet_unref(pkt);
    }
}

static AVFrame *decode() {
    AVFormatContext *format = avformat_alloc_context();
    AVCodecContext *ctx;
    AVCodec *codec;
    int video_stream;

    if (!avformat_open_input(&format, "/mnt/media/input_video.mp4", NULL, NULL) < 0) {
        printf("Cant open input video!\n");
        return NULL;
    }

    if (avformat_find_stream_info(format, NULL) < 0)
    {
        printf("cant find streams\n");
        return NULL;
    }

    for(uint32_t i = 0;i < format->nb_streams;i++)
    {
        if(format->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream = i;
            break;
        }
    }

    if(video_stream == -1)
    {
        printf("no video stream\n");
        return NULL;
    }

    ctx = format->streams[video_stream]->codec;
    codec = avcodec_find_decoder(ctx->codec_id);

    if (avcodec_open2(ctx, codec, NULL) < 0)
    {
        printf("cant open codec\n");
        return NULL;
    }

    AVFrame *frame = av_frame_alloc();
    AVPacket *packet = av_packet_alloc();
    int frame_check = 0;

//    av_seek_frame(format, video_stream, (1000 / 30 * (double) format->streams[video_stream]->time_base.den /
//                                         (double) format->streams[video_stream]->time_base.num), AVSEEK_FLAG_BACKWARD);

    while (1) {
        if (av_read_frame(format, packet) < 0) {
            printf("Cant read frame\n");
            break;
        }

        if (packet->stream_index != video_stream) {
            printf("stream error\n");
            continue;
        }

        avcodec_decode_video2(ctx, frame, &frame_check, packet);

        if (frame_check) {
            printf("Frame ready!\n");
            break;
        }
    }

    return frame;
}

int main(int argc, char **argv)
{
    const char *filename, *codec_name;
    const AVCodec *codec;
    AVCodecContext *c= NULL;
    int i, ret, x, y;
    FILE *f;
    AVFrame *frame;
    AVPacket *pkt;
    uint8_t endcode[] = { 0, 0, 1, 0xb7 };

    frame = decode();
    if (frame == NULL) {
        printf("Cant decode frame from video!\n");
        return 1;
    }

//    if (argc <= 2) {
//        fprintf(stderr, "Usage: %s <output file> <codec name>\n", argv[0]);
//        exit(0);
//    }
    filename = "/mnt/media/output_video.mp4";
    codec_name = "mpeg1video";

    /* find the mpeg1video encoder */
    codec = avcodec_find_encoder_by_name(codec_name);
    if (!codec) {
        fprintf(stderr, "Codec '%s' not found\n", codec_name);
        exit(1);
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }

    pkt = av_packet_alloc();
    if (!pkt)
        exit(1);

    /* put sample parameters */
    c->bit_rate = 400000;
    /* resolution must be a multiple of two */
    c->width = frame->width;
    c->height = frame->height;
    /* frames per second */
    c->time_base = (AVRational){1, 25};
    c->framerate = (AVRational){25, 1};

    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    c->gop_size = 10;
    c->max_b_frames = 1;
    c->pix_fmt = AV_PIX_FMT_YUV420P;

    if (codec->id == AV_CODEC_ID_H264)
        av_opt_set(c->priv_data, "preset", "slow", 0);

    /* open it */
    ret = avcodec_open2(c, codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
        exit(1);
    }

    f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }

//    frame = av_frame_alloc();
//    if (!frame) {
//        fprintf(stderr, "Could not allocate video frame\n");
//        exit(1);
//    }
//    frame->format = c->pix_fmt;
//    frame->width  = c->width;
//    frame->height = c->height;

//    ret = av_frame_get_buffer(frame, 0);
//    if (ret < 0) {
//        fprintf(stderr, "Could not allocate the video frame data\n");
//        exit(1);
//    }

    /* encode 1 second of video */
    for (i = 0; i < 2500; i++) {
        fflush(stdout);

        frame = decode();
        if (frame == NULL) {
            printf("Cant decode frame from video!\n");
            return 1;
        }

        /* make sure the frame data is writable */
//        ret = av_frame_make_writable(frame);
//        if (ret < 0)
//            exit(1);

        /* prepare a dummy image */
        /* Y */
        for (y = 0; y < c->height / 2; y++) {
            for (x = 0; x < c->width / 2; x++) {
                frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
            }
        }

        frame->pts = i;

        /* encode the image */
        encode(c, frame, pkt, f);
    }

    /* flush the encoder */
    encode(c, NULL, pkt, f);

    /* add sequence end code to have a real MPEG file */
    if (codec->id == AV_CODEC_ID_MPEG1VIDEO || codec->id == AV_CODEC_ID_MPEG2VIDEO)
        fwrite(endcode, 1, sizeof(endcode), f);
    fclose(f);

    avcodec_free_context(&c);
    av_frame_free(&frame);
    av_packet_free(&pkt);

    return 0;
}
