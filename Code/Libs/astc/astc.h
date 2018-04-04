#pragma once

#include <stdint.h>

enum astc_decode_mode
{
	DECODE_LDR_SRGB,
	DECODE_LDR,
	DECODE_HDR
};

#define MAX_TEXELS_PER_BLOCK 216
#define MAX_WEIGHTS_PER_BLOCK 64

struct error_weighting_params
{
	float rgb_power;
	float rgb_base_weight;
	float rgb_mean_weight;
	float rgb_stdev_weight;
	float alpha_power;
	float alpha_base_weight;
	float alpha_mean_weight;
	float alpha_stdev_weight;
	float rgb_mean_and_stdev_mixing;
	int mean_stdev_radius;
	int enable_rgb_scale_with_alpha;
	int alpha_radius;
	int ra_normal_angular_scale;
	float block_artifact_suppression;
	float rgba_weights[4];

	float block_artifact_suppression_expanded[MAX_TEXELS_PER_BLOCK];

	// parameters that deal with heuristic codec speedups
	int partition_search_limit;
	float block_mode_cutoff;
	float texel_avg_error_limit;
	float partition_1_to_2_limit;
	float lowest_correlation_cutoff;
	int max_refinement_iters;
};

struct swizzlepattern
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
};

struct symbolic_compressed_block
{
	int error_block;			// 1 marks error block, 0 marks non-error-block.
	int block_mode;				// 0 to 2047. Negative value marks constant-color block (-1: FP16, -2:UINT16)
	int partition_count;		// 1 to 4; Zero marks a constant-color block.
	int partition_index;		// 0 to 1023
	int color_formats[4];		// color format for each endpoint color pair.
	int color_formats_matched;	// color format for all endpoint pairs are matched.
	int color_values[4][12];	// quantized endpoint color pairs.
	int color_quantization_level;
	uint8_t plane1_weights[MAX_WEIGHTS_PER_BLOCK];	// quantized and decimated weights
	uint8_t plane2_weights[MAX_WEIGHTS_PER_BLOCK];
	int plane2_color_component;	// color component for the secondary plane of weights
	int constant_color[4];		// constant-color, as FP16 or UINT16. Used for constant-color blocks only.
};

struct physical_compressed_block
{
	uint8_t data[16];
};

struct imageblock
{
	float orig_data[MAX_TEXELS_PER_BLOCK * 4];	// original input data
	float work_data[MAX_TEXELS_PER_BLOCK * 4];	// the data that we will compress, either linear or LNS (0..65535 in both cases)
	float deriv_data[MAX_TEXELS_PER_BLOCK * 4];	// derivative of the conversion function used, used ot modify error weighting

	uint8_t rgb_lns[MAX_TEXELS_PER_BLOCK * 4];	// 1 if RGB data are being trated as LNS
	uint8_t alpha_lns[MAX_TEXELS_PER_BLOCK * 4];	// 1 if Alpha data are being trated as LNS
	uint8_t nan_texel[MAX_TEXELS_PER_BLOCK * 4];	// 1 if the texel is a NaN-texel.

	float red_min, red_max;
	float green_min, green_max;
	float blue_min, blue_max;
	float alpha_min, alpha_max;
	int grayscale;				// 1 if R=G=B for every pixel, 0 otherwise

	int xpos, ypos, zpos;
};

extern int rgb_force_use_of_hdr;
extern int alpha_force_use_of_hdr;

// Table generation

struct partition_info;
const partition_info *get_partition_table(int xdim, int ydim, int zdim, int partition_count);

struct block_size_descriptor;
const block_size_descriptor *get_block_size_descriptor(int xdim, int ydim, int zdim);

void prepare_angular_tables(void);

void build_quantization_mode_table(void);

// Allocation

struct astc_codec_image
{
	uint8_t ***imagedata8;
	uint16_t ***imagedata16;
	int xsize;
	int ysize;
	int zsize;
	int padding;
};

void destroy_image(astc_codec_image * img);
astc_codec_image *allocate_image(int bitness, int xsize, int ysize, int zsize, int padding);
void initialize_image(astc_codec_image * img);
void fill_image_padding_area(astc_codec_image * img);

// Decompress

void decompress_symbolic_block(astc_decode_mode decode_mode,
							   // dimensions of block
							   int xdim, int ydim, int zdim,
							   // position of block
							   int xpos, int ypos, int zpos, const symbolic_compressed_block * scb, imageblock * blk);

void physical_to_symbolic(int xdim, int ydim, int zdim, physical_compressed_block pb, symbolic_compressed_block * res);

// write an image block to the output file buffer.
// the data written are taken from orig_data.
void write_imageblock(astc_codec_image * img, const imageblock * pb,	// picture-block to imitialize with image data
					  // block dimensions
					  int xdim, int ydim, int zdim,
					  // position in picture to write block to.
					  int xpos, int ypos, int zpos, swizzlepattern swz);

// Compress

void expand_block_artifact_suppression(int xdim, int ydim, int zdim, error_weighting_params * ewp);

float compress_symbolic_block(const astc_codec_image * input_image,
							  astc_decode_mode decode_mode, int xdim, int ydim, int zdim, const error_weighting_params * ewp, const imageblock * blk, symbolic_compressed_block * scb);

physical_compressed_block symbolic_to_physical(int xdim, int ydim, int zdim, const symbolic_compressed_block * sc);

// fetch an image-block from the input file
void fetch_imageblock(const astc_codec_image * img, imageblock * pb,	// picture-block to imitialize with image data
					  // block dimensions
					  int xdim, int ydim, int zdim,
					  // position in picture to fetch block from
					  int xpos, int ypos, int zpos, swizzlepattern swz);

// function to compute regional averages and variances for an image
void compute_averages_and_variances(const astc_codec_image * img, float rgb_power_to_use, float alpha_power_to_use, int avg_kernel_radius, int var_kernel_radius, swizzlepattern swz);
