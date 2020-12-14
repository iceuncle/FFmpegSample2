//
//  main.c
//  ff_filter
//
//  Created by tianyang on 2020/12/14.
//  Copyright © 2020 tianyang. All rights reserved.
//

#include <stdio.h>
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"

static AVFormatContext *fmt_ctx;
static AVCodecContext *dec_ctx;

AVFilterGraph *graph = NULL;
AVFilterContext *buf_ctx = NULL;
AVFilterContext *bufsink_ctx = NULL;

static int v_stream_index = -1;

/**
 *  @brief 打开媒体流和解码器
 */
static int open_input_file(const char *filename) {
    int ret = -1;
    AVCodec *dec = NULL;
    
    if ((ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to open file %s\n", filename);
        return ret;
    }
    
    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to find stream information!\n");
        return ret;
    }
    
    if ((ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO,  -1, -1, &dec, 0)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't find video stream!");
        return ret;
    }
    
    v_stream_index = ret;
    
    dec_ctx = avcodec_alloc_context3(dec);
    if (!dec_ctx) {
        return AVERROR(ENOMEM);
    }
    
    avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[v_stream_index]->codecpar);
    
    if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to open decoder!");
        return ret;
    }
    
    return 0;
}

/**
 * @brief 初始化Filter
 */
static int init_filters(const char *filter_desc) {
    
    int ret = -1;
    
    char args[512] = {};
    AVRational time_base = fmt_ctx->streams[v_stream_index]->time_base;
    
    AVFilterInOut *inputs = avfilter_inout_alloc();
    AVFilterInOut *outputs = avfilter_inout_alloc();
    
    if(!inputs || !outputs) {
        av_log(NULL, AV_LOG_ERROR, "No Memory when alloc inputs or outputs!\n");
        return AVERROR(ENOMEM);
    }
    
    graph = avfilter_graph_alloc();
    if (!graph) {
        av_log(NULL, AV_LOG_ERROR, "No Memory when create graph!\n");
        return AVERROR(ENOMEM);
    }
    
    const AVFilter *bufsrc = avfilter_get_by_name("buffer");
    if (!bufsrc) {
        av_log(NULL, AV_LOG_ERROR, "Failed to get buffer filter!\n");
        return -1;
    }
    
    const AVFilter *bufsink = avfilter_get_by_name("buffersink");
    if (!bufsink) {
        av_log(NULL, AV_LOG_ERROR, "Failed to get buffersink filter!\n");
        return -1;
    }
    
    //输入 buffer filter
    //"[in]drawbox=xxx[out]"
    snprintf(args, 512, "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             dec_ctx->width, dec_ctx->height,
             dec_ctx->pix_fmt,
             time_base.num, time_base.den,
             dec_ctx->sample_aspect_ratio.num, dec_ctx->sample_aspect_ratio.den);
    
    if((ret = avfilter_graph_create_filter(&buf_ctx, bufsrc, "in", args, NULL, graph)) < 0) {
      av_log(NULL, AV_LOG_ERROR, "Failed to create buffer filter context!\n");
      goto __ERROR;
    }
    
     //输出 buffer sink filter
    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_GRAY8, AV_PIX_FMT_NONE};
    if((ret = avfilter_graph_create_filter(&bufsink_ctx, bufsink, "out", NULL, NULL, graph)) < 0) {
       av_log(NULL, AV_LOG_ERROR, "Failed to create buffer sink filter context!\n");
       goto __ERROR;
    }
    
    av_opt_set_int_list(bufsink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    
    //create in/out
    inputs->name = av_strdup("out");
    inputs->filter_ctx = bufsink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;
    
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buf_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;
    
    //create filter and add graph for filter desciption
    if ((ret = avfilter_graph_parse_ptr(graph, filter_desc, &inputs, &outputs, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to parse filter description!\n");
        goto __ERROR;
    }
    
    if((ret = avfilter_graph_config(graph, NULL)) < 0) {
       av_log(NULL, AV_LOG_ERROR, "Failed to config graph!\n");
    }
  
__ERROR:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    
    return ret;
}

static int do_frame(AVFrame *filt_frame, FILE *out) {
    
    fwrite(filt_frame->data[0], 1, filt_frame->width*filt_frame->height, out);
    fflush(out);
    
    return 0;
}

//do filter
static int filter_video(AVFrame *frame, AVFrame *filt_frame, FILE *out) {
    
    int ret;
    if ((ret = av_buffersrc_add_frame(buf_ctx, frame)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to feed to filter graph!");
        return ret;
    }
    
    while (1) {
        ret = av_buffersink_get_frame(bufsink_ctx, filt_frame);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
            break;
        }
        if (ret < 0) {
            return ret;
        }
        do_frame(filt_frame, out);
        av_frame_unref(filt_frame);
    }
    
    av_frame_unref(frame);
    return ret;
}

//解码视频帧并对视频帧进行滤镜处理
static int decode_frame_and_filter(AVFrame *frame, AVFrame *filt_frame, FILE *out) {
    int ret = avcodec_receive_frame(dec_ctx, frame);
    if (ret < 0) {
        if(ret != AVERROR_EOF && ret != AVERROR(EAGAIN)){
            av_log(NULL, AV_LOG_ERROR, "Error while receiving a frame from decoder!\n");
        }
        return ret;
    }
    return filter_video(frame, filt_frame, out);
}

int main(int argc, const char * argv[]) {
    
    int ret;
    
    FILE *out;
    
    AVPacket packet;
    AVFrame *frame = NULL;
    AVFrame *filt_frame = NULL;
    
    const char *filter_desc="drawbox=30:10:64:64:red";
    const char* filename = "/Users/tianyang/Projects/XCodeProjects/FFmpegSample2/test.mp4";
    const char* outfile = "/Users/tianyang/Desktop/out.yuv";
    
    av_log_set_level(AV_LOG_DEBUG);
    
    frame = av_frame_alloc();
    filt_frame = av_frame_alloc();
    if (!frame || !filt_frame) {
        av_log(NULL, AV_LOG_ERROR, "No memory to alloc frame\n");
        exit(-1);
    }
    
    out = fopen(outfile, "wb");
    if (!out) {
        av_log(NULL, AV_LOG_ERROR, "Failed to open yuv file!\n");
        exit(-1);
    }
    
    if (open_input_file(filename) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to open media file\n");
    } else {
        if (init_filters(filter_desc) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to initialize filter!\n");
            goto __ERROR;
        }
    }
    
    //read avpacket from media file
    while (1) {
        if ((ret = av_read_frame(fmt_ctx, &packet)) < 0) {
            break;
        }
        if (packet.stream_index == v_stream_index) {
            ret = avcodec_send_packet(dec_ctx, &packet);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to send avpacket to decoder!\n");
                break;
            }
            if ((ret = decode_frame_and_filter(frame, filt_frame, out)) < 0) {
                if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
                    continue;
                }
                av_log(NULL, AV_LOG_ERROR, "Failed to decode or filter!\n");
                goto __ERROR;
            }
        }
    }
    
    return 0;
    
__ERROR:
    return -1;
}
