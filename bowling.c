#include "bowling.h"



// bowling pins
void bowling_pin(double x, double y, double z, double xScale, double yScale, double zScale, double rotateX, double rotateY)
{
  int resolution = 24;
  double fraction = 360 / resolution;
  double tilt,  width, alphawidth, height, additive;
  glPushMatrix();
  glTranslated(x, y,z);
  glRotated(rotateY, 0,1,0);
  glRotated(rotateX, 1,0,0);
  glScaled(xScale, yScale, zScale);
  int i;
  int k;
  double alphaheight = 0.0;
  double alphatilt = 0.0;
  double turning = 0.0;
  double f = 0.0;
  glBegin(GL_TRIANGLE_FAN);
  glColor3f(1,1,1);
  glNormal3f( 0, -1, 0);
  glVertex3f(0,0, 0);
  //bottom
  for(i = 0; i <= resolution ; i++)
  {
    glVertex3f(Cos(fraction*i),0, Sin(fraction*i));
  }
  glEnd();
  //side 1
  for(k = 1; k <= 13 ; k++)
  {
    additive = k < 12 ? (double)((12 - k)*(12 - k))/300 : 0;
    tilt = alphatilt;
    alphatilt = Sin((k)*15+turning);
    width = 1 + tilt;
    alphawidth = 1 + alphatilt;
    height = alphaheight;
    alphaheight +=  0.5;
    alphaheight += additive;
    glBegin(GL_QUAD_STRIP);
    if((k == 13))
    {
      glColor3f(0.765,0.0,0.2);
    }
    else
    {
      glColor3f(1,1,1);
    }
    for(i = 0; i <= resolution ; i++)
    {
      f = (double)i / resolution;
      glNormal3f( 0.5 * Cos(fraction*i), 0.5 , 0.5 * Sin(fraction*i));
      glVertex3f( width * Cos(fraction*i), height, width * Sin(fraction*i));
      glVertex3f( alphawidth * Cos(fraction*i), alphaheight, alphawidth * Sin(fraction*i));
    }
    glEnd();
  }
  //middle white stripe
  width = alphawidth;
  alphawidth = 0.7;
  height = alphaheight;
  alphaheight += 0.5;
  glBegin(GL_QUAD_STRIP);
  glColor3f(1,1,1);
  for(i = 0; i <= resolution ; i++)
  {
    f = (double)i / resolution;
    glNormal3f( 0.5 * Cos(fraction*i), 0.5 , 0.5 * Sin(fraction*i));
    glVertex3f( width * Cos(fraction*i), height, width * Sin(fraction*i));
    glVertex3f( alphawidth * Cos(fraction*i), alphaheight, alphawidth * Sin(fraction*i));
  }
  glEnd();
  //top red stripe
  width = alphawidth;
  alphawidth = 0.75;
  height = alphaheight;
  alphaheight += 0.5;
  glBegin(GL_QUAD_STRIP);
  glColor3f(0.765,0.0,0.2);
  for(i = 0; i <= resolution ; i++)
  {
    f = (double)i / resolution;
    glNormal3f( 0.5 * Cos(fraction*i), 0.5 , 0.5 * Sin(fraction*i));
    glVertex3f( width * Cos(fraction*i), height, width * Sin(fraction*i));
    glVertex3f( alphawidth * Cos(fraction*i), alphaheight, alphawidth * Sin(fraction*i));
  }
  glEnd();
  //top white tie
  width = alphawidth;
  alphawidth = 0.85;
  height = alphaheight;
  alphaheight += 1.0;
  glBegin(GL_QUAD_STRIP);
  glColor3f(1,1,1);
  for(i = 0; i <= resolution ; i++)
  {
    f = (double)i / resolution;
    glNormal3f( 0.5 * Cos(fraction*i), 0.5 , 0.5 * Sin(fraction*i));
    glVertex3f( width * Cos(fraction*i), height, width * Sin(fraction*i));
    glVertex3f( alphawidth * Cos(fraction*i), alphaheight, alphawidth * Sin(fraction*i));
  }
  glEnd();
  //top white tie
  width = alphawidth;
  alphawidth = 0.75;
  height = alphaheight;
  alphaheight += 0.5;
  glBegin(GL_QUAD_STRIP);
  glColor3f(1,1,1);
  for(i = 0; i <= resolution ; i++)
  {
    f = (double)i / resolution;
    glNormal3f( 0.5 * Cos(fraction*i), 0.5 , 0.5 * Sin(fraction*i));
    glVertex3f( width * Cos(fraction*i), height, width * Sin(fraction*i));
    glVertex3f( alphawidth * Cos(fraction*i), alphaheight, alphawidth * Sin(fraction*i));
  }
  glEnd();
  //top white tie
  width = alphawidth;
  alphawidth = 0.60;
  height = alphaheight;
  alphaheight += 0.35;
  glBegin(GL_QUAD_STRIP);
  glColor3f(1,1,1);
  for(i = 0; i <= resolution ; i++)
  {
    f = (double)i / resolution;
    glNormal3f( 0.5 * Cos(fraction*i), 0.5 , 0.5 * Sin(fraction*i));
    glVertex3f( width * Cos(fraction*i), height, width * Sin(fraction*i));
    glVertex3f( alphawidth * Cos(fraction*i), alphaheight, alphawidth * Sin(fraction*i));
  }
  glEnd();
  //top white tie
  width = alphawidth;
  alphawidth = 0.15;
  height = alphaheight;
  alphaheight += 0.2;
  glBegin(GL_QUAD_STRIP);
  glColor3f(1,1,1);
  for(i = 0; i <= resolution ; i++)
  {
    f = (double)i / resolution;
    glNormal3f( 0.5 * Cos(fraction*i), 0.5 , 0.5 * Sin(fraction*i));
    glVertex3f( width * Cos(fraction*i), height, width * Sin(fraction*i));
    glVertex3f( alphawidth * Cos(fraction*i), alphaheight, alphawidth * Sin(fraction*i));
  }
  glEnd();
  glBegin(GL_TRIANGLE_FAN);
  glColor3f(1,1,1);
  glNormal3f( 0, 1, 0);
  glVertex3f(0,alphaheight, 0);
  //top
  for(i = 0; i <= resolution ; i++)
  {
    glVertex3f(alphawidth * Sin(fraction*i), alphaheight, alphawidth * Cos(fraction*i));
  }
  glEnd();
  glPopMatrix();
}

void divider(double x, double y, double z,
  double xScale, double yScale, double zScale,
  double rotateX, double rotateY)
  {
    double height = 0.5;
    double length = 12;
    int i = 0;
    glPushMatrix();
    glTranslated(x, y,z);
    glRotated(rotateY, 0,1,0);
    glRotated(rotateX, 1,0,0);
    glScaled(xScale, yScale, zScale);

    glBegin(GL_QUAD_STRIP);
    glColor3f(0.6,0.6,0.6);
    // divider
    for(i = 0; i < length ; i++)
    {
      glBegin(GL_QUAD_STRIP);
      glNormal3f( -1, 0, 0);
      glVertex3f(0,0,i);
      glVertex3f(0,0,i+1);
      glVertex3f(0,height,i);
      glVertex3f(0,height,i+1);
      glEnd();
      glBegin(GL_QUAD_STRIP);
      glNormal3f( 0, 1, 0);
      glVertex3f(0,height,i);
      glVertex3f(0,height,i+1);
      glVertex3f(1,height,i);
      glVertex3f(1,height,i+1);
      glEnd();
      glBegin(GL_QUAD_STRIP);
      glNormal3f( 1, 0, 0);
      glVertex3f(1,height,i);
      glVertex3f(1,height,i+1);
      glVertex3f(1,0,i);
      glVertex3f(1,0,i+1);
      glEnd();
    }
    glPopMatrix();

  }

  void ball_return_body(double x, double y, double z,
    double xScale, double yScale, double zScale,
    double rotateX, double rotateY)
    {
      int i;
      int resolution = 24;
      double fraction = 360 / resolution;
      double top = 0.6;
      double flat = 0.5;
      double platform = 0.3;
      double base = 0.0;
      double base_a[3] = {0,base,0};
      double base_b[3] = {0,base,0.6};
      double base_c[3] = {0.5,base,0.6};
      double base_d[3] = {0.5,base,0};
      double platform_a[3] = {0.1,platform,0.15};
      double platform_b[3] = {0.1,platform,0.5};
      double platform_c[3] = {0.4,platform,0.5};
      double platform_d[3] = {0.4,platform,0.15};
      double flat_a[3] = {0.05,flat,0.25};
      double flat_b[3] = {0.05,flat,0.5};
      double flat_c[3] = {0.45,flat,0.5};
      double flat_d[3] = {0.45,flat,0.25};
      double top_a[3] = {0.15,top,0.3};
      double top_b[3] = {0.15,top,0.6};
      double top_c[3] = {0.35,top,0.6};
      double top_d[3] = {0.35,top,0.3};


      glPushMatrix();
      glTranslated(x, y,z);
      glRotated(rotateY, 0,1,0);
      glRotated(rotateX, 1,0,0);
      glScaled(xScale, yScale, zScale);


      glColor3f(0.6,0.6,0.6);
      // right
      glBegin(GL_QUAD_STRIP);
      glColor3f(0.6,0.0,0.0);
      glNormal3f( -1, 0, 0);
      glVertex3dv(&base_a[0]);
      glVertex3dv(&base_b[0]);
      glVertex3dv(&platform_a[0]);
      glVertex3dv(&platform_b[0]);
      glVertex3dv(&flat_a[0]);
      glVertex3dv(&flat_b[0]);
      glVertex3dv(&top_a[0]);
      glVertex3dv(&top_b[0]);
      glEnd();
      // left
      glBegin(GL_QUAD_STRIP);
      glColor3f(0.0,0.6,0.0);
      glNormal3f( 1, 0, 0);
      glVertex3dv(&base_c[0]);
      glVertex3dv(&base_d[0]);
      glVertex3dv(&platform_c[0]);
      glVertex3dv(&platform_d[0]);
      glVertex3dv(&flat_c[0]);
      glVertex3dv(&flat_d[0]);
      glVertex3dv(&top_c[0]);
      glVertex3dv(&top_d[0]);
      glEnd();
      glBegin(GL_QUAD_STRIP);
      glColor3f(0.0,0.0,0.6);
      glNormal3f( 1, 0, 0);
      glVertex3dv(&base_a[0]);
      glVertex3dv(&base_d[0]);
      glVertex3dv(&platform_a[0]);
      glVertex3dv(&platform_d[0]);
      glVertex3dv(&flat_a[0]);
      glVertex3dv(&flat_d[0]);
      glVertex3dv(&top_a[0]);
      glVertex3dv(&top_d[0]);
      glEnd();
      //top
      glBegin(GL_QUADS);
      glColor3f(0.6,0.6,0.6);
      glNormal3f( 0, 1, 0);
      glVertex3dv(&top_a[0]);
      glVertex3dv(&top_b[0]);
      glVertex3dv(&top_c[0]);
      glVertex3dv(&top_d[0]);
      glEnd();
      //head lip
      glBegin(GL_QUADS);
      glNormal3f( 0, Sin(315), Cos(315));
      glVertex3dv(&flat_b[0]);
      glVertex3dv(&flat_c[0]);
      glVertex3dv(&top_c[0]);
      glVertex3dv(&top_b[0]);
      glEnd();
      //base lip
      glBegin(GL_QUADS);
      glNormal3f( 0, Sin(15), Cos(15));
      glVertex3dv(&base_b[0]);
      glVertex3dv(&base_c[0]);
      glVertex3dv(&platform_c[0]);
      glVertex3dv(&platform_b[0]);
      glEnd();
      //ball return face
      glBegin(GL_QUADS);
      glNormal3f( 0, 0,1);
      glVertex3dv(&flat_b[0]);
      glVertex3dv(&flat_c[0]);
      glVertex3dv(&platform_c[0]);
      glVertex3dv(&platform_b[0]);
      glEnd();
      //ball return center
      glBegin(GL_TRIANGLE_FAN);
      glColor3f(0,0,0);
      glNormal3f( 0, 0, 1);
      glVertex3f(0.25,0.4, 0.501);
      // {0.1,platform,0.5}
      //bottom
      for(i = 0; i <= resolution ; i++)
      {
        glVertex3f(0.25+0.1*Cos(fraction*i),0.4+0.1*Sin(fraction*i),0.501);
      }
      glEnd();
      //extensions
      //left
      glBegin(GL_QUAD_STRIP);
      glColor3f(0.0,0.0,0.0);
      glNormal3f( 1, 0, 0);
      glVertex3f(0.1,platform,0.5);
      glVertex3f(0.1,platform,0.5+1);
      glVertex3f(0.1,platform+0.02,0.5);
      glVertex3f(0.1,platform+0.02,0.5+1);
      glVertex3f(0.1+0.02,platform,0.5);
      glVertex3f(0.1+0.02,platform,0.5+1);
      glEnd();
      //right
      glBegin(GL_QUAD_STRIP);
      glColor3f(0.0,0.0,0.0);
      glNormal3f( 1, 0, 0);
      glVertex3f(0.4,platform,0.5);
      glVertex3f(0.4,platform,0.5+1);
      glVertex3f(0.4,platform+0.02,0.5);
      glVertex3f(0.4,platform+0.02,0.5+1);
      glVertex3f(0.4-0.02,platform,0.5);
      glVertex3f(0.4-0.02,platform,0.5+1);
      glEnd();
      //middle
      glBegin(GL_QUAD_STRIP);
      glColor3f(0.0,0.0,0.0);
      glNormal3f( 0, 1, 0);
      glVertex3f(0.25+0.02,platform,0.5);
      glVertex3f(0.25+0.02,platform,0.5+1);
      glVertex3f(0.25,platform+0.02,0.5);
      glVertex3f(0.25,platform+0.02,0.5+1);
      glVertex3f(0.25-0.02,platform,0.5);
      glVertex3f(0.25-0.02,platform,0.5+1);
      glEnd();
      //end
      glBegin(GL_QUADS);
      glColor3f(0.0,0.0,0.0);
      glNormal3f( 0, 0, 1);
      glVertex3f(0.4,platform,1.5);
      glVertex3f(0.4,platform+0.02,1.5);
      glVertex3f(0.25,platform+0.02,1.5);
      glVertex3f(0.25,platform,1.5);
      glVertex3f(0.25,platform,1.5);
      glVertex3f(0.25,platform+0.02,1.5);
      glVertex3f(0.1,platform+0.02,1.5);
      glVertex3f(0.1,platform,1.5);
      glEnd();






      glPopMatrix();

    }



void cap(double x, double y, double z,
  double xScale, double yScale, double zScale,
  double rotateX, double rotateY)
  {
    glPushMatrix();
    glTranslated(x, y,z);
    glRotated(rotateY, 0,1,0);
    glRotated(rotateX, 1,0,0);
    glScaled(xScale, yScale, zScale);
    double height = 0.5;
    glBegin(GL_QUAD_STRIP);
    glColor3f(0.6,0.6,0.6);
    glNormal3f( 1, 0, 0);
    glVertex3f(0,0,0);
    glVertex3f(0,height,0);
    glVertex3f(0,0,0.1);
    glVertex3f(0,height,0.1);
    glNormal3f( .7, 0, .7);
    glVertex3f(0.2,0,0.2);
    glVertex3f(0.2,height/2,0.2);
    glNormal3f( 0, 0, 1);
    glVertex3f(0.8,0,0.2);
    glVertex3f(0.8,height/2,0.2);
    glNormal3f( -.7, 0, .7);
    glVertex3f(0.9,0,0.1);
    glVertex3f(0.9,height,0.1);
    glNormal3f( -1, 0, 0);
    glVertex3f(1.0,0,0);
    glVertex3f(1.0,height,0);
    glEnd();
    glBegin(GL_POLYGON);
    glColor3f(0.6,0.6,0.6);
    glNormal3f( 0, 1, 0);
    glVertex3f(0,height,0);
    glVertex3f(0,height,0.1);
    glVertex3f(0.2,height/2,0.2);
    glVertex3f(0.8,height/2,0.2);
    glVertex3f(0.9,height,0.1);
    glVertex3f(1.0,height,0);
    glEnd();
    glPopMatrix();


  }



void alley(double x, double y, double z,
  double xScale, double yScale, double zScale,
  double rotateX, double rotateY, unsigned int floor_texture)
  {
    int i, l;
    double k,j;
    // int resolution  = 12;
    // double fraction = 360 / resolution;
    double length = 12;
    double slightlyabove = 0.001;
    double xp, zp;

    glPushMatrix();
    glTranslated(x, y,z);
    glRotated(rotateY, 0,1,0);
    glRotated(rotateX, 1,0,0);
    glScaled(xScale, yScale, zScale);


    glBegin(GL_QUAD_STRIP);
    glColor3f(0.2,0.2,0.2);
    // gutters
    for(l = 0; l < 2 ; l++)
    {
      for(i = 0; i < length ; i++)
      {
        k = l ==0 ? -0.25 : 1.25;
        glBegin(GL_QUAD_STRIP);
        glNormal3f( -1, 0, 0);
        glVertex3f(l,0,i);
        glVertex3f(l,0,i+1);
        glVertex3f(l,-0.1,i);
        glVertex3f(l,-0.1,i+1);
        glEnd();
        glBegin(GL_QUAD_STRIP);
        glNormal3f( 0, 1, 0);
        glVertex3f(l,-0.1,i);
        glVertex3f(l,-0.1,i+1);
        glVertex3f(k,-0.1,i);
        glVertex3f(k,-0.1,i+1);
        glEnd();
        glBegin(GL_QUAD_STRIP);
        glNormal3f( 1, 0, 0);
        glVertex3f(k,-0.1,i);
        glVertex3f(k,-0.1,i+1);
        glVertex3f(k,0,i);
        glVertex3f(k,0,i+1);
        glEnd();
      }
    }
    //alley
    glColor3f(1,1,1);
    glEnable(GL_TEXTURE_2D);
    glNormal3f( 0, 1, 0);
    for(i = 0; i < length ; i++)
    {
      k = (double)((double)i / length);
      j = (double)((double)(i+1) / length);
      glBindTexture(GL_TEXTURE_2D,floor_texture);
      glBegin(GL_QUAD_STRIP);
      glNormal3f( 0, 1, 0);
      glTexCoord2f(0,k); glVertex3f(0,0,i);
      glTexCoord2f(1,k); glVertex3f(1,0,i);
      glTexCoord2f(0,j); glVertex3f(0,0,i+1);
      glTexCoord2f(1,j); glVertex3f(1,0,i+1);
      glEnd();
    }
    glDisable(GL_TEXTURE_2D);
    //arrows
    glBegin(GL_TRIANGLES);
    glColor3f(1,0,0);
    for(l = 0; l < 2; l++)
    {

      for(i = 0; i < 5 ; i++)
      {
        xp = 0.10 * i + 0.10;
        zp = 6*l+0.03*i+0.1;
        glVertex3f(xp,slightlyabove,zp);
        glVertex3f(xp-0.04,0,zp-0.08);
        glVertex3f(xp+0.04,0,zp-0.08);
        xp = 1 - (0.10 * i) - 0.10;
        glVertex3f(xp,slightlyabove,zp);
        glVertex3f(xp-0.04,0,zp-0.08);
        glVertex3f(xp+0.04,0,zp-0.08);
      }

    }
    glEnd();


    glPopMatrix();
  }

  void wall(double x, double y, double z,
    double xScale, double yScale, double zScale,
    double rotateX, double rotateY, unsigned int wall_texture )
    {
      glPushMatrix();
      glTranslated(x, y,z);
      glRotated(rotateY, 0,1,0);
      glRotated(rotateX, 1,0,0);
      glScaled(xScale, yScale, zScale);
      double bottom = 0.0;
      double top = 3.0;
      double length = 3.0;
      glEnable(GL_TEXTURE_2D);
      glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
      glBindTexture(GL_TEXTURE_2D,wall_texture);
      glBegin(GL_QUADS);
      glColor3f(0.8,0.4,0.4);
      glNormal3f( 1, 0, 0);
      glTexCoord2f(0,0);  glVertex3f(0,bottom,0);
      glTexCoord2f(1,0);  glVertex3f(0,bottom,length);
      glTexCoord2f(1,1);  glVertex3f(0,top,length);
      glTexCoord2f(0,1);  glVertex3f(0,top,0);
      glEnd();
      glDisable(GL_TEXTURE_2D);


      glPopMatrix();
    }

  void mural(double x, double y, double z,
    double xScale, double yScale, double zScale,
    double rotateX, double rotateY, unsigned int mural_texture )
    {
      int length = 12;
      float shinyvec[1];
      float emission = 0.0;
      float white[] = {1,1,1,1};
      float Emission[]  = {0.0,0.0,0.01*emission,1.0};
      double bottom = 0.5;
      double top = 3.0;

      glMaterialfv(GL_FRONT,GL_SHININESS,shinyvec);
      glMaterialfv(GL_FRONT,GL_SPECULAR,white);
      glMaterialfv(GL_FRONT,GL_EMISSION,Emission);

      glPushMatrix();
      glTranslated(x, y,z);
      glRotated(rotateY, 0,1,0);
      glRotated(rotateX, 1,0,0);
      glScaled(xScale, yScale, zScale);

      glColor3f(1,1,1);



      glEnable(GL_TEXTURE_2D);
      glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
      glBindTexture(GL_TEXTURE_2D,mural_texture);
      glBegin(GL_QUADS);
      glColor3f(0.6,0.6,0.6);
      glNormal3f( 0, 0.8, -0.2);
      glTexCoord2f(0,0);  glVertex3f(3,bottom,length);
      glTexCoord2f(1,0);  glVertex3f(0,bottom,length);
      glTexCoord2f(1,1);  glVertex3f(0,top,length-0.2);
      glTexCoord2f(0,1);  glVertex3f(3,top,length-0.2);
      glEnd();
      glDisable(GL_TEXTURE_2D);


      glPopMatrix();
    }

  void floor_panel(double x, double y, double z, double xScale, double yScale, double zScale, double rotateX, double rotateY, unsigned int floor_texture)
  {
    glPushMatrix();
    glTranslated(x, y,z);
    glRotated(rotateY, 0,1,0);
    glRotated(rotateX, 1,0,0);
    glScaled(xScale, yScale, zScale);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D,floor_texture);
    glBegin(GL_QUADS);
    glNormal3f( 0, 0.8, -0.2);
    glTexCoord2f(1,0);  glVertex3f(3,0,0);
    glTexCoord2f(1,1);  glVertex3f(0,0,0);
    glTexCoord2f(0,1);  glVertex3f(0,0,1);
    glTexCoord2f(0,0);  glVertex3f(3,0,1);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glPopMatrix();
  }

  void pins(double x, double y, double z, double xScale, double yScale, double zScale, double rotateX, double rotateY)
  {
    // head
    bowling_pin(x+5,y+0,z+111,0.25*xScale,0.25*yScale,0.25*zScale,0,0);
    // twins
    bowling_pin(x+4,y+0,z+112.5,0.25*xScale,0.25*yScale,0.25*zScale,0,0);
    bowling_pin(x+6,y+0,z+112.5,0.25*xScale,0.25*yScale,0.25*zScale,0,0);
    //thirds
    bowling_pin(x+3,y+0,z+114,0.25*xScale,0.25*yScale,0.25*zScale,0,0);
    bowling_pin(x+5,y+0,z+114,0.25*xScale,0.25*yScale,0.25*zScale,0,0);
    bowling_pin(x+7,y+0,z+114,0.25*xScale,0.25*yScale,0.25*zScale,0,0);
    //back
    bowling_pin(x+2,y+0,z+115.5,0.25*xScale,0.25*yScale,0.25*zScale,0,0);
    bowling_pin(x+4,y+0,z+115.5,0.25*xScale,0.25*yScale,0.25*zScale,0,0);
    bowling_pin(x+6,y+0,z+115.5,0.25*xScale,0.25*yScale,0.25*zScale,0,0);
    bowling_pin(x+8,y+0,z+115.5,0.25*xScale,0.25*yScale,0.25*zScale,0,0);
  }

  void double_lane(double x, double y, double z, double xScale, double yScale, double zScale, double rotateX, double rotateY, unsigned int mural_texture, unsigned int floor_texture)
  {
    mural(x-2,y,z-10, 12*xScale,10*yScale,10*zScale , 0, 0, mural_texture);
    alley(x,y,z, 10*xScale,10*yScale,10*zScale , 0, 0, floor_texture);
    pins(x,y,z,1,1,1,0,0);
    divider(x+12.5,y,z, 4*xScale,1*yScale,10*zScale , 0, 0);
    cap(x+16.5,y,z,4*xScale,1*yScale,10*zScale,0,180);
    ball_return_body(x+17,y,z-15, 10*xScale,10*yScale,10*zScale , 0, 180);
    alley(x+19,y,z, 10*xScale,10*yScale,10*zScale , 0, 0, floor_texture);
    pins(x+19,y,z,1,1,1,0,0);
    divider(x+31.5,y,z, 2*xScale,1*yScale,10*zScale , 0, 0);
    cap(x+33.5,y,z,2*xScale,1*yScale,5*zScale,0,180);
    floor_panel(x-2.5,y,z-10,12*xScale,10*yScale,10*zScale,0,0, floor_texture);
    floor_panel(x-2.5,y,z-20,12*xScale,10*yScale,10*zScale,0,0, floor_texture);
    floor_panel(x-2.5,y,z-30,12*xScale,10*yScale,10*zScale,0,0, floor_texture);
    floor_panel(x-2.5,y,z-40,12*xScale,10*yScale,10*zScale,0,0, floor_texture);
  }


  /*  Make a cube of unit length. Rotate, scale, and translate based on
  designated parameters. */
  void cube(double x, double y, double z, double xScale, double yScale, double zScale, double rotateX, double rotateY, unsigned int texture)
  {
    float gray[] = {0.1,0.1,0.1,1.0};

    glPushMatrix();
    glTranslated(x, y,z);
    glRotated(rotateY, 0,1,0);
    glRotated(rotateX, 1,0,0);
    glScaled(xScale, yScale, zScale);

    glColor3f(0.892,0.872,0.784);
    glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,gray);

    glBindTexture(GL_TEXTURE_2D, texture);

    glBegin(GL_QUADS);
    glNormal3d(-1.0,0.0,0.0);
    glTexCoord2f(0.0,yScale/6);
    glVertex3d(-0.5, 1.0, -0.5);
    glTexCoord2f(zScale/5,yScale/6);
    glVertex3d(-0.5, 1.0, 0.5);
    glTexCoord2f(zScale/5,0.0);
    glVertex3d(-0.5, 0.0, 0.5);
    glTexCoord2f(0.0,0.0);
    glVertex3d(-0.5, 0.0, -0.5);
    glEnd();


    glBegin(GL_QUADS);
    glNormal3d(1.0,0.0,0.0);
    glTexCoord2f(0.0,yScale/6);
    glVertex3d(0.5, 1.0, -0.5);
    glTexCoord2f(zScale/5,yScale/6);
    glVertex3d(0.5, 1.0, 0.5);
    glTexCoord2f(zScale/5,0.0);
    glVertex3d(0.5, 0.0, 0.5);
    glTexCoord2f(0.0,0.0);
    glVertex3d(0.5, 0.0, -0.5);
    glEnd();


    glBegin(GL_QUADS);
    glNormal3d(0.0,0.0,-1.0);
    glTexCoord2f(0.0,yScale/6);
    glVertex3d(-0.5, 1.0, -0.5);
    glTexCoord2f(xScale/5,yScale/6);
    glVertex3d(0.5, 1.0, -0.5);
    glTexCoord2f(xScale/5,0.0);
    glVertex3d(0.5, 0.0, -0.5);
    glTexCoord2f(0.0,0.0);
    glVertex3d(-0.5, 0.0, -0.5);
    glEnd();

    glBegin(GL_QUADS);
    glNormal3d(0.0,0.0,1.0);
    glTexCoord2f(0.0,yScale/6);
    glVertex3d(-0.5, 1.0, 0.5);
    glTexCoord2f(xScale/5,yScale/6);
    glVertex3d(0.5, 1.0, 0.5);
    glTexCoord2f(xScale/5,0.0);
    glVertex3d(0.5, 0.0, 0.5);
    glTexCoord2f(0.0,0.0);
    glVertex3d(-0.5, 0.0, 0.5);
    glEnd();

    glBegin(GL_QUADS);
    glNormal3d(0.0,1.0,0.0);
    glTexCoord2f(0.0,0.0);
    glVertex3d(-0.5, 1.0, -0.5);
    glTexCoord2f(xScale/5,0.0);
    glVertex3d(0.5, 1.0, -0.5);
    glTexCoord2f(xScale/5,zScale/6);
    glVertex3d(0.5, 1.0, 0.5);
    glTexCoord2f(0.0,zScale/6);
    glVertex3d(-0.5, 1.0, 0.5);
    glEnd();

    glBegin(GL_QUADS);
    glNormal3d(0.0,-1.0,0.0);
    glTexCoord2f(0.0,0.0);
    glVertex3d(-0.5, 0.0, -0.5);
    glTexCoord2f(xScale/5,0.0);
    glVertex3d(0.5, 0.0, -0.5);
    glTexCoord2f(xScale/5,zScale/6);
    glVertex3d(0.5, 0.0, 0.5);
    glTexCoord2f(0.0,zScale/6);
    glVertex3d(-0.5, 0.0, 0.5);
    glEnd();

    glPopMatrix();
  }

  /*  Make a cube of unit length. Rotate, scale, and translate based on
  designated parameters. */
  void pyramid(double x, double y, double z, double xScale, double yScale, double zScale, double rotateAngle, unsigned int texture)
  {
    float gray[] = {0.1,0.1,0.1,1.0};

    glPushMatrix();
    glTranslated(x, y , z);
    glRotated(rotateAngle, 0,1,0);
    glScaled(xScale, yScale, zScale);

    glColor3f(0.892,0.872,0.784);
    glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,gray);

    glBindTexture(GL_TEXTURE_2D, texture);

    glBegin(GL_TRIANGLES);
    glNormal3d(0.0,0.5,-0.25);
    glTexCoord2f(xScale/10,yScale/6);
    glVertex3d(0.0,1.0,0.0);
    glNormal3d(0.0,0.5,-0.25);
    glTexCoord2f(0.0,0.0);
    glVertex3d(-0.5, 0.0, -0.5);
    glNormal3d(0.0,0.5,-0.25);
    glTexCoord2f(xScale/5,0.0);
    glVertex3d(0.5, 0.0, -0.5);


    glNormal3d(0.25,0.5,0.0);
    glTexCoord2f(zScale/10,yScale/6);
    glVertex3d(0.0,1.0,0.0);
    glNormal3d(0.25,0.5,0.0);
    glTexCoord2f(0.0,0.0);
    glVertex3d(0.5, 0.0, -0.5);
    glNormal3d(0.25,0.5,0.0);
    glTexCoord2f(zScale/5,0.0);
    glVertex3d(0.5, 0.0, 0.5);


    glNormal3d(0.0,0.5,0.25);
    glTexCoord2f(xScale/10,yScale/6);
    glVertex3d(0.0,1.0,0.0);
    glNormal3d(0.0,0.5,0.25);
    glTexCoord2f(0.0,0.0);
    glVertex3d(0.5, 0.0, 0.5);
    glNormal3d(0.0,0.5,0.25);
    glTexCoord2f(xScale/5,0.0);
    glVertex3d(-0.5, 0.0, 0.5);


    glNormal3d(-0.25,0.5,0.0);
    glTexCoord2f(zScale/10,yScale/6);
    glVertex3d(0.0,1.0,0.0);
    glNormal3d(-0.25,0.5,0.0);
    glTexCoord2f(0.0,0.0);
    glVertex3d(-0.5, 0.0, 0.5);
    glNormal3d(-0.25,0.5,0.0);
    glTexCoord2f(zScale/5,0.0);
    glVertex3d(-0.5, 0.0, -0.5);
    glEnd();

    glPopMatrix();
  }

  /*  Draw a white sphere */
  void sphere(double x, double y, double z, double xScale, double yScale, double zScale)
  {
    double az;
    double el;

    float white[] = {1.0,1.0,1.0,1.0};
    float black[] = {0.0,0.0,0.0,1.0};

    glPushMatrix();
    glTranslated(x, y, z);
    glScaled(xScale, yScale, zScale);
    glColor3f(1,1,1);
    glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,white);
    glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,black);
    for(el = -90.0; el < 90; el+=0.5)
    {
      glBegin(GL_QUAD_STRIP);
      for(az = 0.0; az <= 360.0; az+=0.5)
      {
        glNormal3d(Sin(az)*Cos(el),Sin(el),Cos(az)*Cos(el));
        glVertex3d(Sin(az)*Cos(el),Sin(el),Cos(az)*Cos(el));
        glNormal3d(Sin(az)*Cos(el+0.5),Sin(el+0.5),Cos(az)*Cos(el+0.5));
        glVertex3d(Sin(az)*Cos(el+0.5),Sin(el+0.5),Cos(az)*Cos(el+0.5));
      }
      glEnd();
    }

    glPopMatrix();
  }
