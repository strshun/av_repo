#include <iostream>
#include <thread>

extern "C"
{
#include "httpget.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
}
typedef struct
{
    char cameraID[100]={0};
    char rtspurl[300]={0};
    char picName[400]={0};

}SnapEvent;

SnapEvent RtspQueue[20000];   //环形缓冲数组

int savePicture(AVFrame *pFrame, const char *out_name) {//编码保存图片

    std::cout<<"savePicture" << out_name<<std::endl;
    int width = pFrame->width;
    int height = pFrame->height;
    AVCodecContext *pCodeCtx = NULL;


    AVFormatContext *pFormatCtx = avformat_alloc_context();
    // 设置输出文件格式
    pFormatCtx->oformat = av_guess_format("mjpeg", NULL, NULL);

    // 创建并初始化输出AVIOContext
    if (avio_open2(&pFormatCtx->pb, out_name, AVIO_FLAG_READ_WRITE,NULL,NULL) < 0) {
        printf("Couldn't open output file.\n");
        return -1;
    }

    // 构建一个新stream
    AVStream *pAVStream = avformat_new_stream(pFormatCtx, 0);
    if (pAVStream == NULL) {
        return -1;
    }

    AVCodecParameters *parameters = pAVStream->codecpar;
    parameters->codec_id = pFormatCtx->oformat->video_codec;
    parameters->codec_type = AVMEDIA_TYPE_VIDEO;
    parameters->format = AV_PIX_FMT_YUVJ420P;
    parameters->width = pFrame->width;
    parameters->height = pFrame->height;

    AVCodec *pCodec = avcodec_find_encoder(pAVStream->codecpar->codec_id);

    if (!pCodec) {
        printf("Could not find encoder\n");
        return -1;
    }

    pCodeCtx = avcodec_alloc_context3(pCodec);
    if (!pCodeCtx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        return -1;
    }

    if ((avcodec_parameters_to_context(pCodeCtx, pAVStream->codecpar)) < 0) {
        fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return -1;
    }

    pCodeCtx->time_base = (AVRational) {1, 25};

    if (avcodec_open2(pCodeCtx, pCodec, NULL) < 0) {
        printf("Could not open codec.");
        return -1;
    }

    int ret = avformat_write_header(pFormatCtx, NULL);
    if (ret < 0) {
        printf("write_header fail\n");
        return -1;
    }

    int y_size = width * height;

    //Encode
    // 给AVPacket分配足够大的空间
    AVPacket pkt;
    av_new_packet(&pkt, y_size * 3);

    // 编码数据
    ret = avcodec_send_frame(pCodeCtx, pFrame);
    if (ret < 0) {
        printf("Could not avcodec_send_frame.");
        return -1;
    }

    // 得到编码后数据
    ret = avcodec_receive_packet(pCodeCtx, &pkt);
    if (ret < 0) {
        printf("Could not avcodec_receive_packet");
        return -1;
    }

    ret = av_write_frame(pFormatCtx, &pkt);

    if (ret < 0) {
        printf("Could not av_write_frame");
        return -1;
    }

    av_packet_unref(&pkt);

    //Write Trailer
    av_write_trailer(pFormatCtx);


    avcodec_close(pCodeCtx);
    avio_close(pFormatCtx->pb);
    avformat_free_context(pFormatCtx);

    return 0;
}
int initFFMPEG(const char* rtsp_url, const char* picName){
    std::cout<<"program running....."<<std::endl;
    int ret;
    AVFormatContext *fmt_ctx = NULL;

    const AVCodec *codec;
    AVCodecContext *codeCtx = NULL;

    AVStream *stream = NULL;
    int stream_index;

    AVPacket avpkt;

    int frame_count;
    AVFrame *frame;


    // 1
#if 0
    rtsp_url = "./test.mp4";
    if (avformat_open_input(&fmt_ctx, rtsp_url, NULL, NULL) < 0) {
        printf("Could not open source file %s\n", rtsp_url);
        return -1;
    }
#else
    //    rtsp_url = "rtsp://admin:qaz_wsx1@10.1.107.45:80/cam/realmonitor?channel=1&subtype=0";
    //Network
    avformat_network_init();

    //使用TCP连接打开RTSP，设置最大延迟时间
    AVDictionary *avdic=NULL;
    char option_key[]="rtsp_transport";
    char option_value[]="tcp";
    av_dict_set(&avdic,option_key,option_value,0);
    char option_key2[]="max_delay";
    char option_value2[]="5000000";
    av_dict_set(&avdic, "stimeout", "3000000", 0);  //如果没有设置stimeout
    av_dict_set(&avdic,option_key2,option_value2,0);

    if((ret=avformat_open_input(&fmt_ctx,rtsp_url,0,&avdic))<0)
    {
        printf("Could not open input file %s.\n",rtsp_url);
        return -1;
    }
#endif

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        printf("Could not find stream information\n");
        return -1;
    }
    av_dump_format(fmt_ctx, 0, rtsp_url, 0);

    av_init_packet(&avpkt);
    avpkt.data = NULL;
    avpkt.size = 0;

    // 2
    stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO), rtsp_url);
        return ret;
    }

    stream = fmt_ctx->streams[stream_index];

    // 3
    codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (codec == NULL) {
        return -1;
    }

    // 4
    codeCtx = avcodec_alloc_context3(NULL);
    if (!codeCtx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        return -1;
    }


    // 5
    if ((ret = avcodec_parameters_to_context(codeCtx, stream->codecpar)) < 0) {
        fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return ret;
    }

    // 6
    avcodec_open2(codeCtx, codec, NULL);


    //初始化frame，解码后数据
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        return -1;
    }

    frame_count = 0;
    char buf[1024];
    // 7
    while (av_read_frame(fmt_ctx, &avpkt) >= 0) {
        if (avpkt.stream_index == stream_index && avpkt.flags == AV_PKT_FLAG_KEY && avpkt.pts>=0) {
            // 8
            int re = avcodec_send_packet(codeCtx, &avpkt);
            if (re < 0) {
                continue;
            }

            // 9 这里必须用while()，因为一次avcodec_receive_frame可能无法接收到所有数据
            while (avcodec_receive_frame(codeCtx, frame) == 0) {
                // 拼接图片路径、名称
                snprintf(buf, sizeof(buf), ".//pic//%s.jpg", picName);
                savePicture(frame, buf); //保存为jpg图片
                return 0;
            }

            frame_count++;
        }
        av_packet_unref(&avpkt);
    }
}
void run(int start, int end){
    for(int j = start;j< end;j++)
    {
        initFFMPEG(RtspQueue[j].rtspurl, RtspQueue[j].picName);
    }
}


int main(int argc, char *argv[]) {//解码视频





    if (argc <= 1) {
        printf("Usage: %s threadRunNum \n", argv[0]);
        exit(0);
    }
    //读取设备列表
    //http post
    char req[1024]={0};
    char reData[500]={0};
    int i=0,j=0, CameNum=0,recvlen=0;
    //file read
    int fddev = -1;
    char devlist[3000000]={0};
    fddev=open("devlist.txt",O_RDONLY);
    if(fddev == -1){printf("open devlist.txt fail\n");return 0;}
    read(fddev,devlist,3000000);
    close(fddev);
    //printf("get devlist:%s\n",devlist);

    char * pt;  //外
    char *outer_ptr=NULL;
    char dev[300]={0};
    char * p;  //内
    pt =strtok_r(devlist,"/",&outer_ptr);
    while(pt!= NULL)
    {
        //printf("pt:%s,size:%d\n",pt,strlen(pt));
        if(strlen(pt)>20 && *pt=='d' && *(pt+1)=='e' && *(pt+2)=='v')
        {
            sprintf(dev, "%s", pt);
            p = strtok(dev, ",");  // 过滤掉dev前缀
            p = strtok(NULL, ",");
            sprintf(RtspQueue[CameNum].picName, "%s", p);
            p = strtok(NULL, ",");
            sprintf(RtspQueue[CameNum].cameraID, "%s", p);


            printf("[%d]http get_cameraID:%s,picName:%s\n",CameNum,RtspQueue[CameNum].cameraID,RtspQueue[CameNum].picName);
            sprintf(req, "GET /vivian/dv/service/v1/device/channel/getRtsp/%s HTTP/1.1\r\n"
                         "Accept:*/*\r\n"
                         "Host:34.191.45.6:8081\r\n"
                         "Connection: close\r\n"
                         "Content-Type:application/json;charset=UTF-8\r\n\r\n",
                    RtspQueue[CameNum].cameraID);
            recvlen=http_send("34.191.45.6",8081,req, strlen(req),reData);
            for (i = 0; i < 498;i++)
            {
                if(reData[i]=='r' && reData[i+1]=='t' && reData[i + 2] == 's')
                    break;
            }
            if (i == 498) {pt =strtok_r(NULL,"/",&outer_ptr);continue;}
            j=i;
            while(reData[i]!='"') i++;
            reData[i] = '\0';
            memcpy(RtspQueue[CameNum].rtspurl,reData+j,i-j+1);
            printf("[%d]http recvlen:%d,rtspurl:%s\n",CameNum,recvlen,RtspQueue[CameNum].rtspurl);

            CameNum++;
        }

        pt =strtok_r(NULL,"/",&outer_ptr);
    }

    printf("httpget rtspUrl module end...CameNum:%d\n",CameNum);




    int thread_runNum = atoi(argv[1]);
    int thread_num = CameNum / thread_runNum;
    if(thread_num == 0)
    {
        thread_num++;
    }
    for (int i = 0; i < thread_num; i++)
    {
        if(RtspQueue[i].rtspurl[0]=='\0') continue;
        if(i == thread_num - 1)
        {
            std::thread t(run, i* thread_runNum, CameNum);
            t.detach();
        }else{
            std::thread t(run, i* thread_runNum, i* thread_runNum + thread_runNum);
            t.detach();
        }


    }
    getchar();

}
