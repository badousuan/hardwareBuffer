#include "NubiaGLCopy.h"
#include "GraphicBufferEx.h"
#include "Utils.h"


JNIEXPORT jlong JNICALL
Java_com_example_copygpu_GLCopyJni_initHardwareBuffer(
        JNIEnv*env, jclass type, jint width, jint height, jint textureId)
{
    GraphicBufferEx* graphicBufferEx = new GraphicBufferEx(eglGetCurrentDisplay(), EGL_NO_CONTEXT);
    int format = AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420; //HAL_PIXEL_FORMAT_YCrCb_420_SP
    graphicBufferEx->create(width, height, textureId, format);
    return (jlong)graphicBufferEx;
}

JNIEXPORT void JNICALL
Java_com_example_copygpu_GLCopyJni_releaseHardwareBuffer(
        JNIEnv*env, jclass type, jlong handler)
{
    GraphicBufferEx* graphicBufferEx =  (GraphicBufferEx*)handler;
    graphicBufferEx->destroy();
    delete graphicBufferEx;
}

JNIEXPORT jbyteArray JNICALL
Java_com_example_copygpu_GLCopyJni_getBuffer(
        JNIEnv*env, jclass type, jlong handler)
{
    GraphicBufferEx* graphicBufferEx =  (GraphicBufferEx*)handler;
    int width = graphicBufferEx->getWidth();
    int stride = graphicBufferEx->getStride();
    int height = graphicBufferEx->getHeight();
    jbyteArray jYuvArray = env->NewByteArray(stride*height*3/2);
    jbyte* yuvArray = env->GetByteArrayElements(jYuvArray, NULL);
    GPUIPBuffer buffer;
    int plannerSize = stride * height;
    buffer.width = width;
    buffer.height = height;
    buffer.format = graphicBufferEx->getFormat();
    buffer.stride = stride;
    buffer.length = plannerSize * 3 / 2;
    buffer.pY = yuvArray;
    buffer.pU = yuvArray + plannerSize;
    if(buffer.format == AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420) {
        buffer.pV = yuvArray + plannerSize*5/4;
    } else  if(buffer.format == 17) {
        buffer.pV = buffer.pU;
    }


    graphicBufferEx->getBuffer(GPUIPBuffer_YV12_COPY, &buffer);
    //LOGI("dump yuv");
    //dump("/sdcard/123.yuv", buffer.pY, buffer.length);
    // to yv12
    uint8_t * tmpSrc =  static_cast<uint8_t*>(buffer.pU);
    uint8_t * tmpSrcUV = tmpSrc;

    uint8_t* tmpDst = new uint8_t[plannerSize/2];
    uint8_t * tmpDstV =  (uint8_t*)tmpDst + plannerSize/4;
    uint8_t * tmpDstU = tmpDst;

    for (int i = 0; i < plannerSize/4; ++i) {
      *(tmpDstV++) = *(tmpSrcUV++);
      *(tmpDstU++) = *(tmpSrcUV++);
    }
    memcpy(tmpSrc,tmpDst,plannerSize/2);
    delete[] tmpDst;
    env->ReleaseByteArrayElements(jYuvArray, yuvArray, 0);
    return jYuvArray;
}

JNIEXPORT void JNICALL
Java_com_example_copygpu_GLCopyJni_setBuffer(
        JNIEnv*env, jclass type, jlong handler, jbyteArray jbuffer)
{
    GraphicBufferEx* graphicBufferEx =  (GraphicBufferEx*)handler;
    jbyte* buffer = env->GetByteArrayElements(jbuffer, NULL);
    GPUIPBuffer inputBuffer;
    inputBuffer.width = graphicBufferEx->getWidth();
    inputBuffer.height = graphicBufferEx->getHeight();
    inputBuffer.format = 17; //HAL_PIXEL_FORMAT_YCrCb_420_SP
    inputBuffer.stride = inputBuffer.width;
    inputBuffer.length = inputBuffer.stride * inputBuffer.height * 3 / 2;
    inputBuffer.pY = buffer;
    inputBuffer.pU = buffer + inputBuffer.stride * inputBuffer.height;
    inputBuffer.pV = inputBuffer.pU;
    graphicBufferEx->setBuffer(&inputBuffer);
    env->ReleaseByteArrayElements(jbuffer, buffer, 0);
}