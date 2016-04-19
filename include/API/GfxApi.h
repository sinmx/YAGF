// Copyright (C) 2015 Vincent Lejeune
// For conditions of distribution and use, see copyright notice in License.txt
#pragma once

#include <vector>
#include <tuple>
#include <array>
#include "..\Core\SColor.h"

namespace irr
{
	namespace video
	{
		enum class E_INDEX_TYPE
		{
			EIT_16BIT,
			EIT_32BIT
		};

		enum class E_POLYGON_MODE
		{
			EPM_FILL,
			EPM_LINE,
			EPM_POINT
		};

		enum class E_CULL_MODE
		{
			ECM_NONE,
			ECM_FRONT,
			ECM_BACK,
			ECM_FRONT_AND_BACK
		};

		enum class E_FRONT_FACE
		{
			EFF_CCW,
			EFF_CW
		};

		enum class E_SAMPLE_COUNT
		{
			ESC_1,
			ESC_2,
			ESC_4,
			ESC_8
		};

		enum class E_PRIMITIVE_TYPE
		{
			EPT_POINTS,
			EPT_LINE_STRIP,
			EPT_LINES,
			EPT_TRIANGLE_STRIP,
			EPT_TRIANGLES,
		};

		enum class E_COMPARE_FUNCTION
		{
			ECF_LESS,
			ECF_NEVER,
		};

		enum class E_STENCIL_OP
		{
			ESO_KEEP,
		};

		enum class E_ASPECT
		{
			EA_COLOR,
			EA_DEPTH,
			EA_STENCIL,
			EA_DEPTH_STENCIL
		};
	}
}

enum class RESOURCE_USAGE
{
	PRESENT,
	COPY_DEST,
	COPY_SRC,
	RENDER_TARGET,
	READ_GENERIC,
	DEPTH_WRITE,
	undefined,
};

enum image_flags
{
	usage_transfer_src = 0x1,
	usage_transfer_dst = 0x2,
	usage_sampled = 0x4,
	usage_render_target = 0x8,
	usage_depth_stencil = 0x10,
	usage_input_attachment = 0x20,
	usage_cube = 0x40,
};

enum class SAMPLER_TYPE
{
	NEAREST,
	BILINEAR,
	TRILINEAR,
	ANISOTROPIC,
};

struct MipLevelData
{
	size_t Offset;
	size_t Width;
	size_t Height;
	size_t RowPitch;
};


enum class RESOURCE_VIEW
{
	CONSTANTS_BUFFER,
	SHADER_RESOURCE,
	SAMPLER,
	UAV,
};



struct range_of_descriptors
{
	const RESOURCE_VIEW range_type;
	const uint32_t bind_point;
	const uint32_t count;

	constexpr range_of_descriptors(const RESOURCE_VIEW rt, const uint32_t bindpoint, const uint32_t range_size)
		: range_type(rt), bind_point(bindpoint), count(range_size)
	{}
};

struct descriptor_set_
{
	const range_of_descriptors *descriptors_ranges;
	const uint32_t count;

	constexpr descriptor_set_(const range_of_descriptors * ptr, const uint32_t cnt) : descriptors_ranges(ptr), count(cnt)
	{ }
};

template<size_t N>
constexpr descriptor_set_ descriptor_set(const range_of_descriptors (&arr)[N])
{
	return descriptor_set_(arr, N);
}

template<int N>
struct pipeline_layout_description_
{
	const std::array<descriptor_set_, N> descriptors_sets;

	constexpr pipeline_layout_description_(const std::array<descriptor_set_, N> arr) : descriptors_sets(arr)
	{}
};

template<typename...T>
constexpr pipeline_layout_description_<sizeof...(T)> pipeline_layout_description(const T...args)
{
	return pipeline_layout_description_<sizeof...(T)>({ args... });
}


struct pipeline_state_description
{
	const bool rasterization_depth_clamp_enable;
	const bool rasterization_discard_enable;
	const irr::video::E_POLYGON_MODE rasterization_polygon_mode;
	const irr::video::E_CULL_MODE rasterization_cull_mode;
	const irr::video::E_FRONT_FACE rasterization_front_face;
	const bool rasterization_depth_bias_enable;
	const float rasterization_depth_bias_constant_factor;
	const float rasterization_depth_bias_clamp;
	const float rasterization_depth_bias_slope_factor;
	const float rasterization_line_width;
	const bool rasterization_conservative_enable;

	const bool multisample_multisample_enable;
	const irr::video::E_SAMPLE_COUNT multisample_sample_count;
	const float multisample_min_sample_shading;
	// sample mask ?
	const bool multisample_alpha_to_coverage;
	const bool multisample_alpha_to_one;

	const irr::video::E_PRIMITIVE_TYPE input_assembly_topology;
	const bool input_assembly_primitive_restart;

	const bool depth_stencil_depth_test;
	const bool depth_stencil_depth_write;
	const irr::video::E_COMPARE_FUNCTION depth_stencil_depth_compare_op;
	const bool depth_stencil_depth_clip_enable;
	const bool depth_stencil_stencil_test;
	const irr::video::E_STENCIL_OP depth_stencil_front_stencil_fail_op;
	const irr::video::E_STENCIL_OP depth_stencil_front_stencil_depth_fail_op;
	const irr::video::E_STENCIL_OP depth_stencil_front_stencil_pass_op;
	const irr::video::E_COMPARE_FUNCTION depth_stencil_front_stencil_compare_op;
	const irr::video::E_STENCIL_OP depth_stencil_back_stencil_fail_op;
	const irr::video::E_STENCIL_OP depth_stencil_back_stencil_depth_fail_op;
	const irr::video::E_STENCIL_OP depth_stencil_back_stencil_pass_op;
	const irr::video::E_COMPARE_FUNCTION depth_stencil_back_stencil_compare_op;
	const float depth_stencil_min_depth_clip;
	const float depth_stencil_max_depth_clip;

	static constexpr pipeline_state_description get()
	{
		return pipeline_state_description(false, false, irr::video::E_POLYGON_MODE::EPM_FILL, irr::video::E_CULL_MODE::ECM_BACK, irr::video::E_FRONT_FACE::EFF_CW, false, 0.f, 0.f, 0.f, 1.f, false,
			false, irr::video::E_SAMPLE_COUNT::ESC_1, 0.f, false, false, irr::video::E_PRIMITIVE_TYPE::EPT_TRIANGLES, false, true, true, irr::video::E_COMPARE_FUNCTION::ECF_LESS, false, false,
			irr::video::E_STENCIL_OP::ESO_KEEP, irr::video::E_STENCIL_OP::ESO_KEEP, irr::video::E_STENCIL_OP::ESO_KEEP, irr::video::E_COMPARE_FUNCTION::ECF_NEVER,
			irr::video::E_STENCIL_OP::ESO_KEEP, irr::video::E_STENCIL_OP::ESO_KEEP, irr::video::E_STENCIL_OP::ESO_KEEP, irr::video::E_COMPARE_FUNCTION::ECF_NEVER,
			0.f, 1.f);
	}

private:
	constexpr pipeline_state_description(
		bool depth_clamp_enable,
		bool rasterizer_discard_enable,
		irr::video::E_POLYGON_MODE polygon_mode,
		irr::video::E_CULL_MODE cull_mode,
		irr::video::E_FRONT_FACE front_face,
		bool depth_bias_enable,
		float depth_bias_constant_factor,
		float depth_bias_clamp,
		float depth_bias_slope_factor,
		float line_width,
		bool conservative_enable,
		bool multisample_enable,
		irr::video::E_SAMPLE_COUNT sample_count,
		float min_sample_shading,
		bool alpha_to_coverage,
		bool alpha_to_one,
		irr::video::E_PRIMITIVE_TYPE topology,
		bool primitive_restart,
		bool depth_test,
		bool depth_write,
		irr::video::E_COMPARE_FUNCTION depth_compare_op,
		bool depth_clip_enable,
		bool stencil_test,
		irr::video::E_STENCIL_OP front_stencil_fail_op,
		irr::video::E_STENCIL_OP front_stencil_depth_fail_op,
		irr::video::E_STENCIL_OP front_stencil_pass_op,
		irr::video::E_COMPARE_FUNCTION front_stencil_compare_op,
		irr::video::E_STENCIL_OP back_stencil_fail_op,
		irr::video::E_STENCIL_OP back_stencil_depth_fail_op,
		irr::video::E_STENCIL_OP back_stencil_pass_op,
		irr::video::E_COMPARE_FUNCTION back_stencil_compare_op,
		float min_depth_clip,
		float max_depth_clip
	)
		: rasterization_depth_clamp_enable(depth_clamp_enable),
		rasterization_discard_enable(rasterizer_discard_enable),
		rasterization_polygon_mode(polygon_mode),
		rasterization_cull_mode(cull_mode),
		rasterization_front_face(front_face),
		rasterization_depth_bias_enable(depth_bias_enable),
		rasterization_depth_bias_constant_factor(depth_bias_constant_factor),
		rasterization_depth_bias_clamp(depth_bias_clamp),
		rasterization_depth_bias_slope_factor(depth_bias_slope_factor),
		rasterization_line_width(line_width),
		rasterization_conservative_enable(conservative_enable),
		multisample_multisample_enable(multisample_enable),
		multisample_sample_count(sample_count),
		multisample_min_sample_shading(min_sample_shading),
		multisample_alpha_to_coverage(alpha_to_coverage),
		multisample_alpha_to_one(alpha_to_one),
		input_assembly_topology(topology),
		input_assembly_primitive_restart(primitive_restart),
		depth_stencil_depth_test(depth_test),
		depth_stencil_depth_write(depth_write),
		depth_stencil_depth_compare_op(depth_compare_op),
		depth_stencil_depth_clip_enable(depth_clip_enable),
		depth_stencil_stencil_test(stencil_test),
		depth_stencil_front_stencil_fail_op(front_stencil_fail_op),
		depth_stencil_front_stencil_depth_fail_op(front_stencil_depth_fail_op),
		depth_stencil_front_stencil_pass_op(front_stencil_pass_op),
		depth_stencil_front_stencil_compare_op(front_stencil_compare_op),
		depth_stencil_back_stencil_fail_op(back_stencil_fail_op),
		depth_stencil_back_stencil_depth_fail_op(back_stencil_depth_fail_op),
		depth_stencil_back_stencil_pass_op(back_stencil_pass_op),
		depth_stencil_back_stencil_compare_op(back_stencil_compare_op),
		depth_stencil_min_depth_clip(min_depth_clip),
		depth_stencil_max_depth_clip(max_depth_clip)
	{ }
};



command_list_storage_t create_command_storage(device_t dev);
command_list_t create_command_list(device_t dev, command_list_storage_t storage);
void reset_command_list_storage(device_t dev, command_list_storage_t storage);
buffer_t create_buffer(device_t dev, size_t size);
image_t create_image(device_t dev, irr::video::ECOLOR_FORMAT format, uint32_t width, uint32_t height, uint16_t mipmap, uint32_t flags, clear_value_structure_t *clear_value);
descriptor_storage_t create_descriptor_storage(device_t dev, uint32_t num_sets, const std::vector<std::tuple<RESOURCE_VIEW, uint32_t> > &num_descriptors);
void copy_buffer_to_image_subresource(command_list_t list, image_t destination_image, uint32_t destination_subresource, buffer_t source, uint64_t offset_in_buffer,
	uint32_t width, uint32_t height, uint32_t row_pitch, irr::video::ECOLOR_FORMAT format);
//framebuffer_t create_frame_buffer(device_t dev, std::vector<std::tuple<image_t, irr::video::ECOLOR_FORMAT>> render_targets);
framebuffer_t create_frame_buffer(device_t dev, std::vector<std::tuple<image_t, irr::video::ECOLOR_FORMAT>> render_targets, std::tuple<image_t, irr::video::ECOLOR_FORMAT> depth_stencil_texture, uint32_t width, uint32_t height, render_pass_t render_pass);

void* map_buffer(device_t dev, buffer_t buffer);
void unmap_buffer(device_t dev, buffer_t buffer);

void start_command_list_recording(device_t dev, command_list_t command_list, command_list_storage_t storage);
void make_command_list_executable(command_list_t command_list);
void wait_for_command_queue_idle(device_t dev, command_queue_t command_queue);
void present(device_t dev, command_queue_t cmdqueue, swap_chain_t chain, uint32_t backbuffer_index);
void set_pipeline_barrier(device_t dev, command_list_t command_list, image_t resource, RESOURCE_USAGE before, RESOURCE_USAGE after, uint32_t subresource, irr::video::E_ASPECT);


enum class depth_stencil_aspect
{
	depth_only,
	stencil_only,
	depth_and_stencil
};

void set_viewport(command_list_t command_list, float x, float width, float y, float height, float min_depth, float max_depth);
void set_scissor(command_list_t command_list, uint32_t left, uint32_t right, uint32_t top, uint32_t bottom);
void set_graphic_pipeline(command_list_t command_list, pipeline_state_t pipeline);

void bind_index_buffer(command_list_t command_list, buffer_t buffer, uint64_t offset, uint32_t size, irr::video::E_INDEX_TYPE type);
void bind_vertex_buffers(command_list_t commandlist, uint32_t first_bind, const std::vector<std::tuple<buffer_t, uint64_t, uint32_t, uint32_t> > &buffer_offset_stride_size);
void submit_executable_command_list(command_queue_t command_queue, command_list_t command_list);
void draw_indexed(command_list_t command_list, uint32_t index_count, uint32_t instance_count, uint32_t base_index, int32_t base_vertex, uint32_t base_instance);
uint32_t get_next_backbuffer_id(device_t dev, swap_chain_t chain);

std::tuple<device_t, swap_chain_t, command_queue_t, size_t, size_t, irr::video::ECOLOR_FORMAT> create_device_swapchain_and_graphic_presentable_queue(HINSTANCE hinstance, HWND window);
std::vector<image_t> get_image_view_from_swap_chain(device_t dev, swap_chain_t chain);
