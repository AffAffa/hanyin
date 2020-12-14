#include <QCoreApplication>
#include "tspl_sdk_api.h"
#include <QString>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <QImage>
#include<QBuffer>
#include<QtDebug>
#include<QPainter>
#include<iostream>
#include<QFile>
using namespace std;
struct bitmap_t {
    int32_t width;
    int32_t height;
    unsigned int rowwidth;
    uint16_t depth;
    unsigned char *data;
};

int read_bitmap_from_file2(QByteArray imgdate, struct bitmap_t* bitmap) {
    int ret = 0;
    char* buffer;
    char header[54];
    unsigned short offset;
    unsigned char bottomup;
    unsigned int align;

     memcpy(header,imgdate.data(),54);
    // check signature
    if ((header[0] != 0x42) || (header[1] != 0x4D)) {
        fprintf(stderr, "File is not a valid Windows Bitmap! (Wrong signature: 0x%X%X)\n", header[0], header[1]);
        ret = -3;

    }

    // offset where image data start
    offset = (header[11] << 8) | header[10];

    // read width from header, should be positiv
    bitmap->width = *(int32_t*)(header+18);

    // read height from header
    bitmap->height = *(int32_t*)(header+22);

    // Is this a bottum up picture?
    bottomup = bitmap->height >= 0;

    // color depth of image, should be 1 for monochromes
    bitmap->depth = *(uint16_t*)(header+28);

    // width of a byte row
    if (bitmap->width % 8) {
        bitmap->rowwidth = ((bitmap->width/8)+1) * bitmap->depth;
    } else {
        bitmap->rowwidth = (bitmap->width/8) * bitmap->depth;
    }

    // 4-byte alignment width of a byte row, align >= bitmap->rowwidth
    if (bitmap->rowwidth % 4) {
        align = ((bitmap->rowwidth / 4)+1)*4;
    } else {
        align = bitmap->rowwidth;
    }

    fprintf(stdout, "File is a %ix%ix%i bitmap\n", bitmap->width, bitmap->height, bitmap->depth);

    if (bitmap->depth != 1) {
        fprintf(stderr, "File is not an 1-bit Bitmap!\n");
        ret = -4;
        goto read_bitmap_ret;
    }


    if ((bitmap->data = (unsigned char*)malloc(align*bitmap->height)) == NULL) {
        fprintf(stderr, "Could not aquire memory for image data (%u bytes)!\n", align*bitmap->height);
        ret = -5;
        goto read_bitmap_ret;
    }

    if ((buffer = (char*)malloc(align)) == NULL) {
        fprintf(stderr, "Could not aquire memory for read buffer (%u bytes)!\n", align);
        ret = -6;
        free(bitmap->data);
        goto read_bitmap_ret;
    }

    for (unsigned int row=0; row<bitmap->height; ++row) {
        // get char by char
        for (unsigned int col =0; col <= align; ++col) {
            if(offset+row*align+col<imgdate.length())
                buffer[col] = imgdate.at(offset+row*align+col);
        }
        if (bottomup) {
            memcpy(bitmap->data+(((bitmap->height-1)-row)*bitmap->rowwidth), buffer, bitmap->rowwidth);
        } else {
            memcpy(bitmap->data+(row*abs(bitmap->width/8)), buffer, bitmap->rowwidth);
        }

    }

    free(buffer);


read_bitmap_ret:
    return ret;
}
int write_bitmap_as_xbm(const char* filename, struct bitmap_t* bitmap, const char* name) {
    int ret = 0;
    FILE *fd;

    // open file
    if ((fd = fopen(filename, "w")) == NULL) {
        fprintf(stderr, "Error while opening file '%s'.\n", filename);
        ret = -1;
        goto write_bitmap_xbm_ret;
    }

    fprintf(fd, "#define %s_width %u\n", name, bitmap->width);
    fprintf(fd, "#define %s_height %u\n", name, bitmap->height);
    fprintf(fd, "static char %s_bits[] = {\n", name);

    for (unsigned int line = 0; line <= (bitmap->height*bitmap->rowwidth)/12; ++line) {
        fprintf(fd, "\t");

        unsigned int max_pos = (line < (bitmap->height*bitmap->rowwidth)/12) ? 12 : (bitmap->height*bitmap->rowwidth) % 12;

        for (unsigned int pos = 0; pos < max_pos; ++pos) {
            unsigned char data = 0;

            // change LSB<->MSB
            for (unsigned int bit = 0; bit < 8; ++bit) {
                if (bitmap->data[line*12+pos] & (1 << bit)) {
                    data |= (1 << (7-bit));
                }
            }

            fprintf(fd, "%02X ", data);
        }

        if (line == (bitmap->height*bitmap->rowwidth)/12) {
            fprintf(fd, "};");
        }

    }

    fclose(fd);

write_bitmap_xbm_ret:
    return ret;
}


// 二值化处理
QImage TowValue(QImage img, int value)
{
    int r,g,b,gray;
    QImage grayImg(img.width(), img.height(), QImage::Format_Grayscale8);
    for (int i = 0; i < img.width(); i++)
    {
        for (int j= 0; j < img.height(); j++)
        {
            r = qRed(img.pixel(i, j));
            g = qGreen(img.pixel(i, j));
            b = qBlue(img.pixel(i, j));
            gray = (r+g+b)/3;
            if(gray<value){
                grayImg.setPixel(i, j, qRgb(0, 0, 0));
            }else{
                grayImg.setPixel(i, j, qRgb(255, 255, 255));
            }
        }
    }
    return grayImg;
}

void test_tspl()
{
    void *phandle = NULL;
    int result = 0;
    char error_msg[1024] = {0};


    result = PrinterCreator(&phandle, "HD100");

    if (0 != result)
    {
        printf("%s\n", error_msg);
        return;
    }
    result = PortOpen(phandle, "USB,HD100,vid=0x20D1,pid=0x7007");
    if (0 != result)
    {
        printf("%d\n", result);
        return;
    }
    else
    {
        result = TSPL_ClearBuffer(phandle);
        if (0 != result)
        {
            printf("%d %s\n", result,error_msg);
            return;
        }
        result = TSPL_Setup(phandle,100,100,2,10,1,1,0);
        if (0 != result)
        {
            printf("%d %s\n", result,"setup");
            return;
        }

        struct bitmap_t bitmap;
        ///mnt/hgfs/bitmap打印
        QImage vimg("/mnt/hgfs/bitmap打印/tmp2.png");

        //vimg=vimg.convertToFormat(QImage::Format_Mono);
        QByteArray ba;
        QBuffer buffer(&ba);
        QFile file;
        buffer.open(QIODevice::WriteOnly);
        vimg.save(&buffer, "bmp");
        buffer.close();
        read_bitmap_from_file2(ba,&bitmap);
        write_bitmap_as_xbm("/home/moyilong/下载/aa",&bitmap,"aa");
        result=TSPL_Bitmap(phandle,0,0,0,vimg.width(),vimg.height(),bitmap.data);






        if (0 != result)
        {
            //FormatError(result, 0, (unsigned char*)error_msg, 0, sizeof(error_msg));
            printf("%d %s\n", result,error_msg);
            return;
        }
        result = TSPL_Print(phandle, 1, 1);
    //打印自检页
        //result = TSPL_SelfTest(phandle);

    }

    result = PortClose(phandle);

    result = PrinterDestroy(phandle);
}
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    test_tspl();
    return a.exec();
}
