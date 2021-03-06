// GLSL shader autogenerated by cg2glsl.py.
#if defined(VERTEX)

#if __VERSION__ >= 130
#define COMPAT_VARYING out
#define COMPAT_ATTRIBUTE in
#define COMPAT_TEXTURE texture
#else
#define COMPAT_VARYING varying 
#define COMPAT_ATTRIBUTE attribute 
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif
COMPAT_VARYING     vec4 _t1;
COMPAT_VARYING     vec2 _texCoord1;
COMPAT_VARYING     vec4 _color1;
COMPAT_VARYING     vec4 _position1;
COMPAT_VARYING     float _frame_rotation;
struct input_dummy {
    vec2 _video_size;
    vec2 _texture_size;
    vec2 _output_dummy_size;
    float _frame_count;
    float _frame_direction;
    float _frame_rotation;
};
struct out_vertex {
    vec4 _position1;
    vec4 _color1;
    vec2 _texCoord1;
    vec4 _t1;
};
out_vertex _ret_0;
input_dummy _IN1;
vec4 _r0010;
COMPAT_ATTRIBUTE vec4 VertexCoord;
COMPAT_ATTRIBUTE vec4 COLOR;
COMPAT_ATTRIBUTE vec4 TexCoord;
COMPAT_VARYING vec4 COL0;
COMPAT_VARYING vec4 TEX0;
COMPAT_VARYING vec4 TEX1;
 
uniform mat4 MVPMatrix;
uniform int FrameDirection;
uniform int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;
void main()
{
    out_vertex _OUT;
    vec2 _ps;
    _r0010 = VertexCoord.x*MVPMatrix[0];
    _r0010 = _r0010 + VertexCoord.y*MVPMatrix[1];
    _r0010 = _r0010 + VertexCoord.z*MVPMatrix[2];
    _r0010 = _r0010 + VertexCoord.w*MVPMatrix[3];
    _ps = vec2(float((1.00000000E+00/TextureSize.x)), float((1.00000000E+00/TextureSize.y)));
    _OUT._t1.xy = vec2(_ps.x, 0.00000000E+00);
    _OUT._t1.zw = vec2(0.00000000E+00, _ps.y);
    _ret_0._position1 = _r0010;
    _ret_0._color1 = COLOR;
    _ret_0._texCoord1 = TexCoord.xy;
    _ret_0._t1 = _OUT._t1;
    gl_Position = vec4(float(_r0010.x), float(_r0010.y), float(_r0010.z), float(_r0010.w));
    COL0 = COLOR;
    TEX0.xy = TexCoord.xy;
    TEX1 = _OUT._t1;
    return;
    COL0 = _ret_0._color1;
    TEX0.xy = _ret_0._texCoord1;
    TEX1 = _ret_0._t1;
} 
#elif defined(FRAGMENT)

#if __VERSION__ >= 130
#define COMPAT_VARYING in
#define COMPAT_TEXTURE texture
out vec4 FragColor;
#else
#define COMPAT_VARYING varying
#define FragColor gl_FragColor
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif
COMPAT_VARYING     vec4 _t1;
COMPAT_VARYING     vec2 _texCoord;
COMPAT_VARYING     vec4 _color;
COMPAT_VARYING     float _frame_rotation;
struct input_dummy {
    vec2 _video_size;
    vec2 _texture_size;
    vec2 _output_dummy_size;
    float _frame_count;
    float _frame_direction;
    float _frame_rotation;
};
struct out_vertex {
    vec4 _color;
    vec2 _texCoord;
    vec4 _t1;
};
vec4 _ret_0;
vec3 _TMP35;
vec3 _TMP37;
vec3 _TMP39;
vec3 _TMP41;
vec3 _TMP42;
vec3 _TMP40;
vec3 _TMP38;
vec3 _TMP36;
vec4 _TMP28;
vec4 _TMP27;
float _TMP48;
vec4 _TMP20;
vec4 _TMP19;
vec4 _TMP18;
vec4 _TMP17;
vec4 _TMP16;
vec4 _TMP15;
vec4 _TMP14;
vec4 _TMP13;
vec4 _TMP12;
vec4 _TMP11;
vec4 _TMP10;
vec4 _TMP9;
vec4 _TMP8;
vec4 _TMP7;
vec4 _TMP6;
vec4 _TMP5;
vec4 _TMP4;
vec4 _TMP3;
vec4 _TMP2;
vec4 _TMP1;
vec4 _TMP0;
uniform sampler2D Texture;
input_dummy _IN1;
vec2 _x0062;
vec2 _c0064;
vec2 _c0066;
vec2 _c0068;
vec2 _c0070;
vec2 _c0074;
vec2 _c0076;
vec2 _c0078;
vec2 _c0080;
vec2 _c0082;
vec2 _c0084;
vec2 _c0086;
vec2 _c0088;
vec2 _c0090;
vec2 _c0092;
vec2 _c0094;
vec2 _c0096;
vec2 _c0098;
vec2 _c0100;
vec2 _c0102;
vec2 _c0104;
vec4 _r0106;
vec4 _r0116;
vec4 _r0126;
vec4 _r0136;
vec4 _r0146;
vec4 _r0156;
vec4 _TMP167;
vec4 _a0170;
vec4 _TMP171;
vec4 _a0174;
vec4 _TMP175;
vec4 _a0178;
vec4 _TMP179;
vec4 _a0182;
vec4 _TMP183;
vec4 _a0186;
vec4 _TMP189;
vec4 _a0192;
vec4 _TMP193;
vec4 _a0196;
vec4 _TMP197;
vec4 _a0200;
vec4 _TMP201;
vec4 _a0204;
vec4 _TMP205;
vec4 _a0208;
vec4 _TMP209;
vec4 _a0212;
vec4 _TMP213;
vec4 _a0216;
vec4 _TMP217;
vec4 _a0220;
vec4 _TMP221;
vec4 _a0224;
vec4 _TMP225;
vec4 _a0228;
vec4 _TMP229;
vec4 _a0232;
COMPAT_VARYING vec4 TEX0;
COMPAT_VARYING vec4 TEX1;
 
uniform int FrameDirection;
uniform int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;
void main()
{
    bvec4 _edr;
    bvec4 _edr_left;
    bvec4 _edr_up;
    bvec4 _px;
    bvec4 _interp_restriction_lv1;
    bvec4 _interp_restriction_lv2_left;
    bvec4 _interp_restriction_lv2_up;
    bvec4 _nc;
    bvec4 _fx;
    bvec4 _fx_left;
    bvec4 _fx_up;
    vec2 _fp;
    vec3 _A1;
    vec3 _B1;
    vec3 _C;
    vec3 _D;
    vec3 _E;
    vec3 _F;
    vec3 _G;
    vec3 _H;
    vec3 _I;
    vec3 _A11;
    vec3 _C1;
    vec3 _A0;
    vec3 _G0;
    vec3 _C4;
    vec3 _I4;
    vec3 _G5;
    vec3 _I5;
    vec3 _B11;
    vec3 _D0;
    vec3 _H5;
    vec3 _F4;
    vec4 _b1;
    vec4 _c1;
    vec4 _e1;
    vec4 _i4;
    vec4 _i5;
    vec4 _h5;
    _x0062 = TEX0.xy*TextureSize;
    _fp = fract(_x0062);
    _c0064 = (TEX0.xy - vec2(float(TEX1.x), float(TEX1.y))) - vec2(float(TEX1.z), float(TEX1.w));
    _TMP0 = COMPAT_TEXTURE(Texture, _c0064);
    _A1 = vec3(float(_TMP0.x), float(_TMP0.y), float(_TMP0.z));
    _c0066 = TEX0.xy - vec2(float(TEX1.z), float(TEX1.w));
    _TMP1 = COMPAT_TEXTURE(Texture, _c0066);
    _B1 = vec3(float(_TMP1.x), float(_TMP1.y), float(_TMP1.z));
    _c0068 = (TEX0.xy + vec2(float(TEX1.x), float(TEX1.y))) - vec2(float(TEX1.z), float(TEX1.w));
    _TMP2 = COMPAT_TEXTURE(Texture, _c0068);
    _C = vec3(float(_TMP2.x), float(_TMP2.y), float(_TMP2.z));
    _c0070 = TEX0.xy - vec2(float(TEX1.x), float(TEX1.y));
    _TMP3 = COMPAT_TEXTURE(Texture, _c0070);
    _D = vec3(float(_TMP3.x), float(_TMP3.y), float(_TMP3.z));
    _TMP4 = COMPAT_TEXTURE(Texture, TEX0.xy);
    _E = vec3(float(_TMP4.x), float(_TMP4.y), float(_TMP4.z));
    _c0074 = TEX0.xy + vec2(float(TEX1.x), float(TEX1.y));
    _TMP5 = COMPAT_TEXTURE(Texture, _c0074);
    _F = vec3(float(_TMP5.x), float(_TMP5.y), float(_TMP5.z));
    _c0076 = (TEX0.xy - vec2(float(TEX1.x), float(TEX1.y))) + vec2(float(TEX1.z), float(TEX1.w));
    _TMP6 = COMPAT_TEXTURE(Texture, _c0076);
    _G = vec3(float(_TMP6.x), float(_TMP6.y), float(_TMP6.z));
    _c0078 = TEX0.xy + vec2(float(TEX1.z), float(TEX1.w));
    _TMP7 = COMPAT_TEXTURE(Texture, _c0078);
    _H = vec3(float(_TMP7.x), float(_TMP7.y), float(_TMP7.z));
    _c0080 = TEX0.xy + vec2(float(TEX1.x), float(TEX1.y)) + vec2(float(TEX1.z), float(TEX1.w));
    _TMP8 = COMPAT_TEXTURE(Texture, _c0080);
    _I = vec3(float(_TMP8.x), float(_TMP8.y), float(_TMP8.z));
    _c0082 = (TEX0.xy - vec2(float(TEX1.x), float(TEX1.y))) - vec2(float((2.00000000E+00*TEX1.zw).x), float((2.00000000E+00*TEX1.zw).y));
    _TMP9 = COMPAT_TEXTURE(Texture, _c0082);
    _A11 = vec3(float(_TMP9.x), float(_TMP9.y), float(_TMP9.z));
    _c0084 = (TEX0.xy + vec2(float(TEX1.x), float(TEX1.y))) - vec2(float((2.00000000E+00*TEX1.zw).x), float((2.00000000E+00*TEX1.zw).y));
    _TMP10 = COMPAT_TEXTURE(Texture, _c0084);
    _C1 = vec3(float(_TMP10.x), float(_TMP10.y), float(_TMP10.z));
    _c0086 = (TEX0.xy - vec2(float((2.00000000E+00*TEX1.xy).x), float((2.00000000E+00*TEX1.xy).y))) - vec2(float(TEX1.z), float(TEX1.w));
    _TMP11 = COMPAT_TEXTURE(Texture, _c0086);
    _A0 = vec3(float(_TMP11.x), float(_TMP11.y), float(_TMP11.z));
    _c0088 = (TEX0.xy - vec2(float((2.00000000E+00*TEX1.xy).x), float((2.00000000E+00*TEX1.xy).y))) + vec2(float(TEX1.z), float(TEX1.w));
    _TMP12 = COMPAT_TEXTURE(Texture, _c0088);
    _G0 = vec3(float(_TMP12.x), float(_TMP12.y), float(_TMP12.z));
    _c0090 = (TEX0.xy + vec2(float((2.00000000E+00*TEX1.xy).x), float((2.00000000E+00*TEX1.xy).y))) - vec2(float(TEX1.z), float(TEX1.w));
    _TMP13 = COMPAT_TEXTURE(Texture, _c0090);
    _C4 = vec3(float(_TMP13.x), float(_TMP13.y), float(_TMP13.z));
    _c0092 = TEX0.xy + vec2(float((2.00000000E+00*TEX1.xy).x), float((2.00000000E+00*TEX1.xy).y)) + vec2(float(TEX1.z), float(TEX1.w));
    _TMP14 = COMPAT_TEXTURE(Texture, _c0092);
    _I4 = vec3(float(_TMP14.x), float(_TMP14.y), float(_TMP14.z));
    _c0094 = (TEX0.xy - vec2(float(TEX1.x), float(TEX1.y))) + vec2(float((2.00000000E+00*TEX1.zw).x), float((2.00000000E+00*TEX1.zw).y));
    _TMP15 = COMPAT_TEXTURE(Texture, _c0094);
    _G5 = vec3(float(_TMP15.x), float(_TMP15.y), float(_TMP15.z));
    _c0096 = TEX0.xy + vec2(float(TEX1.x), float(TEX1.y)) + vec2(float((2.00000000E+00*TEX1.zw).x), float((2.00000000E+00*TEX1.zw).y));
    _TMP16 = COMPAT_TEXTURE(Texture, _c0096);
    _I5 = vec3(float(_TMP16.x), float(_TMP16.y), float(_TMP16.z));
    _c0098 = TEX0.xy - vec2(float((2.00000000E+00*TEX1.zw).x), float((2.00000000E+00*TEX1.zw).y));
    _TMP17 = COMPAT_TEXTURE(Texture, _c0098);
    _B11 = vec3(float(_TMP17.x), float(_TMP17.y), float(_TMP17.z));
    _c0100 = TEX0.xy - vec2(float((2.00000000E+00*TEX1.xy).x), float((2.00000000E+00*TEX1.xy).y));
    _TMP18 = COMPAT_TEXTURE(Texture, _c0100);
    _D0 = vec3(float(_TMP18.x), float(_TMP18.y), float(_TMP18.z));
    _c0102 = TEX0.xy + vec2(float((2.00000000E+00*TEX1.zw).x), float((2.00000000E+00*TEX1.zw).y));
    _TMP19 = COMPAT_TEXTURE(Texture, _c0102);
    _H5 = vec3(float(_TMP19.x), float(_TMP19.y), float(_TMP19.z));
    _c0104 = TEX0.xy + vec2(float((2.00000000E+00*TEX1.xy).x), float((2.00000000E+00*TEX1.xy).y));
    _TMP20 = COMPAT_TEXTURE(Texture, _c0104);
    _F4 = vec3(float(_TMP20.x), float(_TMP20.y), float(_TMP20.z));
    _TMP48 = dot(vec3(float(_B1.x), float(_B1.y), float(_B1.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0106.x = float(_TMP48);
    _TMP48 = dot(vec3(float(_D.x), float(_D.y), float(_D.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0106.y = float(_TMP48);
    _TMP48 = dot(vec3(float(_H.x), float(_H.y), float(_H.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0106.z = float(_TMP48);
    _TMP48 = dot(vec3(float(_F.x), float(_F.y), float(_F.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0106.w = float(_TMP48);
    _b1 = vec4(float(_r0106.x), float(_r0106.y), float(_r0106.z), float(_r0106.w));
    _TMP48 = dot(vec3(float(_C.x), float(_C.y), float(_C.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0116.x = float(_TMP48);
    _TMP48 = dot(vec3(float(_A1.x), float(_A1.y), float(_A1.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0116.y = float(_TMP48);
    _TMP48 = dot(vec3(float(_G.x), float(_G.y), float(_G.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0116.z = float(_TMP48);
    _TMP48 = dot(vec3(float(_I.x), float(_I.y), float(_I.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0116.w = float(_TMP48);
    _c1 = vec4(float(_r0116.x), float(_r0116.y), float(_r0116.z), float(_r0116.w));
    _TMP48 = dot(vec3(float(_E.x), float(_E.y), float(_E.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0126.x = float(_TMP48);
    _TMP48 = dot(vec3(float(_E.x), float(_E.y), float(_E.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0126.y = float(_TMP48);
    _TMP48 = dot(vec3(float(_E.x), float(_E.y), float(_E.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0126.z = float(_TMP48);
    _TMP48 = dot(vec3(float(_E.x), float(_E.y), float(_E.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0126.w = float(_TMP48);
    _e1 = vec4(float(_r0126.x), float(_r0126.y), float(_r0126.z), float(_r0126.w));
    _TMP48 = dot(vec3(float(_I4.x), float(_I4.y), float(_I4.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0136.x = float(_TMP48);
    _TMP48 = dot(vec3(float(_C1.x), float(_C1.y), float(_C1.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0136.y = float(_TMP48);
    _TMP48 = dot(vec3(float(_A0.x), float(_A0.y), float(_A0.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0136.z = float(_TMP48);
    _TMP48 = dot(vec3(float(_G5.x), float(_G5.y), float(_G5.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0136.w = float(_TMP48);
    _i4 = vec4(float(_r0136.x), float(_r0136.y), float(_r0136.z), float(_r0136.w));
    _TMP48 = dot(vec3(float(_I5.x), float(_I5.y), float(_I5.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0146.x = float(_TMP48);
    _TMP48 = dot(vec3(float(_C4.x), float(_C4.y), float(_C4.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0146.y = float(_TMP48);
    _TMP48 = dot(vec3(float(_A11.x), float(_A11.y), float(_A11.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0146.z = float(_TMP48);
    _TMP48 = dot(vec3(float(_G0.x), float(_G0.y), float(_G0.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0146.w = float(_TMP48);
    _i5 = vec4(float(_r0146.x), float(_r0146.y), float(_r0146.z), float(_r0146.w));
    _TMP48 = dot(vec3(float(_H5.x), float(_H5.y), float(_H5.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0156.x = float(_TMP48);
    _TMP48 = dot(vec3(float(_F4.x), float(_F4.y), float(_F4.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0156.y = float(_TMP48);
    _TMP48 = dot(vec3(float(_B11.x), float(_B11.y), float(_B11.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0156.z = float(_TMP48);
    _TMP48 = dot(vec3(float(_D0.x), float(_D0.y), float(_D0.z)), vec3( 1.43593750E+01, 2.81718750E+01, 5.47265625E+00));
    _r0156.w = float(_TMP48);
    _h5 = vec4(float(_r0156.x), float(_r0156.y), float(_r0156.z), float(_r0156.w));
    _fx = bvec4((vec4( 1.00000000E+00, -1.00000000E+00, -1.00000000E+00, 1.00000000E+00)*_fp.y + vec4( 1.00000000E+00, 1.00000000E+00, -1.00000000E+00, -1.00000000E+00)*_fp.x).x > 1.50000000E+00, (vec4( 1.00000000E+00, -1.00000000E+00, -1.00000000E+00, 1.00000000E+00)*_fp.y + vec4( 1.00000000E+00, 1.00000000E+00, -1.00000000E+00, -1.00000000E+00)*_fp.x).y > 5.00000000E-01, (vec4( 1.00000000E+00, -1.00000000E+00, -1.00000000E+00, 1.00000000E+00)*_fp.y + vec4( 1.00000000E+00, 1.00000000E+00, -1.00000000E+00, -1.00000000E+00)*_fp.x).z > -5.00000000E-01, (vec4( 1.00000000E+00, -1.00000000E+00, -1.00000000E+00, 1.00000000E+00)*_fp.y + vec4( 1.00000000E+00, 1.00000000E+00, -1.00000000E+00, -1.00000000E+00)*_fp.x).w > 5.00000000E-01);
    _fx_left = bvec4((vec4( 1.00000000E+00, -1.00000000E+00, -1.00000000E+00, 1.00000000E+00)*_fp.y + vec4( 5.00000000E-01, 2.00000000E+00, -5.00000000E-01, -2.00000000E+00)*_fp.x).x > 1.00000000E+00, (vec4( 1.00000000E+00, -1.00000000E+00, -1.00000000E+00, 1.00000000E+00)*_fp.y + vec4( 5.00000000E-01, 2.00000000E+00, -5.00000000E-01, -2.00000000E+00)*_fp.x).y > 1.00000000E+00, (vec4( 1.00000000E+00, -1.00000000E+00, -1.00000000E+00, 1.00000000E+00)*_fp.y + vec4( 5.00000000E-01, 2.00000000E+00, -5.00000000E-01, -2.00000000E+00)*_fp.x).z > -5.00000000E-01, (vec4( 1.00000000E+00, -1.00000000E+00, -1.00000000E+00, 1.00000000E+00)*_fp.y + vec4( 5.00000000E-01, 2.00000000E+00, -5.00000000E-01, -2.00000000E+00)*_fp.x).w > 0.00000000E+00);
    _fx_up = bvec4((vec4( 1.00000000E+00, -1.00000000E+00, -1.00000000E+00, 1.00000000E+00)*_fp.y + vec4( 2.00000000E+00, 5.00000000E-01, -2.00000000E+00, -5.00000000E-01)*_fp.x).x > 2.00000000E+00, (vec4( 1.00000000E+00, -1.00000000E+00, -1.00000000E+00, 1.00000000E+00)*_fp.y + vec4( 2.00000000E+00, 5.00000000E-01, -2.00000000E+00, -5.00000000E-01)*_fp.x).y > 0.00000000E+00, (vec4( 1.00000000E+00, -1.00000000E+00, -1.00000000E+00, 1.00000000E+00)*_fp.y + vec4( 2.00000000E+00, 5.00000000E-01, -2.00000000E+00, -5.00000000E-01)*_fp.x).z > -1.00000000E+00, (vec4( 1.00000000E+00, -1.00000000E+00, -1.00000000E+00, 1.00000000E+00)*_fp.y + vec4( 2.00000000E+00, 5.00000000E-01, -2.00000000E+00, -5.00000000E-01)*_fp.x).w > 5.00000000E-01);
    _interp_restriction_lv1 = bvec4(_e1.x != _b1.w && _e1.x != _b1.z && (_b1.w != _b1.x && _b1.w != _c1.x || _b1.z != _b1.y && _b1.z != _c1.z || _e1.x == _c1.w && (_b1.w != _h5.y && _b1.w != _i4.x || _b1.z != _h5.x && _b1.z != _i5.x) || _e1.x == _c1.z || _e1.x == _c1.x), _e1.y != _b1.x && _e1.y != _b1.w && (_b1.x != _b1.y && _b1.x != _c1.y || _b1.w != _b1.z && _b1.w != _c1.w || _e1.y == _c1.x && (_b1.x != _h5.z && _b1.x != _i4.y || _b1.w != _h5.y && _b1.w != _i5.y) || _e1.y == _c1.w || _e1.y == _c1.y), _e1.z != _b1.y && _e1.z != _b1.x && (_b1.y != _b1.z && _b1.y != _c1.z || _b1.x != _b1.w && _b1.x != _c1.x || _e1.z == _c1.y && (_b1.y != _h5.w && _b1.y != _i4.z || _b1.x != _h5.z && _b1.x != _i5.z) || _e1.z == _c1.x || _e1.z == _c1.z), _e1.w != _b1.z && _e1.w != _b1.y && (_b1.z != _b1.w && _b1.z != _c1.w || _b1.y != _b1.x && _b1.y != _c1.y || _e1.w == _c1.z && (_b1.z != _h5.x && _b1.z != _i4.w || _b1.y != _h5.w && _b1.y != _i5.w) || _e1.w == _c1.y || _e1.w == _c1.w));
    _interp_restriction_lv2_left = bvec4(_e1.x != _c1.z && _b1.y != _c1.z, _e1.y != _c1.w && _b1.z != _c1.w, _e1.z != _c1.x && _b1.w != _c1.x, _e1.w != _c1.y && _b1.x != _c1.y);
    _interp_restriction_lv2_up = bvec4(_e1.x != _c1.x && _b1.x != _c1.x, _e1.y != _c1.y && _b1.y != _c1.y, _e1.z != _c1.z && _b1.z != _c1.z, _e1.w != _c1.w && _b1.w != _c1.w);
    _a0170 = _e1 - _c1;
    _TMP167 = abs(_a0170);
    _a0174 = _e1 - _c1.zwxy;
    _TMP171 = abs(_a0174);
    _a0178 = _c1.wxyz - _h5;
    _TMP175 = abs(_a0178);
    _a0182 = _c1.wxyz - _h5.yzwx;
    _TMP179 = abs(_a0182);
    _a0186 = _b1.zwxy - _b1.wxyz;
    _TMP183 = abs(_a0186);
    _TMP27 = _TMP167 + _TMP171 + _TMP175 + _TMP179 + 4.00000000E+00*_TMP183;
    _a0192 = _b1.zwxy - _b1.yzwx;
    _TMP189 = abs(_a0192);
    _a0196 = _b1.zwxy - _i5;
    _TMP193 = abs(_a0196);
    _a0200 = _b1.wxyz - _i4;
    _TMP197 = abs(_a0200);
    _a0204 = _b1.wxyz - _b1;
    _TMP201 = abs(_a0204);
    _a0208 = _e1 - _c1.wxyz;
    _TMP205 = abs(_a0208);
    _TMP28 = _TMP189 + _TMP193 + _TMP197 + _TMP201 + 4.00000000E+00*_TMP205;
    _edr = bvec4(_TMP27.x < _TMP28.x && _interp_restriction_lv1.x, _TMP27.y < _TMP28.y && _interp_restriction_lv1.y, _TMP27.z < _TMP28.z && _interp_restriction_lv1.z, _TMP27.w < _TMP28.w && _interp_restriction_lv1.w);
    _a0212 = _b1.wxyz - _c1.zwxy;
    _TMP209 = abs(_a0212);
    _a0216 = _b1.zwxy - _c1;
    _TMP213 = abs(_a0216);
    _edr_left = bvec4((2.00000000E+00*_TMP209).x <= _TMP213.x && _interp_restriction_lv2_left.x, (2.00000000E+00*_TMP209).y <= _TMP213.y && _interp_restriction_lv2_left.y, (2.00000000E+00*_TMP209).z <= _TMP213.z && _interp_restriction_lv2_left.z, (2.00000000E+00*_TMP209).w <= _TMP213.w && _interp_restriction_lv2_left.w);
    _a0220 = _b1.wxyz - _c1.zwxy;
    _TMP217 = abs(_a0220);
    _a0224 = _b1.zwxy - _c1;
    _TMP221 = abs(_a0224);
    _edr_up = bvec4(_TMP217.x >= (2.00000000E+00*_TMP221).x && _interp_restriction_lv2_up.x, _TMP217.y >= (2.00000000E+00*_TMP221).y && _interp_restriction_lv2_up.y, _TMP217.z >= (2.00000000E+00*_TMP221).z && _interp_restriction_lv2_up.z, _TMP217.w >= (2.00000000E+00*_TMP221).w && _interp_restriction_lv2_up.w);
    _nc = bvec4(_edr.x && (_fx.x || _edr_left.x && _fx_left.x || _edr_up.x && _fx_up.x), _edr.y && (_fx.y || _edr_left.y && _fx_left.y || _edr_up.y && _fx_up.y), _edr.z && (_fx.z || _edr_left.z && _fx_left.z || _edr_up.z && _fx_up.z), _edr.w && (_fx.w || _edr_left.w && _fx_left.w || _edr_up.w && _fx_up.w));
    _a0228 = _e1 - _b1.wxyz;
    _TMP225 = abs(_a0228);
    _a0232 = _e1 - _b1.zwxy;
    _TMP229 = abs(_a0232);
    _px = bvec4(_TMP225.x <= _TMP229.x, _TMP225.y <= _TMP229.y, _TMP225.z <= _TMP229.z, _TMP225.w <= _TMP229.w);
    if (_nc.x) { 
        if (_px.x) { 
            _TMP36 = _F;
        } else {
            _TMP36 = _H;
        } 
        _TMP35 = _TMP36;
    } else {
        if (_nc.y) { 
            if (_px.y) { 
                _TMP38 = _B1;
            } else {
                _TMP38 = _F;
            } 
            _TMP37 = _TMP38;
        } else {
            if (_nc.z) { 
                if (_px.z) { 
                    _TMP40 = _D;
                } else {
                    _TMP40 = _B1;
                } 
                _TMP39 = _TMP40;
            } else {
                if (_nc.w) { 
                    if (_px.w) { 
                        _TMP42 = _H;
                    } else {
                        _TMP42 = _D;
                    } 
                    _TMP41 = _TMP42;
                } else {
                    _TMP41 = _E;
                } 
                _TMP39 = _TMP41;
            } 
            _TMP37 = _TMP39;
        } 
        _TMP35 = _TMP37;
    } 
    _ret_0 = vec4(_TMP35.x, _TMP35.y, _TMP35.z, 1.00000000E+00);
    FragColor = vec4(float(_ret_0.x), float(_ret_0.y), float(_ret_0.z), float(_ret_0.w));
    return;
} 
#endif
