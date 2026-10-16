#include <metal_stdlib>
using namespace metal;

struct BaseUniforms {
    uint4 display; /* x, y, width, height in VRAM */
    uint4 target;  /* output width, output height, enabled, rgb24 */
};

struct ItemUniforms {
    uint4 display;
    uint4 target;  /* output width, output height, enabled, unused */
    uint4 texture; /* flags, tpage, clut, texture window */
    uint4 control; /* semi mode, depth, mask-set, mask-check */
};

struct GpuVertex {
    float2 position [[attribute(0)]];
    float2 uv       [[attribute(1)]];
    float4 color    [[attribute(2)]];
};

struct GpuVarying {
    float4 position [[position]];
    float2 uv [[user(locn0), center_no_perspective]];
    float4 color [[user(locn1), center_no_perspective]];
};

vertex GpuVarying jojo_fullscreen_vertex(uint vertexID [[vertex_id]]) {
    const float2 p[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };
    GpuVarying out;
    out.position = float4(p[vertexID], 0.0, 1.0);
    out.uv = float2(0.0);
    out.color = float4(1.0);
    return out;
}

vertex GpuVarying jojo_gpu_vertex(const device GpuVertex *vertices [[buffer(0)]],
                                 uint vertexID [[vertex_id]],
                                 constant ItemUniforms &u [[buffer(1)]]) {
    GpuVertex v = vertices[vertexID];
    float2 size = float2(max(u.target.x, 1u), max(u.target.y, 1u));
    GpuVarying out;
    out.position = float4((v.position.x / size.x) * 2.0 - 1.0,
                          1.0 - (v.position.y / size.y) * 2.0, 0.0, 1.0);
    out.uv = v.uv;
    out.color = v.color;
    return out;
}

static inline float3 rgb555(ushort value) {
    return float3(float(value & 31u), float((value >> 5) & 31u), float((value >> 10) & 31u)) / 31.0;
}

static inline ushort read_vram(texture2d<ushort, access::read> vram, uint2 p) {
    return vram.read(p).r;
}

fragment float4 jojo_base_fragment(GpuVarying in [[stage_in]],
                                   texture2d<ushort, access::read> vram [[texture(0)]],
                                   constant BaseUniforms &u [[buffer(0)]]) {
    uint2 p = uint2(in.position.xy);
    if (u.target.z == 0u || p.x >= u.target.x || p.y >= u.target.y ||
        p.x >= 384u || p.y >= 240u || u.display.z == 0u || u.display.w == 0u) {
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    uint sx = u.display.x + (p.x * u.display.z) / max(u.target.x, 1u);
    uint sy = u.display.y + (p.y * u.display.w) / max(u.target.y, 1u);
    if (u.target.w != 0u) {
        /* 24-bit scanout is a byte stream packed across 16-bit VRAM words. */
        uint byteX = u.display.x * 2u + (p.x * u.display.z * 3u) / max(u.target.x, 1u);
        uint row = sy % 512u;
        uint byte0 = byteX;
        uint x0 = (byte0 >> 1) % 1024u;
        ushort w0 = read_vram(vram, uint2(x0, row));
        uint c0 = (uint(w0) >> ((byte0 & 1u) * 8u)) & 255u;
        uint byte1 = byte0 + 1u;
        ushort w1 = read_vram(vram, uint2((byte1 >> 1) % 1024u, row));
        uint c1 = (uint(w1) >> ((byte1 & 1u) * 8u)) & 255u;
        uint byte2 = byte0 + 2u;
        ushort w2 = read_vram(vram, uint2((byte2 >> 1) % 1024u, row));
        uint c2 = (uint(w2) >> ((byte2 & 1u) * 8u)) & 255u;
        return float4(float3(c0, c1, c2) / 255.0, 0.0);
    }
    ushort value = read_vram(vram, uint2(sx % 1024u, sy % 512u));
    return float4(rgb555(value), (value & 0x8000u) != 0u ? 1.0 : 0.0);
}

static inline ushort texel(texture2d<ushort, access::read> vram, uint2 uv, uint pageX, uint pageY,
                           uint depth, uint clut) {
    if (depth == 0u) {
        uint word = uint(read_vram(vram, uint2((pageX + (uv.x >> 2)) % 1024u, (pageY + uv.y) % 512u)));
        uint index = (word >> ((uv.x & 3u) * 4u)) & 15u;
        uint cx = ((clut & 63u) << 4) + index;
        uint cy = (clut >> 6) & 511u;
        return read_vram(vram, uint2(cx % 1024u, cy));
    }
    if (depth == 1u) {
        uint word = uint(read_vram(vram, uint2((pageX + (uv.x >> 1)) % 1024u, (pageY + uv.y) % 512u)));
        uint index = (word >> ((uv.x & 1u) * 8u)) & 255u;
        uint cx = ((clut & 63u) << 4) + index;
        uint cy = (clut >> 6) & 511u;
        return read_vram(vram, uint2(cx % 1024u, cy));
    }
    return read_vram(vram, uint2((pageX + uv.x) % 1024u, (pageY + uv.y) % 512u));
}

fragment float4 jojo_item_fragment(GpuVarying in [[stage_in]],
                                   texture2d<ushort, access::read> vram [[texture(0)]],
                                   texture2d<float, access::read> prior [[texture(1)]],
                                   constant ItemUniforms &u [[buffer(0)]]) {
    uint2 p = uint2(in.position.xy);
    float4 old = prior.read(p);
    uint flags = u.texture.x;
    bool textured = (flags & 0x0002u) != 0u;
    bool semi = (flags & 0x0004u) != 0u;
    bool raw = (flags & 0x0008u) != 0u;
    bool dither = u.target.w != 0u;   /* decided per primitive in GpuMetal.swift (Gouraud, modulated texture, lines) */
    bool maskSet = u.control.z != 0u;
    bool maskCheck = u.control.w != 0u;
    if (maskCheck && old.a >= 0.5) return old;

    /* value5 is the PS1 colour in 5-bit units before dither and truncation.
     * Flat or Gouraud: the 8-bit colour is truncated by >> 3, so value5 = colour8 / 8.
     * Textured: texel5 * colour8 / 128 (128 is neutral, 255 is about 2x). Raw texture skips the multiply. */
    float3 value5;
    bool texelMask = false;
    if (textured) {
        uint uCoord = uint(max(in.uv.x, 0.0)) & 255u;
        uint vCoord = uint(max(in.uv.y, 0.0)) & 255u;
        /* GP0(E2): mask X bits 0-4, mask Y 5-9, offset X 10-14, offset Y 15-19, all in 8-texel steps.
         * u = (u & ~(maskX*8)) | ((offX & maskX) * 8), and the same for v. Same as gpu_sw_renderer.c:341-344. */
        uint window = u.texture.w;
        uint maskX = window & 31u;
        uint maskY = (window >> 5) & 31u;
        uint offX = (window >> 10) & 31u;
        uint offY = (window >> 15) & 31u;
        uCoord = (uCoord & ~(maskX << 3)) | ((offX & maskX) << 3);
        vCoord = (vCoord & ~(maskY << 3)) | ((offY & maskY) << 3);
        uint pageX = (u.texture.y & 15u) << 6;
        uint pageY = ((u.texture.y >> 4) & 1u) << 8;
        ushort value = texel(vram, uint2(uCoord, vCoord), pageX, pageY, u.control.y, u.texture.z);
        if ((value & 0x7fffu) == 0u) discard_fragment();
        texelMask = (value & 0x8000u) != 0u;
        float3 t5 = float3(float(value & 31u), float((value >> 5) & 31u), float((value >> 10) & 31u));
        value5 = raw ? t5 : t5 * (in.color.rgb * 255.0) / 128.0;
    } else {
        value5 = in.color.rgb * 255.0 / 8.0;
    }

    if (dither) {
        const float matrix[16] = { -4.0, 0.0, -3.0, 1.0, 2.0, -2.0, 3.0, -1.0,
                                   -3.0, 1.0, -4.0, 0.0, 3.0, -1.0, 2.0, -2.0 };
        uint2 absolute = uint2((p.x * u.display.z) / max(u.target.x, 1u) + u.display.x,
                               (p.y * u.display.w) / max(u.target.y, 1u) + u.display.y);
        /* The matrix is in 8-bit units, so dividing by 8 puts it in 5-bit units. */
        value5 += matrix[(absolute.y & 3u) * 4u + (absolute.x & 3u)] / 8.0;
    }
    /* Truncate (the hardware >> 3 is a floor). The 1e-3 absorbs float error in colours that are exact multiples of 8. */
    float3 src5 = clamp(floor(value5 + 1e-3), 0.0, 31.0);
    float3 dst5 = floor(clamp(old.rgb, 0.0, 1.0) * 31.0 + 0.5);
    bool blend = semi && (!textured || texelMask);
    float3 result5 = src5;
    if (blend) {
        switch (u.control.x & 3u) {
        case 0u: result5 = floor((dst5 + src5) * 0.5); break;
        case 1u: result5 = min(float3(31.0), dst5 + src5); break;
        case 2u: result5 = max(float3(0.0), dst5 - src5); break;
        default: result5 = min(float3(31.0), dst5 + floor(src5 * 0.25)); break;
        }
    }
    float alpha = (maskSet || texelMask) ? 1.0 : 0.0;
    return float4(result5 / 31.0, alpha);
}

fragment float4 jojo_present_fragment(GpuVarying in [[stage_in]],
                                      texture2d<float, access::read> image [[texture(0)]],
                                      constant uint2 &size [[buffer(0)]]) {
    uint2 p = uint2(in.position.xy * (float2(384.0, 240.0) / max(float2(size), float2(1.0))));
    p = min(p, uint2(383u, 239u));
    float4 color = image.read(p);
    return float4(color.rgb, 1.0);
}
