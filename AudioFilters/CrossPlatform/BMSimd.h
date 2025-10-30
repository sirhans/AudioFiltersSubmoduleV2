
#ifndef BM_SIMD_H
#define BM_SIMD_H

//#ifdef __cplusplus
//extern "C" {
//#endif

#include <math.h>
#include <algorithm.h>
#include "../Constants.h"

typedef float simd_float1;

typedef struct {
    double x;
    double y;
} simd_double2;

typedef struct simd_float2{
    float x;
    float y;
    
    simd_float2() : x(0),y(0){}
    simd_float2(float v) : x(v),y(v){}
    simd_float2(float v1,float v2) : x(v1),y(v2){}
    
    simd_float2 operator=(const simd_float2& rhs){
        return {rhs.x,rhs.y};
    }
    simd_float2 operator*(const simd_float2& rhs){
        return {this->x * rhs.x,this->y * rhs.y};
    }
    simd_float2 operator*(float value){
        return {this->x * value,this->y * value};
    }
    simd_float2 operator+(const simd_float2& rhs){
        return {this->x + rhs.x,this->y + rhs.y};
    }
    simd_float2 operator+(const float value){
        return {this->x + value,this->y + value};
    }
    simd_float2 operator-(const simd_float2& rhs){
        return {this->x - rhs.x,this->y - rhs.y};
    }
    bool operator==(const simd_float2& rhs){
        return this->x == rhs.x && this->y == rhs.y;
    }
    
    float& operator[](int index){
        if(index == 0)
            return x;
        else
            return y;
    }
    
} simd_float2;

typedef struct simd_float3{
    float x;
    float y;
    float z;
    
    simd_float3() : x(0),y(0),z(0){}
    simd_float3(float v) : x(v),y(v),z(v){}
    simd_float3(float v1,float v2,float v3) : x(v1),y(v2),z(v3){}
    
    simd_float3 operator*(const simd_float3& rhs){
        return {this->x * rhs.x,this->y * rhs.y,this->z * rhs.z};
    }
    simd_float3 operator*(float v){
        return {this->x * v,this->y * v,this->z * v};
    }
    simd_float3 operator+(const simd_float3& rhs){
        return {this->x + rhs.x,this->y + rhs.y,this->z + rhs.z};
    }
    simd_float3 operator+(float value){
        return {this->x + value,this->y + value,this->z + value};
    }
    simd_float3 operator-(float value){
        return {this->x - value,this->y - value,this->z - value};
    }
    simd_float3 operator-(const simd_float3& rhs){
        return {this->x - rhs.x,this->y - rhs.y,this->z - rhs.z};
    }
} simd_float3;


typedef struct simd_float4 {
    float x = 0;
    float y = 0;
    float z = 0;
    float w = 0;
    
    simd_float4() : x(0),y(0),z(0),w(0){}
    simd_float4(float v) : x(v),y(v),z(v),w(v){}
    simd_float4(float v1,float v2,float v3,float v4) : x(v1),y(v2),z(v3),w(v4){}
    
    simd_float4 operator+(const simd_float4& rhs){
        return {this->x + rhs.x,this->y + rhs.y,this->z + rhs.z,this->w + rhs.w};
    }
    simd_float4 operator+(float v){
        return {this->x + v,this->y + v,this->z + v,this->w + v};
    }
    simd_float4 operator*(const simd_float4& rhs){
        return {this->x * rhs.x,this->y * rhs.y,this->z * rhs.z,this->w * rhs.w};
    }
    simd_float4 operator*(float mul){
        return {this->x * mul,this->y * mul,this->z * mul,this->w * mul};
    }
    simd_float4 operator/(const simd_float4& rhs){
        return {this->x / rhs.x,this->y / rhs.y,this->z / rhs.z,this->w / rhs.w};
    }
    
    simd_float4 operator-(const simd_float4& rhs){
        return {this->x - rhs.x,this->y - rhs.y,this->z - rhs.z,this->w - rhs.w};
    }
    
    float& operator[](int index){
        if(index == 0){
            return x;
        }else if(index == 1){
            return y;
        }else if(index == 2){
            return z;
        }else{
            return w;
        }
    }
} simd_float4;

static inline simd_float4 simd_make_float4(float v){
    return {v,v,v,v};
}

static inline simd_float4 simd_make_float4(float x,float y,float z,float w){
    return {x,y,z,w};
}

static inline simd_float2 simd_make_float2(float x,float y){
    return {x,y};
}

static inline simd_float3 simd_make_float3(float x,float y,float z){
    return {x,y,z};
}

static inline simd_float2 simd_float4_addXYToZW(simd_float4& rhs){
    return {rhs.x + rhs.z,rhs.y + rhs.w};
}

static inline float limit(float v,float min,float max){
    float value = v;
    if(value < min)
        value = min;
    if(value > max)
        value = max;
    return value;
}

static inline simd_float4 simd_clamp(const simd_float4& v,float min,float max){
    return {limit(v.x, min, max),limit(v.y, min, max),limit(v.z, min, max),limit(v.w, min, max)};
}

static inline simd_float3 simd_clamp(simd_float3& v,float min,float max){
    return {limit(v.x, min, max),limit(v.y, min, max),limit(v.z, min, max)};
}

static inline simd_float2 simd_clamp(simd_float2& v,float min,float max){
    return {limit(v.x, min, max),limit(v.y, min, max)};
}

static inline float simd_clamp(float v,float min,float max){
    return limit(v,min,max);
}

static inline simd_float2 simd_min(simd_float2 v1,simd_float2 v2){
    return {MIN(v1.x,v2.x),MIN(v1.y,v2.y)};
}

static inline float simd_min(float v1,float v2){
    return MIN(v1,v2);
}

static inline simd_float4 simd_min(simd_float4 v1,simd_float4 v2){
    return {MIN(v1.x,v2.x),MIN(v1.y,v2.y),MIN(v1.z,v2.z),MIN(v1.w,v2.w)};
}

static inline simd_float2 simd_max(simd_float2 v1,simd_float2 v2){
    return {MAX(v1.x,v2.x),MAX(v1.y,v2.y)};
}

static inline float simd_max(float v1,float v2){
    return MAX(v1,v2);
}

static inline simd_float4 simd_max(simd_float4 v1,simd_float4 v2){
    return {MAX(v1.x,v2.x),MAX(v1.y,v2.y),MAX(v1.z,v2.z),MAX(v1.w,v2.w)};
}

static inline float simd_fract(float v){
    return v - floorf(v);
}

static inline simd_float3 simd_fract(simd_float3 v){
    return {v.x - floorf(v.x),v.y - floorf(v.y),v.z - floorf(v.z)};
}

static inline simd_float2 simd_abs(simd_float2 v){
    return {fabsf(v.x),fabsf(v.y)};
}

static inline simd_float3 simd_abs(simd_float3 v){
    return {fabsf(v.x),fabsf(v.y),fabsf(v.z)};
}

static inline simd_float4 simd_abs(simd_float4 v){
    return {fabsf(v.x),fabsf(v.y),fabsf(v.z),fabsf(v.w)};
}

static inline float simd_smoothstep(float edge0, float edge1, float x) {
    // Scale, bias and saturate x to 0..1 range
    x = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    // Evaluate polynomial
    return x * x * (3 - 2 * x);
}

static inline simd_float4 simd_smoothstep(simd_float4 edge0,simd_float4 edge1,simd_float4 v){
    return {simd_smoothstep(edge0.x, edge1.x, v.x),
        simd_smoothstep(edge0.y, edge1.y, v.y),
        simd_smoothstep(edge0.z, edge1.z, v.z),
        simd_smoothstep(edge0.w, edge1.w, v.w)};
}

typedef struct {
    simd_float2 columns[2];
} simd_float2x2;
typedef struct {
    simd_double2 columns[2];
} simd_double2x2;

static inline bool simd_equal(simd_float2x2 v1,simd_float2x2 v2){
    return v1.columns[0] == v2.columns[0] &&
        v1.columns[1] == v2.columns[1];
}

/*Review: */
static inline simd_float2 simd_mul(simd_float2x2 matrix,simd_float2 v){
    return {matrix.columns[0].x * v.x + matrix.columns[0].y * v.y,
        matrix.columns[1].x * v.x + matrix.columns[1].y * v.y};
}

static inline float simd_dot(simd_float3 v1,simd_float3 v2){
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

static inline float simd_dot(simd_float4 v1,simd_float4 v2){
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w;
}

//    typedef float simd_matrix;
static inline simd_float2x2 simd_matrix(simd_float2  col0, simd_float2  col1) {
    simd_float2x2 r;
    r.columns[0] = col0;
    r.columns[1] = col1;
    return r;
}

static inline simd_double2x2 simd_matrix(simd_double2 col0, simd_double2 col1) { 
    simd_double2x2 r;
    r.columns[0] = col0;
    r.columns[1] = col1;
    return r;
}

//#ifdef __cplusplus
//}
//#endif

#endif
