#include "gles1_dyn.h"

#include <cstdio>

// GLES1 stub layer for OHOS.
// Provides real, globally-linkable GLES1 symbols (glOrtho, glVertexPointer, ...)
// which forward to the dynamically loaded GLES1 entry points resolved at runtime
// from the system GLES driver (see gles1_dyn.cpp).
//
// This allows the MCPE source to link unchanged against <GLES/gl.h>.

namespace {

Gles1Api& GetApi()
{
    static Gles1Api api = [] {
        std::string report;
        Gles1Api a = LoadGles1Api(&report);
        printf("[gles1] %s\n", report.c_str());
        fflush(stdout);
        return a;
    }();
    return api;
}

} // namespace

extern "C" {

void glActiveTexture(GLenum texture) { if (GetApi().glActiveTexture) GetApi().glActiveTexture(texture); }
void glAlphaFunc(GLenum func, GLclampf ref) { if (GetApi().glAlphaFunc) GetApi().glAlphaFunc(func, ref); }
void glBindBuffer(GLenum target, GLuint buffer) { if (GetApi().glBindBuffer) GetApi().glBindBuffer(target, buffer); }
void glBindTexture(GLenum target, GLuint texture) { if (GetApi().glBindTexture) GetApi().glBindTexture(target, texture); }
void glBlendFunc(GLenum sfactor, GLenum dfactor) { if (GetApi().glBlendFunc) GetApi().glBlendFunc(sfactor, dfactor); }
void glBufferData(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage) { if (GetApi().glBufferData) GetApi().glBufferData(target, size, data, usage); }
void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data) { if (GetApi().glBufferData) GetApi().glBufferData(target, size, data, GL_STATIC_DRAW); }
void glClear(GLbitfield mask) { if (GetApi().glClear) GetApi().glClear(mask); }
void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a) { if (GetApi().glClearColor) GetApi().glClearColor(r, g, b, a); }
void glClearDepthf(GLclampf depth) { if (GetApi().glClearDepth) GetApi().glClearDepth(depth); }
void glClearStencil(GLint s) { (void)s; }
void glClientActiveTexture(GLenum texture) { if (GetApi().glClientActiveTexture) GetApi().glClientActiveTexture(texture); }
void glColor3f(GLfloat r, GLfloat g, GLfloat b) { if (GetApi().glColor3f) GetApi().glColor3f(r, g, b); else if (GetApi().glColor4f) GetApi().glColor4f(r, g, b, 1.0f); }
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { if (GetApi().glColor4f) GetApi().glColor4f(r, g, b, a); }
void glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) { if (GetApi().glColorMask) GetApi().glColorMask(r, g, b, a); }
void glColorMaterial(GLenum face, GLenum mode) { if (GetApi().glColorMaterial) GetApi().glColorMaterial(face, mode); }
void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) { if (GetApi().glColorPointer) GetApi().glColorPointer(size, type, stride, pointer); }
void glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid* data) { if (GetApi().glCompressedTexImage2D) GetApi().glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data); }
void glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid* data) { (void)target; (void)level; (void)xoffset; (void)yoffset; (void)width; (void)height; (void)format; (void)imageSize; (void)data; }
void glCullFace(GLenum mode) { if (GetApi().glCullFace) GetApi().glCullFace(mode); }
void glDeleteBuffers(GLsizei n, const GLuint* buffers) { if (GetApi().glDeleteBuffers) GetApi().glDeleteBuffers(n, buffers); }
void glDeleteTextures(GLsizei n, const GLuint* textures) { if (GetApi().glDeleteTextures) GetApi().glDeleteTextures(n, textures); }
void glDepthFunc(GLenum func) { if (GetApi().glDepthFunc) GetApi().glDepthFunc(func); }
void glDepthMask(GLboolean flag) { if (GetApi().glDepthMask) GetApi().glDepthMask(flag); }
void glDepthRangef(GLclampf zNear, GLclampf zFar) { if (GetApi().glDepthRange) GetApi().glDepthRange(zNear, zFar); }
void glDisable(GLenum cap) { if (GetApi().glDisable) GetApi().glDisable(cap); }
void glDisableClientState(GLenum array) { if (GetApi().glDisableClientState) GetApi().glDisableClientState(array); }
void glDrawArrays(GLenum mode, GLint first, GLsizei count) { if (GetApi().glDrawArrays) GetApi().glDrawArrays(mode, first, count); }
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices) { if (GetApi().glDrawElements) GetApi().glDrawElements(mode, count, type, indices); }
void glEnable(GLenum cap) { if (GetApi().glEnable) GetApi().glEnable(cap); }
void glEnableClientState(GLenum array) { if (GetApi().glEnableClientState) GetApi().glEnableClientState(array); }
void glFinish(void) { if (GetApi().glFinish) GetApi().glFinish(); }
void glFlush(void) { if (GetApi().glFlush) GetApi().glFlush(); }
void glFogf(GLenum pname, GLfloat param) { if (GetApi().glFogf) GetApi().glFogf(pname, param); }
void glFogfv(GLenum pname, const GLfloat* params) { if (GetApi().glFogfv) GetApi().glFogfv(pname, params); }
void glFogx(GLenum pname, GLfixed param) { if (GetApi().glFogx) GetApi().glFogx(pname, param); else if (GetApi().glFogi) GetApi().glFogi(pname, (GLint)param); }
void glFogxv(GLenum pname, const GLfixed* params) { if (params) glFogx(pname, params[0]); }
void glFrustumf(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) { if (GetApi().glFrustum) GetApi().glFrustum(l, r, b, t, n, f); }
void glGenBuffers(GLsizei n, GLuint* buffers) { if (GetApi().glGenBuffers) GetApi().glGenBuffers(n, buffers); else for (GLsizei i = 0; i < n; ++i) buffers[i] = 0; }
void glGenTextures(GLsizei n, GLuint* textures) { if (GetApi().glGenTextures) GetApi().glGenTextures(n, textures); else for (GLsizei i = 0; i < n; ++i) textures[i] = 0; }
GLenum glGetError(void) { if (GetApi().glGetError) return GetApi().glGetError(); return 0; }
void glGetFloatv(GLenum pname, GLfloat* params) { if (GetApi().glGetFloatv) GetApi().glGetFloatv(pname, params); }
void glGetIntegerv(GLenum pname, GLint* params) { if (GetApi().glGetIntegerv) GetApi().glGetIntegerv(pname, params); }
const GLubyte* glGetString(GLenum name) { if (GetApi().glGetString) return GetApi().glGetString(name); return (const GLubyte*)""; }
void glHint(GLenum target, GLenum mode) { if (GetApi().glHint) GetApi().glHint(target, mode); }
void glLightModelfv(GLenum pname, const GLfloat* params) { if (GetApi().glLightModelfv) GetApi().glLightModelfv(pname, params); }
void glLightfv(GLenum light, GLenum pname, const GLfloat* params) { if (GetApi().glLightfv) GetApi().glLightfv(light, pname, params); }
void glLineWidth(GLfloat width) { if (GetApi().glLineWidth) GetApi().glLineWidth(width); }
void glLoadIdentity(void) { if (GetApi().glLoadIdentity) GetApi().glLoadIdentity(); }
void glLoadMatrixf(const GLfloat* m) { if (GetApi().glLoadMatrixf) GetApi().glLoadMatrixf(m); }
void glMatrixMode(GLenum mode) { if (GetApi().glMatrixMode) GetApi().glMatrixMode(mode); }
void glMultMatrixf(const GLfloat* m) { if (GetApi().glMultMatrixf) GetApi().glMultMatrixf(m); }
void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz) { if (GetApi().glNormal3f) GetApi().glNormal3f(nx, ny, nz); }
void glNormalPointer(GLenum type, GLsizei stride, const GLvoid* pointer) { if (GetApi().glNormalPointer) GetApi().glNormalPointer(type, stride, pointer); }
void glOrthof(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) { if (GetApi().glOrtho) GetApi().glOrtho(l, r, b, t, n, f); }
void glPixelStorei(GLenum pname, GLint param) { if (GetApi().glPixelStorei) GetApi().glPixelStorei(pname, param); }
void glPointSize(GLfloat size) { if (GetApi().glPointSize) GetApi().glPointSize(size); }
void glPolygonOffset(GLfloat factor, GLfloat units) { if (GetApi().glPolygonOffset) GetApi().glPolygonOffset(factor, units); }
void glPopMatrix(void) { if (GetApi().glPopMatrix) GetApi().glPopMatrix(); }
void glPushMatrix(void) { if (GetApi().glPushMatrix) GetApi().glPushMatrix(); }
void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid* pixels) { if (GetApi().glReadPixels) GetApi().glReadPixels(x, y, width, height, format, type, pixels); }
void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) { if (GetApi().glRotatef) GetApi().glRotatef(angle, x, y, z); }
void glSampleCoverage(GLclampf value, GLboolean invert) { (void)value; (void)invert; }
void glScalef(GLfloat x, GLfloat y, GLfloat z) { if (GetApi().glScalef) GetApi().glScalef(x, y, z); }
void glScissor(GLint x, GLint y, GLsizei width, GLsizei height) { if (GetApi().glScissor) GetApi().glScissor(x, y, width, height); }
void glShadeModel(GLenum mode) { if (GetApi().glShadeModel) GetApi().glShadeModel(mode); }
void glStencilFunc(GLenum func, GLint ref, GLuint mask) { (void)func; (void)ref; (void)mask; }
void glStencilMask(GLuint mask) { (void)mask; }
void glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) { (void)fail; (void)zfail; (void)zpass; }
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) { if (GetApi().glTexCoordPointer) GetApi().glTexCoordPointer(size, type, stride, pointer); }
void glTexEnvf(GLenum target, GLenum pname, GLfloat param) { if (GetApi().glTexEnvf) GetApi().glTexEnvf(target, pname, param); }
void glTexEnvi(GLenum target, GLenum pname, GLint param) { if (GetApi().glTexEnvi) GetApi().glTexEnvi(target, pname, param); }
void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* pixels) { if (GetApi().glTexImage2D) GetApi().glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels); }
void glTexParameterf(GLenum target, GLenum pname, GLfloat param) { if (GetApi().glTexParameterf) GetApi().glTexParameterf(target, pname, param); }
void glTexParameterfv(GLenum target, GLenum pname, const GLfloat* params) { if (params) glTexParameterf(target, pname, params[0]); }
void glTexParameteri(GLenum target, GLenum pname, GLint param) { if (GetApi().glTexParameteri) GetApi().glTexParameteri(target, pname, param); }
void glTexParameteriv(GLenum target, GLenum pname, const GLint* params) { if (params) glTexParameteri(target, pname, params[0]); }
void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid* pixels) { if (GetApi().glTexSubImage2D) GetApi().glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels); }
void glTranslatef(GLfloat x, GLfloat y, GLfloat z) { if (GetApi().glTranslatef) GetApi().glTranslatef(x, y, z); }
void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) { if (GetApi().glVertexPointer) GetApi().glVertexPointer(size, type, stride, pointer); }
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) { if (GetApi().glViewport) GetApi().glViewport(x, y, width, height); }

// Desktop-GL style aliases used by a few MCPE call sites.
void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) { if (GetApi().glOrtho) GetApi().glOrtho((GLfloat)l, (GLfloat)r, (GLfloat)b, (GLfloat)t, (GLfloat)n, (GLfloat)f); }
void glClearDepth(GLdouble depth) { if (GetApi().glClearDepth) GetApi().glClearDepth((GLfloat)depth); }
void glDepthRange(GLdouble zNear, GLdouble zFar) { if (GetApi().glDepthRange) GetApi().glDepthRange((GLfloat)zNear, (GLfloat)zFar); }
void glFogi(GLenum pname, GLint param) { if (GetApi().glFogi) GetApi().glFogi(pname, param); else glFogf(pname, (GLfloat)param); }


} // extern "C"