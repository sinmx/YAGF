#include <D3DAPI/Texture.h>
#include <D3DAPI/Context.h>
#include <D3DAPI/RootSignature.h>

#include <D3DAPI/Misc.h>
#include <D3DAPI/VAO.h>
#include <D3DAPI/S3DVertex.h>
#include <Loaders/B3D.h>
#include <Loaders/DDS.h>
#include <tuple>
#include <D3DAPI/PSO.h>
#include <D3DAPI/Resource.h>
#include <D3DAPI/ConstantBuffer.h>


#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "d3dcompiler.lib")

using namespace Microsoft::WRL; // For ComPtr

ComPtr<ID3D12GraphicsCommandList> cmdlist;
ComPtr<ID3D12DescriptorHeap> ReadResourceHeaps;
ComPtr<ID3D12DescriptorHeap> TexResourceHeaps;
std::vector<ComPtr<ID3D12Resource> > Textures;
ComPtr<ID3D12DescriptorHeap> Sampler;
ComPtr<ID3D12Resource> DepthBuffer;
ComPtr<ID3D12DescriptorHeap> DepthDescriptorHeap;
Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdalloc;

FormattedVertexStorage<irr::video::S3DVertex2TCoords, irr::video::SkinnedVertexData> *vao;

typedef RootSignature<D3D12_ROOT_SIGNATURE_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
  DescriptorTable<ConstantsBufferResource<0>, ConstantsBufferResource<1>>,
  DescriptorTable<ShaderResource<0> >,
  DescriptorTable<SamplerResource<0>> > RS;

struct Matrixes
{
  float Model[16];
  float ViewProj[16];
};

struct JointTransform
{
  float Model[16 * 48];
};

ConstantBuffer<Matrixes> *cbuffer;
ConstantBuffer<JointTransform> *jointbuffer;

class Object : public PipelineStateObject<Object, irr::video::S3DVertex2TCoords>
{
public:
  Object() : PipelineStateObject<Object, irr::video::S3DVertex2TCoords>(L"Debug\\vtx.cso", L"Debug\\pix.cso")
  {}

  static void SetRasterizerAndBlendStates(D3D12_GRAPHICS_PIPELINE_STATE_DESC& psodesc)
  {
    psodesc.pRootSignature = RS::getInstance()->pRootSignature.Get();
    psodesc.RasterizerState = CD3D12_RASTERIZER_DESC(D3D12_DEFAULT);
    psodesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    psodesc.NumRenderTargets = 1;
    psodesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psodesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psodesc.DepthStencilState = CD3D12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    psodesc.BlendState = CD3D12_BLEND_DESC(D3D12_DEFAULT);
    static D3D12_INPUT_ELEMENT_DESC IAdesc[] =
    {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D12_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 1, DXGI_FORMAT_R32_SINT, 1, 0, D3D12_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 2, DXGI_FORMAT_R32_FLOAT, 1, 4, D3D12_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 3, DXGI_FORMAT_R32_SINT, 1, 8, D3D12_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 4, DXGI_FORMAT_R32_FLOAT, 1, 12, D3D12_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 5, DXGI_FORMAT_R32_SINT, 1, 16, D3D12_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 6, DXGI_FORMAT_R32_FLOAT, 1, 20, D3D12_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 7, DXGI_FORMAT_R32_SINT, 1, 24, D3D12_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 8, DXGI_FORMAT_R32_FLOAT, 1, 28, D3D12_INPUT_PER_VERTEX_DATA, 0 },
    };

    psodesc.InputLayout.pInputElementDescs = IAdesc;
    psodesc.InputLayout.NumElements = 10;
  }
};

irr::scene::CB3DMeshFileLoader *loader;
std::unordered_map<std::string, D3D12_GPU_DESCRIPTOR_HANDLE> textureSet;

void Init(HWND hWnd)
{
  Context::getInstance()->InitD3D(hWnd);
  ID3D12Device *dev = Context::getInstance()->dev.Get();

  HRESULT hr = dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdalloc));
  hr = dev->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdalloc.Get(), 0, IID_PPV_ARGS(&cmdlist));

  {
    D3D12_DESCRIPTOR_HEAP_DESC heapdesc = {};
    heapdesc.Type = D3D12_CBV_SRV_UAV_DESCRIPTOR_HEAP;
    heapdesc.NumDescriptors = 2;
    heapdesc.Flags = D3D12_DESCRIPTOR_HEAP_SHADER_VISIBLE;
    hr = dev->CreateDescriptorHeap(&heapdesc, IID_PPV_ARGS(&ReadResourceHeaps));

    cbuffer = new ConstantBuffer<Matrixes>();
    jointbuffer = new ConstantBuffer<JointTransform>();

    dev->CreateConstantBufferView(&cbuffer->getDesc(), ReadResourceHeaps->GetCPUDescriptorHandleForHeapStart());
    dev->CreateConstantBufferView(&jointbuffer->getDesc(), ReadResourceHeaps->GetCPUDescriptorHandleForHeapStart().MakeOffsetted(dev->GetDescriptorHandleIncrementSize(D3D12_CBV_SRV_UAV_DESCRIPTOR_HEAP)));
  }

  {
    hr = dev->CreateCommittedResource(
      &CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
      D3D12_HEAP_MISC_NONE,
      &CD3D12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, 1024, 1024, 1, 0, 1, 0, D3D12_RESOURCE_MISC_ALLOW_DEPTH_STENCIL),
      D3D12_RESOURCE_USAGE_DEPTH,
      &CD3D12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1., 0),
      IID_PPV_ARGS(&DepthBuffer));

    D3D12_DESCRIPTOR_HEAP_DESC heapdesc = {};
    heapdesc.Type = D3D12_DSV_DESCRIPTOR_HEAP;
    heapdesc.NumDescriptors = 1;
    heapdesc.Flags = D3D12_DESCRIPTOR_HEAP_NONE;
    hr = dev->CreateDescriptorHeap(&heapdesc, IID_PPV_ARGS(&DepthDescriptorHeap));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
    dsv.Format = DXGI_FORMAT_D32_FLOAT;
    dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv.Texture2D.MipSlice = 0;
    dev->CreateDepthStencilView(DepthBuffer.Get(), &dsv, DepthDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
  }

  irr::io::CReadFile reader("..\\examples\\xue.b3d");
  loader = new irr::scene::CB3DMeshFileLoader(&reader);
  std::vector<std::pair<irr::scene::SMeshBufferLightMap, irr::video::SMaterial> > buffers = loader->AnimatedMesh.getMeshBuffers();
  std::vector<irr::scene::SMeshBufferLightMap> reorg;

  for (auto buf : buffers)
    reorg.push_back(buf.first);

  // Format Weight

  std::vector<std::vector<irr::video::SkinnedVertexData> > weightsList;
  for (auto weightbuffer : loader->AnimatedMesh.WeightBuffers)
  {
    std::vector<irr::video::SkinnedVertexData> weights;
    for (unsigned j = 0; j < weightbuffer.size(); j += 4)
    {
      irr::video::SkinnedVertexData tmp = {
        weightbuffer[j].Index, weightbuffer[j].Weight,
        weightbuffer[j + 1].Index, weightbuffer[j + 1].Weight,
        weightbuffer[j + 2].Index, weightbuffer[j + 2].Weight,
        weightbuffer[j + 3].Index, weightbuffer[j + 3].Weight,
      };
      weights.push_back(tmp);
    }
    weightsList.push_back(weights);
  }

  vao = new FormattedVertexStorage<irr::video::S3DVertex2TCoords, irr::video::SkinnedVertexData>(Context::getInstance()->cmdqueue.Get(), reorg, weightsList);

  // Upload to gpudata
  {
    // Texture
    std::vector<std::tuple<std::string, ID3D12Resource*, D3D12_SHADER_RESOURCE_VIEW_DESC> > textureNamePairs;
    for (unsigned i = 0; i < loader->Textures.size(); i++)
    {
      Textures.push_back(ComPtr<ID3D12Resource>());
      const std::string &fixed = "..\\examples\\" + loader->Textures[i].TextureName.substr(0, loader->Textures[i].TextureName.find_last_of('.')) + ".DDS";
      std::ifstream DDSFile(fixed, std::ifstream::binary);
      irr::video::CImageLoaderDDS DDSPic(DDSFile);

      Texture TextureInRam(DDSPic.getLoadedImage());

      hr = dev->CreateCommittedResource(
        &CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_MISC_NONE,
        &CD3D12_RESOURCE_DESC::Tex2D(getDXGIFormatFromColorFormat(DDSPic.getLoadedImage().Format), (UINT)TextureInRam.getWidth(), (UINT)TextureInRam.getHeight(), 1, (UINT16)DDSPic.getLoadedImage().MipMapData.size()),
        D3D12_RESOURCE_USAGE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&Textures.back())
        );

      TextureInRam.CreateUploadCommandToResourceInDefaultHeap(cmdlist.Get(), Textures.back().Get());
      textureNamePairs.push_back(std::make_tuple(loader->Textures[i].TextureName, Textures.back().Get(), TextureInRam.getResourceViewDesc()));

      cmdlist->Close();
      Context::getInstance()->cmdqueue->ExecuteCommandLists(1, (ID3D12CommandList**)cmdlist.GetAddressOf());
      HANDLE handle = getCPUSyncHandle(Context::getInstance()->cmdqueue.Get());
      WaitForSingleObject(handle, INFINITE);
      CloseHandle(handle);
      cmdlist->Reset(cmdalloc.Get(), Object::getInstance()->pso.Get());
    }

    // Create Texture Heap
    {
      D3D12_DESCRIPTOR_HEAP_DESC heapdesc = {};
      heapdesc.Type = D3D12_CBV_SRV_UAV_DESCRIPTOR_HEAP;
      heapdesc.NumDescriptors = (UINT)textureNamePairs.size();
      heapdesc.Flags = D3D12_DESCRIPTOR_HEAP_SHADER_VISIBLE;
      hr = dev->CreateDescriptorHeap(&heapdesc, IID_PPV_ARGS(&TexResourceHeaps));
      for (unsigned i = 0; i < textureNamePairs.size(); i++)
      {
        std::tuple<std::string, ID3D12Resource*, D3D12_SHADER_RESOURCE_VIEW_DESC> texturedata = textureNamePairs[i];
        dev->CreateShaderResourceView(std::get<1>(texturedata), &std::get<2>(texturedata), TexResourceHeaps->GetCPUDescriptorHandleForHeapStart().MakeOffsetted(i * dev->GetDescriptorHandleIncrementSize(D3D12_CBV_SRV_UAV_DESCRIPTOR_HEAP)));
        textureSet[std::get<0>(texturedata)] = TexResourceHeaps->GetGPUDescriptorHandleForHeapStart().MakeOffsetted(i * dev->GetDescriptorHandleIncrementSize(D3D12_CBV_SRV_UAV_DESCRIPTOR_HEAP));
      }
    }

    D3D12_DESCRIPTOR_HEAP_DESC sampler_heap = {};
    sampler_heap.Type = D3D12_SAMPLER_DESCRIPTOR_HEAP;
    sampler_heap.NumDescriptors = 1;
    sampler_heap.Flags = D3D12_DESCRIPTOR_HEAP_SHADER_VISIBLE;
    hr = dev->CreateDescriptorHeap(&sampler_heap, IID_PPV_ARGS(&Sampler));

    D3D12_SAMPLER_DESC samplerdesc = {};
    samplerdesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplerdesc.AddressU = D3D12_TEXTURE_ADDRESS_WRAP;
    samplerdesc.AddressV = D3D12_TEXTURE_ADDRESS_WRAP;
    samplerdesc.AddressW = D3D12_TEXTURE_ADDRESS_WRAP;
    samplerdesc.MaxAnisotropy = 1;
    samplerdesc.MinLOD = 0;
    samplerdesc.MaxLOD = 10;
    dev->CreateSampler(&samplerdesc, Sampler->GetCPUDescriptorHandleForHeapStart());

    cmdlist->Close();
    Context::getInstance()->cmdqueue->ExecuteCommandLists(1, (ID3D12CommandList**)cmdlist.GetAddressOf());

    HANDLE handle = getCPUSyncHandle(Context::getInstance()->cmdqueue.Get());
    WaitForSingleObject(handle, INFINITE);
    CloseHandle(handle);
    cmdlist->Reset(cmdalloc.Get(), Object::getInstance()->pso.Get());
  }
}


static float timer = 0.;

void Draw()
{
  D3D12_RECT rect = {};
  rect.left = 0;
  rect.top = 0;
  rect.bottom = 1024;
  rect.right = 1024;

  D3D12_VIEWPORT view = {};
  view.Height = 1024;
  view.Width = 1024;
  view.TopLeftX = 0;
  view.TopLeftY = 0;
  view.MinDepth = 0;
  view.MaxDepth = 1.;

  {
    Matrixes *cbufdata = cbuffer->map();
    irr::core::matrix4 Model, View;
    View.buildProjectionMatrixPerspectiveFovLH(70.f / 180.f * 3.14f, 1.f, 1.f, 100.f);
    Model.setTranslation(irr::core::vector3df(0.f, 0.f, 2.f));
    Model.setRotationDegrees(irr::core::vector3df(0.f, timer / 360.f, 0.f));

    memcpy(cbufdata->Model, Model.pointer(), 16 * sizeof(float));
    memcpy(cbufdata->ViewProj, View.pointer(), 16 * sizeof(float));
    cbuffer->unmap();
  }

  timer += 16.f;

  {
    double intpart;
    float frame = (float)modf(timer / 10000., &intpart);
    frame *= 300.f;
    loader->AnimatedMesh.animateMesh(frame, 1.f);
    loader->AnimatedMesh.skinMesh(1.f);

    memcpy(jointbuffer->map(), loader->AnimatedMesh.JointMatrixes.data(), loader->AnimatedMesh.JointMatrixes.size() * 16 * sizeof(float));
    jointbuffer->unmap();
  }

  cmdlist->RSSetViewports(1, &view);
  cmdlist->RSSetScissorRects(1, &rect);

  ID3D12DescriptorHeap *descriptorlst[] =
  {
    ReadResourceHeaps.Get()
  };
  cmdlist->SetDescriptorHeaps(descriptorlst, 1);

  cmdlist->ResourceBarrier(1, &setResourceTransitionBarrier(Context::getInstance()->getCurrentBackBuffer(), D3D12_RESOURCE_USAGE_PRESENT, D3D12_RESOURCE_USAGE_RENDER_TARGET));

  float clearColor[] = { .25f, .25f, 0.35f, 1.0f };
  cmdlist->ClearRenderTargetView(Context::getInstance()->getCurrentBackBufferDescriptor(), clearColor, 0, 0);
  cmdlist->ClearDepthStencilView(DepthDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_DEPTH, 1., 0, nullptr, 0);

  cmdlist->SetGraphicsRootSignature(RS::getInstance()->pRootSignature.Get());
  float c[] = { 1., 1., 1., 1. };
  cmdlist->SetGraphicsRootDescriptorTable(0, ReadResourceHeaps->GetGPUDescriptorHandleForHeapStart());
  cmdlist->SetGraphicsRootDescriptorTable(2, Sampler->GetGPUDescriptorHandleForHeapStart());
  cmdlist->SetRenderTargets(&Context::getInstance()->getCurrentBackBufferDescriptor(), true, 1, &DepthDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
  cmdlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  cmdlist->SetIndexBuffer(&vao->getIndexBufferView());
  cmdlist->SetVertexBuffers(0, &vao->getVertexBufferView()[0], 1);
  cmdlist->SetVertexBuffers(1, &vao->getVertexBufferView()[1], 1);

  std::vector<std::pair<irr::scene::SMeshBufferLightMap, irr::video::SMaterial> > buffers = loader->AnimatedMesh.getMeshBuffers();
  for (unsigned i = 0; i < buffers.size(); i++)
  {
    cmdlist->SetGraphicsRootDescriptorTable(1, textureSet[buffers[i].second.TextureNames[0]]);
    cmdlist->DrawIndexedInstanced((UINT)std::get<0>(vao->meshOffset[i]), 1, (UINT)std::get<2>(vao->meshOffset[i]), (UINT)std::get<1>(vao->meshOffset[i]), 0);
  }

  cmdlist->ResourceBarrier(1, &setResourceTransitionBarrier(Context::getInstance()->getCurrentBackBuffer(), D3D12_RESOURCE_USAGE_RENDER_TARGET, D3D12_RESOURCE_USAGE_PRESENT));
  HRESULT hr = cmdlist->Close();
  Context::getInstance()->cmdqueue->ExecuteCommandLists(1, (ID3D12CommandList**)cmdlist.GetAddressOf());
  HANDLE handle = getCPUSyncHandle(Context::getInstance()->cmdqueue.Get());
  WaitForSingleObject(handle, INFINITE);
  CloseHandle(handle);
  cmdalloc->Reset();
  cmdlist->Reset(cmdalloc.Get(), Object::getInstance()->pso.Get());
  Context::getInstance()->Swap();
}

void Clean()
{
  Context::getInstance()->kill();
  Object::getInstance()->kill();
  RS::getInstance()->kill();
  delete vao;
  delete cbuffer;
  delete jointbuffer;
}

int WINAPI WinMain(HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  LPSTR lpCmdLine,
  int nCmdShow)
{

  Init(WindowUtil::Create(hInstance, hPrevInstance, lpCmdLine, nCmdShow));
  // this struct holds Windows event messages
  MSG msg = {};

  // Loop from https://msdn.microsoft.com/en-us/library/windows/apps/dn166880.aspx
  while (WM_QUIT != msg.message)
  {
    bool bGotMsg = (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE) != 0);

    if (bGotMsg)
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    Draw();
  }

  Clean();
  // return this part of the WM_QUIT message to Windows
  return (int)msg.wParam;
}

