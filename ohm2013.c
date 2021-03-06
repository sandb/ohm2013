#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <GLES2/gl2.h>  /* use OpenGL ES 2.x */
#include <EGL/egl.h>
#include <time.h>

#define FLOAT_TO_FIXED(X)   ((X) * 65535.0)

#define N_CUBES 50
#define N_CUBES_FAR 100

static GLfloat view_rotx = 0.0, view_roty = 0.0, view_rotz = 0.0, view_transz = 2.0, view_scale = 1.0;

static GLint u_matrix = -1;
static GLint u_projection = -1;

static GLint attr_pos = 0, attr_color = 1;

struct cubeset {
    GLfloat x;
    GLfloat y;
    GLfloat z;
    GLfloat rx;
    GLfloat ry;
    GLfloat rz;
};

struct cube {
    struct cubeset c, delta;
};

static void
wait_sleep() {
   struct timespec ts;
   /* Delay for a bit */
   ts.tv_sec = 0;
   ts.tv_nsec = 1000;
   nanosleep (&ts, NULL);
}

static void
matrix_multiply(GLfloat *prod, const GLfloat *a, const GLfloat *b)
{
#define A(row,col)  a[(col<<2)+row]
#define B(row,col)  b[(col<<2)+row]
#define P(row,col)  p[(col<<2)+row]
   GLfloat p[16];
   GLint i;
   for (i = 0; i < 4; i++) {
      const GLfloat ai0=A(i,0),  ai1=A(i,1),  ai2=A(i,2),  ai3=A(i,3);
      P(i,0) = ai0 * B(0,0) + ai1 * B(1,0) + ai2 * B(2,0) + ai3 * B(3,0);
      P(i,1) = ai0 * B(0,1) + ai1 * B(1,1) + ai2 * B(2,1) + ai3 * B(3,1);
      P(i,2) = ai0 * B(0,2) + ai1 * B(1,2) + ai2 * B(2,2) + ai3 * B(3,2);
      P(i,3) = ai0 * B(0,3) + ai1 * B(1,3) + ai2 * B(2,3) + ai3 * B(3,3);
   }
   memcpy(prod, p, sizeof(p));
#undef A
#undef B
#undef PROD
}

void
matrix_make_unity(GLfloat *m)
{
   int i;
   for (i = 0; i < 16; i++)
      m[i] = 0.0;
   m[0] = m[5] = m[10] = m[15] = 1.0;
}

void
matrix_rotate_x(GLfloat angle, GLfloat *mat)
{
   GLfloat m[16];
   float c = cos(angle * M_PI / 180.0);
   float s = sin(angle * M_PI / 180.0);
   int i;
   for (i = 0; i < 16; i++)
      m[i] = 0.0;
   m[0] = m[5] = m[10] = m[15] = 1.0;

   m[5] = c;                
   m[6] = s;
   m[9] = -s;
   m[10] = c;
   matrix_multiply(mat, m, mat);
}

void
matrix_rotate_y(GLfloat angle, GLfloat *mat)
{
   GLfloat m[16];
   float c = cos(angle * M_PI / 180.0);
   float s = sin(angle * M_PI / 180.0);
   int i;
   for (i = 0; i < 16; i++)
      m[i] = 0.0;
   m[0] = m[5] = m[10] = m[15] = 1.0;

   m[0] = c;                
   m[2] = -s;
   m[8] = s;
   m[10] = c;
   matrix_multiply(mat, m, mat);
}

static void
matrix_rotate_z(GLfloat angle, GLfloat *mat)
{
   GLfloat m[16];
   float c = cos(angle * M_PI / 180.0);
   float s = sin(angle * M_PI / 180.0);
   int i;
   for (i = 0; i < 16; i++)
      m[i] = 0.0;
   m[0] = m[5] = m[10] = m[15] = 1.0;

   m[0] = c;
   m[1] = s;
   m[4] = -s;
   m[5] = c;
   matrix_multiply(mat, m, mat);
}

static void
matrix_translate(GLfloat x, GLfloat y, GLfloat z, GLfloat *mat)
{
   GLfloat m[16];
   int i;
   for (i = 0; i < 16; i++)
      m[i] = 0.0;
   m[0] = m[5] = m[10] = m[15] = 1.0;
   m[12] = x;
   m[13] = y;
   m[14] = z;
   matrix_multiply(mat, m, mat);
}

static void
matrix_scale(GLfloat xs, GLfloat ys, GLfloat zs, GLfloat *mat)
{
   GLfloat m[16];
   int i;
   for (i = 0; i < 16; i++)
      m[i] = 0.0;
   m[0] = xs;
   m[5] = ys;
   m[10] = zs;
   m[15] = 1.0;
   matrix_multiply(mat, m, mat);
}


static void
matrix_make_projection(GLfloat focal_distance, GLfloat *m)
{
   matrix_make_unity(m);
   m[11] = 1.0/focal_distance;
   m[15] = 0.0;
}

void
print_matrix(GLfloat *m) 
{
    printf("{\n");
    int i = 0;
    for (i = 0; i < 4; i++) {
        printf(" [%f,%f,%f,%f]\n", m[i*4+0], m[i*4+1], m[i*4+2], m[i*4+3]);
    }
    printf("}\n");
}

#define N 14

void
start_cube() 
{
   #define A {  1, -1,  1 }
   #define B {  1, -1, -1 }
   #define C { -1, -1,  1 }
   #define D { -1, -1, -1 }
   #define E { -1,  1, -1 }
   #define F {  1,  1, -1 }
   #define G {  1,  1,  1 }
   #define H { -1,  1,  1 }
   #define c { 0.0, 0.0, 0.0 }
   #define Rc { 1.0, 0.0, 0.0 }
   #define Gc { 0.0, 1.0, 0.0 }
   #define Bc { 0.0, 0.0, 1.0 }
   #define RGc { 1.0, 1.0, 0.0 }
   #define RBc { 1.0, 0.0, 1.0 }
   #define GBc { 0.0, 1.0, 1.0 }
   #define RGBc { 1.0, 1.0, 1.0 }
   static const GLfloat verts[N][3] = {
      A, B, C, D, E, B, F, G, E, H, C, G, A, B
   };
   static const GLfloat colors[N][3] = {
      c, Rc, Bc, RBc, RGc, Rc, Gc, GBc, RGc, RGBc, Bc, GBc, c, Rc
   };

   #undef A
   #undef B
   #undef C
   #undef D
   #undef E
   #undef F
   #undef G
   #undef H
   #undef c
   #undef Rc
   #undef Gc
   #undef Bc
   #undef RGc
   #undef RBc
   #undef GBc
   #undef RGBc
   glVertexAttribPointer(attr_pos, 3, GL_FLOAT, GL_FALSE, 0, verts);
   glVertexAttribPointer(attr_color, 3, GL_FLOAT, GL_FALSE, 0, colors);

   glEnableVertexAttribArray(attr_pos);
   glEnableVertexAttribArray(attr_color);
}

void
draw_cube(GLfloat x, GLfloat y, GLfloat z, GLfloat rx, GLfloat ry, GLfloat rz, GLfloat scale, GLfloat *m) 
{
   GLfloat mat[16];
   memcpy(mat, m, sizeof(mat));

   matrix_scale(scale, scale, scale, mat);
   matrix_translate(x, y, z, mat);
   matrix_rotate_z(rz, mat);
   matrix_rotate_y(ry, mat);
   matrix_rotate_x(rx, mat);

   glUniformMatrix4fv(u_matrix, 1, GL_FALSE, mat);
   
   glDrawArrays(GL_TRIANGLE_STRIP, 0, N);
}

void 
end_cube() 
{
   glDisableVertexAttribArray(attr_pos);
   glDisableVertexAttribArray(attr_color);
}

static void 
init_cube(struct cube *c, GLfloat minz, GLfloat maxz) {
   GLfloat rm = RAND_MAX;
   c->c.x = (random() / rm) * (20) - 10.0;
   c->c.y = (random() / rm) * (20) - 10.0;
   c->c.z = (random() / rm) * (maxz - minz) + minz;
   c->c.rx = (random() / rm) * (2 * M_PI);
   c->c.ry = (random() / rm) * (2 * M_PI);
   c->c.rz = (random() / rm) * (2 * M_PI);
   c->delta.x = (random() / rm) * (0.2) - 0.1;
   c->delta.y = (random() / rm) * (0.2) - 0.1;
   c->delta.z = -(random() / rm) * (0.5) + 0.5;
   c->delta.rx = (random() / rm) * (0.2) - 0.1;
   c->delta.ry = (random() / rm) * (0.2) - 0.1;
   c->delta.rz = (random() / rm) * (0.2) - 0.1;
}

static void
update_cube(struct cube *c) {
   c->c.x += c->delta.x;
   c->c.y += c->delta.y;
   c->c.z += c->delta.z;
   c->c.rx += c->delta.rx;
   c->c.ry += c->delta.ry;
   c->c.rz += c->delta.rz;
   if (c->c.z > 50.0) {
      init_cube(c, 3.0, N_CUBES_FAR);
   }
}

static void
draw_a_cube(struct cube *c, GLfloat *mat) {
   draw_cube(c->c.x, c->c.y, c->c.z, c->c.rx, c->c.ry, c->c.rz, 1.0, mat);
}

static void
print_cube(struct cube *c) {
   struct cubeset *cs;
   cs = &c->c;
   printf("c:[x: %f, y:%f, z:%f, rx:%f, ry:%f, rz:%f]\n", cs->x, cs->y, cs->z, cs->rx, cs->ry, cs->rz);
   cs = &c->delta;
   printf("c:[x: %f, y:%f, z:%f, rx:%f, ry:%f, rz:%f]\n", cs->x, cs->y, cs->z, cs->rx, cs->ry, cs->rz);
}

struct cube cubes[N_CUBES];

static void
draw(void)
{
   GLfloat mat[16], projection[16];

   struct cube c;
   /* Set modelview/projection matrix */
   matrix_make_unity(mat);
   matrix_rotate_x(view_rotx, mat);
   matrix_rotate_y(view_roty, mat);
   matrix_rotate_z(view_rotz, mat);
   matrix_translate(0.0, 0.0, view_transz, mat);
   matrix_scale(view_scale, view_scale, view_scale, mat);

   matrix_make_projection(0.9, projection);

   //print_matrix(mat);

   glUniformMatrix4fv(u_projection, 1, GL_FALSE, projection);

   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   start_cube();

   int a;
   for (a = 0; a < N_CUBES; a++) {
//      print_cube(&cubes[a]);
      update_cube(&cubes[a]);
      draw_a_cube(&cubes[a], mat);
      printf("a=%d\n",a);
      print_cube(&cubes[a]);
   }
   /*
   GLfloat i,j,k;
   #define LOW -4.0
   #define HIGH 4.0
   #define STEP 2.0
   for (i = LOW; i < HIGH; i += STEP) {
       for (j = LOW; j < HIGH; j += STEP) {
          for (k = HIGH+10.0; k > LOW+10.0; k -= STEP) {
             draw_cube(i, j, k, 0.0, 0.0, 0.0, 1.0, mat);
          }
       }
   }
   #undef LOW
   #undef HIGH
   #undef STEP
   */
   end_cube();
   #undef N
}


/* new window size or exposure */
static void
reshape(int width, int height)
{
   glViewport(0, 0, (GLint) width, (GLint) height);
}


static void
create_shaders(void)
{
   static const char *fragShaderText =
      "varying vec4 v_color;\n"
      "varying vec4 position;\n"
      "void main() {\n"
      "   if( position.z > 0.5 && position.z/position.w > 1.0 ){\n"
      "       gl_FragColor = v_color;\n"
      "   }else{discard;}\n"
      "}\n";
   static const char *vertShaderText =
      "uniform mat4 modelviewProjection;\n"
      "uniform mat4 projectionMatrix;\n"
      "attribute vec4 pos;\n"
      "attribute vec4 color;\n"
      "varying vec4 v_color;\n"
      "varying vec4 position;\n"
      "void main() {\n"
      "   vec4 p = modelviewProjection * pos;\n"
      "   vec4 p2 = projectionMatrix * p;\n"
      "   gl_Position = p2;\n"
      "   v_color = color;\n"
      "   position = p;\n"
      "}\n";

   GLuint fragShader, vertShader, program;
   GLint stat;

   fragShader = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(fragShader, 1, (const char **) &fragShaderText, NULL);
   glCompileShader(fragShader);
   glGetShaderiv(fragShader, GL_COMPILE_STATUS, &stat);
   if (!stat) {
      printf("Error: fragment shader did not compile!\n");
      exit(1);
   }

   vertShader = glCreateShader(GL_VERTEX_SHADER);
   glShaderSource(vertShader, 1, (const char **) &vertShaderText, NULL);
   glCompileShader(vertShader);
   glGetShaderiv(vertShader, GL_COMPILE_STATUS, &stat);
   if (!stat) {
      printf("Error: vertex shader did not compile!\n");
      exit(1);
   }

   program = glCreateProgram();
   glAttachShader(program, fragShader);
   glAttachShader(program, vertShader);
   glLinkProgram(program);

   glGetProgramiv(program, GL_LINK_STATUS, &stat);
   if (!stat) {
      char log[1000];
      GLsizei len;
      glGetProgramInfoLog(program, 1000, &len, log);
      printf("Error: linking:\n%s\n", log);
      exit(1);
   }

   glUseProgram(program);

   if (1) {
      /* test setting attrib locations */
      glBindAttribLocation(program, attr_pos, "pos");
      glBindAttribLocation(program, attr_color, "color");
      glLinkProgram(program);  /* needed to put attribs into effect */
   }
   else {
      /* test automatic attrib locations */
      attr_pos = glGetAttribLocation(program, "pos");
      attr_color = glGetAttribLocation(program, "color");
   }

   u_matrix = glGetUniformLocation(program, "modelviewProjection");
   u_projection = glGetUniformLocation(program, "projectionMatrix");
   printf("Uniform modelviewProjection at %d\n", u_matrix);
   printf("Attrib pos at %d\n", attr_pos);
   printf("Attrib color at %d\n", attr_color);
}


static void
init(void)
{
   typedef void (*proc)();

#if 1 /* test code */
   proc p = eglGetProcAddress("glMapBufferOES");
   assert(p);
#endif

   glClearColor(0.0, 0.0, 0.0, 0.0);
//   glClearDepth(1.0f);
   create_shaders();
}


/*
 * Create an RGB, double-buffered X window.
 * Return the window and context handles.
 */
static void
make_x_window(Display *x_dpy, EGLDisplay egl_dpy,
              const char *name,
              int x, int y, int width, int height,
              Window *winRet,
              EGLContext *ctxRet,
              EGLSurface *surfRet)
{
   static const EGLint attribs[] = {
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_DEPTH_SIZE, 24,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_NONE
   };
   static const EGLint ctx_attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
   };

   int scrnum;
   XSetWindowAttributes attr;
   unsigned long mask;
   Window root;
   Window win;
   XVisualInfo *visInfo, visTemplate;
   int num_visuals;
   EGLContext ctx;
   EGLConfig config;
   EGLint num_configs;
   EGLint vid;

   scrnum = DefaultScreen( x_dpy );
   root = RootWindow( x_dpy, scrnum );

   if (!eglChooseConfig( egl_dpy, attribs, &config, 1, &num_configs)) {
      printf("Error: couldn't get an EGL visual config\n");
      exit(1);
   }
 
   assert(config);
   assert(num_configs > 0);

   if (!eglGetConfigAttrib(egl_dpy, config, EGL_NATIVE_VISUAL_ID, &vid)) {
      printf("Error: eglGetConfigAttrib() failed\n");
      exit(1);
   }

   /* The X window visual must match the EGL config */
   visTemplate.visualid = vid;
   visInfo = XGetVisualInfo(x_dpy, VisualIDMask, &visTemplate, &num_visuals);
   if (!visInfo) {
      printf("Error: couldn't get X visual\n");
      exit(1);
   }

   /* window attributes */
   attr.background_pixel = 0;
   attr.border_pixel = 0;
   attr.colormap = XCreateColormap( x_dpy, root, visInfo->visual, AllocNone);
   attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
   mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

   win = XCreateWindow( x_dpy, root, 0, 0, width, height,
		        0, visInfo->depth, InputOutput,
		        visInfo->visual, mask, &attr );

   /* set hints and properties */
   {
      XSizeHints sizehints;
      sizehints.x = x;
      sizehints.y = y;
      sizehints.width  = width;
      sizehints.height = height;
      sizehints.flags = USSize | USPosition;
      XSetNormalHints(x_dpy, win, &sizehints);
      XSetStandardProperties(x_dpy, win, name, name,
                              None, (char **)NULL, 0, &sizehints);
   }

   eglBindAPI(EGL_OPENGL_ES_API);

   ctx = eglCreateContext(egl_dpy, config, EGL_NO_CONTEXT, ctx_attribs );
   if (!ctx) {
      printf("Error: eglCreateContext failed\n");
      exit(1);
   }

   /* test eglQueryContext() */
   {
      EGLint val;
      eglQueryContext(egl_dpy, ctx, EGL_CONTEXT_CLIENT_VERSION, &val);
      assert(val == 2);
   }

   *surfRet = eglCreateWindowSurface(egl_dpy, config, win, NULL);
   if (!*surfRet) {
      printf("Error: eglCreateWindowSurface failed\n");
      exit(1);
   }

   /* sanity checks */
   {
      EGLint val;
      eglQuerySurface(egl_dpy, *surfRet, EGL_WIDTH, &val);
      assert(val == width);
      eglQuerySurface(egl_dpy, *surfRet, EGL_HEIGHT, &val);
      assert(val == height);
      assert(eglGetConfigAttrib(egl_dpy, config, EGL_SURFACE_TYPE, &val));
      assert(val & EGL_WINDOW_BIT);
   }

   XFree(visInfo);

   *winRet = win;
   *ctxRet = ctx;
}


static int
handle_input(XEvent *event) {
   int state = 0;
   switch (event->type) {
   case Expose:
      state = 1;
      break;
   case ConfigureNotify:
      reshape(event->xconfigure.width, event->xconfigure.height);
      break;
   case KeyPress:
      {
         char buffer[10];
         int r, code;
         code = XLookupKeysym(&event->xkey, 0);
         if (code == XK_Left) {
            view_roty += 5.0;
         }
         else if (code == XK_Right) {
            view_roty -= 5.0;
         }
         else if (code == XK_Up) {
            view_rotx += 5.0;
         }
         else if (code == XK_Down) {
            view_rotx -= 5.0;
         }
         else if (code == XK_Page_Up) {
            view_rotz += 5.0;
         } 
         else if (code == XK_Page_Down) {
            view_rotz -= 5.0;
         }
         else if (code == XK_End) {
            view_transz -= 0.1;
         } 
         else if (code == XK_Home) {
            view_transz += 0.1;
         }
         else if (code == XK_F1) {
            view_scale /= 2.0;
         } 
         else if (code == XK_F2) {
            view_scale *= 2.0;
         }
         else {
            r = XLookupString(&event->xkey, buffer, sizeof(buffer),
                              NULL, NULL);
            if (buffer[0] == 27) {
               /* escape */
               return 2;
            }
            if (buffer[0] == 32) {
               return 3;
            }
         }
      }
      state = 1;
      break;
   default:
      ; /*no-op*/
   }
   return state;
}

static void
event_loop(Display *dpy, Window win,
           EGLDisplay egl_dpy, EGLSurface egl_surf)
{
      int redraw = 0;
 while (1) {
      XEvent event;
      if (XPending(dpy) > 0) {
         XNextEvent(dpy, &event);
         switch(handle_input(&event)) {
            case 1:
                redraw = 1;
                break;
            case 2:
                return;
            case 3:
               redraw = 3;
               break;
            default:
                break;
         }
      }

      if (!redraw) {
         wait_sleep();
      }
      if (redraw == 1) {
         view_rotx += 0.1;
         draw();
      }
      eglSwapBuffers(egl_dpy, egl_surf);
   }
}


static void
usage(void)
{
   printf("Usage:\n");
   printf("  -display <displayname>  set the display to run on\n");
   printf("  -info                   display OpenGL renderer info\n");
}


int
main(int argc, char *argv[])
{
   const int winWidth = 300, winHeight = 300;
   Display *x_dpy;
   Window win;
   EGLSurface egl_surf;
   EGLContext egl_ctx;
   EGLDisplay egl_dpy;
   char *dpyName = NULL;
   GLboolean printInfo = GL_FALSE;
   EGLint egl_major, egl_minor;
   int i;
   const char *s;

   for (i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-display") == 0) {
         dpyName = argv[i+1];
         i++;
      }
      else if (strcmp(argv[i], "-info") == 0) {
         printInfo = GL_TRUE;
      }
      else {
         usage();
         return -1;
      }
   }

   x_dpy = XOpenDisplay(dpyName);
   if (!x_dpy) {
      printf("Error: couldn't open display %s\n",
	     dpyName ? dpyName : getenv("DISPLAY"));
      return -1;
   }

   egl_dpy = eglGetDisplay(x_dpy);
   if (!egl_dpy) {
      printf("Error: eglGetDisplay() failed\n");
      return -1;
   }

   if (!eglInitialize(egl_dpy, &egl_major, &egl_minor)) {
      printf("Error: eglInitialize() failed\n");
      return -1;
   }

   s = eglQueryString(egl_dpy, EGL_VERSION);
   printf("EGL_VERSION = %s\n", s);

   s = eglQueryString(egl_dpy, EGL_VENDOR);
   printf("EGL_VENDOR = %s\n", s);

   s = eglQueryString(egl_dpy, EGL_EXTENSIONS);
   printf("EGL_EXTENSIONS = %s\n", s);

   s = eglQueryString(egl_dpy, EGL_CLIENT_APIS);
   printf("EGL_CLIENT_APIS = %s\n", s);

   make_x_window(x_dpy, egl_dpy,
                 "OpenGL ES 2.x tri", 0, 0, winWidth, winHeight,
                 &win, &egl_ctx, &egl_surf);

   XMapWindow(x_dpy, win);
   if (!eglMakeCurrent(egl_dpy, egl_surf, egl_surf, egl_ctx)) {
      printf("Error: eglMakeCurrent() failed\n");
      return -1;
   }

   if (printInfo) {
      printf("GL_RENDERER   = %s\n", (char *) glGetString(GL_RENDERER));
      printf("GL_VERSION    = %s\n", (char *) glGetString(GL_VERSION));
      printf("GL_VENDOR     = %s\n", (char *) glGetString(GL_VENDOR));
      printf("GL_EXTENSIONS = %s\n", (char *) glGetString(GL_EXTENSIONS));
   }

   init();
   glEnable(GL_CULL_FACE);
   glEnable(GL_DEPTH_TEST);
   glDepthMask(GL_TRUE);
   glDepthFunc(GL_LEQUAL);
   glDepthRangef(0.2f, 0.99f);
   glClearDepthf(0.999f);

   for (i = 0; i < N_CUBES; i++) {
      print_cube(&cubes[i]);
      init_cube(&cubes[i], 3.0, N_CUBES_FAR);
      print_cube(&cubes[i]);
   }

   /* Set initial projection/viewing transformation.
    * We can't be sure we'll get a ConfigureNotify event when the window
    * first appears.
    */
   reshape(winWidth, winHeight);

   event_loop(x_dpy, win, egl_dpy, egl_surf);

   eglDestroyContext(egl_dpy, egl_ctx);
   eglDestroySurface(egl_dpy, egl_surf);
   eglTerminate(egl_dpy);


   XDestroyWindow(x_dpy, win);
   XCloseDisplay(x_dpy);

   return 0;
}
