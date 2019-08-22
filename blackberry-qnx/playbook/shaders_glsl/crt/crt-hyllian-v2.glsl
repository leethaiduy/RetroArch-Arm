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
COMPAT_VARYING     vec2 VARt8;
COMPAT_VARYING     vec4 VARt7;
COMPAT_VARYING     vec4 VARt6;
COMPAT_VARYING     vec4 VARt5;
COMPAT_VARYING     vec4 VARt4;
COMPAT_VARYING     vec4 VARt3;
COMPAT_VARYING     vec4 VARt2;
COMPAT_VARYING     vec4 VARt1;
COMPAT_VARYING     vec2 VARtexCoord;
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
    vec2 VARtexCoord;
    vec4 VARt1;
    vec4 VARt2;
    vec4 VARt3;
    vec4 VARt4;
    vec4 VARt5;
    vec4 VARt6;
    vec4 VARt7;
    vec2 VARt8;
};
vec4 _oPosition1;
out_vertex _ret_0;
input_dummy _IN1;
vec4 _r0022;
COMPAT_ATTRIBUTE vec4 VertexCoord;
COMPAT_ATTRIBUTE vec4 TexCoord;
 
uniform mat4 MVPMatrix;
uniform int FrameDirection;
uniform int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;
void main()
{
    vec2 _ps;
    vec2 _tex;
    out_vertex _TMP17;
    _ps = vec2(1.00000000E+00/TextureSize.x, 1.00000000E+00/TextureSize.y);
    _r0022 = VertexCoord.x*MVPMatrix[0];
    _r0022 = _r0022 + VertexCoord.y*MVPMatrix[1];
    _r0022 = _r0022 + VertexCoord.z*MVPMatrix[2];
    _r0022 = _r0022 + VertexCoord.w*MVPMatrix[3];
    _oPosition1 = _r0022;
    _tex = TexCoord.xy + vec2( 1.00000001E-07, 1.00000001E-07);
    _TMP17.VARt1 = vec4(_tex.x, _tex.y, _tex.x, _tex.y) + vec4(-_ps.x, -_ps.y, 0.00000000E+00, -_ps.y);
    _TMP17.VARt2 = vec4(_tex.x, _tex.y, _tex.x, _tex.y) + vec4(_ps.x, -_ps.y, 2.00000000E+00*_ps.x, -_ps.y);
    _TMP17.VARt3 = vec4(_tex.x, _tex.y, _tex.x, _tex.y) + vec4(-_ps.x, 0.00000000E+00, _ps.x, 0.00000000E+00);
    _TMP17.VARt4 = vec4(_tex.x, _tex.y, _tex.x, _tex.y) + vec4(2.00000000E+00*_ps.x, 0.00000000E+00, -_ps.x, _ps.y);
    _TMP17.VARt5 = vec4(_tex.x, _tex.y, _tex.x, _tex.y) + vec4(0.00000000E+00, _ps.y, _ps.x, _ps.y);
    _TMP17.VARt6 = vec4(_tex.x, _tex.y, _tex.x, _tex.y) + vec4(2.00000000E+00*_ps.x, _ps.y, -_ps.x, 2.00000000E+00*_ps.y);
    _TMP17.VARt7 = vec4(_tex.x, _tex.y, _tex.x, _tex.y) + vec4(0.00000000E+00, 2.00000000E+00*_ps.y, _ps.x, 2.00000000E+00*_ps.y);
    _TMP17.VARt8 = _tex + vec2(2.00000000E+00*_ps.x, 2.00000000E+00*_ps.y);
    VARtexCoord = _tex;
    VARt1 = _TMP17.VARt1;
    VARt2 = _TMP17.VARt2;
    VARt3 = _TMP17.VARt3;
    VARt4 = _TMP17.VARt4;
    VARt5 = _TMP17.VARt5;
    VARt6 = _TMP17.VARt6;
    VARt7 = _TMP17.VARt7;
    VARt8 = _TMP17.VARt8;
    gl_Position = _r0022;
    return;
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
COMPAT_VARYING     vec2 _t8;
COMPAT_VARYING     vec4 _t7;
COMPAT_VARYING     vec4 _t6;
COMPAT_VARYING     vec4 _t5;
COMPAT_VARYING     vec4 _t4;
COMPAT_VARYING     vec4 _t3;
COMPAT_VARYING     vec4 VARt2;
COMPAT_VARYING     vec4 VARt1;
COMPAT_VARYING     vec2 VARtexCoord;
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
    vec2 VARtexCoord;
    vec4 VARt1;
    vec4 VARt2;
    vec4 _t3;
    vec4 _t4;
    vec4 _t5;
    vec4 _t6;
    vec4 _t7;
    vec2 _t8;
};
vec4 _ret_0;
vec3 _TMP20;
float _TMP23;
float _TMP22;
float _TMP21;
float _TMP19;
float _TMP24;
vec4 _TMP3;
vec4 _TMP2;
vec4 _TMP1;
vec4 _TMP0;
out_vertex _VAR1;
uniform sampler2D Texture;
input_dummy _IN1;
vec2 _x0036;
vec4 _C0070;
vec4 _C0080;
vec4 _C0090;
vec4 _C0100;
float _a0110;
float _x0112;
float _TMP115;
vec3 _a0122;
vec4 _TMP137;
vec4 _TMP138;
vec4 _TMP139;
vec4 _TMP140;
vec4 _TMP141;
vec4 _TMP142;
vec4 _TMP143;
 
uniform int FrameDirection;
uniform int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;
void main()
{
    vec2 _fp;
    float _d;
    vec3 _color;
    vec4 _TMP29;
    vec4 _TMP31[4];
    vec4 _TMP32[4];
    vec4 _TMP33[4];
    _x0036 = VARtexCoord*TextureSize;
    _fp = fract(_x0036);
    _TMP0 = COMPAT_TEXTURE(Texture, VARt1.xy);
    _TMP1 = COMPAT_TEXTURE(Texture, VARt1.zw);
    _TMP2 = COMPAT_TEXTURE(Texture, VARt2.xy);
    _TMP3 = COMPAT_TEXTURE(Texture, VARt2.zw);
    _TMP33[0] = vec4(_TMP0.x, _TMP1.x, _TMP2.x, _TMP3.x);
    _TMP32[0] = vec4(_TMP0.y, _TMP1.y, _TMP2.y, _TMP3.y);
    _TMP31[0] = vec4(_TMP0.z, _TMP1.z, _TMP2.z, _TMP3.z);
    _TMP29[0] = _fp.x*_fp.x*_fp.x;
    _TMP29[1] = _fp.x*_fp.x;
    _TMP29[2] = _fp.x;
    _TMP137.x = _TMP29[0];
    _TMP137.y = _TMP29[1];
    _TMP137.z = _TMP29[2];
    _TMP137.w = 1.00000000E+00;
    _C0070[0] = dot(vec4( -1.66666672E-01, 5.00000000E-01, -3.33333343E-01, 0.00000000E+00), _TMP137);
    _TMP138.x = _TMP29[0];
    _TMP138.y = _TMP29[1];
    _TMP138.z = _TMP29[2];
    _TMP138.w = 1.00000000E+00;
    _C0070[1] = dot(vec4( 5.00000000E-01, -1.00000000E+00, -5.00000000E-01, 1.00000000E+00), _TMP138);
    _TMP139.x = _TMP29[0];
    _TMP139.y = _TMP29[1];
    _TMP139.z = _TMP29[2];
    _TMP139.w = 1.00000000E+00;
    _C0070[2] = dot(vec4( -5.00000000E-01, 5.00000000E-01, 1.00000000E+00, 0.00000000E+00), _TMP139);
    _TMP140.x = _TMP29[0];
    _TMP140.y = _TMP29[1];
    _TMP140.z = _TMP29[2];
    _TMP140.w = 1.00000000E+00;
    _C0070[3] = dot(vec4( 1.66666672E-01, 0.00000000E+00, -1.66666672E-01, 0.00000000E+00), _TMP140);
    _TMP141.x = _C0070[0];
    _TMP141.y = _C0070[1];
    _TMP141.z = _C0070[2];
    _TMP141.w = _C0070[3];
    _C0080[0] = dot(_TMP33[0], _TMP141);
    _TMP142.x = _C0070[0];
    _TMP142.y = _C0070[1];
    _TMP142.z = _C0070[2];
    _TMP142.w = _C0070[3];
    _C0090[0] = dot(_TMP32[0], _TMP142);
    _TMP143.x = _C0070[0];
    _TMP143.y = _C0070[1];
    _TMP143.z = _C0070[2];
    _TMP143.w = _C0070[3];
    _C0100[0] = dot(_TMP31[0], _TMP143);
    _a0110 = _fp.y - 5.00000000E-01;
    _TMP19 = abs(_a0110);
    _x0112 = 1.00000000E+00 - _TMP19;
    _TMP24 = min(1.00000000E+00, _x0112);
    _TMP115 = max(0.00000000E+00, _TMP24);
    _d = _TMP115*_TMP115*(3.00000000E+00 - 2.00000000E+00*_TMP115);
    _a0122 = vec3(_C0080[0], _C0090[0], _C0100[0])*_d;
    _TMP21 = pow(_a0122.x, 2.40000010E+00);
    _TMP22 = pow(_a0122.y, 2.40000010E+00);
    _TMP23 = pow(_a0122.z, 2.40000010E+00);
    _color = vec3(_TMP21, _TMP22, _TMP23);
    _TMP21 = pow(_color.x, 4.54545438E-01);
    _TMP22 = pow(_color.y, 4.54545438E-01);
    _TMP23 = pow(_color.z, 4.54545438E-01);
    _TMP20 = vec3(_TMP21, _TMP22, _TMP23);
    _ret_0 = vec4(_TMP20.x, _TMP20.y, _TMP20.z, 1.00000000E+00);
    FragColor = _ret_0;
    return;
} 
#endif