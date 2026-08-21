#include "gles1_dyn.h"

#include <EGL/egl.h>

#include <dlfcn.h>

#include <cstring>
#include <sstream>
#include <vector>

namespace {

const char* kLibNames[] = {
    "libGLESv3.so",
    "libGLESv2.so",
    "libGLESv1_CM.so",
    "libGLES_CM.so",
};

template <typename T>
void LoadSym(const std::vector<void*>& handles, T* out, const char* name, int& loaded, int& missing,
    std::ostringstream& missingReport)
{
    void* sym = (void*)eglGetProcAddress(name);
    if (sym != nullptr) {
        memcpy(out, &sym, sizeof(sym));
        loaded++;
        return;
    }
    for (void* handle : handles) {
        sym = dlsym(handle, name);
        if (sym != nullptr) {
            memcpy(out, &sym, sizeof(sym));
            loaded++;
            return;
        }
    }
    *out = nullptr;
    missing++;
    missingReport << "  " << name << "\n";
}

} // namespace

Gles1Api LoadGles1Api(std::string* missingReport)
{
    Gles1Api api;
    memset(&api, 0, sizeof(api));

    std::vector<void*> handles;
    std::ostringstream report;
    for (const char* lib : kLibNames) {
        void* handle = dlopen(lib, RTLD_NOW | RTLD_LOCAL);
        if (handle != nullptr) {
            handles.push_back(handle);
            report << "opened: " << lib << "\n";
        } else {
            report << "dlopen FAILED " << lib << ": " << dlerror() << "\n";
        }
    }

    if (handles.empty()) {
        report << "dlopen ALL GLES libs FAILED\n";
        api.loaded = 0;
        api.missing = 0x7fffffff;
        api.valid = false;
        if (missingReport) {
            *missingReport = report.str();
        }
        return api;
    }

    int loaded = 0;
    int missing = 0;

    LoadSym(handles, &api.glClear, "glClear", loaded, missing, report);
    LoadSym(handles, &api.glClearColor, "glClearColor", loaded, missing, report);
    LoadSym(handles, &api.glViewport, "glViewport", loaded, missing, report);
    LoadSym(handles, &api.glEnable, "glEnable", loaded, missing, report);
    LoadSym(handles, &api.glDisable, "glDisable", loaded, missing, report);
    LoadSym(handles, &api.glMatrixMode, "glMatrixMode", loaded, missing, report);
    LoadSym(handles, &api.glLoadIdentity, "glLoadIdentity", loaded, missing, report);
    LoadSym(handles, &api.glOrtho, "glOrtho", loaded, missing, report);
    LoadSym(handles, &api.glFrustum, "glFrustum", loaded, missing, report);
    LoadSym(handles, &api.glPushMatrix, "glPushMatrix", loaded, missing, report);
    LoadSym(handles, &api.glPopMatrix, "glPopMatrix", loaded, missing, report);
    LoadSym(handles, &api.glTranslatef, "glTranslatef", loaded, missing, report);
    LoadSym(handles, &api.glRotatef, "glRotatef", loaded, missing, report);
    LoadSym(handles, &api.glScalef, "glScalef", loaded, missing, report);
    LoadSym(handles, &api.glLoadMatrixf, "glLoadMatrixf", loaded, missing, report);
    LoadSym(handles, &api.glMultMatrixf, "glMultMatrixf", loaded, missing, report);
    LoadSym(handles, &api.glEnableClientState, "glEnableClientState", loaded, missing, report);
    LoadSym(handles, &api.glDisableClientState, "glDisableClientState", loaded, missing, report);
    LoadSym(handles, &api.glVertexPointer, "glVertexPointer", loaded, missing, report);
    LoadSym(handles, &api.glColorPointer, "glColorPointer", loaded, missing, report);
    LoadSym(handles, &api.glTexCoordPointer, "glTexCoordPointer", loaded, missing, report);
    LoadSym(handles, &api.glNormalPointer, "glNormalPointer", loaded, missing, report);
    LoadSym(handles, &api.glDrawArrays, "glDrawArrays", loaded, missing, report);
    LoadSym(handles, &api.glDrawElements, "glDrawElements", loaded, missing, report);
    LoadSym(handles, &api.glColor4f, "glColor4f", loaded, missing, report);
    LoadSym(handles, &api.glShadeModel, "glShadeModel", loaded, missing, report);
    LoadSym(handles, &api.glGenTextures, "glGenTextures", loaded, missing, report);
    LoadSym(handles, &api.glBindTexture, "glBindTexture", loaded, missing, report);
    LoadSym(handles, &api.glTexImage2D, "glTexImage2D", loaded, missing, report);
    LoadSym(handles, &api.glTexSubImage2D, "glTexSubImage2D", loaded, missing, report);
    LoadSym(handles, &api.glTexParameteri, "glTexParameteri", loaded, missing, report);
    LoadSym(handles, &api.glTexParameterf, "glTexParameterf", loaded, missing, report);
    LoadSym(handles, &api.glTexEnvf, "glTexEnvf", loaded, missing, report);
    LoadSym(handles, &api.glTexEnvi, "glTexEnvi", loaded, missing, report);
    LoadSym(handles, &api.glBlendFunc, "glBlendFunc", loaded, missing, report);
    LoadSym(handles, &api.glGetIntegerv, "glGetIntegerv", loaded, missing, report);
    LoadSym(handles, &api.glGetFloatv, "glGetFloatv", loaded, missing, report);
    LoadSym(handles, &api.glGetString, "glGetString", loaded, missing, report);
    LoadSym(handles, &api.glGetError, "glGetError", loaded, missing, report);
    LoadSym(handles, &api.glFlush, "glFlush", loaded, missing, report);
    LoadSym(handles, &api.glFinish, "glFinish", loaded, missing, report);
    LoadSym(handles, &api.glDepthFunc, "glDepthFunc", loaded, missing, report);
    LoadSym(handles, &api.glDepthMask, "glDepthMask", loaded, missing, report);
    LoadSym(handles, &api.glClearDepth, "glClearDepth", loaded, missing, report);
    LoadSym(handles, &api.glCullFace, "glCullFace", loaded, missing, report);
    LoadSym(handles, &api.glHint, "glHint", loaded, missing, report);
    LoadSym(handles, &api.glLineWidth, "glLineWidth", loaded, missing, report);
    LoadSym(handles, &api.glPointSize, "glPointSize", loaded, missing, report);
    LoadSym(handles, &api.glAlphaFunc, "glAlphaFunc", loaded, missing, report);
    LoadSym(handles, &api.glScissor, "glScissor", loaded, missing, report);
    LoadSym(handles, &api.glPixelStorei, "glPixelStorei", loaded, missing, report);
    LoadSym(handles, &api.glReadPixels, "glReadPixels", loaded, missing, report);
    LoadSym(handles, &api.glGenBuffers, "glGenBuffers", loaded, missing, report);
    LoadSym(handles, &api.glBindBuffer, "glBindBuffer", loaded, missing, report);
    LoadSym(handles, &api.glBufferData, "glBufferData", loaded, missing, report);
    LoadSym(handles, &api.glClientActiveTexture, "glClientActiveTexture", loaded, missing, report);
    LoadSym(handles, &api.glActiveTexture, "glActiveTexture", loaded, missing, report);
    LoadSym(handles, &api.glFogf, "glFogf", loaded, missing, report);
    LoadSym(handles, &api.glFogfv, "glFogfv", loaded, missing, report);
    LoadSym(handles, &api.glFogx, "glFogx", loaded, missing, report);
    LoadSym(handles, &api.glFogi, "glFogi", loaded, missing, report);
    LoadSym(handles, &api.glNormal3f, "glNormal3f", loaded, missing, report);
    LoadSym(handles, &api.glColor3f, "glColor3f", loaded, missing, report);
    LoadSym(handles, &api.glColorMask, "glColorMask", loaded, missing, report);
    LoadSym(handles, &api.glColorMaterial, "glColorMaterial", loaded, missing, report);
    LoadSym(handles, &api.glCompressedTexImage2D, "glCompressedTexImage2D", loaded, missing, report);
    LoadSym(handles, &api.glDeleteTextures, "glDeleteTextures", loaded, missing, report);
    LoadSym(handles, &api.glLightfv, "glLightfv", loaded, missing, report);
    LoadSym(handles, &api.glLightModelfv, "glLightModelfv", loaded, missing, report);
    LoadSym(handles, &api.glPolygonOffset, "glPolygonOffset", loaded, missing, report);
    LoadSym(handles, &api.glDepthRange, "glDepthRange", loaded, missing, report);
    LoadSym(handles, &api.glDeleteBuffers, "glDeleteBuffers", loaded, missing, report);

    // GLES1 常见的别名
    if (api.glOrtho == nullptr) {
        LoadSym(handles, &api.glOrtho, "glOrthof", loaded, missing, report);
    }
    if (api.glFrustum == nullptr) {
        LoadSym(handles, &api.glFrustum, "glFrustumf", loaded, missing, report);
    }
    if (api.glClearDepth == nullptr) {
        LoadSym(handles, &api.glClearDepth, "glClearDepthf", loaded, missing, report);
    }
    if (api.glDepthRange == nullptr) {
        LoadSym(handles, &api.glDepthRange, "glDepthRangef", loaded, missing, report);
    }
    if (api.glFogi == nullptr) {
        LoadSym(handles, &api.glFogi, "glFogx", loaded, missing, report);
    }
    if (api.glFogx == nullptr) {
        LoadSym(handles, &api.glFogx, "glFogi", loaded, missing, report);
    }
    if (api.glColor3f == nullptr && api.glColor4f != nullptr) {
        api.glColor3f = (GLES1_Color3fProc)(void*)api.glColor4f;
    }

    api.loaded = loaded;
    api.missing = missing;

    const bool keySymbols = api.glClear != nullptr && api.glClearColor != nullptr &&
        api.glViewport != nullptr && api.glMatrixMode != nullptr &&
        api.glLoadIdentity != nullptr && api.glOrtho != nullptr &&
        api.glVertexPointer != nullptr && api.glEnableClientState != nullptr &&
        api.glDrawArrays != nullptr && api.glGetError != nullptr &&
        api.glColorPointer != nullptr;
    api.valid = keySymbols;

    report << "loaded=" << loaded << " missing=" << missing << " valid=" << (api.valid ? "YES" : "NO") << "\n";
    if (missing > 0) {
        report << "missing symbols:\n";
    }

    if (missingReport) {
        *missingReport = report.str();
    }

    return api;
}
