// Copyright (C) 2015 Vincent Lejeune
// For conditions of distribution and use, see copyright notice in License.txt
#ifndef __GFX_API_H__
#define __GFX_API_H__

#include <Core/SColor.h>
#include <vector>
#include <tuple>

#ifdef GLBUILD
#include <GL/glew.h>
#include <GLAPI/GLRTTSet.h>
#endif

#ifdef DXBUILD
#include <D3DAPI/D3DRTTSet.h>
#include <D3DAPI/VAO.h>
#include <Core/BasicVertexLayout.h>
#endif

union WrapperRTTSet
{
#ifdef DXBUILD
  D3DRTTSet D3DValue;
#endif
#ifdef GLBUILD
  GLRTTSet GLValue;
#endif
};

union WrapperCommandList
{
#ifdef DXBUILD
  struct {
    ID3D12CommandAllocator* CommandAllocator;
    ID3D12GraphicsCommandList* CommandList;
  } D3DValue;
#endif
#ifdef GLBUILD
#endif
};

union WrapperResource
{
#ifdef DXBUILD
  ID3D12Resource* D3DValue;
#endif
#ifdef GLBUILD
  GLuint GLValue;
#endif
};

union WrapperIndexVertexBuffersSet
{
#ifdef DXBUILD
  FormattedVertexStorage<irr::video::S3DVertex2TCoords> D3DValue;
#endif
#ifdef GLBUILD
  GLuint GLValue;
#endif
};

union WrapperPipelineState
{
#ifdef GLBUILD
  struct {
    GLuint Program;
    void (*StateSetter)(void);
  } GLValue;
#endif
};

class GFXAPI
{
public:
  enum RESOURCE_USAGE
  {
    PRESENT,
    COPY_DEST,
    COPY_SRC,
    RENDER_TARGET,
  };
  virtual union WrapperResource* createRTT(irr::video::ECOLOR_FORMAT, size_t Width, size_t Height, float fastColor[4]) = 0;
  virtual union WrapperRTTSet* createRTTSet(const std::vector<union WrapperResource*> &RTTs, const std::vector<irr::video::ECOLOR_FORMAT> &formats, size_t Width, size_t Height) = 0;
  virtual void clearRTTSet(union WrapperCommandList* wrappedCmdList, union WrapperRTTSet*, float color[4]) = 0;
  virtual void setRTTSet(union WrapperCommandList* wrappedCmdList, union WrapperRTTSet*) = 0;
  virtual void setPipelineState(union WrapperCommandList* wrappedCmdList, union WrapperPipelineState* pipelineState) = 0;
  virtual void setIndexVertexBuffersSet(union WrapperCommandList* wrappedCmdList, WrapperIndexVertexBuffersSet*) = 0;
  virtual union WrapperResource *createConstantsBuffer(size_t) = 0;
  virtual void *mapConstantsBuffer(union WrapperResource *) = 0;
  virtual void unmapConstantsBuffers(union WrapperResource *wrappedConstantsBuffer) = 0;
  virtual void writeResourcesTransitionBarrier(union WrapperCommandList* wrappedCmdList, const std::vector<std::tuple<union WrapperResource *, enum RESOURCE_USAGE, enum RESOURCE_USAGE> > &) = 0;
  virtual union WrapperCommandList* createCommandList() = 0;
  virtual void closeCommandList(union WrapperCommandList* wrappedCmdList) = 0;
  virtual void drawIndexedInstanced(union WrapperCommandList *wrappedCmdList, size_t indexCount, size_t instanceCount, size_t indexOffset, size_t vertexOffset, size_t instanceOffset) = 0;
};

//Global
extern GFXAPI* GlobalGFXAPI;

#endif