/*
*  Textures
*
*  Draws 3 Barrells to demonstrate orthogonal & prespective projections
*  Throws in some lighting and sweet bowling_pin textures as well
*
*  Key bindings:
*  m          Toggle between perspective and orthogonal
*  +/-        Changes field of view for perspective
*  a          Toggle axes
*  arrows     Change view angle
*  PgDn/PgUp  Zoom in and out
*  0          Reset view angle
*  ESC        Exit
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "CSCIx229.h"
#include "bowling.h"

//  OpenGL with prototypes for glext
#define GL_GLxOffsetT_PROTOTYPES
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

//Azimuth
int th=0;
//Elevation
int ph=0;

//Projection variables
int mode = 1;
int fov=55;
double asp=1;
double dim=50.0;

//first person location
double xOffset = 0;
double yOffset = 0;
double zOffset = -60;


int axes=0;       //  Display axes
double idle = 0.0;
int window_width = 600;
int window_height = 600;
int left_click_down = 0;
int saved_th = 0;
int saved_ph = 0;
int light = 1;
int camera = 1;
int move = 1;       //  Move light
int one       =   1;  // Unit value
int distance  =   5;  // Light distance
int inc       =  10;  // Ball increment
int smooth    =   1;  // Smooth/Flat shading
int local     =   0;  // Local Viewer Model

unsigned int side = 0;// side texture
unsigned int end = 0; // end texture
unsigned int floor_texture = 0; // floor texture
unsigned int arrow_texture = 0; // floor texture

double AX = 0; // x-coordinate of where the camera is looking
double AY = 0; // y-coordinate of where the camera is looking
double AZ = 0; // z-coordinate of where the camera is looking

double UX = 0; // x-coordinate of the up vector
double UY = 1; // y-coordinate of the up vector
double UZ = 0; // z-coordinate of the up vector

/*
*  Convenience routine to output raster text
*  Use VARARGS to make this more flexible
*/
#define LEN 8192  //  Maximum length of text string
void Print(const char* format , ...)
{
  char    buf[LEN];
  char*   ch=buf;
  va_list args;
  //  Turn the parameters into a character string
  va_start(args,format);
  vsnprintf(buf,LEN,format,args);
  va_end(args);
  //  Display the characters one at a time at the current raster position
  while (*ch)
  glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18,*ch++);
}

void checkOffsets()
{
    if(xOffset > 2000)
        xOffset = 2000;
    else if(xOffset < -2000)
        xOffset = -2000;
    if(zOffset > 2000)
        zOffset = 2000;
    else if(zOffset < -2000)
        zOffset = -2000;
}

double mouse_rotation(double delta, double mid)
{
  return 180 * (delta / mid);
}

  /*
  *  OpenGL (GLUT) calls this routine to display the scene
  */
  void display()
  {
    const double len=10.0;  //  Length of axes
    //  Erase the window and the depth buffer
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    //  Enable Z-buffering in OpenGL
    glEnable(GL_DEPTH_TEST);
    //  Undo previous transformations
    glLoadIdentity();

    if(mode == 0)
    {
        //Set up perspective projection
        double Ex = 2*dim*Sin(th)*Cos(ph);
        double Ey = 2*dim*Sin(ph);
        double Ez = -2*dim*Cos(th)*Cos(ph);
        gluLookAt(Ex,Ey,Ez ,0,0,0, 0,Cos(ph),0);
    }
    else
    {
        //Set up first person projection
        double Cx = -2*dim*Sin(th)*Cos(ph) + xOffset;
        double Cy = 2*dim*Sin(ph) + yOffset;
        double Cz = 2*dim*Cos(th)*Cos(ph) + zOffset;
        gluLookAt(xOffset,yOffset,zOffset ,Cx,Cy,Cz, 0,Cos(ph),0);
    }

    //  Light switch
    if (light)
    {
      //  Translate intensity to color vectors
      float Ambient[]   = {0.3,0.3,0.3,1.0};
      float Diffuse[]   = {1,1,1,1};
      float Specular[]  = {1,1,0,1};
      float white[]     = {1,1,1,1};
      //  Light direction
      float Position[]  = {10*Cos(idle),10,10*Sin(idle),1};
      // printf("%f | %f | %f\n" , Position[0] , Position[1] , Position[2]    );
      //  Draw light position as ball (still no lighting here)
      sphere(Position[0],Position[1],Position[2] , 1,1,1);
      //  Enable lighting with normalization
      glEnable(GL_NORMALIZE);
      //  Enable lighting
      glEnable(GL_LIGHTING);
      //  Location of viewer for specular calculations
      glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,local);
      //  glColor sets ambient and diffuse color materials
      glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
      glEnable(GL_COLOR_MATERIAL);
      //  Enable light 0
      glEnable(GL_LIGHT0);
      //  Set ambient, diffuse, specular components and position of light 0
      glLightfv(GL_LIGHT0,GL_AMBIENT ,Ambient);
      glLightfv(GL_LIGHT0,GL_DIFFUSE ,Diffuse);
      glLightfv(GL_LIGHT0,GL_SPECULAR,Specular);
      glLightfv(GL_LIGHT0,GL_POSITION,Position);
      glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,32.0f);
      glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,white);
    }


    alley(0,-10,0, 10,10,10 , 0, 0, floor_texture, arrow_texture);

    //enable textures


    //  disbale lighting from here on
    glDisable(GL_LIGHTING);


    //  Draw axes
    glColor3f(1,1,1);
    if (axes)
    {
      glBegin(GL_LINES);
      glVertex3d(0.0,0.0,0.0);
      glVertex3d(len,0.0,0.0);
      glVertex3d(0.0,0.0,0.0);
      glVertex3d(0.0,len,0.0);
      glVertex3d(0.0,0.0,0.0);
      glVertex3d(0.0,0.0,len);
      glEnd();
      //  Label axes
      glRasterPos3d(len,0.0,0.0);
      Print("X");
      glRasterPos3d(0.0,len,0.0);
      Print("Y");
      glRasterPos3d(0.0,0.0,len);
      Print("Z");
    }

      glWindowPos2i(5,5);
      if(mode == 0)
      {
          Print("Projection Type: Perspective");
          glWindowPos2i(5,25);
          Print("Dimension: %.0f", dim);
      }
      else
      {
          Print("Projection Type: First Person");
          glWindowPos2i(5,25);
          Print("User Location: { %.3f, %.3f, %.3f }", xOffset, yOffset, zOffset);
      }

      //Render scene
      glFlush();
      glutSwapBuffers();
  }

  /*
  *  GLUT calls this routine when an arrow key is pressed
  */
  void special(int key,int x,int y)
  {
    //  Right arrow key - increase angle by 5 degrees
    if (key == GLUT_KEY_RIGHT)
    th += 5;
    //  Left arrow key - decrease angle by 5 degrees
    else if (key == GLUT_KEY_LEFT)
    th -= 5;
    //  Up arrow key - increase elevation by 5 degrees
    else if (key == GLUT_KEY_UP)
    ph += 5;
    //  Down arrow key - decrease elevation by 5 degrees
    else if (key == GLUT_KEY_DOWN)
    ph -= 5;
    //  PageUp key - increase dim
    else if (key == GLUT_KEY_PAGE_UP)
    dim += 0.1;
    //  PageDown key - decrease dim
    else if (key == GLUT_KEY_PAGE_DOWN && dim>1)
    dim -= 0.1;
    //  Keep angles to +/-360 degrees
    th %= 360;
    ph %= 360;
    //  Update projection
    Project(fov,asp,dim);
    //  Tell GLUT it is necessary to redisplay the scene
    glutPostRedisplay();
  }

  /*
  *  GLUT calls this routine when a key is pressed
  */
  void key(unsigned char ch,int x,int y)
  {
    //  Exit on ESC
    if (ch == 27)
    exit(0);
    //  Reset view angle
    else if (ch == '0')
    {
      th = ph = 0;
      xOffset = 0;
      yOffset = 0;
      zOffset = 0;
    }
    //  Toggle axes
    else if (ch == 'x' || ch == 'X')
    axes = 1-axes;
    //  Toggle lighting
    else if (ch == 'l' || ch == 'L')
    light = 1-light;
    //  Switch projection mode
    else if (ch == 'p' || ch == 'P')
    {
      mode = (mode+1)%3;
      th = ph = 0;
      xOffset = 0;
      yOffset = 0;
      zOffset = 2*dim;
    }
    //  Change field of view angle
    else if (ch == '-' && ch>1)
    fov--;
    else if (ch == '+' && ch<179)
    fov++;
    //FIRST PERSON NAVIGATION WITH WASD
    else if(ch == 'w' || ch == 'W')
    {
        xOffset -= 2*Sin(th);
        zOffset += 2*Cos(th);
        checkOffsets();
    }
    else if(ch == 's' || ch == 'S')
    {
        xOffset += 2*Sin(th);
        zOffset -= 2*Cos(th);
        checkOffsets();
    }
    else if(ch == 'a' || ch == 'A')
    {
        xOffset -= 2*Sin(th-90);
        zOffset += 2*Cos(th-90);
        checkOffsets();
    }
    else if(ch == 'd' || ch == 'D')
    {
        xOffset += 2*Sin(th-90);
        zOffset -= 2*Cos(th-90);
        checkOffsets();
    }

    //  Reproject
    Project(fov,asp,dim);
    //  Tell GLUT it is necessary to redisplay the scene
    glutPostRedisplay();
  }

  void mouse(int button, int state, int x, int y)
  {
    // Wheel reports as button 3(scroll up) and button 4(scroll down)
    if ((button == 3) || (button == 4)) // It's a wheel event
    {
      if (button == 3)
      dim += 0.1;
      else if(dim>1)
      dim -= 0.1;
    }
    if ((button == 0) && (state == GLUT_DOWN || state == GLUT_UP))
    {
      if (state == GLUT_DOWN)
      left_click_down = 1;
      else if (state == GLUT_UP)
      left_click_down = 0;
      double midx = (window_width / 2);
      double midy = (window_height / 2);
      double deltax = (midx - x);
      double deltay = (midy - y);
      saved_th = th - mouse_rotation(deltax , midx);
      saved_ph = ph - mouse_rotation(deltay , midy);
    }
    //  Update projection
    Project(fov,asp,dim);
    //  Tell GLUT it is necessary to redisplay the scene
    glutPostRedisplay();
  }

  void motionmouse(int x, int y)
  {
    if (left_click_down)
    {
      double midx = (window_width / 2);
      double midy = (window_height / 2);
      double deltax = (midx - x);
      double deltay = (midy - y);
      th = mouse_rotation(deltax , midx) + saved_th;
      ph = mouse_rotation(deltay , midy) + saved_ph;
      th %= 360;
      ph %= 360;
    }
    //  Update projection
    Project(fov,asp,dim);
    //  Tell GLUT it is necessary to redisplay the scene
    glutPostRedisplay();
  }



  /*
  *  GLUT calls this routine when the window is resized
  */
  void reshape(int width,int height)
  {
    window_width = width;
    window_height = height;
    //  Ratio of the width to the height of the window
    asp = (height>0) ? (double)width/height : 1;
    //  Set the viewport to the entire window
    glViewport(0,0, width,height);
    //  Set projection
    Project(fov,asp,dim);
  }

  /*
  *  GLUT calls this toutine when there is nothing else to do
  */
  void idlefunc()
  {
    double t = glutGet(GLUT_ELAPSED_TIME)/1000.0;
    idle = fmod(90*t,360);
    glutPostRedisplay();
  }

  /*
  *  Start up GLUT and tell it what to do
  */
  int main(int argc,char* argv[])
  {
    //  Initialize GLUT
    glutInit(&argc,argv);
    //  Request double buffered, true color window with Z buffering at 600x600
    glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
    glutInitWindowSize(window_width,window_height);
    glutCreateWindow("Samuel Volin");

    glutIdleFunc(idlefunc);
    //  Set callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutSpecialFunc(special);
    glutMouseFunc(mouse);
    glutMotionFunc(motionmouse);
    glutKeyboardFunc(key);
    // load textures
    floor_texture = LoadTexBMP("floor.bmp");
    arrow_texture = LoadTexBMP("arrows.bmp");
    // int x, y;
    // // unsigned char *ImageData = stbi_load("arrows4ch.bmp", &x, &y, NULL, 4);
    // GLubyte *ImageData = stbi_load("arrows4ch.bmp", &x, &y, NULL, 4);
    // arrow_texture = (GL_TxOffsetTURE_2D, 0, GL_RGBA8, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, ImageData);
    // printf("%d" , arrow_texture );
    glutMainLoop();
    return 0;
  }
