// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   DXGL_D3D11.h
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Contains portable definition of structs and enums to match
//               those in D3D11.h in the DirectX SDK
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __DXGL_D3D11_h__
#define __DXGL_D3D11_h__

#include "DXGL_dxgi.h"
#include "DXGL_D3DCommon.h"

////////////////////////////////////////////////////////////////////////////
//  Defines
////////////////////////////////////////////////////////////////////////////

#ifndef _D3D11_CONSTANTS
	#define _D3D11_CONSTANTS
	#define D3D11_16BIT_INDEX_STRIP_CUT_VALUE                                    (0xffff)

	#define D3D11_32BIT_INDEX_STRIP_CUT_VALUE                                    (0xffffffff)

	#define D3D11_8BIT_INDEX_STRIP_CUT_VALUE                                     (0xff)

	#define D3D11_ARRAY_AXIS_ADDRESS_RANGE_BIT_COUNT                             (9)

	#define D3D11_CLIP_OR_CULL_DISTANCE_COUNT                                    (8)

	#define D3D11_CLIP_OR_CULL_DISTANCE_ELEMENT_COUNT                            (2)

	#define D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT                    (14)

	#define D3D11_COMMONSHADER_CONSTANT_BUFFER_COMPONENTS                        (4)

	#define D3D11_COMMONSHADER_CONSTANT_BUFFER_COMPONENT_BIT_COUNT               (32)

	#define D3D11_COMMONSHADER_CONSTANT_BUFFER_HW_SLOT_COUNT                     (15)

	#define D3D11_COMMONSHADER_CONSTANT_BUFFER_REGISTER_COMPONENTS               (4)

	#define D3D11_COMMONSHADER_CONSTANT_BUFFER_REGISTER_COUNT                    (15)

	#define D3D11_COMMONSHADER_CONSTANT_BUFFER_REGISTER_READS_PER_INST           (1)

	#define D3D11_COMMONSHADER_CONSTANT_BUFFER_REGISTER_READ_PORTS               (1)

	#define D3D11_COMMONSHADER_FLOWCONTROL_NESTING_LIMIT                         (64)

	#define D3D11_COMMONSHADER_IMMEDIATE_CONSTANT_BUFFER_REGISTER_COMPONENTS     (4)

	#define D3D11_COMMONSHADER_IMMEDIATE_CONSTANT_BUFFER_REGISTER_COUNT          (1)

	#define D3D11_COMMONSHADER_IMMEDIATE_CONSTANT_BUFFER_REGISTER_READS_PER_INST (1)

	#define D3D11_COMMONSHADER_IMMEDIATE_CONSTANT_BUFFER_REGISTER_READ_PORTS     (1)

	#define D3D11_COMMONSHADER_IMMEDIATE_VALUE_COMPONENT_BIT_COUNT               (32)

	#define D3D11_COMMONSHADER_INPUT_RESOURCE_REGISTER_COMPONENTS                (1)

	#define D3D11_COMMONSHADER_INPUT_RESOURCE_REGISTER_COUNT                     (128)

	#define D3D11_COMMONSHADER_INPUT_RESOURCE_REGISTER_READS_PER_INST            (1)

	#define D3D11_COMMONSHADER_INPUT_RESOURCE_REGISTER_READ_PORTS                (1)

	#define D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT                         (128)

	#define D3D11_COMMONSHADER_SAMPLER_REGISTER_COMPONENTS                       (1)

	#define D3D11_COMMONSHADER_SAMPLER_REGISTER_COUNT                            (16)

	#define D3D11_COMMONSHADER_SAMPLER_REGISTER_READS_PER_INST                   (1)

	#define D3D11_COMMONSHADER_SAMPLER_REGISTER_READ_PORTS                       (1)

	#define D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT                                (16)

	#define D3D11_COMMONSHADER_SUBROUTINE_NESTING_LIMIT                          (32)

	#define D3D11_COMMONSHADER_TEMP_REGISTER_COMPONENTS                          (4)

	#define D3D11_COMMONSHADER_TEMP_REGISTER_COMPONENT_BIT_COUNT                 (32)

	#define D3D11_COMMONSHADER_TEMP_REGISTER_COUNT                               (4096)

	#define D3D11_COMMONSHADER_TEMP_REGISTER_READS_PER_INST                      (3)

	#define D3D11_COMMONSHADER_TEMP_REGISTER_READ_PORTS                          (3)

	#define D3D11_COMMONSHADER_TEXCOORD_RANGE_REDUCTION_MAX                      (10)

	#define D3D11_COMMONSHADER_TEXCOORD_RANGE_REDUCTION_MIN                      (-10)

	#define D3D11_COMMONSHADER_TEXEL_OFFSET_MAX_NEGATIVE                         (-8)

	#define D3D11_COMMONSHADER_TEXEL_OFFSET_MAX_POSITIVE                         (7)

	#define D3D11_CS_4_X_BUCKET00_MAX_BYTES_TGSM_WRITABLE_PER_THREAD             (256)

	#define D3D11_CS_4_X_BUCKET00_MAX_NUM_THREADS_PER_GROUP                      (64)

	#define D3D11_CS_4_X_BUCKET01_MAX_BYTES_TGSM_WRITABLE_PER_THREAD             (240)

	#define D3D11_CS_4_X_BUCKET01_MAX_NUM_THREADS_PER_GROUP                      (68)

	#define D3D11_CS_4_X_BUCKET02_MAX_BYTES_TGSM_WRITABLE_PER_THREAD             (224)

	#define D3D11_CS_4_X_BUCKET02_MAX_NUM_THREADS_PER_GROUP                      (72)

	#define D3D11_CS_4_X_BUCKET03_MAX_BYTES_TGSM_WRITABLE_PER_THREAD             (208)

	#define D3D11_CS_4_X_BUCKET03_MAX_NUM_THREADS_PER_GROUP                      (76)

	#define D3D11_CS_4_X_BUCKET04_MAX_BYTES_TGSM_WRITABLE_PER_THREAD             (192)

	#define D3D11_CS_4_X_BUCKET04_MAX_NUM_THREADS_PER_GROUP                      (84)

	#define D3D11_CS_4_X_BUCKET05_MAX_BYTES_TGSM_WRITABLE_PER_THREAD             (176)

	#define D3D11_CS_4_X_BUCKET05_MAX_NUM_THREADS_PER_GROUP                      (92)

	#define D3D11_CS_4_X_BUCKET06_MAX_BYTES_TGSM_WRITABLE_PER_THREAD             (160)

	#define D3D11_CS_4_X_BUCKET06_MAX_NUM_THREADS_PER_GROUP                      (100)

	#define D3D11_CS_4_X_BUCKET07_MAX_BYTES_TGSM_WRITABLE_PER_THREAD             (144)

	#define D3D11_CS_4_X_BUCKET07_MAX_NUM_THREADS_PER_GROUP                      (112)

	#define D3D11_CS_4_X_BUCKET08_MAX_BYTES_TGSM_WRITABLE_PER_THREAD             (128)

	#define D3D11_CS_4_X_BUCKET08_MAX_NUM_THREADS_PER_GROUP                      (128)

	#define D3D11_CS_4_X_BUCKET09_MAX_BYTES_TGSM_WRITABLE_PER_THREAD             (112)

	#define D3D11_CS_4_X_BUCKET09_MAX_NUM_THREADS_PER_GROUP                      (144)

	#define D3D11_CS_4_X_BUCKET10_MAX_BYTES_TGSM_WRITABLE_PER_THREAD             (96)

	#define D3D11_CS_4_X_BUCKET10_MAX_NUM_THREADS_PER_GROUP                      (168)

	#define D3D11_CS_4_X_BUCKET11_MAX_BYTES_TGSM_WRITABLE_PER_THREAD             (80)

	#define D3D11_CS_4_X_BUCKET11_MAX_NUM_THREADS_PER_GROUP                      (204)

	#define D3D11_CS_4_X_BUCKET12_MAX_BYTES_TGSM_WRITABLE_PER_THREAD             (64)

	#define D3D11_CS_4_X_BUCKET12_MAX_NUM_THREADS_PER_GROUP                      (256)

	#define D3D11_CS_4_X_BUCKET13_MAX_BYTES_TGSM_WRITABLE_PER_THREAD             (48)

	#define D3D11_CS_4_X_BUCKET13_MAX_NUM_THREADS_PER_GROUP                      (340)

	#define D3D11_CS_4_X_BUCKET14_MAX_BYTES_TGSM_WRITABLE_PER_THREAD             (32)

	#define D3D11_CS_4_X_BUCKET14_MAX_NUM_THREADS_PER_GROUP                      (512)

	#define D3D11_CS_4_X_BUCKET15_MAX_BYTES_TGSM_WRITABLE_PER_THREAD             (16)

	#define D3D11_CS_4_X_BUCKET15_MAX_NUM_THREADS_PER_GROUP                      (768)

	#define D3D11_CS_4_X_DISPATCH_MAX_THREAD_GROUPS_IN_Z_DIMENSION               (1)

	#define D3D11_CS_4_X_RAW_UAV_BYTE_ALIGNMENT                                  (256)

	#define D3D11_CS_4_X_THREAD_GROUP_MAX_THREADS_PER_GROUP                      (768)

	#define D3D11_CS_4_X_THREAD_GROUP_MAX_X                                      (768)

	#define D3D11_CS_4_X_THREAD_GROUP_MAX_Y                                      (768)

	#define D3D11_CS_4_X_UAV_REGISTER_COUNT                                      (1)

	#define D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION                    (65535)

	#define D3D11_CS_TGSM_REGISTER_COUNT                                         (8192)

	#define D3D11_CS_TGSM_REGISTER_READS_PER_INST                                (1)

	#define D3D11_CS_TGSM_RESOURCE_REGISTER_COMPONENTS                           (1)

	#define D3D11_CS_TGSM_RESOURCE_REGISTER_READ_PORTS                           (1)

	#define D3D11_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP                          (1024)

	#define D3D11_CS_THREAD_GROUP_MAX_X                                          (1024)

	#define D3D11_CS_THREAD_GROUP_MAX_Y                                          (1024)

	#define D3D11_CS_THREAD_GROUP_MAX_Z                                          (64)

	#define D3D11_CS_THREAD_GROUP_MIN_X                                          (1)

	#define D3D11_CS_THREAD_GROUP_MIN_Y                                          (1)

	#define D3D11_CS_THREAD_GROUP_MIN_Z                                          (1)

	#define D3D11_CS_THREAD_LOCAL_TEMP_REGISTER_POOL                             (16384)

	#define D3D11_DEFAULT_BLEND_FACTOR_ALPHA                                     (1.0f)
	#define D3D11_DEFAULT_BLEND_FACTOR_BLUE                                      (1.0f)
	#define D3D11_DEFAULT_BLEND_FACTOR_GREEN                                     (1.0f)
	#define D3D11_DEFAULT_BLEND_FACTOR_RED                                       (1.0f)
	#define D3D11_DEFAULT_BORDER_COLOR_COMPONENT                                 (0.0f)
	#define D3D11_DEFAULT_DEPTH_BIAS                                             (0)

	#define D3D11_DEFAULT_DEPTH_BIAS_CLAMP                                       (0.0f)
	#define D3D11_DEFAULT_MAX_ANISOTROPY                                         (16)
	#define D3D11_DEFAULT_MIP_LOD_BIAS                                           (0.0f)
	#define D3D11_DEFAULT_RENDER_TARGET_ARRAY_INDEX                              (0)

	#define D3D11_DEFAULT_SAMPLE_MASK                                            (0xffffffff)

	#define D3D11_DEFAULT_SCISSOR_ENDX                                           (0)

	#define D3D11_DEFAULT_SCISSOR_ENDY                                           (0)

	#define D3D11_DEFAULT_SCISSOR_STARTX                                         (0)

	#define D3D11_DEFAULT_SCISSOR_STARTY                                         (0)

	#define D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS                                (0.0f)
	#define D3D11_DEFAULT_STENCIL_READ_MASK                                      (0xff)

	#define D3D11_DEFAULT_STENCIL_REFERENCE                                      (0)

	#define D3D11_DEFAULT_STENCIL_WRITE_MASK                                     (0xff)

	#define D3D11_DEFAULT_VIEWPORT_AND_SCISSORRECT_INDEX                         (0)

	#define D3D11_DEFAULT_VIEWPORT_HEIGHT                                        (0)

	#define D3D11_DEFAULT_VIEWPORT_MAX_DEPTH                                     (0.0f)
	#define D3D11_DEFAULT_VIEWPORT_MIN_DEPTH                                     (0.0f)
	#define D3D11_DEFAULT_VIEWPORT_TOPLEFTX                                      (0)

	#define D3D11_DEFAULT_VIEWPORT_TOPLEFTY                                      (0)

	#define D3D11_DEFAULT_VIEWPORT_WIDTH                                         (0)

	#define D3D11_DS_INPUT_CONTROL_POINTS_MAX_TOTAL_SCALARS                      (3968)

	#define D3D11_DS_INPUT_CONTROL_POINT_REGISTER_COMPONENTS                     (4)

	#define D3D11_DS_INPUT_CONTROL_POINT_REGISTER_COMPONENT_BIT_COUNT            (32)

	#define D3D11_DS_INPUT_CONTROL_POINT_REGISTER_COUNT                          (32)

	#define D3D11_DS_INPUT_CONTROL_POINT_REGISTER_READS_PER_INST                 (2)

	#define D3D11_DS_INPUT_CONTROL_POINT_REGISTER_READ_PORTS                     (1)

	#define D3D11_DS_INPUT_DOMAIN_POINT_REGISTER_COMPONENTS                      (3)

	#define D3D11_DS_INPUT_DOMAIN_POINT_REGISTER_COMPONENT_BIT_COUNT             (32)

	#define D3D11_DS_INPUT_DOMAIN_POINT_REGISTER_COUNT                           (1)

	#define D3D11_DS_INPUT_DOMAIN_POINT_REGISTER_READS_PER_INST                  (2)

	#define D3D11_DS_INPUT_DOMAIN_POINT_REGISTER_READ_PORTS                      (1)

	#define D3D11_DS_INPUT_PATCH_CONSTANT_REGISTER_COMPONENTS                    (4)

	#define D3D11_DS_INPUT_PATCH_CONSTANT_REGISTER_COMPONENT_BIT_COUNT           (32)

	#define D3D11_DS_INPUT_PATCH_CONSTANT_REGISTER_COUNT                         (32)

	#define D3D11_DS_INPUT_PATCH_CONSTANT_REGISTER_READS_PER_INST                (2)

	#define D3D11_DS_INPUT_PATCH_CONSTANT_REGISTER_READ_PORTS                    (1)

	#define D3D11_DS_OUTPUT_REGISTER_COMPONENTS                                  (4)

	#define D3D11_DS_OUTPUT_REGISTER_COMPONENT_BIT_COUNT                         (32)

	#define D3D11_DS_OUTPUT_REGISTER_COUNT                                       (32)

	#define D3D11_FLOAT16_FUSED_TOLERANCE_IN_ULP                                 (0.6)
	#define D3D11_FLOAT32_MAX                                                    (3.402823466e+38f)
	#define D3D11_FLOAT32_TO_INTEGER_TOLERANCE_IN_ULP                            (0.6f)
	#define D3D11_FLOAT_TO_SRGB_EXPONENT_DENOMINATOR                             (2.4f)
	#define D3D11_FLOAT_TO_SRGB_EXPONENT_NUMERATOR                               (1.0f)
	#define D3D11_FLOAT_TO_SRGB_OFFSET                                           (0.055f)
	#define D3D11_FLOAT_TO_SRGB_SCALE_1                                          (12.92f)
	#define D3D11_FLOAT_TO_SRGB_SCALE_2                                          (1.055f)
	#define D3D11_FLOAT_TO_SRGB_THRESHOLD                                        (0.0031308f)
	#define D3D11_FTOI_INSTRUCTION_MAX_INPUT                                     (2147483647.999f)
	#define D3D11_FTOI_INSTRUCTION_MIN_INPUT                                     (-2147483648.999f)
	#define D3D11_FTOU_INSTRUCTION_MAX_INPUT                                     (4294967295.999f)
	#define D3D11_FTOU_INSTRUCTION_MIN_INPUT                                     (0.0f)
	#define D3D11_GS_INPUT_INSTANCE_ID_READS_PER_INST                            (2)

	#define D3D11_GS_INPUT_INSTANCE_ID_READ_PORTS                                (1)

	#define D3D11_GS_INPUT_INSTANCE_ID_REGISTER_COMPONENTS                       (1)

	#define D3D11_GS_INPUT_INSTANCE_ID_REGISTER_COMPONENT_BIT_COUNT              (32)

	#define D3D11_GS_INPUT_INSTANCE_ID_REGISTER_COUNT                            (1)

	#define D3D11_GS_INPUT_PRIM_CONST_REGISTER_COMPONENTS                        (1)

	#define D3D11_GS_INPUT_PRIM_CONST_REGISTER_COMPONENT_BIT_COUNT               (32)

	#define D3D11_GS_INPUT_PRIM_CONST_REGISTER_COUNT                             (1)

	#define D3D11_GS_INPUT_PRIM_CONST_REGISTER_READS_PER_INST                    (2)

	#define D3D11_GS_INPUT_PRIM_CONST_REGISTER_READ_PORTS                        (1)

	#define D3D11_GS_INPUT_REGISTER_COMPONENTS                                   (4)

	#define D3D11_GS_INPUT_REGISTER_COMPONENT_BIT_COUNT                          (32)

	#define D3D11_GS_INPUT_REGISTER_COUNT                                        (32)

	#define D3D11_GS_INPUT_REGISTER_READS_PER_INST                               (2)

	#define D3D11_GS_INPUT_REGISTER_READ_PORTS                                   (1)

	#define D3D11_GS_INPUT_REGISTER_VERTICES                                     (32)

	#define D3D11_GS_MAX_INSTANCE_COUNT                                          (32)

	#define D3D11_GS_MAX_OUTPUT_VERTEX_COUNT_ACROSS_INSTANCES                    (1024)

	#define D3D11_GS_OUTPUT_ELEMENTS                                             (32)

	#define D3D11_GS_OUTPUT_REGISTER_COMPONENTS                                  (4)

	#define D3D11_GS_OUTPUT_REGISTER_COMPONENT_BIT_COUNT                         (32)

	#define D3D11_GS_OUTPUT_REGISTER_COUNT                                       (32)

	#define D3D11_HS_CONTROL_POINT_PHASE_INPUT_REGISTER_COUNT                    (32)

	#define D3D11_HS_CONTROL_POINT_PHASE_OUTPUT_REGISTER_COUNT                   (32)

	#define D3D11_HS_CONTROL_POINT_REGISTER_COMPONENTS                           (4)

	#define D3D11_HS_CONTROL_POINT_REGISTER_COMPONENT_BIT_COUNT                  (32)

	#define D3D11_HS_CONTROL_POINT_REGISTER_READS_PER_INST                       (2)

	#define D3D11_HS_CONTROL_POINT_REGISTER_READ_PORTS                           (1)

	#define D3D11_HS_FORK_PHASE_INSTANCE_COUNT_UPPER_BOUND                       (0xffffffff)

	#define D3D11_HS_INPUT_FORK_INSTANCE_ID_REGISTER_COMPONENTS                  (1)

	#define D3D11_HS_INPUT_FORK_INSTANCE_ID_REGISTER_COMPONENT_BIT_COUNT         (32)

	#define D3D11_HS_INPUT_FORK_INSTANCE_ID_REGISTER_COUNT                       (1)

	#define D3D11_HS_INPUT_FORK_INSTANCE_ID_REGISTER_READS_PER_INST              (2)

	#define D3D11_HS_INPUT_FORK_INSTANCE_ID_REGISTER_READ_PORTS                  (1)

	#define D3D11_HS_INPUT_JOIN_INSTANCE_ID_REGISTER_COMPONENTS                  (1)

	#define D3D11_HS_INPUT_JOIN_INSTANCE_ID_REGISTER_COMPONENT_BIT_COUNT         (32)

	#define D3D11_HS_INPUT_JOIN_INSTANCE_ID_REGISTER_COUNT                       (1)

	#define D3D11_HS_INPUT_JOIN_INSTANCE_ID_REGISTER_READS_PER_INST              (2)

	#define D3D11_HS_INPUT_JOIN_INSTANCE_ID_REGISTER_READ_PORTS                  (1)

	#define D3D11_HS_INPUT_PRIMITIVE_ID_REGISTER_COMPONENTS                      (1)

	#define D3D11_HS_INPUT_PRIMITIVE_ID_REGISTER_COMPONENT_BIT_COUNT             (32)

	#define D3D11_HS_INPUT_PRIMITIVE_ID_REGISTER_COUNT                           (1)

	#define D3D11_HS_INPUT_PRIMITIVE_ID_REGISTER_READS_PER_INST                  (2)

	#define D3D11_HS_INPUT_PRIMITIVE_ID_REGISTER_READ_PORTS                      (1)

	#define D3D11_HS_JOIN_PHASE_INSTANCE_COUNT_UPPER_BOUND                       (0xffffffff)

	#define D3D11_HS_MAXTESSFACTOR_LOWER_BOUND                                   (1.0f)
	#define D3D11_HS_MAXTESSFACTOR_UPPER_BOUND                                   (64.0f)
	#define D3D11_HS_OUTPUT_CONTROL_POINTS_MAX_TOTAL_SCALARS                     (3968)

	#define D3D11_HS_OUTPUT_CONTROL_POINT_ID_REGISTER_COMPONENTS                 (1)

	#define D3D11_HS_OUTPUT_CONTROL_POINT_ID_REGISTER_COMPONENT_BIT_COUNT        (32)

	#define D3D11_HS_OUTPUT_CONTROL_POINT_ID_REGISTER_COUNT                      (1)

	#define D3D11_HS_OUTPUT_CONTROL_POINT_ID_REGISTER_READS_PER_INST             (2)

	#define D3D11_HS_OUTPUT_CONTROL_POINT_ID_REGISTER_READ_PORTS                 (1)

	#define D3D11_HS_OUTPUT_PATCH_CONSTANT_REGISTER_COMPONENTS                   (4)

	#define D3D11_HS_OUTPUT_PATCH_CONSTANT_REGISTER_COMPONENT_BIT_COUNT          (32)

	#define D3D11_HS_OUTPUT_PATCH_CONSTANT_REGISTER_COUNT                        (32)

	#define D3D11_HS_OUTPUT_PATCH_CONSTANT_REGISTER_READS_PER_INST               (2)

	#define D3D11_HS_OUTPUT_PATCH_CONSTANT_REGISTER_READ_PORTS                   (1)

	#define D3D11_IA_DEFAULT_INDEX_BUFFER_OFFSET_IN_BYTES                        (0)

	#define D3D11_IA_DEFAULT_PRIMITIVE_TOPOLOGY                                  (0)

	#define D3D11_IA_DEFAULT_VERTEX_BUFFER_OFFSET_IN_BYTES                       (0)

	#define D3D11_IA_INDEX_INPUT_RESOURCE_SLOT_COUNT                             (1)

	#define D3D11_IA_INSTANCE_ID_BIT_COUNT                                       (32)

	#define D3D11_IA_INTEGER_ARITHMETIC_BIT_COUNT                                (32)

	#define D3D11_IA_PATCH_MAX_CONTROL_POINT_COUNT                               (32)

	#define D3D11_IA_PRIMITIVE_ID_BIT_COUNT                                      (32)

	#define D3D11_IA_VERTEX_ID_BIT_COUNT                                         (32)

	#define D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT                            (32)

	#define D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENTS_COMPONENTS                  (128)

	#define D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT                        (32)

	#define D3D11_INTEGER_DIVIDE_BY_ZERO_QUOTIENT                                (0xffffffff)

	#define D3D11_INTEGER_DIVIDE_BY_ZERO_REMAINDER                               (0xffffffff)

	#define D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL                          (0xffffffff)

	#define D3D11_KEEP_UNORDERED_ACCESS_VIEWS                                    (0xffffffff)

	#define D3D11_LINEAR_GAMMA                                                   (1.0f)
	#define D3D11_MAJOR_VERSION                                                  (11)

	#define D3D11_MAX_BORDER_COLOR_COMPONENT                                     (1.0f)
	#define D3D11_MAX_DEPTH                                                      (1.0f)
	#define D3D11_MAX_MAXANISOTROPY                                              (16)

	#define D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT                                   (32)

	#define D3D11_MAX_POSITION_VALUE                                             (3.402823466e+34f)
	#define D3D11_MAX_TEXTURE_DIMENSION_2_TO_EXP                                 (17)

	#define D3D11_MINOR_VERSION                                                  (0)

	#define D3D11_MIN_BORDER_COLOR_COMPONENT                                     (0.0f)
	#define D3D11_MIN_DEPTH                                                      (0.0f)
	#define D3D11_MIN_MAXANISOTROPY                                              (0)

	#define D3D11_MIP_LOD_BIAS_MAX                                               (15.99f)
	#define D3D11_MIP_LOD_BIAS_MIN                                               (-16.0f)
	#define D3D11_MIP_LOD_FRACTIONAL_BIT_COUNT                                   (8)

	#define D3D11_MIP_LOD_RANGE_BIT_COUNT                                        (8)

	#define D3D11_MULTISAMPLE_ANTIALIAS_LINE_WIDTH                               (1.4f)
	#define D3D11_NONSAMPLE_FETCH_OUT_OF_RANGE_ACCESS_RESULT                     (0)

	#define D3D11_PIXEL_ADDRESS_RANGE_BIT_COUNT                                  (15)

	#define D3D11_PRE_SCISSOR_PIXEL_ADDRESS_RANGE_BIT_COUNT                      (16)

	#define D3D11_PS_CS_UAV_REGISTER_COMPONENTS                                  (1)

	#define D3D11_PS_CS_UAV_REGISTER_COUNT                                       (8)

	#define D3D11_PS_CS_UAV_REGISTER_READS_PER_INST                              (1)

	#define D3D11_PS_CS_UAV_REGISTER_READ_PORTS                                  (1)

	#define D3D11_PS_FRONTFACING_DEFAULT_VALUE                                   (0xffffffff)

	#define D3D11_PS_FRONTFACING_FALSE_VALUE                                     (0)

	#define D3D11_PS_FRONTFACING_TRUE_VALUE                                      (0xffffffff)

	#define D3D11_PS_INPUT_REGISTER_COMPONENTS                                   (4)

	#define D3D11_PS_INPUT_REGISTER_COMPONENT_BIT_COUNT                          (32)

	#define D3D11_PS_INPUT_REGISTER_COUNT                                        (32)

	#define D3D11_PS_INPUT_REGISTER_READS_PER_INST                               (2)

	#define D3D11_PS_INPUT_REGISTER_READ_PORTS                                   (1)

	#define D3D11_PS_LEGACY_PIXEL_CENTER_FRACTIONAL_COMPONENT                    (0.0f)
	#define D3D11_PS_OUTPUT_DEPTH_REGISTER_COMPONENTS                            (1)

	#define D3D11_PS_OUTPUT_DEPTH_REGISTER_COMPONENT_BIT_COUNT                   (32)

	#define D3D11_PS_OUTPUT_DEPTH_REGISTER_COUNT                                 (1)

	#define D3D11_PS_OUTPUT_MASK_REGISTER_COMPONENTS                             (1)

	#define D3D11_PS_OUTPUT_MASK_REGISTER_COMPONENT_BIT_COUNT                    (32)

	#define D3D11_PS_OUTPUT_MASK_REGISTER_COUNT                                  (1)

	#define D3D11_PS_OUTPUT_REGISTER_COMPONENTS                                  (4)

	#define D3D11_PS_OUTPUT_REGISTER_COMPONENT_BIT_COUNT                         (32)

	#define D3D11_PS_OUTPUT_REGISTER_COUNT                                       (8)

	#define D3D11_PS_PIXEL_CENTER_FRACTIONAL_COMPONENT                           (0.5f)
	#define D3D11_RAW_UAV_SRV_BYTE_ALIGNMENT                                     (16)

	#define D3D11_REQ_BLEND_OBJECT_COUNT_PER_DEVICE                              (4096)

	#define D3D11_REQ_BUFFER_RESOURCE_TEXEL_COUNT_2_TO_EXP                       (27)

	#define D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT                              (4096)

	#define D3D11_REQ_DEPTH_STENCIL_OBJECT_COUNT_PER_DEVICE                      (4096)

	#define D3D11_REQ_DRAWINDEXED_INDEX_COUNT_2_TO_EXP                           (32)

	#define D3D11_REQ_DRAW_VERTEX_COUNT_2_TO_EXP                                 (32)

	#define D3D11_REQ_FILTERING_HW_ADDRESSABLE_RESOURCE_DIMENSION                (16384)

	#define D3D11_REQ_GS_INVOCATION_32BIT_OUTPUT_COMPONENT_LIMIT                 (1024)

	#define D3D11_REQ_IMMEDIATE_CONSTANT_BUFFER_ELEMENT_COUNT                    (4096)

	#define D3D11_REQ_MAXANISOTROPY                                              (16)

	#define D3D11_REQ_MIP_LEVELS                                                 (15)

	#define D3D11_REQ_MULTI_ELEMENT_STRUCTURE_SIZE_IN_BYTES                      (2048)

	#define D3D11_REQ_RASTERIZER_OBJECT_COUNT_PER_DEVICE                         (4096)

	#define D3D11_REQ_RENDER_TO_BUFFER_WINDOW_WIDTH                              (16384)

	#define D3D11_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_A_TERM               (128)

	#define D3D11_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_B_TERM               (0.25f)
	#define D3D11_REQ_RESOURCE_VIEW_COUNT_PER_DEVICE_2_TO_EXP                    (20)

	#define D3D11_REQ_SAMPLER_OBJECT_COUNT_PER_DEVICE                            (4096)

	#define D3D11_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION                             (2048)

	#define D3D11_REQ_TEXTURE1D_U_DIMENSION                                      (16384)

	#define D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION                             (2048)

	#define D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION                                 (16384)

	#define D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION                               (2048)

	#define D3D11_REQ_TEXTURECUBE_DIMENSION                                      (16384)

	#define D3D11_RESINFO_INSTRUCTION_MISSING_COMPONENT_RETVAL                   (0)

	#define D3D11_SHADER_MAJOR_VERSION                                           (5)

	#define D3D11_SHADER_MAX_INSTANCES                                           (65535)

	#define D3D11_SHADER_MAX_INTERFACES                                          (253)

	#define D3D11_SHADER_MAX_INTERFACE_CALL_SITES                                (4096)

	#define D3D11_SHADER_MAX_TYPES                                               (65535)

	#define D3D11_SHADER_MINOR_VERSION                                           (0)

	#define D3D11_SHIFT_INSTRUCTION_PAD_VALUE                                    (0)

	#define D3D11_SHIFT_INSTRUCTION_SHIFT_VALUE_BIT_COUNT                        (5)

	#define D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT                               (8)

	#define D3D11_SO_BUFFER_MAX_STRIDE_IN_BYTES                                  (2048)

	#define D3D11_SO_BUFFER_MAX_WRITE_WINDOW_IN_BYTES                            (512)

	#define D3D11_SO_BUFFER_SLOT_COUNT                                           (4)

	#define D3D11_SO_DDI_REGISTER_INDEX_DENOTING_GAP                             (0xffffffff)

	#define D3D11_SO_NO_RASTERIZED_STREAM                                        (0xffffffff)

	#define D3D11_SO_OUTPUT_COMPONENT_COUNT                                      (128)

	#define D3D11_SO_STREAM_COUNT                                                (4)

	#define D3D11_SPEC_DATE_DAY                                                  (04)

	#define D3D11_SPEC_DATE_MONTH                                                (06)

	#define D3D11_SPEC_DATE_YEAR                                                 (2009)

	#define D3D11_SPEC_VERSION                                                   (1.0)
	#define D3D11_SRGB_GAMMA                                                     (2.2f)
	#define D3D11_SRGB_TO_FLOAT_DENOMINATOR_1                                    (12.92f)
	#define D3D11_SRGB_TO_FLOAT_DENOMINATOR_2                                    (1.055f)
	#define D3D11_SRGB_TO_FLOAT_EXPONENT                                         (2.4f)
	#define D3D11_SRGB_TO_FLOAT_OFFSET                                           (0.055f)
	#define D3D11_SRGB_TO_FLOAT_THRESHOLD                                        (0.04045f)
	#define D3D11_SRGB_TO_FLOAT_TOLERANCE_IN_ULP                                 (0.5f)
	#define D3D11_STANDARD_COMPONENT_BIT_COUNT                                   (32)

	#define D3D11_STANDARD_COMPONENT_BIT_COUNT_DOUBLED                           (64)

	#define D3D11_STANDARD_MAXIMUM_ELEMENT_ALIGNMENT_BYTE_MULTIPLE               (4)

	#define D3D11_STANDARD_PIXEL_COMPONENT_COUNT                                 (128)

	#define D3D11_STANDARD_PIXEL_ELEMENT_COUNT                                   (32)

	#define D3D11_STANDARD_VECTOR_SIZE                                           (4)

	#define D3D11_STANDARD_VERTEX_ELEMENT_COUNT                                  (32)

	#define D3D11_STANDARD_VERTEX_TOTAL_COMPONENT_COUNT                          (64)

	#define D3D11_SUBPIXEL_FRACTIONAL_BIT_COUNT                                  (8)

	#define D3D11_SUBTEXEL_FRACTIONAL_BIT_COUNT                                  (8)

	#define D3D11_TESSELLATOR_MAX_EVEN_TESSELLATION_FACTOR                       (64)

	#define D3D11_TESSELLATOR_MAX_ISOLINE_DENSITY_TESSELLATION_FACTOR            (64)

	#define D3D11_TESSELLATOR_MAX_ODD_TESSELLATION_FACTOR                        (63)

	#define D3D11_TESSELLATOR_MAX_TESSELLATION_FACTOR                            (64)

	#define D3D11_TESSELLATOR_MIN_EVEN_TESSELLATION_FACTOR                       (2)

	#define D3D11_TESSELLATOR_MIN_ISOLINE_DENSITY_TESSELLATION_FACTOR            (1)

	#define D3D11_TESSELLATOR_MIN_ODD_TESSELLATION_FACTOR                        (1)

	#define D3D11_TEXEL_ADDRESS_RANGE_BIT_COUNT                                  (16)

	#define D3D11_UNBOUND_MEMORY_ACCESS_RESULT                                   (0)

	#define D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX                             (15)

	#define D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE             (16)

	#define D3D11_VIEWPORT_BOUNDS_MAX                                            (32767)

	#define D3D11_VIEWPORT_BOUNDS_MIN                                            (-32768)

	#define D3D11_VS_INPUT_REGISTER_COMPONENTS                                   (4)

	#define D3D11_VS_INPUT_REGISTER_COMPONENT_BIT_COUNT                          (32)

	#define D3D11_VS_INPUT_REGISTER_COUNT                                        (32)

	#define D3D11_VS_INPUT_REGISTER_READS_PER_INST                               (2)

	#define D3D11_VS_INPUT_REGISTER_READ_PORTS                                   (1)

	#define D3D11_VS_OUTPUT_REGISTER_COMPONENTS                                  (4)

	#define D3D11_VS_OUTPUT_REGISTER_COMPONENT_BIT_COUNT                         (32)

	#define D3D11_VS_OUTPUT_REGISTER_COUNT                                       (32)

	#define D3D11_WHQL_CONTEXT_COUNT_FOR_RESOURCE_LIMIT                          (10)

	#define D3D11_WHQL_DRAWINDEXED_INDEX_COUNT_2_TO_EXP                          (25)

	#define D3D11_WHQL_DRAW_VERTEX_COUNT_2_TO_EXP                                (25)

#endif

#ifndef _D3D11_1_CONSTANTS
	#define _D3D11_1_CONSTANTS
	#define D3D11_1_UAV_SLOT_COUNT (64)

#endif

#define D3D11_SDK_VERSION (7)

////////////////////////////////////////////////////////////////////////////
//  Enums
////////////////////////////////////////////////////////////////////////////

typedef
  enum D3D11_INPUT_CLASSIFICATION
{
	D3D11_INPUT_PER_VERTEX_DATA   = 0,
	D3D11_INPUT_PER_INSTANCE_DATA = 1
}   D3D11_INPUT_CLASSIFICATION;

typedef
  enum D3D11_FILL_MODE
{
	D3D11_FILL_WIREFRAME = 2,
	D3D11_FILL_SOLID     = 3
}   D3D11_FILL_MODE;

typedef D3D_PRIMITIVE_TOPOLOGY D3D11_PRIMITIVE_TOPOLOGY;

typedef D3D_PRIMITIVE          D3D11_PRIMITIVE;

typedef
  enum D3D11_CULL_MODE
{
	D3D11_CULL_NONE  = 1,
	D3D11_CULL_FRONT = 2,
	D3D11_CULL_BACK  = 3
}   D3D11_CULL_MODE;

typedef
  enum D3D11_RESOURCE_DIMENSION
{
	D3D11_RESOURCE_DIMENSION_UNKNOWN   = 0,
	D3D11_RESOURCE_DIMENSION_BUFFER    = 1,
	D3D11_RESOURCE_DIMENSION_TEXTURE1D = 2,
	D3D11_RESOURCE_DIMENSION_TEXTURE2D = 3,
	D3D11_RESOURCE_DIMENSION_TEXTURE3D = 4
}   D3D11_RESOURCE_DIMENSION;

typedef D3D_SRV_DIMENSION D3D11_SRV_DIMENSION;

typedef
  enum D3D11_DSV_DIMENSION
{
	D3D11_DSV_DIMENSION_UNKNOWN          = 0,
	D3D11_DSV_DIMENSION_TEXTURE1D        = 1,
	D3D11_DSV_DIMENSION_TEXTURE1DARRAY   = 2,
	D3D11_DSV_DIMENSION_TEXTURE2D        = 3,
	D3D11_DSV_DIMENSION_TEXTURE2DARRAY   = 4,
	D3D11_DSV_DIMENSION_TEXTURE2DMS      = 5,
	D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY = 6
}   D3D11_DSV_DIMENSION;

typedef
  enum D3D11_RTV_DIMENSION
{
	D3D11_RTV_DIMENSION_UNKNOWN          = 0,
	D3D11_RTV_DIMENSION_BUFFER           = 1,
	D3D11_RTV_DIMENSION_TEXTURE1D        = 2,
	D3D11_RTV_DIMENSION_TEXTURE1DARRAY   = 3,
	D3D11_RTV_DIMENSION_TEXTURE2D        = 4,
	D3D11_RTV_DIMENSION_TEXTURE2DARRAY   = 5,
	D3D11_RTV_DIMENSION_TEXTURE2DMS      = 6,
	D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY = 7,
	D3D11_RTV_DIMENSION_TEXTURE3D        = 8
}   D3D11_RTV_DIMENSION;

typedef
  enum D3D11_UAV_DIMENSION
{
	D3D11_UAV_DIMENSION_UNKNOWN        = 0,
	D3D11_UAV_DIMENSION_BUFFER         = 1,
	D3D11_UAV_DIMENSION_TEXTURE1D      = 2,
	D3D11_UAV_DIMENSION_TEXTURE1DARRAY = 3,
	D3D11_UAV_DIMENSION_TEXTURE2D      = 4,
	D3D11_UAV_DIMENSION_TEXTURE2DARRAY = 5,
	D3D11_UAV_DIMENSION_TEXTURE3D      = 8
}   D3D11_UAV_DIMENSION;

typedef
  enum D3D11_USAGE
{
	D3D11_USAGE_DEFAULT   = 0,
	D3D11_USAGE_IMMUTABLE = 1,
	D3D11_USAGE_DYNAMIC   = 2,
	D3D11_USAGE_STAGING   = 3
}   D3D11_USAGE;

typedef
  enum D3D11_BIND_FLAG
{
	D3D11_BIND_VERTEX_BUFFER    = 0x1L,
	D3D11_BIND_INDEX_BUFFER     = 0x2L,
	D3D11_BIND_CONSTANT_BUFFER  = 0x4L,
	D3D11_BIND_SHADER_RESOURCE  = 0x8L,
	D3D11_BIND_STREAM_OUTPUT    = 0x10L,
	D3D11_BIND_RENDER_TARGET    = 0x20L,
	D3D11_BIND_DEPTH_STENCIL    = 0x40L,
	D3D11_BIND_UNORDERED_ACCESS = 0x80L
}   D3D11_BIND_FLAG;

typedef
  enum D3D11_CPU_ACCESS_FLAG
{
	D3D11_CPU_ACCESS_WRITE = 0x10000L,
	D3D11_CPU_ACCESS_READ  = 0x20000L
}   D3D11_CPU_ACCESS_FLAG;

typedef
  enum D3D11_RESOURCE_MISC_FLAG
{
	D3D11_RESOURCE_MISC_GENERATE_MIPS          = 0x1L,
	D3D11_RESOURCE_MISC_SHARED                 = 0x2L,
	D3D11_RESOURCE_MISC_TEXTURECUBE            = 0x4L,
	D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS      = 0x10L,
	D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS = 0x20L,
	D3D11_RESOURCE_MISC_BUFFER_STRUCTURED      = 0x40L,
	D3D11_RESOURCE_MISC_RESOURCE_CLAMP         = 0x80L,
	D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX      = 0x100L,
	D3D11_RESOURCE_MISC_GDI_COMPATIBLE         = 0x200L,
	D3D11_RESOURCE_MISC_DXGL_NO_STREAMING      = 0x10000000L,
	D3D11_RESOURCE_MISC_DXGL_MAP_PERSISTENT    = 0x20000000L,
	D3D11_RESOURCE_MISC_DXGL_MAP_COHERENT      = 0x40000000L,
	D3D11_RESOURCE_MISC_DXGL_FORCE_ARRAY       = 0x80000000L
}   D3D11_RESOURCE_MISC_FLAG;

typedef
  enum D3D11_MAP
{
	D3D11_MAP_READ               = 1,
	D3D11_MAP_WRITE              = 2,
	D3D11_MAP_READ_WRITE         = 3,
	D3D11_MAP_WRITE_DISCARD      = 4,
	D3D11_MAP_WRITE_NO_OVERWRITE = 5
}   D3D11_MAP;

typedef
  enum D3D11_MAP_FLAG
{
	D3D11_MAP_FLAG_DO_NOT_WAIT     = 0x100000L,
	D3D11_MAP_FLAG_DXGL_PERSISTENT = 0x40000000L,
	D3D11_MAP_FLAG_DXGL_COHERENT   = 0x80000000L
}   D3D11_MAP_FLAG;

typedef
  enum D3D11_RAISE_FLAG
{
	D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR = 0x1L
}   D3D11_RAISE_FLAG;

typedef
  enum D3D11_CLEAR_FLAG
{
	D3D11_CLEAR_DEPTH   = 0x1L,
	D3D11_CLEAR_STENCIL = 0x2L
}   D3D11_CLEAR_FLAG;

typedef RECT D3D11_RECT;

typedef
  enum D3D11_COMPARISON_FUNC
{
	D3D11_COMPARISON_NEVER         = 1,
	D3D11_COMPARISON_LESS          = 2,
	D3D11_COMPARISON_EQUAL         = 3,
	D3D11_COMPARISON_LESS_EQUAL    = 4,
	D3D11_COMPARISON_GREATER       = 5,
	D3D11_COMPARISON_NOT_EQUAL     = 6,
	D3D11_COMPARISON_GREATER_EQUAL = 7,
	D3D11_COMPARISON_ALWAYS        = 8
}   D3D11_COMPARISON_FUNC;

typedef
  enum D3D11_DEPTH_WRITE_MASK
{
	D3D11_DEPTH_WRITE_MASK_ZERO = 0,
	D3D11_DEPTH_WRITE_MASK_ALL  = 1
}   D3D11_DEPTH_WRITE_MASK;

typedef
  enum D3D11_STENCIL_OP
{
	D3D11_STENCIL_OP_KEEP     = 1,
	D3D11_STENCIL_OP_ZERO     = 2,
	D3D11_STENCIL_OP_REPLACE  = 3,
	D3D11_STENCIL_OP_INCR_SAT = 4,
	D3D11_STENCIL_OP_DECR_SAT = 5,
	D3D11_STENCIL_OP_INVERT   = 6,
	D3D11_STENCIL_OP_INCR     = 7,
	D3D11_STENCIL_OP_DECR     = 8
}   D3D11_STENCIL_OP;

typedef
  enum D3D11_BLEND
{
	D3D11_BLEND_ZERO             = 1,
	D3D11_BLEND_ONE              = 2,
	D3D11_BLEND_SRC_COLOR        = 3,
	D3D11_BLEND_INV_SRC_COLOR    = 4,
	D3D11_BLEND_SRC_ALPHA        = 5,
	D3D11_BLEND_INV_SRC_ALPHA    = 6,
	D3D11_BLEND_DEST_ALPHA       = 7,
	D3D11_BLEND_INV_DEST_ALPHA   = 8,
	D3D11_BLEND_DEST_COLOR       = 9,
	D3D11_BLEND_INV_DEST_COLOR   = 10,
	D3D11_BLEND_SRC_ALPHA_SAT    = 11,
	D3D11_BLEND_BLEND_FACTOR     = 14,
	D3D11_BLEND_INV_BLEND_FACTOR = 15,
	D3D11_BLEND_SRC1_COLOR       = 16,
	D3D11_BLEND_INV_SRC1_COLOR   = 17,
	D3D11_BLEND_SRC1_ALPHA       = 18,
	D3D11_BLEND_INV_SRC1_ALPHA   = 19
}   D3D11_BLEND;

typedef
  enum D3D11_BLEND_OP
{
	D3D11_BLEND_OP_ADD          = 1,
	D3D11_BLEND_OP_SUBTRACT     = 2,
	D3D11_BLEND_OP_REV_SUBTRACT = 3,
	D3D11_BLEND_OP_MIN          = 4,
	D3D11_BLEND_OP_MAX          = 5
}   D3D11_BLEND_OP;

typedef
  enum D3D11_COLOR_WRITE_ENABLE
{
	D3D11_COLOR_WRITE_ENABLE_RED   = 1,
	D3D11_COLOR_WRITE_ENABLE_GREEN = 2,
	D3D11_COLOR_WRITE_ENABLE_BLUE  = 4,
	D3D11_COLOR_WRITE_ENABLE_ALPHA = 8,
	D3D11_COLOR_WRITE_ENABLE_ALL   = (((D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN) | D3D11_COLOR_WRITE_ENABLE_BLUE) | D3D11_COLOR_WRITE_ENABLE_ALPHA)
}   D3D11_COLOR_WRITE_ENABLE;

typedef
  enum D3D11_TEXTURECUBE_FACE
{
	D3D11_TEXTURECUBE_FACE_POSITIVE_X = 0,
	D3D11_TEXTURECUBE_FACE_NEGATIVE_X = 1,
	D3D11_TEXTURECUBE_FACE_POSITIVE_Y = 2,
	D3D11_TEXTURECUBE_FACE_NEGATIVE_Y = 3,
	D3D11_TEXTURECUBE_FACE_POSITIVE_Z = 4,
	D3D11_TEXTURECUBE_FACE_NEGATIVE_Z = 5
}   D3D11_TEXTURECUBE_FACE;

typedef
  enum D3D11_BUFFEREX_SRV_FLAG
{
	D3D11_BUFFEREX_SRV_FLAG_RAW = 0x1
}   D3D11_BUFFEREX_SRV_FLAG;

typedef
  enum D3D11_DSV_FLAG
{
	D3D11_DSV_READ_ONLY_DEPTH   = 0x1L,
	D3D11_DSV_READ_ONLY_STENCIL = 0x2L
}   D3D11_DSV_FLAG;

typedef
  enum D3D11_BUFFER_UAV_FLAG
{
	D3D11_BUFFER_UAV_FLAG_RAW     = 0x1,
	D3D11_BUFFER_UAV_FLAG_APPEND  = 0x2,
	D3D11_BUFFER_UAV_FLAG_COUNTER = 0x4
}   D3D11_BUFFER_UAV_FLAG;

typedef
  enum D3D11_FILTER
{
	D3D11_FILTER_MIN_MAG_MIP_POINT                          = 0,
	D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR                   = 0x1,
	D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT             = 0x4,
	D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR                   = 0x5,
	D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT                   = 0x10,
	D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR            = 0x11,
	D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT                   = 0x14,
	D3D11_FILTER_MIN_MAG_MIP_LINEAR                         = 0x15,
	D3D11_FILTER_ANISOTROPIC                                = 0x55,
	D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT               = 0x80,
	D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR        = 0x81,
	D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT  = 0x84,
	D3D11_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR        = 0x85,
	D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT        = 0x90,
	D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR = 0x91,
	D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT        = 0x94,
	D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR              = 0x95,
	D3D11_FILTER_COMPARISON_ANISOTROPIC                     = 0xd5
}   D3D11_FILTER;

typedef
  enum D3D11_FILTER_TYPE
{
	D3D11_FILTER_TYPE_POINT  = 0,
	D3D11_FILTER_TYPE_LINEAR = 1
}   D3D11_FILTER_TYPE;

typedef
  enum D3D11_TEXTURE_ADDRESS_MODE
{
	D3D11_TEXTURE_ADDRESS_WRAP        = 1,
	D3D11_TEXTURE_ADDRESS_MIRROR      = 2,
	D3D11_TEXTURE_ADDRESS_CLAMP       = 3,
	D3D11_TEXTURE_ADDRESS_BORDER      = 4,
	D3D11_TEXTURE_ADDRESS_MIRROR_ONCE = 5
}   D3D11_TEXTURE_ADDRESS_MODE;

typedef
  enum D3D11_FORMAT_SUPPORT
{
	D3D11_FORMAT_SUPPORT_BUFFER                      = 0x1,
	D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER            = 0x2,
	D3D11_FORMAT_SUPPORT_IA_INDEX_BUFFER             = 0x4,
	D3D11_FORMAT_SUPPORT_SO_BUFFER                   = 0x8,
	D3D11_FORMAT_SUPPORT_TEXTURE1D                   = 0x10,
	D3D11_FORMAT_SUPPORT_TEXTURE2D                   = 0x20,
	D3D11_FORMAT_SUPPORT_TEXTURE3D                   = 0x40,
	D3D11_FORMAT_SUPPORT_TEXTURECUBE                 = 0x80,
	D3D11_FORMAT_SUPPORT_SHADER_LOAD                 = 0x100,
	D3D11_FORMAT_SUPPORT_SHADER_SAMPLE               = 0x200,
	D3D11_FORMAT_SUPPORT_SHADER_SAMPLE_COMPARISON    = 0x400,
	D3D11_FORMAT_SUPPORT_SHADER_SAMPLE_MONO_TEXT     = 0x800,
	D3D11_FORMAT_SUPPORT_MIP                         = 0x1000,
	D3D11_FORMAT_SUPPORT_MIP_AUTOGEN                 = 0x2000,
	D3D11_FORMAT_SUPPORT_RENDER_TARGET               = 0x4000,
	D3D11_FORMAT_SUPPORT_BLENDABLE                   = 0x8000,
	D3D11_FORMAT_SUPPORT_DEPTH_STENCIL               = 0x10000,
	D3D11_FORMAT_SUPPORT_CPU_LOCKABLE                = 0x20000,
	D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE         = 0x40000,
	D3D11_FORMAT_SUPPORT_DISPLAY                     = 0x80000,
	D3D11_FORMAT_SUPPORT_CAST_WITHIN_BIT_LAYOUT      = 0x100000,
	D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET    = 0x200000,
	D3D11_FORMAT_SUPPORT_MULTISAMPLE_LOAD            = 0x400000,
	D3D11_FORMAT_SUPPORT_SHADER_GATHER               = 0x800000,
	D3D11_FORMAT_SUPPORT_BACK_BUFFER_CAST            = 0x1000000,
	D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW = 0x2000000,
	D3D11_FORMAT_SUPPORT_SHADER_GATHER_COMPARISON    = 0x4000000
}   D3D11_FORMAT_SUPPORT;

typedef
  enum D3D11_FORMAT_SUPPORT2
{
	D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_ADD                               = 0x1,
	D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_BITWISE_OPS                       = 0x2,
	D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_COMPARE_STORE_OR_COMPARE_EXCHANGE = 0x4,
	D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_EXCHANGE                          = 0x8,
	D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_SIGNED_MIN_OR_MAX                 = 0x10,
	D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_UNSIGNED_MIN_OR_MAX               = 0x20,
	D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD                               = 0x40,
	D3D11_FORMAT_SUPPORT2_UAV_TYPED_STORE                              = 0x80
}   D3D11_FORMAT_SUPPORT2;

typedef
  enum D3D11_ASYNC_GETDATA_FLAG
{
	D3D11_ASYNC_GETDATA_DONOTFLUSH = 0x1
}   D3D11_ASYNC_GETDATA_FLAG;

typedef
  enum D3D11_QUERY
{
	D3D11_QUERY_EVENT                         = 0,
	D3D11_QUERY_OCCLUSION                     = (D3D11_QUERY_EVENT + 1),
	D3D11_QUERY_TIMESTAMP                     = (D3D11_QUERY_OCCLUSION + 1),
	D3D11_QUERY_TIMESTAMP_DISJOINT            = (D3D11_QUERY_TIMESTAMP + 1),
	D3D11_QUERY_PIPELINE_STATISTICS           = (D3D11_QUERY_TIMESTAMP_DISJOINT + 1),
	D3D11_QUERY_OCCLUSION_PREDICATE           = (D3D11_QUERY_PIPELINE_STATISTICS + 1),
	D3D11_QUERY_SO_STATISTICS                 = (D3D11_QUERY_OCCLUSION_PREDICATE + 1),
	D3D11_QUERY_SO_OVERFLOW_PREDICATE         = (D3D11_QUERY_SO_STATISTICS + 1),
	D3D11_QUERY_SO_STATISTICS_STREAM0         = (D3D11_QUERY_SO_OVERFLOW_PREDICATE + 1),
	D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM0 = (D3D11_QUERY_SO_STATISTICS_STREAM0 + 1),
	D3D11_QUERY_SO_STATISTICS_STREAM1         = (D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM0 + 1),
	D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM1 = (D3D11_QUERY_SO_STATISTICS_STREAM1 + 1),
	D3D11_QUERY_SO_STATISTICS_STREAM2         = (D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM1 + 1),
	D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM2 = (D3D11_QUERY_SO_STATISTICS_STREAM2 + 1),
	D3D11_QUERY_SO_STATISTICS_STREAM3         = (D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM2 + 1),
	D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM3 = (D3D11_QUERY_SO_STATISTICS_STREAM3 + 1)
}   D3D11_QUERY;

typedef
  enum D3D11_QUERY_MISC_FLAG
{
	D3D11_QUERY_MISC_PREDICATEHINT = 0x1
}   D3D11_QUERY_MISC_FLAG;

typedef
  enum D3D11_COUNTER
{
	D3D11_COUNTER_DEVICE_DEPENDENT_0 = 0x40000000
}   D3D11_COUNTER;

typedef
  enum D3D11_COUNTER_TYPE
{
	D3D11_COUNTER_TYPE_FLOAT32 = 0,
	D3D11_COUNTER_TYPE_UINT16  = (D3D11_COUNTER_TYPE_FLOAT32 + 1),
	D3D11_COUNTER_TYPE_UINT32  = (D3D11_COUNTER_TYPE_UINT16 + 1),
	D3D11_COUNTER_TYPE_UINT64  = (D3D11_COUNTER_TYPE_UINT32 + 1)
}   D3D11_COUNTER_TYPE;

typedef
  enum D3D11_STANDARD_MULTISAMPLE_QUALITY_LEVELS
{
	D3D11_STANDARD_MULTISAMPLE_PATTERN = 0xffffffff,
	D3D11_CENTER_MULTISAMPLE_PATTERN   = 0xfffffffe
}   D3D11_STANDARD_MULTISAMPLE_QUALITY_LEVELS;

typedef
  enum D3D11_DEVICE_CONTEXT_TYPE
{
	D3D11_DEVICE_CONTEXT_IMMEDIATE = 0,
	D3D11_DEVICE_CONTEXT_DEFERRED  = (D3D11_DEVICE_CONTEXT_IMMEDIATE + 1)
}   D3D11_DEVICE_CONTEXT_TYPE;

typedef
  enum D3D11_FEATURE
{
	D3D11_FEATURE_THREADING                = 0,
	D3D11_FEATURE_DOUBLES                  = (D3D11_FEATURE_THREADING + 1),
	D3D11_FEATURE_FORMAT_SUPPORT           = (D3D11_FEATURE_DOUBLES + 1),
	D3D11_FEATURE_FORMAT_SUPPORT2          = (D3D11_FEATURE_FORMAT_SUPPORT + 1),
	D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS = (D3D11_FEATURE_FORMAT_SUPPORT2 + 1)
}   D3D11_FEATURE;

typedef
  enum D3D11_CREATE_DEVICE_FLAG
{
	D3D11_CREATE_DEVICE_SINGLETHREADED                           = 0x1,
	D3D11_CREATE_DEVICE_DEBUG                                    = 0x2,
	D3D11_CREATE_DEVICE_SWITCH_TO_REF                            = 0x4,
	D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS = 0x8,
	D3D11_CREATE_DEVICE_BGRA_SUPPORT                             = 0x20
}   D3D11_CREATE_DEVICE_FLAG;

////////////////////////////////////////////////////////////////////////////
//  Structs
////////////////////////////////////////////////////////////////////////////

typedef struct D3D11_INPUT_ELEMENT_DESC
{
	LPCSTR                     SemanticName;
	UINT                       SemanticIndex;
	DXGI_FORMAT                Format;
	UINT                       InputSlot;
	UINT                       AlignedByteOffset;
	D3D11_INPUT_CLASSIFICATION InputSlotClass;
	UINT                       InstanceDataStepRate;
}   D3D11_INPUT_ELEMENT_DESC;

typedef struct D3D11_SO_DECLARATION_ENTRY
{
	UINT   Stream;
	LPCSTR SemanticName;
	UINT   SemanticIndex;
	BYTE   StartComponent;
	BYTE   ComponentCount;
	BYTE   OutputSlot;
}   D3D11_SO_DECLARATION_ENTRY;

typedef struct D3D11_VIEWPORT
{
	FLOAT TopLeftX;
	FLOAT TopLeftY;
	FLOAT Width;
	FLOAT Height;
	FLOAT MinDepth;
	FLOAT MaxDepth;
}   D3D11_VIEWPORT;

typedef struct D3D11_BOX
{
	UINT left;
	UINT top;
	UINT front;
	UINT right;
	UINT bottom;
	UINT back;
}   D3D11_BOX;

typedef struct D3D11_DEPTH_STENCILOP_DESC
{
	D3D11_STENCIL_OP      StencilFailOp;
	D3D11_STENCIL_OP      StencilDepthFailOp;
	D3D11_STENCIL_OP      StencilPassOp;
	D3D11_COMPARISON_FUNC StencilFunc;
}   D3D11_DEPTH_STENCILOP_DESC;

typedef struct D3D11_DEPTH_STENCIL_DESC
{
	BOOL                       DepthEnable;
	D3D11_DEPTH_WRITE_MASK     DepthWriteMask;
	D3D11_COMPARISON_FUNC      DepthFunc;
	BOOL                       StencilEnable;
	UINT8                      StencilReadMask;
	UINT8                      StencilWriteMask;
	D3D11_DEPTH_STENCILOP_DESC FrontFace;
	D3D11_DEPTH_STENCILOP_DESC BackFace;
}   D3D11_DEPTH_STENCIL_DESC;

typedef struct D3D11_RENDER_TARGET_BLEND_DESC
{
	BOOL           BlendEnable;
	D3D11_BLEND    SrcBlend;
	D3D11_BLEND    DestBlend;
	D3D11_BLEND_OP BlendOp;
	D3D11_BLEND    SrcBlendAlpha;
	D3D11_BLEND    DestBlendAlpha;
	D3D11_BLEND_OP BlendOpAlpha;
	UINT8          RenderTargetWriteMask;
}   D3D11_RENDER_TARGET_BLEND_DESC;

typedef struct D3D11_BLEND_DESC
{
	BOOL                           AlphaToCoverageEnable;
	BOOL                           IndependentBlendEnable;
	D3D11_RENDER_TARGET_BLEND_DESC RenderTarget[8];
}   D3D11_BLEND_DESC;

typedef struct D3D11_RASTERIZER_DESC
{
	D3D11_FILL_MODE FillMode;
	D3D11_CULL_MODE CullMode;
	BOOL            FrontCounterClockwise;
	INT             DepthBias;
	FLOAT           DepthBiasClamp;
	FLOAT           SlopeScaledDepthBias;
	BOOL            DepthClipEnable;
	BOOL            ScissorEnable;
	BOOL            MultisampleEnable;
	BOOL            AntialiasedLineEnable;
}   D3D11_RASTERIZER_DESC;

typedef struct D3D11_SUBRESOURCE_DATA
{
	const void* pSysMem;
	UINT        SysMemPitch;
	UINT        SysMemSlicePitch;
}   D3D11_SUBRESOURCE_DATA;

typedef struct D3D11_MAPPED_SUBRESOURCE
{
	void* pData;
	UINT  RowPitch;
	UINT  DepthPitch;
}   D3D11_MAPPED_SUBRESOURCE;

typedef struct D3D11_BUFFER_DESC
{
	UINT        ByteWidth;
	D3D11_USAGE Usage;
	UINT        BindFlags;
	UINT        CPUAccessFlags;
	UINT        MiscFlags;
	UINT        StructureByteStride;
}   D3D11_BUFFER_DESC;

typedef struct D3D11_TEXTURE1D_DESC
{
	UINT        Width;
	UINT        MipLevels;
	UINT        ArraySize;
	DXGI_FORMAT Format;
	D3D11_USAGE Usage;
	UINT        BindFlags;
	UINT        CPUAccessFlags;
	UINT        MiscFlags;
}   D3D11_TEXTURE1D_DESC;

typedef struct D3D11_TEXTURE2D_DESC
{
	UINT             Width;
	UINT             Height;
	UINT             MipLevels;
	UINT             ArraySize;
	DXGI_FORMAT      Format;
	DXGI_SAMPLE_DESC SampleDesc;
	D3D11_USAGE      Usage;
	UINT             BindFlags;
	UINT             CPUAccessFlags;
	UINT             MiscFlags;
}   D3D11_TEXTURE2D_DESC;

typedef struct D3D11_TEXTURE3D_DESC
{
	UINT        Width;
	UINT        Height;
	UINT        Depth;
	UINT        MipLevels;
	DXGI_FORMAT Format;
	D3D11_USAGE Usage;
	UINT        BindFlags;
	UINT        CPUAccessFlags;
	UINT        MiscFlags;
}   D3D11_TEXTURE3D_DESC;

typedef struct D3D11_BUFFER_SRV
{
	union
	{
		UINT FirstElement;
		UINT ElementOffset;
	};
	union
	{
		UINT NumElements;
		UINT ElementWidth;
	};
}   D3D11_BUFFER_SRV;

typedef struct D3D11_BUFFEREX_SRV
{
	UINT FirstElement;
	UINT NumElements;
	UINT Flags;
}   D3D11_BUFFEREX_SRV;

typedef struct D3D11_TEX1D_SRV
{
	UINT MostDetailedMip;
	UINT MipLevels;
}   D3D11_TEX1D_SRV;

typedef struct D3D11_TEX1D_ARRAY_SRV
{
	UINT MostDetailedMip;
	UINT MipLevels;
	UINT FirstArraySlice;
	UINT ArraySize;
}   D3D11_TEX1D_ARRAY_SRV;

typedef struct D3D11_TEX2D_SRV
{
	UINT MostDetailedMip;
	UINT MipLevels;
}   D3D11_TEX2D_SRV;

typedef struct D3D11_TEX2D_ARRAY_SRV
{
	UINT MostDetailedMip;
	UINT MipLevels;
	UINT FirstArraySlice;
	UINT ArraySize;
}   D3D11_TEX2D_ARRAY_SRV;

typedef struct D3D11_TEX3D_SRV
{
	UINT MostDetailedMip;
	UINT MipLevels;
}   D3D11_TEX3D_SRV;

typedef struct D3D11_TEXCUBE_SRV
{
	UINT MostDetailedMip;
	UINT MipLevels;
}   D3D11_TEXCUBE_SRV;

typedef struct D3D11_TEXCUBE_ARRAY_SRV
{
	UINT MostDetailedMip;
	UINT MipLevels;
	UINT First2DArrayFace;
	UINT NumCubes;
}   D3D11_TEXCUBE_ARRAY_SRV;

typedef struct D3D11_TEX2DMS_SRV
{
	UINT UnusedField_NothingToDefine;
}   D3D11_TEX2DMS_SRV;

typedef struct D3D11_TEX2DMS_ARRAY_SRV
{
	UINT FirstArraySlice;
	UINT ArraySize;
}   D3D11_TEX2DMS_ARRAY_SRV;

typedef struct D3D11_SHADER_RESOURCE_VIEW_DESC
{
	DXGI_FORMAT         Format;
	D3D11_SRV_DIMENSION ViewDimension;
	union
	{
		D3D11_BUFFER_SRV        Buffer;
		D3D11_TEX1D_SRV         Texture1D;
		D3D11_TEX1D_ARRAY_SRV   Texture1DArray;
		D3D11_TEX2D_SRV         Texture2D;
		D3D11_TEX2D_ARRAY_SRV   Texture2DArray;
		D3D11_TEX2DMS_SRV       Texture2DMS;
		D3D11_TEX2DMS_ARRAY_SRV Texture2DMSArray;
		D3D11_TEX3D_SRV         Texture3D;
		D3D11_TEXCUBE_SRV       TextureCube;
		D3D11_TEXCUBE_ARRAY_SRV TextureCubeArray;
		D3D11_BUFFEREX_SRV      BufferEx;
	};
}   D3D11_SHADER_RESOURCE_VIEW_DESC;

typedef struct D3D11_BUFFER_RTV
{
	union
	{
		UINT FirstElement;
		UINT ElementOffset;
	};
	union
	{
		UINT NumElements;
		UINT ElementWidth;
	};
}   D3D11_BUFFER_RTV;

typedef struct D3D11_TEX1D_RTV
{
	UINT MipSlice;
}   D3D11_TEX1D_RTV;

typedef struct D3D11_TEX1D_ARRAY_RTV
{
	UINT MipSlice;
	UINT FirstArraySlice;
	UINT ArraySize;
}   D3D11_TEX1D_ARRAY_RTV;

typedef struct D3D11_TEX2D_RTV
{
	UINT MipSlice;
}   D3D11_TEX2D_RTV;

typedef struct D3D11_TEX2DMS_RTV
{
	UINT UnusedField_NothingToDefine;
}   D3D11_TEX2DMS_RTV;

typedef struct D3D11_TEX2D_ARRAY_RTV
{
	UINT MipSlice;
	UINT FirstArraySlice;
	UINT ArraySize;
}   D3D11_TEX2D_ARRAY_RTV;

typedef struct D3D11_TEX2DMS_ARRAY_RTV
{
	UINT FirstArraySlice;
	UINT ArraySize;
}   D3D11_TEX2DMS_ARRAY_RTV;

typedef struct D3D11_TEX3D_RTV
{
	UINT MipSlice;
	UINT FirstWSlice;
	UINT WSize;
}   D3D11_TEX3D_RTV;

typedef struct D3D11_RENDER_TARGET_VIEW_DESC
{
	DXGI_FORMAT         Format;
	D3D11_RTV_DIMENSION ViewDimension;
	union
	{
		D3D11_BUFFER_RTV        Buffer;
		D3D11_TEX1D_RTV         Texture1D;
		D3D11_TEX1D_ARRAY_RTV   Texture1DArray;
		D3D11_TEX2D_RTV         Texture2D;
		D3D11_TEX2D_ARRAY_RTV   Texture2DArray;
		D3D11_TEX2DMS_RTV       Texture2DMS;
		D3D11_TEX2DMS_ARRAY_RTV Texture2DMSArray;
		D3D11_TEX3D_RTV         Texture3D;
	};
}   D3D11_RENDER_TARGET_VIEW_DESC;

typedef struct D3D11_TEX1D_DSV
{
	UINT MipSlice;
}   D3D11_TEX1D_DSV;

typedef struct D3D11_TEX1D_ARRAY_DSV
{
	UINT MipSlice;
	UINT FirstArraySlice;
	UINT ArraySize;
}   D3D11_TEX1D_ARRAY_DSV;

typedef struct D3D11_TEX2D_DSV
{
	UINT MipSlice;
}   D3D11_TEX2D_DSV;

typedef struct D3D11_TEX2D_ARRAY_DSV
{
	UINT MipSlice;
	UINT FirstArraySlice;
	UINT ArraySize;
}   D3D11_TEX2D_ARRAY_DSV;

typedef struct D3D11_TEX2DMS_DSV
{
	UINT UnusedField_NothingToDefine;
}   D3D11_TEX2DMS_DSV;

typedef struct D3D11_TEX2DMS_ARRAY_DSV
{
	UINT FirstArraySlice;
	UINT ArraySize;
}   D3D11_TEX2DMS_ARRAY_DSV;

typedef struct D3D11_DEPTH_STENCIL_VIEW_DESC
{
	DXGI_FORMAT         Format;
	D3D11_DSV_DIMENSION ViewDimension;
	UINT                Flags;
	union
	{
		D3D11_TEX1D_DSV         Texture1D;
		D3D11_TEX1D_ARRAY_DSV   Texture1DArray;
		D3D11_TEX2D_DSV         Texture2D;
		D3D11_TEX2D_ARRAY_DSV   Texture2DArray;
		D3D11_TEX2DMS_DSV       Texture2DMS;
		D3D11_TEX2DMS_ARRAY_DSV Texture2DMSArray;
	};
}   D3D11_DEPTH_STENCIL_VIEW_DESC;

typedef struct D3D11_BUFFER_UAV
{
	UINT FirstElement;
	UINT NumElements;
	UINT Flags;
}   D3D11_BUFFER_UAV;

typedef struct D3D11_TEX1D_UAV
{
	UINT MipSlice;
}   D3D11_TEX1D_UAV;

typedef struct D3D11_TEX1D_ARRAY_UAV
{
	UINT MipSlice;
	UINT FirstArraySlice;
	UINT ArraySize;
}   D3D11_TEX1D_ARRAY_UAV;

typedef struct D3D11_TEX2D_UAV
{
	UINT MipSlice;
}   D3D11_TEX2D_UAV;

typedef struct D3D11_TEX2D_ARRAY_UAV
{
	UINT MipSlice;
	UINT FirstArraySlice;
	UINT ArraySize;
}   D3D11_TEX2D_ARRAY_UAV;

typedef struct D3D11_TEX3D_UAV
{
	UINT MipSlice;
	UINT FirstWSlice;
	UINT WSize;
}   D3D11_TEX3D_UAV;

typedef struct D3D11_UNORDERED_ACCESS_VIEW_DESC
{
	DXGI_FORMAT         Format;
	D3D11_UAV_DIMENSION ViewDimension;
	union
	{
		D3D11_BUFFER_UAV      Buffer;
		D3D11_TEX1D_UAV       Texture1D;
		D3D11_TEX1D_ARRAY_UAV Texture1DArray;
		D3D11_TEX2D_UAV       Texture2D;
		D3D11_TEX2D_ARRAY_UAV Texture2DArray;
		D3D11_TEX3D_UAV       Texture3D;
	};
}   D3D11_UNORDERED_ACCESS_VIEW_DESC;

typedef struct D3D11_SAMPLER_DESC
{
	D3D11_FILTER               Filter;
	D3D11_TEXTURE_ADDRESS_MODE AddressU;
	D3D11_TEXTURE_ADDRESS_MODE AddressV;
	D3D11_TEXTURE_ADDRESS_MODE AddressW;
	FLOAT                      MipLODBias;
	UINT                       MaxAnisotropy;
	D3D11_COMPARISON_FUNC      ComparisonFunc;
	FLOAT                      BorderColor[4];
	FLOAT                      MinLOD;
	FLOAT                      MaxLOD;
}   D3D11_SAMPLER_DESC;

typedef struct D3D11_QUERY_DESC
{
	D3D11_QUERY Query;
	UINT        MiscFlags;
}   D3D11_QUERY_DESC;

typedef struct D3D11_QUERY_DATA_TIMESTAMP_DISJOINT
{
	UINT64 Frequency;
	BOOL   Disjoint;
}   D3D11_QUERY_DATA_TIMESTAMP_DISJOINT;

typedef struct D3D11_QUERY_DATA_PIPELINE_STATISTICS
{
	UINT64 IAVertices;
	UINT64 IAPrimitives;
	UINT64 VSInvocations;
	UINT64 GSInvocations;
	UINT64 GSPrimitives;
	UINT64 CInvocations;
	UINT64 CPrimitives;
	UINT64 PSInvocations;
	UINT64 HSInvocations;
	UINT64 DSInvocations;
	UINT64 CSInvocations;
}   D3D11_QUERY_DATA_PIPELINE_STATISTICS;

typedef struct D3D11_QUERY_DATA_SO_STATISTICS
{
	UINT64 NumPrimitivesWritten;
	UINT64 PrimitivesStorageNeeded;
}   D3D11_QUERY_DATA_SO_STATISTICS;

typedef struct D3D11_COUNTER_DESC
{
	D3D11_COUNTER Counter;
	UINT          MiscFlags;
}   D3D11_COUNTER_DESC;

typedef struct D3D11_COUNTER_INFO
{
	D3D11_COUNTER LastDeviceDependentCounter;
	UINT          NumSimultaneousCounters;
	UINT8         NumDetectableParallelUnits;
}   D3D11_COUNTER_INFO;

typedef struct D3D11_CLASS_INSTANCE_DESC
{
	UINT InstanceId;
	UINT InstanceIndex;
	UINT TypeId;
	UINT ConstantBuffer;
	UINT BaseConstantBufferOffset;
	UINT BaseTexture;
	UINT BaseSampler;
	BOOL Created;
}   D3D11_CLASS_INSTANCE_DESC;

typedef struct D3D11_FEATURE_DATA_THREADING
{
	BOOL DriverConcurrentCreates;
	BOOL DriverCommandLists;
}   D3D11_FEATURE_DATA_THREADING;

typedef struct D3D11_FEATURE_DATA_DOUBLES
{
	BOOL DoublePrecisionFloatShaderOps;
}   D3D11_FEATURE_DATA_DOUBLES;

typedef struct D3D11_FEATURE_DATA_FORMAT_SUPPORT
{
	DXGI_FORMAT InFormat;
	UINT        OutFormatSupport;
}   D3D11_FEATURE_DATA_FORMAT_SUPPORT;

typedef struct D3D11_FEATURE_DATA_FORMAT_SUPPORT2
{
	DXGI_FORMAT InFormat;
	UINT        OutFormatSupport2;
}   D3D11_FEATURE_DATA_FORMAT_SUPPORT2;

typedef struct D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS
{
	BOOL ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x;
}   D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS;

////////////////////////////////////////////////////////////////////////////
//  Constants
////////////////////////////////////////////////////////////////////////////

const UINT D3D11_APPEND_ALIGNED_ELEMENT = 0xffffffff;

////////////////////////////////////////////////////////////////////////////
//  Forward declaration of unused interfaces
////////////////////////////////////////////////////////////////////////////

//struct ID3D11DeviceChild;         // Typedef as CCryDXGLDeviceChild
//struct ID3D11DepthStencilState;   // Typedef as CCryDXGLDepthStencilState
//struct ID3D11BlendState;          // Typedef as CCryDXGLBlendState
//struct ID3D11RasterizerState;     // Typedef as CCryDXGLRasterizerState
//struct ID3D11Resource;            // Typedef as CCryDXGLResource
//struct ID3D11Buffer;              // Typedef as CCryDXGLBuffer
//struct ID3D11Texture1D;           // Typedef as CCryDXGLTexture1D
//struct ID3D11Texture2D;           // Typedef as CCryDXGLTexture2D
//struct ID3D11Texture3D;           // Typedef as CCryDXGLTexture3D
//struct ID3D11View;                // Typedef as CCryDXGLView
//struct ID3D11ShaderResourceView;  // Typedef as CCryDXGLShaderResourceView
//struct ID3D11RenderTargetView;    // Typedef as CCryDXGLRenderTargetView
//struct ID3D11DepthStencilView;    // Typedef as CCryDXGLDepthStencilView
//struct ID3D11UnorderedAccessView; // Typedef as CCryDXGLUnorderedAccessView
//struct ID3D11VertexShader;        // Typedef as CCryDXGLVertexShader
//struct ID3D11HullShader;          // Typedef as CCryDXGLHullShader
//struct ID3D11DomainShader;        // Typedef as CCryDXGLDomainShader
//struct ID3D11GeometryShader;      // Typedef as CCryDXGLGeometryShader
//struct ID3D11PixelShader;         // Typedef as CCryDXGLPixelShader
//struct ID3D11ComputeShader;       // Typedef as CCryDXGLComputeShader
//struct ID3D11InputLayout;         // Typedef as CCryDXGLInputLayout
//struct ID3D11SamplerState;        // Typedef as CCryDXGLSamplerState
//struct ID3D11Asynchronous;        // Typedef as CCryDXGLQuery
//struct ID3D11Query;               // Typedef as CCryDXGLQuery
//struct ID3D11Predicate            // Typedef as CCryDXGLQuery
//struct ID3D11Counter              // Typedef as CCryDXGLDeviceChild
//struct ID3D11ClassLinkage         // Typedef as CCryDXGLDeviceChild
struct ID3D11ClassInstance;
struct ID3D11CommandList;
//struct ID3D11DeviceContext;       // Typedef as CCryDXGLDeviceContext
//struct ID3D11Device;              // Typedef as CCryDXGLDevice

////////////////////////////////////////////////////////////////////////////
//  Interfaces for full DX emulation
////////////////////////////////////////////////////////////////////////////

#if DXGL_FULL_EMULATION

struct ID3D11DeviceChild : IUnknown
{
	virtual void STDMETHODCALLTYPE    GetDevice(ID3D11Device** ppDevice) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void* pData) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) = 0;
};

struct ID3D11ClassLinkage : ID3D11DeviceChild {};
struct ID3D11ClassInstance : ID3D11DeviceChild {};
struct ID3D11CommandList : ID3D11DeviceChild {};

struct ID3D11DepthStencilState : ID3D11DeviceChild
{
	virtual void STDMETHODCALLTYPE GetDesc(D3D11_DEPTH_STENCIL_DESC* pDesc) = 0;
};

struct ID3D11BlendState : ID3D11DeviceChild
{
	virtual void STDMETHODCALLTYPE GetDesc(D3D11_BLEND_DESC* pDesc) = 0;
};

struct ID3D11RasterizerState : ID3D11DeviceChild
{
	virtual void STDMETHODCALLTYPE GetDesc(D3D11_RASTERIZER_DESC* pDesc) = 0;
};

struct ID3D11Resource : ID3D11DeviceChild
{
	virtual void STDMETHODCALLTYPE GetType(D3D11_RESOURCE_DIMENSION* pResourceDimension) = 0;
	virtual void STDMETHODCALLTYPE SetEvictionPriority(UINT EvictionPriority) = 0;
	virtual UINT STDMETHODCALLTYPE GetEvictionPriority() = 0;
};

struct ID3D11Buffer : ID3D11Resource
{
	virtual void STDMETHODCALLTYPE GetDesc(D3D11_BUFFER_DESC* pDesc) = 0;
};

struct ID3D11Texture1D : ID3D11Resource
{
	virtual void STDMETHODCALLTYPE GetDesc(D3D11_TEXTURE1D_DESC* pDesc) = 0;
};

struct ID3D11Texture2D : ID3D11Resource
{
	virtual void STDMETHODCALLTYPE GetDesc(D3D11_TEXTURE2D_DESC* pDesc) = 0;
};

struct ID3D11Texture3D : ID3D11Resource
{
	virtual void STDMETHODCALLTYPE GetDesc(D3D11_TEXTURE3D_DESC* pDesc) = 0;
};

struct ID3D11View : ID3D11DeviceChild
{
	virtual void STDMETHODCALLTYPE GetResource(ID3D11Resource** ppResource) = 0;
};

struct ID3D11ShaderResourceView : ID3D11View
{
	virtual void STDMETHODCALLTYPE GetDesc(D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc) = 0;
};

struct ID3D11RenderTargetView : ID3D11View
{
	virtual void STDMETHODCALLTYPE GetDesc(D3D11_RENDER_TARGET_VIEW_DESC* pDesc) = 0;
};

struct ID3D11DepthStencilView : ID3D11View
{
	virtual void STDMETHODCALLTYPE GetDesc(D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc) = 0;
};

struct ID3D11UnorderedAccessView : ID3D11View
{
	virtual void STDMETHODCALLTYPE GetDesc(D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc) = 0;
};

struct ID3D11VertexShader : ID3D11DeviceChild {};

struct ID3D11HullShader : ID3D11DeviceChild {};

struct ID3D11DomainShader : ID3D11DeviceChild {};

struct ID3D11GeometryShader : ID3D11DeviceChild {};

struct ID3D11PixelShader : ID3D11DeviceChild {};

struct ID3D11ComputeShader : ID3D11DeviceChild {};

struct ID3D11InputLayout : ID3D11DeviceChild {};

struct ID3D11SamplerState : ID3D11DeviceChild
{
	virtual void STDMETHODCALLTYPE GetDesc(D3D11_SAMPLER_DESC* pDesc) = 0;
};

struct ID3D11Asynchronous : ID3D11DeviceChild
{
	virtual UINT STDMETHODCALLTYPE GetDataSize() = 0;
};

struct ID3D11Query : ID3D11Asynchronous
{
	virtual void STDMETHODCALLTYPE GetDesc(D3D11_QUERY_DESC* pDesc) = 0;
};

struct ID3D11Counter : ID3D11Asynchronous {};

struct ID3D11Predicate : ID3D11Query
{
};

//struct ID3D11Counter : ID3D11Asynchronous
//{
//	virtual void STDMETHODCALLTYPE GetDesc(D3D11_COUNTER_DESC *pDesc) = 0;
//};
//
//struct ID3D11ClassLinkage : ID3D11DeviceChild
//{
//	virtual HRESULT STDMETHODCALLTYPE GetClassInstance(LPCSTR pClassInstanceName, UINT InstanceIndex, ID3D11ClassInstance **ppInstance) = 0;
//	virtual HRESULT STDMETHODCALLTYPE CreateClassInstance(LPCSTR pClassTypeName, UINT ConstantBufferOffset, UINT ConstantVectorOffset, UINT TextureOffset, UINT SamplerOffset, ID3D11ClassInstance **ppInstance) = 0;
//};

struct ID3D11SwitchToRef : IUnknown
{
	virtual BOOL STDMETHODCALLTYPE SetUseRef(BOOL UseRef) = 0;
	virtual BOOL STDMETHODCALLTYPE GetUseRef() = 0;
};

#endif //DXGL_FULL_EMULATION

#if defined(DXGL_BLOB_INTEROPERABILITY) || DXGL_FULL_EMULATION

struct ID3D10Blob : IUnknown
{
	virtual LPVOID STDMETHODCALLTYPE GetBufferPointer() = 0;
	virtual SIZE_T STDMETHODCALLTYPE GetBufferSize() = 0;
};

#endif //defined(DXGL_BLOB_INTEROPERABILITY) || DXGL_FULL_EMULATION

#endif // __DXGL_D3D11_h__
