// Copyright (C) 2015 Vincent Lejeune
// For conditions of distribution and use, see copyright notice in License.txt

#ifndef __SCENE_H__
#define __SCENE_H__
#include <list>
#include <ISceneNode.h>
#include <MeshSceneNode.h>

#include <Material.h>
#include <RenderTargets.h>

#ifdef GLBUILD
#include <GLAPI/Samplers.h>
#endif

#ifdef DXBUILD
#include <d3dapi.h>
#include <D3DAPI/Sampler.h>
#endif

struct ViewBuffer
{
  float ViewProj[16];
};

class Scene
{
private:
  std::list<irr::scene::IMeshSceneNode*> Meshes;
  std::shared_ptr<WrapperCommandList> cmdList;
#ifdef DXBUILD
  // Should be tied to view rather than scene
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cbufferDescriptorHeap;
  ConstantBuffer<ViewBuffer> cbuffer;

  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Sampler;
#endif
#ifdef GLBUILD
  GLuint cbuf;
  GLuint TrilinearSampler;
#endif
public:
  Scene()
  {
    cmdList = GlobalGFXAPI->createCommandList();
#ifdef GLBUILD
    glGenBuffers(1, &cbuf);
    TrilinearSampler = SamplerHelper::createTrilinearSampler();
#endif
#ifdef DXBUILD
    cbufferDescriptorHeap = createDescriptorHeap(Context::getInstance()->dev.Get(), 1, D3D12_CBV_SRV_UAV_DESCRIPTOR_HEAP, true);
    Context::getInstance()->dev->CreateConstantBufferView(&cbuffer.getDesc(), cbufferDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    Sampler = createDescriptorHeap(Context::getInstance()->dev.Get(), 1, D3D12_SAMPLER_DESCRIPTOR_HEAP, true);

    Context::getInstance()->dev->CreateSampler(&Samplers::getTrilinearSamplerDesc(), Sampler->GetCPUDescriptorHandleForHeapStart());
#endif
  }

  ~Scene()
  {
    for (irr::scene::IMeshSceneNode* node : Meshes)
      delete node;
#ifdef GLBUILD
    glDeleteSamplers(1, &TrilinearSampler);
    glDeleteBuffers(1, &cbuf);
#endif
  }

  void update()
  {
    for (irr::scene::IMeshSceneNode* mesh : Meshes)
    {
      mesh->updateAbsolutePosition();
      mesh->render();
    }
  }

  irr::scene::IMeshSceneNode *addMeshSceneNode(const irr::scene::ISkinnedMesh *mesh, irr::scene::ISceneNode* parent,
    const irr::core::vector3df& position =irr:: core::vector3df(0, 0, 0),
    const irr::core::vector3df& rotation = irr::core::vector3df(0, 0, 0),
    const irr::core::vector3df& scale = irr::core::vector3df(1.f, 1.f, 1.f))
  {
    irr::scene::IMeshSceneNode* node = new irr::scene::IMeshSceneNode(parent, position, rotation, scale);
    node->setMesh(mesh);
    Meshes.push_back(node);
    return node;
  }

  void renderGBuffer(const class Camera*, RenderTargets &rtts)
  {
    ViewBuffer cbufdata;
    irr::core::matrix4 View;
    View.buildProjectionMatrixPerspectiveFovLH(70.f / 180.f * 3.14f, 1.f, 1.f, 100.f);
    memcpy(&cbufdata, View.pointer(), 16 * sizeof(float));
#ifdef GLBUILD
    glEnable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    rtts.getFrameBuffer(RenderTargets::FBO_GBUFFER).Bind();

    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClearDepth(1.);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindBuffer(GL_UNIFORM_BUFFER, cbuf);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(ViewBuffer), &cbufdata, GL_STATIC_DRAW);

    glUseProgram(ObjectShader::getInstance()->Program);
    glBindVertexArray(VertexArrayObject<FormattedVertexStorage<irr::video::S3DVertex2TCoords> >::getInstance()->getVAO());
    for (const irr::scene::IMeshSceneNode* node : Meshes)
    {
      for (const irr::video::DrawData &drawdata : node->getDrawDatas())
      {
        ObjectShader::getInstance()->SetTextureUnits(node->getConstantBuffer(), cbuf, drawdata.textures[0]->Id, TrilinearSampler);
        glDrawElementsBaseVertex(GL_TRIANGLES, (GLsizei)drawdata.IndexCount, GL_UNSIGNED_SHORT, (void *)drawdata.vaoOffset, (GLsizei)drawdata.vaoBaseVertex);
      }
    }

    rtts.getFrameBuffer(RenderTargets::FBO_GBUFFER).BlitToDefault(0, 0, 1024, 1024);
#endif

#ifdef DXBUILD
    memcpy(cbuffer.map(), &cbufdata, sizeof(ViewBuffer));
    cbuffer.unmap();

    ID3D12DescriptorHeap *descriptorlst[] =
    {
      cbufferDescriptorHeap.Get()
    };
    ID3D12GraphicsCommandList *cmdlist = unwrap(cmdList.get()).Get();
    cmdlist->SetDescriptorHeaps(descriptorlst, 1);

    float clearColor[] = { 0.f, 0.f, 0.f, 0.f };

    GlobalGFXAPI->clearRTTSet(cmdList.get(), rtts.getRTTSet(RenderTargets::FBO_GBUFFER), clearColor);
    cmdlist->ClearDepthStencilView(rtts.getDepthDescriptorHeap()->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_DEPTH, 1., 0, nullptr, 0);

    cmdlist->SetPipelineState(Object::getInstance()->pso.Get());
    cmdlist->SetGraphicsRootSignature((*RS::getInstance())());
    cmdlist->SetGraphicsRootDescriptorTable(0, cbufferDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    GlobalGFXAPI->setRTTSet(cmdList.get(), rtts.getRTTSet(RenderTargets::FBO_GBUFFER));
    cmdlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (irr::scene::IMeshSceneNode* node : Meshes)
    {
      cmdlist->SetIndexBuffer(&node->getVAO()->getIndexBufferView());
      cmdlist->SetVertexBuffers(0, node->getVAO()->getVertexBufferView().data(), 1);

      for (irr::video::DrawData drawdata : node->getDrawDatas())
      {
        cmdlist->SetGraphicsRootDescriptorTable(1, drawdata.descriptors->GetGPUDescriptorHandleForHeapStart());
        cmdlist->SetGraphicsRootDescriptorTable(2, drawdata.descriptors->GetGPUDescriptorHandleForHeapStart().MakeOffsetted(Context::getInstance()->dev->GetDescriptorHandleIncrementSize(D3D12_CBV_SRV_UAV_DESCRIPTOR_HEAP)));
        cmdlist->SetGraphicsRootDescriptorTable(3, Sampler->GetGPUDescriptorHandleForHeapStart());
        GlobalGFXAPI->drawIndexedInstanced(cmdList.get(), drawdata.IndexCount, 1, drawdata.vaoOffset, drawdata.vaoBaseVertex, 0);
      }
    }

    ID3D12Resource *gbuffer_base_color = unwrap(rtts.getRTT(RenderTargets::GBUFFER_BASE_COLOR)).Get();
    GlobalGFXAPI->writeResourcesTransitionBarrier(cmdList.get(), { std::make_tuple(rtts.getRTT(RenderTargets::GBUFFER_BASE_COLOR), GFXAPI::RENDER_TARGET, GFXAPI::COPY_SRC) });
    cmdlist->ResourceBarrier(1, &setResourceTransitionBarrier(Context::getInstance()->getCurrentBackBuffer(), D3D12_RESOURCE_USAGE_PRESENT, D3D12_RESOURCE_USAGE_COPY_DEST));
    D3D12_TEXTURE_COPY_LOCATION src = {}, dst = {};
    src.Type = D3D12_SUBRESOURCE_VIEW_SELECT_SUBRESOURCE;
    src.pResource = gbuffer_base_color;
    dst.Type = D3D12_SUBRESOURCE_VIEW_SELECT_SUBRESOURCE;
    dst.pResource = Context::getInstance()->getCurrentBackBuffer();
    D3D12_BOX box = {0, 0, 0, 1008, 985, 1};
    cmdlist->CopyTextureRegion(&dst, 0, 0, 0, &src, &box, D3D12_COPY_NONE);
    GlobalGFXAPI->writeResourcesTransitionBarrier(cmdList.get(), { std::make_tuple(rtts.getRTT(RenderTargets::GBUFFER_BASE_COLOR), GFXAPI::COPY_SRC, GFXAPI::RENDER_TARGET) });
    cmdlist->ResourceBarrier(1, &setResourceTransitionBarrier(Context::getInstance()->getCurrentBackBuffer(), D3D12_RESOURCE_USAGE_COPY_DEST, D3D12_RESOURCE_USAGE_PRESENT));
    GlobalGFXAPI->closeCommandList(cmdList.get());
    Context::getInstance()->cmdqueue->ExecuteCommandLists(1, (ID3D12CommandList**)&cmdlist);
    HANDLE handle = getCPUSyncHandle(Context::getInstance()->cmdqueue.Get());
    WaitForSingleObject(handle, INFINITE);
    CloseHandle(handle);
    dynamic_cast<WrapperD3DCommandList*>(cmdList.get())->CommandAllocator->Reset();
    cmdlist->Reset(dynamic_cast<WrapperD3DCommandList*>(cmdList.get())->CommandAllocator.Get(), nullptr);
    Context::getInstance()->Swap();
#endif

  }
};

#endif