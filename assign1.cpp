/*
  CSCI 480 Computer Graphics
  Assignment 1: Height Fields
  C++ starter code
  First Author: Instructor
  Author: Wei-Hung Liu
*/

#include <stdlib.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>
#include <pic.h>
#include <sstream>
#include <math.h>


const int WIDTH    		= 640;		/* initial window width */
const int HEIGHT   		= 480;		/* initial window height */
const GLdouble FOV 		= 40.0;	  /* perspective field of view */
bool SAVE_SCREENSHOT 	= false;
int COLOR_VERTICES  	= 0;      /* 0:single color, 1:multiple color, 3: texture mapping*/
bool START_ANIMATION  = true;

int frame_Num 				= 0;      /* screenshot number */
int delay = 15;      				    /* minimum delay between frames in milliseconds*/     				
float rpm = 15;         				/* rotation speed */


int g_iMenuId;

/* most recent mouse position in screen coordinates */
/* only differences are relevant, not absolute positions */
int g_vMousePos[2]       = {0, 0};

/* mouse button state, 1 if pressed, 0 if released */
int g_iLeftMouseButton   = 0;    
int g_iMiddleMouseButton = 0;
int g_iRightMouseButton  = 0;

/* control state derived from mouse button state */
typedef enum { ROTATE, TRANSLATE, SCALE } CONTROLSTATE;
CONTROLSTATE g_ControlState = ROTATE;

/* render state derived from keyboard state */
typedef enum {POINTS, WIREFRAME, TRIANGLES, MESH} RENDERSTATE;
RENDERSTATE g_RenderState = POINTS;


/* state of the world */
float g_vLandRotate[3]    = {0.0, 0.0, 0.0};
float g_vLandTranslate[3] = {0.0, 0.0, 0.0};
float g_vLandScale[3]     = {1.0, 1.0, 1.0};

/* see <your pic directory>/pic.h for type Pic */
Pic * g_pHeightData;

/* storage for textures (only one for now)*/
GLuint texture[1];      

/* Image type - contains height, width, and data */
struct Image {
	GLubyte *bits;
	unsigned long width;
	unsigned long height;
};


/*----------------------- Utility function ------------------------------*/
/* Little Endian - Big Endian platforms*/
unsigned int getFourBytes(FILE *in) {
   unsigned int b, b1, b2, b3;
   // get 4 bytes
   b  = ((unsigned int)getc(in));  
   b1 = ((unsigned int)getc(in)) << 8;  
   b2 = ((unsigned int)getc(in)) << 16;  
   b3 = ((unsigned int)getc(in)) << 24;  
   return b | b1 | b2 | b3;
}
unsigned short getTwoBytes(FILE* in){
   unsigned short b, b1;
   //get 2 bytes
   b  = ((unsigned short)getc(in));  
   b1 = ((unsigned short)getc(in)) << 8;  
   return b | b1;
}

/* bitmap loader for 24 bit bitmaps with 1 plane only. */  
bool ReadImage(char *filename, Image *image) {
   FILE *in;                                                              

   unsigned short int planes;   /* number of planes in image (must be 1)    */
   unsigned short int bpp;      /* number of bits per pixel (must be 24)    */
   unsigned long bpp_size;          /* size of the image in bytes.              */
   size_t i,j,k, distance_line; /* standard counter.                        */   
   char tmpdata;                /* temporary storage for bgr-rgb conversion.*/
	 /* read the file stream */	
   if ((in = fopen(filename, "rb"))==NULL) {
      printf("File Not Found : %s\n",filename);
      return false;
   }
   /* read bmp header, up to the width/height: */
   fseek(in, 18, SEEK_CUR);
		
	 /* read width/height */	
   image->width = getFourBytes(in);
   printf("Width of %s: %lu\n", filename, image->width);
   image->height = getFourBytes(in);
   printf("Height of %s: %lu\n", filename, image->height);


   /* read the planes */
   planes = getTwoBytes(in);
   if (planes != 1) {
      printf("Planes from %s is not 1: %u\n", filename, planes);
      return false;
   }

   /* read the bpp */
   bpp = getTwoBytes(in);
   if (bpp != 24) {
      printf("Bpp from %s is not 24: %u\n", filename, bpp);
      return 0;
   }

   /* read the rest of the bitmap header.*/
   fseek(in, 24, SEEK_CUR);

   /* calculate the size (assuming 24 bits or 3 bytes per pixel).*/
   bpp_size = 4.0*ceil(image->width*24.0/32.0) * image->height ;
   
   /* allocate space for the image bits. */ 
   image->bits = new GLubyte[bpp_size];
   if (image->bits == NULL) {
      printf("Error allocating memory for color-corrected image data");
      return false;
   }

   /* read the data */
   i = fread(image->bits, bpp_size, 1, in);
   if (i != 1) {
      printf("Error reading image data from %s.\n", filename);
      return false;
   }
	
   /* calculate distance to 4 byte boundary for each line.
   * if this distance is not 0, then there will be a color reversal error
   * unless we correct for the distance on each line. 
   * reverse all of the colors. (bgr -> rgb) */
   distance_line = 4.0*ceil(image->width*24.0/32.0) - image->width*3.0;
   k = 0;
   for (j=0;j<image->height;j++) {
      for (i=0;i<image->width;i++) {
	 			tmpdata          = image->bits[k];
	 			image->bits[k]   = image->bits[k+2];
	 			image->bits[k+2] = tmpdata;
	 			k+=3;
      }
      k+= distance_line;
   }
   return true;
}   
/* Bitmaps Convert To Textures */
void ReadTextures() {	
   
   Image *imageTexture;
   imageTexture = new Image();
   if (imageTexture == NULL){                													/* allocate space for texture */
      printf("Error allocating space for image");
      exit(0);
   }
   if (!ReadImage("lena256.bmp", imageTexture)) { 										/* load picture from file*/
      exit(1);
   }
   glGenTextures(1, &texture[0]);              												/* Create Texture Name and Bind it as current*/
   glBindTexture(GL_TEXTURE_2D, texture[0]);   												/* 2d texture (x and y size)*/
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);   	/* scale linearly when image bigger than texture */
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);   	/* scale linearly when image smaller than texture*/
   // Load texture into OpenGL RC
   glTexImage2D(GL_TEXTURE_2D,     	// 2D texture
		0,                  						// level of detail 0 (normal)
		GL_RGB,	                				// 3 color components
		imageTexture->width,      			// x size from image
		imageTexture->height,      			// y size from image
		0,	                						// border 0 (normal)
		GL_RGB,             						// rgb color data order
		GL_UNSIGNED_BYTE,   						// color component types
		imageTexture->bits        			// image data itself
    );
};

/* Write a screenshot to the specified filename */
void saveScreenshot (char *filename)
{
  int i, j;
  Pic *in = NULL;

  if (filename == NULL)
    return;

  in = pic_alloc(WIDTH, HEIGHT, 3, NULL);/* Allocate a picture buffer */

  printf("File to save to: %s\n", filename);

  for (i=479; i>=0; i--) {
    glReadPixels(0, 479-i, 640, 1, GL_RGB, GL_UNSIGNED_BYTE,
                 &in->pix[i*in->nx*in->bpp]);
  }

  if (jpeg_write(filename, in))
    printf("File saved Successfully\n");
  else
    printf("Error in Saving\n");

  pic_free(in);
}
/* call screenshot function and create filename */
void triggerSaveScreenshot(){
	char str[2048];
	if (SAVE_SCREENSHOT && frame_Num <= 300) {
		frame_Num++;
		sprintf(str, "%03d.jpg", frame_Num); 
		saveScreenshot(str);		
	} else {
		SAVE_SCREENSHOT = false;
	}	
}
void animation () {
   //Rotations     1 minute    1 second   360 degrees     degrees
   //---------  * ---------- * -------- * ------------ =  -------
   //1 minute     60 seconds   1000 ms      Rotation        ms
   float rotateunit = rpm/60/1000*360*delay;
   g_vLandRotate[2]+=rotateunit; /* Increase Z Axis Rotation  */
}

/*----------------------- Draw function ------------------------------*/
/* draw floor */
void drawFloor()
{
  glColor3f(1.0, 1.0, 1.0);
  glBegin(GL_LINES);
  for (GLfloat i = -2.5; i <= 2.5; i += 0.25) {
    glVertex3f(i, 2.5, 0); 
    glVertex3f(i, -2.5, 0);
    glVertex3f(2.5, i, 0); 
    glVertex3f(-2.5, i, 0);
  }
  glEnd();
}
/* drax x and y axes in medium red */
void drawXYAxes()
{
  glColor3f(0.5, 0.5, 0.5);
  glBegin(GL_LINES);
    glVertex3f(-2.5,  0.0, 0.0);
    glVertex3f( 2.5,  0.0, 0.0);
    glVertex3f( 0.0, -2.5, 0.0);
    glVertex3f( 0.0,  2.5, 0.0);
  glEnd();
}

/* draw z axis in medium red */
void drawZAxis()
{
  glColor3f(0.5, 0.5, 0.5);
  glBegin(GL_LINES);
    glVertex3f(0.0, 0.0, 0.0);
    glVertex3f(0.0, 0.0, 2.5);
  glEnd();
}

void colorVertices(float z)
{
	if(z< 0.2f)      	/* violet - cyan */
      glColor3f(0.7 - z*3, 0.2 + z*2, 1); 
	else if(z< 0.4f) 	/* cyan - lime */
      glColor3f(0.1 + (z - 0.2f)/2*5, 0.6 + (z - 0.2f), 0); 	
	else if(z<0.6f)  	/* lime - yellow*/
      glColor3f(0.6 + (z - 0.4f)/2*3, 0.8, 0); 	
	else if(z< 0.8f )	/*yellow - orange*/
      glColor3f(0.95, 0.85 - (z- 0.6f), 0);  
	else 							/*orange - pink */
	    glColor3f(0.95,0.65 - (z- 0.8f), (z- 0.8f)*4); 
}


void drawHeightFieldPoints()
{
  int i, j;
  GLfloat x, y, z;
  int nx = g_pHeightData->nx;        /* number of x-pixels */
  int ny = g_pHeightData->ny;        /* number of y-pixels */
  GLfloat s = (nx >= ny) ? nx : ny;  /* scale to preserve aspect ratio */
  for (i = 0; i < nx; i++) {
    x = (GLfloat) i;
    glBegin(GL_POINTS);
    for (j = 0; j < ny; j++) {
      y = (GLfloat) j;
      z = (GLfloat) PIC_PIXEL(g_pHeightData, i, j, 2);
      if (COLOR_VERTICES == 1)
      	colorVertices(z/255.0);
      else   
        glColor3f(z/255.0, z/255.0, 1.0);
      glVertex3f(x/s-0.5, y/s-0.5, z/255.0);
    }
    glEnd();
  }
}
void drawHeightFieldWireFrame()
{
  int i2, j2, i3, j3;
  GLfloat x2, y2, z2;
  int nx2 = g_pHeightData->nx;            /* number of x-pixels */
  int ny2 = g_pHeightData->ny;            /* number of y-pixels */
  GLfloat s2 = (nx2 >= ny2) ? nx2 : ny2;  /* scale to preserve aspect ratio */
  
  for (i2 = 0; i2 < nx2 ;i2++) {
    x2 = (GLfloat) i2;		
		glBegin(GL_LINE_STRIP);          			/* strip of line along the row*/
		for (j2 = 0; j2 < ny2 ;j2++) {
      y2 = (GLfloat) j2;			
      z2 = (GLfloat) PIC_PIXEL(g_pHeightData, j2, i2, 2);
      if (COLOR_VERTICES == 1)
      	colorVertices(z2/255.0);
      else   
        glColor3f(z2/255.0, z2/255.0, 1.0);
      if (g_RenderState == MESH) glColor4f(0.2,0.2,0.2,0.5);        /* transparent wireframe*/
      glVertex3f(y2/s2-0.5, x2/s2-0.5, z2/255.0);	
		}
		glEnd();
	}

	for (j3 = 0; j3 < nx2 ;j3++) {
    y2 = (GLfloat) j3;					
		glBegin(GL_LINE_STRIP);          			/* strip of line along the column*/ 
		for (i3 = 0; i3 < ny2;i3++) {
      x2 = (GLfloat) i3;					
      z2 = (GLfloat) PIC_PIXEL(g_pHeightData, j3, i3, 2);
      if (COLOR_VERTICES == 1)
      	colorVertices(z2/255.0);
      else   
        glColor3f(z2/255.0, z2/255.0, 1.0);
      if (g_RenderState == MESH) glColor4f(0.2,0.2,0.2,0.5);        /* transparent wireframe*/        
      glVertex3f(y2/s2-0.5, x2/s2-0.5, z2/255.0);
		}
		glEnd();
	}
}
/* draw height field in blue to white */
/* into cube -0.5 <= x,y < 0.5, 0.0 <= z <= 1.0 */
/* based on blue component of picture heightData */
/* if polygonmode = GL_POINT every point will be drawn twice */
/* this could be fixed with a small amount of additional complexity */
void drawHeightFieldTriangles()
{
  int i, j;
  GLfloat x, y, z;
  int nx = g_pHeightData->nx;        /* number of x-pixels */
  int ny = g_pHeightData->ny;        /* number of y-pixels */
  GLfloat s = (nx >= ny) ? nx : ny;  /* scale to preserve aspect ratio */
  GLfloat temp, temp2;
  int kk=0;
  for (i = 0; i < nx-1; i++) {
    x = (GLfloat) i;
    glBegin(GL_TRIANGLE_STRIP);
    for (j = 0; j < ny-1; j++) {
      y = (GLfloat) j;
      
      z = (GLfloat) PIC_PIXEL(g_pHeightData, i, j, 2);
      // color vertices mode 
      if (COLOR_VERTICES == 1)
      	colorVertices(z/255.0);
      else if (COLOR_VERTICES == 2)  {
				if(kk%4 == 0){// repeat mode
					glTexCoord2f(0.0f, 0.0f);
				} else if (kk%4 == 1 ){
					glTexCoord2f(1.0f, 0.0f);
				}  else if (kk%4 == 2 ){
					glTexCoord2f(1.0f, 1.0f);
				}  else if (kk%4 == 3 ){
					glTexCoord2f(0.0f, 1.0f);
				}     
				kk++;
			} else {
				glColor3f(z/255.0, z/255.0, 1.0);
			}
      glVertex3f(x/s-0.5, y/s-0.5, z/255.0);
			
			
      z = (GLfloat) PIC_PIXEL(g_pHeightData, i+1, j, 2);
      // color vertices mode 
      if (COLOR_VERTICES == 1)
      	colorVertices(z/255.0);
      else if (COLOR_VERTICES == 2)  {
				if(kk%4 == 0){// repeat mode
					glTexCoord2f(0.0f, 0.0f);
				} else if (kk%4 == 1 ){
					glTexCoord2f(1.0f, 0.0f);
				}  else if (kk%4 == 2 ){
					glTexCoord2f(1.0f, 1.0f);
				}  else if (kk%4 == 3 ){
					glTexCoord2f(0.0f, 1.0f);
				}     
				kk++;
			} else {
				glColor3f(z/255.0, z/255.0, 1.0);
			}
      glVertex3f((x+1.0)/s-0.5, y/s-0.5, z/255.0);   
    }
    glEnd();
  }
} 

void drawHeightFieldMesh()
{
	glEnable(GL_POLYGON_OFFSET_FILL);      	/* for offset */
	glEnable(GL_BLEND);                    	/* transparency for enhancement of the visual effect*/
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glPolygonOffset(1,1);             
  drawHeightFieldTriangles();            	/* render triangles */
	glDisable(GL_POLYGON_OFFSET_FILL);
	drawHeightFieldWireFrame();							/* render wireframe */
	glDisable(GL_BLEND);
}

void drawText(char *string, GLfloat z)
{
    glColor3f(1, 1, 0);
    glRasterPos3f( -0.9 , 0.9 , z);
    for(int i = 0; string[i] != '\0'; i++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, string[i]);
}

/*----------------------- Opengl Default function ------------------------------*/
void myinit()
{	
  	glClearColor(0.0, 0.0, 0.0, 0.0);/* setup gl view here */
  	glEnable(GL_DEPTH_TEST);         /* enable depth buffering */
  	glShadeModel(GL_SMOOTH);         /* interpolate colors during rasterization */   
}
/* reshape callback */
/* set projection to aspect ratio of window */
void reshape(int w, int h)
{
    GLfloat aspect = (GLfloat) w / (GLfloat) h;
    glViewport(0, 0, w, h);        /* scale viewport with window */

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(FOV, aspect, 1.0, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

/* draw 1x1 cube about origin */
/* replace this code with your height field implementation */
/* you may also want to precede it with your rotation/translation/scaling */
void display()
{

  	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);/* clear buffers */
  	glLoadIdentity();                                  /* reset transformation */
  	gluLookAt(-2,-2,1.0, 0.0,0.0,0.5, 0.0,0.0,1.0);/* set the camera position */
  	

  	/* draw text on screen 
  	*  NOTICE: put code ahead of tranformation or it will chage with transformation
  	*/
  	drawText("User Interaction Control    																	",1.0);  
  	drawText("esc: quit the program       																	",0.95);  
 		drawText("a  : left rotate            																	",0.90);
 		drawText("d  : right rotate           																	",0.85);
 		drawText("w  : move forward           																	",0.80);
 		drawText("s  : move backward          																	",0.75);
 		drawText("q  : zoom out               																	",0.70);
 		drawText("z  : zoom in                																	",0.65);
 		drawText("p  : point mode             																	",0.60);
 		drawText("l  : line mode              																	",0.55);
 		drawText("f  : triangle strip mode    																	",0.50);
 		drawText("m  : mesh mode              																	",0.45);
 		drawText("1  : save screenshot        																	",0.40);
 		drawText("2  : color vertives - single color      											",0.35);
 		drawText("3  : color vertives - multiple color    											",0.30);
 		drawText("4  : color vertives - from image        											",0.25); 		 		
 		drawText("5  : color vertives - texture mapping (in triangle strip mode)",0.20); 		
 		drawText("6  : stop animation                     											",0.15);
 		drawText("ctrl + mouse : translate                											",0.10);
 		drawText("shift + mouse: scale                    											",0.05);
 		drawText("mouse        : rotate                   											",0.00);

 		
  	
  	/* transformation */
  	glTranslatef(g_vLandTranslate[0], g_vLandTranslate[1], g_vLandTranslate[2]);
  	glRotatef(g_vLandRotate[0], 1.0, 0.0, 0.0);
  	glRotatef(g_vLandRotate[1], 0.0, 1.0, 0.0);
  	glRotatef(g_vLandRotate[2], 0.0, 0.0, 1.0);
  	glScalef(g_vLandScale[0], g_vLandScale[1], g_vLandScale[2]);
  	
  	
  	/* draw x-y-axes on top of floor */
  	/* use just depth buffer (instead of stencil buffer) */
  	glDepthMask(GL_FALSE);	                             /* set depth buffer to read-only */
  	drawFloor();			                                   /* draw floor into color buffer (only) */
  	glDepthMask(GL_TRUE);		                             /* set depth buffer to read-write */  
  	drawXYAxes();			                                   /* draw x-y-axes normally */
  	
  	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); /* set color buffer to read-only */
  	drawFloor();			                                   /* draw floor into depth-buffer (only) */
  	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);     /* set color buffer to read-write */  
  	
  	glTranslatef(0.0, 0.0, 0.01);	                       /* move up slightly to prevent floor overlap */
  	drawZAxis();			                                   /* draw z-axis normally */  
  	
  	if (COLOR_VERTICES == 2) {
   			glBegin(GL_TRIANGLE_STRIP);   // begin drawing a cube
   			//(note that the texture corners match quad corners)
   			// Front Face 
   			glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f/2, 1.0f, 1.0f);
   			glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f/2, 1.0f, 1.0f);
   			glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f/2, 1.0f, 0.0f);	
   			glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f/2, 1.0f, 0.0f);	 
   			glEnd(); // done with the Cube
    }
  	
  	
		switch(g_RenderState)
		{
		case POINTS:     
  	  drawHeightFieldPoints();   /* Points */
  	  break;
		case WIREFRAME:  
  	  drawHeightFieldWireFrame();/* Lines("Wireframe") */
			break;
		case TRIANGLES:  
  	  drawHeightFieldTriangles();/* Solid Triangles */
			break;
		case MESH:	     
			drawHeightFieldMesh();     /* Wireframe on top of solid triangles */
			break;	
		}
  	
  	glFlush();
  	glutSwapBuffers();																		/* double buffer flush */  
}

void doIdle()
{
  	/* do some stuff... */
		triggerSaveScreenshot();
		if(START_ANIMATION)
			animation();
  	/* make the screen update */
  	glutPostRedisplay();
}

/*-----------------------User Input function ------------------------------*/
void menufunc(int value)
{
  switch (value)
  {
    case 0:
      exit(0);
      break;
  }
}

/* converts mouse drags into information about rotation/translation/scaling */
void mousedrag(int x, int y)
{
  int vMouseDelta[2] = {x-g_vMousePos[0], y-g_vMousePos[1]};
  
  switch (g_ControlState)
  {
    case TRANSLATE:  
      if (g_iLeftMouseButton)
      {
        g_vLandTranslate[0] += vMouseDelta[0]*0.01;
        g_vLandTranslate[1] -= vMouseDelta[1]*0.01;
      }
      if (g_iMiddleMouseButton)
      {
        g_vLandTranslate[2] += vMouseDelta[1]*0.01;
      }
      break;
    case ROTATE:
      if (g_iLeftMouseButton)
      {
        g_vLandRotate[0] += vMouseDelta[1];/* drag up = clockwise around x-axis */
        g_vLandRotate[1] += vMouseDelta[0];/* drag right = counter-clockwise around y-axis */
      }
      if (g_iMiddleMouseButton)
      {
        g_vLandRotate[2] += vMouseDelta[1];/* drag right = counter-clockwise around z-axis */
      }
      break;
    case SCALE:
      if (g_iLeftMouseButton)
      {
        g_vLandScale[0] *= 1.0+vMouseDelta[0]*0.01;/* drag right = scale up in x-direction */
        g_vLandScale[1] *= 1.0-vMouseDelta[1]*0.01;/* drag up = scale up in y-direction */
      }
      if (g_iMiddleMouseButton)
      {
        g_vLandScale[2] *= 1.0-vMouseDelta[1]*0.01;/* drag up = scale up in z-direction */
      }
      break;
  }
  g_vMousePos[0] = x;
  g_vMousePos[1] = y;
}

void mouseidle(int x, int y)
{
  g_vMousePos[0] = x;
  g_vMousePos[1] = y;
}

/* mouse callback */
/* set state based on modifier key */
/* ctrl = translate, shift = scale, otherwise rotate */
void mousebutton(int button, int state, int x, int y)
{

  switch (button)
  {
    case GLUT_LEFT_BUTTON:
      g_iLeftMouseButton = (state==GLUT_DOWN);
      break;
    case GLUT_MIDDLE_BUTTON:
      g_iMiddleMouseButton = (state==GLUT_DOWN);
      break;
    case GLUT_RIGHT_BUTTON:
      g_iRightMouseButton = (state==GLUT_DOWN);
      break;
  }
 
  switch(glutGetModifiers())
  {
    case GLUT_ACTIVE_CTRL:
      g_ControlState = TRANSLATE;
      break;
    case GLUT_ACTIVE_SHIFT:
      g_ControlState = SCALE;
      break;
    default:
      g_ControlState = ROTATE;
      break;
  }

  g_vMousePos[0] = x;
  g_vMousePos[1] = y;
}


/* keyboard callback */
void keyboard(unsigned char key, int x, int y)
{
  switch (key) {
  case 27:
    exit(0);
    break;
	case '1'	:
		SAVE_SCREENSHOT = true;
		break; 
	case '2'	:
		COLOR_VERTICES = 0;glDisable(GL_TEXTURE_2D);
		break;
	case '3'	:
		COLOR_VERTICES = 1;glDisable(GL_TEXTURE_2D);
		break;
	case '4'	:
		COLOR_VERTICES = 3;
  	ReadTextures();glEnable(GL_TEXTURE_2D);         /* Enable Texture Mapping */		
		break;					
	case '5'	:
		COLOR_VERTICES = 2;
  	ReadTextures();glEnable(GL_TEXTURE_2D);         /* Enable Texture Mapping */		
		break;				
	case '6'	:
		START_ANIMATION = false;
		break;				   
  case 'f': case 'F':
    //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    g_RenderState = TRIANGLES;
    break;
  case 'l': case 'L':
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    g_RenderState = WIREFRAME;
    break;
  case 'p': case 'P':
    //glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
    g_RenderState = POINTS;
    break;
  case 'm': case 'M':
    g_RenderState = MESH;
    break;  
	case 'a': case 'A':
		g_vLandRotate[2] += 2; 
		break;		 
	case 'd': case 'D':
		g_vLandRotate[2] -=2; 
		break;			
	case 'w': case 'W':
		g_vLandTranslate[0] -=0.01;g_vLandTranslate[1] -=0.01;
		break;
	case 's': case 'S':
		g_vLandTranslate[0] +=0.01;g_vLandTranslate[1] +=0.01;		 
		break;					
	case 'e': case 'E':
		g_vLandRotate[1] += 2; 
		break;		 
	case 'c': case 'C':
		g_vLandRotate[1] -=2; 
		break;		
	case 'z': case 'Z':
		g_vLandScale[0] *= 1.2;g_vLandScale[1] *= 1.2;g_vLandScale[2] *= 1.2;
		break;
	case 'q': case 'Q':
		g_vLandScale[0] *= 0.8;g_vLandScale[1] *= 0.8;g_vLandScale[2] *= 0.8;
		break; 
			
  }
  glutPostRedisplay();
}

/*----------------------- main function ------------------------------*/
int main (int argc, char ** argv)
{
  if (argc<2)
  {  
    printf ("usage: %s heightfield.jpg\n", argv[0]);
    exit(1);
  }

  g_pHeightData = jpeg_read(argv[1], NULL);
  
  if (!g_pHeightData)
  {
    printf ("error reading %s.\n", argv[1]);
    exit(1);
  }
  
	/* create window */
  glutInit(&argc,argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_DEPTH | GLUT_RGBA);  
  glutInitWindowSize(WIDTH, HEIGHT);
  glutInitWindowPosition(0, 0);  
  glutCreateWindow("Assignment 1 - Height Fields");   
    
  /* allow the user to quit using the right mouse button menu */
  g_iMenuId = glutCreateMenu(menufunc);
  glutSetMenu(g_iMenuId);
  glutAddMenuEntry("Quit",0);
  glutAttachMenu(GLUT_RIGHT_BUTTON);
  
  /* replace with any animate code */
  glutIdleFunc(doIdle);

	/*  set GLUT callbacks */
  glutDisplayFunc(display);        /* display function to redraw */	
  glutMotionFunc(mousedrag);       /* callback for mouse drags */
  glutPassiveMotionFunc(mouseidle);/* callback for idle mouse */
  glutMouseFunc(mousebutton);      /* callback for mouse button */
  glutReshapeFunc(reshape);	       /* reshape callback */
  glutKeyboardFunc(keyboard);	     /* keyboard callback */
  
  /* do initialization */
  myinit();
  
	/* start GLUT program */
  glutMainLoop();
  return(0);
}
