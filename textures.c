/*
*  Textures
*
*  Draws 3 Barrells to demonstrate orthogonal & prespective projections
*  Throws in some lighting and sweet barrel textures as well
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
#include "CSCIx229.h"

//  OpenGL with prototypes for glext
#define GL_GLEXT_PROTOTYPES
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

int axes=0;       //  Display axes
int mode=0;       //  Projection mode
int th=0;         //  Azimuth of view angle
int ph=0;         //  Elevation of view angle
int fov=55;       //  Field of view (for perspective)
double asp=1;     //  Aspect ratio
double dim=5.0;   //  Size of world
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
int emission  =   0;  // Emission intensity (%)
int ambient   =  30;  // Ambient intensity (%)
int diffuse   = 100;  // Diffuse intensity (%)
int specular  =   0;  // Specular intensity (%)
int shininess =   0;  // Shininess (power of two)
float shinyvec[1];    // Shininess (value)
int zh        =  90;  // Light azimuth
float ylight  =   0;  // Elevation of light
unsigned int side = 0;// side texture
unsigned int end = 0; // end texture


double EX = 0; // x-coordinate of camera position
double EY = 0; // y-coordinate of camera position
double EZ = 10; // z-coordinate of camera position

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


double mouse_rotation(double delta, double mid)
{
  return 180 * (delta / mid);
}

/*
*  Draw vertex in polar coordinates with normal
*/
static void Vertex(double th,double ph)
{
  double x = Sin(th)*Cos(ph);
  double y = Cos(th)*Cos(ph);
  double z =         Sin(ph);
  //  For a sphere at the origin, the position
  //  and normal vectors are the same
  glNormal3d(x,y,z);
  glVertex3d(x,y,z);
}

static void barrel(double x, double y, double z,
  double dx, double dy, double dz,
  double th )
  {
    int resolution = 24;
    double fraction = 15;
    double ex = 1.25;
    double tilt = Sin(60);
    double oneminustilt = 1 - tilt;
    glEnable(GL_TEXTURE_2D);
    // Translations
    glPushMatrix();
    glTranslated(x, y, z);
    glRotated(th, 0, 1, 0);
    glScaled(dx, dy, dz);
    int i;
    float f;



    glBindTexture(GL_TEXTURE_2D,end);
    glBegin(GL_TRIANGLE_FAN);
    glColor3f(1,1,1);
    glNormal3f( 0, -1, 0);
    glTexCoord2f(0.55,0.5);
    glVertex3f(0,-1, 0);
    //bottom
    for(i = 0; i <= resolution ; i++)
    {
      glTexCoord2f(0.47*Cos(fraction*i)+0.60,0.5*Sin(fraction*i)+0.5);
      glVertex3f(Cos(fraction*i),-1, Sin(fraction*i));
    }
    glEnd();




    //side 1
    glBindTexture(GL_TEXTURE_2D,side);
    glBegin(GL_QUAD_STRIP);
    glColor3f(1,1,1);
    for(i = 0; i <= resolution ; i++)
    {
      f = (double)i / resolution;
      glNormal3f( oneminustilt * Cos(fraction*i), -tilt, oneminustilt * Sin(fraction*i));
      glTexCoord2f(f,0.0); glVertex3f(Cos(fraction*i),-1, Sin(fraction*i));
      glTexCoord2f(f,0.2); glVertex3f(ex*Cos(fraction*i),-0.5, ex*Sin(fraction*i));
    }
    glEnd();
    //side 2
    glBegin(GL_QUAD_STRIP);
    glColor3f(1,1,1);
    for(i = 0; i <= resolution ; i++)
    {

      f = (double)i / resolution;
      glNormal3f( Cos(fraction*i), 0, Sin(fraction*i));
      glTexCoord2f(f,0.2); glVertex3f(ex*Cos(fraction*i),-0.5, ex*Sin(fraction*i));
      glTexCoord2f(f,0.8); glVertex3f(ex*Cos(fraction*i),0.5, ex*Sin(fraction*i));
    }
    glEnd();
    //side 3
    glBegin(GL_QUAD_STRIP);
    glColor3f(1,1,1);
    for(i = 0; i <= resolution ; i++)
    {
      f = (double)i / resolution;
      glNormal3f( oneminustilt * Cos(fraction*i), tilt,  oneminustilt * Sin(fraction*i));
      glTexCoord2f(f,0.8); glVertex3f(ex*Cos(fraction*i),0.5, ex*Sin(fraction*i));
      glTexCoord2f(f,1.0); glVertex3f(Cos(fraction*i),1, Sin(fraction*i));
    }
    glEnd();

    glBindTexture(GL_TEXTURE_2D,end);
    glBegin(GL_TRIANGLE_FAN);
    glColor3f(1,1,1);
    glNormal3f( 0, 1, 0);
    glTexCoord2f(0.55,0.5);
    glVertex3f(0,1, 0);
    for(i = 0; i <= resolution ; i++)
    {
      glTexCoord2f(0.47*Cos(fraction*i)+0.60, 0.5*Sin(fraction*i)+0.5);
      glVertex3f(Cos(fraction*i),1, Sin(fraction*i));
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glPopMatrix();
  }

  static void ball(double x,double y,double z,double r)
  {
    int th,ph;
    float yellow[] = {1.0,1.0,0.0,1.0};
    float Emission[]  = {0.0,0.0,0.01*emission,1.0};
    //  Save transformation
    glPushMatrix();
    //  Offset, scale and rotate
    glTranslated(x,y,z);
    glScaled(r,r,r);
    //  White ball
    glColor3f(1,1,1);
    glMaterialfv(GL_FRONT,GL_SHININESS,shinyvec);
    glMaterialfv(GL_FRONT,GL_SPECULAR,yellow);
    glMaterialfv(GL_FRONT,GL_EMISSION,Emission);
    //  Bands of latitude
    for (ph=-90;ph<90;ph+=inc)
    {
      glBegin(GL_QUAD_STRIP);
      for (th=0;th<=360;th+=2*inc)
      {
        Vertex(th,ph);
        Vertex(th,ph+inc);
      }
      glEnd();
    }
    //  Undo transofrmations
    glPopMatrix();
  }


  /*
  *  OpenGL (GLUT) calls this routine to display the scene
  */
  void display()
  {
    const double len=1.5;  //  Length of axes
    //  Erase the window and the depth buffer
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    //  Enable Z-buffering in OpenGL
    glEnable(GL_DEPTH_TEST);
    //  Undo previous transformations
    glLoadIdentity();
    //  Orthogonal - set world orientation
    if(mode == 0)
    {
      glRotatef(ph,1,0,0);
      glRotatef(th,0,1,0);
    }
    //  Perspective - set eye position
    else if (mode == 1)
    {
      double Ex = -2*dim*Sin(th)*Cos(ph);
      double Ey = +2*dim        *Sin(ph);
      double Ez = +2*dim*Cos(th)*Cos(ph);
      gluLookAt(Ex,Ey,Ez , 0,0,0 , 0,Cos(ph),0);
    }
    // First person view
    else{
		// Recalculate where the camera is looking
		AX = -2*dim*Sin(th)*Cos(ph);
		AY = -2*dim*Sin(ph);
		AZ = -2*dim*Cos(th)*Cos(ph);
		// Orient the scene so it imitates first person movement
		gluLookAt(EX, EY, EZ, AX + EX, AY + EY, AZ + EZ, UX, UY, UZ);
	}


    //  Flat or smooth shading
    // glShadeModel(smooth ? GL_SMOOTH : GL_FLAT);

    //  Light switch
    if (light)
    {
      //  Translate intensity to color vectors
      float Ambient[]   = {0.3,0.3,0.3,1.0};
      float Diffuse[]   = {1,1,1,1};
      float Specular[]  = {1,1,0,1};
      float white[]     = {1,1,1,1};
      //  Light direction
      float Position[]  = {5*Cos(idle),0,5*Sin(idle),1};
      // printf("%f | %f | %f\n" , Position[0] , Position[1] , Position[2]    );
      //  Draw light position as ball (still no lighting here)
      ball(Position[0],Position[1],Position[2] , 0.1);
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

    //  Draw Barells
    barrel(0,0,0, 0.5,0.5,0.5 , 0);
    barrel(Cos(idle),Sin(idle), 0 ,.3,.3,.3 , Cos(3*idle));
    barrel(0,2*Cos(idle),2*Sin(idle) ,.3,.3,.3 , Sin(2*idle));
    barrel(3*Sin(idle), 0, 3*Cos(idle),.3,.3,.3 , 0);

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
    //  Display parameters
    glWindowPos2i(10,10);
    Print("Angle=%d,%d  Dim=%.1f FOV=%d Projection=%d",th,ph,dim,fov,mode);
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
    EX = 0;
    EY = 0;
    EZ = 2*dim;
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
    EX = 0;
    EY = 0;
    EZ = 2*dim;
    }
    //  Change field of view angle
    else if (ch == '-' && ch>1)
    fov--;
    else if (ch == '+' && ch<179)
    fov++;
    //  Light elevation
    else if (ch=='[')
    ylight -= 0.1;
    else if (ch==']')
    ylight += 0.1;
    //  Ambient level
    else if (ch=='a' && ambient>0)
    ambient -= 5;
    else if (ch=='A' && ambient<100)
    ambient += 5;
    //  Diffuse level
    else if (ch=='d' && diffuse>0)
    diffuse -= 5;
    else if (ch=='D' && diffuse<100)
    diffuse += 5;
    //  Specular level
    else if (ch=='s' && specular>0)
    specular -= 5;
    else if (ch=='S' && specular<100)
    specular += 5;
    //  Emission level
    else if (ch=='e' && emission>0)
    emission -= 5;
    else if (ch=='E' && emission<100)
    emission += 5;
    //  Shininess level
    else if (ch=='n' && shininess>-1)
    shininess -= 1;
    else if (ch=='N' && shininess<7)
    shininess += 1;
      // Move forward in the scene
    else if(ch == 'f'){
      EX += AX*.1;
      EZ += AZ*.1;
    }
    // Move backwards in the scene
    else if(ch == 'v'){
      EX -= AX*.1;
      EZ -= AZ*.1;
    }
    //  Translate shininess power to value (-1 => 0)
    shinyvec[0] = shininess<0 ? 0 : pow(2.0,shininess);
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
    end = LoadTexBMP("end.bmp");
    side = LoadTexBMP("side.bmp");
    //  Pass control to GLUT so it can interact with the user
    glutMainLoop();
    return 0;
  }
