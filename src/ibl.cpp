// Copyright (C) 2015 Vincent Lejeune
// For conditions of distribution and use, see copyright notice in License.txt

#include <Scene/IBL.h>
#include <Maths/matrix4.h>
#include <cmath>
#include <set>
#include <d3dcompiler.h>

namespace
{
	constexpr auto object_descriptor_set_type = descriptor_set({
		range_of_descriptors(RESOURCE_VIEW::CONSTANTS_BUFFER, 0, 1),
		range_of_descriptors(RESOURCE_VIEW::SHADER_RESOURCE, 1, 1),
		range_of_descriptors(RESOURCE_VIEW::UAV, 2, 1) },

		shader_stage::all);

	constexpr auto sampler_descriptor_set_type = descriptor_set({
		range_of_descriptors(RESOURCE_VIEW::SAMPLER, 3, 1) },
		shader_stage::fragment_shader);

	std::unique_ptr<compute_pipeline_state_t> get_compute_sh_pipeline_state(device_t* dev, pipeline_layout_t pipeline_layout)
	{
#ifdef D3D12
		ID3D12PipelineState* result;
		Microsoft::WRL::ComPtr<ID3DBlob> blob;
		CHECK_HRESULT(D3DReadFileToBlob(L"computesh.cso", blob.GetAddressOf()));

		D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_desc{};
		pipeline_desc.CS.BytecodeLength = blob->GetBufferSize();
		pipeline_desc.CS.pShaderBytecode = blob->GetBufferPointer();
		pipeline_desc.pRootSignature = pipeline_layout.Get();

		CHECK_HRESULT(dev->object->CreateComputePipelineState(&pipeline_desc, IID_PPV_ARGS(&result)));
		return std::make_unique<compute_pipeline_state_t>(result);
#else
		vulkan_wrapper::shader_module module(dev->object, "..\\..\\..\\computesh.spv");
		VkPipelineShaderStageCreateInfo shader_stages{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module.object, "main", nullptr };
		return std::make_unique<compute_pipeline_state_t>(dev->object, shader_stages, pipeline_layout->object, VkPipeline(VK_NULL_HANDLE), -1);
#endif // D3D12
	}

}

struct SHCoefficients
{
	float Red[9];
	float Green[9];
	float Blue[9];
};


std::unique_ptr<buffer_t> computeSphericalHarmonics(device_t* dev, command_queue_t* cmd_queue, image_t *probe, size_t edge_size)
{
	std::unique_ptr<command_list_storage_t> command_storage = create_command_storage(dev);
	std::unique_ptr<command_list_t> command_list = create_command_list(dev, command_storage.get());

	start_command_list_recording(dev, command_list.get(), command_storage.get());
	std::unique_ptr<buffer_t> cbuf = create_buffer(dev, sizeof(int), irr::video::E_MEMORY_POOL::EMP_CPU_WRITEABLE, none);
	void* tmp = map_buffer(dev, cbuf.get());
	float cube_size = (float)edge_size / 10.;
	memcpy(tmp, &cube_size, sizeof(int));
	unmap_buffer(dev, cbuf.get());
#ifdef D3D12
	auto compute_sh_sig = get_pipeline_layout_from_desc(dev, { object_descriptor_set_type, sampler_descriptor_set_type });
#else
	std::shared_ptr<vulkan_wrapper::pipeline_descriptor_set> object_set = get_object_descriptor_set(dev, object_descriptor_set_type);
	std::shared_ptr<vulkan_wrapper::pipeline_descriptor_set> sampler_set = get_object_descriptor_set(dev, sampler_descriptor_set_type);
	auto compute_sh_sig = std::make_shared<vulkan_wrapper::pipeline_layout>(dev->object, 0, std::vector<VkDescriptorSetLayout>{ object_set->object, sampler_set->object}, std::vector<VkPushConstantRange>());
#endif
	std::unique_ptr<compute_pipeline_state_t> compute_sh_pso = get_compute_sh_pipeline_state(dev, compute_sh_sig);
	std::unique_ptr<descriptor_storage_t> srv_cbv_uav_heap = create_descriptor_storage(dev, 1, { { RESOURCE_VIEW::CONSTANTS_BUFFER, 1 }, { RESOURCE_VIEW::SHADER_RESOURCE, 1}, { RESOURCE_VIEW::UAV, 1} });
	std::unique_ptr<descriptor_storage_t> sampler_heap = create_descriptor_storage(dev, 1, { { RESOURCE_VIEW::SAMPLER, 1 } });

	std::unique_ptr<buffer_t> sh_buffer = create_buffer(dev, sizeof(SH), irr::video::E_MEMORY_POOL::EMP_GPU_LOCAL, usage_uav);
	std::unique_ptr<buffer_t> sh_buffer_readback = create_buffer(dev, sizeof(SH), irr::video::E_MEMORY_POOL::EMP_CPU_READABLE, usage_transfer_dst);

#ifdef D3D12
	create_constant_buffer_view(dev, srv_cbv_uav_heap.get(), 0, cbuf.get(), sizeof(int));
	create_image_view(dev, srv_cbv_uav_heap.get(), 1, probe, 9, irr::video::ECF_BC1_UNORM_SRGB, D3D12_SRV_DIMENSION_TEXTURECUBE);
	create_buffer_uav_view(dev, srv_cbv_uav_heap.get(), 2, sh_buffer.get(), sizeof(SH));

	command_list->object->SetPipelineState(compute_sh_pso->object);
	command_list->object->SetComputeRootSignature(compute_sh_sig.Get());
	std::array<ID3D12DescriptorHeap*, 2> heaps = { srv_cbv_uav_heap->object, sampler_heap->object };
	command_list->object->SetDescriptorHeaps(heaps.size(), heaps.data());
	command_list->object->SetComputeRootDescriptorTable(0, srv_cbv_uav_heap->object->GetGPUDescriptorHandleForHeapStart());
	command_list->object->SetComputeRootDescriptorTable(1, sampler_heap->object->GetGPUDescriptorHandleForHeapStart());

	command_list->object->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(sh_buffer->object, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

//	command_list->object->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(sh_buffer->object, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));
//	command_list->object->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(sh_buffer->object));

#else
	VkDescriptorSet sampler_descriptors = util::allocate_descriptor_sets(dev->object, sampler_heap->object, { sampler_set->object });
	VkDescriptorSet input_descriptors = util::allocate_descriptor_sets(dev->object, srv_cbv_uav_heap->object, { object_set->object });
	std::unique_ptr<vulkan_wrapper::sampler> sampler = std::make_unique<vulkan_wrapper::sampler>(dev->object, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR,
		VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, 0.f, true, 16.f);
	std::unique_ptr<vulkan_wrapper::image_view> skybox_view = std::make_unique<vulkan_wrapper::image_view>(dev->object, probe->object, VK_IMAGE_VIEW_TYPE_CUBE, VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
		structures::component_mapping(), structures::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT, 0, 9, 0, 6));

	util::update_descriptor_sets(dev->object,
	{
		structures::write_descriptor_set(sampler_descriptors, VK_DESCRIPTOR_TYPE_SAMPLER,
			{ VkDescriptorImageInfo{ sampler->object, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } }, 3),
		structures::write_descriptor_set(input_descriptors, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			{ VkDescriptorBufferInfo{ cbuf->object, 0, sizeof(int) } }, 0),
		structures::write_descriptor_set(input_descriptors, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			{ VkDescriptorImageInfo{ VK_NULL_HANDLE, skybox_view->object, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } }, 1),
		structures::write_descriptor_set(input_descriptors, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			{ VkDescriptorBufferInfo{ sh_buffer->object, 0, sizeof(SH) } }, 2)
	});

	vkCmdBindPipeline(command_list->object, VK_PIPELINE_BIND_POINT_COMPUTE, compute_sh_pso->object);
	vkCmdBindDescriptorSets(command_list->object, VK_PIPELINE_BIND_POINT_COMPUTE, compute_sh_sig->object, 0, 1, &input_descriptors, 0, nullptr);
	vkCmdBindDescriptorSets(command_list->object, VK_PIPELINE_BIND_POINT_COMPUTE, compute_sh_sig->object, 1, 1, &sampler_descriptors, 0, nullptr);
#endif
	dispatch(command_list.get(), 1, 1, 1);
	copy_buffer(command_list.get(), sh_buffer.get(), 0, sh_buffer_readback.get(), 0, sizeof(SH));

	make_command_list_executable(command_list.get());
	submit_executable_command_list(cmd_queue, command_list.get());
	// for debug
	wait_for_command_queue_idle(dev, cmd_queue);
	SHCoefficients Result;
	float* Shval = (float*)map_buffer(dev, sh_buffer_readback.get());
	memcpy(Result.Blue, Shval, 9 * sizeof(float));
	memcpy(Result.Green, &Shval[9], 9 * sizeof(float));
	memcpy(Result.Red, &Shval[18], 9 * sizeof(float));

	return std::move(sh_buffer);
}

namespace
{
// From http://http.developer.nvidia.com/GPUGems3/gpugems3_ch20.html
/**
 * Returns the n-th set from the 2 dimension Hammersley sequence.
 * The 2 dimension Hammersley seq is a pseudo random uniform distribution
*  between 0 and 1 for 2 components.
 * We use the natural indexation on the set to avoid storing the whole set.
 * \param i index of the pair
 * \param size of the set. */
std::pair<float, float> HammersleySequence(int n, int samples)
{
	float InvertedBinaryRepresentation = 0.;
	for (size_t i = 0; i < 32; i++)
	{
		InvertedBinaryRepresentation += ((n >> i) & 0x1) * powf(.5, (float)(i + 1.));
	}
	return std::make_pair(float(n) / float(samples), InvertedBinaryRepresentation);
}

/** Returns a pseudo random (theta, phi) generated from a probability density function modeled after Phong function.
\param a pseudo random float pair from a uniform density function between 0 and 1.
\param exponent from the Phong formula. */
std::pair<float, float> ImportanceSamplingPhong(std::pair<float, float> Seeds, float exponent)
{
	return std::make_pair(acosf(powf(Seeds.first, 1.f / (exponent + 1.f))), 2.f * 3.14f * Seeds.second);
}

/** Returns a pseudo random (theta, phi) generated from a probability density function modeled after GGX distribtion function.
From "Real Shading in Unreal Engine 4" paper
\param a pseudo random float pair from a uniform density function between 0 and 1.
\param exponent from the Phong formula. */
std::pair<float, float> ImportanceSamplingGGX(std::pair<float, float> Seeds, float roughness)
{
	float a = roughness * roughness;
	float CosTheta = sqrtf((1.f - Seeds.second) / (1.f + (a * a - 1.f) * Seeds.second));
	return std::make_pair(acosf(CosTheta), 2.f * 3.14f * Seeds.first);
}

/** Returns a pseudo random (theta, phi) generated from a probability density function modeled after cos distribtion function.
\param a pseudo random float pair from a uniform density function between 0 and 1. */
std::pair<float, float> ImportanceSamplingCos(std::pair<float, float> Seeds)
{
	return std::make_pair(acosf(Seeds.first), 2.f * 3.14f * Seeds.second);
}

irr::core::matrix4 getPermutationMatrix(size_t indexX, float valX, size_t indexY, float valY, size_t indexZ, float valZ)
{
	irr::core::matrix4 resultMat;
	float *M = resultMat.pointer();
	memset(M, 0, 16 * sizeof(float));
	assert(indexX < 4);
	assert(indexY < 4);
	assert(indexZ < 4);
	M[indexX] = valX;
	M[4 + indexY] = valY;
	M[8 + indexZ] = valZ;
	return resultMat;
}

struct PermutationMatrix
{
	float Matrix[16];
};

constexpr auto image_set_type = descriptor_set({
	range_of_descriptors(RESOURCE_VIEW::SHADER_RESOURCE, 0, 1) },
	shader_stage::all);

constexpr auto face_set_type = descriptor_set({
	range_of_descriptors(RESOURCE_VIEW::CONSTANTS_BUFFER, 1, 1) },
	shader_stage::all);

constexpr auto mipmap_set_type = descriptor_set({
	range_of_descriptors(RESOURCE_VIEW::SHADER_RESOURCE, 2, 1) },
	shader_stage::all);

constexpr auto uav_set_type = descriptor_set({
	range_of_descriptors(RESOURCE_VIEW::UAV, 3, 1) },
	shader_stage::all);

constexpr auto sampler_set_type = descriptor_set({
	range_of_descriptors(RESOURCE_VIEW::SAMPLER, 4, 1) },
	shader_stage::all);

std::unique_ptr<compute_pipeline_state_t> ImportanceSamplingForSpecularCubemap(device_t* dev, pipeline_layout_t pipeline_layout)
{
#ifdef D3D12
	ID3D12PipelineState* result;
	Microsoft::WRL::ComPtr<ID3DBlob> blob;
	CHECK_HRESULT(D3DReadFileToBlob(L"importance_sampling_specular.cso", blob.GetAddressOf()));

	D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_desc{};
	pipeline_desc.CS.BytecodeLength = blob->GetBufferSize();
	pipeline_desc.CS.pShaderBytecode = blob->GetBufferPointer();
	pipeline_desc.pRootSignature = pipeline_layout.Get();

	CHECK_HRESULT(dev->object->CreateComputePipelineState(&pipeline_desc, IID_PPV_ARGS(&result)));
	return std::make_unique<compute_pipeline_state_t>(result);
#else
	vulkan_wrapper::shader_module module(dev->object, "..\\..\\..\\computesh.spv");
	VkPipelineShaderStageCreateInfo shader_stages{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module.object, "main", nullptr };
	return std::make_unique<compute_pipeline_state_t>(dev->object, shader_stages, pipeline_layout->object, VkPipeline(VK_NULL_HANDLE), -1);
#endif // D3D12
}
}

std::unique_ptr<image_t> generateSpecularCubemap(device_t* dev, command_queue_t* cmd_queue, image_t *probe)
{
	size_t cubemap_size = 256;

	std::unique_ptr<command_list_storage_t> command_storage = create_command_storage(dev);
	std::unique_ptr<command_list_t> command_list = create_command_list(dev, command_storage.get());
	pipeline_layout_t importance_sampling_sig = get_pipeline_layout_from_desc(dev, { image_set_type, face_set_type, mipmap_set_type, uav_set_type, sampler_set_type });
	std::unique_ptr<compute_pipeline_state_t> importance_sampling = ImportanceSamplingForSpecularCubemap(dev, importance_sampling_sig);
	std::unique_ptr<descriptor_storage_t> input_heap = create_descriptor_storage(dev, 10, { { RESOURCE_VIEW::SHADER_RESOURCE, 9 },{ RESOURCE_VIEW::CONSTANTS_BUFFER, 6 },{ RESOURCE_VIEW::UAV, 48 } });
	std::unique_ptr<descriptor_storage_t> sampler_heap = create_descriptor_storage(dev, 1, { { RESOURCE_VIEW::SAMPLER, 1 } });
	create_sampler(dev, sampler_heap.get(), 0, SAMPLER_TYPE::TRILINEAR);

	allocated_descriptor_set image_descriptors = allocate_descriptor_set_from_cbv_srv_uav_heap(dev, input_heap.get(), 0);

	irr::core::matrix4 M[6] = {
		getPermutationMatrix(2, -1., 1, -1., 0, 1.),
		getPermutationMatrix(2, 1., 1, -1., 0, -1.),
		getPermutationMatrix(0, 1., 2, 1., 1, 1.),
		getPermutationMatrix(0, 1., 2, -1., 1, -1.),
		getPermutationMatrix(0, 1., 1, -1., 2, 1.),
		getPermutationMatrix(0, -1., 1, -1., 2, -1.),
	};

	std::array<std::unique_ptr<buffer_t>, 6> permutation_matrix{};
	std::array<allocated_descriptor_set, 6> permutation_matrix_descriptors;
	for (unsigned i = 0; i < 6; i++)
	{
		permutation_matrix_descriptors[i] = allocate_descriptor_set_from_cbv_srv_uav_heap(dev, input_heap.get(), i + 1);
		permutation_matrix[i] = create_buffer(dev, sizeof(PermutationMatrix), irr::video::E_MEMORY_POOL::EMP_CPU_WRITEABLE, none);
		memcpy(map_buffer(dev, permutation_matrix[i].get()), M[i].pointer(), 16 * sizeof(float));
		unmap_buffer(dev, permutation_matrix[i].get());
	}

	std::array<std::unique_ptr<buffer_t>, 8> sample_location_buffer{};
	std::array<allocated_descriptor_set, 8> sample_buffer_descriptors;
	for (unsigned i = 0; i < 8; i++)
	{
		sample_buffer_descriptors[i] = allocate_descriptor_set_from_cbv_srv_uav_heap(dev, input_heap.get(), i + 7);
		sample_location_buffer[i] = create_buffer(dev, 2048 * sizeof(float), irr::video::E_MEMORY_POOL::EMP_CPU_WRITEABLE, none);

		float roughness = .05f + .95f * i / 8.f;
		float viewportSize = float(1 << (8 - i));
		float *tmp = reinterpret_cast<float*>(map_buffer(dev, sample_location_buffer[i].get()));
		for (unsigned j = 0; j < 1024; j++)
		{
			std::pair<float, float> sample = ImportanceSamplingGGX(HammersleySequence(j, 1024), roughness);
			tmp[2 * j] = sample.first;
			tmp[2 * j + 1] = sample.second;
		}
		unmap_buffer(dev, sample_location_buffer[i].get());

		D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
		srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srv.Buffer.FirstElement = 0;
		srv.Buffer.NumElements = 1024;
		srv.Buffer.StructureByteStride = 2 * sizeof(float);
		srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		dev->object->CreateShaderResourceView(sample_location_buffer[i]->object, &srv, sample_buffer_descriptors[i]);
	}

	std::unique_ptr<image_t> result = create_image(dev, irr::video::ECF_R16G16B16A16F, 256, 256, 8, 6, usage_cube | usage_sampled | usage_render_target | usage_uav, nullptr);
	std::array<allocated_descriptor_set, 48> level_face_descriptor;
	for (unsigned level = 0; level < 8; level++)
	{
		for (unsigned face = 0; face < 6; face++)
		{
			level_face_descriptor[face + level * 6] = allocate_descriptor_set_from_cbv_srv_uav_heap(dev, input_heap.get(), face + level * 6 + 15);
			D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
			desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			desc.Texture2DArray.MipSlice = level;
			desc.Texture2DArray.ArraySize = 1;
			desc.Texture2DArray.FirstArraySlice = face;
			desc.Texture2DArray.PlaneSlice = 0;
			desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

			dev->object->CreateUnorderedAccessView(result->object, nullptr, &desc,
				CD3DX12_CPU_DESCRIPTOR_HANDLE(level_face_descriptor[face + 6 * level]));
		}
	}

	start_command_list_recording(dev, command_list.get(), command_storage.get());
	command_list->object->SetComputeRootSignature(importance_sampling_sig.Get());
	command_list->object->SetPipelineState(importance_sampling->object);
	std::array<ID3D12DescriptorHeap*, 2> heaps{ input_heap->object, sampler_heap->object };
	command_list->object->SetDescriptorHeaps(2, heaps.data());

	command_list->object->SetComputeRootDescriptorTable(0, image_descriptors);
	command_list->object->SetComputeRootDescriptorTable(4, sampler_heap->object->GetGPUDescriptorHandleForHeapStart());

	for (unsigned level = 0; level < 8; level++)
	{
		command_list->object->SetComputeRootDescriptorTable(1, sample_buffer_descriptors[level]);
		for (unsigned face = 0; face < 6; face++)
		{
			command_list->object->SetComputeRootDescriptorTable(2,
				CD3DX12_GPU_DESCRIPTOR_HANDLE(level_face_descriptor[face + 6 * level]));

			dispatch(command_list.get(), 256, 256, 1);
		}
	}
	make_command_list_executable(command_list.get());
	submit_executable_command_list(cmd_queue, command_list.get());
	wait_for_command_queue_idle(dev, cmd_queue);

	return result;
}

#if 0
static float G1_Schlick(const irr::core::vector3df &V, const irr::core::vector3df &normal, float k)
{
	float NdotV = V.dotProduct(normal);
	NdotV = NdotV > 0.f ? NdotV : 0.f;
	NdotV = NdotV < 1.f ? NdotV : 1.f;
	return 1.f / (NdotV * (1.f - k) + k);
}

float G_Smith(const irr::core::vector3df &lightdir, const irr::core::vector3df &viewdir, const irr::core::vector3df &normal, float roughness)
{
	float k = (roughness + 1.f) * (roughness + 1.f) / 8.f;
	return G1_Schlick(lightdir, normal, k) * G1_Schlick(viewdir, normal, k);
}

static
std::pair<float, float> getSpecularDFG(float roughness, float NdotV)
{
	// We assume a local referential where N points in Y direction
	irr::core::vector3df V(sqrtf(1.f - NdotV * NdotV), NdotV, 0.f);

	float DFG1 = 0., DFG2 = 0.;
	for (unsigned sample = 0; sample < 1024; sample++)
	{
		std::pair<float, float> ThetaPhi = ImportanceSamplingGGX(HammersleySequence(sample, 1024), roughness);
		float Theta = ThetaPhi.first, Phi = ThetaPhi.second;
		irr::core::vector3df H(sinf(Theta) * cosf(Phi), cosf(Theta), sinf(Theta) * sinf(Phi));
		irr::core::vector3df L = 2 * H.dotProduct(V) * H - V;
		float NdotL = L.Y;

		float NdotH = H.Y > 0. ? H.Y : 0.;
		if (NdotL > 0.)
		{
			float VdotH = V.dotProduct(H);
			VdotH = VdotH > 0.f ? VdotH : 0.f;
			VdotH = VdotH < 1.f ? VdotH : 1.f;
			float Fc = powf(1.f - VdotH, 5.f);
			float G = G_Smith(L, V, irr::core::vector3df(0.f, 1.f, 0.f), roughness);
			DFG1 += (1.f - Fc) * G * VdotH;
			DFG2 += Fc * G * VdotH;
		}
	}
	return std::make_pair(DFG1 / 1024, DFG2 / 1024);
}

static
float getDiffuseDFG(float roughness, float NdotV)
{
	// We assume a local referential where N points in Y direction
	irr::core::vector3df V(sqrtf(1.f - NdotV * NdotV), NdotV, 0.f);
	float DFG = 0.f;
	for (unsigned sample = 0; sample < 1024; sample++)
	{
		std::pair<float, float> ThetaPhi = ImportanceSamplingCos(HammersleySequence(sample, 1024));
		float Theta = ThetaPhi.first, Phi = ThetaPhi.second;
		irr::core::vector3df L(sinf(Theta) * cosf(Phi), cosf(Theta), sinf(Theta) * sinf(Phi));
		float NdotL = L.Y;
		if (NdotL > 0.f)
		{
			irr::core::vector3df H = (L + V).normalize();
			float LdotH = L.dotProduct(H);
			float f90 = .5f + 2.f * LdotH * LdotH * roughness * roughness;
			DFG += (1.f + (f90 - 1.f) * (1.f - powf(NdotL, 5.f))) * (1.f + (f90 - 1.f) * (1.f - powf(NdotV, 5.f)));
		}
	}
	return DFG / 1024;
}

/** Generate the Look Up Table for the DFG texture.
	DFG Texture is used to compute diffuse and specular response from environmental lighting. */
IImage getDFGLUT(size_t DFG_LUT_size)
{
	IImage DFG_LUT_texture;
	DFG_LUT_texture.Format = irr::video::ECF_R32G32B32A32F;
	DFG_LUT_texture.Type = TextureType::TEXTURE2D;
	float *texture_content = new float[4 * DFG_LUT_size * DFG_LUT_size];

	PackedMipMapLevel LUT = {
	  DFG_LUT_size,
	  DFG_LUT_size,
	  texture_content,
	  4 * DFG_LUT_size * DFG_LUT_size * sizeof(float)
	};
	DFG_LUT_texture.Layers.push_back({ LUT });

#pragma omp parallel for
	for (int i = 0; i < int(DFG_LUT_size); i++)
	{
		float roughness = .05f + .95f * float(i) / float(DFG_LUT_size - 1);
		for (unsigned j = 0; j < DFG_LUT_size; j++)
		{
			float NdotV = float(1 + j) / float(DFG_LUT_size + 1);
			std::pair<float, float> DFG = getSpecularDFG(roughness, NdotV);
			texture_content[4 * (i * DFG_LUT_size + j)] = DFG.first;
			texture_content[4 * (i * DFG_LUT_size + j) + 1] = DFG.second;
			texture_content[4 * (i * DFG_LUT_size + j) + 2] = getDiffuseDFG(roughness, NdotV);
			texture_content[4 * (i * DFG_LUT_size + j) + 3] = 0.;
		}
	}

	return DFG_LUT_texture;
}
#endif

