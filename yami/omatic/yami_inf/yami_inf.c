
/* use casees
   no X11 and no xcb, server use
   X11 and no xcb, must use X11
   X11 and xcb, use xcb if dri3 is present
*/

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <va/va.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>

#if defined(YAMI_INF_X11)
#include <va/va_x11.h>
#endif

#if defined(YAMI_INF_XCB)
#include <xcb/xcb.h>
#endif

#include <YamiVersion.h>
#include <VideoEncoderCapi.h>
#include <VideoDecoderCapi.h>

#include "yami_inf.h"

static VADisplay g_va_display = 0;
static VAConfigID g_vp_config = 0;
static VAContextID g_vp_context = 0;
static int g_dri3_major = 0;
static int g_dri3_minor = 0;
static int g_dri3_first_error = 0;
static int g_dri3_present = 0;
static int g_present_major = 0;
static int g_present_minor = 0;
static int g_present_first_error = 0;
static int g_present_present = 0;

struct yami_inf_enc_priv
{
    EncodeHandler encoder;
    int width;
    int height;
    int type;
    int flags;
    VASurfaceID va_surface[1];
    VASurfaceID fd_va_surface[1];
    int fd_yami_fourcc;
    int pad0;
    VAImage va_image[1];
    char *yuvdata;
};

struct yami_inf_dec_priv
{
    DecodeHandler decoder;
    VideoFormatInfo vfi;
    int width;
    int height;
    int type;
    int flags;
    int pixmap;
    int pixmap_width;
    int pixmap_height;
    VAContextID surface_ctx;
    VASurfaceID surface;
    int surface_width;
    int surface_height;
    void *display;
};

struct yami_inf_sur_priv
{
    int width;
    int height;
    int type;
    int flags;
    VAImage yuv_image;
    VASurfaceID yuv_surface;
    VAContextID rgb_surface_ctx;
    VASurfaceID rgb_surface;
    char *yuvdata;
};

/*****************************************************************************/
int
yami_get_version(int *version)
{
    *version = YI_VERSION_INT(YI_MAJOR, YI_MINOR);
    return YI_SUCCESS;
}

/*****************************************************************************/
int
yami_init(int type, void *display)
{
    int fd;
    int major_version;
    int minor_version;
    uint32_t yami_version;
    VAStatus va_status;
#if defined(YAMI_INF_X11)
    Display *dpy;

    dpy = (Display *) display;
#endif
    if (type == YI_TYPE_DRM)
    {
        fd = (int) (size_t) display;
        g_va_display = vaGetDisplayDRM(fd);
    }
    else if (type == YI_TYPE_X11)
    {
#if defined(YAMI_INF_X11)
        g_va_display = vaGetDisplay(dpy);
#else
        return YI_ERROR_UNIMP;
#endif
    }
    else
    {
        return YI_ERROR_TYPE;
    }
    va_status = vaInitialize(g_va_display, &major_version, &minor_version);
    if (va_status != VA_STATUS_SUCCESS)
    {
        return YI_ERROR_VAINITIALIZE;
    }
    va_status = vaCreateConfig(g_va_display, VAProfileNone,
                               VAEntrypointVideoProc, NULL, 0, &g_vp_config);
    if (va_status != VA_STATUS_SUCCESS)
    {
        return YI_ERROR_VACREATECONFIG;
    }
    va_status = vaCreateContext(g_va_display, g_vp_config, 0, 0, 0, NULL, 0,
                                &g_vp_context);
    if (va_status != VA_STATUS_SUCCESS)
    {
        return YI_ERROR_VACREATECONTEXT;
    }
    yamiGetApiVersion(&yami_version);
#if defined(YAMI_INF_X11) && defined(YAMI_INF_XCB)
    if (XQueryExtension(dpy, "DRI3", &g_dri3_major, &g_dri3_minor,
                        &g_dri3_first_error))
    {
        g_dri3_present = 1;
    }
    if (XQueryExtension(dpy, "Present", &g_present_major, &g_present_minor,
                        &g_present_first_error))
    {
        g_present_present = 1;
    }
    if (g_present_present == 0)
    {
        g_dri3_present = 0;
    }
#endif
    return YI_SUCCESS;
}

/*****************************************************************************/
int
yami_deinit(void)
{
    vaDestroyContext(g_va_display, g_vp_context);
    g_vp_context = 0;
    vaDestroyConfig(g_va_display, g_vp_config);
    g_vp_config = 0;
    vaTerminate(g_va_display);
    g_va_display = 0;
    return 0;
}

/*****************************************************************************/
int
yami_encoder_create(void **obj, int width, int height, int type, int flags)
{
    struct yami_inf_enc_priv *enc;
    VASurfaceAttrib attribs[1];
    VAStatus va_status;
    VAImageFormat va_image_format;
    NativeDisplay nd;
    YamiStatus yami_status;
    VideoParamsCommon encVideoParams;
    VideoConfigAVCStreamFormat streamFormat;

    enc = (struct yami_inf_enc_priv *)
          calloc(1, sizeof(struct yami_inf_enc_priv));
    if (enc == NULL)
    {
        return YI_ERROR_MEMORY;
    }
    if (type == YI_TYPE_H264)
    {
        enc->encoder = createEncoder(YAMI_MIME_H264);
        if (enc->encoder == NULL)
        {
            free(enc);
            return YI_ERROR_CREATEENCODER;
        }
    }
    else
    {
        free(enc);
        return YI_ERROR_TYPE;
    }
    memset(attribs, 0, sizeof(attribs));
    attribs[0].type = VASurfaceAttribPixelFormat;
    attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[0].value.type = VAGenericValueTypeInteger;
    attribs[0].value.value.i = VA_FOURCC_NV12;
    va_status = vaCreateSurfaces(g_va_display, VA_RT_FORMAT_YUV420,
                                 width, height,
                                 enc->va_surface, 1, attribs, 1);
    if (va_status != VA_STATUS_SUCCESS)
    {
        releaseEncoder(enc->encoder);
        free(enc);
        return YI_ERROR_VACREATESURFACES;
    }
    memset(&va_image_format, 0, sizeof(va_image_format));
    va_image_format.fourcc = VA_FOURCC_NV12;
    va_status = vaCreateImage(g_va_display, &va_image_format,
                              width, height, enc->va_image);
    if (va_status != VA_STATUS_SUCCESS)
    {
        releaseEncoder(enc->encoder);
        vaDestroySurfaces(g_va_display, enc->va_surface, 1);
        free(enc);
        return YI_ERROR_VACREATEIMAGE;
    }
    memset(&nd, 0, sizeof(nd));
    nd.handle = (intptr_t) g_va_display;
    nd.type = NATIVE_DISPLAY_VA;
    encodeSetNativeDisplay(enc->encoder, &nd);
    memset(&encVideoParams, 0, sizeof(encVideoParams));
    encVideoParams.size = sizeof(VideoParamsCommon);
    yami_status = encodeGetParameters(enc->encoder, VideoParamsTypeCommon,
                                      &encVideoParams);
    if (yami_status != ENCODE_SUCCESS)
    {
        vaDestroyImage(g_va_display, enc->va_image[0].image_id);
        vaDestroySurfaces(g_va_display, enc->va_surface, 1);
        releaseEncoder(enc->encoder);
        free(enc);
        return YI_ERROR_ENCODEGETPARAMETERS;
    }
    encVideoParams.size = sizeof(VideoParamsCommon);
    encVideoParams.resolution.width = width;
    encVideoParams.resolution.height = height;
    encVideoParams.rcMode = RATE_CONTROL_CQP;
    encVideoParams.rcParams.initQP = 28;
    encVideoParams.intraPeriod = 1024;
    if (type == YI_TYPE_H264)
    {
        switch (flags & YI_H264_ENC_FLAGS_PROFILE_MASK)
        {
            case YI_H264_ENC_FLAGS_PROFILE_HIGH:
                encVideoParams.profile = VAProfileH264High;
                break;
            case YI_H264_ENC_FLAGS_PROFILE_MAIN:
                encVideoParams.profile = VAProfileH264Main;
                break;
            default:
                encVideoParams.profile = VAProfileH264ConstrainedBaseline;
                break;
        }
    }
    yami_status = encodeSetParameters(enc->encoder, VideoParamsTypeCommon,
                                      &encVideoParams);
    if (yami_status != ENCODE_SUCCESS)
    {
        vaDestroyImage(g_va_display, enc->va_image[0].image_id);
        vaDestroySurfaces(g_va_display, enc->va_surface, 1);
        releaseEncoder(enc->encoder);
        free(enc);
        return YI_ERROR_ENCODESETPARAMETERS;
    }
    memset(&streamFormat, 0, sizeof(streamFormat));
    streamFormat.size = sizeof(VideoConfigAVCStreamFormat);
    yami_status = encodeGetParameters(enc->encoder,
                                      VideoConfigTypeAVCStreamFormat,
                                      &streamFormat);
    if (yami_status != ENCODE_SUCCESS)
    {
        vaDestroyImage(g_va_display, enc->va_image[0].image_id);
        vaDestroySurfaces(g_va_display, enc->va_surface, 1);
        releaseEncoder(enc->encoder);
        free(enc);
        return YI_ERROR_ENCODEGETPARAMETERS;
    }
    streamFormat.streamFormat = AVC_STREAM_FORMAT_ANNEXB;
    yami_status = encodeSetParameters(enc->encoder,
                                      VideoConfigTypeAVCStreamFormat,
                                      &streamFormat);
    if (yami_status != ENCODE_SUCCESS)
    {
        vaDestroyImage(g_va_display, enc->va_image[0].image_id);
        vaDestroySurfaces(g_va_display, enc->va_surface, 1);
        releaseEncoder(enc->encoder);
        free(enc);
        return YI_ERROR_ENCODESETPARAMETERS;
    }
    yami_status = encodeStart(enc->encoder);
    if (yami_status != ENCODE_SUCCESS)
    {
        vaDestroyImage(g_va_display, enc->va_image[0].image_id);
        vaDestroySurfaces(g_va_display, enc->va_surface, 1);
        releaseEncoder(enc->encoder);
        free(enc);
        return YI_ERROR_ENCODESTART;
    }
    enc->width = width;
    enc->height = height;
    enc->type = type;
    enc->flags = flags;
    *obj = enc;
    return YI_SUCCESS;
}

/*****************************************************************************/
int
yami_encoder_delete(void *obj)
{
    struct yami_inf_enc_priv *enc;

    enc = (struct yami_inf_enc_priv *) obj;
    if (enc == NULL)
    {
        return YI_SUCCESS;
    }
    encodeStop(enc->encoder);
    vaDestroyImage(g_va_display, enc->va_image[0].image_id);
    vaDestroySurfaces(g_va_display, enc->va_surface, 1);
    releaseEncoder(enc->encoder);
    free(enc);
    return YI_SUCCESS;
}

/*****************************************************************************/
int
yami_encoder_get_width(void *obj, int *width)
{
    struct yami_inf_enc_priv *enc;

    enc = (struct yami_inf_enc_priv *) obj;
    *width = enc->width;
    return YI_SUCCESS;
}

/*****************************************************************************/
int
yami_encoder_get_height(void *obj, int *height)
{
    struct yami_inf_enc_priv *enc;

    enc = (struct yami_inf_enc_priv *) obj;
    *height = enc->height;
    return YI_SUCCESS;
}

/*****************************************************************************/
int
yami_encoder_resize(void *obj, int width, int height)
{
    struct yami_inf_enc_priv *enc;
    VideoParamsCommon encVideoParams;
    YamiStatus yami_status;
    VAStatus va_status;
    VAImageFormat va_image_format;
    VASurfaceAttrib attribs[1];

    enc = (struct yami_inf_enc_priv *) obj;
    encodeStop(enc->encoder);
    memset(&encVideoParams, 0, sizeof(encVideoParams));
    encVideoParams.size = sizeof(VideoParamsCommon);
    yami_status = encodeGetParameters(enc->encoder, VideoParamsTypeCommon,
                                      &encVideoParams);
    if (yami_status != ENCODE_SUCCESS)
    {
        return YI_ERROR_ENCODEGETPARAMETERS;
    }
    encVideoParams.resolution.width = width;
    encVideoParams.resolution.height = height;
    encVideoParams.size = sizeof(VideoParamsCommon);
    yami_status = encodeSetParameters(enc->encoder, VideoParamsTypeCommon,
                                      &encVideoParams);
    if (yami_status != ENCODE_SUCCESS)
    {
        return YI_ERROR_ENCODESETPARAMETERS;
    }
    yami_status = encodeStart(enc->encoder);
    if (yami_status != ENCODE_SUCCESS)
    {
        return YI_ERROR_ENCODESTART;
    }
    va_status = vaDestroyImage(g_va_display, enc->va_image[0].image_id);
    if (va_status != VA_STATUS_SUCCESS)
    {
        return 1;
    }
    va_status = vaDestroySurfaces(g_va_display, enc->va_surface, 1);
    if (va_status != VA_STATUS_SUCCESS)
    {
        return 1;
    }
    memset(attribs, 0, sizeof(attribs));
    attribs[0].type = VASurfaceAttribPixelFormat;
    attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[0].value.type = VAGenericValueTypeInteger;
    attribs[0].value.value.i = VA_FOURCC_NV12;
    va_status = vaCreateSurfaces(g_va_display, VA_RT_FORMAT_YUV420,
                                 width, height,
                                 enc->va_surface, 1, attribs, 1);
    if (va_status != VA_STATUS_SUCCESS)
    {
        return YI_ERROR_VACREATESURFACES;
    }
    memset(&va_image_format, 0, sizeof(va_image_format));
    va_image_format.fourcc = VA_FOURCC_NV12;
    va_status = vaCreateImage(g_va_display, &va_image_format,
                              width, height, enc->va_image);
    if (va_status != VA_STATUS_SUCCESS)
    {
        return 1;
    }
    if (enc->fd_va_surface[0] != 0)
    {
        va_status = vaDestroySurfaces(g_va_display, enc->fd_va_surface, 1);
        if (va_status != VA_STATUS_SUCCESS)
        {
            return YI_ERROR_VACREATESURFACES;
        }
        enc->fd_va_surface[0] = 0;
    }
    enc->yuvdata = NULL;
    enc->width = width;
    enc->height = height;
    return YI_SUCCESS;
}

/*****************************************************************************/
int
yami_encoder_get_ybuffer(void *obj, void **ydata, int *ydata_stride_bytes)
{
    struct yami_inf_enc_priv *enc;
    VAStatus va_status;
    void *buf_ptr;

    enc = (struct yami_inf_enc_priv *) obj;
    if (enc->yuvdata == NULL)
    {
        va_status = vaMapBuffer(g_va_display, enc->va_image[0].buf, &buf_ptr);
        if (va_status != VA_STATUS_SUCCESS)
        {
            return YI_ERROR_VAMAPBUFFER;
        }
        enc->yuvdata = (char *) buf_ptr;
    }
    *ydata = enc->yuvdata + enc->va_image[0].offsets[0];
    *ydata_stride_bytes = enc->va_image[0].pitches[0];
    return YI_SUCCESS;
}

/*****************************************************************************/
int
yami_encoder_get_uvbuffer(void *obj, void **uvdata, int *uvdata_stride_bytes)
{
    struct yami_inf_enc_priv *enc;
    VAStatus va_status;
    void *buf_ptr;

    enc = (struct yami_inf_enc_priv *) obj;
    if (enc->yuvdata == NULL)
    {
        va_status = vaMapBuffer(g_va_display, enc->va_image[0].buf, &buf_ptr);
        if (va_status != VA_STATUS_SUCCESS)
        {
            return YI_ERROR_VAMAPBUFFER;
        }
        enc->yuvdata = (char *) buf_ptr;
    }
    *uvdata = enc->yuvdata + enc->va_image[0].offsets[1];
    *uvdata_stride_bytes = enc->va_image[0].pitches[1];
    return YI_SUCCESS;
}

/*****************************************************************************/
int
yami_encoder_set_fd_src(void *obj, int fd, int fd_width, int fd_height,
                        int fd_stride, int fd_size, int fd_bpp)
{
    struct yami_inf_enc_priv *enc;
    VASurfaceAttribExternalBuffers external;
    VASurfaceAttrib attribs[2];
    unsigned long fd_handle;
    VAStatus va_status;
    VASurfaceID surface;
    int fourcc;

    enc = (struct yami_inf_enc_priv *) obj;
    fd_handle = (unsigned long) fd;
    memset(&external, 0, sizeof(external));
    external.pixel_format = VA_FOURCC_BGRX;
    external.width = fd_width;
    external.height = fd_height;
    external.data_size = fd_size;
    external.num_planes = 1;
    external.pitches[0] = fd_stride;
    external.buffers = &fd_handle;
    external.num_buffers = 1;
    memset(attribs, 0, sizeof(attribs));
    attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[0].type = VASurfaceAttribMemoryType;
    attribs[0].value.type = VAGenericValueTypeInteger;
    attribs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
    attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[1].type = VASurfaceAttribExternalBufferDescriptor;
    attribs[1].value.type = VAGenericValueTypePointer;
    attribs[1].value.value.p = &external;
    switch (fd_bpp)
    {
        case YI_NV12:
            external.pixel_format = VA_FOURCC_NV12;
            external.num_planes = 2;
            external.pitches[1] = fd_stride;
            external.offsets[1] = fd_height * fd_stride;
            va_status = vaCreateSurfaces(g_va_display, VA_RT_FORMAT_YUV420,
                                         fd_width, fd_height, &surface, 1,
                                         attribs, 2);
            fourcc = YAMI_FOURCC_NV12;
            break;
        case YI_YUY2:
            external.pixel_format = VA_FOURCC_YUY2;
            va_status = vaCreateSurfaces(g_va_display, VA_RT_FORMAT_YUV422,
                                         fd_width, fd_height, &surface, 1,
                                         attribs, 2);
            fourcc = YAMI_FOURCC_YUY2;
            break;
        default:
            va_status = vaCreateSurfaces(g_va_display, VA_RT_FORMAT_RGB32,
                                         fd_width, fd_height, &surface, 1,
                                         attribs, 2);
            fourcc = YAMI_FOURCC_RGBX;
            break;
    }
    if (va_status != VA_STATUS_SUCCESS)
    {
        return YI_ERROR_VACREATESURFACES;
    }
    if (enc->fd_va_surface[0] != 0)
    {
        vaDestroySurfaces(g_va_display, enc->fd_va_surface, 1);
    }
    enc->fd_va_surface[0] = surface;
    enc->fd_yami_fourcc = fourcc;
    return YI_SUCCESS;
}

/*****************************************************************************/
int
yami_encoder_encode(void *obj, void *cdata,
                    int *cdata_max_bytes, int force_key_frame)
{
    struct yami_inf_enc_priv *enc;
    VAStatus va_status;
    VideoFrame yami_vf;
    YamiStatus yami_status;
    VideoEncOutputBuffer outb;
    int width;
    int height;

    enc = (struct yami_inf_enc_priv *) obj;
    if (enc->yuvdata != NULL)
    {
        va_status = vaUnmapBuffer(g_va_display, enc->va_image[0].buf);
        if (va_status != VA_STATUS_SUCCESS)
        {
            return YI_ERROR_VAUNMAPBUFFER;
        }
        enc->yuvdata = NULL;
    }
    memset(&yami_vf, 0, sizeof(yami_vf));
    yami_vf.crop.width = enc->width;
    yami_vf.crop.width = enc->height;
    yami_vf.flags = force_key_frame ? VIDEO_FRAME_FLAGS_KEY : 0;
    if (enc->fd_va_surface[0] != 0)
    {
        yami_vf.surface = enc->fd_va_surface[0];
        yami_vf.fourcc = enc->fd_yami_fourcc;
    }
    else
    {
        width = enc->va_image[0].width;
        height = enc->va_image[0].height;
        va_status = vaPutImage(g_va_display, enc->va_surface[0],
                               enc->va_image[0].image_id,
                               0, 0, width, height,
                               0, 0, width, height);
        if (va_status != VA_STATUS_SUCCESS)
        {
            return YI_ERROR_VAPUTIMAGE;
        }
        yami_vf.surface = enc->va_surface[0];
        yami_vf.fourcc = YAMI_FOURCC_NV12;
    }
    va_status = vaSyncSurface(g_va_display, yami_vf.surface);
    if (va_status != VA_STATUS_SUCCESS)
    {
        return YI_ERROR_VASYNCSURFACE;
    }
    yami_status = encodeEncode(enc->encoder, &yami_vf);
    if (yami_status != YAMI_SUCCESS)
    {
        return YI_ERROR_ENCODEENCODE;
    }
    memset(&outb, 0, sizeof(outb));
    outb.data = (unsigned char *) cdata;
    outb.bufferSize = *cdata_max_bytes;
    outb.format = OUTPUT_EVERYTHING;
    yami_status = encodeGetOutput(enc->encoder, &outb, 1);
    if (yami_status != 0)
    {
        return YI_ERROR_ENCODEGETOUTPUT;
    }
    *cdata_max_bytes = outb.dataSize;
    return YI_SUCCESS;
}

/*****************************************************************************/
int
yami_decoder_create(void **obj, int width, int height, int type, int flags)
{
    struct yami_inf_dec_priv *dec;
    NativeDisplay nd;
    VideoConfigBuffer cb;

    dec = (struct yami_inf_dec_priv *)
          calloc(1, sizeof(struct yami_inf_dec_priv));
    if (dec == NULL)
    {
        return YI_ERROR_MEMORY;
    }
    if (type == YI_TYPE_H264)
    {
        dec->decoder = createDecoder(YAMI_MIME_H264);
        if (dec->decoder == NULL)
        {
            free(dec);
            return YI_ERROR_CREATEDECODER;
        }
    }
    else if (type == YI_TYPE_MPEG2)
    {
        dec->decoder = createDecoder(YAMI_MIME_MPEG2);
        if (dec->decoder == NULL)
        {
            free(dec);
            return YI_ERROR_CREATEDECODER;
        }
    }
    else
    {
        free(dec);
        return YI_ERROR_UNIMP;
    }
    memset(&nd, 0, sizeof(nd));
    nd.type = NATIVE_DISPLAY_VA;
    nd.handle = (long) g_va_display;
    decodeSetNativeDisplay(dec->decoder, &nd);
    memset(&cb, 0, sizeof(cb));
    cb.profile = VAProfileNone;
    if ((type == YI_TYPE_H264) && (flags & YI_H264_DEC_FLAG_LOWLATENCY))
    {
        cb.enableLowLatency = 1;
    }
    if (decodeStart(dec->decoder, &cb) != YAMI_SUCCESS)
    {
        releaseDecoder(dec->decoder);
        free(dec);
        return YI_ERROR_CREATEDECODER;
    }
    dec->width = width;
    dec->height = height;
    *obj = dec;
    return YI_SUCCESS;
}

/*****************************************************************************/
int
yami_decoder_delete(void *obj)
{
    struct yami_inf_dec_priv *dec;

    dec = (struct yami_inf_dec_priv *) obj;
    if (dec == NULL)
    {
        return YI_SUCCESS;
    }
    if ((dec->display != NULL) && (dec->pixmap != 0))
    {
#if defined(YAMI_INF_X11)
        XFreePixmap((Display *) (dec->display), (Pixmap) (dec->pixmap));
#endif
    }
    releaseDecoder(dec->decoder);
    if (dec->surface_ctx != 0)
    {
        vaDestroyContext(g_va_display, dec->surface_ctx);
    }
    if (dec->surface != 0)
    {
        vaDestroySurfaces(g_va_display, &dec->surface, 1);
    }
    free(dec);
    return YI_SUCCESS;
}

/*****************************************************************************/
int
yami_decoder_decode(void *obj, void *cdata, int cdata_bytes)
{
    return yami_decoder_decode_time(obj, cdata, cdata_bytes, 0);
}

/*****************************************************************************/
int
yami_decoder_decode_time(void *obj, void *cdata, int cdata_bytes,
                         YI_INT64 time)
{
    struct yami_inf_dec_priv *dec;
    YamiStatus yami_status;
    VideoDecodeBuffer ib;
    const VideoFormatInfo *fi;

    dec = (struct yami_inf_dec_priv *) obj;
    memset(&ib, 0, sizeof(ib));
    ib.data = cdata;
    ib.size = cdata_bytes;
    ib.flag = VIDEO_DECODE_BUFFER_FLAG_FRAME_END;
    ib.timeStamp = time;
    yami_status = decodeDecode(dec->decoder, &ib);
    if (yami_status == YAMI_DECODE_FORMAT_CHANGE)
    {
        fi = decodeGetFormatInfo(dec->decoder);
        if (fi == NULL)
        {
            return YI_ERROR_DECODEGETFORMATINFO;
        }
        dec->vfi = *fi;
        yami_status = decodeDecode(dec->decoder, &ib);
    }
    if (yami_status != DECODE_SUCCESS)
    {
        return YI_ERROR_DECODEDECODE;
    }
    return YI_SUCCESS;
}

/*****************************************************************************/
int
yami_decoder_get_pixmap(void *obj, void* display,
                        int width, int height, int *pixmap)
{
#if defined(YAMI_INF_X11)
    struct yami_inf_dec_priv *dec;
    VideoFrame *vf;
    VAStatus va_status;
    Display *dpy;
    int depth;
    Window root_window;

    dec = (struct yami_inf_dec_priv *) obj;
    vf = decodeGetOutput(dec->decoder);
    if (vf == NULL)
    {
        return YI_ERROR_DECODEGETOUTPUT;
    }
    if (vf->surface == 0)
    {
        vf->free(vf);
        return YI_ERROR_DECODEGETOUTPUT;
    }
    if ((dec->pixmap_width != width) || (dec->pixmap_height != height))
    {
        dpy = (Display *) display;
        if (dec->pixmap != 0)
        {
            XFreePixmap(dpy, dec->pixmap);
        }
        root_window = XDefaultRootWindow(dpy);
        depth = DefaultDepth(dpy, DefaultScreen(dpy));
        dec->pixmap = (int) XCreatePixmap(dpy, root_window,
                                          width, height, depth);
        dec->pixmap_width = width;
        dec->pixmap_height = height;
        dec->display = display;
    }
    if (dec->pixmap == 0)
    {
        vf->free(vf);
        return YI_ERROR_OTHER;
    }
    va_status = vaPutSurface(g_va_display,
                             vf->surface, /* src */
                             (Pixmap) (dec->pixmap), /* dst */
                             0, 0, dec->width, dec->height, /* src */
                             0, 0, width, height, /* dst */
                             0, 0, 0);
    vf->free(vf);
    if (va_status != VA_STATUS_SUCCESS)
    {
        return YI_ERROR_VAPUTSURFACE;
    }
    *pixmap = dec->pixmap;
    return YI_SUCCESS;
#else
    return YI_ERROR_UNIMP;
#endif
}

/*****************************************************************************/
int
yami_decoder_get_fd_dst(void *obj, int *fd, int *fd_width, int *fd_height,
                        int *fd_stride, int *fd_size, int *fd_bpp,
                        YI_INT64* fd_time)
{
    struct yami_inf_dec_priv *dec;
    VideoFrame *vf;
    VAStatus va_status;
    VAImage image;
    VABufferInfo bufferInfo;
    VASurfaceAttrib forcc;
    int bytes;
    int index;
    VABufferID pipeline_buf;
    VAProcPipelineParameterBuffer* pipeline_param;
    VABufferType buf_type;
    void *buf;
    VADRMPRIMESurfaceDescriptor drm_desc;
    YI_INT64 time;
    VARectangle input_region;
    VARectangle output_region;

    dec = (struct yami_inf_dec_priv *) obj;
    vf = decodeGetOutput(dec->decoder);
    if (vf == NULL)
    {
        return YI_ERROR_DECODEGETOUTPUT;
    }
    if (vf->surface == 0)
    {
        vf->free(vf);
        return YI_ERROR_DECODEGETOUTPUT;
    }
    if ((dec->surface_width != dec->vfi.width) ||
        (dec->surface_height != dec->vfi.height))
    {
        if (dec->surface_ctx != 0)
        {
            vaDestroyContext(g_va_display, dec->surface_ctx);
        }
        if (dec->surface != 0)
        {
            vaDestroySurfaces(g_va_display, &dec->surface, 1);
        }
        dec->surface_ctx = 0;
        dec->surface = 0;
        memset(&forcc, 0, sizeof(forcc));
        forcc.type = VASurfaceAttribPixelFormat;
        forcc.flags = VA_SURFACE_ATTRIB_SETTABLE;
        forcc.value.type = VAGenericValueTypeInteger;
        forcc.value.value.i = VA_FOURCC_BGRX;
        va_status = vaCreateSurfaces(g_va_display, VA_RT_FORMAT_RGB32,
                                     dec->vfi.width,
                                     dec->vfi.height,
                                     &dec->surface, 1, &forcc, 1);
        if (va_status != VA_STATUS_SUCCESS)
        {
            vf->free(vf);
            return YI_ERROR_VACREATESURFACES;
        }
        va_status = vaCreateContext(g_va_display, g_vp_config,
                                    dec->vfi.width,
                                    dec->vfi.height,
                                    VA_PROGRESSIVE, &dec->surface, 1,
                                    &dec->surface_ctx);
        if (va_status != VA_STATUS_SUCCESS)
        {
            vaDestroySurfaces(g_va_display, &dec->surface, 1);
            dec->surface = 0;
            vf->free(vf);
            return YI_ERROR_VACREATECONTEXT;
        }
        dec->surface_width = dec->vfi.width;
        dec->surface_height = dec->vfi.height;
    }
    buf_type = VAProcPipelineParameterBufferType;
    bytes = sizeof(VAProcPipelineParameterBuffer);
    va_status = vaCreateBuffer(g_va_display, dec->surface_ctx, buf_type,
                               bytes, 1, NULL, &pipeline_buf);
    if (va_status != VA_STATUS_SUCCESS)
    {
        vf->free(vf);
        return YI_ERROR_VACREATEBUFFER;
    }
    buf = NULL;
    va_status = vaMapBuffer(g_va_display, pipeline_buf, &buf);
    if (va_status != VA_STATUS_SUCCESS)
    {
        vaDestroyBuffer(g_va_display, pipeline_buf);
        vf->free(vf);
        return YI_ERROR_VAMAPBUFFER;
    }
    memset(&input_region, 0, sizeof(input_region));
    input_region.width = dec->width;
    input_region.height = dec->height;
    memset(&output_region, 0, sizeof(output_region));
    output_region.width = dec->width;
    output_region.height = dec->height;
    pipeline_param = (VAProcPipelineParameterBuffer*)buf;
    memset(pipeline_param, 0, sizeof(VAProcPipelineParameterBuffer));
    pipeline_param->surface = vf->surface;
    pipeline_param->surface_region = &input_region;
    pipeline_param->output_region = &output_region;
    vaUnmapBuffer(g_va_display, pipeline_buf);
    va_status = vaBeginPicture(g_va_display, dec->surface_ctx, dec->surface);
    if (va_status != VA_STATUS_SUCCESS)
    {
        vaDestroyBuffer(g_va_display, pipeline_buf);
        vf->free(vf);
        return YI_ERROR_VABEGINPICTURE;
    }
    va_status = vaRenderPicture(g_va_display, dec->surface_ctx,
                                &pipeline_buf, 1);
    if (va_status != VA_STATUS_SUCCESS)
    {
        vaDestroyBuffer(g_va_display, pipeline_buf);
        vf->free(vf);
        return YI_ERROR_VARENDERPICTURE;
    }
    va_status = vaEndPicture(g_va_display, dec->surface_ctx);
    if (va_status != VA_STATUS_SUCCESS)
    {
        vaDestroyBuffer(g_va_display, pipeline_buf);
        vf->free(vf);
        return YI_ERROR_VAENDPICTURE;
    }
    va_status = vaSyncSurface(g_va_display, dec->surface);
    if (va_status != VA_STATUS_SUCCESS)
    {
        vaDestroyBuffer(g_va_display, pipeline_buf);
        vf->free(vf);
        return YI_ERROR_VASYNCSURFACE;
    }
    vaDestroyBuffer(g_va_display, pipeline_buf);
    time = vf->timeStamp;
    vf->free(vf);
    memset(&drm_desc, 0, sizeof(drm_desc));
    va_status = vaExportSurfaceHandle(g_va_display, dec->surface,
                                      VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                      0, &drm_desc);
    if (va_status != VA_STATUS_SUCCESS)
    {
        return YI_ERROR_VAEXPORTSURFACE;
    }
    if ((drm_desc.num_objects != 1) || (drm_desc.num_layers != 1))
    {
        for (index = 0; index < drm_desc.num_objects; index++)
        {
            close(drm_desc.objects[index].fd);
        }
        return YI_ERROR_OTHER;
    }
    *fd = drm_desc.objects[0].fd;
    *fd_width = drm_desc.width;
    *fd_height = drm_desc.height;
    *fd_stride = drm_desc.layers[0].pitch[0];
    *fd_size = drm_desc.objects[0].size;
    *fd_bpp = 0;
    *fd_time = time;
    return YI_SUCCESS;
}

/*****************************************************************************/
int
yami_surface_create(void **obj, int width, int height, int type, int flags)
{
    struct yami_inf_sur_priv *sur;
    VASurfaceAttrib attribs;
    VAStatus va_status;
    VAImageFormat va_image_format;

    sur = (struct yami_inf_sur_priv *)
          calloc(1, sizeof(struct yami_inf_sur_priv));
    if (sur == NULL)
    {
        return YI_ERROR_MEMORY;
    }
    memset(&attribs, 0, sizeof(attribs));
    attribs.type = VASurfaceAttribPixelFormat;
    attribs.flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs.value.type = VAGenericValueTypeInteger;
    attribs.value.value.i = VA_FOURCC_NV12;
    va_status = vaCreateSurfaces(g_va_display, VA_RT_FORMAT_YUV420,
                                 width, height,
                                 &(sur->yuv_surface), 1, &attribs, 1);
    if (va_status != VA_STATUS_SUCCESS)
    {
        free(sur);
        return YI_ERROR_VACREATESURFACES;
    }
    memset(&va_image_format, 0, sizeof(va_image_format));
    va_image_format.fourcc = VA_FOURCC_NV12;
    va_status = vaCreateImage(g_va_display, &va_image_format,
                              width, height, &(sur->yuv_image));
    if (va_status != VA_STATUS_SUCCESS)
    {
        vaDestroySurfaces(g_va_display, &(sur->yuv_surface), 1);
        free(sur);
        return YI_ERROR_VACREATEIMAGE;
    }

    memset(&attribs, 0, sizeof(attribs));
    attribs.type = VASurfaceAttribPixelFormat;
    attribs.flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs.value.type = VAGenericValueTypeInteger;
    attribs.value.value.i = VA_FOURCC_BGRX;
    va_status = vaCreateSurfaces(g_va_display, VA_RT_FORMAT_RGB32,
                                 width, height,
                                 &(sur->rgb_surface), 1, &attribs, 1);
    if (va_status != VA_STATUS_SUCCESS)
    {
        vaDestroyImage(g_va_display, sur->yuv_image.image_id);
        vaDestroySurfaces(g_va_display, &(sur->yuv_surface), 1);
        free(sur);
        return YI_ERROR_VACREATESURFACES;
    }
    va_status = vaCreateContext(g_va_display, g_vp_config,
                                width, height,
                                VA_PROGRESSIVE, &(sur->rgb_surface), 1,
                                &(sur->rgb_surface_ctx));
    if (va_status != VA_STATUS_SUCCESS)
    {
        vaDestroySurfaces(g_va_display, &(sur->rgb_surface), 1);
        vaDestroyImage(g_va_display, sur->yuv_image.image_id);
        vaDestroySurfaces(g_va_display, &(sur->yuv_surface), 1);
        free(sur);
        return YI_ERROR_VACREATECONTEXT;
    }
    sur->width = width;
    sur->height = height;
    sur->type = type;
    sur->flags = flags;
    *obj = sur;
    return YI_SUCCESS;
}

/*****************************************************************************/
int
yami_surface_delete(void *obj)
{
    struct yami_inf_sur_priv *sur;

    sur = (struct yami_inf_sur_priv *) obj;
    if (sur == NULL)
    {
        return YI_SUCCESS;
    }
    vaDestroyContext(g_va_display, sur->rgb_surface_ctx);
    vaDestroySurfaces(g_va_display, &(sur->rgb_surface), 1);
    vaDestroyImage(g_va_display, sur->yuv_image.image_id);
    vaDestroySurfaces(g_va_display, &(sur->yuv_surface), 1);
    free(sur);
    return YI_SUCCESS;
}

/*****************************************************************************/
int
yami_surface_get_ybuffer(void *obj, void **ydata, int *ydata_stride_bytes)
{
    struct yami_inf_sur_priv *sur;
    VAStatus va_status;
    void *buf_ptr;

    sur = (struct yami_inf_sur_priv *) obj;
    if (sur->yuvdata == NULL)
    {
        va_status = vaMapBuffer(g_va_display, sur->yuv_image.buf, &buf_ptr);
        if (va_status != VA_STATUS_SUCCESS)
        {
            return YI_ERROR_VAMAPBUFFER;
        }
        sur->yuvdata = (char *) buf_ptr;
    }
    *ydata = sur->yuvdata + sur->yuv_image.offsets[0];
    *ydata_stride_bytes = sur->yuv_image.pitches[0];
    return YI_SUCCESS;
}

/*****************************************************************************/
int
yami_surface_get_uvbuffer(void *obj, void **uvdata, int *uvdata_stride_bytes)
{
    struct yami_inf_sur_priv *sur;
    VAStatus va_status;
    void *buf_ptr;

    sur = (struct yami_inf_sur_priv *) obj;
    if (sur->yuvdata == NULL)
    {
        va_status = vaMapBuffer(g_va_display, sur->yuv_image.buf, &buf_ptr);
        if (va_status != VA_STATUS_SUCCESS)
        {
            return YI_ERROR_VAMAPBUFFER;
        }
        sur->yuvdata = (char *) buf_ptr;
    }
    *uvdata = sur->yuvdata + sur->yuv_image.offsets[1];
    *uvdata_stride_bytes = sur->yuv_image.pitches[1];
    return YI_SUCCESS;
}

/*****************************************************************************/
int
yami_surface_get_fd_dst(void *obj, int *fd, int *fd_width, int *fd_height,
                        int *fd_stride, int *fd_size, int *fd_bpp)
{
    struct yami_inf_sur_priv *sur;
    VAStatus va_status;
    VAImage image;
    VABufferInfo bufferInfo;
    VASurfaceAttrib forcc;
    int bytes;
    int index;
    VABufferID pipeline_buf;
    VAProcPipelineParameterBuffer* pipeline_param;
    VABufferType buf_type;
    void *buf;
    VADRMPRIMESurfaceDescriptor drm_desc;

    sur = (struct yami_inf_sur_priv *) obj;
    if (sur->yuvdata != NULL)
    {
        va_status = vaUnmapBuffer(g_va_display, sur->yuv_image.buf);
        if (va_status != VA_STATUS_SUCCESS)
        {
            return YI_ERROR_VAUNMAPBUFFER;
        }
        sur->yuvdata = NULL;
    }
    va_status = vaPutImage(g_va_display, sur->yuv_surface,
                           sur->yuv_image.image_id,
                           0, 0, sur->width, sur->height,
                           0, 0, sur->width, sur->height);
    if (va_status != VA_STATUS_SUCCESS)
    {
        return YI_ERROR_VAPUTIMAGE;
    }
    buf_type = VAProcPipelineParameterBufferType;
    bytes = sizeof(VAProcPipelineParameterBuffer);
    va_status = vaCreateBuffer(g_va_display, sur->rgb_surface_ctx, buf_type,
                               bytes, 1, NULL, &pipeline_buf);
    if (va_status != VA_STATUS_SUCCESS)
    {
        return YI_ERROR_VACREATEBUFFER;
    }
    buf = NULL;
    va_status = vaMapBuffer(g_va_display, pipeline_buf, &buf);
    if (va_status != VA_STATUS_SUCCESS)
    {
        vaDestroyBuffer(g_va_display, pipeline_buf);
        return YI_ERROR_VAMAPBUFFER;
    }
    pipeline_param = (VAProcPipelineParameterBuffer*)buf;
    memset(pipeline_param, 0, sizeof(VAProcPipelineParameterBuffer));
    pipeline_param->surface = sur->yuv_surface;
    vaUnmapBuffer(g_va_display, pipeline_buf);
    va_status = vaBeginPicture(g_va_display, sur->rgb_surface_ctx,
                               sur->rgb_surface);
    if (va_status != VA_STATUS_SUCCESS)
    {
        vaDestroyBuffer(g_va_display, pipeline_buf);
        return YI_ERROR_VABEGINPICTURE;
    }
    va_status = vaRenderPicture(g_va_display, sur->rgb_surface_ctx,
                                &pipeline_buf, 1);
    if (va_status != VA_STATUS_SUCCESS)
    {
        vaDestroyBuffer(g_va_display, pipeline_buf);
        return YI_ERROR_VARENDERPICTURE;
    }
    va_status = vaEndPicture(g_va_display, sur->rgb_surface_ctx);
    if (va_status != VA_STATUS_SUCCESS)
    {
        vaDestroyBuffer(g_va_display, pipeline_buf);
        return YI_ERROR_VAENDPICTURE;
    }
    va_status = vaSyncSurface(g_va_display, sur->rgb_surface);
    if (va_status != VA_STATUS_SUCCESS)
    {
        vaDestroyBuffer(g_va_display, pipeline_buf);
        return YI_ERROR_VASYNCSURFACE;
    }
    vaDestroyBuffer(g_va_display, pipeline_buf);
    memset(&drm_desc, 0, sizeof(drm_desc));
    va_status = vaExportSurfaceHandle(g_va_display, sur->rgb_surface,
                                      VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                      0, &drm_desc);
    if (va_status != VA_STATUS_SUCCESS)
    {
        return YI_ERROR_VAEXPORTSURFACE;
    }
    if ((drm_desc.num_objects != 1) || (drm_desc.num_layers != 1))
    {
        for (index = 0; index < drm_desc.num_objects; index++)
        {
            close(drm_desc.objects[index].fd);
        }
        return YI_ERROR_OTHER;
    }
    *fd = drm_desc.objects[0].fd;
    *fd_width = drm_desc.width;
    *fd_height = drm_desc.height;
    *fd_stride = drm_desc.layers[0].pitch[0];
    *fd_size = drm_desc.objects[0].size;
    *fd_bpp = 0;
    return YI_SUCCESS;
}

/*****************************************************************************/
int
yami_get_funcs(struct yami_funcs *funcs, int version)
{
    if (version >= YI_VERSION_INT(0, 2))
    {
        funcs->yami_get_version = yami_get_version;
        funcs->yami_init = yami_init;
        funcs->yami_deinit = yami_deinit;
        /* encoder */
        funcs->yami_encoder_create = yami_encoder_create;
        funcs->yami_encoder_delete = yami_encoder_delete;
        funcs->yami_encoder_get_width = yami_encoder_get_width;
        funcs->yami_encoder_get_height = yami_encoder_get_height;
        funcs->yami_encoder_resize = yami_encoder_resize;
        funcs->yami_encoder_get_ybuffer = yami_encoder_get_ybuffer;
        funcs->yami_encoder_get_uvbuffer = yami_encoder_get_uvbuffer;
        funcs->yami_encoder_set_fd_src = yami_encoder_set_fd_src;
        funcs->yami_encoder_encode = yami_encoder_encode;
        /* decoder */
        funcs->yami_decoder_create = yami_decoder_create;
        funcs->yami_decoder_delete = yami_decoder_delete;
        funcs->yami_decoder_decode = yami_decoder_decode;
        funcs->yami_decoder_get_pixmap = yami_decoder_get_pixmap;
        funcs->yami_decoder_get_fd_dst = yami_decoder_get_fd_dst;
        /* surface */
        funcs->yami_surface_create = yami_surface_create;
        funcs->yami_surface_delete = yami_surface_delete;
        funcs->yami_surface_get_ybuffer = yami_surface_get_ybuffer;
        funcs->yami_surface_get_uvbuffer = yami_surface_get_uvbuffer;
        funcs->yami_surface_get_fd_dst = yami_surface_get_fd_dst;
        return YI_SUCCESS;
    }
    return YI_ERROR_OTHER;
}

