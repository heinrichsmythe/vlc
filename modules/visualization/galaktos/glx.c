/*****************************************************************************
 * glx.c:
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id$
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
 *          based on Scivi http://xmms-scivi.sourceforge.net
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include "glx.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>

/* Local prototypes */

static int InitGLX12( galaktos_thread_t *p_thread );
static int InitGLX13( galaktos_thread_t *p_thread );
static void CreateWindow( galaktos_thread_t *p_thread, XVisualInfo *p_vi );


typedef struct
{
    Display     *p_display;
    int         b_glx13;
    GLXContext  gwctx;
    Window      wnd;
    GLXWindow   gwnd;
    GLXPbuffer  gpbuf;
    GLXContext  gpctx;
    Atom        wm_delete;
}
glx_data_t;
#define OS_DATA ((glx_data_t*)(p_thread->p_os_data))


int galaktos_glx_init( galaktos_thread_t *p_thread )
{
    Display *p_display;
    int i_opcode, i_evt, i_err;
    int i_maj, i_min;

    /* Initialize OS data */
    p_thread->p_os_data = malloc( sizeof( glx_data_t ) );

    /* Open the display */
    OS_DATA->p_display = p_display = XOpenDisplay( NULL );
    if( !p_display )
    {
        msg_Err( p_thread, "Cannot open display" );
        return -1;
    }

    /* Check for GLX extension */
    if( !XQueryExtension( p_display, "GLX", &i_opcode, &i_evt, &i_err ) )
    {
        msg_Err( p_thread, "GLX extension not supported" );
        return -1;
    }
    if( !glXQueryExtension( p_display, &i_err, &i_evt ) )
    {
        msg_Err( p_thread, "glXQueryExtension failed" );
        return -1;
    }

    /* Check GLX version */
    if (!glXQueryVersion( p_display, &i_maj, &i_min ) )
    {
        msg_Err( p_thread, "glXQueryVersion failed" );
        return -1;
    }
    if( i_maj <= 0 || ((i_maj == 1) && (i_min < 3)) )
    {
        OS_DATA->b_glx13 = 0;
        msg_Info( p_thread, "Using GLX 1.2 API" );
        if( InitGLX12( p_thread ) == -1 )
        {
            return -1;
        }
    }
    else
    {
        OS_DATA->b_glx13 = 1;
        msg_Info( p_thread, "Using GLX 1.3 API" );
        if( InitGLX13( p_thread ) == -1 )
        {
            return -1;
        }
    }

    XMapWindow( p_display, OS_DATA->wnd );
    if( p_thread->b_fullscreen )
    {
        //XXX
        XMoveWindow( p_display, OS_DATA->wnd, 0, 0 );
    }
    XFlush( p_display );

    return 0;
}


int galaktos_glx_handle_events( galaktos_thread_t *p_thread )
{
    Display *p_display;

    p_display = OS_DATA->p_display;

    /* loop on X11 events */
    while( XPending( p_display ) > 0 )
    {
        XEvent evt;
        XNextEvent( p_display, &evt );
        switch( evt.type )
        {
            case ClientMessage:
            {
                /* Delete notification */
                if( (evt.xclient.format == 32) &&
                    ((Atom)evt.xclient.data.l[0] == OS_DATA->wm_delete) )
                {
                    return 1;
                }
                break;
            }
        }
    }
    return 0;
}


void galaktos_glx_activate_pbuffer( galaktos_thread_t *p_thread )
{

    glXMakeContextCurrent( OS_DATA->p_display, OS_DATA->gpbuf, OS_DATA->gpbuf,
                           OS_DATA->gpctx );
}


void galaktos_glx_activate_window( galaktos_thread_t *p_thread )
{
    if( OS_DATA->b_glx13 )
    {
        glXMakeContextCurrent( OS_DATA->p_display, OS_DATA->gwnd, OS_DATA->gwnd,
                               OS_DATA->gwctx );
    }
    else
    {
        glXMakeCurrent( OS_DATA->p_display, OS_DATA->wnd, OS_DATA->gwctx );
    }
}


void galaktos_glx_swap( galaktos_thread_t *p_thread )
{
    if( OS_DATA->b_glx13 )
    {
        glXSwapBuffers( OS_DATA->p_display, OS_DATA->gwnd );
    }
    else
    {
        glXSwapBuffers( OS_DATA->p_display, OS_DATA->wnd );
    }
}


void galaktos_glx_done( galaktos_thread_t *p_thread )
{
    Display *p_display;

    p_display = OS_DATA->p_display;
    if( OS_DATA->b_glx13 )
    {
        glXDestroyContext( p_display, OS_DATA->gpctx );
        glXDestroyPbuffer( p_display, OS_DATA->gpbuf );
    }
    glXDestroyContext( p_display, OS_DATA->gwctx );
    if( OS_DATA->b_glx13 )
    {
        glXDestroyWindow( p_display, OS_DATA->gwnd );
    }
    XDestroyWindow( p_display, OS_DATA->wnd );
    XCloseDisplay( p_display );
}


int InitGLX12( galaktos_thread_t *p_thread )
{
    Display *p_display;
    XVisualInfo *p_vi;
    GLXContext gwctx;
    int p_attr[] = { GLX_RGBA, GLX_RED_SIZE, 5, GLX_GREEN_SIZE, 5,
                     GLX_BLUE_SIZE, 5, GLX_DOUBLEBUFFER,
                     0 };

    p_display = OS_DATA->p_display;

    p_vi = glXChooseVisual( p_display, DefaultScreen( p_display), p_attr );
    if(! p_vi )
    {
        msg_Err( p_thread, "Cannot get GLX 1.2 visual" );
        return -1;
    }

    /* Create the window */
    CreateWindow( p_thread, p_vi );

     /* Create an OpenGL context */
    OS_DATA->gwctx = gwctx = glXCreateContext( p_display, p_vi, 0, True );
    if( !gwctx )
    {
        msg_Err( p_thread, "Cannot create OpenGL context");
        XFree( p_vi );
        return -1;
    }
    XFree( p_vi );

    return 0;
}


int InitGLX13( galaktos_thread_t *p_thread )
{
    Display *p_display;
    int i_nbelem;
    GLXFBConfig *p_fbconfs, fbconf;
    XVisualInfo *p_vi;
    GLXContext gwctx;
    int i;
    GLXPbuffer gpbuf;
    int p_attr[] = { GLX_RED_SIZE, 5, GLX_GREEN_SIZE, 5,
                     GLX_BLUE_SIZE, 5, GLX_DOUBLEBUFFER, True,
                     GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, 0 };

    p_display = OS_DATA->p_display;

    /* Get the FB configuration */
    p_fbconfs = glXChooseFBConfig( p_display, 0, p_attr, &i_nbelem );
    if( (i_nbelem <= 0) || !p_fbconfs )
    {
        msg_Err( p_thread, "Cannot get FB configurations");
        if( p_fbconfs )
        {
            XFree( p_fbconfs );
        }
        return -1;
    }
    fbconf = p_fbconfs[0];

    /* Get the X11 visual */
    p_vi = glXGetVisualFromFBConfig( p_display, fbconf );
    if( !p_vi )
    {
        msg_Err( p_thread, "Cannot get X11 visual" );
        XFree( p_fbconfs );
        return -1;
    }

    /* Create the window */
    CreateWindow( p_thread, p_vi );

    XFree( p_vi );

    /* Create the GLX window */
    OS_DATA->gwnd = glXCreateWindow( p_display, fbconf, OS_DATA->wnd, NULL );
    if( OS_DATA->gwnd == None )
    {
        msg_Err( p_thread, "Cannot create GLX window" );
        return -1;
    }

    /* Create an OpenGL context */
    OS_DATA->gwctx = gwctx = glXCreateNewContext( p_display, fbconf,
                                                  GLX_RGBA_TYPE, NULL, True );
    if( !gwctx )
    {
        msg_Err( p_thread, "Cannot create OpenGL context");
        XFree( p_fbconfs );
        return -1;
    }
    XFree( p_fbconfs );

    /* Get a FB config for the pbuffer */
    p_attr[1] = 8;                  // RED_SIZE
    p_attr[3] = 8;                  // GREEN_SIZE
    p_attr[5] = 8;                  // BLUE_SIZE
    p_attr[7] = False;              // DOUBLEBUFFER
    p_attr[9] = GLX_PBUFFER_BIT;    // DRAWABLE_TYPE
    p_fbconfs = glXChooseFBConfig( p_display, 0, p_attr, &i_nbelem );
    if( (i_nbelem <= 0) || !p_fbconfs )
    {
        msg_Err( p_thread, "Cannot get FB configurations for pbuffer");
        if( p_fbconfs )
        {
            XFree( p_fbconfs );
        }
        return -1;
    }
    fbconf = p_fbconfs[0];

    /* Create a pbuffer */
    i = 0;
    p_attr[i++] = GLX_PBUFFER_WIDTH;
    p_attr[i++] = 512;
    p_attr[i++] = GLX_PBUFFER_HEIGHT;
    p_attr[i++] = 512;
    p_attr[i++] = GLX_PRESERVED_CONTENTS;
    p_attr[i++] = True;
    p_attr[i++] = 0;
    OS_DATA->gpbuf = gpbuf = glXCreatePbuffer( p_display, fbconf, p_attr );
    if( !gpbuf )
    {
        msg_Err( p_thread, "Failed to create GLX pbuffer" );
        XFree( p_fbconfs );
        return -1;
    }

    /* Create the pbuffer context */
    OS_DATA->gpctx = glXCreateNewContext( p_display, fbconf, GLX_RGBA_TYPE,
                                          gwctx, True );
    if( !OS_DATA->gpctx )
    {
        msg_Err( p_thread, "Failed to create pbuffer context" );
        XFree( p_fbconfs );
        return -1;
    }

    XFree( p_fbconfs );

    return 0;
}



void CreateWindow( galaktos_thread_t *p_thread, XVisualInfo *p_vi )
{
    Display *p_display;
    XSetWindowAttributes xattr;
    Window wnd;
    Colormap cm;
    XSizeHints* p_size_hints;
    Atom prop;
    mwmhints_t mwmhints;

    p_display = OS_DATA->p_display;

    /* Create a colormap */
    cm = XCreateColormap( p_display, RootWindow( p_display, p_vi->screen ),
                          p_vi->visual, AllocNone );

    /* Create the window */
    xattr.background_pixel = BlackPixel( p_display, DefaultScreen(p_display) );
    xattr.border_pixel = 0;
    xattr.colormap = cm;
    OS_DATA->wnd = wnd = XCreateWindow( p_display, DefaultRootWindow(p_display),
            0, 0, p_thread->i_width, p_thread->i_height, 0, p_vi->depth,
            InputOutput, p_vi->visual,
            CWBackPixel | CWBorderPixel | CWColormap, &xattr);

    /* Allow the window to be deleted by the window manager */
    OS_DATA->wm_delete = XInternAtom( p_display, "WM_DELETE_WINDOW", False );
    XSetWMProtocols( p_display, wnd, &OS_DATA->wm_delete, 1 );

    if( p_thread->b_fullscreen )
    {
        mwmhints.flags = MWM_HINTS_DECORATIONS;
        mwmhints.decorations = False;

        prop = XInternAtom( p_display, "_MOTIF_WM_HINTS", False );
        XChangeProperty( p_display, wnd, prop, prop, 32, PropModeReplace,
                         (unsigned char *)&mwmhints, PROP_MWM_HINTS_ELEMENTS );
    }
    else
    {
        /* Prevent the window from being resized */
        p_size_hints = XAllocSizeHints();
        p_size_hints->flags = PMinSize | PMaxSize;
        p_size_hints->min_width = p_thread->i_width;
        p_size_hints->min_height = p_thread->i_height;
        p_size_hints->max_width = p_thread->i_width;
        p_size_hints->max_height = p_thread->i_height;
        XSetWMNormalHints( p_display, wnd, p_size_hints );
        XFree( p_size_hints );
    }
    XSelectInput( p_display, wnd, KeyPressMask );
}
