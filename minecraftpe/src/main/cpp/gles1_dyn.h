#ifndef GLES1_DYN_H
#define GLES1_DYN_H

#include <GLES/gl.h>

#include <string>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*GLES1_ClearProc)(GLbitfield);
typedef void (*GLES1_ClearColorProc)(GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (*GLES1_ViewportProc)(GLint, GLint, GLsizei, GLsizei);
typedef void (*GLES1_EnableProc)(GLenum);
typedef void (*GLES1_DisableProc)(GLenum);
typedef void (*GLES1_MatrixModeProc)(GLenum);
typedef void (*GLES1_LoadIdentityProc)(void);
typedef void (*GLES1_OrthoProc)(GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (*GLES1_FrustumProc)(GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (*GLES1_PushMatrixProc)(void);
typedef void (*GLES1_PopMatrixProc)(void);
typedef void (*GLES1_TranslatefProc)(GLfloat, GLfloat, GLfloat);
typedef void (*GLES1_RotatefProc)(GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (*GLES1_ScalefProc)(GLfloat, GLfloat, GLfloat);
typedef void (*GLES1_LoadMatrixfProc)(const GLfloat*);
typedef void (*GLES1_MultMatrixfProc)(const GLfloat*);
typedef void (*GLES1_EnableClientStateProc)(GLenum);
typedef void (*GLES1_DisableClientStateProc)(GLenum);
typedef void (*GLES1_VertexPointerProc)(GLint, GLenum, GLsizei, const void*);
typedef void (*GLES1_ColorPointerProc)(GLint, GLenum, GLsizei, const void*);
typedef void (*GLES1_TexCoordPointerProc)(GLint, GLenum, GLsizei, const void*);
typedef void (*GLES1_NormalPointerProc)(GLenum, GLsizei, const void*);
typedef void (*GLES1_DrawArraysProc)(GLenum, GLint, GLsizei);
typedef void (*GLES1_DrawElementsProc)(GLenum, GLsizei, GLenum, const void*);
typedef void (*GLES1_Color4fProc)(GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (*GLES1_ShadeModelProc)(GLenum);
typedef void (*GLES1_GenTexturesProc)(GLsizei, GLuint*);
typedef void (*GLES1_BindTextureProc)(GLenum, GLuint);
typedef void (*GLES1_TexImage2DProc)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
typedef void (*GLES1_TexSubImage2DProc)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);
typedef void (*GLES1_TexParameteriProc)(GLenum, GLenum, GLint);
typedef void (*GLES1_TexParameterfProc)(GLenum, GLenum, GLfloat);
typedef void (*GLES1_TexEnvfProc)(GLenum, GLenum, GLfloat);
typedef void (*GLES1_TexEnviProc)(GLenum, GLenum, GLint);
typedef void (*GLES1_BlendFuncProc)(GLenum, GLenum);
typedef void (*GLES1_GetIntegervProc)(GLenum, GLint*);
typedef void (*GLES1_GetFloatvProc)(GLenum, GLfloat*);
typedef const GLubyte* (*GLES1_GetStringProc)(GLenum);
typedef GLenum (*GLES1_GetErrorProc)(void);
typedef void (*GLES1_FlushProc)(void);
typedef void (*GLES1_FinishProc)(void);
typedef void (*GLES1_DepthFuncProc)(GLenum);
typedef void (*GLES1_DepthMaskProc)(GLboolean);
typedef void (*GLES1_ClearDepthProc)(GLfloat);
typedef void (*GLES1_CullFaceProc)(GLenum);
typedef void (*GLES1_HintProc)(GLenum, GLenum);
typedef void (*GLES1_LineWidthProc)(GLfloat);
typedef void (*GLES1_PointSizeProc)(GLfloat);
typedef void (*GLES1_AlphaFuncProc)(GLenum, GLfloat);
typedef void (*GLES1_ScissorProc)(GLint, GLint, GLsizei, GLsizei);
typedef void (*GLES1_PixelStoreiProc)(GLenum, GLint);
typedef void (*GLES1_ReadPixelsProc)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
typedef void (*GLES1_GenBuffersProc)(GLsizei, GLuint*);
typedef void (*GLES1_DeleteBuffersProc)(GLsizei, const GLuint*);
typedef void (*GLES1_BindBufferProc)(GLenum, GLuint);
typedef void (*GLES1_BufferDataProc)(GLenum, GLsizeiptr, const void*, GLenum);
typedef void (*GLES1_ClientActiveTextureProc)(GLenum);
typedef void (*GLES1_ActiveTextureProc)(GLenum);
typedef void (*GLES1_FogfProc)(GLenum, GLfloat);
typedef void (*GLES1_FogfvProc)(GLenum, const GLfloat*);
typedef void (*GLES1_FogxProc)(GLenum, GLfixed);
typedef void (*GLES1_FogiProc)(GLenum, GLint);
typedef void (*GLES1_Normal3fProc)(GLfloat, GLfloat, GLfloat);
typedef void (*GLES1_Color3fProc)(GLfloat, GLfloat, GLfloat);
typedef void (*GLES1_ColorMaskProc)(GLboolean, GLboolean, GLboolean, GLboolean);
typedef void (*GLES1_ColorMaterialProc)(GLenum, GLenum);
typedef void (*GLES1_CompressedTexImage2DProc)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void*);
typedef void (*GLES1_DeleteTexturesProc)(GLsizei, const GLuint*);
typedef void (*GLES1_LightfvProc)(GLenum, GLenum, const GLfloat*);
typedef void (*GLES1_LightModelfvProc)(GLenum, const GLfloat*);
typedef void (*GLES1_PolygonOffsetProc)(GLfloat, GLfloat);
typedef void (*GLES1_DepthRangeProc)(GLfloat, GLfloat);

struct Gles1Api {
    GLES1_ClearProc glClear;
    GLES1_ClearColorProc glClearColor;
    GLES1_ViewportProc glViewport;
    GLES1_EnableProc glEnable;
    GLES1_DisableProc glDisable;
    GLES1_MatrixModeProc glMatrixMode;
    GLES1_LoadIdentityProc glLoadIdentity;
    GLES1_OrthoProc glOrtho;
    GLES1_FrustumProc glFrustum;
    GLES1_PushMatrixProc glPushMatrix;
    GLES1_PopMatrixProc glPopMatrix;
    GLES1_TranslatefProc glTranslatef;
    GLES1_RotatefProc glRotatef;
    GLES1_ScalefProc glScalef;
    GLES1_LoadMatrixfProc glLoadMatrixf;
    GLES1_MultMatrixfProc glMultMatrixf;
    GLES1_EnableClientStateProc glEnableClientState;
    GLES1_DisableClientStateProc glDisableClientState;
    GLES1_VertexPointerProc glVertexPointer;
    GLES1_ColorPointerProc glColorPointer;
    GLES1_TexCoordPointerProc glTexCoordPointer;
    GLES1_NormalPointerProc glNormalPointer;
    GLES1_DrawArraysProc glDrawArrays;
    GLES1_DrawElementsProc glDrawElements;
    GLES1_Color4fProc glColor4f;
    GLES1_ShadeModelProc glShadeModel;
    GLES1_GenTexturesProc glGenTextures;
    GLES1_BindTextureProc glBindTexture;
    GLES1_TexImage2DProc glTexImage2D;
    GLES1_TexSubImage2DProc glTexSubImage2D;
    GLES1_TexParameteriProc glTexParameteri;
    GLES1_TexParameterfProc glTexParameterf;
    GLES1_TexEnvfProc glTexEnvf;
    GLES1_TexEnviProc glTexEnvi;
    GLES1_BlendFuncProc glBlendFunc;
    GLES1_GetIntegervProc glGetIntegerv;
    GLES1_GetFloatvProc glGetFloatv;
    GLES1_GetStringProc glGetString;
    GLES1_GetErrorProc glGetError;
    GLES1_FlushProc glFlush;
    GLES1_FinishProc glFinish;
    GLES1_DepthFuncProc glDepthFunc;
    GLES1_DepthMaskProc glDepthMask;
    GLES1_ClearDepthProc glClearDepth;
    GLES1_CullFaceProc glCullFace;
    GLES1_HintProc glHint;
    GLES1_LineWidthProc glLineWidth;
    GLES1_PointSizeProc glPointSize;
    GLES1_AlphaFuncProc glAlphaFunc;
    GLES1_ScissorProc glScissor;
    GLES1_PixelStoreiProc glPixelStorei;
    GLES1_ReadPixelsProc glReadPixels;
    GLES1_GenBuffersProc glGenBuffers;
    GLES1_DeleteBuffersProc glDeleteBuffers;
    GLES1_BindBufferProc glBindBuffer;
    GLES1_BufferDataProc glBufferData;
    GLES1_ClientActiveTextureProc glClientActiveTexture;
    GLES1_ActiveTextureProc glActiveTexture;
    GLES1_FogfProc glFogf;
    GLES1_FogfvProc glFogfv;
    GLES1_FogxProc glFogx;
    GLES1_FogiProc glFogi;
    GLES1_Normal3fProc glNormal3f;
    GLES1_Color3fProc glColor3f;
    GLES1_ColorMaskProc glColorMask;
    GLES1_ColorMaterialProc glColorMaterial;
    GLES1_CompressedTexImage2DProc glCompressedTexImage2D;
    GLES1_DeleteTexturesProc glDeleteTextures;
    GLES1_LightfvProc glLightfv;
    GLES1_LightModelfvProc glLightModelfv;
    GLES1_PolygonOffsetProc glPolygonOffset;
    GLES1_DepthRangeProc glDepthRange;

    int loaded;     // 成功加载数
    int missing;    // 缺失数
    bool valid;     // 关键符号是否齐全
};

// 从运行时 GL 库中动态加载 GLES1 符号；返回加载到的 API
// missingReport 输出缺失符号名列表
Gles1Api LoadGles1Api(std::string* missingReport);

#ifdef __cplusplus
}
#endif

#endif
