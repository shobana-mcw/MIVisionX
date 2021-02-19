/*
Copyright (c) 2015 - 2020 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "hip_common.h"
#include "hip_host_decls.h"

#define RGB2Y(R,G,B) ((R * 0.2126f) + (G * 0.7152f) + (B * 0.0722f))
#define RGB2U(R,G,B) ((R * -0.1146f) + (G * -0.3854f) + (B * 0.5f) + 128.0f)
#define RGB2V(R,G,B) ((R * 0.5f) + (G * -0.4542f) + (B * -0.0458f) + 128.0f)

#define YUV2R(Y,U,V) (Y + ((V - 128.0f) * 1.5748f))
#define YUV2G(Y,U,V) (Y - ((U - 128.0f) * 0.1873f) - ((V - 128.0f) * 0.4681f))
#define YUV2B(Y,U,V) (Y + ((U - 128.0f) * 1.8556f))

__device__ __forceinline__ float2 uchars_to_float2 (uint src) {
    return make_float2((float)(src & 0xFF), (float)((src & 0xFF00) >> 8));
}

__device__ __forceinline__ uint float2_to_uchars (float2 src) {
    return (((uint)src.x & 0xFF) | (((uint)src.y & 0xFF) << 8) );
}

__device__ __forceinline__ uint4 uchars_to_uint4 (unsigned int src) {
    return make_uint4((unsigned int)(src & 0xFF), (unsigned int)((src & 0xFF00) >> 8), (unsigned int)((src & 0xFF0000) >> 16), (unsigned int)((src & 0xFF000000) >> 24));
}

__device__ __forceinline__ unsigned int uint4_to_uchars (uint4 src) {
    return ((unsigned char)src.x & 0xFF) | (((unsigned char)src.y & 0xFF) << 8) | (((unsigned char)src.z & 0xFF) << 16) | (((unsigned char)src.w & 0xFF) << 24);
}

__device__ __forceinline__ uint2 uchars_to_uint2 (unsigned int src) {
    return make_uint2((unsigned int)(src & 0xFF), (unsigned int)((src & 0xFF00) >> 8));
}

__device__ __forceinline__ unsigned int uint2_to_uchars (uint2 src) {
    return (((unsigned char)src.x & 0xFF) | (((unsigned char)src.y & 0xFF) << 8));
}

// ----------------------------------------------------------------------------
// VxColorDepth kernels for hip backend
// ----------------------------------------------------------------------------

__global__ void __attribute__((visibility("default")))
Hip_ColorDepth_U8_S16_Wrap(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes,
    const uchar *pSrcImage, uint srcImageStrideInBytes,
    const int shift) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if (x >= dstWidth || y >= dstHeight) {
        return;
    }

    uint srcIdx =  y * srcImageStrideInBytes + x + x;
    uint dstIdx =  y * dstImageStrideInBytes + x;

    int4 src = *((int4 *)(&pSrcImage[srcIdx]));
    uint2 dst;

    int sr = shift;
    sr += 16;
    dst.x  = ((src.x   << 16) >> sr) & 0xff;
    dst.x |= ((src.x          >> sr) & 0xff) <<  8;
    dst.x |= (((src.y  << 16) >> sr) & 0xff) << 16;
    dst.x |= ((src.y          >> sr) & 0xff) << 24;
    dst.y  = ((src.z   << 16) >> sr) & 0xff;
    dst.y |= ((src.z          >> sr) & 0xff) <<  8;
    dst.y |= (((src.w  << 16) >> sr) & 0xff) << 16;
    dst.y |= ((src.w          >> sr) & 0xff) << 24;

    *((uint2 *)(&pDstImage[dstIdx])) = dst;
}
int HipExec_ColorDepth_U8_S16_Wrap(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_int16 *pHipSrcImage, vx_uint32 srcImageStrideInBytes,
    const vx_int32 shift) {
    int localThreads_x = 16;
    int localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ColorDepth_U8_S16_Wrap, dim3(ceil((float)globalThreads_x/localThreads_x), ceil((float)globalThreads_y/localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage, dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage, srcImageStrideInBytes, shift);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ColorDepth_U8_S16_Sat(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes,
    const uchar *pSrcImage, uint srcImageStrideInBytes,
    const int shift) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if (x >= dstWidth || y >= dstHeight) {
        return;
    }

    uint srcIdx =  y * srcImageStrideInBytes + x + x;
    uint dstIdx =  y * dstImageStrideInBytes + x;

    int4 src = *((int4 *)(&pSrcImage[srcIdx]));
    uint2 dst;

    int sr = shift;
    sr += 16;
    float4 f;
    f.x = (float)((src.x << 16) >> sr);
    f.y = (float)( src.x        >> sr);
    f.z = (float)((src.y << 16) >> sr);
    f.w = (float)( src.y        >> sr);
    dst.x = pack_(f);
    f.x = (float)((src.z << 16) >> sr);
    f.y = (float)( src.z        >> sr);
    f.z = (float)((src.w << 16) >> sr);
    f.w = (float)( src.w        >> sr);
    dst.y = pack_(f);

    *((uint2 *)(&pDstImage[dstIdx])) = dst;
}
int HipExec_ColorDepth_U8_S16_Sat(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_int16 *pHipSrcImage, vx_uint32 srcImageStrideInBytes,
    const vx_int32 shift) {
    int localThreads_x = 16;
    int localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ColorDepth_U8_S16_Sat, dim3(ceil((float)globalThreads_x/localThreads_x), ceil((float)globalThreads_y/localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage , dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage, srcImageStrideInBytes, shift);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ColorDepth_S16_U8(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes,
    const uchar *pSrcImage, uint srcImageStrideInBytes,
    const int shift) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if (x >= dstWidth || y >= dstHeight) {
        return;
    }

    uint srcIdx =  y * srcImageStrideInBytes + x;
    uint dstIdx =  y * dstImageStrideInBytes + x + x;

    uint2 src = *((uint2 *)(&pSrcImage[srcIdx]));
    int4 dst;

    dst.x  =  (src.x & 0x000000ff) <<       shift ;
    dst.x |=  (src.x & 0x0000ff00) << ( 8 + shift);
    dst.y  =  (src.x & 0x00ff0000) >> (16 - shift);
    dst.y |=  (src.x & 0xff000000) >> ( 8 - shift);
    dst.z  =  (src.y & 0x000000ff) <<       shift ;
    dst.z |=  (src.y & 0x0000ff00) << ( 8 + shift);
    dst.w  =  (src.y & 0x00ff0000) >> (16 - shift);
    dst.w |=  (src.y & 0xff000000) >> ( 8 - shift);

    *((int4 *)(&pDstImage[dstIdx])) = dst;
}
int HipExec_ColorDepth_S16_U8(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_int16 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage, vx_uint32 srcImageStrideInBytes,
    const vx_int32 shift) {
    int localThreads_x = 16;
    int localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ColorDepth_S16_U8, dim3(ceil((float)globalThreads_x/localThreads_x), ceil((float)globalThreads_y/localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage , dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage, srcImageStrideInBytes, shift);

    return VX_SUCCESS;
}

// ----------------------------------------------------------------------------
// VxChannelExtract kernels for hip backend
// ----------------------------------------------------------------------------

__global__ void __attribute__((visibility("default")))
Hip_ChannelExtract_U8_U16_Pos0(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes,
    const uchar *pSrcImage, uint srcImageStrideInBytes) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if (x >= dstWidth || y >= dstHeight) {
        return;
    }

    unsigned int srcIdx = y * srcImageStrideInBytes + x + x;
    unsigned int dstIdx = y * dstImageStrideInBytes + x;

    uint4 src = *((uint4 *)(&pSrcImage[srcIdx]));
    uint2 dst;

    dst.x = pack_(make_float4(unpack0_(src.x), unpack2_(src.x), unpack0_(src.y), unpack2_(src.y)));
    dst.y = pack_(make_float4(unpack0_(src.z), unpack2_(src.z), unpack0_(src.w), unpack2_(src.w)));

    *((uint2 *)(&pDstImage[dstIdx])) = dst;
}
int HipExec_ChannelExtract_U8_U16_Pos0(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage1, vx_uint32 srcImage1StrideInBytes) {
    int localThreads_x = 16, localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ChannelExtract_U8_U16_Pos0, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage, dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage1, srcImage1StrideInBytes);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ChannelExtract_U8_U16_Pos1(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes,
    const uchar *pSrcImage, uint srcImageStrideInBytes) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if (x >= dstWidth || y >= dstHeight) {
        return;
    }

    unsigned int srcIdx = y * srcImageStrideInBytes + x + x;
    unsigned int dstIdx = y * dstImageStrideInBytes + x;

    uint4 src = *((uint4 *)(&pSrcImage[srcIdx]));
    uint2 dst;

    dst.x = pack_(make_float4(unpack1_(src.x), unpack3_(src.x), unpack1_(src.y), unpack3_(src.y)));
    dst.y = pack_(make_float4(unpack1_(src.z), unpack3_(src.z), unpack1_(src.w), unpack3_(src.w)));

    *((uint2 *)(&pDstImage[dstIdx])) = dst;
}
int HipExec_ChannelExtract_U8_U16_Pos1(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage1, vx_uint32 srcImage1StrideInBytes) {
    int localThreads_x = 16, localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ChannelExtract_U8_U16_Pos1, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage, dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage1, srcImage1StrideInBytes);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ChannelExtract_U8_U24_Pos0(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes,
    const uchar *pSrcImage1, uint srcImage1StrideInBytes) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if (x >= dstWidth || y >= dstHeight) {
        return;
    }

    uint src1Idx = y * srcImage1StrideInBytes + (x * 3);
    uint dstIdx  = y * dstImageStrideInBytes + x;

    d_uint6 src1 = *((d_uint6 *)(&pSrcImage1[src1Idx]));
    uint2 dst;

    dst.x = pack_((float4)(unpack0_(src1.data[0]), unpack3_(src1.data[0]), unpack2_(src1.data[1]), unpack1_(src1.data[2])));
    dst.y = pack_((float4)(unpack0_(src1.data[3]), unpack3_(src1.data[3]), unpack2_(src1.data[4]), unpack1_(src1.data[5])));

    *((uint2 *)(&pDstImage[dstIdx])) = dst;
}
int HipExec_ChannelExtract_U8_U24_Pos0(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage1, vx_uint32 srcImage1StrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ChannelExtract_U8_U24_Pos0, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage, dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage1, srcImage1StrideInBytes);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ChannelExtract_U8_U24_Pos1(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes,
    const uchar *pSrcImage1, uint srcImage1StrideInBytes) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if (x >= dstWidth || y >= dstHeight) {
        return;
    }

    uint src1Idx = y * srcImage1StrideInBytes + (x * 3);
    uint dstIdx  = y * dstImageStrideInBytes + x;

    d_uint6 src1 = *((d_uint6 *)(&pSrcImage1[src1Idx]));
    uint2 dst;

    dst.x = pack_((float4)(unpack1_(src1.data[0]), unpack0_(src1.data[1]), unpack3_(src1.data[1]), unpack2_(src1.data[2])));
    dst.y = pack_((float4)(unpack1_(src1.data[3]), unpack0_(src1.data[4]), unpack3_(src1.data[4]), unpack2_(src1.data[5])));

    *((uint2 *)(&pDstImage[dstIdx])) = dst;
}
int HipExec_ChannelExtract_U8_U24_Pos1(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage1, vx_uint32 srcImage1StrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ChannelExtract_U8_U24_Pos1, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage, dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage1, srcImage1StrideInBytes);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ChannelExtract_U8_U24_Pos2(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes,
    const uchar *pSrcImage1, uint srcImage1StrideInBytes) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if (x >= dstWidth || y >= dstHeight) {
        return;
    }

    uint src1Idx = y * srcImage1StrideInBytes + (x * 3);
    uint dstIdx  = y * dstImageStrideInBytes + x;

    d_uint6 src1 = *((d_uint6 *)(&pSrcImage1[src1Idx]));
    uint2 dst;

    dst.x = pack_((float4)(unpack2_(src1.data[0]), unpack1_(src1.data[1]), unpack0_(src1.data[2]), unpack3_(src1.data[2])));
    dst.y = pack_((float4)(unpack2_(src1.data[3]), unpack1_(src1.data[4]), unpack0_(src1.data[5]), unpack3_(src1.data[5])));

    *((uint2 *)(&pDstImage[dstIdx])) = dst;
}
int HipExec_ChannelExtract_U8_U24_Pos2(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage1, vx_uint32 srcImage1StrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ChannelExtract_U8_U24_Pos2, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage, dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage1, srcImage1StrideInBytes);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ChannelExtract_U8_U32_Pos0(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes,
    const uchar *pSrcImage1, uint srcImage1StrideInBytes) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if (x >= dstWidth || y >= dstHeight) {
        return;
    }

    uint src1Idx = y * srcImage1StrideInBytes + (x << 2);
    uint dstIdx  = y * dstImageStrideInBytes + x;

    d_uint8 src1 = *((d_uint8 *)(&pSrcImage1[src1Idx]));
    uint2 dst;

    dst.x = pack_((float4)(unpack0_(src1.data[0]), unpack0_(src1.data[1]), unpack0_(src1.data[2]), unpack0_(src1.data[3])));
    dst.y = pack_((float4)(unpack0_(src1.data[4]), unpack0_(src1.data[5]), unpack0_(src1.data[6]), unpack0_(src1.data[7])));

    *((uint2 *)(&pDstImage[dstIdx])) = dst;
}
int HipExec_ChannelExtract_U8_U32_Pos0(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage1, vx_uint32 srcImage1StrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ChannelExtract_U8_U32_Pos0, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage, dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage1, srcImage1StrideInBytes);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ChannelExtract_U8_U32_Pos1(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes,
    const uchar *pSrcImage1, uint srcImage1StrideInBytes) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if (x >= dstWidth || y >= dstHeight) {
        return;
    }

    uint src1Idx = y * srcImage1StrideInBytes + (x << 2);
    uint dstIdx  = y * dstImageStrideInBytes + x;

    d_uint8 src1 = *((d_uint8 *)(&pSrcImage1[src1Idx]));
    uint2 dst;

    dst.x = pack_((float4)(unpack1_(src1.data[0]), unpack1_(src1.data[1]), unpack1_(src1.data[2]), unpack1_(src1.data[3])));
    dst.y = pack_((float4)(unpack1_(src1.data[4]), unpack1_(src1.data[5]), unpack1_(src1.data[6]), unpack1_(src1.data[7])));

    *((uint2 *)(&pDstImage[dstIdx])) = dst;
}
int HipExec_ChannelExtract_U8_U32_Pos1(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage1, vx_uint32 srcImage1StrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ChannelExtract_U8_U32_Pos1, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage, dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage1, srcImage1StrideInBytes);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ChannelExtract_U8_U32_Pos2(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes,
    const uchar *pSrcImage1, uint srcImage1StrideInBytes) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if (x >= dstWidth || y >= dstHeight) {
        return;
    }

    uint src1Idx = y * srcImage1StrideInBytes + (x << 2);
    uint dstIdx  = y * dstImageStrideInBytes + x;

    d_uint8 src1 = *((d_uint8 *)(&pSrcImage1[src1Idx]));
    uint2 dst;

    dst.x = pack_((float4)(unpack2_(src1.data[0]), unpack2_(src1.data[1]), unpack2_(src1.data[2]), unpack2_(src1.data[3])));
    dst.y = pack_((float4)(unpack2_(src1.data[4]), unpack2_(src1.data[5]), unpack2_(src1.data[6]), unpack2_(src1.data[7])));

    *((uint2 *)(&pDstImage[dstIdx])) = dst;
}
int HipExec_ChannelExtract_U8_U32_Pos2(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage1, vx_uint32 srcImage1StrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ChannelExtract_U8_U32_Pos2, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage, dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage1, srcImage1StrideInBytes);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ChannelExtract_U8_U32_Pos3(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes,
    const uchar *pSrcImage1, uint srcImage1StrideInBytes) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if (x >= dstWidth || y >= dstHeight) {
        return;
    }

    uint src1Idx = y * srcImage1StrideInBytes + (x << 2);
    uint dstIdx  = y * dstImageStrideInBytes + x;

    d_uint8 src1 = *((d_uint8 *)(&pSrcImage1[src1Idx]));
    uint2 dst;

    dst.x = pack_((float4)(unpack3_(src1.data[0]), unpack3_(src1.data[1]), unpack3_(src1.data[2]), unpack3_(src1.data[3])));
    dst.y = pack_((float4)(unpack3_(src1.data[4]), unpack3_(src1.data[5]), unpack3_(src1.data[6]), unpack3_(src1.data[7])));

    *((uint2 *)(&pDstImage[dstIdx])) = dst;
}
int HipExec_ChannelExtract_U8_U32_Pos3(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage1, vx_uint32 srcImage1StrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ChannelExtract_U8_U32_Pos3, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage, dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage1, srcImage1StrideInBytes);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ChannelExtract_U8U8U8_U24(uint dstWidth, uint dstHeight,
    uchar *pDstImage1, uchar *pDstImage2, uchar *pDstImage3, uint dstImageStrideInBytes,
    const uchar *pSrcImage1, uint srcImage1StrideInBytes) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if (x >= dstWidth || y >= dstHeight) {
        return;
    }

    uint src1Idx = y * srcImage1StrideInBytes + (x * 3);
    uint dstIdx  = y * dstImageStrideInBytes + x;

    d_uint6 src1 = *((d_uint6 *)(&pSrcImage1[src1Idx]));
    uint2 dst1, dst2, dst3;

    dst1.x = pack_((float4)(unpack0_(src1.data[0]), unpack3_(src1.data[0]), unpack2_(src1.data[1]), unpack1_(src1.data[2])));
    dst1.y = pack_((float4)(unpack0_(src1.data[3]), unpack3_(src1.data[3]), unpack2_(src1.data[4]), unpack1_(src1.data[5])));
    dst2.x = pack_((float4)(unpack1_(src1.data[0]), unpack0_(src1.data[1]), unpack3_(src1.data[1]), unpack2_(src1.data[2])));
    dst2.y = pack_((float4)(unpack1_(src1.data[3]), unpack0_(src1.data[4]), unpack3_(src1.data[4]), unpack2_(src1.data[5])));
    dst3.x = pack_((float4)(unpack2_(src1.data[0]), unpack1_(src1.data[1]), unpack0_(src1.data[2]), unpack3_(src1.data[2])));
    dst3.y = pack_((float4)(unpack2_(src1.data[3]), unpack1_(src1.data[4]), unpack0_(src1.data[5]), unpack3_(src1.data[5])));

    *((uint2 *)(&pDstImage1[dstIdx])) = dst1;
    *((uint2 *)(&pDstImage2[dstIdx])) = dst2;
    *((uint2 *)(&pDstImage3[dstIdx])) = dst3;
}
int HipExec_ChannelExtract_U8U8U8_U24(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage1, vx_uint8 *pHipDstImage2, vx_uint8 *pHipDstImage3, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage1, vx_uint32 srcImage1StrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ChannelExtract_U8U8U8_U24, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage1, (uchar *)pHipDstImage2, (uchar *)pHipDstImage3, dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage1, srcImage1StrideInBytes);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ChannelExtract_U8U8U8_U32(uint dstWidth, uint dstHeight,
    uchar *pDstImage1, uchar *pDstImage2, uchar *pDstImage3, uint dstImageStrideInBytes,
    const uchar *pSrcImage1, uint srcImage1StrideInBytes) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if (x >= dstWidth || y >= dstHeight) {
        return;
    }

    uint src1Idx = y * srcImage1StrideInBytes + (x << 2);
    uint dstIdx  = y * dstImageStrideInBytes + x;

    d_uint8 src1 = *((d_uint8 *)(&pSrcImage1[src1Idx]));
    uint2 dst1, dst2, dst3;

    dst1.x = pack_((float4)(unpack0_(src1.data[0]), unpack0_(src1.data[1]), unpack0_(src1.data[2]), unpack0_(src1.data[3])));
    dst1.y = pack_((float4)(unpack0_(src1.data[4]), unpack0_(src1.data[5]), unpack0_(src1.data[6]), unpack0_(src1.data[7])));
    dst2.x = pack_((float4)(unpack1_(src1.data[0]), unpack1_(src1.data[1]), unpack1_(src1.data[2]), unpack1_(src1.data[3])));
    dst2.y = pack_((float4)(unpack1_(src1.data[4]), unpack1_(src1.data[5]), unpack1_(src1.data[6]), unpack1_(src1.data[7])));
    dst3.x = pack_((float4)(unpack2_(src1.data[0]), unpack2_(src1.data[1]), unpack2_(src1.data[2]), unpack2_(src1.data[3])));
    dst3.y = pack_((float4)(unpack2_(src1.data[4]), unpack2_(src1.data[5]), unpack2_(src1.data[6]), unpack2_(src1.data[7])));

    *((uint2 *)(&pDstImage1[dstIdx])) = dst1;
    *((uint2 *)(&pDstImage2[dstIdx])) = dst2;
    *((uint2 *)(&pDstImage3[dstIdx])) = dst3;
}
int HipExec_ChannelExtract_U8U8U8_U32(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage1, vx_uint8 *pHipDstImage2, vx_uint8 *pHipDstImage3, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage1, vx_uint32 srcImage1StrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ChannelExtract_U8U8U8_U32, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage1, (uchar *)pHipDstImage2, (uchar *)pHipDstImage3, dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage1, srcImage1StrideInBytes);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ChannelExtract_U8U8U8U8_U32(uint dstWidth, uint dstHeight,
    uchar *pDstImage1, uchar *pDstImage2, uchar *pDstImage3, uchar *pDstImage4, uint dstImageStrideInBytes,
    const uchar *pSrcImage1, uint srcImage1StrideInBytes) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if (x >= dstWidth || y >= dstHeight) {
        return;
    }

    uint src1Idx = y * srcImage1StrideInBytes + (x << 2);
    uint dstIdx  = y * dstImageStrideInBytes + x;

    d_uint8 src1 = *((d_uint8 *)(&pSrcImage1[src1Idx]));
    uint2 dst1, dst2, dst3, dst4;

    dst1.x = pack_((float4)(unpack0_(src1.data[0]), unpack0_(src1.data[1]), unpack0_(src1.data[2]), unpack0_(src1.data[3])));
    dst1.y = pack_((float4)(unpack0_(src1.data[4]), unpack0_(src1.data[5]), unpack0_(src1.data[6]), unpack0_(src1.data[7])));
    dst2.x = pack_((float4)(unpack1_(src1.data[0]), unpack1_(src1.data[1]), unpack1_(src1.data[2]), unpack1_(src1.data[3])));
    dst2.y = pack_((float4)(unpack1_(src1.data[4]), unpack1_(src1.data[5]), unpack1_(src1.data[6]), unpack1_(src1.data[7])));
    dst3.x = pack_((float4)(unpack2_(src1.data[0]), unpack2_(src1.data[1]), unpack2_(src1.data[2]), unpack2_(src1.data[3])));
    dst3.y = pack_((float4)(unpack2_(src1.data[4]), unpack2_(src1.data[5]), unpack2_(src1.data[6]), unpack2_(src1.data[7])));
    dst4.x = pack_((float4)(unpack3_(src1.data[0]), unpack3_(src1.data[1]), unpack3_(src1.data[2]), unpack3_(src1.data[3])));
    dst4.y = pack_((float4)(unpack3_(src1.data[4]), unpack3_(src1.data[5]), unpack3_(src1.data[6]), unpack3_(src1.data[7])));

    *((uint2 *)(&pDstImage1[dstIdx])) = dst1;
    *((uint2 *)(&pDstImage2[dstIdx])) = dst2;
    *((uint2 *)(&pDstImage3[dstIdx])) = dst3;
    *((uint2 *)(&pDstImage4[dstIdx])) = dst4;
}
int HipExec_ChannelExtract_U8U8U8U8_U32(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage1, vx_uint8 *pHipDstImage2, vx_uint8 *pHipDstImage3, vx_uint8 *pHipDstImage4, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage1, vx_uint32 srcImage1StrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ChannelExtract_U8U8U8U8_U32, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage1, (uchar *)pHipDstImage2, (uchar *)pHipDstImage3, (uchar *)pHipDstImage4, dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage1, srcImage1StrideInBytes);

    return VX_SUCCESS;
}

// ----------------------------------------------------------------------------
// VxChannelCombine kernels for hip backend
// ----------------------------------------------------------------------------

__global__ void __attribute__((visibility("default")))
Hip_ChannelCombine_U16_U8U8(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes,
    const uchar *pSrcImage1, uint srcImage1StrideInBytes,
    const uchar *pSrcImage2, uint srcImage2StrideInBytes) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if (x >= dstWidth || y >= dstHeight) {
        return;
    }

    uint src1Idx = y * srcImage1StrideInBytes + x;
    uint src2Idx = y * srcImage2StrideInBytes + x;
    uint dstIdx =  y * dstImageStrideInBytes + x + x;

    uint2 src1 = *((uint2 *)(&pSrcImage1[src1Idx]));
    uint2 src2 = *((uint2 *)(&pSrcImage2[src2Idx]));
    uint4 dst;

    dst.x = pack_((float4)(unpack0_(src1.x), unpack0_(src2.x), unpack1_(src1.x), unpack1_(src2.x)));
    dst.y = pack_((float4)(unpack2_(src1.x), unpack2_(src2.x), unpack3_(src1.x), unpack3_(src2.x)));
    dst.z = pack_((float4)(unpack0_(src1.y), unpack0_(src2.y), unpack1_(src1.y), unpack1_(src2.y)));
    dst.w = pack_((float4)(unpack2_(src1.y), unpack2_(src2.y), unpack3_(src1.y), unpack3_(src2.y)));

    *((uint4 *)(&pDstImage[dstIdx])) = dst;
}
int HipExec_ChannelCombine_U16_U8U8(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage1, vx_uint32 srcImage1StrideInBytes,
    const vx_uint8 *pHipSrcImage2, vx_uint32 srcImage2StrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ChannelCombine_U16_U8U8, dim3(ceil((float)globalThreads_x/localThreads_x), ceil((float)globalThreads_y/localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage , dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage1, srcImage1StrideInBytes, (const uchar *)pHipSrcImage2, srcImage2StrideInBytes);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ChannelCombine_U24_U8U8U8_RGB(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes,
    const uchar *pSrcImage1, uint srcImage1StrideInBytes,
    const uchar *pSrcImage2, uint srcImage2StrideInBytes,
    const uchar *pSrcImage3, uint srcImage3StrideInBytes) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if (x >= dstWidth || y >= dstHeight) {
        return;
    }

    uint src1Idx = y * srcImage1StrideInBytes + x;
    uint src2Idx = y * srcImage2StrideInBytes + x;
    uint src3Idx = y * srcImage3StrideInBytes + x;
    uint dstIdx =  y * dstImageStrideInBytes + (x * 3);

    uint2 src1 = *((uint2 *)(&pSrcImage1[src1Idx]));
    uint2 src2 = *((uint2 *)(&pSrcImage2[src2Idx]));
    uint2 src3 = *((uint2 *)(&pSrcImage3[src3Idx]));
    d_uint6 dst;

    dst.data[0] = pack_(make_float4(unpack0_(src1.x), unpack0_(src2.x), unpack0_(src3.x), unpack1_(src1.x)));
    dst.data[1] = pack_(make_float4(unpack1_(src2.x), unpack1_(src3.x), unpack2_(src1.x), unpack2_(src2.x)));
    dst.data[2] = pack_(make_float4(unpack2_(src3.x), unpack3_(src1.x), unpack3_(src2.x), unpack3_(src3.x)));
    dst.data[3] = pack_(make_float4(unpack0_(src1.y), unpack0_(src2.y), unpack0_(src3.y), unpack1_(src1.y)));
    dst.data[4] = pack_(make_float4(unpack1_(src2.y), unpack1_(src3.y), unpack2_(src1.y), unpack2_(src2.y)));
    dst.data[5] = pack_(make_float4(unpack2_(src3.y), unpack3_(src1.y), unpack3_(src2.y), unpack3_(src3.y)));

    *((d_uint6 *)(&pDstImage[dstIdx])) = dst;
}
int HipExec_ChannelCombine_U24_U8U8U8_RGB(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage1, vx_uint32 srcImage1StrideInBytes,
    const vx_uint8 *pHipSrcImage2, vx_uint32 srcImage2StrideInBytes,
    const vx_uint8 *pHipSrcImage3, vx_uint32 srcImage3StrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ChannelCombine_U24_U8U8U8_RGB, dim3(ceil((float)globalThreads_x/localThreads_x), ceil((float)globalThreads_y/localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage , dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage1, srcImage1StrideInBytes, (const uchar *)pHipSrcImage2, srcImage2StrideInBytes, (const uchar *)pHipSrcImage3, srcImage3StrideInBytes);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ChannelCombine_U32_U8U8U8_UYVY(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes,
    const uchar *pSrcImage1, uint srcImage1StrideInBytes,
    const uchar *pSrcImage2, uint srcImage2StrideInBytes,
    const uchar *pSrcImage3, uint srcImage3StrideInBytes,
    uint dstWidthComp) {

    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if ((x < dstWidthComp) && (y < dstHeight)) {
        uint src1Idx = y * srcImage1StrideInBytes + (x << 3);
        uint src2Idx = y * srcImage2StrideInBytes + (x << 2);
        uint src3Idx = y * srcImage3StrideInBytes + (x << 2);
        uint dstIdx =  y * dstImageStrideInBytes + (x << 4);

        uint2 src1 = *((uint2 *)(&pSrcImage1[src1Idx]));
        uint src2 = *((uint *)(&pSrcImage2[src2Idx]));
        uint src3 = *((uint *)(&pSrcImage3[src3Idx]));
        uint4 dst;

        dst.x = pack_(make_float4(unpack0_(src2), unpack0_(src1.x), unpack0_(src3), unpack1_(src1.x)));
        dst.y = pack_(make_float4(unpack1_(src2), unpack2_(src1.x), unpack1_(src3), unpack3_(src1.x)));
        dst.z = pack_(make_float4(unpack2_(src2), unpack0_(src1.y), unpack2_(src3), unpack1_(src1.y)));
        dst.w = pack_(make_float4(unpack3_(src2), unpack2_(src1.y), unpack3_(src3), unpack3_(src1.y)));

        *((uint4 *)(&pDstImage[dstIdx])) = dst;
    }
}
int HipExec_ChannelCombine_U32_U8U8U8_UYVY(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage1, vx_uint32 srcImage1StrideInBytes,
    const vx_uint8 *pHipSrcImage2, vx_uint32 srcImage2StrideInBytes,
    const vx_uint8 *pHipSrcImage3, vx_uint32 srcImage3StrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 4;
    int globalThreads_x = dstWidth;
    int globalThreads_y = dstHeight;

    vx_uint32 dstWidthComp = (dstWidth + 7) / 8;

    hipLaunchKernelGGL(Hip_ChannelCombine_U32_U8U8U8_UYVY, dim3(ceil((float)globalThreads_x/localThreads_x), ceil((float)globalThreads_y/localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage , dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage1, srcImage1StrideInBytes, (const uchar *)pHipSrcImage2, srcImage2StrideInBytes, (const uchar *)pHipSrcImage3, srcImage3StrideInBytes,
                        dstWidthComp);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ChannelCombine_U32_U8U8U8_YUYV(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes,
    const uchar *pSrcImage1, uint srcImage1StrideInBytes,
    const uchar *pSrcImage2, uint srcImage2StrideInBytes,
    const uchar *pSrcImage3, uint srcImage3StrideInBytes,
    uint dstWidthComp) {

    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if ((x < dstWidthComp) && (y < dstHeight)) {
        uint src1Idx = y * srcImage1StrideInBytes + (x << 3);
        uint src2Idx = y * srcImage2StrideInBytes + (x << 2);
        uint src3Idx = y * srcImage3StrideInBytes + (x << 2);
        uint dstIdx =  y * dstImageStrideInBytes + (x << 4);

        uint2 src1 = *((uint2 *)(&pSrcImage1[src1Idx]));
        uint src2 = *((uint *)(&pSrcImage2[src2Idx]));
        uint src3 = *((uint *)(&pSrcImage3[src3Idx]));
        uint4 dst;

        dst.x = pack_(make_float4(unpack0_(src1.x), unpack0_(src2), unpack1_(src1.x), unpack0_(src3)));
        dst.y = pack_(make_float4(unpack2_(src1.x), unpack1_(src2), unpack3_(src1.x), unpack1_(src3)));
        dst.z = pack_(make_float4(unpack0_(src1.y), unpack2_(src2), unpack1_(src1.y), unpack2_(src3)));
        dst.w = pack_(make_float4(unpack2_(src1.y), unpack3_(src2), unpack3_(src1.y), unpack3_(src3)));

        *((uint4 *)(&pDstImage[dstIdx])) = dst;
    }
}
int HipExec_ChannelCombine_U32_U8U8U8_YUYV(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage1, vx_uint32 srcImage1StrideInBytes,
    const vx_uint8 *pHipSrcImage2, vx_uint32 srcImage2StrideInBytes,
    const vx_uint8 *pHipSrcImage3, vx_uint32 srcImage3StrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 4;
    int globalThreads_x = dstWidth;
    int globalThreads_y = dstHeight;

    vx_uint32 dstWidthComp = (dstWidth + 7) / 8;

    hipLaunchKernelGGL(Hip_ChannelCombine_U32_U8U8U8_YUYV, dim3(ceil((float)globalThreads_x/localThreads_x), ceil((float)globalThreads_y/localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage , dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage1, srcImage1StrideInBytes, (const uchar *)pHipSrcImage2, srcImage2StrideInBytes, (const uchar *)pHipSrcImage3, srcImage3StrideInBytes,
                        dstWidthComp);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ChannelCombine_U32_U8U8U8U8_RGBX(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes,
    const uchar *pSrcImage1, uint srcImage1StrideInBytes,
    const uchar *pSrcImage2, uint srcImage2StrideInBytes,
    const uchar *pSrcImage3, uint srcImage3StrideInBytes,
    const uchar *pSrcImage4, uint srcImage4StrideInBytes) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if (x >= dstWidth || y >= dstHeight) {
        return;
    }

    uint src1Idx = y * srcImage1StrideInBytes + x;
    uint src2Idx = y * srcImage2StrideInBytes + x;
    uint src3Idx = y * srcImage3StrideInBytes + x;
    uint src4Idx = y * srcImage4StrideInBytes + x;
    uint dstIdx =  y * dstImageStrideInBytes + (x << 2);

    uint2 src1 = *((uint2 *)(&pSrcImage1[src1Idx]));
    uint2 src2 = *((uint2 *)(&pSrcImage2[src2Idx]));
    uint2 src3 = *((uint2 *)(&pSrcImage3[src3Idx]));
    uint2 src4 = *((uint2 *)(&pSrcImage4[src4Idx]));
    d_uint8 dst;

    dst.data[0] = pack_(make_float4(unpack0_(src1.x), unpack0_(src2.x), unpack0_(src3.x), unpack0_(src4.x)));
    dst.data[1] = pack_(make_float4(unpack1_(src1.x), unpack1_(src2.x), unpack1_(src3.x), unpack1_(src4.x)));
    dst.data[2] = pack_(make_float4(unpack2_(src1.x), unpack2_(src2.x), unpack2_(src3.x), unpack2_(src4.x)));
    dst.data[3] = pack_(make_float4(unpack3_(src1.x), unpack3_(src2.x), unpack3_(src3.x), unpack3_(src4.x)));
    dst.data[4] = pack_(make_float4(unpack0_(src1.y), unpack0_(src2.y), unpack0_(src3.y), unpack0_(src4.y)));
    dst.data[5] = pack_(make_float4(unpack1_(src1.y), unpack1_(src2.y), unpack1_(src3.y), unpack1_(src4.y)));
    dst.data[6] = pack_(make_float4(unpack2_(src1.y), unpack2_(src2.y), unpack2_(src3.y), unpack2_(src4.y)));
    dst.data[7] = pack_(make_float4(unpack3_(src1.y), unpack3_(src2.y), unpack3_(src3.y), unpack3_(src4.y)));

    *((d_uint8 *)(&pDstImage[dstIdx])) = dst;
}
int HipExec_ChannelCombine_U32_U8U8U8U8_RGBX(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage1, vx_uint32 srcImage1StrideInBytes,
    const vx_uint8 *pHipSrcImage2, vx_uint32 srcImage2StrideInBytes,
    const vx_uint8 *pHipSrcImage3, vx_uint32 srcImage3StrideInBytes,
    const vx_uint8 *pHipSrcImage4, vx_uint32 srcImage4StrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ChannelCombine_U32_U8U8U8U8_RGBX, dim3(ceil((float)globalThreads_x/localThreads_x), ceil((float)globalThreads_y/localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage , dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage1, srcImage1StrideInBytes, (const uchar *)pHipSrcImage2, srcImage2StrideInBytes, (const uchar *)pHipSrcImage3, srcImage3StrideInBytes, (const uchar *)pHipSrcImage4, srcImage4StrideInBytes);

    return VX_SUCCESS;
}

// ----------------------------------------------------------------------------
// VxColorConvert kernels for hip backend
// ----------------------------------------------------------------------------





// Group 1 - Destination RGB, Source Packed

__global__ void __attribute__((visibility("default")))
Hip_ColorConvert_RGB_RGBX(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes,
    const uchar *pSrcImage, uint srcImageStrideInBytes) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if (x >= dstWidth || y >= dstHeight) {
        return;
    }

    uint srcIdx = y * srcImageStrideInBytes + (x << 2);
    uint dstIdx  = y * dstImageStrideInBytes + (x * 3);

    d_uint8 src = *((d_uint8 *)(&pSrcImage[srcIdx]));
    d_uint6 dst = *((d_uint6 *)(&pDstImage[dstIdx]));

    dst.data[0] = pack_(make_float4(unpack0_(src.data[0]), unpack1_(src.data[0]), unpack2_(src.data[0]), unpack0_(src.data[1])));
    dst.data[1] = pack_(make_float4(unpack1_(src.data[1]), unpack2_(src.data[1]), unpack0_(src.data[2]), unpack1_(src.data[2])));
    dst.data[2] = pack_(make_float4(unpack2_(src.data[2]), unpack0_(src.data[3]), unpack1_(src.data[3]), unpack2_(src.data[3])));
    dst.data[3] = pack_(make_float4(unpack0_(src.data[4]), unpack1_(src.data[4]), unpack2_(src.data[4]), unpack0_(src.data[5])));
    dst.data[4] = pack_(make_float4(unpack1_(src.data[5]), unpack2_(src.data[5]), unpack0_(src.data[6]), unpack1_(src.data[6])));
    dst.data[5] = pack_(make_float4(unpack2_(src.data[6]), unpack0_(src.data[7]), unpack1_(src.data[7]), unpack2_(src.data[7])));

    *((d_uint6 *)(&pDstImage[dstIdx])) = dst;
}
int HipExec_ColorConvert_RGB_RGBX(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage1, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage1, vx_uint32 srcImage1StrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ColorConvert_RGB_RGBX, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage1, dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage1, srcImage1StrideInBytes);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ColorConvert_RGB_UYVY(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes, uint dstImageStrideInBytesComp,
    const uchar *pSrcImage, uint srcImageStrideInBytes, uint srcImageStrideInBytesComp,
    uint dstWidthComp, uint dstHeightComp) {

    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if ((x < dstWidthComp) && (y < dstHeightComp)) {
        uint L0Idx = y * srcImageStrideInBytesComp + (x << 4);
        uint L1Idx = L0Idx + srcImageStrideInBytes;
        uint4 L0 = *((uint4 *)(&pSrcImage[L0Idx]));
        uint4 L1 = *((uint4 *)(&pSrcImage[L1Idx]));

        uint RGB0Idx = y * dstImageStrideInBytesComp + (x * 24);
        uint RGB1Idx = RGB0Idx + dstImageStrideInBytes;

        float4 f;

        uint2 pY0, pY1;
        uint2 pU0, pU1;
        uint2 pV0, pV1;

        pY0.x = pack_(make_float4(unpack1_(L0.x), unpack3_(L0.x), unpack1_(L0.y), unpack3_(L0.y)));
        pY0.y = pack_(make_float4(unpack1_(L0.z), unpack3_(L0.z), unpack1_(L0.w), unpack3_(L0.w)));
        pY1.x = pack_(make_float4(unpack1_(L1.x), unpack3_(L1.x), unpack1_(L1.y), unpack3_(L1.y)));
        pY1.y = pack_(make_float4(unpack1_(L1.z), unpack3_(L1.z), unpack1_(L1.w), unpack3_(L1.w)));
        pU0.x = pack_(make_float4(unpack0_(L0.x), unpack0_(L0.x), unpack0_(L0.y), unpack0_(L0.y)));
        pU0.y = pack_(make_float4(unpack0_(L0.z), unpack0_(L0.z), unpack0_(L0.w), unpack0_(L0.w)));
        pU1.x = pack_(make_float4(unpack0_(L1.x), unpack0_(L1.x), unpack0_(L1.y), unpack0_(L1.y)));
        pU1.y = pack_(make_float4(unpack0_(L1.z), unpack0_(L1.z), unpack0_(L1.w), unpack0_(L1.w)));
        pV0.x = pack_(make_float4(unpack2_(L0.x), unpack2_(L0.x), unpack2_(L0.y), unpack2_(L0.y)));
        pV0.y = pack_(make_float4(unpack2_(L0.z), unpack2_(L0.z), unpack2_(L0.w), unpack2_(L0.w)));
        pV1.x = pack_(make_float4(unpack2_(L1.x), unpack2_(L1.x), unpack2_(L1.y), unpack2_(L1.y)));
        pV1.y = pack_(make_float4(unpack2_(L1.z), unpack2_(L1.z), unpack2_(L1.w), unpack2_(L1.w)));

        float2 cR = make_float2( 0.0000f,  1.5748f);
        float2 cG = make_float2(-0.1873f, -0.4681f);
        float2 cB = make_float2( 1.8556f,  0.0000f);
        float3 yuv;
        d_uint6 pRGB0, pRGB1;

        yuv.x = unpack0_(pY0.x); yuv.y = unpack0_(pU0.x); yuv.z = unpack0_(pV0.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack1_(pY0.x); yuv.y = unpack1_(pU0.x); yuv.z = unpack1_(pV0.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.w = fmaf(cR.y, yuv.z, yuv.x); pRGB0.data[0] = pack_(f); f.x = fmaf(cG.x, yuv.y, yuv.x); f.x = fmaf(cG.y, yuv.z, f.x); f.y = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack2_(pY0.x); yuv.y = unpack2_(pU0.x); yuv.z = unpack2_(pV0.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.z = fmaf(cR.y, yuv.z, yuv.x); f.w = fmaf(cG.x, yuv.y, yuv.x); f.w = fmaf(cG.y, yuv.z, f.w); pRGB0.data[1] = pack_(f); f.x = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack3_(pY0.x); yuv.y = unpack3_(pU0.x); yuv.z = unpack3_(pV0.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.y = fmaf(cR.y, yuv.z, yuv.x); f.z = fmaf(cG.x, yuv.y, yuv.x); f.z = fmaf(cG.y, yuv.z, f.z); f.w = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[2] = pack_(f);
        yuv.x = unpack0_(pY0.y); yuv.y = unpack0_(pU0.y); yuv.z = unpack0_(pV0.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack1_(pY0.y); yuv.y = unpack1_(pU0.y); yuv.z = unpack1_(pV0.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.w = fmaf(cR.y, yuv.z, yuv.x); pRGB0.data[3] = pack_(f); f.x = fmaf(cG.x, yuv.y, yuv.x); f.x = fmaf(cG.y, yuv.z, f.x); f.y = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack2_(pY0.y); yuv.y = unpack2_(pU0.y); yuv.z = unpack2_(pV0.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.z = fmaf(cR.y, yuv.z, yuv.x); f.w = fmaf(cG.x, yuv.y, yuv.x); f.w = fmaf(cG.y, yuv.z, f.w); pRGB0.data[4] = pack_(f); f.x = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack3_(pY0.y); yuv.y = unpack3_(pU0.y); yuv.z = unpack3_(pV0.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.y = fmaf(cR.y, yuv.z, yuv.x); f.z = fmaf(cG.x, yuv.y, yuv.x); f.z = fmaf(cG.y, yuv.z, f.z); f.w = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[5] = pack_(f);
        yuv.x = unpack0_(pY1.x); yuv.y = unpack0_(pU1.x); yuv.z = unpack0_(pV1.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack1_(pY1.x); yuv.y = unpack1_(pU1.x); yuv.z = unpack1_(pV1.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.w = fmaf(cR.y, yuv.z, yuv.x); pRGB1.data[0] = pack_(f); f.x = fmaf(cG.x, yuv.y, yuv.x); f.x = fmaf(cG.y, yuv.z, f.x); f.y = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack2_(pY1.x); yuv.y = unpack2_(pU1.x); yuv.z = unpack2_(pV1.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.z = fmaf(cR.y, yuv.z, yuv.x); f.w = fmaf(cG.x, yuv.y, yuv.x); f.w = fmaf(cG.y, yuv.z, f.w); pRGB1.data[1] = pack_(f); f.x = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack3_(pY1.x); yuv.y = unpack3_(pU1.x); yuv.z = unpack3_(pV1.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.y = fmaf(cR.y, yuv.z, yuv.x); f.z = fmaf(cG.x, yuv.y, yuv.x); f.z = fmaf(cG.y, yuv.z, f.z); f.w = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[2] = pack_(f);
        yuv.x = unpack0_(pY1.y); yuv.y = unpack0_(pU1.y); yuv.z = unpack0_(pV1.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack1_(pY1.y); yuv.y = unpack1_(pU1.y); yuv.z = unpack1_(pV1.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.w = fmaf(cR.y, yuv.z, yuv.x); pRGB1.data[3] = pack_(f); f.x = fmaf(cG.x, yuv.y, yuv.x); f.x = fmaf(cG.y, yuv.z, f.x); f.y = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack2_(pY1.y); yuv.y = unpack2_(pU1.y); yuv.z = unpack2_(pV1.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.z = fmaf(cR.y, yuv.z, yuv.x); f.w = fmaf(cG.x, yuv.y, yuv.x); f.w = fmaf(cG.y, yuv.z, f.w); pRGB1.data[4] = pack_(f); f.x = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack3_(pY1.y); yuv.y = unpack3_(pU1.y); yuv.z = unpack3_(pV1.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.y = fmaf(cR.y, yuv.z, yuv.x); f.z = fmaf(cG.x, yuv.y, yuv.x); f.z = fmaf(cG.y, yuv.z, f.z); f.w = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[5] = pack_(f);

        *((d_uint6 *)(&pDstImage[RGB0Idx])) = pRGB0;
        *((d_uint6 *)(&pDstImage[RGB1Idx])) = pRGB1;
    }
}
int HipExec_ColorConvert_RGB_UYVY(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage, vx_uint32 srcImageStrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 4;
    int globalThreads_x = dstWidth;
    int globalThreads_y = dstHeight;

    vx_uint32 dstWidthComp = (dstWidth + 7) / 8;
    vx_uint32 dstHeightComp = (dstHeight + 1) / 2;
    vx_uint32 dstImageStrideInBytesComp = dstImageStrideInBytes * 2;
    vx_uint32 srcImageStrideInBytesComp = srcImageStrideInBytes * 2;

    hipLaunchKernelGGL(Hip_ColorConvert_RGB_UYVY, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage, dstImageStrideInBytes, dstImageStrideInBytesComp,
                        (const uchar *)pHipSrcImage, srcImageStrideInBytes, srcImageStrideInBytesComp,
                        dstWidthComp, dstHeightComp);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ColorConvert_RGB_YUYV(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes, uint dstImageStrideInBytesComp,
    const uchar *pSrcImage, uint srcImageStrideInBytes, uint srcImageStrideInBytesComp,
    uint dstWidthComp, uint dstHeightComp) {

    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if ((x < dstWidthComp) && (y < dstHeightComp)) {
        uint L0Idx = y * srcImageStrideInBytesComp + (x << 4);
        uint L1Idx = L0Idx + srcImageStrideInBytes;
        uint4 L0 = *((uint4 *)(&pSrcImage[L0Idx]));
        uint4 L1 = *((uint4 *)(&pSrcImage[L1Idx]));

        uint RGB0Idx = y * dstImageStrideInBytesComp + (x * 24);
        uint RGB1Idx = RGB0Idx + dstImageStrideInBytes;

        float4 f;

        uint2 pY0, pY1;
        uint2 pU0, pU1;
        uint2 pV0, pV1;

        pY0.x = pack_(make_float4(unpack0_(L0.x), unpack2_(L0.x), unpack0_(L0.y), unpack2_(L0.y)));
        pY0.y = pack_(make_float4(unpack0_(L0.z), unpack2_(L0.z), unpack0_(L0.w), unpack2_(L0.w)));
        pY1.x = pack_(make_float4(unpack0_(L1.x), unpack2_(L1.x), unpack0_(L1.y), unpack2_(L1.y)));
        pY1.y = pack_(make_float4(unpack0_(L1.z), unpack2_(L1.z), unpack0_(L1.w), unpack2_(L1.w)));
        pU0.x = pack_(make_float4(unpack1_(L0.x), unpack1_(L0.x), unpack1_(L0.y), unpack1_(L0.y)));
        pU0.y = pack_(make_float4(unpack1_(L0.z), unpack1_(L0.z), unpack1_(L0.w), unpack1_(L0.w)));
        pU1.x = pack_(make_float4(unpack1_(L1.x), unpack1_(L1.x), unpack1_(L1.y), unpack1_(L1.y)));
        pU1.y = pack_(make_float4(unpack1_(L1.z), unpack1_(L1.z), unpack1_(L1.w), unpack1_(L1.w)));
        pV0.x = pack_(make_float4(unpack3_(L0.x), unpack3_(L0.x), unpack3_(L0.y), unpack3_(L0.y)));
        pV0.y = pack_(make_float4(unpack3_(L0.z), unpack3_(L0.z), unpack3_(L0.w), unpack3_(L0.w)));
        pV1.x = pack_(make_float4(unpack3_(L1.x), unpack3_(L1.x), unpack3_(L1.y), unpack3_(L1.y)));
        pV1.y = pack_(make_float4(unpack3_(L1.z), unpack3_(L1.z), unpack3_(L1.w), unpack3_(L1.w)));

        float2 cR = make_float2( 0.0000f,  1.5748f);
        float2 cG = make_float2(-0.1873f, -0.4681f);
        float2 cB = make_float2( 1.8556f,  0.0000f);
        float3 yuv;
        d_uint6 pRGB0, pRGB1;

        yuv.x = unpack0_(pY0.x); yuv.y = unpack0_(pU0.x); yuv.z = unpack0_(pV0.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack1_(pY0.x); yuv.y = unpack1_(pU0.x); yuv.z = unpack1_(pV0.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.w = fmaf(cR.y, yuv.z, yuv.x); pRGB0.data[0] = pack_(f); f.x = fmaf(cG.x, yuv.y, yuv.x); f.x = fmaf(cG.y, yuv.z, f.x); f.y = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack2_(pY0.x); yuv.y = unpack2_(pU0.x); yuv.z = unpack2_(pV0.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.z = fmaf(cR.y, yuv.z, yuv.x); f.w = fmaf(cG.x, yuv.y, yuv.x); f.w = fmaf(cG.y, yuv.z, f.w); pRGB0.data[1] = pack_(f); f.x = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack3_(pY0.x); yuv.y = unpack3_(pU0.x); yuv.z = unpack3_(pV0.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.y = fmaf(cR.y, yuv.z, yuv.x); f.z = fmaf(cG.x, yuv.y, yuv.x); f.z = fmaf(cG.y, yuv.z, f.z); f.w = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[2] = pack_(f);
        yuv.x = unpack0_(pY0.y); yuv.y = unpack0_(pU0.y); yuv.z = unpack0_(pV0.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack1_(pY0.y); yuv.y = unpack1_(pU0.y); yuv.z = unpack1_(pV0.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.w = fmaf(cR.y, yuv.z, yuv.x); pRGB0.data[3] = pack_(f); f.x = fmaf(cG.x, yuv.y, yuv.x); f.x = fmaf(cG.y, yuv.z, f.x); f.y = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack2_(pY0.y); yuv.y = unpack2_(pU0.y); yuv.z = unpack2_(pV0.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.z = fmaf(cR.y, yuv.z, yuv.x); f.w = fmaf(cG.x, yuv.y, yuv.x); f.w = fmaf(cG.y, yuv.z, f.w); pRGB0.data[4] = pack_(f); f.x = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack3_(pY0.y); yuv.y = unpack3_(pU0.y); yuv.z = unpack3_(pV0.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.y = fmaf(cR.y, yuv.z, yuv.x); f.z = fmaf(cG.x, yuv.y, yuv.x); f.z = fmaf(cG.y, yuv.z, f.z); f.w = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[5] = pack_(f);
        yuv.x = unpack0_(pY1.x); yuv.y = unpack0_(pU1.x); yuv.z = unpack0_(pV1.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack1_(pY1.x); yuv.y = unpack1_(pU1.x); yuv.z = unpack1_(pV1.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.w = fmaf(cR.y, yuv.z, yuv.x); pRGB1.data[0] = pack_(f); f.x = fmaf(cG.x, yuv.y, yuv.x); f.x = fmaf(cG.y, yuv.z, f.x); f.y = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack2_(pY1.x); yuv.y = unpack2_(pU1.x); yuv.z = unpack2_(pV1.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.z = fmaf(cR.y, yuv.z, yuv.x); f.w = fmaf(cG.x, yuv.y, yuv.x); f.w = fmaf(cG.y, yuv.z, f.w); pRGB1.data[1] = pack_(f); f.x = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack3_(pY1.x); yuv.y = unpack3_(pU1.x); yuv.z = unpack3_(pV1.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.y = fmaf(cR.y, yuv.z, yuv.x); f.z = fmaf(cG.x, yuv.y, yuv.x); f.z = fmaf(cG.y, yuv.z, f.z); f.w = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[2] = pack_(f);
        yuv.x = unpack0_(pY1.y); yuv.y = unpack0_(pU1.y); yuv.z = unpack0_(pV1.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack1_(pY1.y); yuv.y = unpack1_(pU1.y); yuv.z = unpack1_(pV1.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.w = fmaf(cR.y, yuv.z, yuv.x); pRGB1.data[3] = pack_(f); f.x = fmaf(cG.x, yuv.y, yuv.x); f.x = fmaf(cG.y, yuv.z, f.x); f.y = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack2_(pY1.y); yuv.y = unpack2_(pU1.y); yuv.z = unpack2_(pV1.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.z = fmaf(cR.y, yuv.z, yuv.x); f.w = fmaf(cG.x, yuv.y, yuv.x); f.w = fmaf(cG.y, yuv.z, f.w); pRGB1.data[4] = pack_(f); f.x = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack3_(pY1.y); yuv.y = unpack3_(pU1.y); yuv.z = unpack3_(pV1.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.y = fmaf(cR.y, yuv.z, yuv.x); f.z = fmaf(cG.x, yuv.y, yuv.x); f.z = fmaf(cG.y, yuv.z, f.z); f.w = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[5] = pack_(f);

        *((d_uint6 *)(&pDstImage[RGB0Idx])) = pRGB0;
        *((d_uint6 *)(&pDstImage[RGB1Idx])) = pRGB1;

    }
}
int HipExec_ColorConvert_RGB_YUYV(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage, vx_uint32 srcImageStrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 4;
    int globalThreads_x = dstWidth;
    int globalThreads_y = dstHeight;

    vx_uint32 dstWidthComp = (dstWidth + 7) / 8;
    vx_uint32 dstHeightComp = (dstHeight + 1) / 2;
    vx_uint32 dstImageStrideInBytesComp = dstImageStrideInBytes * 2;
    vx_uint32 srcImageStrideInBytesComp = srcImageStrideInBytes * 2;

    hipLaunchKernelGGL(Hip_ColorConvert_RGB_YUYV, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage, dstImageStrideInBytes, dstImageStrideInBytesComp,
                        (const uchar *)pHipSrcImage, srcImageStrideInBytes, srcImageStrideInBytesComp,
                        dstWidthComp, dstHeightComp);

    return VX_SUCCESS;
}





// Group 2 - Destination RGBX, Source Packed

__global__ void __attribute__((visibility("default")))
Hip_ColorConvert_RGBX_RGB(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes,
    const uchar *pSrcImage, uint srcImageStrideInBytes) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if (x >= dstWidth || y >= dstHeight) {
        return;
    }

    uint srcIdx = y * srcImageStrideInBytes + (x * 3);
    uint dstIdx  = y * dstImageStrideInBytes + (x << 2);

    d_uint6 src = *((d_uint6 *)(&pSrcImage[srcIdx]));
    d_uint8 dst = *((d_uint8 *)(&pDstImage[dstIdx]));

    dst.data[0] = pack_(make_float4(unpack0_(src.data[0]), unpack1_(src.data[0]), unpack2_(src.data[0]), 255.0f));
    dst.data[1] = pack_(make_float4(unpack3_(src.data[0]), unpack0_(src.data[1]), unpack1_(src.data[1]), 255.0f));
    dst.data[2] = pack_(make_float4(unpack2_(src.data[1]), unpack3_(src.data[1]), unpack0_(src.data[2]), 255.0f));
    dst.data[3] = pack_(make_float4(unpack1_(src.data[2]), unpack2_(src.data[2]), unpack3_(src.data[2]), 255.0f));
    dst.data[4] = pack_(make_float4(unpack0_(src.data[3]), unpack1_(src.data[3]), unpack2_(src.data[3]), 255.0f));
    dst.data[5] = pack_(make_float4(unpack3_(src.data[3]), unpack0_(src.data[4]), unpack1_(src.data[4]), 255.0f));
    dst.data[6] = pack_(make_float4(unpack2_(src.data[4]), unpack3_(src.data[4]), unpack0_(src.data[5]), 255.0f));
    dst.data[7] = pack_(make_float4(unpack1_(src.data[5]), unpack2_(src.data[5]), unpack3_(src.data[5]), 255.0f));

    *((d_uint8 *)(&pDstImage[dstIdx])) = dst;
}
int HipExec_ColorConvert_RGBX_RGB(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage1, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage1, vx_uint32 srcImage1StrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ColorConvert_RGBX_RGB, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage1, dstImageStrideInBytes,
                        (const uchar *)pHipSrcImage1, srcImage1StrideInBytes);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ColorConvert_RGBX_UYVY(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes, uint dstImageStrideInBytesComp,
    const uchar *pSrcImage, uint srcImageStrideInBytes, uint srcImageStrideInBytesComp,
    uint dstWidthComp, uint dstHeightComp) {

    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if ((x < dstWidthComp) && (y < dstHeightComp)) {
        uint L0Idx = y * srcImageStrideInBytesComp + (x << 4);
        uint L1Idx = L0Idx + srcImageStrideInBytes;
        uint4 L0 = *((uint4 *)(&pSrcImage[L0Idx]));
        uint4 L1 = *((uint4 *)(&pSrcImage[L1Idx]));

        uint RGB0Idx = y * dstImageStrideInBytesComp + (x << 5);
        uint RGB1Idx = RGB0Idx + dstImageStrideInBytes;

        float4 f;

        uint2 pY0, pY1;
        uint2 pU0, pU1;
        uint2 pV0, pV1;

        pY0.x = pack_(make_float4(unpack1_(L0.x), unpack3_(L0.x), unpack1_(L0.y), unpack3_(L0.y)));
        pY0.y = pack_(make_float4(unpack1_(L0.z), unpack3_(L0.z), unpack1_(L0.w), unpack3_(L0.w)));
        pY1.x = pack_(make_float4(unpack1_(L1.x), unpack3_(L1.x), unpack1_(L1.y), unpack3_(L1.y)));
        pY1.y = pack_(make_float4(unpack1_(L1.z), unpack3_(L1.z), unpack1_(L1.w), unpack3_(L1.w)));
        pU0.x = pack_(make_float4(unpack0_(L0.x), unpack0_(L0.x), unpack0_(L0.y), unpack0_(L0.y)));
        pU0.y = pack_(make_float4(unpack0_(L0.z), unpack0_(L0.z), unpack0_(L0.w), unpack0_(L0.w)));
        pU1.x = pack_(make_float4(unpack0_(L1.x), unpack0_(L1.x), unpack0_(L1.y), unpack0_(L1.y)));
        pU1.y = pack_(make_float4(unpack0_(L1.z), unpack0_(L1.z), unpack0_(L1.w), unpack0_(L1.w)));
        pV0.x = pack_(make_float4(unpack2_(L0.x), unpack2_(L0.x), unpack2_(L0.y), unpack2_(L0.y)));
        pV0.y = pack_(make_float4(unpack2_(L0.z), unpack2_(L0.z), unpack2_(L0.w), unpack2_(L0.w)));
        pV1.x = pack_(make_float4(unpack2_(L1.x), unpack2_(L1.x), unpack2_(L1.y), unpack2_(L1.y)));
        pV1.y = pack_(make_float4(unpack2_(L1.z), unpack2_(L1.z), unpack2_(L1.w), unpack2_(L1.w)));

        float2 cR = make_float2( 0.0000f,  1.5748f);
        float2 cG = make_float2(-0.1873f, -0.4681f);
        float2 cB = make_float2( 1.8556f,  0.0000f);
        float3 yuv;
        f.w = 255.0f;
        d_uint8 pRGB0, pRGB1;

        yuv.x = unpack0_(pY0.x); yuv.y = unpack0_(pU0.x); yuv.z = unpack0_(pV0.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[0] = pack_(f);
        yuv.x = unpack1_(pY0.x); yuv.y = unpack1_(pU0.x); yuv.z = unpack1_(pV0.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[1] = pack_(f);
        yuv.x = unpack2_(pY0.x); yuv.y = unpack2_(pU0.x); yuv.z = unpack2_(pV0.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[2] = pack_(f);
        yuv.x = unpack3_(pY0.x); yuv.y = unpack3_(pU0.x); yuv.z = unpack3_(pV0.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[3] = pack_(f);
        yuv.x = unpack0_(pY0.y); yuv.y = unpack0_(pU0.y); yuv.z = unpack0_(pV0.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[4] = pack_(f);
        yuv.x = unpack1_(pY0.y); yuv.y = unpack1_(pU0.y); yuv.z = unpack1_(pV0.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[5] = pack_(f);
        yuv.x = unpack2_(pY0.y); yuv.y = unpack2_(pU0.y); yuv.z = unpack2_(pV0.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[6] = pack_(f);
        yuv.x = unpack3_(pY0.y); yuv.y = unpack3_(pU0.y); yuv.z = unpack3_(pV0.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[7] = pack_(f);
        yuv.x = unpack0_(pY1.x); yuv.y = unpack0_(pU1.x); yuv.z = unpack0_(pV1.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[0] = pack_(f);
        yuv.x = unpack1_(pY1.x); yuv.y = unpack1_(pU1.x); yuv.z = unpack1_(pV1.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[1] = pack_(f);
        yuv.x = unpack2_(pY1.x); yuv.y = unpack2_(pU1.x); yuv.z = unpack2_(pV1.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[2] = pack_(f);
        yuv.x = unpack3_(pY1.x); yuv.y = unpack3_(pU1.x); yuv.z = unpack3_(pV1.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[3] = pack_(f);
        yuv.x = unpack0_(pY1.y); yuv.y = unpack0_(pU1.y); yuv.z = unpack0_(pV1.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[4] = pack_(f);
        yuv.x = unpack1_(pY1.y); yuv.y = unpack1_(pU1.y); yuv.z = unpack1_(pV1.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[5] = pack_(f);
        yuv.x = unpack2_(pY1.y); yuv.y = unpack2_(pU1.y); yuv.z = unpack2_(pV1.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[6] = pack_(f);
        yuv.x = unpack3_(pY1.y); yuv.y = unpack3_(pU1.y); yuv.z = unpack3_(pV1.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[7] = pack_(f);

        *((d_uint8 *)(&pDstImage[RGB0Idx])) = pRGB0;
        *((d_uint8 *)(&pDstImage[RGB1Idx])) = pRGB1;
    }
}
int HipExec_ColorConvert_RGBX_UYVY(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage, vx_uint32 srcImageStrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 4;
    int globalThreads_x = dstWidth;
    int globalThreads_y = dstHeight;

    vx_uint32 dstWidthComp = (dstWidth + 7) / 8;
    vx_uint32 dstHeightComp = (dstHeight + 1) / 2;
    vx_uint32 dstImageStrideInBytesComp = dstImageStrideInBytes * 2;
    vx_uint32 srcImageStrideInBytesComp = srcImageStrideInBytes * 2;

    hipLaunchKernelGGL(Hip_ColorConvert_RGBX_UYVY, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage, dstImageStrideInBytes, dstImageStrideInBytesComp,
                        (const uchar *)pHipSrcImage, srcImageStrideInBytes, srcImageStrideInBytesComp,
                        dstWidthComp, dstHeightComp);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ColorConvert_RGBX_YUYV(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes, uint dstImageStrideInBytesComp,
    const uchar *pSrcImage, uint srcImageStrideInBytes, uint srcImageStrideInBytesComp,
    uint dstWidthComp, uint dstHeightComp) {

    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if ((x < dstWidthComp) && (y < dstHeightComp)) {
        uint L0Idx = y * srcImageStrideInBytesComp + (x << 4);
        uint L1Idx = L0Idx + srcImageStrideInBytes;
        uint4 L0 = *((uint4 *)(&pSrcImage[L0Idx]));
        uint4 L1 = *((uint4 *)(&pSrcImage[L1Idx]));

        uint RGB0Idx = y * dstImageStrideInBytesComp + (x << 5);
        uint RGB1Idx = RGB0Idx + dstImageStrideInBytes;

        float4 f;

        uint2 pY0, pY1;
        uint2 pU0, pU1;
        uint2 pV0, pV1;

        pY0.x = pack_(make_float4(unpack0_(L0.x), unpack2_(L0.x), unpack0_(L0.y), unpack2_(L0.y)));
        pY0.y = pack_(make_float4(unpack0_(L0.z), unpack2_(L0.z), unpack0_(L0.w), unpack2_(L0.w)));
        pY1.x = pack_(make_float4(unpack0_(L1.x), unpack2_(L1.x), unpack0_(L1.y), unpack2_(L1.y)));
        pY1.y = pack_(make_float4(unpack0_(L1.z), unpack2_(L1.z), unpack0_(L1.w), unpack2_(L1.w)));
        pU0.x = pack_(make_float4(unpack1_(L0.x), unpack1_(L0.x), unpack1_(L0.y), unpack1_(L0.y)));
        pU0.y = pack_(make_float4(unpack1_(L0.z), unpack1_(L0.z), unpack1_(L0.w), unpack1_(L0.w)));
        pU1.x = pack_(make_float4(unpack1_(L1.x), unpack1_(L1.x), unpack1_(L1.y), unpack1_(L1.y)));
        pU1.y = pack_(make_float4(unpack1_(L1.z), unpack1_(L1.z), unpack1_(L1.w), unpack1_(L1.w)));
        pV0.x = pack_(make_float4(unpack3_(L0.x), unpack3_(L0.x), unpack3_(L0.y), unpack3_(L0.y)));
        pV0.y = pack_(make_float4(unpack3_(L0.z), unpack3_(L0.z), unpack3_(L0.w), unpack3_(L0.w)));
        pV1.x = pack_(make_float4(unpack3_(L1.x), unpack3_(L1.x), unpack3_(L1.y), unpack3_(L1.y)));
        pV1.y = pack_(make_float4(unpack3_(L1.z), unpack3_(L1.z), unpack3_(L1.w), unpack3_(L1.w)));

        float2 cR = make_float2( 0.0000f,  1.5748f);
        float2 cG = make_float2(-0.1873f, -0.4681f);
        float2 cB = make_float2( 1.8556f,  0.0000f);
        float3 yuv;
        f.w = 255.0f;
        d_uint8 pRGB0, pRGB1;

        yuv.x = unpack0_(pY0.x); yuv.y = unpack0_(pU0.x); yuv.z = unpack0_(pV0.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[0] = pack_(f);
        yuv.x = unpack1_(pY0.x); yuv.y = unpack1_(pU0.x); yuv.z = unpack1_(pV0.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[1] = pack_(f);
        yuv.x = unpack2_(pY0.x); yuv.y = unpack2_(pU0.x); yuv.z = unpack2_(pV0.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[2] = pack_(f);
        yuv.x = unpack3_(pY0.x); yuv.y = unpack3_(pU0.x); yuv.z = unpack3_(pV0.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[3] = pack_(f);
        yuv.x = unpack0_(pY0.y); yuv.y = unpack0_(pU0.y); yuv.z = unpack0_(pV0.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[4] = pack_(f);
        yuv.x = unpack1_(pY0.y); yuv.y = unpack1_(pU0.y); yuv.z = unpack1_(pV0.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[5] = pack_(f);
        yuv.x = unpack2_(pY0.y); yuv.y = unpack2_(pU0.y); yuv.z = unpack2_(pV0.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[6] = pack_(f);
        yuv.x = unpack3_(pY0.y); yuv.y = unpack3_(pU0.y); yuv.z = unpack3_(pV0.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[7] = pack_(f);
        yuv.x = unpack0_(pY1.x); yuv.y = unpack0_(pU1.x); yuv.z = unpack0_(pV1.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[0] = pack_(f);
        yuv.x = unpack1_(pY1.x); yuv.y = unpack1_(pU1.x); yuv.z = unpack1_(pV1.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[1] = pack_(f);
        yuv.x = unpack2_(pY1.x); yuv.y = unpack2_(pU1.x); yuv.z = unpack2_(pV1.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[2] = pack_(f);
        yuv.x = unpack3_(pY1.x); yuv.y = unpack3_(pU1.x); yuv.z = unpack3_(pV1.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[3] = pack_(f);
        yuv.x = unpack0_(pY1.y); yuv.y = unpack0_(pU1.y); yuv.z = unpack0_(pV1.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[4] = pack_(f);
        yuv.x = unpack1_(pY1.y); yuv.y = unpack1_(pU1.y); yuv.z = unpack1_(pV1.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[5] = pack_(f);
        yuv.x = unpack2_(pY1.y); yuv.y = unpack2_(pU1.y); yuv.z = unpack2_(pV1.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[6] = pack_(f);
        yuv.x = unpack3_(pY1.y); yuv.y = unpack3_(pU1.y); yuv.z = unpack3_(pV1.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[7] = pack_(f);

        *((d_uint8 *)(&pDstImage[RGB0Idx])) = pRGB0;
        *((d_uint8 *)(&pDstImage[RGB1Idx])) = pRGB1;

    }
}
int HipExec_ColorConvert_RGBX_YUYV(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcImage, vx_uint32 srcImageStrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 4;
    int globalThreads_x = dstWidth;
    int globalThreads_y = dstHeight;

    vx_uint32 dstWidthComp = (dstWidth + 7) / 8;
    vx_uint32 dstHeightComp = (dstHeight + 1) / 2;
    vx_uint32 dstImageStrideInBytesComp = dstImageStrideInBytes * 2;
    vx_uint32 srcImageStrideInBytesComp = srcImageStrideInBytes * 2;

    hipLaunchKernelGGL(Hip_ColorConvert_RGBX_YUYV, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage, dstImageStrideInBytes, dstImageStrideInBytesComp,
                        (const uchar *)pHipSrcImage, srcImageStrideInBytes, srcImageStrideInBytesComp,
                        dstWidthComp, dstHeightComp);

    return VX_SUCCESS;
}





// Group 3 - Destination RGB, Source Planar

__global__ void __attribute__((visibility("default")))
Hip_ColorConvert_RGB_IYUV(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes, uint dstImageStrideInBytesComp,
    const uchar *pSrcYImage, uint srcYImageStrideInBytes, const uchar *pSrcUImage, uint srcUImageStrideInBytes, const uchar *pSrcVImage, uint srcVImageStrideInBytes,
    uint dstWidthComp, uint dstHeightComp, uint srcYImageStrideInBytesComp) {

    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if ((x < dstWidthComp) && (y < dstHeightComp)) {
        uint srcY0Idx = y * srcYImageStrideInBytesComp + (x << 3);
        uint srcY1Idx = srcY0Idx + srcYImageStrideInBytes;
        uint srcUIdx = y * srcUImageStrideInBytes + (x << 2);
        uint srcVIdx = y * srcVImageStrideInBytes + (x << 2);
        uint2 pY0 = *((uint2 *)(&pSrcYImage[srcY0Idx]));
        uint2 pY1 = *((uint2 *)(&pSrcYImage[srcY1Idx]));
        uint2 pUV;
        pUV.x = *((uint *)(&pSrcUImage[srcUIdx]));
        pUV.y = *((uint *)(&pSrcVImage[srcVIdx]));

        uint RGB0Idx = y * dstImageStrideInBytesComp + (x * 24);
        uint RGB1Idx = RGB0Idx + dstImageStrideInBytes;

        float4 f;

        uint2 pU0, pU1;
        uint2 pV0, pV1;
        f.x = unpack0_(pUV.x); f.y = f.x;
        f.z = unpack1_(pUV.x); f.w = f.z;
        pU0.x = pack_(f);
        f.x = unpack2_(pUV.x); f.y = f.x;
        f.z = unpack3_(pUV.x); f.w = f.z;
        pU0.y = pack_(f);
        pU1.x = pU0.x;
        pU1.y = pU0.y;
        f.x = unpack0_(pUV.y); f.y = f.x;
        f.z = unpack1_(pUV.y); f.w = f.z;
        pV0.x = pack_(f);
        f.x = unpack2_(pUV.y); f.y = f.x;
        f.z = unpack3_(pUV.y); f.w = f.z;
        pV0.y = pack_(f);
        pV1.x = pV0.x;
        pV1.y = pV0.y;

        float2 cR = make_float2( 0.0000f,  1.5748f);
        float2 cG = make_float2(-0.1873f, -0.4681f);
        float2 cB = make_float2( 1.8556f,  0.0000f);
        float3 yuv;
        d_uint6 pRGB0, pRGB1;

        yuv.x = unpack0_(pY0.x); yuv.y = unpack0_(pU0.x); yuv.z = unpack0_(pV0.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack1_(pY0.x); yuv.y = unpack1_(pU0.x); yuv.z = unpack1_(pV0.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.w = fmaf(cR.y, yuv.z, yuv.x); pRGB0.data[0] = pack_(f); f.x = fmaf(cG.x, yuv.y, yuv.x); f.x = fmaf(cG.y, yuv.z, f.x); f.y = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack2_(pY0.x); yuv.y = unpack2_(pU0.x); yuv.z = unpack2_(pV0.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.z = fmaf(cR.y, yuv.z, yuv.x); f.w = fmaf(cG.x, yuv.y, yuv.x); f.w = fmaf(cG.y, yuv.z, f.w); pRGB0.data[1] = pack_(f); f.x = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack3_(pY0.x); yuv.y = unpack3_(pU0.x); yuv.z = unpack3_(pV0.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.y = fmaf(cR.y, yuv.z, yuv.x); f.z = fmaf(cG.x, yuv.y, yuv.x); f.z = fmaf(cG.y, yuv.z, f.z); f.w = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[2] = pack_(f);
        yuv.x = unpack0_(pY0.y); yuv.y = unpack0_(pU0.y); yuv.z = unpack0_(pV0.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack1_(pY0.y); yuv.y = unpack1_(pU0.y); yuv.z = unpack1_(pV0.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.w = fmaf(cR.y, yuv.z, yuv.x); pRGB0.data[3] = pack_(f); f.x = fmaf(cG.x, yuv.y, yuv.x); f.x = fmaf(cG.y, yuv.z, f.x); f.y = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack2_(pY0.y); yuv.y = unpack2_(pU0.y); yuv.z = unpack2_(pV0.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.z = fmaf(cR.y, yuv.z, yuv.x); f.w = fmaf(cG.x, yuv.y, yuv.x); f.w = fmaf(cG.y, yuv.z, f.w); pRGB0.data[4] = pack_(f); f.x = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack3_(pY0.y); yuv.y = unpack3_(pU0.y); yuv.z = unpack3_(pV0.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.y = fmaf(cR.y, yuv.z, yuv.x); f.z = fmaf(cG.x, yuv.y, yuv.x); f.z = fmaf(cG.y, yuv.z, f.z); f.w = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[5] = pack_(f);
        yuv.x = unpack0_(pY1.x); yuv.y = unpack0_(pU1.x); yuv.z = unpack0_(pV1.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack1_(pY1.x); yuv.y = unpack1_(pU1.x); yuv.z = unpack1_(pV1.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.w = fmaf(cR.y, yuv.z, yuv.x); pRGB1.data[0] = pack_(f); f.x = fmaf(cG.x, yuv.y, yuv.x); f.x = fmaf(cG.y, yuv.z, f.x); f.y = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack2_(pY1.x); yuv.y = unpack2_(pU1.x); yuv.z = unpack2_(pV1.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.z = fmaf(cR.y, yuv.z, yuv.x); f.w = fmaf(cG.x, yuv.y, yuv.x); f.w = fmaf(cG.y, yuv.z, f.w); pRGB1.data[1] = pack_(f); f.x = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack3_(pY1.x); yuv.y = unpack3_(pU1.x); yuv.z = unpack3_(pV1.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.y = fmaf(cR.y, yuv.z, yuv.x); f.z = fmaf(cG.x, yuv.y, yuv.x); f.z = fmaf(cG.y, yuv.z, f.z); f.w = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[2] = pack_(f);
        yuv.x = unpack0_(pY1.y); yuv.y = unpack0_(pU1.y); yuv.z = unpack0_(pV1.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack1_(pY1.y); yuv.y = unpack1_(pU1.y); yuv.z = unpack1_(pV1.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.w = fmaf(cR.y, yuv.z, yuv.x); pRGB1.data[3] = pack_(f); f.x = fmaf(cG.x, yuv.y, yuv.x); f.x = fmaf(cG.y, yuv.z, f.x); f.y = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack2_(pY1.y); yuv.y = unpack2_(pU1.y); yuv.z = unpack2_(pV1.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.z = fmaf(cR.y, yuv.z, yuv.x); f.w = fmaf(cG.x, yuv.y, yuv.x); f.w = fmaf(cG.y, yuv.z, f.w); pRGB1.data[4] = pack_(f); f.x = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack3_(pY1.y); yuv.y = unpack3_(pU1.y); yuv.z = unpack3_(pV1.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.y = fmaf(cR.y, yuv.z, yuv.x); f.z = fmaf(cG.x, yuv.y, yuv.x); f.z = fmaf(cG.y, yuv.z, f.z); f.w = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[5] = pack_(f);

        *((d_uint6 *)(&pDstImage[RGB0Idx])) = pRGB0;
        *((d_uint6 *)(&pDstImage[RGB1Idx])) = pRGB1;

    }
}
int HipExec_ColorConvert_RGB_IYUV(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcYImage, vx_uint32 srcYImageStrideInBytes,
    const vx_uint8 *pHipSrcUImage, vx_uint32 srcUImageStrideInBytes,
    const vx_uint8 *pHipSrcVImage, vx_uint32 srcVImageStrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 4;
    int globalThreads_x = dstWidth;
    int globalThreads_y = dstHeight;

    vx_uint32 dstWidthComp = (dstWidth + 7) / 8;
    vx_uint32 dstHeightComp = (dstHeight + 1) / 2;
    vx_uint32 dstImageStrideInBytesComp = dstImageStrideInBytes * 2;
    vx_uint32 srcYImageStrideInBytesComp = srcYImageStrideInBytes * 2;

    hipLaunchKernelGGL(Hip_ColorConvert_RGB_IYUV, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage, dstImageStrideInBytes, dstImageStrideInBytesComp,
                        (const uchar *)pHipSrcYImage, srcYImageStrideInBytes, (const uchar *)pHipSrcUImage, srcUImageStrideInBytes, (const uchar *)pHipSrcVImage, srcVImageStrideInBytes,
                        dstWidthComp, dstHeightComp, srcYImageStrideInBytesComp);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ColorConvert_RGB_NV12(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes, uint dstImageStrideInBytesComp,
    const uchar *pSrcLumaImage, uint srcLumaImageStrideInBytes, const uchar *pSrcChromaImage, uint srcChromaImageStrideInBytes,
    uint dstWidthComp, uint dstHeightComp, uint srcLumaImageStrideInBytesComp) {

    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if ((x < dstWidthComp) && (y < dstHeightComp)) {
        uint srcY0Idx = y * srcLumaImageStrideInBytesComp + (x << 3);
        uint srcY1Idx = srcY0Idx + srcLumaImageStrideInBytes;
        uint srcUVIdx = y * srcChromaImageStrideInBytes + (x << 3);
        uint2 pY0 = *((uint2 *)(&pSrcLumaImage[srcY0Idx]));
        uint2 pY1 = *((uint2 *)(&pSrcLumaImage[srcY1Idx]));
        uint2 pUV = *((uint2 *)(&pSrcChromaImage[srcUVIdx]));

        uint RGB0Idx = y * dstImageStrideInBytesComp + (x * 24);
        uint RGB1Idx = RGB0Idx + dstImageStrideInBytes;

        float4 f;

        uint2 pU0, pU1;
        uint2 pV0, pV1;
        f.x = unpack0_(pUV.x); f.y = f.x;
        f.z = unpack2_(pUV.x); f.w = f.z;
        pU0.x = pack_(f);
        f.x = unpack0_(pUV.y); f.y = f.x;
        f.z = unpack2_(pUV.y); f.w = f.z;
        pU0.y = pack_(f);
        pU1.x = pU0.x;
        pU1.y = pU0.y;
        f.x = unpack1_(pUV.x); f.y = f.x;
        f.z = unpack3_(pUV.x); f.w = f.z;
        pV0.x = pack_(f);
        f.x = unpack1_(pUV.y); f.y = f.x;
        f.z = unpack3_(pUV.y); f.w = f.z;
        pV0.y = pack_(f);
        pV1.x = pV0.x;
        pV1.y = pV0.y;

        float2 cR = make_float2( 0.0000f,  1.5748f);
        float2 cG = make_float2(-0.1873f, -0.4681f);
        float2 cB = make_float2( 1.8556f,  0.0000f);
        float3 yuv;
        d_uint6 pRGB0, pRGB1;

        yuv.x = unpack0_(pY0.x); yuv.y = unpack0_(pU0.x); yuv.z = unpack0_(pV0.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack1_(pY0.x); yuv.y = unpack1_(pU0.x); yuv.z = unpack1_(pV0.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.w = fmaf(cR.y, yuv.z, yuv.x); pRGB0.data[0] = pack_(f); f.x = fmaf(cG.x, yuv.y, yuv.x); f.x = fmaf(cG.y, yuv.z, f.x); f.y = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack2_(pY0.x); yuv.y = unpack2_(pU0.x); yuv.z = unpack2_(pV0.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.z = fmaf(cR.y, yuv.z, yuv.x); f.w = fmaf(cG.x, yuv.y, yuv.x); f.w = fmaf(cG.y, yuv.z, f.w); pRGB0.data[1] = pack_(f); f.x = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack3_(pY0.x); yuv.y = unpack3_(pU0.x); yuv.z = unpack3_(pV0.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.y = fmaf(cR.y, yuv.z, yuv.x); f.z = fmaf(cG.x, yuv.y, yuv.x); f.z = fmaf(cG.y, yuv.z, f.z); f.w = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[2] = pack_(f);
        yuv.x = unpack0_(pY0.y); yuv.y = unpack0_(pU0.y); yuv.z = unpack0_(pV0.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack1_(pY0.y); yuv.y = unpack1_(pU0.y); yuv.z = unpack1_(pV0.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.w = fmaf(cR.y, yuv.z, yuv.x); pRGB0.data[3] = pack_(f); f.x = fmaf(cG.x, yuv.y, yuv.x); f.x = fmaf(cG.y, yuv.z, f.x); f.y = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack2_(pY0.y); yuv.y = unpack2_(pU0.y); yuv.z = unpack2_(pV0.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.z = fmaf(cR.y, yuv.z, yuv.x); f.w = fmaf(cG.x, yuv.y, yuv.x); f.w = fmaf(cG.y, yuv.z, f.w); pRGB0.data[4] = pack_(f); f.x = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack3_(pY0.y); yuv.y = unpack3_(pU0.y); yuv.z = unpack3_(pV0.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.y = fmaf(cR.y, yuv.z, yuv.x); f.z = fmaf(cG.x, yuv.y, yuv.x); f.z = fmaf(cG.y, yuv.z, f.z); f.w = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[5] = pack_(f);
        yuv.x = unpack0_(pY1.x); yuv.y = unpack0_(pU1.x); yuv.z = unpack0_(pV1.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack1_(pY1.x); yuv.y = unpack1_(pU1.x); yuv.z = unpack1_(pV1.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.w = fmaf(cR.y, yuv.z, yuv.x); pRGB1.data[0] = pack_(f); f.x = fmaf(cG.x, yuv.y, yuv.x); f.x = fmaf(cG.y, yuv.z, f.x); f.y = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack2_(pY1.x); yuv.y = unpack2_(pU1.x); yuv.z = unpack2_(pV1.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.z = fmaf(cR.y, yuv.z, yuv.x); f.w = fmaf(cG.x, yuv.y, yuv.x); f.w = fmaf(cG.y, yuv.z, f.w); pRGB1.data[1] = pack_(f); f.x = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack3_(pY1.x); yuv.y = unpack3_(pU1.x); yuv.z = unpack3_(pV1.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.y = fmaf(cR.y, yuv.z, yuv.x); f.z = fmaf(cG.x, yuv.y, yuv.x); f.z = fmaf(cG.y, yuv.z, f.z); f.w = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[2] = pack_(f);
        yuv.x = unpack0_(pY1.y); yuv.y = unpack0_(pU1.y); yuv.z = unpack0_(pV1.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack1_(pY1.y); yuv.y = unpack1_(pU1.y); yuv.z = unpack1_(pV1.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.w = fmaf(cR.y, yuv.z, yuv.x); pRGB1.data[3] = pack_(f); f.x = fmaf(cG.x, yuv.y, yuv.x); f.x = fmaf(cG.y, yuv.z, f.x); f.y = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack2_(pY1.y); yuv.y = unpack2_(pU1.y); yuv.z = unpack2_(pV1.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.z = fmaf(cR.y, yuv.z, yuv.x); f.w = fmaf(cG.x, yuv.y, yuv.x); f.w = fmaf(cG.y, yuv.z, f.w); pRGB1.data[4] = pack_(f); f.x = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack3_(pY1.y); yuv.y = unpack3_(pU1.y); yuv.z = unpack3_(pV1.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.y = fmaf(cR.y, yuv.z, yuv.x); f.z = fmaf(cG.x, yuv.y, yuv.x); f.z = fmaf(cG.y, yuv.z, f.z); f.w = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[5] = pack_(f);

        *((d_uint6 *)(&pDstImage[RGB0Idx])) = pRGB0;
        *((d_uint6 *)(&pDstImage[RGB1Idx])) = pRGB1;
    }
}
int HipExec_ColorConvert_RGB_NV12(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcLumaImage, vx_uint32 srcLumaImageStrideInBytes,
    const vx_uint8 *pHipSrcChromaImage, vx_uint32 srcChromaImageStrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 4;
    int globalThreads_x = dstWidth;
    int globalThreads_y = dstHeight;

    vx_uint32 dstWidthComp = (dstWidth + 7) / 8;
    vx_uint32 dstHeightComp = (dstHeight + 1) / 2;
    vx_uint32 dstImageStrideInBytesComp = dstImageStrideInBytes * 2;
    vx_uint32 srcLumaImageStrideInBytesComp = srcLumaImageStrideInBytes * 2;

    hipLaunchKernelGGL(Hip_ColorConvert_RGB_NV12, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage, dstImageStrideInBytes, dstImageStrideInBytesComp,
                        (const uchar *)pHipSrcLumaImage, srcLumaImageStrideInBytes, (const uchar *)pHipSrcChromaImage, srcChromaImageStrideInBytes,
                        dstWidthComp, dstHeightComp, srcLumaImageStrideInBytesComp);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ColorConvert_RGB_NV21(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes, uint dstImageStrideInBytesComp,
    const uchar *pSrcLumaImage, uint srcLumaImageStrideInBytes, const uchar *pSrcChromaImage, uint srcChromaImageStrideInBytes,
    uint dstWidthComp, uint dstHeightComp, uint srcLumaImageStrideInBytesComp) {

    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if ((x < dstWidthComp) && (y < dstHeightComp)) {
        uint srcY0Idx = y * srcLumaImageStrideInBytesComp + (x << 3);
        uint srcY1Idx = srcY0Idx + srcLumaImageStrideInBytes;
        uint srcUVIdx = y * srcChromaImageStrideInBytes + (x << 3);
        uint2 pY0 = *((uint2 *)(&pSrcLumaImage[srcY0Idx]));
        uint2 pY1 = *((uint2 *)(&pSrcLumaImage[srcY1Idx]));
        uint2 pUV = *((uint2 *)(&pSrcChromaImage[srcUVIdx]));

        uint RGB0Idx = y * dstImageStrideInBytesComp + (x * 24);
        uint RGB1Idx = RGB0Idx + dstImageStrideInBytes;

        float4 f;

        uint2 pU0, pU1;
        uint2 pV0, pV1;
        f.x = unpack1_(pUV.x); f.y = f.x;
        f.z = unpack3_(pUV.x); f.w = f.z;
        pU0.x = pack_(f);
        f.x = unpack1_(pUV.y); f.y = f.x;
        f.z = unpack3_(pUV.y); f.w = f.z;
        pU0.y = pack_(f);
        pU1.x = pU0.x;
        pU1.y = pU0.y;
        f.x = unpack0_(pUV.x); f.y = f.x;
        f.z = unpack2_(pUV.x); f.w = f.z;
        pV0.x = pack_(f);
        f.x = unpack0_(pUV.y); f.y = f.x;
        f.z = unpack2_(pUV.y); f.w = f.z;
        pV0.y = pack_(f);
        pV1.x = pV0.x;
        pV1.y = pV0.y;

        float2 cR = make_float2( 0.0000f,  1.5748f);
        float2 cG = make_float2(-0.1873f, -0.4681f);
        float2 cB = make_float2( 1.8556f,  0.0000f);
        float3 yuv;
        d_uint6 pRGB0, pRGB1;

        yuv.x = unpack0_(pY0.x); yuv.y = unpack0_(pU0.x); yuv.z = unpack0_(pV0.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack1_(pY0.x); yuv.y = unpack1_(pU0.x); yuv.z = unpack1_(pV0.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.w = fmaf(cR.y, yuv.z, yuv.x); pRGB0.data[0] = pack_(f); f.x = fmaf(cG.x, yuv.y, yuv.x); f.x = fmaf(cG.y, yuv.z, f.x); f.y = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack2_(pY0.x); yuv.y = unpack2_(pU0.x); yuv.z = unpack2_(pV0.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.z = fmaf(cR.y, yuv.z, yuv.x); f.w = fmaf(cG.x, yuv.y, yuv.x); f.w = fmaf(cG.y, yuv.z, f.w); pRGB0.data[1] = pack_(f); f.x = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack3_(pY0.x); yuv.y = unpack3_(pU0.x); yuv.z = unpack3_(pV0.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.y = fmaf(cR.y, yuv.z, yuv.x); f.z = fmaf(cG.x, yuv.y, yuv.x); f.z = fmaf(cG.y, yuv.z, f.z); f.w = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[2] = pack_(f);
        yuv.x = unpack0_(pY0.y); yuv.y = unpack0_(pU0.y); yuv.z = unpack0_(pV0.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack1_(pY0.y); yuv.y = unpack1_(pU0.y); yuv.z = unpack1_(pV0.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.w = fmaf(cR.y, yuv.z, yuv.x); pRGB0.data[3] = pack_(f); f.x = fmaf(cG.x, yuv.y, yuv.x); f.x = fmaf(cG.y, yuv.z, f.x); f.y = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack2_(pY0.y); yuv.y = unpack2_(pU0.y); yuv.z = unpack2_(pV0.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.z = fmaf(cR.y, yuv.z, yuv.x); f.w = fmaf(cG.x, yuv.y, yuv.x); f.w = fmaf(cG.y, yuv.z, f.w); pRGB0.data[4] = pack_(f); f.x = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack3_(pY0.y); yuv.y = unpack3_(pU0.y); yuv.z = unpack3_(pV0.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.y = fmaf(cR.y, yuv.z, yuv.x); f.z = fmaf(cG.x, yuv.y, yuv.x); f.z = fmaf(cG.y, yuv.z, f.z); f.w = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[5] = pack_(f);
        yuv.x = unpack0_(pY1.x); yuv.y = unpack0_(pU1.x); yuv.z = unpack0_(pV1.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack1_(pY1.x); yuv.y = unpack1_(pU1.x); yuv.z = unpack1_(pV1.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.w = fmaf(cR.y, yuv.z, yuv.x); pRGB1.data[0] = pack_(f); f.x = fmaf(cG.x, yuv.y, yuv.x); f.x = fmaf(cG.y, yuv.z, f.x); f.y = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack2_(pY1.x); yuv.y = unpack2_(pU1.x); yuv.z = unpack2_(pV1.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.z = fmaf(cR.y, yuv.z, yuv.x); f.w = fmaf(cG.x, yuv.y, yuv.x); f.w = fmaf(cG.y, yuv.z, f.w); pRGB1.data[1] = pack_(f); f.x = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack3_(pY1.x); yuv.y = unpack3_(pU1.x); yuv.z = unpack3_(pV1.x); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.y = fmaf(cR.y, yuv.z, yuv.x); f.z = fmaf(cG.x, yuv.y, yuv.x); f.z = fmaf(cG.y, yuv.z, f.z); f.w = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[2] = pack_(f);
        yuv.x = unpack0_(pY1.y); yuv.y = unpack0_(pU1.y); yuv.z = unpack0_(pV1.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack1_(pY1.y); yuv.y = unpack1_(pU1.y); yuv.z = unpack1_(pV1.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.w = fmaf(cR.y, yuv.z, yuv.x); pRGB1.data[3] = pack_(f); f.x = fmaf(cG.x, yuv.y, yuv.x); f.x = fmaf(cG.y, yuv.z, f.x); f.y = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack2_(pY1.y); yuv.y = unpack2_(pU1.y); yuv.z = unpack2_(pV1.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.z = fmaf(cR.y, yuv.z, yuv.x); f.w = fmaf(cG.x, yuv.y, yuv.x); f.w = fmaf(cG.y, yuv.z, f.w); pRGB1.data[4] = pack_(f); f.x = fmaf(cB.x, yuv.y, yuv.x);
        yuv.x = unpack3_(pY1.y); yuv.y = unpack3_(pU1.y); yuv.z = unpack3_(pV1.y); yuv.y -= 128.0f; yuv.z -= 128.0f;
        f.y = fmaf(cR.y, yuv.z, yuv.x); f.z = fmaf(cG.x, yuv.y, yuv.x); f.z = fmaf(cG.y, yuv.z, f.z); f.w = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[5] = pack_(f);

        *((d_uint6 *)(&pDstImage[RGB0Idx])) = pRGB0;
        *((d_uint6 *)(&pDstImage[RGB1Idx])) = pRGB1;
    }
}
int HipExec_ColorConvert_RGB_NV21(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcLumaImage, vx_uint32 srcLumaImageStrideInBytes,
    const vx_uint8 *pHipSrcChromaImage, vx_uint32 srcChromaImageStrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 4;
    int globalThreads_x = dstWidth;
    int globalThreads_y = dstHeight;

    vx_uint32 dstWidthComp = (dstWidth + 7) / 8;
    vx_uint32 dstHeightComp = (dstHeight + 1) / 2;
    vx_uint32 dstImageStrideInBytesComp = dstImageStrideInBytes * 2;
    vx_uint32 srcLumaImageStrideInBytesComp = srcLumaImageStrideInBytes * 2;

    hipLaunchKernelGGL(Hip_ColorConvert_RGB_NV21, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage, dstImageStrideInBytes, dstImageStrideInBytesComp,
                        (const uchar *)pHipSrcLumaImage, srcLumaImageStrideInBytes, (const uchar *)pHipSrcChromaImage, srcChromaImageStrideInBytes,
                        dstWidthComp, dstHeightComp, srcLumaImageStrideInBytesComp);

    return VX_SUCCESS;
}





// Group 4 - Destination RGBX, Source Planar

__global__ void __attribute__((visibility("default")))
Hip_ColorConvert_RGBX_IYUV(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes, uint dstImageStrideInBytesComp,
    const uchar *pSrcYImage, uint srcYImageStrideInBytes, const uchar *pSrcUImage, uint srcUImageStrideInBytes, const uchar *pSrcVImage, uint srcVImageStrideInBytes,
    uint dstWidthComp, uint dstHeightComp, uint srcYImageStrideInBytesComp) {

    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if ((x < dstWidthComp) && (y < dstHeightComp)) {
        uint srcY0Idx = y * srcYImageStrideInBytesComp + (x << 3);
        uint srcY1Idx = srcY0Idx + srcYImageStrideInBytes;
        uint srcUIdx = y * srcUImageStrideInBytes + (x << 2);
        uint srcVIdx = y * srcVImageStrideInBytes + (x << 2);
        uint2 pY0 = *((uint2 *)(&pSrcYImage[srcY0Idx]));
        uint2 pY1 = *((uint2 *)(&pSrcYImage[srcY1Idx]));
        uint2 pUV;
        pUV.x = *((uint *)(&pSrcUImage[srcUIdx]));
        pUV.y = *((uint *)(&pSrcVImage[srcVIdx]));

        uint RGB0Idx = y * dstImageStrideInBytesComp + (x << 5);
        uint RGB1Idx = RGB0Idx + dstImageStrideInBytes;

        float4 f;

        uint2 pU0, pU1;
        uint2 pV0, pV1;
        f.x = unpack0_(pUV.x); f.y = f.x;
        f.z = unpack1_(pUV.x); f.w = f.z;
        pU0.x = pack_(f);
        f.x = unpack2_(pUV.x); f.y = f.x;
        f.z = unpack3_(pUV.x); f.w = f.z;
        pU0.y = pack_(f);
        pU1.x = pU0.x;
        pU1.y = pU0.y;
        f.x = unpack0_(pUV.y); f.y = f.x;
        f.z = unpack1_(pUV.y); f.w = f.z;
        pV0.x = pack_(f);
        f.x = unpack2_(pUV.y); f.y = f.x;
        f.z = unpack3_(pUV.y); f.w = f.z;
        pV0.y = pack_(f);
        pV1.x = pV0.x;
        pV1.y = pV0.y;

        float2 cR = make_float2( 0.0000f,  1.5748f);
        float2 cG = make_float2(-0.1873f, -0.4681f);
        float2 cB = make_float2( 1.8556f,  0.0000f);
        float3 yuv;
        f.w = 255.0f;
        d_uint8 pRGB0, pRGB1;

        yuv.x = unpack0_(pY0.x); yuv.y = unpack0_(pU0.x); yuv.z = unpack0_(pV0.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[0] = pack_(f);
        yuv.x = unpack1_(pY0.x); yuv.y = unpack1_(pU0.x); yuv.z = unpack1_(pV0.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[1] = pack_(f);
        yuv.x = unpack2_(pY0.x); yuv.y = unpack2_(pU0.x); yuv.z = unpack2_(pV0.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[2] = pack_(f);
        yuv.x = unpack3_(pY0.x); yuv.y = unpack3_(pU0.x); yuv.z = unpack3_(pV0.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[3] = pack_(f);
        yuv.x = unpack0_(pY0.y); yuv.y = unpack0_(pU0.y); yuv.z = unpack0_(pV0.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[4] = pack_(f);
        yuv.x = unpack1_(pY0.y); yuv.y = unpack1_(pU0.y); yuv.z = unpack1_(pV0.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[5] = pack_(f);
        yuv.x = unpack2_(pY0.y); yuv.y = unpack2_(pU0.y); yuv.z = unpack2_(pV0.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[6] = pack_(f);
        yuv.x = unpack3_(pY0.y); yuv.y = unpack3_(pU0.y); yuv.z = unpack3_(pV0.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[7] = pack_(f);
        yuv.x = unpack0_(pY1.x); yuv.y = unpack0_(pU1.x); yuv.z = unpack0_(pV1.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[0] = pack_(f);
        yuv.x = unpack1_(pY1.x); yuv.y = unpack1_(pU1.x); yuv.z = unpack1_(pV1.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[1] = pack_(f);
        yuv.x = unpack2_(pY1.x); yuv.y = unpack2_(pU1.x); yuv.z = unpack2_(pV1.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[2] = pack_(f);
        yuv.x = unpack3_(pY1.x); yuv.y = unpack3_(pU1.x); yuv.z = unpack3_(pV1.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[3] = pack_(f);
        yuv.x = unpack0_(pY1.y); yuv.y = unpack0_(pU1.y); yuv.z = unpack0_(pV1.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[4] = pack_(f);
        yuv.x = unpack1_(pY1.y); yuv.y = unpack1_(pU1.y); yuv.z = unpack1_(pV1.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[5] = pack_(f);
        yuv.x = unpack2_(pY1.y); yuv.y = unpack2_(pU1.y); yuv.z = unpack2_(pV1.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[6] = pack_(f);
        yuv.x = unpack3_(pY1.y); yuv.y = unpack3_(pU1.y); yuv.z = unpack3_(pV1.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[7] = pack_(f);

        *((d_uint8 *)(&pDstImage[RGB0Idx])) = pRGB0;
        *((d_uint8 *)(&pDstImage[RGB1Idx])) = pRGB1;

    }
}
int HipExec_ColorConvert_RGBX_IYUV(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcYImage, vx_uint32 srcYImageStrideInBytes,
    const vx_uint8 *pHipSrcUImage, vx_uint32 srcUImageStrideInBytes,
    const vx_uint8 *pHipSrcVImage, vx_uint32 srcVImageStrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 4;
    int globalThreads_x = dstWidth;
    int globalThreads_y = dstHeight;

    vx_uint32 dstWidthComp = (dstWidth + 7) / 8;
    vx_uint32 dstHeightComp = (dstHeight + 1) / 2;
    vx_uint32 dstImageStrideInBytesComp = dstImageStrideInBytes * 2;
    vx_uint32 srcYImageStrideInBytesComp = srcYImageStrideInBytes * 2;

    hipLaunchKernelGGL(Hip_ColorConvert_RGBX_IYUV, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage, dstImageStrideInBytes, dstImageStrideInBytesComp,
                        (const uchar *)pHipSrcYImage, srcYImageStrideInBytes, (const uchar *)pHipSrcUImage, srcUImageStrideInBytes, (const uchar *)pHipSrcVImage, srcVImageStrideInBytes,
                        dstWidthComp, dstHeightComp, srcYImageStrideInBytesComp);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ColorConvert_RGBX_NV12(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes, uint dstImageStrideInBytesComp,
    const uchar *pSrcLumaImage, uint srcLumaImageStrideInBytes, const uchar *pSrcChromaImage, uint srcChromaImageStrideInBytes,
    uint dstWidthComp, uint dstHeightComp, uint srcLumaImageStrideInBytesComp) {

    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if ((x < dstWidthComp) && (y < dstHeightComp)) {
        uint srcY0Idx = y * srcLumaImageStrideInBytesComp + (x << 3);
        uint srcY1Idx = srcY0Idx + srcLumaImageStrideInBytes;
        uint srcUVIdx = y * srcChromaImageStrideInBytes + (x << 3);
        uint2 pY0 = *((uint2 *)(&pSrcLumaImage[srcY0Idx]));
        uint2 pY1 = *((uint2 *)(&pSrcLumaImage[srcY1Idx]));
        uint2 pUV = *((uint2 *)(&pSrcChromaImage[srcUVIdx]));

        uint RGB0Idx = y * dstImageStrideInBytesComp + (x << 5);
        uint RGB1Idx = RGB0Idx + dstImageStrideInBytes;

        float4 f;

        uint2 pU0, pU1;
        uint2 pV0, pV1;
        f.x = unpack0_(pUV.x); f.y = f.x;
        f.z = unpack2_(pUV.x); f.w = f.z;
        pU0.x = pack_(f);
        f.x = unpack0_(pUV.y); f.y = f.x;
        f.z = unpack2_(pUV.y); f.w = f.z;
        pU0.y = pack_(f);
        pU1.x = pU0.x;
        pU1.y = pU0.y;
        f.x = unpack1_(pUV.x); f.y = f.x;
        f.z = unpack3_(pUV.x); f.w = f.z;
        pV0.x = pack_(f);
        f.x = unpack1_(pUV.y); f.y = f.x;
        f.z = unpack3_(pUV.y); f.w = f.z;
        pV0.y = pack_(f);
        pV1.x = pV0.x;
        pV1.y = pV0.y;

        float2 cR = make_float2( 0.0000f,  1.5748f);
        float2 cG = make_float2(-0.1873f, -0.4681f);
        float2 cB = make_float2( 1.8556f,  0.0000f);
        float3 yuv;
        f.w = 255.0f;
        d_uint8 pRGB0, pRGB1;

        yuv.x = unpack0_(pY0.x); yuv.y = unpack0_(pU0.x); yuv.z = unpack0_(pV0.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[0] = pack_(f);
        yuv.x = unpack1_(pY0.x); yuv.y = unpack1_(pU0.x); yuv.z = unpack1_(pV0.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[1] = pack_(f);
        yuv.x = unpack2_(pY0.x); yuv.y = unpack2_(pU0.x); yuv.z = unpack2_(pV0.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[2] = pack_(f);
        yuv.x = unpack3_(pY0.x); yuv.y = unpack3_(pU0.x); yuv.z = unpack3_(pV0.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[3] = pack_(f);
        yuv.x = unpack0_(pY0.y); yuv.y = unpack0_(pU0.y); yuv.z = unpack0_(pV0.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[4] = pack_(f);
        yuv.x = unpack1_(pY0.y); yuv.y = unpack1_(pU0.y); yuv.z = unpack1_(pV0.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[5] = pack_(f);
        yuv.x = unpack2_(pY0.y); yuv.y = unpack2_(pU0.y); yuv.z = unpack2_(pV0.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[6] = pack_(f);
        yuv.x = unpack3_(pY0.y); yuv.y = unpack3_(pU0.y); yuv.z = unpack3_(pV0.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[7] = pack_(f);
        yuv.x = unpack0_(pY1.x); yuv.y = unpack0_(pU1.x); yuv.z = unpack0_(pV1.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[0] = pack_(f);
        yuv.x = unpack1_(pY1.x); yuv.y = unpack1_(pU1.x); yuv.z = unpack1_(pV1.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[1] = pack_(f);
        yuv.x = unpack2_(pY1.x); yuv.y = unpack2_(pU1.x); yuv.z = unpack2_(pV1.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[2] = pack_(f);
        yuv.x = unpack3_(pY1.x); yuv.y = unpack3_(pU1.x); yuv.z = unpack3_(pV1.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[3] = pack_(f);
        yuv.x = unpack0_(pY1.y); yuv.y = unpack0_(pU1.y); yuv.z = unpack0_(pV1.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[4] = pack_(f);
        yuv.x = unpack1_(pY1.y); yuv.y = unpack1_(pU1.y); yuv.z = unpack1_(pV1.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[5] = pack_(f);
        yuv.x = unpack2_(pY1.y); yuv.y = unpack2_(pU1.y); yuv.z = unpack2_(pV1.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[6] = pack_(f);
        yuv.x = unpack3_(pY1.y); yuv.y = unpack3_(pU1.y); yuv.z = unpack3_(pV1.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[7] = pack_(f);

        *((d_uint8 *)(&pDstImage[RGB0Idx])) = pRGB0;
        *((d_uint8 *)(&pDstImage[RGB1Idx])) = pRGB1;
    }
}
int HipExec_ColorConvert_RGBX_NV12(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcLumaImage, vx_uint32 srcLumaImageStrideInBytes,
    const vx_uint8 *pHipSrcChromaImage, vx_uint32 srcChromaImageStrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 4;
    int globalThreads_x = dstWidth;
    int globalThreads_y = dstHeight;

    vx_uint32 dstWidthComp = (dstWidth + 7) / 8;
    vx_uint32 dstHeightComp = (dstHeight + 1) / 2;
    vx_uint32 dstImageStrideInBytesComp = dstImageStrideInBytes * 2;
    vx_uint32 srcLumaImageStrideInBytesComp = srcLumaImageStrideInBytes * 2;

    hipLaunchKernelGGL(Hip_ColorConvert_RGBX_NV12, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage, dstImageStrideInBytes, dstImageStrideInBytesComp,
                        (const uchar *)pHipSrcLumaImage, srcLumaImageStrideInBytes, (const uchar *)pHipSrcChromaImage, srcChromaImageStrideInBytes,
                        dstWidthComp, dstHeightComp, srcLumaImageStrideInBytesComp);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ColorConvert_RGBX_NV21(uint dstWidth, uint dstHeight,
    uchar *pDstImage, uint dstImageStrideInBytes, uint dstImageStrideInBytesComp,
    const uchar *pSrcLumaImage, uint srcLumaImageStrideInBytes, const uchar *pSrcChromaImage, uint srcChromaImageStrideInBytes,
    uint dstWidthComp, uint dstHeightComp, uint srcLumaImageStrideInBytesComp) {

    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if ((x < dstWidthComp) && (y < dstHeightComp)) {
        uint srcY0Idx = y * srcLumaImageStrideInBytesComp + (x << 3);
        uint srcY1Idx = srcY0Idx + srcLumaImageStrideInBytes;
        uint srcUVIdx = y * srcChromaImageStrideInBytes + (x << 3);
        uint2 pY0 = *((uint2 *)(&pSrcLumaImage[srcY0Idx]));
        uint2 pY1 = *((uint2 *)(&pSrcLumaImage[srcY1Idx]));
        uint2 pUV = *((uint2 *)(&pSrcChromaImage[srcUVIdx]));

        uint RGB0Idx = y * dstImageStrideInBytesComp + (x << 5);
        uint RGB1Idx = RGB0Idx + dstImageStrideInBytes;

        float4 f;

        uint2 pU0, pU1;
        uint2 pV0, pV1;
        f.x = unpack1_(pUV.x); f.y = f.x;
        f.z = unpack3_(pUV.x); f.w = f.z;
        pU0.x = pack_(f);
        f.x = unpack1_(pUV.y); f.y = f.x;
        f.z = unpack3_(pUV.y); f.w = f.z;
        pU0.y = pack_(f);
        pU1.x = pU0.x;
        pU1.y = pU0.y;
        f.x = unpack0_(pUV.x); f.y = f.x;
        f.z = unpack2_(pUV.x); f.w = f.z;
        pV0.x = pack_(f);
        f.x = unpack0_(pUV.y); f.y = f.x;
        f.z = unpack2_(pUV.y); f.w = f.z;
        pV0.y = pack_(f);
        pV1.x = pV0.x;
        pV1.y = pV0.y;

        float2 cR = make_float2( 0.0000f,  1.5748f);
        float2 cG = make_float2(-0.1873f, -0.4681f);
        float2 cB = make_float2( 1.8556f,  0.0000f);
        float3 yuv;
        f.w = 255.0f;
        d_uint8 pRGB0, pRGB1;

        yuv.x = unpack0_(pY0.x); yuv.y = unpack0_(pU0.x); yuv.z = unpack0_(pV0.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[0] = pack_(f);
        yuv.x = unpack1_(pY0.x); yuv.y = unpack1_(pU0.x); yuv.z = unpack1_(pV0.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[1] = pack_(f);
        yuv.x = unpack2_(pY0.x); yuv.y = unpack2_(pU0.x); yuv.z = unpack2_(pV0.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[2] = pack_(f);
        yuv.x = unpack3_(pY0.x); yuv.y = unpack3_(pU0.x); yuv.z = unpack3_(pV0.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[3] = pack_(f);
        yuv.x = unpack0_(pY0.y); yuv.y = unpack0_(pU0.y); yuv.z = unpack0_(pV0.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[4] = pack_(f);
        yuv.x = unpack1_(pY0.y); yuv.y = unpack1_(pU0.y); yuv.z = unpack1_(pV0.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[5] = pack_(f);
        yuv.x = unpack2_(pY0.y); yuv.y = unpack2_(pU0.y); yuv.z = unpack2_(pV0.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[6] = pack_(f);
        yuv.x = unpack3_(pY0.y); yuv.y = unpack3_(pU0.y); yuv.z = unpack3_(pV0.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB0.data[7] = pack_(f);
        yuv.x = unpack0_(pY1.x); yuv.y = unpack0_(pU1.x); yuv.z = unpack0_(pV1.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[0] = pack_(f);
        yuv.x = unpack1_(pY1.x); yuv.y = unpack1_(pU1.x); yuv.z = unpack1_(pV1.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[1] = pack_(f);
        yuv.x = unpack2_(pY1.x); yuv.y = unpack2_(pU1.x); yuv.z = unpack2_(pV1.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[2] = pack_(f);
        yuv.x = unpack3_(pY1.x); yuv.y = unpack3_(pU1.x); yuv.z = unpack3_(pV1.x); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[3] = pack_(f);
        yuv.x = unpack0_(pY1.y); yuv.y = unpack0_(pU1.y); yuv.z = unpack0_(pV1.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[4] = pack_(f);
        yuv.x = unpack1_(pY1.y); yuv.y = unpack1_(pU1.y); yuv.z = unpack1_(pV1.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[5] = pack_(f);
        yuv.x = unpack2_(pY1.y); yuv.y = unpack2_(pU1.y); yuv.z = unpack2_(pV1.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[6] = pack_(f);
        yuv.x = unpack3_(pY1.y); yuv.y = unpack3_(pU1.y); yuv.z = unpack3_(pV1.y); yuv.y -= 128.0f;; yuv.z -= 128.0f;
        f.x = fmaf(cR.y, yuv.z, yuv.x); f.y = fmaf(cG.x, yuv.y, yuv.x); f.y = fmaf(cG.y, yuv.z, f.y); f.z = fmaf(cB.x, yuv.y, yuv.x); pRGB1.data[7] = pack_(f);

        *((d_uint8 *)(&pDstImage[RGB0Idx])) = pRGB0;
        *((d_uint8 *)(&pDstImage[RGB1Idx])) = pRGB1;
    }
}
int HipExec_ColorConvert_RGBX_NV21(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstImageStrideInBytes,
    const vx_uint8 *pHipSrcLumaImage, vx_uint32 srcLumaImageStrideInBytes,
    const vx_uint8 *pHipSrcChromaImage, vx_uint32 srcChromaImageStrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 4;
    int globalThreads_x = dstWidth;
    int globalThreads_y = dstHeight;

    vx_uint32 dstWidthComp = (dstWidth + 7) / 8;
    vx_uint32 dstHeightComp = (dstHeight + 1) / 2;
    vx_uint32 dstImageStrideInBytesComp = dstImageStrideInBytes * 2;
    vx_uint32 srcLumaImageStrideInBytesComp = srcLumaImageStrideInBytes * 2;

    hipLaunchKernelGGL(Hip_ColorConvert_RGBX_NV21, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImage, dstImageStrideInBytes, dstImageStrideInBytesComp,
                        (const uchar *)pHipSrcLumaImage, srcLumaImageStrideInBytes, (const uchar *)pHipSrcChromaImage, srcChromaImageStrideInBytes,
                        dstWidthComp, dstHeightComp, srcLumaImageStrideInBytesComp);

    return VX_SUCCESS;
}





// Group 5 - Destination IYUV, Source Packed

__global__ void __attribute__((visibility("default")))
Hip_ColorConvert_IYUV_RGB(uint dstWidth, uint dstHeight,
    uchar *pDstYImage, uint dstYImageStrideInBytes, uchar *pDstUImage, uint dstUImageStrideInBytes, uchar *pDstVImage, uint dstVImageStrideInBytes,
    const uchar *pSrcImage, uint srcImageStrideInBytes,
    uint dstWidthComp, uint dstHeightComp, uint srcImageStrideInBytesComp, uint dstYImageStrideInBytesComp) {

    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if ((x < dstWidthComp) && (y < dstHeightComp)) {
        uint srcIdx0 = y * srcImageStrideInBytesComp + (x * 24);
        uint srcIdx1 = srcIdx0 + srcImageStrideInBytes;
        d_uint6 pRGB0 = *((d_uint6 *)(&pSrcImage[srcIdx0]));
        d_uint6 pRGB1 = *((d_uint6 *)(&pSrcImage[srcIdx1]));

        uint dstY0Idx  = y * dstYImageStrideInBytesComp + (x << 3);
        uint dstY1Idx  = dstY0Idx + dstYImageStrideInBytes;
        uint dstUIdx  = y * dstUImageStrideInBytes + (x << 2);
        uint dstVIdx  = y * dstVImageStrideInBytes + (x << 2);

        float4 f;

        float4 cY = make_float4(0.2126f, 0.7152f, 0.0722f, 0.0f);
        float3 cY3 = make_float3(0.2126f, 0.7152f, 0.0722f);
        uint2 pY0, pY1;
        f.x = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB0.data[0]), unpack1_(pRGB0.data[0]), unpack2_(pRGB0.data[0])));
        f.y = cY.w + dot3_(cY3, make_float3(unpack3_(pRGB0.data[0]), unpack0_(pRGB0.data[1]), unpack1_(pRGB0.data[1])));
        f.z = cY.w + dot3_(cY3, make_float3(unpack2_(pRGB0.data[1]), unpack3_(pRGB0.data[1]), unpack0_(pRGB0.data[2])));
        f.w = cY.w + dot3_(cY3, make_float3(unpack1_(pRGB0.data[2]), unpack2_(pRGB0.data[2]), unpack3_(pRGB0.data[2])));
        pY0.x = pack_(f);
        f.x = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB0.data[3]), unpack1_(pRGB0.data[3]), unpack2_(pRGB0.data[3])));
        f.y = cY.w + dot3_(cY3, make_float3(unpack3_(pRGB0.data[3]), unpack0_(pRGB0.data[4]), unpack1_(pRGB0.data[4])));
        f.z = cY.w + dot3_(cY3, make_float3(unpack2_(pRGB0.data[4]), unpack3_(pRGB0.data[4]), unpack0_(pRGB0.data[5])));
        f.w = cY.w + dot3_(cY3, make_float3(unpack1_(pRGB0.data[5]), unpack2_(pRGB0.data[5]), unpack3_(pRGB0.data[5])));
        pY0.y = pack_(f);
        f.x = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB1.data[0]), unpack1_(pRGB1.data[0]), unpack2_(pRGB1.data[0])));
        f.y = cY.w + dot3_(cY3, make_float3(unpack3_(pRGB1.data[0]), unpack0_(pRGB1.data[1]), unpack1_(pRGB1.data[1])));
        f.z = cY.w + dot3_(cY3, make_float3(unpack2_(pRGB1.data[1]), unpack3_(pRGB1.data[1]), unpack0_(pRGB1.data[2])));
        f.w = cY.w + dot3_(cY3, make_float3(unpack1_(pRGB1.data[2]), unpack2_(pRGB1.data[2]), unpack3_(pRGB1.data[2])));
        pY1.x = pack_(f);
        f.x = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB1.data[3]), unpack1_(pRGB1.data[3]), unpack2_(pRGB1.data[3])));
        f.y = cY.w + dot3_(cY3, make_float3(unpack3_(pRGB1.data[3]), unpack0_(pRGB1.data[4]), unpack1_(pRGB1.data[4])));
        f.z = cY.w + dot3_(cY3, make_float3(unpack2_(pRGB1.data[4]), unpack3_(pRGB1.data[4]), unpack0_(pRGB1.data[5])));
        f.w = cY.w + dot3_(cY3, make_float3(unpack1_(pRGB1.data[5]), unpack2_(pRGB1.data[5]), unpack3_(pRGB1.data[5])));
        pY1.y = pack_(f);

        float3 cU = make_float3(-0.1146f, -0.3854f, 0.5f);
        uint2 pU0, pU1;
        f.x = dot3_(cU, make_float3(unpack0_(pRGB0.data[0]), unpack1_(pRGB0.data[0]), unpack2_(pRGB0.data[0])));
        f.y = dot3_(cU, make_float3(unpack2_(pRGB0.data[1]), unpack3_(pRGB0.data[1]), unpack0_(pRGB0.data[2])));
        f.z = dot3_(cU, make_float3(unpack0_(pRGB0.data[3]), unpack1_(pRGB0.data[3]), unpack2_(pRGB0.data[3])));
        f.w = dot3_(cU, make_float3(unpack2_(pRGB0.data[4]), unpack3_(pRGB0.data[4]), unpack0_(pRGB0.data[5])));
        pU0.x = pack_(f + (float4)(128));
        f.x = dot3_(cU, make_float3(unpack3_(pRGB0.data[0]), unpack0_(pRGB0.data[1]), unpack1_(pRGB0.data[1])));
        f.y = dot3_(cU, make_float3(unpack1_(pRGB0.data[2]), unpack2_(pRGB0.data[2]), unpack3_(pRGB0.data[2])));
        f.z = dot3_(cU, make_float3(unpack3_(pRGB0.data[3]), unpack0_(pRGB0.data[4]), unpack1_(pRGB0.data[4])));
        f.w = dot3_(cU, make_float3(unpack1_(pRGB0.data[5]), unpack2_(pRGB0.data[5]), unpack3_(pRGB0.data[5])));
        pU0.y = pack_(f + (float4)(128));
        f.x = dot3_(cU, make_float3(unpack0_(pRGB1.data[0]), unpack1_(pRGB1.data[0]), unpack2_(pRGB1.data[0])));
        f.y = dot3_(cU, make_float3(unpack2_(pRGB1.data[1]), unpack3_(pRGB1.data[1]), unpack0_(pRGB1.data[2])));
        f.z = dot3_(cU, make_float3(unpack0_(pRGB1.data[3]), unpack1_(pRGB1.data[3]), unpack2_(pRGB1.data[3])));
        f.w = dot3_(cU, make_float3(unpack2_(pRGB1.data[4]), unpack3_(pRGB1.data[4]), unpack0_(pRGB1.data[5])));
        pU1.x = pack_(f + (float4)(128));
        f.x = dot3_(cU, make_float3(unpack3_(pRGB1.data[0]), unpack0_(pRGB1.data[1]), unpack1_(pRGB1.data[1])));
        f.y = dot3_(cU, make_float3(unpack1_(pRGB1.data[2]), unpack2_(pRGB1.data[2]), unpack3_(pRGB1.data[2])));
        f.z = dot3_(cU, make_float3(unpack3_(pRGB1.data[3]), unpack0_(pRGB1.data[4]), unpack1_(pRGB1.data[4])));
        f.w = dot3_(cU, make_float3(unpack1_(pRGB1.data[5]), unpack2_(pRGB1.data[5]), unpack3_(pRGB1.data[5])));
        pU1.y = pack_(f + (float4)(128));
        pU0.x = lerp_(pU0.x, pU0.y, 0x01010101u);
        pU1.x = lerp_(pU1.x, pU1.y, 0x01010101u);
        pU0.x = lerp_(pU0.x, pU1.x, 0x01010101u);

        float3 cV = make_float3(0.5f, -0.4542f, -0.0458f);
        uint2 pV0, pV1;
        f.x = dot3_(cV, make_float3(unpack0_(pRGB0.data[0]), unpack1_(pRGB0.data[0]), unpack2_(pRGB0.data[0])));
        f.y = dot3_(cV, make_float3(unpack2_(pRGB0.data[1]), unpack3_(pRGB0.data[1]), unpack0_(pRGB0.data[2])));
        f.z = dot3_(cV, make_float3(unpack0_(pRGB0.data[3]), unpack1_(pRGB0.data[3]), unpack2_(pRGB0.data[3])));
        f.w = dot3_(cV, make_float3(unpack2_(pRGB0.data[4]), unpack3_(pRGB0.data[4]), unpack0_(pRGB0.data[5])));
        pV0.x = pack_(f + (float4)(128));
        f.x = dot3_(cV, make_float3(unpack3_(pRGB0.data[0]), unpack0_(pRGB0.data[1]), unpack1_(pRGB0.data[1])));
        f.y = dot3_(cV, make_float3(unpack1_(pRGB0.data[2]), unpack2_(pRGB0.data[2]), unpack3_(pRGB0.data[2])));
        f.z = dot3_(cV, make_float3(unpack3_(pRGB0.data[3]), unpack0_(pRGB0.data[4]), unpack1_(pRGB0.data[4])));
        f.w = dot3_(cV, make_float3(unpack1_(pRGB0.data[5]), unpack2_(pRGB0.data[5]), unpack3_(pRGB0.data[5])));
        pV0.y = pack_(f + (float4)(128));
        f.x = dot3_(cV, make_float3(unpack0_(pRGB1.data[0]), unpack1_(pRGB1.data[0]), unpack2_(pRGB1.data[0])));
        f.y = dot3_(cV, make_float3(unpack2_(pRGB1.data[1]), unpack3_(pRGB1.data[1]), unpack0_(pRGB1.data[2])));
        f.z = dot3_(cV, make_float3(unpack0_(pRGB1.data[3]), unpack1_(pRGB1.data[3]), unpack2_(pRGB1.data[3])));
        f.w = dot3_(cV, make_float3(unpack2_(pRGB1.data[4]), unpack3_(pRGB1.data[4]), unpack0_(pRGB1.data[5])));
        pV1.x = pack_(f + (float4)(128));
        f.x = dot3_(cV, make_float3(unpack3_(pRGB1.data[0]), unpack0_(pRGB1.data[1]), unpack1_(pRGB1.data[1])));
        f.y = dot3_(cV, make_float3(unpack1_(pRGB1.data[2]), unpack2_(pRGB1.data[2]), unpack3_(pRGB1.data[2])));
        f.z = dot3_(cV, make_float3(unpack3_(pRGB1.data[3]), unpack0_(pRGB1.data[4]), unpack1_(pRGB1.data[4])));
        f.w = dot3_(cV, make_float3(unpack1_(pRGB1.data[5]), unpack2_(pRGB1.data[5]), unpack3_(pRGB1.data[5])));
        pV1.y = pack_(f + (float4)(128));
        pV0.x = lerp_(pV0.x, pV0.y, 0x01010101u);
        pV1.x = lerp_(pV1.x, pV1.y, 0x01010101u);
        pV0.x = lerp_(pV0.x, pV1.x, 0x01010101u);

        *((uint2 *)(&pDstYImage[dstY0Idx])) = pY0;
        *((uint2 *)(&pDstYImage[dstY1Idx])) = pY1;
        *((uint *)(&pDstUImage[dstUIdx])) = pU0.x;
        *((uint *)(&pDstVImage[dstVIdx])) = pV0.x;
    }
}
int HipExec_ColorConvert_IYUV_RGB(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstYImage, vx_uint32 dstYImageStrideInBytes,
    vx_uint8 *pHipDstUImage, vx_uint32 dstUImageStrideInBytes,
    vx_uint8 *pHipDstVImage, vx_uint32 dstVImageStrideInBytes,
    const vx_uint8 *pHipSrcImage, vx_uint32 srcImageStrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 4;
    int globalThreads_x = dstWidth;
    int globalThreads_y = dstHeight;

    vx_uint32 dstWidthComp = (dstWidth + 7) / 8;
    vx_uint32 dstHeightComp = (dstHeight + 1) / 2;
    vx_uint32 srcImageStrideInBytesComp = srcImageStrideInBytes * 2;
    vx_uint32 dstYImageStrideInBytesComp = dstYImageStrideInBytes * 2;

    hipLaunchKernelGGL(Hip_ColorConvert_IYUV_RGB, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstYImage, dstYImageStrideInBytes, (uchar *)pHipDstUImage, dstUImageStrideInBytes, (uchar *)pHipDstVImage, dstVImageStrideInBytes,
                        (const uchar *)pHipSrcImage, srcImageStrideInBytes,
                        dstWidthComp, dstHeightComp, srcImageStrideInBytesComp, dstYImageStrideInBytesComp);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ColorConvert_IYUV_RGBX(uint dstWidth, uint dstHeight,
    uchar *pDstYImage, uint dstYImageStrideInBytes, uchar *pDstUImage, uint dstUImageStrideInBytes, uchar *pDstVImage, uint dstVImageStrideInBytes,
    const uchar *pSrcImage, uint srcImageStrideInBytes,
    uint dstWidthComp, uint dstHeightComp, uint srcImageStrideInBytesComp, uint dstYImageStrideInBytesComp) {

    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if ((x < dstWidthComp) && (y < dstHeightComp)) {
        uint srcIdx0 = y * srcImageStrideInBytesComp + (x << 5);
        uint srcIdx1 = srcIdx0 + srcImageStrideInBytes;
        d_uint8 pRGB0 = *((d_uint8 *)(&pSrcImage[srcIdx0]));
        d_uint8 pRGB1 = *((d_uint8 *)(&pSrcImage[srcIdx1]));

        uint dstY0Idx  = y * dstYImageStrideInBytesComp + (x << 3);
        uint dstY1Idx  = dstY0Idx + dstYImageStrideInBytes;
        uint dstUIdx  = y * dstUImageStrideInBytes + (x << 2);
        uint dstVIdx  = y * dstVImageStrideInBytes + (x << 2);

        float4 f;

        float4 cY = make_float4(0.2126f, 0.7152f, 0.0722f, 0.0f);
        float3 cY3 = make_float3(0.2126f, 0.7152f, 0.0722f);
        uint2 pY0, pY1;
        f.x = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB0.data[0]), unpack1_(pRGB0.data[0]), unpack2_(pRGB0.data[0])));
        f.y = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB0.data[1]), unpack1_(pRGB0.data[1]), unpack2_(pRGB0.data[1])));
        f.z = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB0.data[2]), unpack1_(pRGB0.data[2]), unpack2_(pRGB0.data[2])));
        f.w = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB0.data[3]), unpack1_(pRGB0.data[3]), unpack2_(pRGB0.data[3])));
        pY0.x = pack_(f);
        f.x = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB0.data[4]), unpack1_(pRGB0.data[4]), unpack2_(pRGB0.data[4])));
        f.y = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB0.data[5]), unpack1_(pRGB0.data[5]), unpack2_(pRGB0.data[5])));
        f.z = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB0.data[6]), unpack1_(pRGB0.data[6]), unpack2_(pRGB0.data[6])));
        f.w = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB0.data[7]), unpack1_(pRGB0.data[7]), unpack2_(pRGB0.data[7])));
        pY0.y = pack_(f);
        f.x = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB1.data[0]), unpack1_(pRGB1.data[0]), unpack2_(pRGB1.data[0])));
        f.y = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB1.data[1]), unpack1_(pRGB1.data[1]), unpack2_(pRGB1.data[1])));
        f.z = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB1.data[2]), unpack1_(pRGB1.data[2]), unpack2_(pRGB1.data[2])));
        f.w = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB1.data[3]), unpack1_(pRGB1.data[3]), unpack2_(pRGB1.data[3])));
        pY1.x = pack_(f);
        f.x = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB1.data[4]), unpack1_(pRGB1.data[4]), unpack2_(pRGB1.data[4])));
        f.y = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB1.data[5]), unpack1_(pRGB1.data[5]), unpack2_(pRGB1.data[5])));
        f.z = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB1.data[6]), unpack1_(pRGB1.data[6]), unpack2_(pRGB1.data[6])));
        f.w = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB1.data[7]), unpack1_(pRGB1.data[7]), unpack2_(pRGB1.data[7])));
        pY1.y = pack_(f);

        float3 cU = make_float3(-0.1146f, -0.3854f, 0.5f);
        uint2 pU0, pU1;
        f.x = dot3_(cU, make_float3(unpack0_(pRGB0.data[0]), unpack1_(pRGB0.data[0]), unpack2_(pRGB0.data[0])));
        f.y = dot3_(cU, make_float3(unpack0_(pRGB0.data[2]), unpack1_(pRGB0.data[2]), unpack2_(pRGB0.data[2])));
        f.z = dot3_(cU, make_float3(unpack0_(pRGB0.data[4]), unpack1_(pRGB0.data[4]), unpack2_(pRGB0.data[4])));
        f.w = dot3_(cU, make_float3(unpack0_(pRGB0.data[6]), unpack1_(pRGB0.data[6]), unpack2_(pRGB0.data[6])));
        pU0.x = pack_(f + (float4)(128));
        f.x = dot3_(cU, make_float3(unpack0_(pRGB0.data[1]), unpack1_(pRGB0.data[1]), unpack2_(pRGB0.data[1])));
        f.y = dot3_(cU, make_float3(unpack0_(pRGB0.data[3]), unpack1_(pRGB0.data[3]), unpack2_(pRGB0.data[3])));
        f.z = dot3_(cU, make_float3(unpack0_(pRGB0.data[5]), unpack1_(pRGB0.data[5]), unpack2_(pRGB0.data[5])));
        f.w = dot3_(cU, make_float3(unpack0_(pRGB0.data[7]), unpack1_(pRGB0.data[7]), unpack2_(pRGB0.data[7])));
        pU0.y = pack_(f + (float4)(128));
        f.x = dot3_(cU, make_float3(unpack0_(pRGB1.data[0]), unpack1_(pRGB1.data[0]), unpack2_(pRGB1.data[0])));
        f.y = dot3_(cU, make_float3(unpack0_(pRGB1.data[2]), unpack1_(pRGB1.data[2]), unpack2_(pRGB1.data[2])));
        f.z = dot3_(cU, make_float3(unpack0_(pRGB1.data[4]), unpack1_(pRGB1.data[4]), unpack2_(pRGB1.data[4])));
        f.w = dot3_(cU, make_float3(unpack0_(pRGB1.data[6]), unpack1_(pRGB1.data[6]), unpack2_(pRGB1.data[6])));
        pU1.x = pack_(f + (float4)(128));
        f.x = dot3_(cU, make_float3(unpack0_(pRGB1.data[1]), unpack1_(pRGB1.data[1]), unpack2_(pRGB1.data[1])));
        f.y = dot3_(cU, make_float3(unpack0_(pRGB1.data[3]), unpack1_(pRGB1.data[3]), unpack2_(pRGB1.data[3])));
        f.z = dot3_(cU, make_float3(unpack0_(pRGB1.data[5]), unpack1_(pRGB1.data[5]), unpack2_(pRGB1.data[5])));
        f.w = dot3_(cU, make_float3(unpack0_(pRGB1.data[7]), unpack1_(pRGB1.data[7]), unpack2_(pRGB1.data[7])));
        pU1.y = pack_(f + (float4)(128));
        pU0.x = lerp_(pU0.x, pU0.y, 0x01010101u);
        pU1.x = lerp_(pU1.x, pU1.y, 0x01010101u);
        pU0.x = lerp_(pU1.x, pU1.x, 0x01010101u);

        float3 cV = make_float3(0.5f, -0.4542f, -0.0458f);
        uint2 pV0, pV1;
        f.x = dot3_(cV, make_float3(unpack0_(pRGB0.data[0]), unpack1_(pRGB0.data[0]), unpack2_(pRGB0.data[0])));
        f.y = dot3_(cV, make_float3(unpack0_(pRGB0.data[2]), unpack1_(pRGB0.data[2]), unpack2_(pRGB0.data[2])));
        f.z = dot3_(cV, make_float3(unpack0_(pRGB0.data[4]), unpack1_(pRGB0.data[4]), unpack2_(pRGB0.data[4])));
        f.w = dot3_(cV, make_float3(unpack0_(pRGB0.data[6]), unpack1_(pRGB0.data[6]), unpack2_(pRGB0.data[6])));
        pV0.x = pack_(f + (float4)(128));
        f.x = dot3_(cV, make_float3(unpack0_(pRGB0.data[1]), unpack1_(pRGB0.data[1]), unpack2_(pRGB0.data[1])));
        f.y = dot3_(cV, make_float3(unpack0_(pRGB0.data[3]), unpack1_(pRGB0.data[3]), unpack2_(pRGB0.data[3])));
        f.z = dot3_(cV, make_float3(unpack0_(pRGB0.data[5]), unpack1_(pRGB0.data[5]), unpack2_(pRGB0.data[5])));
        f.w = dot3_(cV, make_float3(unpack0_(pRGB0.data[7]), unpack1_(pRGB0.data[7]), unpack2_(pRGB0.data[7])));
        pV0.y = pack_(f + (float4)(128));
        f.x = dot3_(cV, make_float3(unpack0_(pRGB1.data[0]), unpack1_(pRGB1.data[0]), unpack2_(pRGB1.data[0])));
        f.y = dot3_(cV, make_float3(unpack0_(pRGB1.data[2]), unpack1_(pRGB1.data[2]), unpack2_(pRGB1.data[2])));
        f.z = dot3_(cV, make_float3(unpack0_(pRGB1.data[4]), unpack1_(pRGB1.data[4]), unpack2_(pRGB1.data[4])));
        f.w = dot3_(cV, make_float3(unpack0_(pRGB1.data[6]), unpack1_(pRGB1.data[6]), unpack2_(pRGB1.data[6])));
        pV1.x = pack_(f + (float4)(128));
        f.x = dot3_(cV, make_float3(unpack0_(pRGB1.data[1]), unpack1_(pRGB1.data[1]), unpack2_(pRGB1.data[1])));
        f.y = dot3_(cV, make_float3(unpack0_(pRGB1.data[3]), unpack1_(pRGB1.data[3]), unpack2_(pRGB1.data[3])));
        f.z = dot3_(cV, make_float3(unpack0_(pRGB1.data[5]), unpack1_(pRGB1.data[5]), unpack2_(pRGB1.data[5])));
        f.w = dot3_(cV, make_float3(unpack0_(pRGB1.data[7]), unpack1_(pRGB1.data[7]), unpack2_(pRGB1.data[7])));
        pV1.y = pack_(f + (float4)(128));
        pV0.x = lerp_(pV0.x, pV0.y, 0x01010101u);
        pV1.x = lerp_(pV1.x, pV1.y, 0x01010101u);
        pV0.x = lerp_(pV1.x, pV1.x, 0x01010101u);

        *((uint2 *)(&pDstYImage[dstY0Idx])) = pY0;
        *((uint2 *)(&pDstYImage[dstY1Idx])) = pY1;
        *((uint *)(&pDstUImage[dstUIdx])) = pU0.x;
        *((uint *)(&pDstVImage[dstVIdx])) = pV0.x;
    }
}
int HipExec_ColorConvert_IYUV_RGBX(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstYImage, vx_uint32 dstYImageStrideInBytes,
    vx_uint8 *pHipDstUImage, vx_uint32 dstUImageStrideInBytes,
    vx_uint8 *pHipDstVImage, vx_uint32 dstVImageStrideInBytes,
    const vx_uint8 *pHipSrcImage, vx_uint32 srcImageStrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 4;
    int globalThreads_x = dstWidth;
    int globalThreads_y = dstHeight;

    vx_uint32 dstWidthComp = (dstWidth + 7) / 8;
    vx_uint32 dstHeightComp = (dstHeight + 1) / 2;
    vx_uint32 srcImageStrideInBytesComp = srcImageStrideInBytes * 2;
    vx_uint32 dstYImageStrideInBytesComp = dstYImageStrideInBytes * 2;

    hipLaunchKernelGGL(Hip_ColorConvert_IYUV_RGBX, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstYImage, dstYImageStrideInBytes, (uchar *)pHipDstUImage, dstUImageStrideInBytes, (uchar *)pHipDstVImage, dstVImageStrideInBytes,
                        (const uchar *)pHipSrcImage, srcImageStrideInBytes,
                        dstWidthComp, dstHeightComp, srcImageStrideInBytesComp, dstYImageStrideInBytesComp);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_FormatConvert_IYUV_UYVY(uint dstWidth, uint dstHeight,
    uchar *pDstYImage, uint dstYImageStrideInBytes, uchar *pDstUImage, uint dstUImageStrideInBytes, uchar *pDstVImage, uint dstVImageStrideInBytes,
    const uchar *pSrcImage, uint srcImageStrideInBytes,
    uint dstWidthComp, uint dstHeightComp, uint srcImageStrideInBytesComp, uint dstYImageStrideInBytesComp) {

    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if ((x < dstWidthComp) && (y < dstHeightComp)) {
        uint L0Idx = y * srcImageStrideInBytesComp + (x << 4);
        uint L1Idx = L0Idx + srcImageStrideInBytes;
        uint4 L0 = *((uint4 *)(&pSrcImage[L0Idx]));
        uint4 L1 = *((uint4 *)(&pSrcImage[L1Idx]));

        uint dstY0Idx = y * dstYImageStrideInBytesComp + (x << 3);
        uint dstY1Idx = dstY0Idx + dstYImageStrideInBytes;
        uint dstUIdx = y * dstUImageStrideInBytes + (x << 2);
        uint dstVIdx = y * dstVImageStrideInBytes + (x << 2);

        uint2 pY0, pY1;
        uint pU, pV;
        pY0.x = pack_(make_float4(unpack1_(L0.x), unpack3_(L0.x), unpack1_(L0.y), unpack3_(L0.y)));
        pY0.y = pack_(make_float4(unpack1_(L0.z), unpack3_(L0.z), unpack1_(L0.w), unpack3_(L0.w)));
        pY1.x = pack_(make_float4(unpack1_(L1.x), unpack3_(L1.x), unpack1_(L1.y), unpack3_(L1.y)));
        pY1.y = pack_(make_float4(unpack1_(L1.z), unpack3_(L1.z), unpack1_(L1.w), unpack3_(L1.w)));
        L0.x  = lerp_(L0.x, L1.x, 0x01010101);
        L0.y  = lerp_(L0.y, L1.y, 0x01010101);
        L0.z  = lerp_(L0.z, L1.z, 0x01010101);
        L0.w  = lerp_(L0.w, L1.w, 0x01010101);
        pU = pack_(make_float4(unpack0_(L0.x), unpack0_(L0.y), unpack0_(L0.z), unpack0_(L0.w)));
        pV = pack_(make_float4(unpack2_(L0.x), unpack2_(L0.y), unpack2_(L0.z), unpack2_(L0.w)));

        *((uint2 *)(&pDstYImage[dstY0Idx])) = pY0;
        *((uint2 *)(&pDstYImage[dstY1Idx])) = pY1;
        *((uint *)(&pDstUImage[dstUIdx])) = pU;
        *((uint *)(&pDstVImage[dstVIdx])) = pV;
    }
}
int HipExec_FormatConvert_IYUV_UYVY(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstYImage, vx_uint32 dstYImageStrideInBytes,
    vx_uint8 *pHipDstUImage, vx_uint32 dstUImageStrideInBytes,
    vx_uint8 *pHipDstVImage, vx_uint32 dstVImageStrideInBytes,
    const vx_uint8 *pHipSrcImage, vx_uint32 srcImageStrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 4;
    int globalThreads_x = dstWidth;
    int globalThreads_y = dstHeight;

    vx_uint32 dstWidthComp = (dstWidth + 7) / 8;
    vx_uint32 dstHeightComp = (dstHeight + 1) / 2;
    vx_uint32 dstYImageStrideInBytesComp = dstYImageStrideInBytes * 2;
    vx_uint32 srcImageStrideInBytesComp = srcImageStrideInBytes * 2;

    hipLaunchKernelGGL(Hip_FormatConvert_IYUV_UYVY, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstYImage, dstYImageStrideInBytes, (uchar *)pHipDstUImage, dstUImageStrideInBytes, (uchar *)pHipDstVImage, dstVImageStrideInBytes,
                        (const uchar *)pHipSrcImage, srcImageStrideInBytes,
                        dstWidthComp, dstHeightComp, srcImageStrideInBytesComp, dstYImageStrideInBytesComp);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_FormatConvert_IYUV_YUYV(uint dstWidth, uint dstHeight,
    uchar *pDstYImage, uint dstYImageStrideInBytes, uchar *pDstUImage, uint dstUImageStrideInBytes, uchar *pDstVImage, uint dstVImageStrideInBytes,
    const uchar *pSrcImage, uint srcImageStrideInBytes,
    uint dstWidthComp, uint dstHeightComp, uint srcImageStrideInBytesComp, uint dstYImageStrideInBytesComp) {

    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if ((x < dstWidthComp) && (y < dstHeightComp)) {
        uint L0Idx = y * srcImageStrideInBytesComp + (x << 4);
        uint L1Idx = L0Idx + srcImageStrideInBytes;
        uint4 L0 = *((uint4 *)(&pSrcImage[L0Idx]));
        uint4 L1 = *((uint4 *)(&pSrcImage[L1Idx]));

        uint dstY0Idx = y * dstYImageStrideInBytesComp + (x << 3);
        uint dstY1Idx = dstY0Idx + dstYImageStrideInBytes;
        uint dstUIdx = y * dstUImageStrideInBytes + (x << 2);
        uint dstVIdx = y * dstVImageStrideInBytes + (x << 2);

        uint2 pY0, pY1;
        uint pU, pV;
        pY0.x = pack_(make_float4(unpack0_(L0.x), unpack2_(L0.x), unpack0_(L0.y), unpack2_(L0.y)));
        pY0.y = pack_(make_float4(unpack0_(L0.z), unpack2_(L0.z), unpack0_(L0.w), unpack2_(L0.w)));
        pY1.x = pack_(make_float4(unpack0_(L1.x), unpack2_(L1.x), unpack0_(L1.y), unpack2_(L1.y)));
        pY1.y = pack_(make_float4(unpack0_(L1.z), unpack2_(L1.z), unpack0_(L1.w), unpack2_(L1.w)));
        L0.x  = lerp_(L0.x, L1.x, 0x01010101);
        L0.y  = lerp_(L0.y, L1.y, 0x01010101);
        L0.z  = lerp_(L0.z, L1.z, 0x01010101);
        L0.w  = lerp_(L0.w, L1.w, 0x01010101);
        pU = pack_(make_float4(unpack1_(L0.x), unpack1_(L0.y), unpack1_(L0.z), unpack1_(L0.w)));
        pV = pack_(make_float4(unpack3_(L0.x), unpack3_(L0.y), unpack3_(L0.z), unpack3_(L0.w)));

        *((uint2 *)(&pDstYImage[dstY0Idx])) = pY0;
        *((uint2 *)(&pDstYImage[dstY1Idx])) = pY1;
        *((uint *)(&pDstUImage[dstUIdx])) = pU;
        *((uint *)(&pDstVImage[dstVIdx])) = pV;
    }
}
int HipExec_FormatConvert_IYUV_YUYV(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstYImage, vx_uint32 dstYImageStrideInBytes,
    vx_uint8 *pHipDstUImage, vx_uint32 dstUImageStrideInBytes,
    vx_uint8 *pHipDstVImage, vx_uint32 dstVImageStrideInBytes,
    const vx_uint8 *pHipSrcImage, vx_uint32 srcImageStrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 4;
    int globalThreads_x = dstWidth;
    int globalThreads_y = dstHeight;

    vx_uint32 dstWidthComp = (dstWidth + 7) / 8;
    vx_uint32 dstHeightComp = (dstHeight + 1) / 2;
    vx_uint32 dstYImageStrideInBytesComp = dstYImageStrideInBytes * 2;
    vx_uint32 srcImageStrideInBytesComp = srcImageStrideInBytes * 2;

    hipLaunchKernelGGL(Hip_FormatConvert_IYUV_YUYV, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstYImage, dstYImageStrideInBytes, (uchar *)pHipDstUImage, dstUImageStrideInBytes, (uchar *)pHipDstVImage, dstVImageStrideInBytes,
                        (const uchar *)pHipSrcImage, srcImageStrideInBytes,
                        dstWidthComp, dstHeightComp, srcImageStrideInBytesComp, dstYImageStrideInBytesComp);

    return VX_SUCCESS;
}





// Group 6 - Destination NV12, Source Packed

__global__ void __attribute__((visibility("default")))
Hip_ColorConvert_NV12_RGB(uint dstWidth, uint dstHeight,
    uchar *pDstImageLuma, uint dstImageLumaStrideInBytes, uchar *pDstImageChroma, uint dstImageChromaStrideInBytes,
    const uchar *pSrcImage, uint srcImageStrideInBytes,
    uint dstWidthComp, uint dstHeightComp, uint srcImageStrideInBytesComp, uint dstImageLumaStrideInBytesComp) {

    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if ((x < dstWidthComp) && (y < dstHeightComp)) {
        uint srcIdx0 = y * srcImageStrideInBytesComp + (x * 24);
        uint srcIdx1 = srcIdx0 + srcImageStrideInBytes;
        d_uint6 pRGB0 = *((d_uint6 *)(&pSrcImage[srcIdx0]));
        d_uint6 pRGB1 = *((d_uint6 *)(&pSrcImage[srcIdx1]));

        uint dstY0Idx  = y * dstImageLumaStrideInBytesComp + (x << 3);
        uint dstY1Idx  = dstY0Idx + dstImageLumaStrideInBytes;
        uint dstUVIdx  = y * dstImageChromaStrideInBytes + (x << 3);

        float4 f;

        float4 cY = make_float4(0.2126f, 0.7152f, 0.0722f, 0.0f);
        float3 cY3 = make_float3(0.2126f, 0.7152f, 0.0722f);
        uint2 pY0, pY1;
        f.x = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB0.data[0]), unpack1_(pRGB0.data[0]), unpack2_(pRGB0.data[0])));
        f.y = cY.w + dot3_(cY3, make_float3(unpack3_(pRGB0.data[0]), unpack0_(pRGB0.data[1]), unpack1_(pRGB0.data[1])));
        f.z = cY.w + dot3_(cY3, make_float3(unpack2_(pRGB0.data[1]), unpack3_(pRGB0.data[1]), unpack0_(pRGB0.data[2])));
        f.w = cY.w + dot3_(cY3, make_float3(unpack1_(pRGB0.data[2]), unpack2_(pRGB0.data[2]), unpack3_(pRGB0.data[2])));
        pY0.x = pack_(f);
        f.x = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB0.data[3]), unpack1_(pRGB0.data[3]), unpack2_(pRGB0.data[3])));
        f.y = cY.w + dot3_(cY3, make_float3(unpack3_(pRGB0.data[3]), unpack0_(pRGB0.data[4]), unpack1_(pRGB0.data[4])));
        f.z = cY.w + dot3_(cY3, make_float3(unpack2_(pRGB0.data[4]), unpack3_(pRGB0.data[4]), unpack0_(pRGB0.data[5])));
        f.w = cY.w + dot3_(cY3, make_float3(unpack1_(pRGB0.data[5]), unpack2_(pRGB0.data[5]), unpack3_(pRGB0.data[5])));
        pY0.y = pack_(f);
        f.x = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB1.data[0]), unpack1_(pRGB1.data[0]), unpack2_(pRGB1.data[0])));
        f.y = cY.w + dot3_(cY3, make_float3(unpack3_(pRGB1.data[0]), unpack0_(pRGB1.data[1]), unpack1_(pRGB1.data[1])));
        f.z = cY.w + dot3_(cY3, make_float3(unpack2_(pRGB1.data[1]), unpack3_(pRGB1.data[1]), unpack0_(pRGB1.data[2])));
        f.w = cY.w + dot3_(cY3, make_float3(unpack1_(pRGB1.data[2]), unpack2_(pRGB1.data[2]), unpack3_(pRGB1.data[2])));
        pY1.x = pack_(f);
        f.x = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB1.data[3]), unpack1_(pRGB1.data[3]), unpack2_(pRGB1.data[3])));
        f.y = cY.w + dot3_(cY3, make_float3(unpack3_(pRGB1.data[3]), unpack0_(pRGB1.data[4]), unpack1_(pRGB1.data[4])));
        f.z = cY.w + dot3_(cY3, make_float3(unpack2_(pRGB1.data[4]), unpack3_(pRGB1.data[4]), unpack0_(pRGB1.data[5])));
        f.w = cY.w + dot3_(cY3, make_float3(unpack1_(pRGB1.data[5]), unpack2_(pRGB1.data[5]), unpack3_(pRGB1.data[5])));
        pY1.y = pack_(f);

        float3 cU = make_float3(-0.1146f, -0.3854f, 0.5f);
        uint2 pU0, pU1;
        f.x = dot3_(cU, make_float3(unpack0_(pRGB0.data[0]), unpack1_(pRGB0.data[0]), unpack2_(pRGB0.data[0])));
        f.y = dot3_(cU, make_float3(unpack2_(pRGB0.data[1]), unpack3_(pRGB0.data[1]), unpack0_(pRGB0.data[2])));
        f.z = dot3_(cU, make_float3(unpack0_(pRGB0.data[3]), unpack1_(pRGB0.data[3]), unpack2_(pRGB0.data[3])));
        f.w = dot3_(cU, make_float3(unpack2_(pRGB0.data[4]), unpack3_(pRGB0.data[4]), unpack0_(pRGB0.data[5])));
        pU0.x = pack_(f + (float4)(128));
        f.x = dot3_(cU, make_float3(unpack3_(pRGB0.data[0]), unpack0_(pRGB0.data[1]), unpack1_(pRGB0.data[1])));
        f.y = dot3_(cU, make_float3(unpack1_(pRGB0.data[2]), unpack2_(pRGB0.data[2]), unpack3_(pRGB0.data[2])));
        f.z = dot3_(cU, make_float3(unpack3_(pRGB0.data[3]), unpack0_(pRGB0.data[4]), unpack1_(pRGB0.data[4])));
        f.w = dot3_(cU, make_float3(unpack1_(pRGB0.data[5]), unpack2_(pRGB0.data[5]), unpack3_(pRGB0.data[5])));
        pU0.y = pack_(f + (float4)(128));
        f.x = dot3_(cU, make_float3(unpack0_(pRGB1.data[0]), unpack1_(pRGB1.data[0]), unpack2_(pRGB1.data[0])));
        f.y = dot3_(cU, make_float3(unpack2_(pRGB1.data[1]), unpack3_(pRGB1.data[1]), unpack0_(pRGB1.data[2])));
        f.z = dot3_(cU, make_float3(unpack0_(pRGB1.data[3]), unpack1_(pRGB1.data[3]), unpack2_(pRGB1.data[3])));
        f.w = dot3_(cU, make_float3(unpack2_(pRGB1.data[4]), unpack3_(pRGB1.data[4]), unpack0_(pRGB1.data[5])));
        pU1.x = pack_(f + (float4)(128));
        f.x = dot3_(cU, make_float3(unpack3_(pRGB1.data[0]), unpack0_(pRGB1.data[1]), unpack1_(pRGB1.data[1])));
        f.y = dot3_(cU, make_float3(unpack1_(pRGB1.data[2]), unpack2_(pRGB1.data[2]), unpack3_(pRGB1.data[2])));
        f.z = dot3_(cU, make_float3(unpack3_(pRGB1.data[3]), unpack0_(pRGB1.data[4]), unpack1_(pRGB1.data[4])));
        f.w = dot3_(cU, make_float3(unpack1_(pRGB1.data[5]), unpack2_(pRGB1.data[5]), unpack3_(pRGB1.data[5])));
        pU1.y = pack_(f + (float4)(128));
        pU0.x = lerp_(pU0.x, pU0.y, 0x01010101u);
        pU1.x = lerp_(pU1.x, pU1.y, 0x01010101u);
        pU0.x = lerp_(pU0.x, pU1.x, 0x01010101u);

        float3 cV = make_float3(0.5f, -0.4542f, -0.0458f);
        uint2 pV0, pV1;
        f.x = dot3_(cV, make_float3(unpack0_(pRGB0.data[0]), unpack1_(pRGB0.data[0]), unpack2_(pRGB0.data[0])));
        f.y = dot3_(cV, make_float3(unpack2_(pRGB0.data[1]), unpack3_(pRGB0.data[1]), unpack0_(pRGB0.data[2])));
        f.z = dot3_(cV, make_float3(unpack0_(pRGB0.data[3]), unpack1_(pRGB0.data[3]), unpack2_(pRGB0.data[3])));
        f.w = dot3_(cV, make_float3(unpack2_(pRGB0.data[4]), unpack3_(pRGB0.data[4]), unpack0_(pRGB0.data[5])));
        pV0.x = pack_(f + (float4)(128));
        f.x = dot3_(cV, make_float3(unpack3_(pRGB0.data[0]), unpack0_(pRGB0.data[1]), unpack1_(pRGB0.data[1])));
        f.y = dot3_(cV, make_float3(unpack1_(pRGB0.data[2]), unpack2_(pRGB0.data[2]), unpack3_(pRGB0.data[2])));
        f.z = dot3_(cV, make_float3(unpack3_(pRGB0.data[3]), unpack0_(pRGB0.data[4]), unpack1_(pRGB0.data[4])));
        f.w = dot3_(cV, make_float3(unpack1_(pRGB0.data[5]), unpack2_(pRGB0.data[5]), unpack3_(pRGB0.data[5])));
        pV0.y = pack_(f + (float4)(128));
        f.x = dot3_(cV, make_float3(unpack0_(pRGB1.data[0]), unpack1_(pRGB1.data[0]), unpack2_(pRGB1.data[0])));
        f.y = dot3_(cV, make_float3(unpack2_(pRGB1.data[1]), unpack3_(pRGB1.data[1]), unpack0_(pRGB1.data[2])));
        f.z = dot3_(cV, make_float3(unpack0_(pRGB1.data[3]), unpack1_(pRGB1.data[3]), unpack2_(pRGB1.data[3])));
        f.w = dot3_(cV, make_float3(unpack2_(pRGB1.data[4]), unpack3_(pRGB1.data[4]), unpack0_(pRGB1.data[5])));
        pV1.x = pack_(f + (float4)(128));
        f.x = dot3_(cV, make_float3(unpack3_(pRGB1.data[0]), unpack0_(pRGB1.data[1]), unpack1_(pRGB1.data[1])));
        f.y = dot3_(cV, make_float3(unpack1_(pRGB1.data[2]), unpack2_(pRGB1.data[2]), unpack3_(pRGB1.data[2])));
        f.z = dot3_(cV, make_float3(unpack3_(pRGB1.data[3]), unpack0_(pRGB1.data[4]), unpack1_(pRGB1.data[4])));
        f.w = dot3_(cV, make_float3(unpack1_(pRGB1.data[5]), unpack2_(pRGB1.data[5]), unpack3_(pRGB1.data[5])));
        pV1.y = pack_(f + (float4)(128));
        pV0.x = lerp_(pV0.x, pV0.y, 0x01010101u);
        pV1.x = lerp_(pV1.x, pV1.y, 0x01010101u);
        pV0.x = lerp_(pV0.x, pV1.x, 0x01010101u);

        uint2 pUV;
        f.x = unpack0_(pU0.x);
        f.y = unpack0_(pV0.x);
        f.z = unpack1_(pU0.x);
        f.w = unpack1_(pV0.x);
        pUV.x = pack_(f);
        f.x = unpack2_(pU0.x);
        f.y = unpack2_(pV0.x);
        f.z = unpack3_(pU0.x);
        f.w = unpack3_(pV0.x);
        pUV.y = pack_(f);

        *((uint2 *)(&pDstImageLuma[dstY0Idx])) = pY0;
        *((uint2 *)(&pDstImageLuma[dstY1Idx])) = pY1;
        *((uint2 *)(&pDstImageChroma[dstUVIdx])) = pUV;
    }
}
int HipExec_ColorConvert_NV12_RGB(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImageLuma, vx_uint32 dstImageLumaStrideInBytes,
    vx_uint8 *pHipDstImageChroma, vx_uint32 dstImageChromaStrideInBytes,
    const vx_uint8 *pHipSrcImage, vx_uint32 srcImageStrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 4;
    int globalThreads_x = dstWidth;
    int globalThreads_y = dstHeight;

    vx_uint32 dstWidthComp = (dstWidth + 7) / 8;
    vx_uint32 dstHeightComp = (dstHeight + 1) / 2;
    vx_uint32 srcImageStrideInBytesComp = srcImageStrideInBytes * 2;
    vx_uint32 dstImageLumaStrideInBytesComp = dstImageLumaStrideInBytes * 2;

    hipLaunchKernelGGL(Hip_ColorConvert_NV12_RGB, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImageLuma, dstImageLumaStrideInBytes, (uchar *)pHipDstImageChroma, dstImageChromaStrideInBytes,
                        (const uchar *)pHipSrcImage, srcImageStrideInBytes,
                        dstWidthComp, dstHeightComp, srcImageStrideInBytesComp, dstImageLumaStrideInBytesComp);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ColorConvert_NV12_RGBX(uint dstWidth, uint dstHeight,
    uchar *pDstImageLuma, uint dstImageLumaStrideInBytes, uchar *pDstImageChroma, uint dstImageChromaStrideInBytes,
    const uchar *pSrcImage, uint srcImageStrideInBytes,
    uint dstWidthComp, uint dstHeightComp, uint srcImageStrideInBytesComp, uint dstImageLumaStrideInBytesComp) {

    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if ((x < dstWidthComp) && (y < dstHeightComp)) {
        uint srcIdx0 = y * srcImageStrideInBytesComp + (x << 5);
        uint srcIdx1 = srcIdx0 + srcImageStrideInBytes;
        d_uint8 pRGB0 = *((d_uint8 *)(&pSrcImage[srcIdx0]));
        d_uint8 pRGB1 = *((d_uint8 *)(&pSrcImage[srcIdx1]));

        uint dstY0Idx  = y * dstImageLumaStrideInBytesComp + (x << 3);
        uint dstY1Idx  = dstY0Idx + dstImageLumaStrideInBytes;
        uint dstUVIdx  = y * dstImageChromaStrideInBytes + (x << 3);

        float4 f;

        float4 cY = make_float4(0.2126f, 0.7152f, 0.0722f, 0.0f);
        float3 cY3 = make_float3(0.2126f, 0.7152f, 0.0722f);
        uint2 pY0, pY1;
        f.x = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB0.data[0]), unpack1_(pRGB0.data[0]), unpack2_(pRGB0.data[0])));
        f.y = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB0.data[1]), unpack1_(pRGB0.data[1]), unpack2_(pRGB0.data[1])));
        f.z = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB0.data[2]), unpack1_(pRGB0.data[2]), unpack2_(pRGB0.data[2])));
        f.w = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB0.data[3]), unpack1_(pRGB0.data[3]), unpack2_(pRGB0.data[3])));
        pY0.x = pack_(f);
        f.x = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB0.data[4]), unpack1_(pRGB0.data[4]), unpack2_(pRGB0.data[4])));
        f.y = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB0.data[5]), unpack1_(pRGB0.data[5]), unpack2_(pRGB0.data[5])));
        f.z = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB0.data[6]), unpack1_(pRGB0.data[6]), unpack2_(pRGB0.data[6])));
        f.w = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB0.data[7]), unpack1_(pRGB0.data[7]), unpack2_(pRGB0.data[7])));
        pY0.y = pack_(f);
        f.x = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB1.data[0]), unpack1_(pRGB1.data[0]), unpack2_(pRGB1.data[0])));
        f.y = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB1.data[1]), unpack1_(pRGB1.data[1]), unpack2_(pRGB1.data[1])));
        f.z = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB1.data[2]), unpack1_(pRGB1.data[2]), unpack2_(pRGB1.data[2])));
        f.w = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB1.data[3]), unpack1_(pRGB1.data[3]), unpack2_(pRGB1.data[3])));
        pY1.x = pack_(f);
        f.x = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB1.data[4]), unpack1_(pRGB1.data[4]), unpack2_(pRGB1.data[4])));
        f.y = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB1.data[5]), unpack1_(pRGB1.data[5]), unpack2_(pRGB1.data[5])));
        f.z = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB1.data[6]), unpack1_(pRGB1.data[6]), unpack2_(pRGB1.data[6])));
        f.w = cY.w + dot3_(cY3, make_float3(unpack0_(pRGB1.data[7]), unpack1_(pRGB1.data[7]), unpack2_(pRGB1.data[7])));
        pY1.y = pack_(f);

        float3 cU = make_float3(-0.1146f, -0.3854f, 0.5f);
        uint2 pU0, pU1;
        f.x = dot3_(cU, make_float3(unpack0_(pRGB0.data[0]), unpack1_(pRGB0.data[0]), unpack2_(pRGB0.data[0])));
        f.y = dot3_(cU, make_float3(unpack0_(pRGB0.data[2]), unpack1_(pRGB0.data[2]), unpack2_(pRGB0.data[2])));
        f.z = dot3_(cU, make_float3(unpack0_(pRGB0.data[4]), unpack1_(pRGB0.data[4]), unpack2_(pRGB0.data[4])));
        f.w = dot3_(cU, make_float3(unpack0_(pRGB0.data[6]), unpack1_(pRGB0.data[6]), unpack2_(pRGB0.data[6])));
        pU0.x = pack_(f + (float4)(128));
        f.x = dot3_(cU, make_float3(unpack0_(pRGB0.data[1]), unpack1_(pRGB0.data[1]), unpack2_(pRGB0.data[1])));
        f.y = dot3_(cU, make_float3(unpack0_(pRGB0.data[3]), unpack1_(pRGB0.data[3]), unpack2_(pRGB0.data[3])));
        f.z = dot3_(cU, make_float3(unpack0_(pRGB0.data[5]), unpack1_(pRGB0.data[5]), unpack2_(pRGB0.data[5])));
        f.w = dot3_(cU, make_float3(unpack0_(pRGB0.data[7]), unpack1_(pRGB0.data[7]), unpack2_(pRGB0.data[7])));
        pU0.y = pack_(f + (float4)(128));
        f.x = dot3_(cU, make_float3(unpack0_(pRGB1.data[0]), unpack1_(pRGB1.data[0]), unpack2_(pRGB1.data[0])));
        f.y = dot3_(cU, make_float3(unpack0_(pRGB1.data[2]), unpack1_(pRGB1.data[2]), unpack2_(pRGB1.data[2])));
        f.z = dot3_(cU, make_float3(unpack0_(pRGB1.data[4]), unpack1_(pRGB1.data[4]), unpack2_(pRGB1.data[4])));
        f.w = dot3_(cU, make_float3(unpack0_(pRGB1.data[6]), unpack1_(pRGB1.data[6]), unpack2_(pRGB1.data[6])));
        pU1.x = pack_(f + (float4)(128));
        f.x = dot3_(cU, make_float3(unpack0_(pRGB1.data[1]), unpack1_(pRGB1.data[1]), unpack2_(pRGB1.data[1])));
        f.y = dot3_(cU, make_float3(unpack0_(pRGB1.data[3]), unpack1_(pRGB1.data[3]), unpack2_(pRGB1.data[3])));
        f.z = dot3_(cU, make_float3(unpack0_(pRGB1.data[5]), unpack1_(pRGB1.data[5]), unpack2_(pRGB1.data[5])));
        f.w = dot3_(cU, make_float3(unpack0_(pRGB1.data[7]), unpack1_(pRGB1.data[7]), unpack2_(pRGB1.data[7])));
        pU1.y = pack_(f + (float4)(128));
        pU0.x = lerp_(pU0.x, pU0.y, 0x01010101u);
        pU1.x = lerp_(pU1.x, pU1.y, 0x01010101u);
        pU0.x = lerp_(pU1.x, pU1.x, 0x01010101u);

        float3 cV = make_float3(0.5f, -0.4542f, -0.0458f);
        uint2 pV0, pV1;
        f.x = dot3_(cV, make_float3(unpack0_(pRGB0.data[0]), unpack1_(pRGB0.data[0]), unpack2_(pRGB0.data[0])));
        f.y = dot3_(cV, make_float3(unpack0_(pRGB0.data[2]), unpack1_(pRGB0.data[2]), unpack2_(pRGB0.data[2])));
        f.z = dot3_(cV, make_float3(unpack0_(pRGB0.data[4]), unpack1_(pRGB0.data[4]), unpack2_(pRGB0.data[4])));
        f.w = dot3_(cV, make_float3(unpack0_(pRGB0.data[6]), unpack1_(pRGB0.data[6]), unpack2_(pRGB0.data[6])));
        pV0.x = pack_(f + (float4)(128));
        f.x = dot3_(cV, make_float3(unpack0_(pRGB0.data[1]), unpack1_(pRGB0.data[1]), unpack2_(pRGB0.data[1])));
        f.y = dot3_(cV, make_float3(unpack0_(pRGB0.data[3]), unpack1_(pRGB0.data[3]), unpack2_(pRGB0.data[3])));
        f.z = dot3_(cV, make_float3(unpack0_(pRGB0.data[5]), unpack1_(pRGB0.data[5]), unpack2_(pRGB0.data[5])));
        f.w = dot3_(cV, make_float3(unpack0_(pRGB0.data[7]), unpack1_(pRGB0.data[7]), unpack2_(pRGB0.data[7])));
        pV0.y = pack_(f + (float4)(128));
        f.x = dot3_(cV, make_float3(unpack0_(pRGB1.data[0]), unpack1_(pRGB1.data[0]), unpack2_(pRGB1.data[0])));
        f.y = dot3_(cV, make_float3(unpack0_(pRGB1.data[2]), unpack1_(pRGB1.data[2]), unpack2_(pRGB1.data[2])));
        f.z = dot3_(cV, make_float3(unpack0_(pRGB1.data[4]), unpack1_(pRGB1.data[4]), unpack2_(pRGB1.data[4])));
        f.w = dot3_(cV, make_float3(unpack0_(pRGB1.data[6]), unpack1_(pRGB1.data[6]), unpack2_(pRGB1.data[6])));
        pV1.x = pack_(f + (float4)(128));
        f.x = dot3_(cV, make_float3(unpack0_(pRGB1.data[1]), unpack1_(pRGB1.data[1]), unpack2_(pRGB1.data[1])));
        f.y = dot3_(cV, make_float3(unpack0_(pRGB1.data[3]), unpack1_(pRGB1.data[3]), unpack2_(pRGB1.data[3])));
        f.z = dot3_(cV, make_float3(unpack0_(pRGB1.data[5]), unpack1_(pRGB1.data[5]), unpack2_(pRGB1.data[5])));
        f.w = dot3_(cV, make_float3(unpack0_(pRGB1.data[7]), unpack1_(pRGB1.data[7]), unpack2_(pRGB1.data[7])));
        pV1.y = pack_(f + (float4)(128));
        pV0.x = lerp_(pV0.x, pV0.y, 0x01010101u);
        pV1.x = lerp_(pV1.x, pV1.y, 0x01010101u);
        pV0.x = lerp_(pV1.x, pV1.x, 0x01010101u);

        uint2 pUV;
        f.x = unpack0_(pU0.x);
        f.y = unpack0_(pV0.x);
        f.z = unpack1_(pU0.x);
        f.w = unpack1_(pV0.x);
        pUV.x = pack_(f);
        f.x = unpack2_(pU0.x);
        f.y = unpack2_(pV0.x);
        f.z = unpack3_(pU0.x);
        f.w = unpack3_(pV0.x);
        pUV.y = pack_(f);

        *((uint2 *)(&pDstImageLuma[dstY0Idx])) = pY0;
        *((uint2 *)(&pDstImageLuma[dstY1Idx])) = pY1;
        *((uint2 *)(&pDstImageChroma[dstUVIdx])) = pUV;
    }
}
int HipExec_ColorConvert_NV12_RGBX(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImageLuma, vx_uint32 dstImageLumaStrideInBytes,
    vx_uint8 *pHipDstImageChroma, vx_uint32 dstImageChromaStrideInBytes,
    const vx_uint8 *pHipSrcImage, vx_uint32 srcImageStrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 4;
    int globalThreads_x = dstWidth;
    int globalThreads_y = dstHeight;

    vx_uint32 dstWidthComp = (dstWidth + 7) / 8;
    vx_uint32 dstHeightComp = (dstHeight + 1) / 2;
    vx_uint32 srcImageStrideInBytesComp = srcImageStrideInBytes * 2;
    vx_uint32 dstImageLumaStrideInBytesComp = dstImageLumaStrideInBytes * 2;

    hipLaunchKernelGGL(Hip_ColorConvert_NV12_RGBX, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImageLuma, dstImageLumaStrideInBytes, (uchar *)pHipDstImageChroma, dstImageChromaStrideInBytes,
                        (const uchar *)pHipSrcImage, srcImageStrideInBytes,
                        dstWidthComp, dstHeightComp, srcImageStrideInBytesComp, dstImageLumaStrideInBytesComp);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_FormatConvert_NV12_UYVY(uint dstWidth, uint dstHeight,
    uchar *pDstImageLuma, uint dstImageLumaStrideInBytes, uchar *pDstImageChroma, uint dstImageChromaStrideInBytes,
    const uchar *pSrcImage, uint srcImageStrideInBytes,
    uint dstWidthComp, uint dstHeightComp, uint srcImageStrideInBytesComp, uint dstImageLumaStrideInBytesComp) {

    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if ((x < dstWidthComp) && (y < dstHeightComp)) {
        uint L0Idx = y * srcImageStrideInBytesComp + (x << 4);
        uint L1Idx = L0Idx + srcImageStrideInBytes;
        uint4 L0 = *((uint4 *)(&pSrcImage[L0Idx]));
        uint4 L1 = *((uint4 *)(&pSrcImage[L1Idx]));

        uint dstY0Idx = y * dstImageLumaStrideInBytesComp + (x << 3);
        uint dstY1Idx = dstY0Idx + dstImageLumaStrideInBytes;
        uint dstUVIdx = y * dstImageChromaStrideInBytes + (x << 3);

        uint2 pY0, pY1, pUV;
        pY0.x = pack_(make_float4(unpack1_(L0.x), unpack3_(L0.x), unpack1_(L0.y), unpack3_(L0.y)));
        pY0.y = pack_(make_float4(unpack1_(L0.z), unpack3_(L0.z), unpack1_(L0.w), unpack3_(L0.w)));
        pY1.x = pack_(make_float4(unpack1_(L1.x), unpack3_(L1.x), unpack1_(L1.y), unpack3_(L1.y)));
        pY1.y = pack_(make_float4(unpack1_(L1.z), unpack3_(L1.z), unpack1_(L1.w), unpack3_(L1.w)));
        L0.x  = lerp_(L0.x, L1.x, 0x01010101);
        L0.y  = lerp_(L0.y, L1.y, 0x01010101);
        L0.z  = lerp_(L0.z, L1.z, 0x01010101);
        L0.w  = lerp_(L0.w, L1.w, 0x01010101);
        pUV.x = pack_(make_float4(unpack0_(L0.x), unpack2_(L0.x), unpack0_(L0.y), unpack2_(L0.y)));
        pUV.y = pack_(make_float4(unpack0_(L0.z), unpack2_(L0.z), unpack0_(L0.w), unpack2_(L0.w)));

        *((uint2 *)(&pDstImageLuma[dstY0Idx])) = pY0;
        *((uint2 *)(&pDstImageLuma[dstY1Idx])) = pY1;
        *((uint2 *)(&pDstImageChroma[dstUVIdx])) = pUV;
    }
}
int HipExec_FormatConvert_NV12_UYVY(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImageLuma, vx_uint32 dstImageLumaStrideInBytes,
    vx_uint8 *pHipDstImageChroma, vx_uint32 dstImageChromaStrideInBytes,
    const vx_uint8 *pHipSrcImage, vx_uint32 srcImageStrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 4;
    int globalThreads_x = dstWidth;
    int globalThreads_y = dstHeight;

    vx_uint32 dstWidthComp = (dstWidth + 7) / 8;
    vx_uint32 dstHeightComp = (dstHeight + 1) / 2;
    vx_uint32 srcImageStrideInBytesComp = srcImageStrideInBytes * 2;
    vx_uint32 dstImageLumaStrideInBytesComp = dstImageLumaStrideInBytes * 2;

    hipLaunchKernelGGL(Hip_FormatConvert_NV12_UYVY, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImageLuma, dstImageLumaStrideInBytes, (uchar *)pHipDstImageChroma, dstImageChromaStrideInBytes,
                        (const uchar *)pHipSrcImage, srcImageStrideInBytes,
                        dstWidthComp, dstHeightComp, srcImageStrideInBytesComp, dstImageLumaStrideInBytesComp);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_FormatConvert_NV12_YUYV(uint dstWidth, uint dstHeight,
    uchar *pDstImageLuma, uint dstImageLumaStrideInBytes, uchar *pDstImageChroma, uint dstImageChromaStrideInBytes,
    const uchar *pSrcImage, uint srcImageStrideInBytes,
    uint dstWidthComp, uint dstHeightComp, uint srcImageStrideInBytesComp, uint dstImageLumaStrideInBytesComp) {

    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    if ((x < dstWidthComp) && (y < dstHeightComp)) {
        uint L0Idx = y * srcImageStrideInBytesComp + (x << 4);
        uint L1Idx = L0Idx + srcImageStrideInBytes;
        uint4 L0 = *((uint4 *)(&pSrcImage[L0Idx]));
        uint4 L1 = *((uint4 *)(&pSrcImage[L1Idx]));

        uint dstY0Idx = y * dstImageLumaStrideInBytesComp + (x << 3);
        uint dstY1Idx = dstY0Idx + dstImageLumaStrideInBytes;
        uint dstUVIdx = y * dstImageChromaStrideInBytes + (x << 3);

        uint2 pY0, pY1, pUV;
        pY0.x = pack_(make_float4(unpack0_(L0.x), unpack2_(L0.x), unpack0_(L0.y), unpack2_(L0.y)));
        pY0.y = pack_(make_float4(unpack0_(L0.z), unpack2_(L0.z), unpack0_(L0.w), unpack2_(L0.w)));
        pY1.x = pack_(make_float4(unpack0_(L1.x), unpack2_(L1.x), unpack0_(L1.y), unpack2_(L1.y)));
        pY1.y = pack_(make_float4(unpack0_(L1.z), unpack2_(L1.z), unpack0_(L1.w), unpack2_(L1.w)));
        L0.x  = lerp_(L0.x, L1.x, 0x01010101);
        L0.y  = lerp_(L0.y, L1.y, 0x01010101);
        L0.z  = lerp_(L0.z, L1.z, 0x01010101);
        L0.w  = lerp_(L0.w, L1.w, 0x01010101);
        pUV.x = pack_(make_float4(unpack1_(L0.x), unpack3_(L0.x), unpack1_(L0.y), unpack3_(L0.y)));
        pUV.y = pack_(make_float4(unpack1_(L0.z), unpack3_(L0.z), unpack1_(L0.w), unpack3_(L0.w)));

        *((uint2 *)(&pDstImageLuma[dstY0Idx])) = pY0;
        *((uint2 *)(&pDstImageLuma[dstY1Idx])) = pY1;
        *((uint2 *)(&pDstImageChroma[dstUVIdx])) = pUV;
    }
}
int HipExec_FormatConvert_NV12_YUYV(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImageLuma, vx_uint32 dstImageLumaStrideInBytes,
    vx_uint8 *pHipDstImageChroma, vx_uint32 dstImageChromaStrideInBytes,
    const vx_uint8 *pHipSrcImage, vx_uint32 srcImageStrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 4;
    int globalThreads_x = dstWidth;
    int globalThreads_y = dstHeight;

    vx_uint32 dstWidthComp = (dstWidth + 7) / 8;
    vx_uint32 dstHeightComp = (dstHeight + 1) / 2;
    vx_uint32 srcImageStrideInBytesComp = srcImageStrideInBytes * 2;
    vx_uint32 dstImageLumaStrideInBytesComp = dstImageLumaStrideInBytes * 2;

    hipLaunchKernelGGL(Hip_FormatConvert_NV12_YUYV, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstImageLuma, dstImageLumaStrideInBytes, (uchar *)pHipDstImageChroma, dstImageChromaStrideInBytes,
                        (const uchar *)pHipSrcImage, srcImageStrideInBytes,
                        dstWidthComp, dstHeightComp, srcImageStrideInBytesComp, dstImageLumaStrideInBytesComp);

    return VX_SUCCESS;
}





// Group 7 - Destination YUV4, Source Packed

__global__ void __attribute__((visibility("default")))
Hip_ColorConvert_YUV4_RGB(uint dstWidth, uint dstHeight,
    uchar *pDstYImage, uint dstYImageStrideInBytes, uchar *pDstUImage, uint dstUImageStrideInBytes, uchar *pDstVImage, uint dstVImageStrideInBytes,
    const uchar *pSrcImage, uint srcImageStrideInBytes) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    uint srcIdx = y * srcImageStrideInBytes + (x * 3);
    d_uint6 pRGB = *((d_uint6 *)(&pSrcImage[srcIdx]));

    uint dstYIdx  = y * dstYImageStrideInBytes + x;
    uint dstUIdx  = y * dstUImageStrideInBytes + x;
    uint dstVIdx  = y * dstVImageStrideInBytes + x;

    float4 f;
    uint2 yChannel, uChannel, vChannel;

    float3 cY = make_float3(0.2126f, 0.7152f, 0.0722f);
    f.x = dot3_(cY, make_float3(unpack0_(pRGB.data[0]), unpack1_(pRGB.data[0]), unpack2_(pRGB.data[0])));
    f.y = dot3_(cY, make_float3(unpack3_(pRGB.data[0]), unpack0_(pRGB.data[1]), unpack1_(pRGB.data[1])));
    f.z = dot3_(cY, make_float3(unpack2_(pRGB.data[1]), unpack3_(pRGB.data[1]), unpack0_(pRGB.data[2])));
    f.w = dot3_(cY, make_float3(unpack1_(pRGB.data[2]), unpack2_(pRGB.data[2]), unpack3_(pRGB.data[2])));
    yChannel.x = pack_(f);
    f.x = dot3_(cY, make_float3(unpack0_(pRGB.data[3]), unpack1_(pRGB.data[3]), unpack2_(pRGB.data[3])));
    f.y = dot3_(cY, make_float3(unpack3_(pRGB.data[3]), unpack0_(pRGB.data[4]), unpack1_(pRGB.data[4])));
    f.z = dot3_(cY, make_float3(unpack2_(pRGB.data[4]), unpack3_(pRGB.data[4]), unpack0_(pRGB.data[5])));
    f.w = dot3_(cY, make_float3(unpack1_(pRGB.data[5]), unpack2_(pRGB.data[5]), unpack3_(pRGB.data[5])));
    yChannel.y = pack_(f);

    float3 cU = make_float3(-0.1146f, -0.3854f, 0.5f);
    f.x = dot3_(cU, make_float3(unpack0_(pRGB.data[0]), unpack1_(pRGB.data[0]), unpack2_(pRGB.data[0]))) + 128.0f;
    f.y = dot3_(cU, make_float3(unpack3_(pRGB.data[0]), unpack0_(pRGB.data[1]), unpack1_(pRGB.data[1]))) + 128.0f;
    f.z = dot3_(cU, make_float3(unpack2_(pRGB.data[1]), unpack3_(pRGB.data[1]), unpack0_(pRGB.data[2]))) + 128.0f;
    f.w = dot3_(cU, make_float3(unpack1_(pRGB.data[2]), unpack2_(pRGB.data[2]), unpack3_(pRGB.data[2]))) + 128.0f;
    uChannel.x = pack_(f);
    f.x = dot3_(cU, make_float3(unpack0_(pRGB.data[3]), unpack1_(pRGB.data[3]), unpack2_(pRGB.data[3]))) + 128.0f;
    f.y = dot3_(cU, make_float3(unpack3_(pRGB.data[3]), unpack0_(pRGB.data[4]), unpack1_(pRGB.data[4]))) + 128.0f;
    f.z = dot3_(cU, make_float3(unpack2_(pRGB.data[4]), unpack3_(pRGB.data[4]), unpack0_(pRGB.data[5]))) + 128.0f;
    f.w = dot3_(cU, make_float3(unpack1_(pRGB.data[5]), unpack2_(pRGB.data[5]), unpack3_(pRGB.data[5]))) + 128.0f;
    uChannel.y = pack_(f);

    float3 cV = make_float3(0.5f, -0.4542f, -0.0458f);
    f.x = dot3_(cV, make_float3(unpack0_(pRGB.data[0]), unpack1_(pRGB.data[0]), unpack2_(pRGB.data[0]))) + 128.0f;
    f.y = dot3_(cV, make_float3(unpack3_(pRGB.data[0]), unpack0_(pRGB.data[1]), unpack1_(pRGB.data[1]))) + 128.0f;
    f.z = dot3_(cV, make_float3(unpack2_(pRGB.data[1]), unpack3_(pRGB.data[1]), unpack0_(pRGB.data[2]))) + 128.0f;
    f.w = dot3_(cV, make_float3(unpack1_(pRGB.data[2]), unpack2_(pRGB.data[2]), unpack3_(pRGB.data[2]))) + 128.0f;
    vChannel.x = pack_(f);
    f.x = dot3_(cV, make_float3(unpack0_(pRGB.data[3]), unpack1_(pRGB.data[3]), unpack2_(pRGB.data[3]))) + 128.0f;
    f.y = dot3_(cV, make_float3(unpack3_(pRGB.data[3]), unpack0_(pRGB.data[4]), unpack1_(pRGB.data[4]))) + 128.0f;
    f.z = dot3_(cV, make_float3(unpack2_(pRGB.data[4]), unpack3_(pRGB.data[4]), unpack0_(pRGB.data[5]))) + 128.0f;
    f.w = dot3_(cV, make_float3(unpack1_(pRGB.data[5]), unpack2_(pRGB.data[5]), unpack3_(pRGB.data[5]))) + 128.0f;
    vChannel.y = pack_(f);

    *((uint2 *)(&pDstYImage[dstYIdx])) = yChannel;
    *((uint2 *)(&pDstUImage[dstUIdx])) = uChannel;
    *((uint2 *)(&pDstVImage[dstVIdx])) = vChannel;
}
int HipExec_ColorConvert_YUV4_RGB(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstYImage, vx_uint32 dstYImageStrideInBytes,
    vx_uint8 *pHipDstUImage, vx_uint32 dstUImageStrideInBytes,
    vx_uint8 *pHipDstVImage, vx_uint32 dstVImageStrideInBytes,
    const vx_uint8 *pHipSrcImage, vx_uint32 srcImageStrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ColorConvert_YUV4_RGB, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstYImage, dstYImageStrideInBytes, (uchar *)pHipDstUImage, dstUImageStrideInBytes, (uchar *)pHipDstVImage, dstVImageStrideInBytes,
                        (const uchar *)pHipSrcImage, srcImageStrideInBytes);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ColorConvert_YUV4_RGBX(uint dstWidth, uint dstHeight,
    uchar *pDstYImage, uint dstYImageStrideInBytes, uchar *pDstUImage, uint dstUImageStrideInBytes, uchar *pDstVImage, uint dstVImageStrideInBytes,
    const uchar *pSrcImage, uint srcImageStrideInBytes) {

    int x = (hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x) * 8;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;

    uint srcIdx = y * srcImageStrideInBytes + (x << 2);
    d_uint8 pRGB = *((d_uint8 *)(&pSrcImage[srcIdx]));

    uint dstYIdx  = y * dstYImageStrideInBytes + x;
    uint dstUIdx  = y * dstUImageStrideInBytes + x;
    uint dstVIdx  = y * dstVImageStrideInBytes + x;

    float4 f;
    uint2 yChannel, uChannel, vChannel;

    float3 cY = make_float3(0.2126f, 0.7152f, 0.0722f);
    f.x = dot3_(cY, make_float3(unpack0_(pRGB.data[0]), unpack1_(pRGB.data[0]), unpack2_(pRGB.data[0])));
    f.y = dot3_(cY, make_float3(unpack0_(pRGB.data[1]), unpack1_(pRGB.data[1]), unpack2_(pRGB.data[1])));
    f.z = dot3_(cY, make_float3(unpack0_(pRGB.data[2]), unpack1_(pRGB.data[2]), unpack2_(pRGB.data[2])));
    f.w = dot3_(cY, make_float3(unpack0_(pRGB.data[3]), unpack1_(pRGB.data[3]), unpack2_(pRGB.data[3])));
    yChannel.x = pack_(f);
    f.x = dot3_(cY, make_float3(unpack0_(pRGB.data[4]), unpack1_(pRGB.data[4]), unpack2_(pRGB.data[4])));
    f.y = dot3_(cY, make_float3(unpack0_(pRGB.data[5]), unpack1_(pRGB.data[5]), unpack2_(pRGB.data[5])));
    f.z = dot3_(cY, make_float3(unpack0_(pRGB.data[6]), unpack1_(pRGB.data[6]), unpack2_(pRGB.data[6])));
    f.w = dot3_(cY, make_float3(unpack0_(pRGB.data[7]), unpack1_(pRGB.data[7]), unpack2_(pRGB.data[7])));
    yChannel.y = pack_(f);

    float3 cU = make_float3(-0.1146f, -0.3854f, 0.5f);
    f.x = dot3_(cU, make_float3(unpack0_(pRGB.data[0]), unpack1_(pRGB.data[0]), unpack2_(pRGB.data[0]))) + 128.0f;
    f.y = dot3_(cU, make_float3(unpack0_(pRGB.data[1]), unpack1_(pRGB.data[1]), unpack2_(pRGB.data[1]))) + 128.0f;
    f.z = dot3_(cU, make_float3(unpack0_(pRGB.data[2]), unpack1_(pRGB.data[2]), unpack2_(pRGB.data[2]))) + 128.0f;
    f.w = dot3_(cU, make_float3(unpack0_(pRGB.data[3]), unpack1_(pRGB.data[3]), unpack2_(pRGB.data[3]))) + 128.0f;
    uChannel.x = pack_(f);
    f.x = dot3_(cU, make_float3(unpack0_(pRGB.data[4]), unpack1_(pRGB.data[4]), unpack2_(pRGB.data[4]))) + 128.0f;
    f.y = dot3_(cU, make_float3(unpack0_(pRGB.data[5]), unpack1_(pRGB.data[5]), unpack2_(pRGB.data[5]))) + 128.0f;
    f.z = dot3_(cU, make_float3(unpack0_(pRGB.data[6]), unpack1_(pRGB.data[6]), unpack2_(pRGB.data[6]))) + 128.0f;
    f.w = dot3_(cU, make_float3(unpack0_(pRGB.data[7]), unpack1_(pRGB.data[7]), unpack2_(pRGB.data[7]))) + 128.0f;
    uChannel.y = pack_(f);

    float3 cV = make_float3(0.5f, -0.4542f, -0.0458f);
    f.x = dot3_(cV, make_float3(unpack0_(pRGB.data[0]), unpack1_(pRGB.data[0]), unpack2_(pRGB.data[0]))) + 128.0f;
    f.y = dot3_(cV, make_float3(unpack0_(pRGB.data[1]), unpack1_(pRGB.data[1]), unpack2_(pRGB.data[1]))) + 128.0f;
    f.z = dot3_(cV, make_float3(unpack0_(pRGB.data[2]), unpack1_(pRGB.data[2]), unpack2_(pRGB.data[2]))) + 128.0f;
    f.w = dot3_(cV, make_float3(unpack0_(pRGB.data[3]), unpack1_(pRGB.data[3]), unpack2_(pRGB.data[3]))) + 128.0f;
    vChannel.x = pack_(f);
    f.x = dot3_(cV, make_float3(unpack0_(pRGB.data[4]), unpack1_(pRGB.data[4]), unpack2_(pRGB.data[4]))) + 128.0f;
    f.y = dot3_(cV, make_float3(unpack0_(pRGB.data[5]), unpack1_(pRGB.data[5]), unpack2_(pRGB.data[5]))) + 128.0f;
    f.z = dot3_(cV, make_float3(unpack0_(pRGB.data[6]), unpack1_(pRGB.data[6]), unpack2_(pRGB.data[6]))) + 128.0f;
    f.w = dot3_(cV, make_float3(unpack0_(pRGB.data[7]), unpack1_(pRGB.data[7]), unpack2_(pRGB.data[7]))) + 128.0f;
    vChannel.y = pack_(f);

    *((uint2 *)(&pDstYImage[dstYIdx])) = yChannel;
    *((uint2 *)(&pDstUImage[dstUIdx])) = uChannel;
    *((uint2 *)(&pDstVImage[dstVIdx])) = vChannel;
}
int HipExec_ColorConvert_YUV4_RGBX(hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstYImage, vx_uint32 dstYImageStrideInBytes,
    vx_uint8 *pHipDstUImage, vx_uint32 dstUImageStrideInBytes,
    vx_uint8 *pHipDstVImage, vx_uint32 dstVImageStrideInBytes,
    const vx_uint8 *pHipSrcImage, vx_uint32 srcImageStrideInBytes) {
    int localThreads_x = 16;
    int localThreads_y = 16;
    int globalThreads_x = (dstWidth + 7) >> 3;
    int globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ColorConvert_YUV4_RGBX, dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                        dim3(localThreads_x, localThreads_y), 0, stream, dstWidth, dstHeight, (uchar *)pHipDstYImage, dstYImageStrideInBytes, (uchar *)pHipDstUImage, dstUImageStrideInBytes, (uchar *)pHipDstVImage, dstVImageStrideInBytes,
                        (const uchar *)pHipSrcImage, srcImageStrideInBytes);

    return VX_SUCCESS;
}

// __global__ void __attribute__((visibility("default")))
// Hip_ColorConvert_YUV4_RGB(
//     vx_uint32 dstWidth, vx_uint32 dstHeight,
//     unsigned short *pDstYImage, unsigned int dstYImageStrideInBytes,
//     unsigned short *pDstUImage, unsigned int dstUImageStrideInBytes,
//     unsigned short *pDstVImage, unsigned int dstVImageStrideInBytes,
//     const unsigned short *pSrcImage, unsigned int srcImageStrideInBytes
//     ) {
//     int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
//     int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;
//     if ((x * 2 >= dstWidth) || (y  >= dstHeight))    return;
//     unsigned int dstYIdx = y * (dstYImageStrideInBytes >> 1) + x;
//     unsigned int dstUIdx = y * (dstUImageStrideInBytes >> 1) + x;
//     unsigned int dstVIdx = y * (dstVImageStrideInBytes >> 1) + x;
//     unsigned int srcIdx = y * (srcImageStrideInBytes >> 1) + (x * 3);

//     float2 src0 = uchars_to_float2(pSrcImage[srcIdx]);
//     float2 src1 = uchars_to_float2(pSrcImage[srcIdx + 1]);
//     float2 src2 = uchars_to_float2(pSrcImage[srcIdx + 2]);
//     float2 dstY = make_float2(nearbyintf(RGB2Y(src0.x, src0.y, src1.x)),nearbyintf(RGB2Y(src1.y, src2.x, src2.y)));
//     float2 dstU = make_float2(nearbyintf(RGB2U(src0.x, src0.y, src1.x)),nearbyintf(RGB2U(src1.y, src2.x, src2.y)));
//     float2 dstV = make_float2(nearbyintf(RGB2V(src0.x, src0.y, src1.x)),nearbyintf(RGB2V(src1.y, src2.x, src2.y)));
//     pDstYImage[dstYIdx] = float2_to_uchars(dstY);
//     pDstUImage[dstUIdx] = float2_to_uchars(dstU);
//     pDstVImage[dstVIdx] = float2_to_uchars(dstV);
// }
// int HipExec_ColorConvert_YUV4_RGB(
//     hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
//     vx_uint8 *pHipDstYImage, vx_uint32 dstYImageStrideInBytes,
//     vx_uint8 *pHipDstUImage, vx_uint32 dstUImageStrideInBytes,
//     vx_uint8 *pHipDstVImage, vx_uint32 dstVImageStrideInBytes,
//     const vx_uint8 *pHipSrcImage, vx_uint32 srcImageStrideInBytes
//     ) {
//     int localThreads_x = 16, localThreads_y = 16;
//     int globalThreads_x = (dstWidth + 3) >> 1, globalThreads_y = dstHeight;

//     hipLaunchKernelGGL(Hip_ColorConvert_YUV4_RGB,
//                        dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
//                        dim3(localThreads_x, localThreads_y),
//                        0, stream, dstWidth, dstHeight,
//                        (unsigned short *)pHipDstYImage, dstYImageStrideInBytes,
//                        (unsigned short *)pHipDstUImage, dstUImageStrideInBytes,
//                        (unsigned short *)pHipDstVImage, dstVImageStrideInBytes,
//                        (const unsigned short *)pHipSrcImage, srcImageStrideInBytes);
//     return VX_SUCCESS;
// }

// __global__ void __attribute__((visibility("default")))
// Hip_ColorConvert_YUV4_RGBX(
//     vx_uint32 dstWidth, vx_uint32 dstHeight,
//     unsigned short *pDstYImage, unsigned int dstYImageStrideInBytes,
//     unsigned short *pDstUImage, unsigned int dstUImageStrideInBytes,
//     unsigned short *pDstVImage, unsigned int dstVImageStrideInBytes,
//     const unsigned int *pSrcImage, unsigned int srcImageStrideInBytes
//     ) {
//     int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
//     int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;
//     if ((x * 2 >= dstWidth) || (y  >= dstHeight))    return;
//     unsigned int dstYIdx = y * (dstYImageStrideInBytes >> 1) + x;
//     unsigned int dstUIdx = y * (dstUImageStrideInBytes >> 1) + x;
//     unsigned int dstVIdx = y * (dstVImageStrideInBytes >> 1) + x;
//     unsigned int srcIdx = y * (srcImageStrideInBytes >> 2) + (x * 2);

//     float4 rgb0 = uchars_to_float4(pSrcImage[srcIdx]);
//     float4 rgb1 = uchars_to_float4(pSrcImage[srcIdx+1]);
//     float2 dstY = make_float2(nearbyintf(RGB2Y(rgb0.x, rgb0.y, rgb0.z)),nearbyintf(RGB2Y(rgb1.x, rgb1.y, rgb1.z)));
//     float2 dstU = make_float2(nearbyintf(RGB2U(rgb0.x, rgb0.y, rgb0.z)),nearbyintf(RGB2U(rgb1.x, rgb1.y, rgb1.z)));
//     float2 dstV = make_float2(nearbyintf(RGB2V(rgb0.x, rgb0.y, rgb0.z)),nearbyintf(RGB2V(rgb1.x, rgb1.y, rgb1.z)));
//     pDstYImage[dstYIdx] = float2_to_uchars(dstY);
//     pDstUImage[dstUIdx] = float2_to_uchars(dstU);
//     pDstVImage[dstVIdx] = float2_to_uchars(dstV);
// }
// int HipExec_ColorConvert_YUV4_RGBX(
//     hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
//     vx_uint8 *pHipDstYImage, vx_uint32 dstYImageStrideInBytes,
//     vx_uint8 *pHipDstUImage, vx_uint32 dstUImageStrideInBytes,
//     vx_uint8 *pHipDstVImage, vx_uint32 dstVImageStrideInBytes,
//     const vx_uint8 *pHipSrcImage, vx_uint32 srcImageStrideInBytes
//     ) {
//     int localThreads_x = 16, localThreads_y = 16;
//     int globalThreads_x = (dstWidth + 3) >> 1, globalThreads_y = dstHeight;

//     hipLaunchKernelGGL(Hip_ColorConvert_YUV4_RGBX,
//                        dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
//                        dim3(localThreads_x, localThreads_y),
//                        0, stream, dstWidth, dstHeight,
//                        (unsigned short *)pHipDstYImage, dstYImageStrideInBytes,
//                        (unsigned short *)pHipDstUImage, dstUImageStrideInBytes,
//                        (unsigned short *)pHipDstVImage, dstVImageStrideInBytes,
//                        (const unsigned int *)pHipSrcImage, srcImageStrideInBytes);

//     return VX_SUCCESS;
// }













// Group 8 - Helper kernels with vision functions

__global__ void __attribute__((visibility("default")))
Hip_FormatConvert_IUV_UV12(
    vx_uint32 dstWidth, vx_uint32 dstHeight,
    unsigned short *pDstUImage, unsigned int dstUImageStrideInBytes,
    unsigned short *pDstVImage, unsigned int dstVImageStrideInBytes,
    const unsigned int *pSrcChromaImage, unsigned int srcChromaImageStrideInBytes
    ) {
    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;
    if ((x*2 >= dstWidth) || (y >= dstHeight)) return;
    unsigned int dstUIdx = y * (dstUImageStrideInBytes>>1) + x;
    unsigned int dstVIdx = y * (dstVImageStrideInBytes>>1) + x;
    unsigned int srcChromaIdx = y * (srcChromaImageStrideInBytes>>2) + x;

    uint4 src = uchars_to_uint4(pSrcChromaImage[srcChromaIdx]);
    uint2 dstU = make_uint2(src.x, src.z);
    uint2 dstV = make_uint2(src.y, src.w);
    pDstUImage[dstUIdx] = uint2_to_uchars(dstU);
    pDstVImage[dstVIdx] = uint2_to_uchars(dstV);
}
int HipExec_FormatConvert_IUV_UV12(
    hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstUImage, vx_uint32 dstUImageStrideInBytes,
    vx_uint8 *pHipDstVImage, vx_uint32 dstVImageStrideInBytes,
    const vx_uint8 *pHipSrcChromaImage, vx_uint32 srcChromaImageStrideInBytes
    ) {
    int localThreads_x = 16, localThreads_y = 16;
    int globalThreads_x = (dstWidth + 3) >> 1, globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_FormatConvert_IUV_UV12,
                    dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                    dim3(localThreads_x, localThreads_y),
                    0, stream, dstWidth, dstHeight,
                    (unsigned short *)pHipDstUImage, dstUImageStrideInBytes,
                    (unsigned short *)pHipDstVImage, dstVImageStrideInBytes,
                    (const unsigned int *)pHipSrcChromaImage, srcChromaImageStrideInBytes);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_FormatConvert_UV12_IUV(
    vx_uint32 dstWidth, vx_uint32 dstHeight,
    unsigned int *pDstChromaImage, unsigned int dstChromaImageStrideInBytes,
    const unsigned short *pSrcUImage, unsigned int srcUImageStrideInBytes,
    const unsigned short *pSrcVImage, unsigned int srcVImageStrideInBytes
    ) {
    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;
    if ((x * 2 >= dstWidth) || (y >= dstHeight)) return;
    unsigned int dstChromaIdx = y * (dstChromaImageStrideInBytes >> 2) + x;
    unsigned int srcUIdx = y * (srcUImageStrideInBytes >> 1) + x;
    unsigned int srcVIdx = y * (srcVImageStrideInBytes >> 1) + x;

    uint2 srcU = uchars_to_uint2(pSrcUImage[srcUIdx]);
    uint2 srcV = uchars_to_uint2(pSrcVImage[srcVIdx]);
    uint4 dst = make_uint4(srcU.x, srcV.x, srcU.y, srcV.y);
    pDstChromaImage[dstChromaIdx] = uint4_to_uchars(dst);
}
int HipExec_FormatConvert_UV12_IUV(
    hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstChromaImage, vx_uint32 dstChromaImageStrideInBytes,
    const vx_uint8 *pHipSrcUImage, vx_uint32 srcUImageStrideInBytes,
    const vx_uint8 *pHipSrcVImage, vx_uint32 srcVImageStrideInBytes
    ) {
    int localThreads_x = 16, localThreads_y = 16;
    int globalThreads_x = (dstWidth + 3) >> 1, globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_FormatConvert_UV12_IUV,
                       dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                       dim3(localThreads_x, localThreads_y),
                       0, stream, dstWidth, dstHeight,
                       (unsigned int *)pHipDstChromaImage, dstChromaImageStrideInBytes,
                       (const unsigned short *)pHipSrcUImage, srcUImageStrideInBytes,
                       (const unsigned short *)pHipSrcVImage, srcVImageStrideInBytes);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_FormatConvert_UV_UV12(
    vx_uint32 dstWidth, vx_uint32 dstHeight,
    unsigned short *pDstUImage, unsigned int dstUImageStrideInBytes,
    unsigned short *pDstVImage, unsigned int dstVImageStrideInBytes,
    const unsigned short *pSrcCImage, unsigned int srcCImageStrideInBytes
    ) {
    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;
    if ((x * 2 >= dstWidth) || (y >= dstHeight)) return;
    unsigned int dstUIdx = y * (dstUImageStrideInBytes >> 1) + x;
    unsigned int dstVIdx = y * (dstVImageStrideInBytes >> 1) + x;
    unsigned int srcIdx = (y >> 1) * (srcCImageStrideInBytes >> 1) + x;

    uint2 src = uchars_to_uint2(pSrcCImage[srcIdx]);
    uint2 dstU = make_uint2(src.x, src.x);
    uint2 dstV = make_uint2(src.y, src.y);
    pDstUImage[dstUIdx] = uint2_to_uchars(dstU);
    pDstVImage[dstVIdx] = uint2_to_uchars(dstV);
}
int HipExec_FormatConvert_UV_UV12(
    hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstUImage, vx_uint32 dstUImageStrideInBytes,
    vx_uint8 *pHipDstVImage, vx_uint32 dstVImageStrideInBytes,
    const vx_uint8 *pHipSrcCImage, vx_uint32 srcImageStrideInBytes
    ) {
    int localThreads_x = 16, localThreads_y = 16;
    int globalThreads_x = (dstWidth + 3) >> 1, globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_FormatConvert_UV_UV12,
                       dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                       dim3(localThreads_x, localThreads_y),
                       0, stream, dstWidth, dstHeight,
                       (unsigned short *)pHipDstUImage, dstUImageStrideInBytes,
                       (unsigned short *)pHipDstVImage, dstVImageStrideInBytes,
                       (const unsigned short *)pHipSrcCImage, srcImageStrideInBytes);

    return VX_SUCCESS;
}

__global__ void __attribute__((visibility("default")))
Hip_ScaleUp2x2_U8_U8(
    vx_uint32 dstWidth, vx_uint32 dstHeight,
    unsigned int *pDstImage, unsigned int dstImageStrideInBytes,
    const unsigned short *pSrcImage, unsigned int srcCImageStrideInBytes
    ) {
    int x = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    int y = hipBlockDim_y * hipBlockIdx_y + hipThreadIdx_y;
    if ((x * 4 >= dstWidth) || (y >= dstHeight))    return;
    unsigned int dstIdx = y * (dstImageStrideInBytes >> 2) + x;
    unsigned int srcIdx = (y >> 1) * (srcCImageStrideInBytes >> 1) + x;

    uint2 src = uchars_to_uint2(pSrcImage[srcIdx]);
    uint4 dst = make_uint4(src.x, src.x, src.y, src.y);
    pDstImage[dstIdx] = uint4_to_uchars(dst);
}
int HipExec_ScaleUp2x2_U8_U8(
    hipStream_t stream, vx_uint32 dstWidth, vx_uint32 dstHeight,
    vx_uint8 *pHipDstImage, vx_uint32 dstUImageStrideInBytes,
    const vx_uint8 *pHipSrcImage, vx_uint32 srcImageStrideInBytes
    ) {
    int localThreads_x = 16, localThreads_y = 16;
    int globalThreads_x = (dstWidth + 3) >> 2, globalThreads_y = dstHeight;

    hipLaunchKernelGGL(Hip_ScaleUp2x2_U8_U8,
                       dim3(ceil((float)globalThreads_x / localThreads_x), ceil((float)globalThreads_y / localThreads_y)),
                       dim3(localThreads_x, localThreads_y),
                       0, stream, dstWidth, dstHeight,
                       (unsigned int *)pHipDstImage, dstUImageStrideInBytes,
                       (const unsigned short *)pHipSrcImage, srcImageStrideInBytes);

    return VX_SUCCESS;
}
