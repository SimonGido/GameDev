#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38

vec4 EulerToQuat(in vec3 eulerAngles)
{
    vec4 result;
    vec3 c = vec3(cos(eulerAngles.x * 0.5),
                  cos(eulerAngles.y * 0.5),
                  cos(eulerAngles.z * 0.5));

    vec3 s = vec3(sin(eulerAngles.x * 0.5),
                  sin(eulerAngles.y * 0.5),
                  sin(eulerAngles.z * 0.5));

    result.w = c.x * c.y * c.z + s.x * s.y * s.z;
	result.x = s.x * c.y * c.z - c.x * s.y * s.z;
	result.y = c.x * s.y * c.z + s.x * c.y * s.z;
	result.z = c.x * c.y * s.z - s.x * s.y * c.z;
    return result;
}

mat4 QuatToMat4(in vec4 q)
{
    mat4 result;
    float qxx = q.x * q.x;
	float qyy = q.y * q.y;
	float qzz = q.z * q.z;
	float qxz = q.x * q.z;
	float qxy = q.x * q.y;
	float qyz = q.y * q.z;
	float qwx = q.w * q.x;
	float qwy = q.w * q.y;
	float qwz = q.w * q.z;

    result[0][0] = 1.0 - 2.0 * (qyy +  qzz);
	result[0][1] = 2.0 * (qxy + qwz);
	result[0][2] = 2.0 * (qxz - qwy);

	result[1][0] = 2.0 * (qxy - qwz);
	result[1][1] = 1.0 - 2.0 * (qxx +  qzz);
	result[1][2] = 2.0 * (qyz + qwx);

	result[2][0] = 2.0 * (qxz + qwy);
	result[2][1] = 2.0 * (qyz - qwx);
	result[2][2] = 1.0 - 2.0 * (qxx +  qyy);

    result[0][3] = result[1][3] = result[2][3] = result[3][0] = result[3][1] = result[3][2] = 0;
    result[3][3] = 1;
    return result;
}

mat4 TranslationMatrix(in vec3 translation)
{
    return mat4(
        vec4(1.0, 0.0, 0.0, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(translation, 1.0));
}

mat4 TranslationMatrix(in vec3 translation, in vec3 scale)
{
    return mat4(
        vec4(scale.x, 0.0, 0.0, 0.0),
        vec4(0.0, scale.y, 0.0, 0.0),
        vec4(0.0, 0.0, scale.z, 0.0),
        vec4(translation, 1.0));
}

mat4 RotationMatrix(in vec3 eulerAngles)
{
    vec4 quat = EulerToQuat(eulerAngles);
    return QuatToMat4(quat);
}

mat4 RotationMatrix(in vec4 quat)
{
    return QuatToMat4(quat);
}

mat4 TransformFromRows(in vec4 transformRow0, in vec4 transformRow1, in vec4 transformRow2)
{
    return mat4(
		vec4(transformRow0.x, transformRow1.x, transformRow2.x, 0.0),
		vec4(transformRow0.y, transformRow1.y, transformRow2.y, 0.0),
		vec4(transformRow0.z, transformRow1.z, transformRow2.z, 0.0),
		vec4(transformRow0.w, transformRow1.w, transformRow2.w, 1.0)
	);
}

bool AABBCollisionAABB(vec3 min0, vec3 max0, vec3 min1, vec3 max1)
{
    float d1x = min1.x - max0.x;
    float d1y = min1.y - max0.y;
    float d2x = min0.x - max1.x;
    float d2y = min0.y - max1.y;
    
    if (d1x > 0.0 || d1y > 0.0)
    	return false;
    
    if (d2x > 0.0 || d2y > 0.0)
    	return false;
    
    return true;
}

float SQDistance(float v, float min, float max)
{
    float sqDist = 0.0;
    if (v < min) 
        sqDist += (min - v) * (min - v);

    if (v > max) 
        sqDist += (v - max) * (v - max);

    return sqDist;
}

bool AABBCollisionSphere(vec3 min, vec3 max, vec3 center, float radius)
{
    float sqDist = 0.0;

    sqDist += SQDistance(center.x, min.x, max.x);
    sqDist += SQDistance(center.y, min.y, max.y);
    sqDist += SQDistance(center.z, min.z, max.z);

    return sqDist <= radius * radius;
}

int ComputeBinominal(int n, int k)
{
    if (n == k)
        return 1;
 
    int value = 1;
    for (int i = 1; i <= k; i++)
    {
        value = value * ((n + 1 - i) / i);
    }     
    return value;
}


vec2 ComputeBezierCurveCubic(vec2 p[4], float t)
{
    vec2 result = 
            pow((1.0 - t), 3.0) * p[0] 
    + 3.0 * pow((1.0 - t), 2.0) * t * p[1] 
    + 3.0 * pow((1.0 - t), 1.0) * pow(t, 2.0) * p[2] 
    +       pow(t, 3.0) * p[3];

    return result;
}


const float PHI = 1.61803398874989484820459;  // Golden Ratio   

float GoldNoise(in vec2 xy, in float seed)
{
    return fract(tan(distance(xy*PHI, xy)*seed)*xy.x);
}

vec3 RandomValue(in vec2 xy, in float seed)
{
    vec3 result;
    result.x = GoldNoise(xy, seed);
    result.y = GoldNoise(xy, result.x + seed);
    result.z = GoldNoise(xy, result.y + seed);
    return result;
}


bool RayBoxIntersection(vec3 origin, vec3 direction, vec3 lb, vec3 rt, out float tnear, out float tfar)
{
	vec3 dirfrac = 1.0 / direction;
    // lb is the corner of AABB with minimal coordinates - left bottom, rt is maximal corner
    // r.org is origin of ray
   
    float t1 = (lb.x - origin.x) * dirfrac.x;
    float t2 = (rt.x - origin.x) * dirfrac.x;
    float t3 = (lb.y - origin.y) * dirfrac.y;
    float t4 = (rt.y - origin.y) * dirfrac.y;
    float t5 = (lb.z - origin.z) * dirfrac.z;
    float t6 = (rt.z - origin.z) * dirfrac.z;

    tnear = max(max(min(t1, t2), min(t3, t4)), min(t5, t6));
    tfar = min(min(max(t1, t2), max(t3, t4)), max(t5, t6));

    // if tmax < 0, ray (line) is intersecting AABB, but the whole AABB is behind us
    if (tfar < 0.0)
    { 
        return false;
    }
    // if tmin > tmax, ray doesn't intersect AABB
    if (tnear > tfar)
    {
        return false;
    }
	return true;
}

bool RayAABBOverlap(vec3 rayOrigin, vec3 rayDirection, vec3 boxMin, vec3 boxMax)
{
    vec3 invRayDir = 1.0 / rayDirection;  // Inverse ray direction

    // Compute t-values for each pair of min and max planes
    vec3 tMin = (boxMin - rayOrigin) * invRayDir;
    vec3 tMax = (boxMax - rayOrigin) * invRayDir;

    // Find the largest tMin and the smallest tMax
    vec3 tEnter = min(tMin, tMax);
    vec3 tExit = max(tMin, tMax);

    float tNear = max(max(tEnter.x, tEnter.y), tEnter.z);
    float tFar = min(min(tExit.x, tExit.y), tExit.z);

    // Check for overlap
    return tNear <= tFar;
}

bool PointInBox(vec3 point, vec3 boxMin, vec3 boxMax)
{
	return
		point.x >= boxMin.x
		&& point.x <= boxMax.x
		&& point.y >= boxMin.y
		&& point.y <= boxMax.y
		&& point.z >= boxMin.z
		&& point.z <= boxMax.z;
}

float SDFBox (vec3 p, vec3 c, vec3 s)
{
    float x = max
    (   p.x - c.x - s.x / 2.0,
        c.x - p.x - s.x / 2.0
    );
    float y = max
    (   p.y - c.y - s.y / 2.0,
        c.y - p.y - s.y / 2.0
    );
    
    float z = max
    (   p.z - c.z - s.z / 2.0,
        c.z - p.z - s.z / 2.0
    );
    float d = x;
    d = max(d,y);
    d = max(d,z);
    return d;
}

vec3 GetGradient(float value)
{
	vec3 zero = vec3(0.0, 0.0, 0.0);
	vec3 white = vec3(0.0, 0.1, 0.9);
	vec3 red = vec3(0.2, 0.9, 0.4);
	vec3 blue = vec3(0.8, 0.8, 0.3);
	vec3 green = vec3(0.9, 0.2, 0.3);

	float step0 = 0.0f;
	float step1 = 2.0f;
	float step2 = 4.0f;
	float step3 = 8.0f;
	float step4 = 16.0f;

	vec3 color = mix(zero, white, smoothstep(step0, step1, value));
	color = mix(color, white, smoothstep(step1, step2, value));
	color = mix(color, red, smoothstep(step1, step2, value));
	color = mix(color, blue, smoothstep(step2, step3, value));
	color = mix(color, green, smoothstep(step3, step4, value));

	return color;
}

void TransformAABB(in mat4 transform, inout vec3 aabbMin, inout vec3 aabbMax)
{
    vec4 corners[8] = {
			vec4(aabbMin.x, aabbMin.y, aabbMin.z, 1.0), // x y z
			vec4(aabbMax.x, aabbMin.y, aabbMin.z, 1.0), // X y z
			vec4(aabbMin.x, aabbMax.y, aabbMin.z, 1.0), // x Y z
			vec4(aabbMax.x, aabbMax.y, aabbMin.z, 1.0), // X Y z

			vec4(aabbMin.x, aabbMin.y, aabbMax.z, 1.0), // x y Z
			vec4(aabbMax.x, aabbMin.y, aabbMax.z, 1.0), // X y Z
			vec4(aabbMin.x, aabbMax.y, aabbMax.z, 1.0), // x Y Z
			vec4(aabbMax.x, aabbMax.y, aabbMax.z, 1.0), // X Y Z
		};

    aabbMin = vec3(FLT_MAX, FLT_MAX, FLT_MAX);
    aabbMax = vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (int i = 0; i < 8; i++)
    {
        corners[i] = transform * corners[i];
        aabbMin.x = min(aabbMin.x, corners[i].x);
		aabbMin.y = min(aabbMin.y, corners[i].y);
		aabbMin.z = min(aabbMin.z, corners[i].z);

		aabbMax.x = max(aabbMax.x, corners[i].x);
		aabbMax.y = max(aabbMax.y, corners[i].y);
		aabbMax.z = max(aabbMax.z, corners[i].z);
    }
}