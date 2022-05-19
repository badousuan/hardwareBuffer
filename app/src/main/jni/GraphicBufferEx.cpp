#include "GraphicBufferEx.h"
#include "Utils.h"

#include <dlfcn.h>
#include <string>
#include <thread>
#include <mutex>
static const int USAGE = (AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN
                           | AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN);

#include <sys/system_properties.h>

#define HAL_PIXEL_FORMAT_YCrCb_420_SP 17
#define HAL_PIXEL_FORMAT_YV12 0x32315659

static int getSDKVersion() {
  static int sdk_version = -1;
  if(sdk_version == -1) {
    char osVersion[PROP_VALUE_MAX+1] = {0};
    int osVersionLength = __system_property_get("ro.build.version.sdk", osVersion);
    if(osVersionLength > 0) {
      sdk_version = std::atoi(osVersion);
    }
  }
  return sdk_version;
}
typedef int (*AHardwareBuffer_lockPlanes_fnc)(AHardwareBuffer* , uint64_t ,
                                              int32_t , const ARect* , AHardwareBuffer_Planes*);

static AHardwareBuffer_lockPlanes_fnc lockPlanes_fPtr;
static std::once_flag onceFlag;
static void initDynamicLoadLib(){
  static void* gNativeWinLibHandle;
  gNativeWinLibHandle = dlopen("libnativewindow.so",RTLD_NOW);
  if (gNativeWinLibHandle) {
    void*  lockPlanes_fnp = dlsym(gNativeWinLibHandle, "AHardwareBuffer_lockPlanes");
    lockPlanes_fPtr = lockPlanes_fnp?(AHardwareBuffer_lockPlanes_fnc)lockPlanes_fnp:NULL;
  }
}

GraphicBufferEx::GraphicBufferEx(
        EGLDisplay eglDisplay, EGLContext eglContext)
{
    mEGLDisplay = eglDisplay;
    mEGLContext = eglContext;
    std::call_once(onceFlag, initDynamicLoadLib);
}

int GraphicBufferEx::getWidth()
{
    AHardwareBuffer_Desc outDesc;
    AHardwareBuffer_describe(mHardwareBuffer, &outDesc);
    return outDesc.width;
}

int GraphicBufferEx::getHeight()
{
    AHardwareBuffer_Desc outDesc;
    AHardwareBuffer_describe(mHardwareBuffer, &outDesc);
    return outDesc.height;
}

int GraphicBufferEx::getStride()
{
    AHardwareBuffer_Desc outDesc;
    AHardwareBuffer_describe(mHardwareBuffer, &outDesc);
    return outDesc.stride;
}

int GraphicBufferEx::getFormat()
{
    AHardwareBuffer_Desc outDesc;
    AHardwareBuffer_describe(mHardwareBuffer, &outDesc);
    return outDesc.format;
}

void GraphicBufferEx::create(int width, int height, 
        int textureId, int format)
{
    //format
    //RGBA_8888                     1
    //RGB_888                       3
    //A_8                           8
    //HAL_PIXEL_FORMAT_YCrCb_420_SP 17
    //HAL_PIXEL_FORMAT_YV12
    /**
     * For example some implementations may allow EGLImages with
     * planar or interleaved YUV data to be GLES texture target siblings.  It is
     * up to the implementation exactly what formats are accepted.
     */
    AHardwareBuffer_Desc buffDesc;
    buffDesc.width = width;
    buffDesc.height = height;
    buffDesc.layers = 1;
    buffDesc.format = format;
    buffDesc.usage = USAGE;
    buffDesc.stride = width;
    buffDesc.rfu0 = 0;
    buffDesc.rfu1 = 0;
    //创建AHardwareBuffer
    int err = AHardwareBuffer_allocate(&buffDesc, &mHardwareBuffer);

    if (err)//NO_ERROR
    {
        LOGE("GraphicBufferEx HardwareBuffer create error. (NO_ERROR != err)");
        return;
    } else {
        LOGI("AHardwareBuffer_allocate %p", mHardwareBuffer);
    }
    //创建EGLClientBuffer
    EGLClientBuffer clientBuffer = eglGetNativeClientBufferANDROID(mHardwareBuffer);
    checkEglError("eglGetNativeClientBufferANDROID");
    //创建 EGLImage
    mEGLImageKHR = eglCreateImageKHR(
            mEGLDisplay, mEGLContext,
            EGL_NATIVE_BUFFER_ANDROID, clientBuffer, 0);
    checkEglError("eglCreateImageKHR");

    if (EGL_NO_IMAGE_KHR == mEGLImageKHR)
    {
        LOGE("GraphicBufferEx create error. (EGL_NO_IMAGE_KHR == mEGLImageKHR)");
        return;
    }

    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);
    checkGlError("glBindTexture");
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, (GLeglImageOES)mEGLImageKHR); //
    checkGlError("glEGLImageTargetTexture2DOES");
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    checkGlError("glTexParameteri");
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    checkGlError("glTexParameteri");
  int dd;
  glGetTexParameteriv(GL_TEXTURE_EXTERNAL_OES,GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES,&dd);
  int dddddd = glGetError();
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
}

void GraphicBufferEx::destroy()
{
    AHardwareBuffer_release(mHardwareBuffer);
    eglDestroyImageKHR(mEGLDisplay, mEGLImageKHR);
}

void GraphicBufferEx::setBuffer(GPUIPBuffer *srcBuffer)
{
    uint8_t *pDstBuffer = NULL;

    if (NULL == srcBuffer)
    {
        LOGE("GraphicBufferEx setBuffer (NULL == srcBuffer)\n");
        return;
    }

    int err = AHardwareBuffer_lock(mHardwareBuffer, USAGE, -1, NULL, (void**)(&pDstBuffer));

    if (0 != err)//NO_ERROR
    {
        LOGE("GraphicBufferEx setBuffer AHardwareBuffer_lock failed. err = %d\n", err);
        return;
    }

    GPUIPBuffer dstBuffer;
    
    dstBuffer.width = getWidth();
    dstBuffer.height = getHeight();
    dstBuffer.format = getFormat();
    dstBuffer.stride = getStride();
    dstBuffer.pY = pDstBuffer;
    dstBuffer.pU = pDstBuffer + dstBuffer.stride * dstBuffer.height;
    dstBuffer.pV = dstBuffer.pU;

    switch(getFormat())
    {
        case 1://RGBA_8888
        {
            GPUIPBuffer_RGBA_COPY(srcBuffer, &dstBuffer);
            break;
        }

        case 3://RGB_888
        {
            GPUIPBuffer_RGB_COPY(srcBuffer, &dstBuffer);
            break;
        }

        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        {
            GPUIPBuffer_NV21_COPY(srcBuffer, &dstBuffer);
            break;
        }

        case AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420:
        {
            GPUIPBuffer_YV12_COPY(srcBuffer, &dstBuffer);
            break;
        }

        default:
        {
            LOGE("GraphicBufferEx setBuffer do not surpport format = %d.\n", getFormat());
        }
    }

    int fence = -1;
    err = AHardwareBuffer_unlock(mHardwareBuffer, &fence);
    
    if (0 != err)//NO_ERROR
    {
        LOGE("GraphicBufferEx setBuffer AHardwareBuffer_lock failed. err = %d\n", err);
        return;
    }
}

void GraphicBufferEx::getBuffer(
        GBDataCallBackFun pCallBackFun, GPUIPBuffer *dstBuffer)
{
    uint8_t *pSrcBuffer = NULL;

    if ((NULL == pCallBackFun) || (NULL == dstBuffer))
    {
        LOGE("GraphicBufferEx getBuffer ((NULL == pCallBackFun) || (NULL == dstBuffer))\n");
        return;
    }

    GPUIPBuffer srcBuffer;
    srcBuffer.width = getWidth();
    srcBuffer.height = getHeight();
    srcBuffer.format = getFormat();
    srcBuffer.stride = getStride();
    int err = 0;
    if(getSDKVersion()>=29 && lockPlanes_fPtr) {
          AHardwareBuffer_Planes outPlanes;
           err = lockPlanes_fPtr(mHardwareBuffer, USAGE, -1, NULL, &outPlanes);
          if(err) {
            LOGE("GraphicBufferEx getBuffer AHardwareBuffer_lockPlanes failed. err = %d\n", err);
          } else {
            // oes 纹理的格式是不透明的，默认是NV21,也没办法查到
            if(outPlanes.planeCount == 3) {
                bool  isNV12 = outPlanes.planes[1].pixelStride == 2;
                if(isNV12) {
                  srcBuffer.pY = outPlanes.planes[0].data;
                  srcBuffer.pU = outPlanes.planes[1].data;
                  srcBuffer.pV = srcBuffer.pU;
                  pCallBackFun = GPUIPBuffer_NV21_COPY;
                  pCallBackFun(&srcBuffer, dstBuffer);
                } else {
                  srcBuffer.pY = pSrcBuffer;
                  srcBuffer.pU = pSrcBuffer + srcBuffer.stride * srcBuffer.height;
                  srcBuffer.pV = pSrcBuffer + srcBuffer.stride * srcBuffer.height*5/4;
                  pCallBackFun = GPUIPBuffer_YV12_COPY;
                  pCallBackFun(&srcBuffer, dstBuffer);
                }
            } else {
              LOGE("not yuv");
            }
          }







    } else if(getSDKVersion() >= 26) {
          int err = AHardwareBuffer_lock(mHardwareBuffer, USAGE, -1, NULL, (void**)(&pSrcBuffer));

          if (0 != err)//NO_ERROR
          {
            LOGE("GraphicBufferEx getBuffer AHardwareBuffer_lock failed. err = %d\n", err);
            return;
          }

          srcBuffer.pY = pSrcBuffer;
          srcBuffer.pU = pSrcBuffer + srcBuffer.stride * srcBuffer.height;
          if(srcBuffer.format == AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420) {
            srcBuffer.pV = pSrcBuffer + srcBuffer.stride * srcBuffer.height*5/4;
          } else if(srcBuffer.format == 17) {
            srcBuffer.pV = srcBuffer.pU;
          }

          pCallBackFun(&srcBuffer, dstBuffer);
    }


    int fence = -1;
    err = AHardwareBuffer_unlock(mHardwareBuffer, &fence);

    if (0 != err)//NO_ERROR
    {
        LOGE("GraphicBufferEx getBuffer AHardwareBuffer_unlock failed. err = %d\n", err);
        return;
    }
}
