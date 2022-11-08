/****************************************************************************/
/***                                                                      ***/
/***   (C) 2012-2014 Dierk Ohlerich et al., all rights reserved.          ***/
/***                                                                      ***/
/***   Released under BSD 2 clause license, see LICENSE.TXT               ***/
/***                                                                      ***/
/****************************************************************************/

#include "main.hpp"
#include "shader.hpp"

/****************************************************************************/
/***                                                                      ***/
/***                                                                      ***/
/***                                                                      ***/
/****************************************************************************/

void Altona2::Main()
{
    sRunApp(new App,sScreenMode(0,"Create Vertex & Index Buffer in Compute Shader",1280,720));
}

uint App::NodeData[App::MaxNodes*4+256];

/****************************************************************************/
/***                                                                      ***/
/***                                                                      ***/
/***                                                                      ***/
/****************************************************************************/

extern int triTableBase[256][16];

#define DIM1 (App::ThreadDim+1)
const int indexmap1[12] =
{
  0 + 0*DIM1*DIM1*3 + 0*DIM1*3 + 0*3,
  1 + 0*DIM1*DIM1*3 + 0*DIM1*3 + 1*3,
  0 + 0*DIM1*DIM1*3 + 1*DIM1*3 + 0*3,
  1 + 0*DIM1*DIM1*3 + 0*DIM1*3 + 0*3,

  0 + 1*DIM1*DIM1*3 + 0*DIM1*3 + 0*3,
  1 + 1*DIM1*DIM1*3 + 0*DIM1*3 + 1*3,
  0 + 1*DIM1*DIM1*3 + 1*DIM1*3 + 0*3,
  1 + 1*DIM1*DIM1*3 + 0*DIM1*3 + 0*3,

  2 + 0*DIM1*DIM1*3 + 0*DIM1*3 + 0*3,
  2 + 0*DIM1*DIM1*3 + 0*DIM1*3 + 1*3,
  2 + 0*DIM1*DIM1*3 + 1*DIM1*3 + 1*3,
  2 + 0*DIM1*DIM1*3 + 1*DIM1*3 + 0*3,
};

const int indexmap2[12] =
{
  0 + 1*DIM1*DIM1*3 + 0*DIM1*3 + 0*3,
  1 + 1*DIM1*DIM1*3 + 0*DIM1*3 + 1*3,
  0 + 1*DIM1*DIM1*3 + 1*DIM1*3 + 0*3,
  1 + 1*DIM1*DIM1*3 + 0*DIM1*3 + 0*3,

  0 + 0*DIM1*DIM1*3 + 0*DIM1*3 + 0*3,
  1 + 0*DIM1*DIM1*3 + 0*DIM1*3 + 1*3,
  0 + 0*DIM1*DIM1*3 + 1*DIM1*3 + 0*3,
  1 + 0*DIM1*DIM1*3 + 0*DIM1*3 + 0*3,

  2 + 1*DIM1*DIM1*3 + 0*DIM1*3 + 0*3,
  2 + 1*DIM1*DIM1*3 + 0*DIM1*3 + 1*3,
  2 + 1*DIM1*DIM1*3 + 1*DIM1*3 + 1*3,
  2 + 1*DIM1*DIM1*3 + 1*DIM1*3 + 0*3,
};

/****************************************************************************/
/***                                                                      ***/
/***                                                                      ***/
/***                                                                      ***/
/****************************************************************************/


App::App()
{
    DPaint = 0;
    cbv0 = 0;
    BigVB = 0;
    BigIB = 0;
    McIndexMap = 0;
    CNodes = 0;
    CountVC = 0;
    CountIC = 0;
    CShader = 0;
    CMtrl = 0;
    Csb0 = 0;
    IndirectShader = 0;
    IndirectBuffer = 0;
    LineMtrl = 0;
}

App::~App()
{
}

void App::OnInit()
{
    Screen = sGetScreen();
    Adapter = Screen->Adapter;
    Context = Adapter->ImmediateContext;

    DPaint = new sDebugPainter(Adapter);

    // compute

    BigVB = Adapter->CreateRawRW(128*1024*1024,sRBM_Vertex);
    BigIB = Adapter->CreateRawRW(128*1024*1024,sRBM_Index);
    CountVC = Adapter->CreateCounterRW(1,4,0);
    CountIC = Adapter->CreateCounterRW(1,4,0);
    CountNC = Adapter->CreateCounterRW(1,4,0);
    CNodes = Adapter->CreateBuffer(MaxNodes,sRFC_RGBA|sRFB_32|sRFT_UInt,sRU_MapWrite,0,0);
    TempBuffer = Adapter->CreateRawRW(GroupCount*BufferPerGroup,0);

    CShader = Adapter->CreateShader(ComputeShader.Get(0),sST_Compute);
    Csb0 = new sCBuffer<ComputeShader_csb0>(Adapter,sST_Compute,0);

    // index map for compute

    sU32 mcmap[2*256*5];
    for(sInt i=0;i<256;i++)
    {
        for(sInt j=0;j<5;j++)
        {
            if(triTableBase[i][j*3]>=0)
            {
                mcmap[i*5+j      ] = (indexmap1[triTableBase[i][j*3+0]]&0x3ff)|((indexmap1[triTableBase[i][j*3+1]]&0x3ff)<<10)|((indexmap1[triTableBase[i][j*3+2]]&0x3ff)<<20);
                mcmap[i*5+j+256*5] = (indexmap2[triTableBase[i][j*3+0]]&0x3ff)|((indexmap2[triTableBase[i][j*3+1]]&0x3ff)<<10)|((indexmap2[triTableBase[i][j*3+2]]&0x3ff)<<20);
            }
            else
            {
                mcmap[i*5+j      ] = 0xffffffff;
                mcmap[i*5+j+256*5] = 0xffffffff;
            }
        }
    }
    McIndexMap = Adapter->CreateBuffer(2*256*5,sRFC_R|sRFB_32|sRFT_UInt,sRU_Static,mcmap,sizeof(mcmap));

    // geometry

    CGeo = new sGeometry(Adapter);
    CGeo->SetVertex(BigVB,0,0);
    CGeo->SetIndex(BigIB,0);
    CGeo->Prepare(Adapter->FormatPN,sGMF_Index32|sGMP_Tris);

    CMtrl = new sFixedMaterial(Adapter);
    CMtrl->SetState(sRenderStatePara(sMTRL_DepthOn|sMTRL_CullInv,sMB_Off));
    CMtrl->Prepare(Adapter->FormatPN);

    cbv0 = new sCBuffer<sFixedMaterialLightVC>(Adapter,sST_Vertex,0);

    // indirect compute

    IndirectBuffer = Adapter->CreateBufferRW(16,sRFB_32|sRFC_R|sRFT_UInt,sRM_DrawIndirect);
    IndirectShader = Adapter->CreateShader(WriteCount.Get(0),sST_Compute);

    // debug

    sEnablePerfMon(1);
    sGpuPerfMon = new sPerfMonGpu(Adapter,"Gpu");

    LineMtrl = new sFixedMaterial(Adapter);
    LineMtrl->SetState(sRenderStatePara(sMTRL_DepthOn|sMTRL_CullOff,sMB_Off));
    LineMtrl->Prepare(Adapter->FormatP);
}

void App::OnExit()
{
    delete cbv0;
    delete DPaint;
    delete BigVB;
    delete BigIB;
    delete McIndexMap;
    delete TempBuffer;
    delete CNodes;
    delete CountVC;
    delete CountIC;
    delete CountNC;
    delete CGeo;
    delete CMtrl;
    delete Csb0;
    delete IndirectBuffer;
    delete LineMtrl;
}

void App::OnFrame()
{
}

/****************************************************************************/
/***                                                                      ***/
/***                                                                      ***/
/***                                                                      ***/
/****************************************************************************/

sVector41 App::Trans(int x,int y,int z,int s)
{
    float o = float(s*ThreadDim)*0.5f*PosMul;
    sVector41 p(float(x*ThreadDim)*PosMul+PosAdd+o,
                float(y*ThreadDim)*PosMul+PosAdd+o,
                float(z*ThreadDim)*PosMul+PosAdd+o);

    return Matrix*p;
}

float App::Trans(int x)
{
    return sAbs(x*ThreadDim*PosMul);
}

float App::Map(sVector41 pos)
{
//    return MapTorus(pos);
//    return MapBox(pos);
    return MapTemple(pos);
//    return MapBulb(pos);
}

float App::MapTorus(sVector41 p)
{
    sVector2 v(sSqrt(p.x*p.x+p.z*p.z)-0.75f,p.y);
    return sLength(v)-0.25f;
}

float App::MapBox(sVector41 p)
{
    return sMax(sMax(sAbs(p.x)-0.9f,sAbs(p.y)-0.2f),sAbs(p.z)-0.1f);
}

float App::MapBulb(sVector41 pos)
{
    float Power = 8.0f;
    sVector3 z = pos;
    float dr = 1.0f;
    float r = 0.0f;
    for(uint i=0;i<13;i++)
    {
        i = i+1;
        r = sLength(z);
        if(r>2) break;
        dr = sPow(r,Power-1.0f)*Power*dr+1.0f;

        float theta = asin(z.z/r);
        float phi = atan2(z.y,z.x);

        float zr = pow(r,Power);
        theta = theta *Power;
        phi = phi*Power;
        z = zr*sVector3(cos(theta)*cos(phi),sin(phi)*cos(theta),sin(theta));

        z+=pos;
    }
    return 0.5f*sLog(r)*r/dr;
}

float App::MapTemple(sVector41 pos)
{
    pos.y += 3.4f;
//    pos = sVector41(sVector40(pos) * 0.5f);

    float Scale = 2.0f;
    float MinRadius = 0.4f;
    float FixedRadius = 1.0f;
    sVector3 AbsVector(0.0f,2.0f,0.0f);
    sVector3 Vector(0.5f,1.0f,0.5f);
    float RenderRadius = 0.0005f;


    float def = Scale; // 1.0;
    sVector3 p = pos;
    for(uint i=0;i<14;i++)
    {
        p = sAbs(p)-AbsVector;
        float sc=Scale/sClamp(sDot(p, p),MinRadius,FixedRadius);
        p*=sc; 
        def*=sc;
        p = p - Vector;
    }
    return (sLength(p)/def-RenderRadius)*1;
}

void App::Register(int x,int y,int z,int s,uint *&data)
{
    float dist = Map(Trans(x,y,z,s));
    float size = Trans(s);
    if(sAbs(dist) < size*1.0f)
    {
        if(s>1)
        {
            s = s/2;
            Register(x  ,y  ,z  ,s,data);
            Register(x  ,y  ,z+s,s,data);
            Register(x  ,y+s,z  ,s,data);
            Register(x  ,y+s,z+s,s,data);
            Register(x+s,y  ,z  ,s,data);
            Register(x+s,y  ,z+s,s,data);
            Register(x+s,y+s,z  ,s,data);
            Register(x+s,y+s,z+s,s,data);
        }
        else
        {
            if(data<NodeData+MaxNodes*4)
            {
                data[0] = x;
                data[1] = y;
                data[2] = z;
                data[3] = 0;
                data+=4;
            }
        }
    }
}

void App::OnPaint()
{
    sF32 time = sGetTimeMS()*0.0008f;
    sTargetPara tp(sTAR_ClearAll,0xff405060,Screen);

    // matrix

    {
        view.Camera.k.w = -1.7f;
        view.Model = sEulerXYZ(-time*0.11f,-time*0.13f,time*0.15f);
        view.ZoomX = 1/tp.Aspect;
        view.ZoomY = 1;
        view.Prepare(tp);

        float group = GroupDim;
        float scale = 2.1f/float(ThreadDim*group);
        PosMul = scale ;//+ (sSin(time*1.12f)+1)*scale*0.125f;
        PosAdd = -(ThreadDim*group-1)*0.5f*PosMul;

   //     Matrix = sEulerXYZ(-time*0.21f,-time*0.23f,-time*0.25f);
    }

    // find relevant nodes

    static int NodeCount;

    if(!(sGetKeyQualifier() & sKEYQ_Shift))
    {
        NodeCount = 0;
        {
            uint *ptr = NodeData;
            Register(0,0,0,GroupDim,ptr);
            NodeCount = (ptr-NodeData)/4;

            for(int i=0;i<16*4;i++)
                *ptr++ = 0;
            uint *dest=0;

            NodeCount = sMin(NodeCount,128*1024);

            CNodes->MapBuffer(&dest,sRMM_Discard);
            sCopyMem(dest,NodeData,16*NodeCount);
            CNodes->Unmap();
        }

        // compute

        if(!(sGetKeyQualifier() & sKEYQ_Shift))
        {
            sDispatchContext dc;
            dc.SetUav(0,BigVB);
            dc.SetUav(1,BigIB);
//            dc.SetUav(2,CountVC,0);
//            dc.SetUav(3,CountIC,0);
            dc.SetUav(2,CountNC,NodeCount);
            dc.SetUav(3,IndirectBuffer);
            dc.SetUav(4,TempBuffer);
            dc.SetResource(0,McIndexMap);
            dc.SetResource(1,CNodes);
            dc.SetCBuffer(Csb0);
         
            Csb0->Map(Context);
            Csb0->Data->Matrix = Matrix;
            Csb0->Data->PosMul = PosMul;
            Csb0->Data->PosAdd = PosAdd;
            Csb0->Data->Size = 0.06125f;
            Csb0->Data->Delta = PosMul*0.7f;
            Csb0->Unmap();

            Context->BeginDispatch(&dc);
    //        Context->Dispatch(CShader,(int)group,(int)group,(int)group);
//            Context->Dispatch(CShader,4,NodeCount/4,1);
            Context->Dispatch(CShader,GroupCount,1,1);

            Context->EndDispatch(&dc);
        }

        // copy index count to indirect buffer

        {
            sDispatchContext dc;
            dc.SetUav(0,CountIC);
            dc.SetUav(1,IndirectBuffer);

            Context->BeginDispatch(&dc);
            Context->Dispatch(IndirectShader,1,1,1);
            Context->EndDispatch(&dc);
        }
    }

    // render

    Context->BeginTarget(tp);

    sFixedMaterialLightPara lp;
    lp.LightDirWS[0].Set(0,0,-1);
    lp.LightColor[0] = 0xffdfbf;
    lp.AmbientColor = 0x201010;

    cbv0->Map();
    cbv0->Data->Set(view,lp);
    cbv0->Unmap();

    sDrawPara dp(CGeo,CMtrl,cbv0);
    dp.IndirectBuffer = IndirectBuffer;
    dp.IndirectBufferOffset = 0;
    dp.Flags |= sDF_Indirect2;
    Context->Draw(dp);

    // render debug boxes

    if(sGetKeyQualifier() & sKEYQ_Ctrl)
    {
        uint *ptr = NodeData;
        for(int gross=0;gross<NodeCount;gross+=4096)
        {
            int count = sMin(NodeCount-gross,4096);
            Matrix.Identity();
            auto dyn = Adapter->DynamicGeometry;
            dyn->Begin(Adapter->FormatP,2,LineMtrl,count*8,count*24,sGMF_Index16|sGMP_Lines);

            sVertexP *vp;
            dyn->BeginVB(&vp);
            for(int i=0;i<count;i++)
            {
                sVector41 p = Trans(ptr[0],ptr[1],ptr[2],0);
                float d = Trans(1);

                vp[0].Set(p.x+0,p.y+0,p.z+0);
                vp[1].Set(p.x+d,p.y+0,p.z+0);
                vp[2].Set(p.x+d,p.y+d,p.z+0);
                vp[3].Set(p.x+0,p.y+d,p.z+0);
                vp[4].Set(p.x+0,p.y+0,p.z+d);
                vp[5].Set(p.x+d,p.y+0,p.z+d);
                vp[6].Set(p.x+d,p.y+d,p.z+d);
                vp[7].Set(p.x+0,p.y+d,p.z+d);
                vp+=8;
                ptr+=4;
            }
            dyn->EndVB();

            int16 *ip;
            dyn->BeginIB(&ip);
            for(int i=0;i<count;i++)
            {
                int v = i*8;
                ip[ 0] = v+0; ip[ 1] = v+1; ip[ 2] = v+1; ip[ 3] = v+2; ip[ 4] = v+2; ip[ 5] = v+3; ip[ 6] = v+3; ip[ 7] = v+0;
                ip[ 8] = v+4; ip[ 9] = v+5; ip[10] = v+5; ip[11] = v+6; ip[12] = v+6; ip[13] = v+7; ip[14] = v+7; ip[15] = v+4;
                ip[16] = v+0; ip[17] = v+4; ip[18] = v+1; ip[19] = v+5; ip[20] = v+2; ip[21] = v+6; ip[22] = v+3; ip[23] = v+7;
                ip += 24;
            }
            dyn->EndIB();
            dyn->End(cbv0);
        }
    }

    // debug

    DPaint->PrintPerfMon(1);
    DPaint->PrintFPS();
    DPaint->PrintStats();
    DPaint->Draw(tp);

    Context->EndTarget();

}

void App::OnKey(const sKeyData &kd)
{
    if((kd.Key&(sKEYQ_Mask|sKEYQ_Break))==27)
        sExit();
}

void App::OnDrag(const sDragData &dd)
{
}

/****************************************************************************/

int triTableBase[256][16] =
{{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1},
{3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1},
{3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1},
{3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1},
{9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1},
{1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1},
{9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
{2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1},
{8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1},
{9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
{4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1},
{3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1},
{1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1},
{4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1},
{4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1},
{9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1},
{1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
{5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1},
{2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1},
{9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
{0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
{2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1},
{10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1},
{4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1},
{5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1},
{5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1},
{9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1},
{0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1},
{1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1},
{10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1},
{8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1},
{2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1},
{7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1},
{9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1},
{2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1},
{11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1},
{9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1},
{5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
{11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
{11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
{1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1},
{9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1},
{5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1},
{2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
{0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
{5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1},
{6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1},
{0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1},
{3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1},
{6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1},
{5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1},
{1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
{10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1},
{6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1},
{1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1},
{8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1},
{7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
{3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
{5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1},
{0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1},
{9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
{8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1},
{5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
{0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
{6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1},
{10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1},
{10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1},
{8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1},
{1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1},
{3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1},
{0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1},
{10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1},
{0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1},
{3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1},
{6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
{9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1},
{8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
{3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1},
{6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1},
{0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1},
{10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1},
{10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1},
{1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1},
{2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
{7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1},
{7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1},
{2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
{1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
{11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1},
{8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
{0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1},
{7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
{10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
{2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
{6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1},
{7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1},
{2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1},
{1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1},
{10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1},
{10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1},
{0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1},
{7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1},
{6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1},
{8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1},
{9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1},
{6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1},
{1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1},
{4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1},
{10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
{8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1},
{0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1},
{1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1},
{8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1},
{10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1},
{4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
{10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
{5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
{11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1},
{9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
{6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1},
{7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1},
{3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
{7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1},
{9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1},
{3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1},
{6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
{9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1},
{1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
{4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
{7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1},
{6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1},
{3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1},
{0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1},
{6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1},
{1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1},
{0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
{11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
{6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1},
{5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1},
{9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1},
{1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
{1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
{10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1},
{0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1},
{5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1},
{10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1},
{11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1},
{0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1},
{9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1},
{7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
{2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1},
{8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1},
{9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1},
{9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
{1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1},
{9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1},
{9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1},
{5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1},
{0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1},
{10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
{2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1},
{0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
{0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
{9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1},
{5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1},
{3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
{5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1},
{8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1},
{0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1},
{9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1},
{0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1},
{1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1},
{3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
{4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1},
{9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
{11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1},
{11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1},
{2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1},
{9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
{3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
{1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1},
{4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1},
{4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1},
{0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1},
{3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1},
{3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1},
{0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1},
{9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1},
{1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}};

