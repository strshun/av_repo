#include <iostream>


extern "C"
{
    /*Include ffmpeg header file*/
    #include <libavformat/avformat.h>
}

int main() {

    AVFormatContext *pFormatCtx = NULL;
    AVDictionary *options = NULL;
    AVPacket *packet = NULL;

    char filePath[] = "rtsp://172.31.96.79:6880/0/video/kll_5min_test.h264";

    //av_register_all();  //函数在ffmpeg4.0以上版本已经被废弃，所以4.0以下版本就需要注册初始函数

    av_dict_set(&options, "buffer_size", "1024000", 0); //设置缓存大小,1080p可将值跳到最大
    av_dict_set(&options, "rtsp_transport", "tcp", 0); //以tcp的方式打开,
    av_dict_set(&options, "stimeout", "5000000", 0); //设置超时断开链接时间，单位us
    av_dict_set(&options, "max_delay", "500000", 0); //设置最大时延

//    pFormatCtx = avformat_alloc_context(); //用来申请AVFormatContext类型变量并初始化默认参数,申请的空间


    //打开网络流或文件流
    if (avformat_open_input(&pFormatCtx, filePath, NULL, &options) != 0)
    {
        printf("Couldn't open input stream.\n");
        return 0;
    }

    //获取视频文件信息
    if (avformat_find_stream_info(pFormatCtx, NULL)<0)
    {
        printf("Couldn't find stream information.\n");
        return 0;
    }

    //查找码流中是否有视频流
    int videoindex = -1;
    unsigned i = 0;
    for (i = 0; i<pFormatCtx->nb_streams; i++)
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoindex = i;
            break;
        }
    if (videoindex == -1)
    {
        printf("Didn't find a video stream.\n");
        return 0;
    }

    packet = (AVPacket *)av_malloc(sizeof(AVPacket)); // 申请空间，存放的每一帧数据 （h264、h265）


    FILE *fpSave;
    fpSave = fopen("qtest.h264", "wb");

    //这边可以调整i的大小来改变文件中的视频时间,每 +1 就是一帧数据

    for (i = 0; i < 400; i++)
    {
        if (av_read_frame(pFormatCtx, packet) >= 0)
        {
            if (packet->stream_index == videoindex)
            {
                fwrite(packet->data, 1, packet->size, fpSave);
            }
            av_packet_unref(packet);
        }
    }

    fclose(fpSave);
    av_free(packet);
    avformat_close_input(&pFormatCtx);

    return 0;
}
