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
vec4 _r0006;
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
    _r0006 = VertexCoord.x*MVPMatrix[0];
    _r0006 = _r0006 + VertexCoord.y*MVPMatrix[1];
    _r0006 = _r0006 + VertexCoord.z*MVPMatrix[2];
    _r0006 = _r0006 + VertexCoord.w*MVPMatrix[3];
    _ps = vec2(1.00000000E+00/TextureSize.x, 1.00000000E+00/TextureSize.y);
    _OUT._t1.xy = vec2(0.00000000E+00, -_ps.y);
    _OUT._t1.zw = vec2(-_ps.x, 0.00000000E+00);
    _ret_0._position1 = _r0006;
    _ret_0._color1 = COLOR;
    _ret_0._texCoord1 = TexCoord.xy;
    _ret_0._t1 = _OUT._t1;
    gl_Position = _r0006;
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
COMPAT_VARYING     vec4 _color1;
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
    vec4 _color1;
    vec2 _texCoord;
    vec4 _t1;
};
vec4 _ret_0;
float _TMP15;
float _TMP16;
vec4 _TMP13;
vec4 _TMP11;
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
vec2 _x0022;
vec2 _c0026;
vec2 _c0028;
vec2 _c0030;
vec2 _c0032;
vec2 _c0036;
vec2 _c0038;
vec2 _c0040;
vec2 _c0042;
vec2 _c0048;
vec2 _c0052;
COMPAT_VARYING vec4 TEX0;
COMPAT_VARYING vec4 TEX1;
 
uniform int FrameDirection;
uniform int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;
void main()
{
    vec2 _fp;
    vec2 _st;
    vec2 _g1;
    vec2 _g2;
    float _AO;
    float _BO;
    float _CO;
    float _AX;
    float _BX;
    float _CX;
    float _AY;
    float _BY;
    float _CY;
    vec3 _A;
    vec3 _B;
    vec3 _C;
    vec3 _D;
    vec3 _E;
    vec3 _F;
    vec3 _G;
    vec3 _H;
    vec3 _I;
    vec3 _C4;
    vec3 _G5;
    float _a;
    float _b;
    float _c;
    float _d;
    float _e;
    float _f;
    float _g;
    float _h;
    float _i;
    float _c4;
    float _g5;
    vec3 _res;
    bool _fx_1;
    bool _fx_2;
    bool _fx_3;
    bool _condition_1;
    bool _condition_1a;
    bool _condition_1b;
    bool _condition_2;
    bool _condition_3;
    bool _condition_4;
    bool _condition_7;
    bool _condition_8;
    bool _condition_7a;
    bool _condition_8a;
    _x0022 = TEX0.xy*TextureSize;
    _fp = fract(_x0022);
    _st = vec2(float((_fp.x >= 5.00000000E-01)), float((_fp.y >= 5.00000000E-01)));
    _g1 = TEX1.xy*((_st.x + _st.y) - 1.00000000E+00) + TEX1.zw*(_st.x - _st.y);
    _g2 = TEX1.xy*(_st.y - _st.x) + TEX1.zw*((_st.x + _st.y) - 1.00000000E+00);
    _AO = 2.00000000E+00*_st.y - 1.00000000E+00;
    _BO = 2.00000000E+00*_st.x - 1.00000000E+00;
    _CO = (_st.x + _st.y) - 5.00000000E-01;
    _AX = (5.00000000E-01*_st.x + 1.50000000E+00*_st.y) - 1.00000000E+00;
    _BX = (1.50000000E+00*_st.x - 5.00000000E-01*_st.y) - 5.00000000E-01;
    _CX = (_st.x + 5.00000000E-01*_st.y) - 5.00000000E-01;
    _AY = (-5.00000000E-01*_st.x + 1.50000000E+00*_st.y) - 5.00000000E-01;
    _BY = (1.50000000E+00*_st.x + 5.00000000E-01*_st.y) - 1.00000000E+00;
    _CY = (5.00000000E-01*_st.x + _st.y) - 5.00000000E-01;
    _c0026 = TEX0.xy + _g1 + _g2;
    _TMP0 = COMPAT_TEXTURE(Texture, _c0026);
    _A = vec3(float(_TMP0.x), float(_TMP0.y), float(_TMP0.z));
    _c0028 = TEX0.xy + _g1;
    _TMP1 = COMPAT_TEXTURE(Texture, _c0028);
    _B = vec3(float(_TMP1.x), float(_TMP1.y), float(_TMP1.z));
    _c0030 = (TEX0.xy + _g1) - _g2;
    _TMP2 = COMPAT_TEXTURE(Texture, _c0030);
    _C = vec3(float(_TMP2.x), float(_TMP2.y), float(_TMP2.z));
    _c0032 = TEX0.xy + _g2;
    _TMP3 = COMPAT_TEXTURE(Texture, _c0032);
    _D = vec3(float(_TMP3.x), float(_TMP3.y), float(_TMP3.z));
    _TMP4 = COMPAT_TEXTURE(Texture, TEX0.xy);
    _E = vec3(float(_TMP4.x), float(_TMP4.y), float(_TMP4.z));
    _c0036 = TEX0.xy - _g2;
    _TMP5 = COMPAT_TEXTURE(Texture, _c0036);
    _F = vec3(float(_TMP5.x), float(_TMP5.y), float(_TMP5.z));
    _c0038 = (TEX0.xy - _g1) + _g2;
    _TMP6 = COMPAT_TEXTURE(Texture, _c0038);
    _G = vec3(float(_TMP6.x), float(_TMP6.y), float(_TMP6.z));
    _c0040 = TEX0.xy - _g1;
    _TMP7 = COMPAT_TEXTURE(Texture, _c0040);
    _H = vec3(float(_TMP7.x), float(_TMP7.y), float(_TMP7.z));
    _c0042 = (TEX0.xy - _g1) - _g2;
    _TMP8 = COMPAT_TEXTURE(Texture, _c0042);
    _I = vec3(float(_TMP8.x), float(_TMP8.y), float(_TMP8.z));
    _c0048 = (TEX0.xy + _g1) - 2.00000000E+00*_g2;
    _TMP11 = COMPAT_TEXTURE(Texture, _c0048);
    _C4 = vec3(float(_TMP11.x), float(_TMP11.y), float(_TMP11.z));
    _c0052 = (TEX0.xy - 2.00000000E+00*_g1) + _g2;
    _TMP13 = COMPAT_TEXTURE(Texture, _c0052);
    _G5 = vec3(float(_TMP13.x), float(_TMP13.y), float(_TMP13.z));
    _TMP16 = dot(vec3(float(_A.x), float(_A.y), float(_A.z)), vec3( 6.55360000E+04, 2.55000000E+02, 1.00000000E+00));
    _TMP15 = float(_TMP16);
    _a = float(_TMP15);
    _TMP16 = dot(vec3(float(_B.x), float(_B.y), float(_B.z)), vec3( 6.55360000E+04, 2.55000000E+02, 1.00000000E+00));
    _TMP15 = float(_TMP16);
    _b = float(_TMP15);
    _TMP16 = dot(vec3(float(_C.x), float(_C.y), float(_C.z)), vec3( 6.55360000E+04, 2.55000000E+02, 1.00000000E+00));
    _TMP15 = float(_TMP16);
    _c = float(_TMP15);
    _TMP16 = dot(vec3(float(_D.x), float(_D.y), float(_D.z)), vec3( 6.55360000E+04, 2.55000000E+02, 1.00000000E+00));
    _TMP15 = float(_TMP16);
    _d = float(_TMP15);
    _TMP16 = dot(vec3(float(_E.x), float(_E.y), float(_E.z)), vec3( 6.55360000E+04, 2.55000000E+02, 1.00000000E+00));
    _TMP15 = float(_TMP16);
    _e = float(_TMP15);
    _TMP16 = dot(vec3(float(_F.x), float(_F.y), float(_F.z)), vec3( 6.55360000E+04, 2.55000000E+02, 1.00000000E+00));
    _TMP15 = float(_TMP16);
    _f = float(_TMP15);
    _TMP16 = dot(vec3(float(_G.x), float(_G.y), float(_G.z)), vec3( 6.55360000E+04, 2.55000000E+02, 1.00000000E+00));
    _TMP15 = float(_TMP16);
    _g = float(_TMP15);
    _TMP16 = dot(vec3(float(_H.x), float(_H.y), float(_H.z)), vec3( 6.55360000E+04, 2.55000000E+02, 1.00000000E+00));
    _TMP15 = float(_TMP16);
    _h = float(_TMP15);
    _TMP16 = dot(vec3(float(_I.x), float(_I.y), float(_I.z)), vec3( 6.55360000E+04, 2.55000000E+02, 1.00000000E+00));
    _TMP15 = float(_TMP16);
    _i = float(_TMP15);
    _TMP16 = dot(vec3(float(_C4.x), float(_C4.y), float(_C4.z)), vec3( 6.55360000E+04, 2.55000000E+02, 1.00000000E+00));
    _TMP15 = float(_TMP16);
    _c4 = float(_TMP15);
    _TMP16 = dot(vec3(float(_G5.x), float(_G5.y), float(_G5.z)), vec3( 6.55360000E+04, 2.55000000E+02, 1.00000000E+00));
    _TMP15 = float(_TMP16);
    _g5 = float(_TMP15);
    _res = vec3(float(_E.x), float(_E.y), float(_E.z));
    _fx_1 = _AO*_fp.y + _BO*_fp.x > _CO;
    _fx_2 = _AX*_fp.y + _BX*_fp.x > _CX;
    _fx_3 = _AY*_fp.y + _BY*_fp.x > _CY;
    _condition_1a = _e == _c && _e == _d && _h == _g;
    _condition_1b = _e == _g && _e == _b && _f == _c;
    _condition_1 = _e != _h && _h == _f && (_e != _i && (_e != _b || _e != _d || _f != _b && _f != _c || _h != _d && _h != _g) || _e == _g && (_i == _h || _e == _d || _h != _d) || _e == _c && (_i == _h || _e == _b || _f != _b));
    _condition_2 = _e != _h && _e != _f && _f != _i && _e == _c && (_h == _i && _f != _b || _e != _i && _f == _c4);
    _condition_3 = _e != _h && _e != _f && _h != _i && _e == _g && (_f == _i && _h != _d || _e != _i && _h == _g5);
    _condition_4 = _e != _h && _g == _e && _e == _c && _e != _i && _e != _f;
    _condition_7 = _e != _f && _f == _b && (_e != _c && (_e != _d || _e != _h || _b != _d && _b != _a || _f != _h && _f != _i) || _e == _i && (_c == _f || _e == _h || _f != _h));
    _condition_8 = _e != _d && _d == _h && (_e != _g && (_e != _f || _e != _b || _h != _f && _h != _i || _d != _b && _d != _a) || _e == _i && (_g == _d || _e == _f || _h != _f));
    _condition_7a = _e == _a && _e == _h && _f == _i;
    _condition_8a = _e == _a && _e == _f && _h == _i;
    if (_condition_1 && (_fx_1 || _condition_1a && _fx_2 || _condition_1b && _fx_3) || _condition_8 && _condition_8a && _BY*_fp.y + _AY*(1.00000000E+00 - _fp.x) > _CY || _fx_1 && _condition_3) { 
        _res = vec3(float(_H.x), float(_H.y), float(_H.z));
    } else {
        if (_condition_7 && _condition_7a && _BX*(1.00000000E+00 - _fp.y) + _AX*_fp.x > _CX || _fx_1 && _condition_2) { 
            _res = vec3(float(_F.x), float(_F.y), float(_F.z));
        } else {
            if (_fx_1 && _condition_4) { 
                _res = vec3(float(((_F + _H)*5.00000000E-01).x), float(((_F + _H)*5.00000000E-01).y), float(((_F + _H)*5.00000000E-01).z));
            } 
        } 
    } 
    _ret_0 = vec4(_res.x, _res.y, _res.z, 1.00000000E+00);
    FragColor = _ret_0;
    return;
} 
#endif
