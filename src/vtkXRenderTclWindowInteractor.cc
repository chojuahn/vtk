/*=========================================================================

  Program:   Visualization Library
  Module:    XInter.cc
  Language:  C++
  Date:      02/20/95
  Version:   1.10

This file is part of the Visualization Library. No part of this file
or its contents may be copied, reproduced or altered in any way
without the express written consent of the authors.

Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen 1993, 1994 

=========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/X.h>
#include <X11/keysym.h>
#include "vtkXRenderWindowInteractor.hh"
#include "vtkXRenderWindow.hh"
#include "vtkActor.hh"
#include <X11/Shell.h>
#include <math.h>
#include "tk.h"

#if (TK_MAJOR_VERSION == 4)&&(TK_MINOR_VERSION == 0)
extern "C" {
  extern int TkXFileProc(ClientData clientData, int mask, int flags);
}
#endif


#define TK_IS_DISPLAY   32


// returns 1 if done
static int vtkTclEventProc(XtPointer clientData,XEvent *event)
{
  Boolean ctd;
  vtkXRenderWindow *rw;
      
  rw = (vtkXRenderWindow *)
    (((vtkXRenderWindowInteractor *)clientData)->GetRenderWindow());
  
  if (rw->GetWindowId() == ((XAnyEvent *)event)->window)
    {
    vtkXRenderWindowInteractorCallback((Widget)NULL,clientData, event, &ctd);
    ctd = 0;
    }
  else
    {
    ctd = 1;
    }

  return !ctd;
}

static void vtkXTclTimerProc(ClientData clientData)
{
  XtIntervalId id;
  vtkXRenderWindowInteractorTimer((XtPointer)clientData,&id);
}

typedef struct
{
  Visual	*visual;
  int	depth;
} OptionsRec;
OptionsRec	Options;

XtResource resources[] =
{
	{"visual", "Visual", XtRVisual, sizeof (Visual *),
	XtOffsetOf (OptionsRec, visual), XtRImmediate, NULL},
	{"depth", "Depth", XtRInt, sizeof (int),
	XtOffsetOf (OptionsRec, depth), XtRImmediate, NULL},
};

XrmOptionDescRec Desc[] =
{
	{"-visual", "*visual", XrmoptionSepArg, NULL},
	{"-depth", "*depth", XrmoptionSepArg, NULL}
};


// states
#define VTKXI_START  0
#define VTKXI_ROTATE 1
#define VTKXI_ZOOM   2
#define VTKXI_PAN    3


// Description:
// Construct object so that light follows camera motion.
vtkXRenderWindowInteractor::vtkXRenderWindowInteractor()
{
  this->State = VTKXI_START;
  this->App = 0;
  this->top = 0;
}

vtkXRenderWindowInteractor::~vtkXRenderWindowInteractor()
{
}

void  vtkXRenderWindowInteractor::SetWidget(Widget foo)
{
  this->top = foo;
} 

void  vtkXRenderWindowInteractor::Start()
{
  Tk_MainLoop();
}

// Description:
// Initializes the event handlers
void vtkXRenderWindowInteractor::Initialize(XtAppContext app)
{
  this->App = app;

  this->Initialize();
}

// Description:
// Begin processing keyboard strokes.
void vtkXRenderWindowInteractor::Initialize()
{
  static int any_initialized = 0;
  static XtAppContext app;
  vtkXRenderWindow *ren;
  int depth;
  Colormap cmap;
  Visual  *vis;
  int *size;
  int *position;
  int argc = 0;

  // make sure we have a RenderWindow and camera
  if ( ! this->RenderWindow)
    {
    vtkErrorMacro(<<"No renderer defined!");
    return;
    }

  this->Initialized = 1;
  ren = (vtkXRenderWindow *)(this->RenderWindow);

  // do initialization stuff if not initialized yet
  // do we already have a display
  this->DisplayId = ren->GetDisplayId();
  if (!this->DisplayId)
    {
    if (this->App)
      {
      any_initialized = 1;
      app = this->App;
      }
    if (!any_initialized)
      {
      XtToolkitInitialize();
      app = XtCreateApplicationContext();
      any_initialized = 1;
      }
    this->App = app;
    
    this->DisplayId = 
      XtOpenDisplay(this->App,NULL,"VTK","vtk",NULL,0,&argc,NULL);
    }
  
  // get the info we need from the RenderingWindow
  ren->SetDisplayId(this->DisplayId);
  depth   = ren->GetDesiredDepth();
  cmap    = ren->GetDesiredColormap();
  vis     = ren->GetDesiredVisual();
  size    = ren->GetSize();
  position= ren->GetPosition();
  
  // do we have a window already
  this->WindowId = ren->GetWindowId();
  if (!this->WindowId)
    {
    this->top = XtVaAppCreateShell(this->RenderWindow->GetName(),"vtk",
				   applicationShellWidgetClass,
				   this->DisplayId,
				   XtNdepth, depth,
				   XtNcolormap, cmap,
				   XtNvisual, vis,
				   XtNx, position[0],
				   XtNy, position[1],
				   XtNwidth, size[0],
				   XtNheight, size[1],
				   XtNinput, True,
				   XtNmappedWhenManaged, 0,
				   NULL);
    
    XtRealizeWidget(this->top);

    /* add callback */
    XSync(this->DisplayId,False);
    ren->SetWindowId(XtWindow(this->top));
    this->WindowId = XtWindow(this->top);
    }
  
  size = ren->GetSize();
  ren->Render();
  size = ren->GetSize();

  this->Size[0] = size[0];
  this->Size[1] = size[1];

  XSelectInput(this->DisplayId,this->WindowId,
	       
	       KeyPressMask | ButtonPressMask | ExposureMask |
		    StructureNotifyMask | ButtonReleaseMask);

  // add in tcl init stuff
#if (TK_MAJOR_VERSION == 3)
  Tk_CreateFileHandler(ConnectionNumber(this->DisplayId),
		       TK_READABLE|TK_IS_DISPLAY, 
		       (void (*)(void *,int)) NULL,
		       (ClientData) this->DisplayId);
#else
#if (TK_MINOR_VERSION == 0)
  Tk_CreateFileHandler2(ConnectionNumber(this->DisplayId), TkXFileProc,
			(ClientData) this->DisplayId);
#else
  Tk_NotifyDisplay(this->DisplayId);
#endif
#endif

  Tk_CreateGenericHandler((Tk_GenericProc *)vtkTclEventProc,(ClientData)this);
}

void vtkXRenderWindowInteractor::PrintSelf(ostream& os, vtkIndent indent)
{
  vtkRenderWindowInteractor::PrintSelf(os,indent);
}


void  vtkXRenderWindowInteractor::UpdateSize(int x,int y)
{
  // if the size changed send this on to the RenderWindow
  if ((x != this->Size[0])||(y != this->Size[1]))
    {
    this->Size[0] = x;
    this->Size[1] = y;
    this->RenderWindow->SetSize(x,y);
    }

}
 
void  vtkXRenderWindowInteractor::StartRotate()
{
  if (this->State != VTKXI_START) return;
  this->State = VTKXI_ROTATE;

  this->RenderWindow->SetDesiredUpdateRate(this->DesiredUpdateRate);
  Tk_CreateTimerHandler(10,vtkXTclTimerProc,(ClientData)this);
}
void  vtkXRenderWindowInteractor::EndRotate()
{
  if (this->State != VTKXI_ROTATE) return;
  this->State = VTKXI_START;
  this->RenderWindow->SetDesiredUpdateRate(this->StillUpdateRate);
  this->RenderWindow->Render();
}

void  vtkXRenderWindowInteractor::StartZoom()
{
  if (this->State != VTKXI_START) return;
  this->State = VTKXI_ZOOM;
  this->RenderWindow->SetDesiredUpdateRate(this->DesiredUpdateRate);
  Tk_CreateTimerHandler(10,vtkXTclTimerProc,(ClientData)this);
}
void  vtkXRenderWindowInteractor::EndZoom()
{
  if (this->State != VTKXI_ZOOM) return;
  this->State = VTKXI_START;
  this->RenderWindow->SetDesiredUpdateRate(this->StillUpdateRate);
  this->RenderWindow->Render();
}

void  vtkXRenderWindowInteractor::StartPan()
{
  float *FocalPoint;
  float *Result;

  if (this->State != VTKXI_START) return;

  this->State = VTKXI_PAN;

  // calculate the focal depth since we'll be using it a lot
  FocalPoint = this->CurrentCamera->GetFocalPoint();
      
  this->CurrentRenderer->SetWorldPoint(FocalPoint[0],FocalPoint[1],
				       FocalPoint[2],1.0);
  this->CurrentRenderer->WorldToDisplay();
  Result = this->CurrentRenderer->GetDisplayPoint();
  this->FocalDepth = Result[2];

  this->RenderWindow->SetDesiredUpdateRate(this->DesiredUpdateRate);
  Tk_CreateTimerHandler(10,vtkXTclTimerProc,(ClientData)this);
}
void  vtkXRenderWindowInteractor::EndPan()
{
  if (this->State != VTKXI_PAN) return;
  this->State = VTKXI_START;
  this->RenderWindow->SetDesiredUpdateRate(this->StillUpdateRate);
  this->RenderWindow->Render();
}

void vtkXRenderWindowInteractorCallback(Widget w,XtPointer client_data, 
				    XEvent *event, Boolean *ctd)
{
  int *size;
  vtkXRenderWindowInteractor *me;

  me = (vtkXRenderWindowInteractor *)client_data;

  switch (event->type) 
    {
    case Expose : me->GetRenderWindow()->Render(); break;
      
    case ConfigureNotify : 
      me->UpdateSize(((XConfigureEvent *)event)->width,
		     ((XConfigureEvent *)event)->height); 
      me->GetRenderWindow()->Render(); 
      break;

    case ButtonPress : 
      {
      switch (((XButtonEvent *)event)->button)
	{
	case Button1 : 
          me->FindPokedCamera(((XButtonEvent*)event)->x,
			      me->Size[1] - ((XButtonEvent*)event)->y);
	  me->StartRotate(); 
	  break;
	case Button2 : 
          me->FindPokedCamera(((XButtonEvent*)event)->x,
			      me->Size[1] - ((XButtonEvent*)event)->y);
	  me->StartPan(); 
	  break;
	case Button3 : 
          me->FindPokedCamera(((XButtonEvent*)event)->x,
			      me->Size[1] - ((XButtonEvent*)event)->y);
	  me->StartZoom(); 
	  break;
	}
      }
      break;

    case ButtonRelease : 
      {
      switch (((XButtonEvent *)event)->button)
	{
	case Button1 : me->EndRotate(); break;
	case Button2 : me->EndPan(); break;
	case Button3 : me->EndZoom(); break;
	}
      }
      break;

    case KeyPress :
      {
      KeySym ks;
      static char buffer[20];

      XLookupString((XKeyEvent *)event,buffer,20,&ks,NULL);
      switch (ks)
	{
	case XK_e : exit(1); break;
	case XK_u :
	  if (me->UserMethod) (*me->UserMethod)(me->UserMethodArg);
	  break;
	case XK_r : //reset
	  {
          me->FindPokedRenderer(((XKeyEvent*)event)->x,
			        me->Size[1] - ((XKeyEvent*)event)->y);
	  me->CurrentRenderer->ResetCamera();
	  me->RenderWindow->Render();
          }
	  break;

	case XK_w : //change all actors to wireframe
	  {
	  vtkActorCollection *ac;
	  vtkActor *anActor;
	  
          me->FindPokedRenderer(((XKeyEvent*)event)->x,
				me->Size[1] - ((XKeyEvent*)event)->y);
	  ac = me->CurrentRenderer->GetActors();
	  for (ac->InitTraversal(); anActor = ac->GetNextItem(); )
	    {
	    anActor->GetProperty()->SetWireframe();
	    }
	  
	  me->RenderWindow->Render();
	  }
	  break;

	case XK_s : //change all actors to "surface" or solid
	  {
	  vtkActorCollection *ac;
	  vtkActor *anActor;
	  
          me->FindPokedRenderer(((XKeyEvent*)event)->x,
			        me->Size[1] - ((XKeyEvent*)event)->y);
	  ac = me->CurrentRenderer->GetActors();
	  for (ac->InitTraversal(); anActor = ac->GetNextItem(); )
	    {
	    anActor->GetProperty()->SetSurface();
	    }
	  
	  me->RenderWindow->Render();
          }
	  break;

	case XK_3 : //3d stereo
	  {
	  // prepare the new window
	  if (me->RenderWindow->GetStereoRender())
	    {
	    if (me->RenderWindow->GetRemapWindow())
	      {
	      me->SetupNewWindow(1);
	      }
	    me->RenderWindow->StereoRenderOff();
	    }
	  else
	    {
	    memcpy(me->PositionBeforeStereo,me->RenderWindow->GetPosition(),
		   sizeof(int)*2);
	    if (me->RenderWindow->GetRemapWindow())
	      {
	      me->SetupNewWindow(1);
	      }
	    me->RenderWindow->StereoRenderOn();
	    }
	  me->RenderWindow->Render();
	  if (me->RenderWindow->GetRemapWindow())
	    {
	    me->FinishSettingUpNewWindow();
	    }
          }
	  break;

	case XK_p : //pick actors
	  {
          me->FindPokedRenderer(((XKeyEvent*)event)->x,
			        me->Size[1] - ((XKeyEvent*)event)->y);
          // Execute start method, if any

          if ( me->StartPickMethod ) 
            (*me->StartPickMethod)(me->StartPickMethodArg);
          me->Picker->Pick(((XButtonEvent*)event)->x,
                             me->Size[1] - ((XButtonEvent*)event)->y, 0.0,
                             me->CurrentRenderer);
          me->HighlightActor(me->Picker->GetActor());
          if ( me->EndPickMethod ) 
            (*me->EndPickMethod)(me->EndPickMethodArg);
          }
	  break;
        }
      }
      break;
    }
}

void vtkXRenderWindowInteractorTimer(XtPointer client_data,XtIntervalId *id)
{
  vtkXRenderWindowInteractor *me;
  Window root,child;
  int root_x,root_y;
  int x,y;
  float xf,yf;
  unsigned int keys;

  me = (vtkXRenderWindowInteractor *)client_data;

  switch (me->State)
    {
    case VTKXI_ROTATE :
      // get the pointer position
      XQueryPointer(me->DisplayId,me->WindowId,
		    &root,&child,&root_x,&root_y,&x,&y,&keys);
      xf = (x - me->Center[0]) * me->DeltaAzimuth;
      yf = ((me->Size[1] - y) - me->Center[1]) * me->DeltaElevation;
      me->CurrentCamera->Azimuth(xf);
      me->CurrentCamera->Elevation(yf);
      me->CurrentCamera->OrthogonalizeViewUp();
      if (me->LightFollowCamera)
	{
	/* get the first light */
	me->CurrentLight->SetPosition(me->CurrentCamera->GetPosition());
	me->CurrentLight->SetFocalPoint(me->CurrentCamera->GetFocalPoint());
	}
      me->RenderWindow->Render();
      Tk_CreateTimerHandler(10,vtkXTclTimerProc,(ClientData)client_data);
      break;
    case VTKXI_PAN :
      {
      float  FPoint[3];
      float *PPoint;
      float  APoint[3];
      float  RPoint[4];

      // get the current focal point and position
      memcpy(FPoint,me->CurrentCamera->GetFocalPoint(),sizeof(float)*3);
      PPoint = me->CurrentCamera->GetPosition();

      // get the pointer position
      XQueryPointer(me->DisplayId,me->WindowId,
		    &root,&child,&root_x,&root_y,&x,&y,&keys);

      APoint[0] = x;
      APoint[1] = me->Size[1] - y;
      APoint[2] = me->FocalDepth;
      me->CurrentRenderer->SetDisplayPoint(APoint);
      me->CurrentRenderer->DisplayToWorld();
      memcpy(RPoint,me->CurrentRenderer->GetWorldPoint(),sizeof(float)*4);
      if (RPoint[3])
	{
	RPoint[0] = RPoint[0]/RPoint[3];
	RPoint[1] = RPoint[1]/RPoint[3];
	RPoint[2] = RPoint[2]/RPoint[3];
	}

      /*
       * Compute a translation vector, moving everything 1/10 
       * the distance to the cursor. (Arbitrary scale factor)
       */
      me->CurrentCamera->SetFocalPoint(
	(FPoint[0]-RPoint[0])/10.0 + FPoint[0],
	(FPoint[1]-RPoint[1])/10.0 + FPoint[1],
	(FPoint[2]-RPoint[2])/10.0 + FPoint[2]);
      me->CurrentCamera->SetPosition(
	(FPoint[0]-RPoint[0])/10.0 + PPoint[0],
	(FPoint[1]-RPoint[1])/10.0 + PPoint[1],
	(FPoint[2]-RPoint[2])/10.0 + PPoint[2]);
      
      me->RenderWindow->Render();
      Tk_CreateTimerHandler(10,vtkXTclTimerProc,(ClientData)client_data);
      }
      break;
    case VTKXI_ZOOM :
      {
      float zoomFactor;
      float *clippingRange;

      // get the pointer position
      XQueryPointer(me->DisplayId,me->WindowId,
		    &root,&child,&root_x,&root_y,&x,&y,&keys);
      yf = ((me->Size[1] - y) - me->Center[1])/(float)me->Center[1];
      zoomFactor = pow(1.1,yf);
      clippingRange = me->CurrentCamera->GetClippingRange();
      me->CurrentCamera->SetClippingRange(clippingRange[0]/zoomFactor,
      					  clippingRange[1]/zoomFactor);
      me->CurrentCamera->Dolly(zoomFactor);
      me->RenderWindow->Render();
      Tk_CreateTimerHandler(10,vtkXTclTimerProc,(ClientData)client_data);
      }
      break;
    }
}  



// Description:
// Setup a new window before a WindowRemap
void vtkXRenderWindowInteractor::SetupNewWindow(int Stereo)
{
  vtkXRenderWindow *ren;
  int depth;
  Colormap cmap;
  Visual  *vis;
  int *size;
  int *position;
  int zero_pos[2];
  
  // get the info we need from the RenderingWindow
  ren = (vtkXRenderWindow *)(this->RenderWindow);
  this->DisplayId = ren->GetDisplayId();
  depth   = ren->GetDesiredDepth();
  cmap    = ren->GetDesiredColormap();
  vis     = ren->GetDesiredVisual();
  size    = ren->GetSize();
  position= ren->GetPosition();

  if (Stereo)
    {
    if (this->RenderWindow->GetStereoRender())
      {
      position = this->PositionBeforeStereo;
      }
    else
      {
      zero_pos[0] = 0;
      zero_pos[1] = 0;
      position = zero_pos;
      }
    }

  this->oldTop = this->top;
  
  this->top = XtVaAppCreateShell(this->RenderWindow->GetName(),"vtk",
				 applicationShellWidgetClass,
				 this->DisplayId,
				 XtNdepth, depth,
				 XtNcolormap, cmap,
				 XtNvisual, vis,
				 XtNx, position[0],
				 XtNy, position[1],
				 XtNwidth, size[0],
				 XtNheight, size[1],
				 XtNmappedWhenManaged, 0,
				 NULL);

  XtRealizeWidget(this->top);

  /* add callback */
  XSync(this->DisplayId,False);
  ren->SetNextWindowId(XtWindow(this->top));
  this->WindowId = XtWindow(this->top);
}

// Description:
// Finish setting up a new window after the WindowRemap.
void vtkXRenderWindowInteractor::FinishSettingUpNewWindow()
{
  int *size;

  // free the previous widget
  XtDestroyWidget(this->oldTop);
  XSync(this->DisplayId,False);

  XSelectInput(this->DisplayId,this->WindowId,
		    KeyPressMask | ButtonPressMask | ExposureMask |
		    StructureNotifyMask | ButtonReleaseMask);

  size = this->RenderWindow->GetSize();
  this->Size[0] = size[0];
  this->Size[1] = size[1];
}

