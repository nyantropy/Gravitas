#include "IMGUtility.h"

IMGUtility::IMGUtility()
{

}

IMGUtility::~IMGUtility()
{
    
}

uint32_t* IMGUtility::extractImageData(const AVFrame* frame, int* width, int* height)
{
    uint32_t* image_data = nullptr;
    *width = frame->width;
    *height = frame->height;

    //check if the frame is in a compatible format, in our case its not rgb, since we convert to yuv420p on the sender side
    if (frame->format != AV_PIX_FMT_RGBA) 
    {
        //we need a sws context to convert to rgba format
        SwsContext* sws_ctx = sws_getContext(
            frame->width, frame->height, (AVPixelFormat)frame->format,
            frame->width, frame->height, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, nullptr, nullptr, nullptr);

        if (!sws_ctx) 
        {
            //unable to create SwsContext, do nothing
            //return;
        }

        AVFrame* converted_frame = av_frame_alloc();
        if (!converted_frame) 
        {
            //unable to allocate frame, do nothing
            sws_freeContext(sws_ctx);
            //return;
        }

        converted_frame->format = AV_PIX_FMT_RGBA;
        converted_frame->width = frame->width;
        converted_frame->height = frame->height;

        if (av_frame_get_buffer(converted_frame, 1) < 0) 
        {
            av_frame_free(&converted_frame);
            sws_freeContext(sws_ctx);
            //return;
        }

        sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height,
                  converted_frame->data, converted_frame->linesize);

        image_data = new uint32_t[converted_frame->width * converted_frame->height];
        for (int y = 0; y < converted_frame->height; y++) 
        {
            memcpy(image_data + y * converted_frame->width,
                   converted_frame->data[0] + y * converted_frame->linesize[0],
                   converted_frame->width * 4);
        }

        av_frame_free(&converted_frame);
        sws_freeContext(sws_ctx);
    } 
    else 
    {
        //also covering the case that the frame is in rgba format already, not tested though, since it does not occur in my program
        image_data = new uint32_t[frame->width * frame->height];
        for (int y = 0; y < frame->height; y++) 
        {
            memcpy(image_data + y * frame->width,
                   frame->data[0] + y * frame->linesize[0],
                   frame->width * 4);
        }
    }

    return image_data;
}

void IMGUtility::saveRGBToPNG(const char* filename, uint32_t* image_data, int width, int height)
{
    FILE* fp = fopen(filename, "wb");
    if (!fp) 
    {
        return;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) 
    {
        fclose(fp);
        return;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) 
    {
        png_destroy_write_struct(&png, nullptr);
        fclose(fp);
        return;
    }

    if (setjmp(png_jmpbuf(png))) 
    {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return;
    }

    png_init_io(png, fp);

    //output is 8-bit depth, RGBA format
    png_set_IHDR(
        png,
        info,
        width, height,
        8,
        PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );

    png_write_info(png, info);

    png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    for (int y = 0; y < height; y++) 
    {
        row_pointers[y] = (png_byte*)(image_data + y * width);
    }

    png_write_image(png, row_pointers);
    png_write_end(png, nullptr);

    free(row_pointers);
    png_destroy_write_struct(&png, &info);
    fclose(fp);

    return;
}

void IMGUtility::saveFrameAsImage(const AVFrame* frame, const std::string& filename) 
{
    if (!frame) {
        std::cerr << "Invalid frame" << std::endl;
        return;
    }

    if (!frame->data[0]) {
        std::cerr << "Frame data is null" << std::endl;
        return;
    }

    int width = frame->width;
    int height = frame->height;

    FILE* file = fopen(filename.c_str(), "wb");
    if (!file) {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return;
    }

    AVFrame* rgbFrame = av_frame_alloc();
    if (!rgbFrame) {
        fclose(file);
        std::cerr << "Failed to allocate RGB frame" << std::endl;
        return;
    }

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height, 1);
    uint8_t* buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    if (!buffer) {
        fclose(file);
        av_frame_free(&rgbFrame);
        std::cerr << "Failed to allocate buffer for RGB frame" << std::endl;
        return;
    }

    av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, buffer, AV_PIX_FMT_RGB24, width, height, 1);

    struct SwsContext* swsContext = sws_getContext(width, height, AV_PIX_FMT_YUV420P,
                                                   width, height, AV_PIX_FMT_RGB24,
                                                   SWS_BILINEAR, NULL, NULL, NULL);
    if (!swsContext) {
        fclose(file);
        av_freep(&buffer);
        av_frame_free(&rgbFrame);
        std::cerr << "Failed to create SwsContext" << std::endl;
        return;
    }

    sws_scale(swsContext, frame->data, frame->linesize, 0, height, rgbFrame->data, rgbFrame->linesize);

    png_structp pngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!pngPtr) {
        fclose(file);
        av_frame_free(&rgbFrame);
        sws_freeContext(swsContext);
        av_freep(&buffer);
        std::cerr << "Failed to create PNG write struct" << std::endl;
        return;
    }

    png_infop infoPtr = png_create_info_struct(pngPtr);
    if (!infoPtr) {
        fclose(file);
        png_destroy_write_struct(&pngPtr, NULL);
        av_frame_free(&rgbFrame);
        sws_freeContext(swsContext);
        av_freep(&buffer);
        std::cerr << "Failed to create PNG info struct" << std::endl;
        return;
    }

    if (setjmp(png_jmpbuf(pngPtr))) {
        fclose(file);
        png_destroy_write_struct(&pngPtr, &infoPtr);
        av_frame_free(&rgbFrame);
        sws_freeContext(swsContext);
        av_freep(&buffer);
        std::cerr << "Error during PNG write" << std::endl;
        return;
    }

    png_init_io(pngPtr, file);
    png_set_IHDR(pngPtr, infoPtr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(pngPtr, infoPtr);

    png_bytep* rowPointers = new png_bytep[height];
    for (int y = 0; y < height; y++) {
        rowPointers[y] = (png_bytep)(rgbFrame->data[0] + y * rgbFrame->linesize[0]);
    }
    png_write_image(pngPtr, rowPointers);

    png_write_end(pngPtr, NULL);

    delete[] rowPointers;
    fclose(file);
    png_destroy_write_struct(&pngPtr, &infoPtr);
    av_frame_free(&rgbFrame);
    sws_freeContext(swsContext);
    av_freep(&buffer);

    std::cout << "Image saved: " << filename << std::endl;
}