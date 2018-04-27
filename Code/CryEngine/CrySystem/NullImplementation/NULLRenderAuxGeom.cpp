// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NULLRenderAuxGeom.h"
#include <CryCore/Platform/CryLibrary.h>

CNULLRenderAuxGeom* CNULLRenderAuxGeom::s_pThis = NULL;

#ifdef ENABLE_WGL_DEBUG_RENDERER

static const float PI = 3.14159265358979323f;
static const float W = 800.0f;
static const float H = 600.0f;
static const float THETA = 5.0f;
static const Vec3 VUP(0.0f, 0.0f, 1.0f);

bool CNULLRenderAuxGeom::s_active = false;
bool CNULLRenderAuxGeom::s_hidden = true;

	#ifdef wglUseFontBitmaps
		#undef wglUseFontBitmaps
	#endif

//////////////////////////////////////////////////////////////////////////
// Dynamically load OpenGL.
//////////////////////////////////////////////////////////////////////////

namespace cry_opengl
{
HGLRC(WINAPI * cry_wglCreateContext)(HDC);
BOOL(WINAPI * cry_wglDeleteContext)(HGLRC);
BOOL(WINAPI * cry_wglMakeCurrent)(HDC, HGLRC);
BOOL(WINAPI * cry_wglUseFontBitmaps)(HDC, DWORD, DWORD, DWORD);

void(APIENTRY * cry_gluPerspective)(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar);
void(APIENTRY * cry_gluLookAt)(GLdouble eyex, GLdouble eyey, GLdouble eyez, GLdouble centerx, GLdouble centery, GLdouble centerz, GLdouble upx, GLdouble upy, GLdouble upz);
int(APIENTRY * cry_gluProject) (GLdouble objx, GLdouble objy, GLdouble objz, const GLdouble modelMatrix[16], const GLdouble projMatrix[16], const GLint viewport[4], GLdouble * winx, GLdouble * winy, GLdouble * winz);
void(APIENTRY * cry_gluSphere) (GLUquadric * qobj, GLdouble radius, GLint slices, GLint stacks);
GLUquadric* (APIENTRY * cry_gluNewQuadric)(void);
void(APIENTRY * cry_gluDeleteQuadric) (GLUquadric * state);

void(APIENTRY * cry_glAccum)(GLenum op, GLfloat value);
void(APIENTRY * cry_glAlphaFunc)(GLenum func, GLclampf ref);
GLboolean(APIENTRY * cry_glAreTexturesResident)(GLsizei n, const GLuint * textures, GLboolean * residences);
void(APIENTRY * cry_glArrayElement)(GLint i);
void(APIENTRY * cry_glBegin)(GLenum mode);
void(APIENTRY * cry_glBindTexture)(GLenum target, GLuint texture);
void(APIENTRY * cry_glBitmap)(GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte * bitmap);
void(APIENTRY * cry_glBlendFunc)(GLenum sfactor, GLenum dfactor);
void(APIENTRY * cry_glCallList)(GLuint list);
void(APIENTRY * cry_glCallLists)(GLsizei n, GLenum type, const GLvoid * lists);
void(APIENTRY * cry_glClear)(GLbitfield mask);
void(APIENTRY * cry_glClearAccum)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void(APIENTRY * cry_glClearColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void(APIENTRY * cry_glClearDepth)(GLclampd depth);
void(APIENTRY * cry_glClearIndex)(GLfloat c);
void(APIENTRY * cry_glClearStencil)(GLint s);
void(APIENTRY * cry_glClipPlane)(GLenum plane, const GLdouble * equation);
void(APIENTRY * cry_glColor3b)(GLbyte red, GLbyte green, GLbyte blue);
void(APIENTRY * cry_glColor3bv)(const GLbyte * v);
void(APIENTRY * cry_glColor3d)(GLdouble red, GLdouble green, GLdouble blue);
void(APIENTRY * cry_glColor3dv)(const GLdouble * v);
void(APIENTRY * cry_glColor3f)(GLfloat red, GLfloat green, GLfloat blue);
void(APIENTRY * cry_glColor3fv)(const GLfloat * v);
void(APIENTRY * cry_glColor3i)(GLint red, GLint green, GLint blue);
void(APIENTRY * cry_glColor3iv)(const GLint * v);
void(APIENTRY * cry_glColor3s)(GLshort red, GLshort green, GLshort blue);
void(APIENTRY * cry_glColor3sv)(const GLshort * v);
void(APIENTRY * cry_glColor3ub)(GLubyte red, GLubyte green, GLubyte blue);
void(APIENTRY * cry_glColor3ubv)(const GLubyte * v);
void(APIENTRY * cry_glColor3ui)(GLuint red, GLuint green, GLuint blue);
void(APIENTRY * cry_glColor3uiv)(const GLuint * v);
void(APIENTRY * cry_glColor3us)(GLushort red, GLushort green, GLushort blue);
void(APIENTRY * cry_glColor3usv)(const GLushort * v);
void(APIENTRY * cry_glColor4b)(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha);
void(APIENTRY * cry_glColor4bv)(const GLbyte * v);
void(APIENTRY * cry_glColor4d)(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha);
void(APIENTRY * cry_glColor4dv)(const GLdouble * v);
void(APIENTRY * cry_glColor4f)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void(APIENTRY * cry_glColor4fv)(const GLfloat * v);
void(APIENTRY * cry_glColor4i)(GLint red, GLint green, GLint blue, GLint alpha);
void(APIENTRY * cry_glColor4iv)(const GLint * v);
void(APIENTRY * cry_glColor4s)(GLshort red, GLshort green, GLshort blue, GLshort alpha);
void(APIENTRY * cry_glColor4sv)(const GLshort * v);
void(APIENTRY * cry_glColor4ub)(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
void(APIENTRY * cry_glColor4ubv)(const GLubyte * v);
void(APIENTRY * cry_glColor4ui)(GLuint red, GLuint green, GLuint blue, GLuint alpha);
void(APIENTRY * cry_glColor4uiv)(const GLuint * v);
void(APIENTRY * cry_glColor4us)(GLushort red, GLushort green, GLushort blue, GLushort alpha);
void(APIENTRY * cry_glColor4usv)(const GLushort * v);
void(APIENTRY * cry_glColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void(APIENTRY * cry_glColorMaterial)(GLenum face, GLenum mode);
void(APIENTRY * cry_glColorPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer);
void(APIENTRY * cry_glCopyPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type);
void(APIENTRY * cry_glCopyTexImage1D)(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border);
void(APIENTRY * cry_glCopyTexImage2D)(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
void(APIENTRY * cry_glCopyTexSubImage1D)(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
void(APIENTRY * cry_glCopyTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
void(APIENTRY * cry_glCullFace)(GLenum mode);
void(APIENTRY * cry_glDeleteLists)(GLuint list, GLsizei range);
void(APIENTRY * cry_glDeleteTextures)(GLsizei n, const GLuint * textures);
void(APIENTRY * cry_glDepthFunc)(GLenum func);
void(APIENTRY * cry_glDepthMask)(GLboolean flag);
void(APIENTRY * cry_glDepthRange)(GLclampd zNear, GLclampd zFar);
void(APIENTRY * cry_glDisable)(GLenum cap);
void(APIENTRY * cry_glDisableClientState)(GLenum array);
void(APIENTRY * cry_glDrawArrays)(GLenum mode, GLint first, GLsizei count);
void(APIENTRY * cry_glDrawBuffer)(GLenum mode);
void(APIENTRY * cry_glDrawElements)(GLenum mode, GLsizei count, GLenum type, const GLvoid * indices);
void(APIENTRY * cry_glDrawPixels)(GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * pixels);
void(APIENTRY * cry_glEdgeFlag)(GLboolean flag);
void(APIENTRY * cry_glEdgeFlagPointer)(GLsizei stride, const GLvoid * pointer);
void(APIENTRY * cry_glEdgeFlagv)(const GLboolean * flag);
void(APIENTRY * cry_glEnable)(GLenum cap);
void(APIENTRY * cry_glEnableClientState)(GLenum array);
void(APIENTRY * cry_glEnd)(void);
void(APIENTRY * cry_glEndList)(void);
void(APIENTRY * cry_glEvalCoord1d)(GLdouble u);
void(APIENTRY * cry_glEvalCoord1dv)(const GLdouble * u);
void(APIENTRY * cry_glEvalCoord1f)(GLfloat u);
void(APIENTRY * cry_glEvalCoord1fv)(const GLfloat * u);
void(APIENTRY * cry_glEvalCoord2d)(GLdouble u, GLdouble v);
void(APIENTRY * cry_glEvalCoord2dv)(const GLdouble * u);
void(APIENTRY * cry_glEvalCoord2f)(GLfloat u, GLfloat v);
void(APIENTRY * cry_glEvalCoord2fv)(const GLfloat * u);
void(APIENTRY * cry_glEvalMesh1)(GLenum mode, GLint i1, GLint i2);
void(APIENTRY * cry_glEvalMesh2)(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2);
void(APIENTRY * cry_glEvalPoint1)(GLint i);
void(APIENTRY * cry_glEvalPoint2)(GLint i, GLint j);
void(APIENTRY * cry_glFeedbackBuffer)(GLsizei size, GLenum type, GLfloat * buffer);
void(APIENTRY * cry_glFinish)(void);
void(APIENTRY * cry_glFlush)(void);
void(APIENTRY * cry_glFogf)(GLenum pname, GLfloat param);
void(APIENTRY * cry_glFogfv)(GLenum pname, const GLfloat * params);
void(APIENTRY * cry_glFogi)(GLenum pname, GLint param);
void(APIENTRY * cry_glFogiv)(GLenum pname, const GLint * params);
void(APIENTRY * cry_glFrontFace)(GLenum mode);
void(APIENTRY * cry_glFrustum)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
GLuint(APIENTRY * cry_glGenLists)(GLsizei range);
void(APIENTRY * cry_glGenTextures)(GLsizei n, GLuint * textures);
void(APIENTRY * cry_glGetBooleanv)(GLenum pname, GLboolean * params);
void(APIENTRY * cry_glGetClipPlane)(GLenum plane, GLdouble * equation);
void(APIENTRY * cry_glGetDoublev)(GLenum pname, GLdouble * params);
GLenum(APIENTRY * cry_glGetError)(void);
void(APIENTRY * cry_glGetFloatv)(GLenum pname, GLfloat * params);
void(APIENTRY * cry_glGetIntegerv)(GLenum pname, GLint * params);
void(APIENTRY * cry_glGetLightfv)(GLenum light, GLenum pname, GLfloat * params);
void(APIENTRY * cry_glGetLightiv)(GLenum light, GLenum pname, GLint * params);
void(APIENTRY * cry_glGetMapdv)(GLenum target, GLenum query, GLdouble * v);
void(APIENTRY * cry_glGetMapfv)(GLenum target, GLenum query, GLfloat * v);
void(APIENTRY * cry_glGetMapiv)(GLenum target, GLenum query, GLint * v);
void(APIENTRY * cry_glGetMaterialfv)(GLenum face, GLenum pname, GLfloat * params);
void(APIENTRY * cry_glGetMaterialiv)(GLenum face, GLenum pname, GLint * params);
void(APIENTRY * cry_glGetPixelMapfv)(GLenum map, GLfloat * values);
void(APIENTRY * cry_glGetPixelMapuiv)(GLenum map, GLuint * values);
void(APIENTRY * cry_glGetPixelMapusv)(GLenum map, GLushort * values);
void(APIENTRY * cry_glGetPointerv)(GLenum pname, GLvoid * *params);
void(APIENTRY * cry_glGetPolygonStipple)(GLubyte * mask);
const GLubyte* (APIENTRY * cry_glGetString)(GLenum name);
void(APIENTRY * cry_glGetTexEnvfv)(GLenum target, GLenum pname, GLfloat * params);
void(APIENTRY * cry_glGetTexEnviv)(GLenum target, GLenum pname, GLint * params);
void(APIENTRY * cry_glGetTexGendv)(GLenum coord, GLenum pname, GLdouble * params);
void(APIENTRY * cry_glGetTexGenfv)(GLenum coord, GLenum pname, GLfloat * params);
void(APIENTRY * cry_glGetTexGeniv)(GLenum coord, GLenum pname, GLint * params);
void(APIENTRY * cry_glGetTexImage)(GLenum target, GLint level, GLenum format, GLenum type, GLvoid * pixels);
void(APIENTRY * cry_glGetTexLevelParameterfv)(GLenum target, GLint level, GLenum pname, GLfloat * params);
void(APIENTRY * cry_glGetTexLevelParameteriv)(GLenum target, GLint level, GLenum pname, GLint * params);
void(APIENTRY * cry_glGetTexParameterfv)(GLenum target, GLenum pname, GLfloat * params);
void(APIENTRY * cry_glGetTexParameteriv)(GLenum target, GLenum pname, GLint * params);
void(APIENTRY * cry_glHint)(GLenum target, GLenum mode);
void(APIENTRY * cry_glIndexMask)(GLuint mask);
void(APIENTRY * cry_glIndexPointer)(GLenum type, GLsizei stride, const GLvoid * pointer);
void(APIENTRY * cry_glIndexd)(GLdouble c);
void(APIENTRY * cry_glIndexdv)(const GLdouble * c);
void(APIENTRY * cry_glIndexf)(GLfloat c);
void(APIENTRY * cry_glIndexfv)(const GLfloat * c);
void(APIENTRY * cry_glIndexi)(GLint c);
void(APIENTRY * cry_glIndexiv)(const GLint * c);
void(APIENTRY * cry_glIndexs)(GLshort c);
void(APIENTRY * cry_glIndexsv)(const GLshort * c);
void(APIENTRY * cry_glIndexub)(GLubyte c);
void(APIENTRY * cry_glIndexubv)(const GLubyte * c);
void(APIENTRY * cry_glInitNames)(void);
void(APIENTRY * cry_glInterleavedArrays)(GLenum format, GLsizei stride, const GLvoid * pointer);
GLboolean(APIENTRY * cry_glIsEnabled)(GLenum cap);
GLboolean(APIENTRY * cry_glIsList)(GLuint list);
GLboolean(APIENTRY * cry_glIsTexture)(GLuint texture);
void(APIENTRY * cry_glLightModelf)(GLenum pname, GLfloat param);
void(APIENTRY * cry_glLightModelfv)(GLenum pname, const GLfloat * params);
void(APIENTRY * cry_glLightModeli)(GLenum pname, GLint param);
void(APIENTRY * cry_glLightModeliv)(GLenum pname, const GLint * params);
void(APIENTRY * cry_glLightf)(GLenum light, GLenum pname, GLfloat param);
void(APIENTRY * cry_glLightfv)(GLenum light, GLenum pname, const GLfloat * params);
void(APIENTRY * cry_glLighti)(GLenum light, GLenum pname, GLint param);
void(APIENTRY * cry_glLightiv)(GLenum light, GLenum pname, const GLint * params);
void(APIENTRY * cry_glLineStipple)(GLint factor, GLushort pattern);
void(APIENTRY * cry_glLineWidth)(GLfloat width);
void(APIENTRY * cry_glListBase)(GLuint base);
void(APIENTRY * cry_glLoadIdentity)(void);
void(APIENTRY * cry_glLoadMatrixd)(const GLdouble * m);
void(APIENTRY * cry_glLoadMatrixf)(const GLfloat * m);
void(APIENTRY * cry_glLoadName)(GLuint name);
void(APIENTRY * cry_glLogicOp)(GLenum opcode);
void(APIENTRY * cry_glMap1d)(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble * points);
void(APIENTRY * cry_glMap1f)(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat * points);
void(APIENTRY * cry_glMap2d)(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble * points);
void(APIENTRY * cry_glMap2f)(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat * points);
void(APIENTRY * cry_glMapGrid1d)(GLint un, GLdouble u1, GLdouble u2);
void(APIENTRY * cry_glMapGrid1f)(GLint un, GLfloat u1, GLfloat u2);
void(APIENTRY * cry_glMapGrid2d)(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2);
void(APIENTRY * cry_glMapGrid2f)(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2);
void(APIENTRY * cry_glMaterialf)(GLenum face, GLenum pname, GLfloat param);
void(APIENTRY * cry_glMaterialfv)(GLenum face, GLenum pname, const GLfloat * params);
void(APIENTRY * cry_glMateriali)(GLenum face, GLenum pname, GLint param);
void(APIENTRY * cry_glMaterialiv)(GLenum face, GLenum pname, const GLint * params);
void(APIENTRY * cry_glMatrixMode)(GLenum mode);
void(APIENTRY * cry_glMultMatrixd)(const GLdouble * m);
void(APIENTRY * cry_glMultMatrixf)(const GLfloat * m);
void(APIENTRY * cry_glNewList)(GLuint list, GLenum mode);
void(APIENTRY * cry_glNormal3b)(GLbyte nx, GLbyte ny, GLbyte nz);
void(APIENTRY * cry_glNormal3bv)(const GLbyte * v);
void(APIENTRY * cry_glNormal3d)(GLdouble nx, GLdouble ny, GLdouble nz);
void(APIENTRY * cry_glNormal3dv)(const GLdouble * v);
void(APIENTRY * cry_glNormal3f)(GLfloat nx, GLfloat ny, GLfloat nz);
void(APIENTRY * cry_glNormal3fv)(const GLfloat * v);
void(APIENTRY * cry_glNormal3i)(GLint nx, GLint ny, GLint nz);
void(APIENTRY * cry_glNormal3iv)(const GLint * v);
void(APIENTRY * cry_glNormal3s)(GLshort nx, GLshort ny, GLshort nz);
void(APIENTRY * cry_glNormal3sv)(const GLshort * v);
void(APIENTRY * cry_glNormalPointer)(GLenum type, GLsizei stride, const GLvoid * pointer);
void(APIENTRY * cry_glOrtho)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
void(APIENTRY * cry_glPassThrough)(GLfloat token);
void(APIENTRY * cry_glPixelMapfv)(GLenum map, GLsizei mapsize, const GLfloat * values);
void(APIENTRY * cry_glPixelMapuiv)(GLenum map, GLsizei mapsize, const GLuint * values);
void(APIENTRY * cry_glPixelMapusv)(GLenum map, GLsizei mapsize, const GLushort * values);
void(APIENTRY * cry_glPixelStoref)(GLenum pname, GLfloat param);
void(APIENTRY * cry_glPixelStorei)(GLenum pname, GLint param);
void(APIENTRY * cry_glPixelTransferf)(GLenum pname, GLfloat param);
void(APIENTRY * cry_glPixelTransferi)(GLenum pname, GLint param);
void(APIENTRY * cry_glPixelZoom)(GLfloat xfactor, GLfloat yfactor);
void(APIENTRY * cry_glPointSize)(GLfloat size);
void(APIENTRY * cry_glPolygonMode)(GLenum face, GLenum mode);
void(APIENTRY * cry_glPolygonOffset)(GLfloat factor, GLfloat units);
void(APIENTRY * cry_glPolygonStipple)(const GLubyte * mask);
void(APIENTRY * cry_glPopAttrib)(void);
void(APIENTRY * cry_glPopClientAttrib)(void);
void(APIENTRY * cry_glPopMatrix)(void);
void(APIENTRY * cry_glPopName)(void);
void(APIENTRY * cry_glPrioritizeTextures)(GLsizei n, const GLuint * textures, const GLclampf * priorities);
void(APIENTRY * cry_glPushAttrib)(GLbitfield mask);
void(APIENTRY * cry_glPushClientAttrib)(GLbitfield mask);
void(APIENTRY * cry_glPushMatrix)(void);
void(APIENTRY * cry_glPushName)(GLuint name);
void(APIENTRY * cry_glRasterPos2d)(GLdouble x, GLdouble y);
void(APIENTRY * cry_glRasterPos2dv)(const GLdouble * v);
void(APIENTRY * cry_glRasterPos2f)(GLfloat x, GLfloat y);
void(APIENTRY * cry_glRasterPos2fv)(const GLfloat * v);
void(APIENTRY * cry_glRasterPos2i)(GLint x, GLint y);
void(APIENTRY * cry_glRasterPos2iv)(const GLint * v);
void(APIENTRY * cry_glRasterPos2s)(GLshort x, GLshort y);
void(APIENTRY * cry_glRasterPos2sv)(const GLshort * v);
void(APIENTRY * cry_glRasterPos3d)(GLdouble x, GLdouble y, GLdouble z);
void(APIENTRY * cry_glRasterPos3dv)(const GLdouble * v);
void(APIENTRY * cry_glRasterPos3f)(GLfloat x, GLfloat y, GLfloat z);
void(APIENTRY * cry_glRasterPos3fv)(const GLfloat * v);
void(APIENTRY * cry_glRasterPos3i)(GLint x, GLint y, GLint z);
void(APIENTRY * cry_glRasterPos3iv)(const GLint * v);
void(APIENTRY * cry_glRasterPos3s)(GLshort x, GLshort y, GLshort z);
void(APIENTRY * cry_glRasterPos3sv)(const GLshort * v);
void(APIENTRY * cry_glRasterPos4d)(GLdouble x, GLdouble y, GLdouble z, GLdouble w);
void(APIENTRY * cry_glRasterPos4dv)(const GLdouble * v);
void(APIENTRY * cry_glRasterPos4f)(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void(APIENTRY * cry_glRasterPos4fv)(const GLfloat * v);
void(APIENTRY * cry_glRasterPos4i)(GLint x, GLint y, GLint z, GLint w);
void(APIENTRY * cry_glRasterPos4iv)(const GLint * v);
void(APIENTRY * cry_glRasterPos4s)(GLshort x, GLshort y, GLshort z, GLshort w);
void(APIENTRY * cry_glRasterPos4sv)(const GLshort * v);
void(APIENTRY * cry_glReadBuffer)(GLenum mode);
void(APIENTRY * cry_glReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid * pixels);
void(APIENTRY * cry_glRectd)(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2);
void(APIENTRY * cry_glRectdv)(const GLdouble * v1, const GLdouble * v2);
void(APIENTRY * cry_glRectf)(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
void(APIENTRY * cry_glRectfv)(const GLfloat * v1, const GLfloat * v2);
void(APIENTRY * cry_glRecti)(GLint x1, GLint y1, GLint x2, GLint y2);
void(APIENTRY * cry_glRectiv)(const GLint * v1, const GLint * v2);
void(APIENTRY * cry_glRects)(GLshort x1, GLshort y1, GLshort x2, GLshort y2);
void(APIENTRY * cry_glRectsv)(const GLshort * v1, const GLshort * v2);
GLint(APIENTRY * cry_glRenderMode)(GLenum mode);
void(APIENTRY * cry_glRotated)(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
void(APIENTRY * cry_glRotatef)(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void(APIENTRY * cry_glScaled)(GLdouble x, GLdouble y, GLdouble z);
void(APIENTRY * cry_glScalef)(GLfloat x, GLfloat y, GLfloat z);
void(APIENTRY * cry_glScissor)(GLint x, GLint y, GLsizei width, GLsizei height);
void(APIENTRY * cry_glSelectBuffer)(GLsizei size, GLuint * buffer);
void(APIENTRY * cry_glShadeModel)(GLenum mode);
void(APIENTRY * cry_glStencilFunc)(GLenum func, GLint ref, GLuint mask);
void(APIENTRY * cry_glStencilMask)(GLuint mask);
void(APIENTRY * cry_glStencilOp)(GLenum fail, GLenum zfail, GLenum zpass);
void(APIENTRY * cry_glTexCoord1d)(GLdouble s);
void(APIENTRY * cry_glTexCoord1dv)(const GLdouble * v);
void(APIENTRY * cry_glTexCoord1f)(GLfloat s);
void(APIENTRY * cry_glTexCoord1fv)(const GLfloat * v);
void(APIENTRY * cry_glTexCoord1i)(GLint s);
void(APIENTRY * cry_glTexCoord1iv)(const GLint * v);
void(APIENTRY * cry_glTexCoord1s)(GLshort s);
void(APIENTRY * cry_glTexCoord1sv)(const GLshort * v);
void(APIENTRY * cry_glTexCoord2d)(GLdouble s, GLdouble t);
void(APIENTRY * cry_glTexCoord2dv)(const GLdouble * v);
void(APIENTRY * cry_glTexCoord2f)(GLfloat s, GLfloat t);
void(APIENTRY * cry_glTexCoord2fv)(const GLfloat * v);
void(APIENTRY * cry_glTexCoord2i)(GLint s, GLint t);
void(APIENTRY * cry_glTexCoord2iv)(const GLint * v);
void(APIENTRY * cry_glTexCoord2s)(GLshort s, GLshort t);
void(APIENTRY * cry_glTexCoord2sv)(const GLshort * v);
void(APIENTRY * cry_glTexCoord3d)(GLdouble s, GLdouble t, GLdouble r);
void(APIENTRY * cry_glTexCoord3dv)(const GLdouble * v);
void(APIENTRY * cry_glTexCoord3f)(GLfloat s, GLfloat t, GLfloat r);
void(APIENTRY * cry_glTexCoord3fv)(const GLfloat * v);
void(APIENTRY * cry_glTexCoord3i)(GLint s, GLint t, GLint r);
void(APIENTRY * cry_glTexCoord3iv)(const GLint * v);
void(APIENTRY * cry_glTexCoord3s)(GLshort s, GLshort t, GLshort r);
void(APIENTRY * cry_glTexCoord3sv)(const GLshort * v);
void(APIENTRY * cry_glTexCoord4d)(GLdouble s, GLdouble t, GLdouble r, GLdouble q);
void(APIENTRY * cry_glTexCoord4dv)(const GLdouble * v);
void(APIENTRY * cry_glTexCoord4f)(GLfloat s, GLfloat t, GLfloat r, GLfloat q);
void(APIENTRY * cry_glTexCoord4fv)(const GLfloat * v);
void(APIENTRY * cry_glTexCoord4i)(GLint s, GLint t, GLint r, GLint q);
void(APIENTRY * cry_glTexCoord4iv)(const GLint * v);
void(APIENTRY * cry_glTexCoord4s)(GLshort s, GLshort t, GLshort r, GLshort q);
void(APIENTRY * cry_glTexCoord4sv)(const GLshort * v);
void(APIENTRY * cry_glTexCoordPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer);
void(APIENTRY * cry_glTexEnvf)(GLenum target, GLenum pname, GLfloat param);
void(APIENTRY * cry_glTexEnvfv)(GLenum target, GLenum pname, const GLfloat * params);
void(APIENTRY * cry_glTexEnvi)(GLenum target, GLenum pname, GLint param);
void(APIENTRY * cry_glTexEnviv)(GLenum target, GLenum pname, const GLint * params);
void(APIENTRY * cry_glTexGend)(GLenum coord, GLenum pname, GLdouble param);
void(APIENTRY * cry_glTexGendv)(GLenum coord, GLenum pname, const GLdouble * params);
void(APIENTRY * cry_glTexGenf)(GLenum coord, GLenum pname, GLfloat param);
void(APIENTRY * cry_glTexGenfv)(GLenum coord, GLenum pname, const GLfloat * params);
void(APIENTRY * cry_glTexGeni)(GLenum coord, GLenum pname, GLint param);
void(APIENTRY * cry_glTexGeniv)(GLenum coord, GLenum pname, const GLint * params);
void(APIENTRY * cry_glTexImage1D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid * pixels);
void(APIENTRY * cry_glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid * pixels);
void(APIENTRY * cry_glTexParameterf)(GLenum target, GLenum pname, GLfloat param);
void(APIENTRY * cry_glTexParameterfv)(GLenum target, GLenum pname, const GLfloat * params);
void(APIENTRY * cry_glTexParameteri)(GLenum target, GLenum pname, GLint param);
void(APIENTRY * cry_glTexParameteriv)(GLenum target, GLenum pname, const GLint * params);
void(APIENTRY * cry_glTexSubImage1D)(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid * pixels);
void(APIENTRY * cry_glTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * pixels);
void(APIENTRY * cry_glTranslated)(GLdouble x, GLdouble y, GLdouble z);
void(APIENTRY * cry_glTranslatef)(GLfloat x, GLfloat y, GLfloat z);
void(APIENTRY * cry_glVertex2d)(GLdouble x, GLdouble y);
void(APIENTRY * cry_glVertex2dv)(const GLdouble * v);
void(APIENTRY * cry_glVertex2f)(GLfloat x, GLfloat y);
void(APIENTRY * cry_glVertex2fv)(const GLfloat * v);
void(APIENTRY * cry_glVertex2i)(GLint x, GLint y);
void(APIENTRY * cry_glVertex2iv)(const GLint * v);
void(APIENTRY * cry_glVertex2s)(GLshort x, GLshort y);
void(APIENTRY * cry_glVertex2sv)(const GLshort * v);
void(APIENTRY * cry_glVertex3d)(GLdouble x, GLdouble y, GLdouble z);
void(APIENTRY * cry_glVertex3dv)(const GLdouble * v);
void(APIENTRY * cry_glVertex3f)(GLfloat x, GLfloat y, GLfloat z);
void(APIENTRY * cry_glVertex3fv)(const GLfloat * v);
void(APIENTRY * cry_glVertex3i)(GLint x, GLint y, GLint z);
void(APIENTRY * cry_glVertex3iv)(const GLint * v);
void(APIENTRY * cry_glVertex3s)(GLshort x, GLshort y, GLshort z);
void(APIENTRY * cry_glVertex3sv)(const GLshort * v);
void(APIENTRY * cry_glVertex4d)(GLdouble x, GLdouble y, GLdouble z, GLdouble w);
void(APIENTRY * cry_glVertex4dv)(const GLdouble * v);
void(APIENTRY * cry_glVertex4f)(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void(APIENTRY * cry_glVertex4fv)(const GLfloat * v);
void(APIENTRY * cry_glVertex4i)(GLint x, GLint y, GLint z, GLint w);
void(APIENTRY * cry_glVertex4iv)(const GLint * v);
void(APIENTRY * cry_glVertex4s)(GLshort x, GLshort y, GLshort z, GLshort w);
void(APIENTRY * cry_glVertex4sv)(const GLshort * v);
void(APIENTRY * cry_glVertexPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer);
void(APIENTRY * cry_glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
};

using namespace cry_opengl;

	#define GET_OPENGL_FUNCTION(param)     *((INT_PTR*)&cry_ ## param) = (INT_PTR)CryGetProcAddress(hOpenGL, # param)
	#define GET_OPENGL_GLU_FUNCTION(param) *((INT_PTR*)&cry_ ## param) = (INT_PTR)CryGetProcAddress(hOpenGL_GLU, # param)

static bool LoadOpenGL()
{
	static HMODULE hOpenGL = CryLoadLibrary((string("opengl32") + CrySharedLibraryExtension).c_str());
	if (!hOpenGL)
		return false;

	#ifdef WIN32
	static HMODULE hOpenGL_GLU = GetModuleHandle("glu32.dll");
	if (!hOpenGL_GLU)
		return false;
	#endif //WIN32

	GET_OPENGL_FUNCTION(glDeleteLists);
	GET_OPENGL_FUNCTION(glGetIntegerv);
	GET_OPENGL_FUNCTION(glGetDoublev);
	GET_OPENGL_FUNCTION(glLoadIdentity);
	GET_OPENGL_FUNCTION(glMatrixMode);
	GET_OPENGL_FUNCTION(glViewport);
	GET_OPENGL_FUNCTION(glEnable);
	GET_OPENGL_FUNCTION(glPolygonMode);
	GET_OPENGL_FUNCTION(glShadeModel);
	GET_OPENGL_FUNCTION(glFlush);
	GET_OPENGL_FUNCTION(glCallLists);
	GET_OPENGL_FUNCTION(glRasterPos2f);
	GET_OPENGL_FUNCTION(glListBase);
	GET_OPENGL_FUNCTION(glPopMatrix);
	GET_OPENGL_FUNCTION(glTranslatef);
	GET_OPENGL_FUNCTION(glPushMatrix);
	GET_OPENGL_FUNCTION(glColor3fv);
	GET_OPENGL_FUNCTION(glDrawArrays);
	GET_OPENGL_FUNCTION(glInterleavedArrays);
	GET_OPENGL_FUNCTION(glColor3f);
	GET_OPENGL_FUNCTION(glClear);
	GET_OPENGL_FUNCTION(glClearColor);

	GET_OPENGL_FUNCTION(wglCreateContext);
	GET_OPENGL_FUNCTION(wglDeleteContext);
	GET_OPENGL_FUNCTION(wglMakeCurrent);
	//GET_OPENGL_FUNCTION( wglUseFontBitmaps );
	// Use non Unicode version
	*((INT_PTR*)&cry_wglUseFontBitmaps) = (INT_PTR)GetProcAddress(hOpenGL, "wglUseFontBitmapsA");

	GET_OPENGL_GLU_FUNCTION(gluPerspective);
	GET_OPENGL_GLU_FUNCTION(gluLookAt);
	GET_OPENGL_GLU_FUNCTION(gluProject);
	GET_OPENGL_GLU_FUNCTION(gluSphere);
	GET_OPENGL_GLU_FUNCTION(gluNewQuadric);
	GET_OPENGL_GLU_FUNCTION(gluDeleteQuadric);

	return true;
}

bool CNULLRenderAuxGeom::EnableOpenGL()
{
	if (!LoadOpenGL())
	{
		return false;
	}

	CCamera camera = gEnv->pSystem->GetViewCamera();
	camera.SetFrustum(static_cast<int>(W), static_cast<int>(H));

	const float FOV = camera.GetFov() / PI * 180.0f;
	const float PNR = camera.GetNearPlane();
	const float PFR = camera.GetFarPlane();

	gEnv->pSystem->SetViewCamera(camera);

	PIXELFORMATDESCRIPTOR pfd;
	int format;

	// get the device context (DC)
	m_hdc = GetDC(m_hwnd);

	// set the pixel format for the DC
	ZeroMemory(&pfd, sizeof(pfd));
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 16;
	pfd.iLayerType = PFD_MAIN_PLANE;
	format = ChoosePixelFormat(m_hdc, &pfd);
	SetPixelFormat(m_hdc, format, &pfd);

	// create and enable the render context (RC)
	m_glrc = cry_wglCreateContext(m_hdc);
	cry_wglMakeCurrent(m_hdc, m_glrc);

	cry_glShadeModel(GL_FLAT);
	cry_glPolygonMode(GL_FRONT, GL_FILL);
	cry_glEnable(GL_DEPTH_TEST);

	cry_glViewport(0, 0, static_cast<GLsizei>(W), static_cast<GLsizei>(H));
	cry_glMatrixMode(GL_PROJECTION);
	cry_glLoadIdentity();
	cry_gluPerspective(FOV, W / H, PNR, PFR);
	cry_glMatrixMode(GL_MODELVIEW);
	cry_glLoadIdentity();

	m_qobj = cry_gluNewQuadric();

	return true;
}

void CNULLRenderAuxGeom::DisableOpenGL()
{
	if (m_glLoaded)
	{
		cry_gluDeleteQuadric(m_qobj);
		cry_wglMakeCurrent(NULL, NULL);
		cry_wglDeleteContext(m_glrc);
	}
	ReleaseDC(m_hwnd, m_hdc);
}

#endif

CNULLRenderAuxGeom::CNULLRenderAuxGeom()
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	const char* wndClassName = "DebugRenderer";

	// register window class
	WNDCLASS wc;
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = CryGetCurrentModule();
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = wndClassName;
	RegisterClass(&wc);

	// create main window
	m_hwnd = CreateWindow(
	  wndClassName, wndClassName,
	  WS_CAPTION | WS_POPUP,
	  0, 0, static_cast<int>(W), static_cast<int>(H),
	  NULL, NULL, wc.hInstance, NULL);

	ShowWindow(m_hwnd, SW_HIDE);
	UpdateWindow(m_hwnd);

	m_glLoaded = EnableOpenGL();
	if (m_glLoaded)
	{
		m_eye.Set(0.0f, 0.0f, 0.0f);
		m_dir.Set(0.0f, 1.0f, 0.0f);
		m_up.Set(0.0f, 0.0f, 1.0f);
		m_updateSystemView = true;
	}

	// Register the commands regardless of gl initialization to let the player
	// use them and see the warning.
	REGISTER_COMMAND("r_debug_renderer_show_window", DebugRendererShowWindow, VF_NULL, "");
	REGISTER_COMMAND("r_debug_renderer_set_eye_pos", DebugRendererSetEyePos, VF_NULL, "");
	REGISTER_COMMAND("r_debug_renderer_update_system_view", DebugRendererUpdateSystemView, VF_NULL, "");
#endif
}

CNULLRenderAuxGeom::~CNULLRenderAuxGeom()
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	DisableOpenGL();
	DestroyWindow(m_hwnd);
#endif
}

void CNULLRenderAuxGeom::BeginFrame()
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);

	gEnv->pSystem->PumpWindowMessage(false);

	{
		m_dir.normalize();
		m_up.normalize();

		Vec3 right = m_dir ^ m_up;

		if (s_active)
		{
			Matrix34 m;

			m.SetIdentity();
			if (GetAsyncKeyState('W') & 0x8000)
				m.AddTranslation(m_dir);
			if (GetAsyncKeyState('S') & 0x8000)
				m.AddTranslation(-m_dir);
			if (GetAsyncKeyState('A') & 0x8000)
				m.AddTranslation(-right);
			if (GetAsyncKeyState('D') & 0x8000)
				m.AddTranslation(right);
			m_eye = m * m_eye;

			m.SetIdentity();
			if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
				m.SetRotationAA(-PI / 180.0f * THETA, VUP); // !m_up
			if (GetAsyncKeyState(VK_LEFT) & 0x8000)
				m.SetRotationAA(PI / 180.0f * THETA, VUP); // !m_up
			if (GetAsyncKeyState(VK_UP) & 0x8000)
				m.SetRotationAA(PI / 180.0f * THETA, right);
			if (GetAsyncKeyState(VK_DOWN) & 0x8000)
				m.SetRotationAA(-PI / 180.0f * THETA, right);
			m_up = m * m_up;
			m_dir = m * m_dir;
		}

		if (m_updateSystemView)
		{
			Matrix34 m(Matrix33::CreateOrientation(m_dir, m_up, 0), m_eye);
			CCamera cam = gEnv->pSystem->GetViewCamera();
			cam.SetMatrix(m);
			gEnv->pSystem->SetViewCamera(cam);
		}
		else
		{
			const Matrix34& viewMatrix = gEnv->pSystem->GetViewCamera().GetMatrix();
			m_eye = viewMatrix.GetTranslation();
			m_dir = viewMatrix.GetColumn1();
			m_up = viewMatrix.GetColumn2();
		}
	}

#endif
}

void CNULLRenderAuxGeom::EndFrame()
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);

	if (!s_hidden)
	{
		cry_glLoadIdentity();

		Vec3 at = m_eye + m_dir;
		cry_gluLookAt(m_eye.x, m_eye.y, m_eye.z, at.x, at.y, at.z, m_up.x, m_up.y, m_up.z);

		cry_glClearColor(0.0f, 0.0f, 0.3f, 0.0f);
		cry_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		cry_glColor3f(1.0f, 0.0f, 0.0f);
		cry_gluSphere(m_qobj, 1.0f, 32, 32);

		cry_glInterleavedArrays(GL_C3F_V3F, 0, &m_points[0]);
		cry_glDrawArrays(GL_POINTS, 0, m_points.size());

		cry_glInterleavedArrays(GL_C3F_V3F, 0, &m_lines[0]);
		cry_glDrawArrays(GL_LINES, 0, m_lines.size() * 2);

		for (size_t i = 0; i < m_polyLines.size(); ++i)
		{
			const SPolyLine& polyline = m_polyLines[i];
			cry_glInterleavedArrays(GL_C3F_V3F, 0, &polyline.points[0]);
			cry_glDrawArrays(GL_LINE_STRIP, 0, polyline.points.size());
		}

		cry_glInterleavedArrays(GL_C3F_V3F, 0, &m_triangles[0]);
		cry_glDrawArrays(GL_TRIANGLES, 0, m_triangles.size() * 3);

		for (size_t i = 0; i < m_spheres.size(); ++i)
		{
			cry_glColor3fv(m_spheres[i].p.color);
			cry_glPushMatrix();
			cry_glLoadIdentity();
			cry_glTranslatef(m_spheres[i].p.vertex[0], m_spheres[i].p.vertex[1], m_spheres[i].p.vertex[2]);
			cry_gluSphere(m_qobj, m_spheres[i].r, 32, 32);
			cry_glPopMatrix();
		}

		cry_glFlush();

		SwapBuffers(m_hdc);
	}

	m_points.resize(0);
	m_lines.resize(0);
	m_polyLines.resize(0);
	m_triangles.resize(0);
	m_spheres.resize(0);
#endif
}

void CNULLRenderAuxGeom::DrawPoint(const Vec3& v, const ColorB& col, uint8 size /* = 1  */)
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	m_points.push_back(SPoint(v, col));
#endif
}

void CNULLRenderAuxGeom::DrawPoints(const Vec3* v, uint32 numPoints, const ColorB* col, uint8 size /* = 1  */)
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	for (uint32 i = 0; i < numPoints; ++i)
		m_points.push_back(SPoint(v[i], *col));
#endif
}

void CNULLRenderAuxGeom::DrawPoints(const Vec3* v, uint32 numPoints, const ColorB& col, uint8 size /* = 1  */)
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	for (uint32 i = 0; i < numPoints; ++i)
		m_points.push_back(SPoint(v[i], col));
#endif
}

void CNULLRenderAuxGeom::DrawLine(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, float thickness /* = 1::0f  */)
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	m_lines.push_back(SLine(SPoint(v0, colV0), SPoint(v1, colV1)));
#endif
}

void CNULLRenderAuxGeom::DrawLines(const Vec3* v, uint32 numPoints, const ColorB& col, float thickness /* = 1::0f  */)
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	assert((numPoints >= 2) && !(numPoints & 1));
	for (uint32 i = 0; i < numPoints; i += 2)
		m_lines.push_back(SLine(SPoint(v[i], col), SPoint(v[i + 1], col)));
#endif
}

void CNULLRenderAuxGeom::DrawLines(const Vec3* v, uint32 numPoints, const ColorB* col, float thickness /* = 1::0f  */)
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	assert((numPoints >= 2) && !(numPoints & 1));
	for (uint32 i = 0; i < numPoints; i += 2)
		m_lines.push_back(SLine(SPoint(v[i], *col), SPoint(v[i + 1], *col)));
#endif
}

void CNULLRenderAuxGeom::DrawLines(const Vec3* v, const uint32* packedColorARGB8888, uint32 numPoints, float thickness, bool alphaFlag)
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	assert((numPoints >= 2) && !(numPoints & 1));
	for (uint32 i = 0; i < numPoints; i += 2)
		m_lines.push_back(SLine(SPoint(v[i], packedColorARGB8888[i]), SPoint(v[i + 1], packedColorARGB8888[i + 1])));
#endif
}

void CNULLRenderAuxGeom::DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col, float thickness /* = 1::0f  */)
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	assert(numPoints >= 2);
	assert((numIndices >= 2) && !(numIndices & 1));
	for (uint32 i = 0; i < numIndices; i += 2)
	{
		vtx_idx i0 = ind[i], i1 = ind[i + 1];
		assert((i0 < numPoints) && (i1 < numPoints));
		m_lines.push_back(SLine(SPoint(v[i0], col), SPoint(v[i1], col)));
	}
#endif
}

void CNULLRenderAuxGeom::DrawLineStrip(const Vec3* v, uint32 numPoints, const ColorB* col, float thickness /* = 1.0f*/)
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	assert(false && "ToDo: Not implemented.");
#endif
}

void CNULLRenderAuxGeom::DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col, float thickness /* = 1::0f  */)
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	assert(numPoints >= 2);
	assert((numIndices >= 2) && !(numIndices & 1));
	for (uint32 i = 0; i < numIndices; i += 2)
	{
		vtx_idx i0 = ind[i], i1 = ind[i + 1];
		assert((i0 < numPoints) && (i1 < numPoints));
		m_lines.push_back(SLine(SPoint(v[i0], *col), SPoint(v[i1], *col)));
	}
#endif
}

void CNULLRenderAuxGeom::DrawPolyline(const Vec3* v, uint32 numPoints, bool closed, const ColorB& col, float thickness /* = 1::0f  */)
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	assert(numPoints >= 2);
	assert(!closed || (numPoints >= 3));   // if "closed" then we need at least three vertices
	m_polyLines.resize(m_polyLines.size() + 1);
	SPolyLine& polyline = m_polyLines[m_polyLines.size() - 1];
	for (uint32 i = 0; i < numPoints; ++i)
		polyline.points.push_back(SPoint(v[i], col));
	if (closed)
		polyline.points.push_back(SPoint(v[0], col));
#endif
}

void CNULLRenderAuxGeom::DrawPolyline(const Vec3* v, uint32 numPoints, bool closed, const ColorB* col, float thickness /* = 1::0f  */)
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	assert(numPoints >= 2);
	assert(!closed || (numPoints >= 3));   // if "closed" then we need at least three vertices
	m_polyLines.resize(m_polyLines.size() + 1);
	SPolyLine& polyline = m_polyLines[m_polyLines.size() - 1];
	for (uint32 i = 0; i < numPoints; ++i)
		polyline.points.push_back(SPoint(v[i], *col));
	if (closed)
		polyline.points.push_back(SPoint(v[0], *col));
#endif
}

void CNULLRenderAuxGeom::DrawTriangle(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, const Vec3& v2, const ColorB& colV2)
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
	m_triangles.push_back(STriangle(SPoint(v0, colV0), SPoint(v1, colV1), SPoint(v2, colV2)));
#endif
}

void CNULLRenderAuxGeom::DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB& col)
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	assert((numPoints >= 3) && !(numPoints % 3));
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
	for (size_t i = 0; i < numPoints; i += 3)
		m_triangles.push_back(STriangle(SPoint(v[i], col), SPoint(v[i + 1], col), SPoint(v[i + 2], col)));
#endif
}

void CNULLRenderAuxGeom::DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB* col)
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	assert((numPoints >= 3) && !(numPoints % 3));
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
	for (size_t i = 0; i < numPoints; i += 3)
		m_triangles.push_back(STriangle(SPoint(v[i], *col), SPoint(v[i + 1], *col), SPoint(v[i + 2], *col)));
#endif
}

void CNULLRenderAuxGeom::DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col)
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	assert(numPoints >= 3);
	assert((numIndices >= 3) && !(numIndices % 3));
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
	for (size_t i = 0; i < numIndices; i += 3)
	{
		vtx_idx i0 = ind[i], i1 = ind[i + 1], i2 = ind[i + 2];
		assert((i0 < numPoints) && (i1 < numPoints) && (i2 < numPoints));
		m_triangles.push_back(STriangle(SPoint(v[i0], col), SPoint(v[i1], col), SPoint(v[i2], col)));
	}
#endif
}

void CNULLRenderAuxGeom::DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col)
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	assert(numPoints >= 3);
	assert((numIndices >= 3) && !(numIndices % 3));
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
	for (size_t i = 0; i < numIndices; i += 3)
	{
		vtx_idx i0 = ind[i], i1 = ind[i + 1], i2 = ind[i + 2];
		assert((i0 < numPoints) && (i1 < numPoints) && (i2 < numPoints));
		m_triangles.push_back(STriangle(SPoint(v[i0], *col), SPoint(v[i1], *col), SPoint(v[i2], *col)));
	}
#endif
}

void CNULLRenderAuxGeom::DrawSphere(const Vec3& pos, float radius, const ColorB& col, bool drawShaded)
{
#ifdef ENABLE_WGL_DEBUG_RENDERER
	m_spheres.push_back(SSphere(SPoint(pos, col), radius));
#endif
}

void CNULLRenderAuxGeom::PushImage(const SRender2DImageDescription &image)
{

}