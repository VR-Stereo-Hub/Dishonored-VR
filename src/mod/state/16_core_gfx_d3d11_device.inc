// State chunk 16 for core/gfx/d3d11_device - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


static const char* kShaderSrc =
    "struct VSIn  { float4 pos : POSITION; float2 uv : TEXCOORD0; };\n"
    "struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };\n"
    "VSOut vsmain(VSIn i) { VSOut o; o.pos = i.pos; o.uv = i.uv; return o; }\n"
    "Texture2D gameTex : register(t0);\n"
    "SamplerState samp : register(s0);\n"
    "float4 psmain(VSOut i) : SV_Target {\n"
    "    return float4(gameTex.Sample(samp, i.uv).rgb, 1.0);\n"
    "}\n";

struct QuadVert { float pos[4]; float uv[2]; };
