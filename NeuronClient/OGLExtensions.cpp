#include "pch.h"
#include "Debug.h"
#include "OGLExtensions.h"

MultiTexCoord2fARB gglMultiTexCoord2fARB = nullptr;
ActiveTextureARB gglActiveTextureARB = nullptr;

glBindBufferARB gglBindBufferARB = nullptr;
glDeleteBuffersARB gglDeleteBuffersARB = nullptr;
glGenBuffersARB gglGenBuffersARB = nullptr;
glIsBufferARB gglIsBufferARB = nullptr;
glBufferDataARB gglBufferDataARB = nullptr;
glBufferSubDataARB gglBufferSubDataARB = nullptr;
glGetBufferSubDataARB gglGetBufferSubDataARB = nullptr;
glMapBufferARB gglMapBufferARB = nullptr;
glUnmapBufferARB gglUnmapBufferARB = nullptr;
glGetBufferParameterivARB gglGetBufferParameterivARB = nullptr;
glGetBufferPointervARB gglGetBufferPointervARB = nullptr;

ChoosePixelFormatARB gglChoosePixelFormatARB = nullptr;

void InitialiseOGLExtensions()
{
  gglMultiTexCoord2fARB = (MultiTexCoord2fARB)wglGetProcAddress("glMultiTexCoord2fARB");
  gglActiveTextureARB = (ActiveTextureARB)wglGetProcAddress("glActiveTextureARB");

  gglBindBufferARB = (glBindBufferARB)wglGetProcAddress("glBindBufferARB");
  gglDeleteBuffersARB = (glDeleteBuffersARB)wglGetProcAddress("glDeleteBuffersARB");
  gglGenBuffersARB = (glGenBuffersARB)wglGetProcAddress("glGenBuffersARB");
  gglIsBufferARB = (glIsBufferARB)wglGetProcAddress("glIsBufferARB");
  gglBufferDataARB = (glBufferDataARB)wglGetProcAddress("glBufferDataARB");
  gglBufferSubDataARB = (glBufferSubDataARB)wglGetProcAddress("glBufferSubDataARB");
  gglGetBufferSubDataARB = (glGetBufferSubDataARB)wglGetProcAddress("glGetBufferSubDataARB");
  gglMapBufferARB = (glMapBufferARB)wglGetProcAddress("glMapBufferARB");
  gglUnmapBufferARB = (glUnmapBufferARB)wglGetProcAddress("glUnmapBufferARB");
  gglGetBufferParameterivARB = (glGetBufferParameterivARB)wglGetProcAddress("glGetBufferParameterivARB");
  gglGetBufferPointervARB = (glGetBufferPointervARB)wglGetProcAddress("glGetBufferPointervARB");

  gglChoosePixelFormatARB = (ChoosePixelFormatARB)wglGetProcAddress("wglChoosePixelFormatARB");
}

int IsOGLExtensionSupported(const char* extension)
{
  // From http://www.opengl.org/resources/features/OGLextensions/
  const GLubyte* extensions = nullptr;
  const GLubyte* start;
  GLubyte *where, *terminator;

  /* Extension names should not have spaces. */
  where = (GLubyte*)strchr(extension, ' ');
  if (where || *extension == '\0')
    return 0;
  extensions = glGetString(GL_EXTENSIONS);
  /* It takes a bit of care to be fool-proof about parsing the
     OpenGL extensions string. Don't be fooled by sub-strings,
     etc. */
  start = extensions;
  for (;;)
  {
    where = (GLubyte*)strstr((const char*)start, extension);
    if (!where)
      break;
    terminator = where + strlen(extension);
    if (where == start || *(where - 1) == ' ')
    {
      if (*terminator == ' ' || *terminator == '\0')
        return 1;
    }
    start = terminator;
  }
  return 0;
}
