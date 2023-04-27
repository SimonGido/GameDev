//#type compute
#version 460
#extension GL_EXT_shader_8bit_storage : enable

#include "Resources/Shaders/Includes/Math.glsl"

#define TILE_SIZE 16
#define MAX_COLORS 1024

const float FLT_MAX = 3.402823466e+38;
const float EPSILON = 0.01;
const uint OPAQUE = 255;

struct VoxelModel
{
	mat4  InverseModelView;
	mat4  InverseTransform;
	vec4  RayOrigin;

	uint  VoxelOffset;
	uint  Width;
	uint  Height;
	uint  Depth;
	uint  ColorIndex;

	float VoxelSize;
	bool  OriginInside;

	uint Padding[1];
};

layout (std140, binding = 16) uniform Scene
{
	// Camera info
	mat4 u_InverseProjection;
	mat4 u_InverseView;	
	mat4 u_InverseLightView;
	vec4 u_CameraPosition;
	vec4 u_ViewportSize;

	// Light info
	vec4 u_LightPosition;
	vec4 u_LightDirection;
	vec4 u_LightColor;
	uint MaxTraverse;
};


layout(std430, binding = 17) readonly buffer buffer_Voxels
{		
	uint8_t Voxels[];
};

layout(std430, binding = 18) readonly buffer buffer_Models
{		
	uint NumModels;
	VoxelModel Models[];
};

layout(std430, binding = 19) readonly buffer buffer_Colors
{		
	uint ColorPallete[MAX_COLORS][256];
};

layout(binding = 22, r32f) uniform image2D o_ShadowImage;
layout(binding = 23, rgba32f) uniform image2D o_ShadowDebugImage;

struct Ray
{
	vec3 Origin;
	vec3 Direction;
};

Ray g_ModelRay;

Ray CreateRay(vec2 coords, uint modelIndex)
{
	coords.x /= u_ViewportSize.x;
	coords.y /= u_ViewportSize.y;
	coords = coords * 2.0 - 1.0; // -1 -> 1
	vec4 target = u_InverseProjection * vec4(coords.x, -coords.y, 1, 1);
	Ray ray;
	ray.Origin = (u_LightPosition).xyz;

	ray.Direction = vec3(Models[modelIndex].InverseTransform * u_InverseLightView * vec4(normalize(vec3(target) / target.w), 0)); // World space
	ray.Direction = normalize(ray.Direction);

	return ray;
}


uint Index3D(int x, int y, int z, uint width, uint height)
{
	return x + width * (y + height * z);
}

uint Index3D(ivec3 index, uint width, uint height)
{
	return Index3D(index.x, index.y, index.z, width, height);
}


bool IsValidVoxel(ivec3 voxel, uint width, uint height, uint depth)
{
	return ((voxel.x < width && voxel.x > 0)
		 && (voxel.y < height && voxel.y > 0)
		 && (voxel.z < depth && voxel.z > 0));
}

vec4 VoxelToColor(uint voxel)
{
	vec4 color;
	color.x = bitfieldExtract(voxel, 0, 8) / 255.0;
	color.y = bitfieldExtract(voxel, 8, 8) / 255.0;
	color.z = bitfieldExtract(voxel, 16, 8) / 255.0;
	color.w = bitfieldExtract(voxel, 24, 8) / 255.0;

	return color;
}

uint VoxelAlpha(uint voxel)
{
	return bitfieldExtract(voxel, 24, 8);
}

struct BoxIntersectionResult
{
	float T;
	bool Hit;
};

BoxIntersectionResult RayBoxIntersection(vec3 origin, vec3 direction, vec3 lb, vec3 rt)
{
	BoxIntersectionResult result;
	result.Hit = false;
	vec3 dirfrac;
    // r.dir is unit direction vector of ray
    dirfrac.x = 1.0 / direction.x;
    dirfrac.y = 1.0 / direction.y;
    dirfrac.z = 1.0 / direction.z;
    // lb is the corner of AABB with minimal coordinates - left bottom, rt is maximal corner
    // r.org is origin of ray
    float t1 = (lb.x - origin.x) * dirfrac.x;
    float t2 = (rt.x - origin.x) * dirfrac.x;
    float t3 = (lb.y - origin.y) * dirfrac.y;
    float t4 = (rt.y - origin.y) * dirfrac.y;
    float t5 = (lb.z - origin.z) * dirfrac.z;
    float t6 = (rt.z - origin.z) * dirfrac.z;

    float tmin = max(max(min(t1, t2), min(t3, t4)), min(t5, t6));
    float tmax = min(min(max(t1, t2), max(t3, t4)), max(t5, t6));

    // if tmax < 0, ray (line) is intersecting AABB, but the whole AABB is behind us
    if (tmax < 0.0)
    {
        result.T = tmax;
        result.Hit = false;
		return result;
    }

    // if tmin > tmax, ray doesn't intersect AABB
    if (tmin > tmax)
    {
        result.T = tmax;
        result.Hit = false;
		return result;
    }
    result.T = tmin;
	result.Hit = true;
	return result;
}

float VoxelDistanceFromRay(vec3 origin, vec3 direction, ivec3 voxel, float voxelSize)
{
	vec3 boxMin = vec3(voxel.x * voxelSize, voxel.y * voxelSize, voxel.z * voxelSize);
	vec3 boxMax = boxMin + voxelSize;

	return RayBoxIntersection(origin, direction, boxMin, boxMax).T;
}

bool DepthTest(ivec3 voxel, float voxelSize, float currentDepth, out float newDepth)
{
	newDepth = VoxelDistanceFromRay(g_ModelRay.Origin, g_ModelRay.Direction, voxel, voxelSize);		
	return newDepth < currentDepth;
}

struct RaymarchHitResult
{
	vec4  Color;
	vec3  T_Max;
	ivec3 CurrentVoxel;
	uint  Alpha;
	uint  TraverseCount;
	bool  Hit;
	float Depth;
};

struct RaymarchResult
{
	vec4  Color;
	float Distance;
	bool  OpaqueHit;
	bool  Hit;
};

RaymarchHitResult RayMarch(vec3 t_max, vec3 t_delta, ivec3 current_voxel, ivec3 step, uint maxTraverses, uint modelIndex, float currentDepth)
{
	RaymarchHitResult result;
	result.Hit = false;

	uint width = Models[modelIndex].Width;
	uint height = Models[modelIndex].Height;
	uint depth = Models[modelIndex].Depth;
	uint voxelOffset = Models[modelIndex].VoxelOffset;
	uint colorPalleteIndex = Models[modelIndex].ColorIndex;
	float voxelSize = Models[modelIndex].VoxelSize;
	
	uint i = 0;
	for (i = 0; i < maxTraverses; i++)
	{
		if (t_max.x < t_max.y && t_max.x < t_max.z) 
		{
			t_max.x += t_delta.x;
			current_voxel.x += step.x;
		}
		else if (t_max.y < t_max.z) 
		{
			t_max.y += t_delta.y;
			current_voxel.y += step.y;			
		}
		else 
		{
			t_max.z += t_delta.z;
			current_voxel.z += step.z;		
		}

		if (IsValidVoxel(current_voxel, width, height, depth))
		{
			if (!DepthTest(current_voxel, voxelSize, currentDepth, result.Depth))
				break;

			uint voxelIndex = Index3D(current_voxel, width, height) + voxelOffset;
			uint colorIndex = uint(Voxels[voxelIndex]);
			uint voxel = ColorPallete[colorPalleteIndex][colorIndex];

			if (voxel != 0)
			{
				result.Color = VoxelToColor(voxel);
				result.Alpha = VoxelAlpha(voxel);
				result.Hit = true;				
				break;
			}
		}
	}
	result.TraverseCount = i;
	result.T_Max = t_max;
	result.CurrentVoxel = current_voxel;

	return result;
}

RaymarchResult RayMarch(uint modelIndex, float currentDepth)
{
	RaymarchResult result;
	result.OpaqueHit = false;
	result.Hit = false;
	result.Distance = FLT_MAX;
	
	vec3 origin = g_ModelRay.Origin;
	vec3 direction = g_ModelRay.Direction;

	float width  = float(Models[modelIndex].Width);
	float height = float(Models[modelIndex].Height);
	float depth  = float(Models[modelIndex].Depth);
	float voxelSize = Models[modelIndex].VoxelSize;
	

	vec3 boxMin = vec3(0,0,0);
	vec3 boxMax = vec3(width, height, depth) * voxelSize;
	// Check if we are intersecting with grid
	BoxIntersectionResult boxIntersection = RayBoxIntersection(origin, direction, boxMin, boxMax);
	if (!boxIntersection.Hit)
		return result; // No intersection


	ivec3 current_voxel = ivec3(floor(origin / voxelSize));
	
	ivec3 step = ivec3(
		(direction.x > 0.0) ? 1 : -1,
		(direction.y > 0.0) ? 1 : -1,
		(direction.z > 0.0) ? 1 : -1
	);
	vec3 next_boundary = vec3(
		float((step.x > 0) ? current_voxel.x + 1 : current_voxel.x) * voxelSize,
		float((step.y > 0) ? current_voxel.y + 1 : current_voxel.y) * voxelSize,
		float((step.z > 0) ? current_voxel.z + 1 : current_voxel.z) * voxelSize
	);
	

	vec3 t_max = (next_boundary - origin) / direction; // we will move along the axis with the smallest value
	vec3 t_delta = voxelSize / direction * vec3(step);	


	uint remainingTraverses = MaxTraverse;
	
	// Raymarch until we find first hit to determine default color
	RaymarchHitResult hitResult = RayMarch(t_max, t_delta, current_voxel, step, remainingTraverses, modelIndex, currentDepth);	
	remainingTraverses -= hitResult.TraverseCount;


	float newDepth = hitResult.Depth;
	if (newDepth > currentDepth) // Depth test
		return result;

	// If we hit something it is our default color
	if (hitResult.Hit)
	{
		result.Color = hitResult.Color;
		result.Hit = true;
		result.OpaqueHit = hitResult.Alpha == OPAQUE;
		result.Distance = newDepth;
	}
	
	// Continue raymarching until we hit opaque object or we are out of traverses
	while (remainingTraverses != 0)
	{		
		if (result.OpaqueHit) // Opaque hit => stop raymarching
			break;

		hitResult = RayMarch(hitResult.T_Max, t_delta, hitResult.CurrentVoxel, step, remainingTraverses, modelIndex, currentDepth);	
		newDepth = hitResult.Depth; // Store raymarch distance
		if (newDepth > currentDepth) // if new depth is bigger than currentDepth it means there is something in front of us
			break;
		
		// We passed depth test
		if (hitResult.Hit) // We hit something so mix colors together
		{
			result.Color.rgb = mix(result.Color.rgb, hitResult.Color.rgb, 1.0 - result.Color.a);
			result.Hit = true;
			result.OpaqueHit = hitResult.Alpha == OPAQUE;
		}
		
		remainingTraverses -= hitResult.TraverseCount;
	}
	result.Distance = newDepth;
	return result;
}


bool ValidPixel(ivec2 index)
{
	return index.x <= int(u_ViewportSize.x) && index.y <= int(u_ViewportSize.y);
}


layout(local_size_x = TILE_SIZE, local_size_y = TILE_SIZE, local_size_z = 1) in;
void main() 
{
	ivec2 textureIndex = ivec2(gl_GlobalInvocationID.xy);
	if (!ValidPixel(textureIndex))
		return;

	imageStore(o_ShadowImage, textureIndex, vec4(FLT_MAX,0,0,0));
	imageStore(o_ShadowDebugImage, textureIndex, vec4(0,0,0,0));

	for (uint i = 0; i < NumModels; i++)
	{
		g_ModelRay = CreateRay(textureIndex, i);
		float currentDepth = FLT_MAX;
		RaymarchResult result = RayMarch(i, currentDepth);

		if (result.Hit)
		{			
			if (result.OpaqueHit)
			{			
				imageStore(o_ShadowImage, textureIndex, vec4(result.Distance, 0,0,0)); // Store new depth
			}		
			imageStore(o_ShadowDebugImage, textureIndex, result.Color); // Store new depth
		}
	}
}