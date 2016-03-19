#include "vnoc_gui.h"

////////////////////////////////////////////////////////////////////////////////
//
// statics
//
////////////////////////////////////////////////////////////////////////////////

static Bool test_if_exposed (Display *disp, XEvent *event_ptr, XPointer dummy) 
{
    // Returns True if the event passed in is an exposure event.   Note that 
    // the bool type returned by this function is defined in Xlib.h.
    if ( event_ptr->type == Expose) {
        return (True);
    }
    return (False);
}

////////////////////////////////////////////////////////////////////////////////
//
// drawing
//
////////////////////////////////////////////////////////////////////////////////

void GUI_GRAPHICS::set_graphics_state( bool show_graphics_val, int wfui_automode_val) 
{
    // Sets the static show_graphics and wait_for_user_input_automode variables to the   
    // desired values.  They control if graphics are enabled and, if so,
    // how often the user is prompted for input.
    _is_gui_usable = show_graphics_val;
    _wait_for_user_input_automode = wfui_automode_val;
}

void GUI_GRAPHICS::update_screen( int priority, char *msg, 
    PICTURE_TYPE pic_on_screen_val) 
{
    // Updates the screen if the user has requested graphics.  The priority  
    // value controls whether or not the Proceed button must be clicked to   
    // continue.  Saves the pic_on_screen_val to allow pan and zoom redraws.
    if ( !_is_gui_usable)
        return;

    // If it's the type of picture displayed has changed, set up the proper
    // buttons.
    if ( _pic_on_screen != pic_on_screen_val) {
        if ( pic_on_screen_val == ROUTERS && _pic_on_screen == NO_PICTURE) {
            create_button( "Window", "Toggle Links", TOGGLE_LINKS);
        }
    }

    // Save the main message.
    strncpy( _default_message, msg, BUFFER_SIZE);
    _pic_on_screen = pic_on_screen_val;
    update_message( msg); 
    drawscreen();

    if ( priority >= _wait_for_user_input_automode) { // priority can be 0,1,2;
        event_loop();
    } else {
        flushinput();
    }
}

void GUI_GRAPHICS::drawscreen (void) 
{
    // This is the screen redrawing routine that event_loop assumes exists.  
    // It erases whatever is on screen, then calls redraw_screen to redraw   
    // it.

    clearscreen();
    redraw_screen();
}

void GUI_GRAPHICS::redraw_screen (void) 
{
    // The screen redrawing routine called by drawscreen and           
    // highlight_blocks.  Call this routine instead of drawscreen if   
    // you know you don't need to erase the current graphics, and want 
    // to avoid a screen "flash".

    if ( _pic_on_screen == ROUTERS) {

        // (a) first bring in the info about the occupancy of routers in the
        // network;
        long routers_count = _vnoc->routers_count();
        long physical_ports_count = 1 + 2 * _topology->cube_size();
        long vc_number = _topology->virtual_channel_number();

        // (b) first go thru all routers and compute current_max_router_occupancy;
        float current_max_router_occupancy = 0;
        for ( long r_i = 0; r_i < routers_count; r_i++) {
            ROUTER *this_router = _vnoc->router( r_i);
            long this_router_occ = 0;

            // calculate on the fly how many flits occupy currently this rouyer;
            for ( long i = 0; i < physical_ports_count; i ++) {
                for ( long j = 0; j < vc_number; j ++) {
                    long this_r_input_occ =
                        this_router->input().input_buff(i,j).size();
                    this_router_occ += (this_r_input_occ > 0) ? this_r_input_occ : 0;
                }
                long this_r_output_occ =
                    this_router->output().out_buffer(i).size();
                this_router_occ += (this_r_output_occ > 0) ? this_r_output_occ : 0; 
            }
            // record it as well;
            _router_occupancy[ r_i] = this_router_occ;
            if ( this_router_occ > current_max_router_occupancy) {
                current_max_router_occupancy = this_router_occ;
            }
        }
        current_max_router_occupancy = max(current_max_router_occupancy, 1);
        
        // (c) set up colors for routers based on occupancy;
        for ( long r_i = 0; r_i < routers_count; r_i++) {
            // before I used _max_router_occupancy, but all routers were same color...
            // for low congestion in entire network;
            float occ_fraction = _router_occupancy[ r_i] / current_max_router_occupancy;

            if ( occ_fraction >= 0.8 && occ_fraction <= 1.0) {
                _router_color[ r_i] = BLACK;
            } else if ( occ_fraction >= 0.6 && occ_fraction < 0.8) {
                _router_color[ r_i] = DARKGREY;
            } else if ( occ_fraction >= 0.4 && occ_fraction < 0.6) {
                _router_color[ r_i] = LIGHTGREY;
            } else if ( occ_fraction >= 0.2 && occ_fraction < 0.4) {
                _router_color[ r_i] = GREY35;
            } else if ( occ_fraction > 0.0 && occ_fraction < 0.2) {
                _router_color[ r_i] = GREY15;
            } else if ( occ_fraction == 0.0) {
                _router_color[ r_i] = WHITE;
            } else {
                assert(0); // impossible;
            }
        }

        // (b) do the drawing with the updated occupancies;
        draw_routers();
    }
}

void GUI_GRAPHICS::toggle_links(void) 
{
    // Enables/disables drawing of nets when a the user clicks on a button.
    // Also disables drawing of routing resources.  See graphics.c for details
    // of how buttons work.

    _show_links = !_show_links;
    _show_congestion = false;

    update_message( _default_message);
    drawscreen();
}

void GUI_GRAPHICS::alloc_draw_structs (void) 
{
    // Allocate the structures needed to draw things
    // Set up the default colors
    long routers_count = _vnoc->routers_count();
    
    _x_router_left.resize( routers_count);
    _y_router_bottom.resize( routers_count);
    _router_color.resize( routers_count);
    _router_occupancy.resize( routers_count);
    _router_has_PE.resize( routers_count);

    deselect_all();
}
void GUI_GRAPHICS::deselect_all (void) 
{
    // Sets the color of all to the default.
    long routers_count = _vnoc->routers_count();
    for ( long i = 0; i < routers_count; i++) {
        _router_color[i] = LIGHTGREY;
    }
}
void GUI_GRAPHICS::init_draw_coords ( float router_width_val, 
    float channel_width_val)
{
    // Load the arrays containing the left and bottom coordinates of the routers
    if ( !_is_gui_usable)
        return;
    long routers_count = _vnoc->routers_count();
    long i = 0;

    long nx = _topology->ary_size();
    long ny = nx;
    // set now numerical values of interest for dimensions and coloring
    // to represent congestion;
    _router_width = router_width_val;
    _channel_width = channel_width_val;
    long physical_ports_count = 1 + 2 * _topology->cube_size();
    long vc_number = _topology->virtual_channel_number();
    long in_out_buffer_size = _topology->input_buffer_size() +
        _topology->output_buffer_size();
    _max_router_occupancy = physical_ports_count * vc_number * in_out_buffer_size;

    _x_router_left[0] = 0.;
    for ( i = 1; i < nx; i++) {
        _x_router_left[i] = _x_router_left[i-1] + _router_width + _channel_width + 1.;
    }

    _y_router_bottom[0] = 0.;
    for ( i = 1; i < ny; i++) {
        _y_router_bottom[i] = _y_router_bottom[i-1] + _router_width + _channel_width + 1.;
    }

    for ( i = 0; i < routers_count; i++) {
        _router_occupancy[i] = 0;
        _router_has_PE[i] = true;
    }

    init_world(
        0., _y_router_bottom[ ny - 1] + _router_width,
        _x_router_left[ nx - 1] + _router_width,  0.);
}

void GUI_GRAPHICS::draw_routers (void) 
{
    // draws routers.  occupancy of each router is also written;
    // colors are grey nuances to show occupancy/congestion; empty
    // ones are left white and have a dashed border; each PE for routers
    // that have PE are small red rects;
    float x1=0, y1=0, x2=0, y2=0;
    float x1_pe=0, y1_pe=0, x2_pe=0, y2_pe=0;
    float pe_width_height = _router_width / 4;
    long i=0, j=0;
    char buf[BUFFER_SIZE];
    long nx = _topology->ary_size();
    long ny = nx;

    setlinewidth(0);

    // routers show;
    for ( i = 0; i < nx; i++) {
        x1 = _x_router_left[i];
        x2 = x1 + _router_width;
        for ( j = 0; j < ny; j++) {

            long this_router_id = i * nx + j;
            y1 = _y_router_bottom[j];
            y2 = y1 + _router_width;
            if ( _router_occupancy[ this_router_id] > 0) {
                setlinestyle( SOLID);
                setcolor ( _router_color[ this_router_id]);
                fillrect ( x1,y1,x2,y2);
                setcolor ( BLACK);
                drawrect ( x1,y1,x2,y2);
                if ( _router_color[ this_router_id] > GREEN) { // see COLOR_TYPES;
                    setcolor ( CYAN);
                } else { 
                    setcolor ( BLACK);
                }
                sprintf ( buf, "ID %d Occ %.0f", this_router_id,
                    _router_occupancy[ this_router_id]);
                drawtext ( (x1 + x2)/2., (y1 + y2)/2., buf, _router_width);
            }
            else {
                setlinestyle (DASHED);
                setcolor (BLACK);
                drawrect (x1,y1,x2,y2);
            }

            // draw the PE;
            x1_pe = x1; x2_pe = x1 + pe_width_height;
            y1_pe = y1; y2_pe = y1 + pe_width_height;
            setlinestyle( SOLID);
            setcolor ( CYAN);
            fillrect ( x1_pe, y1_pe, x2_pe, y2_pe);
            setcolor ( BLACK);
            drawrect ( x1_pe, y1_pe, x2_pe, y2_pe);
            drawtext ( (x1_pe + x2_pe)/2., (y1_pe + y2_pe)/2., 
                "PE", 2*pe_width_height);

        }       
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// action functions
//
////////////////////////////////////////////////////////////////////////////////

//void GUI_GRAPHICS::zoom_in (void (*drawscreen) (void)) 
void GUI_GRAPHICS::zoom_in () 
{
    // Zooms in by a factor of 1.666.
    float xdiff, ydiff;
    xdiff = _xright - _xleft; 
    ydiff = _ybot - _ytop;
    _xleft += xdiff/5.;
    _xright -= xdiff/5.;
    _ytop += ydiff/5.;
    _ybot -= ydiff/5.;
    update_transform();
    drawscreen();
}

//void GUI_GRAPHICS::zoom_out (void (*drawscreen) (void))
void GUI_GRAPHICS::zoom_out () 
{
    // Zooms out by a factor of 1.666. 

    float xdiff, ydiff;

    xdiff = _xright - _xleft; 
    ydiff = _ybot - _ytop;
    _xleft -= xdiff/3.;
    _xright += xdiff/3.;
    _ytop -= ydiff/3.;
    _ybot += ydiff/3.;
    update_transform ();
    drawscreen();
}

//void GUI_GRAPHICS::zoom_fit (void (*drawscreen) (void)) 
void GUI_GRAPHICS::zoom_fit () 
{
    // Sets the view back to the initial view set by init_world (i.e. a full   
    // view) of all the graphics.
    _xleft = _saved_xleft;
    _xright = _saved_xright;
    _ytop = _saved_ytop;
    _ybot = _saved_ybot;
    update_transform();
    drawscreen();
}

//void GUI_GRAPHICS::translate_up (void (*drawscreen) (void)) 
void GUI_GRAPHICS::translate_up () 
{
    // Moves view 1/2 screen up.
    float ystep;
    ystep = (_ybot - _ytop)/2.;
    _ytop -= ystep;
    _ybot -= ystep;
    update_transform();        
    drawscreen();
}

//void GUI_GRAPHICS::translate_down (void (*drawscreen) (void)) 
void GUI_GRAPHICS::translate_down ()
{
    // Moves view 1/2 screen down.
    float ystep;
    ystep = (_ybot - _ytop)/2.;
    _ytop += ystep;
    _ybot += ystep;
    update_transform();         
    drawscreen();
}

//void GUI_GRAPHICS::translate_left (void (*drawscreen) (void)) 
void GUI_GRAPHICS::translate_left () 
{
    // Moves view 1/2 screen left.
    float xstep;
    xstep = (_xright - _xleft)/2.;
    _xleft -= xstep;
    _xright -= xstep; 
    update_transform();         
    drawscreen();
}

//void GUI_GRAPHICS::translate_right (void (*drawscreen) (void)) 
void GUI_GRAPHICS::translate_right () 
{
    // Moves view 1/2 screen right.
    float xstep;
    xstep = (_xright - _xleft)/2.;
    _xleft += xstep;
    _xright += xstep; 
    update_transform();         
    drawscreen();
}

//void GUI_GRAPHICS::update_win (int x[2], int y[2], void (*drawscreen)(void)) 
void GUI_GRAPHICS::update_win (int x[2], int y[2]) 
{
    float x1, x2, y1, y2;
    x[0] = min(x[0], _top_width-MWIDTH); // Can't go under menu 
    x[1] = min(x[1], _top_width-MWIDTH);
    y[0] = min(y[0], _top_height-T_AREA_HEIGHT); // Can't go under text area 
    y[1] = min(y[1], _top_height-T_AREA_HEIGHT);
    if ((x[0] == x[1]) || (y[0] == y[1])) {
        printf("Illegal (zero area) window.  Window unchanged.\n");
        return;
    }
    x1 = XTOWORLD(min(x[0], x[1]));
    x2 = XTOWORLD(max(x[0], x[1]));
    y1 = YTOWORLD(min(y[0], y[1]));
    y2 = YTOWORLD(max(y[0], y[1]));
    _xleft = x1;
    _xright = x2;
    _ytop = y1;
    _ybot = y2;
    update_transform();
    drawscreen();
}

//void GUI_GRAPHICS::adjustwin (void (*drawscreen) (void)) 
void GUI_GRAPHICS::adjustwin () 
{  
    // The window button was pressed.  Let the user click on the two
    //diagonally opposed corners, and zoom in on this area.
    XEvent report;
    int corner, xold, yold, x[2], y[2];
    corner = 0;
    xold = -1;
    yold = -1; // Don't need to init yold, but stops compiler warning. 
 
    while (corner<2) {
        XNextEvent ( _display, &report);

        switch (report.type) {

        case Expose:
            if (report.xexpose.count != 0)
                break;
            if (report.xexpose.window == _menu)
                drawmenu(); 
            else if (report.xexpose.window == _toplevel) {
                drawscreen();
                xold = -1; // No rubber band on screen 
            }
            else if (report.xexpose.window == _textarea)
                draw_message();
            break;

        case ConfigureNotify:
            _top_width = report.xconfigure.width;
            _top_height = report.xconfigure.height;
            update_transform();
            break;

        case ButtonPress:
            if (report.xbutton.window != _toplevel) break;
            x[corner] = report.xbutton.x;
            y[corner] = report.xbutton.y; 
            if (corner == 0) {
                XSelectInput ( _display, _toplevel, ExposureMask |
                    StructureNotifyMask | ButtonPressMask | PointerMotionMask);
            }
            else {
                update_win(x,y); // drawscreen
            }
            corner++;
            break;

        case MotionNotify:
            if (xold >= 0) {  // xold set -ve before we draw first box 
                XDrawRectangle( _display, _toplevel, _gcxor, min(x[0],xold),
                                min(y[0],yold), abs(x[0]-xold), abs(y[0]-yold));
            }
            // Don't allow user to window under menu region 
            xold = min(report.xmotion.x, _top_width - 1 - MWIDTH); 
            yold = report.xmotion.y;
            XDrawRectangle( _display, _toplevel, _gcxor, min(x[0],xold),
                            min(y[0],yold), abs(x[0]-xold), abs(y[0]-yold));
            break;
        }
    }
    XSelectInput( _display, _toplevel, 
        ExposureMask | StructureNotifyMask | ButtonPressMask); 
}

// void GUI_GRAPHICS::postscript (void (*drawscreen) (void)) 
void GUI_GRAPHICS::postscript () 
{
    // Takes a snapshot of the screen and stores it in pic?.ps.  The
    // first picture goes in pic1.ps, the second in pic2.ps, etc.
    _piccount = 1;
    int success;
    char fname[20];

    sprintf(fname, "results/pic%d.ps", _piccount);

    success = init_postscript(fname);
    if (success == 0) 
        return; // Couldn't open file, abort. 

    drawscreen();
    close_postscript();
    _piccount ++;
}

//void GUI_GRAPHICS::proceed (void (*drawscreen)(void)) 
void GUI_GRAPHICS::proceed () 
{
 // Dummy routine.  Just exit the event loop.
}

//void GUI_GRAPHICS::quit (void (*drawscreen) (void)) 
void GUI_GRAPHICS::quit () 
{
    close_graphics();
    exit(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// GUI_GRAPHICS library
//
////////////////////////////////////////////////////////////////////////////////

GUI_GRAPHICS::GUI_GRAPHICS( TOPOLOGY *topology, VNOC *vnoc)
{
    _is_gui_usable = false; // true only if asked by user thru line argument;
    _display_type = SCREEN;
    _topology = topology;
    _vnoc = vnoc;

    _menu_font_size = 14;
    strcpy( _user_message, "\0");
    // Graphics state.  Set start-up defaults here
    _currentcolor = BLACK;
    _currentlinestyle = SOLID;
    _currentlinewidth = 0;
    _currentfontsize = 10;

    _pic_on_screen = NO_PICTURE;
    _show_congestion = false;
    _show_links = false;

    _router_width = 100.0;
    _channel_width = 100.0;
    _max_router_occupancy = 1.0;
}

bool GUI_GRAPHICS::build()
{
    // construct all the gui stuff; before this the gui object was empty and
    // acted only asa place holder, whcih is now populated if user asked for it;
    if ( !_is_gui_usable)
        return false;

    // set up the display graphics;
    init_graphics("vNOC: versatile NOC simulator");

    // populate info on: _x_router_left, _y_router_bottom, _router_color,
    // _router_occupancy
    alloc_draw_structs();

    return true;
}


int GUI_GRAPHICS::xcoord(float worldx) 
{
    // Translates from my internal coordinates to X Windows coordinates  
    // in the x direction.  Add 0.5 at end for extra half-pixel accuracy.
    int winx = (int) ((worldx - _xleft) * _xmult + 0.5);
    // Avoid overflow in the X Window routines.  This will allow horizontal
    // and vertical lines to be drawn correctly regardless of zooming
    winx = max (winx, MINPIXEL);
    winx = min (winx, MAXPIXEL);
    return (winx);
}

int GUI_GRAPHICS::ycoord(float worldy) 
{
    int winy = (int) ((worldy - _ytop) * _ymult + 0.5);
    winy = max (winy, MINPIXEL);
    winy = min (winy, MAXPIXEL);
    return (winy);
}

void GUI_GRAPHICS::load_font(int pointsize) 
{
    // Makes sure the font of the specified size is loaded. Point_size
    // MUST be between 1 and MAX_FONT_SIZE -- no check is performed here. 

    char fontname[128];
    //sprintf(fontname,"-*-courier-medium-r-*--*-%d0-*-*-*-*-*-*",pointsize); 
    sprintf(fontname,"-adobe-helvetica-medium-o-normal--14-140-75-75-p-78-iso8859-1");
    //sprintf(fontname,"-adobe-helvetica-medium-o-normal--14-140-*-*-*-*-*-*");

    //if (( _font_info[pointsize] = XLoadQueryFont( _display, fontname)) == NULL) {
    if (( _font_info[pointsize] = XLoadQueryFont( _display, "9x15")) == NULL) {
        fprintf(stderr, "Cannot open desired font\n");
        exit(1);
    }
}

void GUI_GRAPHICS::force_setcolor(int cindex) 
{
    static char *ps_cnames[COLORS_NUMBER] = {"white", "black", 
        "grey55", "grey75", "blue", "green", "yellow", "cyan", "red",
        "grey35", "grey15" }; // "darkgreen", "magenta"
    _currentcolor = cindex;
    if ( _display_type == SCREEN) {
        XSetForeground( _display, _gc, _colors[cindex]);
    } else {
        fprintf( _ps_file, "%s\n", ps_cnames[cindex]);
    }
}
void GUI_GRAPHICS::setcolor (int cindex) 
{
    if ( _currentcolor != cindex) {
        force_setcolor( cindex);
    }
}

void GUI_GRAPHICS::force_setlinestyle(int linestyle) 
{ 
    // Note SOLID is 0 and DASHED is 1 for linestyle.
    // PostScript and X commands needed, respectively.
    static char *ps_text[2] = {"linesolid", "linedashed"};
    static int x_vals[2] = {LineSolid, LineOnOffDash};

    _currentlinestyle = linestyle;
    if ( _display_type == SCREEN) {
    XSetLineAttributes( _display, _gc, _currentlinewidth, 
        x_vals[linestyle], CapButt, JoinMiter);
    } else {
        fprintf( _ps_file, "%s\n", ps_text[linestyle]);
    }
}

void GUI_GRAPHICS::setlinestyle(int linestyle) 
{
    if (linestyle != _currentlinestyle)
        force_setlinestyle(linestyle);
}

void GUI_GRAPHICS::force_setlinewidth (int linewidth) 
{
    // Note SOLID is 0 and DASHED is 1 for linestyle;
    static int x_vals[2] = {LineSolid, LineOnOffDash};
    _currentlinewidth = linewidth;
    if ( _display_type == SCREEN) {
        XSetLineAttributes( _display, _gc, linewidth, x_vals[ _currentlinestyle],
            CapButt, JoinMiter);
    } else {
        fprintf( _ps_file, "%d setlinewidth\n", linewidth);
    }
}

void GUI_GRAPHICS::setlinewidth (int linewidth) 
{
    if (linewidth != _currentlinewidth)
        force_setlinewidth(linewidth);
}

void GUI_GRAPHICS::force_setfontsize (int pointsize) 
{
    // Valid point sizes are between 1 and MAX_FONT_SIZE
    if (pointsize < 1)
        pointsize = 1;
    else if (pointsize > MAX_FONT_SIZE)
        pointsize = MAX_FONT_SIZE;

    _currentfontsize = pointsize;
    if ( _display_type == SCREEN) {
        if ( !_font_is_loaded[pointsize]) {
            load_font(pointsize);
            _font_is_loaded[pointsize] = 1;
        }
        XSetFont( _display, _gc, _font_info[pointsize]->fid);
    }
    else {
        fprintf( _ps_file, "%d setfontsize\n",pointsize);
    }
}

void GUI_GRAPHICS::setfontsize (int pointsize) 
{
    // For efficiency, this routine doesn't do anything if no change is 
    // implied.  If you want to force the graphics context or PS file   
    // to have font info set, call force_setfontsize (this is necessary 
    // in initialization and X11 / Postscript switches).
    if (pointsize != _currentfontsize)
        force_setfontsize(pointsize);
}

void GUI_GRAPHICS::build_textarea(void) 
{
    // Creates a small window at the top of the graphics area for text messages
    XSetWindowAttributes menu_attributes;
    unsigned long valuemask;

    _textarea = XCreateSimpleWindow( _display, _toplevel,
        0, _top_height - T_AREA_HEIGHT, _display_width, T_AREA_HEIGHT - 4, 2,
        _colors[BLACK], _colors[LIGHTGREY]);
    menu_attributes.event_mask = ExposureMask;
    // ButtonPresses in this area are ignored.
    menu_attributes.do_not_propagate_mask = ButtonPressMask;
    // Keep text area on bottom left
    menu_attributes.win_gravity = SouthWestGravity;
    valuemask = CWWinGravity | CWEventMask | CWDontPropagate;
    XChangeWindowAttributes( _display, _textarea, valuemask, &menu_attributes);
    XMapWindow( _display, _textarea);
}

void GUI_GRAPHICS::setpoly(int bnum, int xc, int yc, int r, float theta) 
{
    // Puts a triangle in the poly array for button[bnum]
    int i;
    _button[bnum].istext = 0;
    _button[bnum].ispoly = 1;
    for ( i=0; i<3; i++) {
        _button[bnum].poly[i][0] = (int) (xc + r*cos(theta) + 0.5);
        _button[bnum].poly[i][1] = (int) (yc + r*sin(theta) + 0.5);
        theta += 2*PI/3;
    }
}

void GUI_GRAPHICS::build_default_menu (void) 
{
    // Sets up the default menu buttons on the right hand side of the window.
    XSetWindowAttributes menu_attributes;
    unsigned long valuemask;
    int i, xcen, x1, y1, bwid, bheight, space;

    _menu = XCreateSimpleWindow( _display, _toplevel,
        _top_width-MWIDTH, 0, MWIDTH-4, _display_height, 2,
        _colors[BLACK], _colors[LIGHTGREY]);
    menu_attributes.event_mask = ExposureMask;
    // Ignore button presses on the menu background.
    menu_attributes.do_not_propagate_mask = ButtonPressMask;
    // Keep menu on top right
    menu_attributes.win_gravity = NorthEastGravity;
    valuemask = CWWinGravity | CWEventMask | CWDontPropagate;
    XChangeWindowAttributes( _display, _menu, valuemask, &menu_attributes);
    XMapWindow( _display, _menu);

    _num_buttons = 11;
    _button = (BUTTON_TYPE *) my_malloc(_num_buttons * sizeof(BUTTON_TYPE));

    // Now do the arrow buttons
    bwid = 28;
    space = 3;
    y1 = 10;
    xcen = 51;
    x1 = xcen - bwid/2;
    _button[0].xleft = x1;
    _button[0].ytop = y1;
    setpoly (0, bwid/2, bwid/2, bwid/3, -PI/2.); // Up
    _button[0].fcn = UP; // translate_up;

    y1 += bwid + space;
    x1 = xcen - 3*bwid/2 - space;
    _button[1].xleft = x1;
    _button[1].ytop = y1;
    setpoly (1, bwid/2, bwid/2, bwid/3, PI); // Left 
    _button[1].fcn = LEFT; // translate_left;

    x1 = xcen + bwid/2 + space;
    _button[2].xleft = x1;
    _button[2].ytop = y1;
    setpoly (2, bwid/2, bwid/2, bwid/3, 0); // Right 
    _button[2].fcn = RIGHT; // translate_right;

    y1 += bwid + space;
    x1 = xcen - bwid/2;
    _button[3].xleft = x1;
    _button[3].ytop = y1;
    setpoly (3, bwid/2, bwid/2, bwid/3, +PI/2.); // Down
    _button[3].fcn = DOWN; // translate_down;

    for ( i=0; i<4; i++) {
        _button[i].width = bwid;
        _button[i].height = bwid;
    } 
 
    // Rectangular _buttons
    y1 += bwid + space + 6;
    space = 8;
    bwid = 90;
    bheight = 26;
    x1 = xcen - bwid/2;
    for ( i=4; i< _num_buttons; i++) {
        _button[i].xleft = x1;
        _button[i].ytop = y1;
        y1 += bheight + space;
        _button[i].istext = 1;
        _button[i].ispoly = 0;
        _button[i].width = bwid;
        _button[i].height = bheight;
    }

    strcpy (_button[4].text,"Zoom In");
    strcpy (_button[5].text,"Zoom Out");
    strcpy (_button[6].text,"Zoom Fit");
    strcpy (_button[7].text,"Window");
    strcpy (_button[8].text,"PostScript");
    strcpy (_button[9].text,"Proceed");
    strcpy (_button[10].text,"Exit");

    _button[4].fcn = ZOOM_IN; // zoom_in;
    _button[5].fcn = ZOOM_OUT; // zoom_out;
    _button[6].fcn = ZOOM_FIT; // zoom_fit;
    _button[7].fcn = ADJUSTWIN; // adjustwin;
    _button[8].fcn = PS; // postscript;
    _button[9].fcn = PROCEED; // proceed;
    _button[10].fcn = QUIT; // quit;

    for ( i=0; i < _num_buttons; i++) {
        map_button(i);
    }
}

void GUI_GRAPHICS::map_button (int bnum)
{
    // Maps a button onto the screen and set it up for input, etc.
    _button[bnum].win = 
        XCreateSimpleWindow( _display, _menu, 
        _button[bnum].xleft, _button[bnum].ytop, 
        _button[bnum].width, _button[bnum].height, 
        0, _colors[WHITE], _colors[LIGHTGREY]);
    XMapWindow( _display, _button[bnum].win);
    XSelectInput( _display, _button[bnum].win, ButtonPressMask);
    _button[bnum].ispressed = 1;
}

void GUI_GRAPHICS::unmap_button (int bnum) 
{
    // Unmaps a button from the screen.
    XUnmapWindow( _display, _button[bnum].win);
}

//void GUI_GRAPHICS::create_button (char *prev_button_text, char *button_text,
//void (*button_func) (void (*drawscreen) (void)))
void GUI_GRAPHICS::create_button (char *prev_button_text, char *button_text,
    BUTTON_FUNCTION button_func)
{
    // Creates a new button below the button containing prev_button_text.
    // The text and button function are set according to button_text and
    // button_func, respectively.
    int i, bnum, space;
    space = 8;
    // Only allow new buttons that are text (not poly) types.
    bnum = -1;
    for ( i=4; i < _num_buttons; i++) {
        if ( _button[i].istext == 1 &&
            strcmp( _button[i].text, prev_button_text) == 0) {
            bnum = i + 1;
            break;
        }
    }

    if (bnum == -1) {
        printf ("Error in create_button:  button with text %s not found.\n",
            prev_button_text);
        exit(1);
    }

    _num_buttons ++;
    _button = (BUTTON_TYPE *) my_realloc( _button, _num_buttons * sizeof(BUTTON_TYPE));

    _button[_num_buttons-1].xleft = _button[_num_buttons-2].xleft;
    _button[_num_buttons-1].ytop = 
        _button[_num_buttons-2].ytop + _button[_num_buttons-2].height + space;
    _button[_num_buttons-1].height = _button[_num_buttons-2].height;
    _button[_num_buttons-1].width = _button[_num_buttons-2].width;
    map_button( _num_buttons-1);

    for ( i = _num_buttons-1; i>bnum; i--) {
        _button[i].ispoly = _button[i-1].ispoly;
        // No poly copy for now, as I'm only providing the ability to create text
        _button[i].istext = _button[i-1].istext;
        strcpy( _button[i].text, _button[i-1].text);
        _button[i].fcn = _button[i-1].fcn;
        _button[i].ispressed = _button[i-1].ispressed;
    }

    _button[bnum].istext = 1;
    _button[bnum].ispoly = 0;
    strncpy( _button[bnum].text, button_text, BUTTON_TEXT_LEN);
    _button[bnum].fcn = button_func;
    _button[bnum].ispressed = 1;
}

void GUI_GRAPHICS::destroy_button(char *button_text) 
{
    // Destroys the _button with text button_text.
    int i, bnum;
    bnum = -1;
    for ( i=4; i< _num_buttons; i++) {
        if ( _button[i].istext == 1 &&
            strcmp( _button[i].text, button_text) == 0) {
            bnum = i;
            break;
        }
    }

    if ( bnum == -1) {
        printf ("Error in destroy_button:  button with text %s not found.\n", button_text);
        exit (1);
    }

    for (i=bnum+1; i < _num_buttons; i++) {
        _button[i-1].ispoly = _button[i].ispoly;
        _button[i-1].istext = _button[i].istext;
        strcpy( _button[i-1].text, _button[i].text);
        _button[i-1].fcn = _button[i].fcn;
        _button[i-1].ispressed = _button[i].ispressed;
    }

    unmap_button( _num_buttons - 1);
    _num_buttons --;
    _button = (BUTTON_TYPE *) my_realloc( _button, _num_buttons * sizeof(BUTTON_TYPE));
}

void GUI_GRAPHICS::init_graphics( char *window_name) 
{
    // Open the top level window, get the colors, 2 graphics 
    // contexts, load a font, and set up the toplevel window
    // Calls build_default_menu to set up the default menu.

    char *display_name = NULL;
    int x, y; // window position
    unsigned int border_width = 2; // ignored by OpenWindows
    XTextProperty windowName;

    // X Windows' names for my colours
    char *cnames[COLORS_NUMBER] = { "white", "black", "grey55", "grey75", "blue",
        "green", "yellow", "cyan", "red", "grey35", "grey15" }; // "RGBi:0.0/0.5/0.0", "magenta"

    XColor exact_def;
    Colormap cmap;
    int i;
    unsigned long valuemask = 0; // ignore XGCvalues and use defaults
    XGCValues values;
    XEvent event;

    _display_type = SCREEN; // Graphics go to screen, not ps

    for (i = 0; i <= MAX_FONT_SIZE; i++)
        _font_is_loaded[i] = 0; // No fonts loaded yet

        // connect to X server
        if ( (_display = XOpenDisplay(display_name)) == NULL ) {
            fprintf( stderr, "Cannot connect to X server %s\n",
                XDisplayName(display_name));
            exit(1);
        }

        // get screen size from display structure macro
        _screen_num = DefaultScreen( _display);
        _display_width = DisplayWidth( _display, _screen_num);
        _display_height = DisplayHeight( _display, _screen_num);

        x = y = 0;
        _top_width = 2 * _display_width / 3;
        _top_height = 4 * _display_height / 5;

        cmap = DefaultColormap( _display, _screen_num);
        _private_cmap = None;

        for ( i = 0; i< COLORS_NUMBER; i++) {
            if ( !XParseColor( _display, cmap, cnames[i], &exact_def)) {
                fprintf(stderr, "Color name %s not in database", cnames[i]);
                exit(-1);
        }
        if ( !XAllocColor( _display, cmap, &exact_def)) {
            fprintf(stderr, "Couldn't allocate color %s.\n", cnames[i]);

            if ( _private_cmap == None) {
                fprintf(stderr, "Will try to allocate a private colourmap.\n");
                _private_cmap = XCopyColormapAndFree( _display, cmap);
                cmap = _private_cmap;
                if ( !XAllocColor ( _display, cmap, &exact_def)) {
                    fprintf (stderr, "Couldn't allocate color %s as private.\n", cnames[i]);
                    exit(1);
                }
            }
            else {
                fprintf(stderr, "Couldn't allocate color %s as private.\n", cnames[i]);
                exit(1);
            }
        }
        _colors[i] = exact_def.pixel;
    }

    _toplevel = XCreateSimpleWindow( _display, RootWindow( _display, _screen_num),
        x, y, _top_width, _top_height, border_width, _colors[BLACK],
        _colors[WHITE]);

    if ( _private_cmap != None) {
        XSetWindowColormap( _display, _toplevel, _private_cmap);
    }


    XSelectInput( _display, _toplevel, 
        ExposureMask | StructureNotifyMask | ButtonPressMask);

    // Create default Graphics Contexts; valuemask = 0 -> use defaults.
    _gc = XCreateGC( _display, _toplevel, valuemask, &values);
    _gc_menus = XCreateGC( _display, _toplevel, valuemask, &values);

    // Create XOR graphics context for Rubber Banding
    values.function = GXxor;
    values.foreground = _colors[BLACK];
    _gcxor = XCreateGC( _display, _toplevel, 
        (GCFunction | GCForeground), &values);

    // specify font for menus;
    load_font( _menu_font_size);
    _font_is_loaded[ _menu_font_size] = 1;
    XSetFont( _display, _gc_menus, _font_info[_menu_font_size]->fid);

    // Set drawing defaults for user-drawable area.  Use whatever the
    // initial values of the current stuff was set to.
    force_setfontsize( _currentfontsize);
    force_setcolor( _currentcolor);
    force_setlinestyle( _currentlinestyle);
    force_setlinewidth( _currentlinewidth);

    XStringListToTextProperty( &window_name, 1, &windowName);
    XSetWMName( _display, _toplevel, &windowName);

    // XStringListToTextProperty copies the window_name string into
    // windowName.value.  Free this memory now;
    free( windowName.value);

    XMapWindow( _display, _toplevel);
    build_textarea();
    build_default_menu();
 
    // The following is completely unnecessary if the user is using the 
    // interactive (event_loop) graphics.  It waits for the first Expose
    // event before returning so that I can tell the window manager has got
    // the top-level window up and running.  Thus the user can start drawing
    // into this window immediately, and there's no danger of the window not
    // being ready and output being lost;
    XPeekIfEvent( _display, &event, test_if_exposed, NULL); 
}

void GUI_GRAPHICS::menutext(Window win, int xc, int yc, char *text) 
{
    // draws text center at xc, yc -- used only by menu drawing stuff
    int len, width;
    len = strlen(text);
    width = XTextWidth( _font_info[_menu_font_size], text, len);
    XDrawString( _display, win, _gc_menus, xc - width/2, 
        yc + (_font_info[_menu_font_size]->ascent - 
            _font_info[_menu_font_size]->descent)/2, text, len);
}

void GUI_GRAPHICS::drawbut (int bnum) 
{
    // Draws button bnum in either its pressed or unpressed state.
    int width, height, thick, i, ispressed;
    XPoint mypoly[6];

    ispressed = _button[bnum].ispressed;
    thick = 2;
    width = _button[bnum].width;
    height = _button[bnum].height;
    // Draw top and left edges of 3D box.
    if (ispressed) {
        XSetForeground( _display, _gc_menus, _colors[BLACK]);
    }
    else {
        XSetForeground( _display, _gc_menus, _colors[WHITE]);
    }

    mypoly[0].x = 0;
    mypoly[0].y = height;
    mypoly[1].x = 0;
    mypoly[1].y = 0;
    mypoly[2].x = width;
    mypoly[2].y = 0;
    mypoly[3].x = width-thick;
    mypoly[3].y = thick;
    mypoly[4].x = thick;
    mypoly[4].y = thick;
    mypoly[5].x = thick;
    mypoly[5].y = height-thick;
    XFillPolygon( _display, _button[bnum].win, _gc_menus, mypoly, 6, 
        Convex, CoordModeOrigin);

    // Draw bottom and right edges of 3D box.
    if (ispressed) {
        XSetForeground( _display, _gc_menus, _colors[WHITE]);
    } else {
        XSetForeground( _display, _gc_menus, _colors[BLACK]);
    } 
    mypoly[0].x = 0;
    mypoly[0].y = height;
    mypoly[1].x = width;
    mypoly[1].y = height;
    mypoly[2].x = width;
    mypoly[2].y = 0;
    mypoly[3].x = width-thick;
    mypoly[3].y = thick;
    mypoly[4].x = width-thick;
    mypoly[4].y = height-thick;
    mypoly[5].x = thick;
    mypoly[5].y = height-thick;
    XFillPolygon( _display, _button[bnum].win, _gc_menus, mypoly, 6,
        Convex, CoordModeOrigin);

    // Draw background 
    if (ispressed) {
        XSetForeground( _display, _gc_menus, _colors[DARKGREY]);
    } else {
        XSetForeground( _display, _gc_menus, _colors[LIGHTGREY]);
    }

    // Give x,y of top corner and width and height 
    XFillRectangle ( _display,_button[bnum].win, _gc_menus, thick, thick,
        width-2*thick, height-2*thick);

    // Draw polygon, if there is one 
    if ( _button[bnum].ispoly) {
        for (i=0;i<3;i++) {
           mypoly[i].x = _button[bnum].poly[i][0];
           mypoly[i].y = _button[bnum].poly[i][1];
        }
        XSetForeground( _display, _gc_menus, _colors[BLACK]);
        XFillPolygon( _display, _button[bnum].win, _gc_menus, mypoly,3,
            Convex, CoordModeOrigin);
    }

    // Draw text, if there is any 
    if ( _button[bnum].istext) {
        XSetForeground( _display, _gc_menus, _colors[BLACK]);
        menutext( _button[bnum].win, _button[bnum].width/2,
            _button[bnum].height/2, _button[bnum].text);
    }
}

void GUI_GRAPHICS::turn_on_off (int pressed) 
{
    // Shows when the menu is active or inactive by colouring the 
    // buttons.
    for (int i=0; i < _num_buttons; i++) {
        _button[i].ispressed = pressed;
        drawbut(i);
    }
}
int GUI_GRAPHICS::which_button (Window win)
{ 
    for (int i=0; i < _num_buttons; i++) {
    if ( _button[i].win == win)
        return(i);
    }
    printf("Error:  Unknown button ID in which_button.\n");
    return(0);
}
void GUI_GRAPHICS::drawmenu(void) 
{
    for (int i=0; i < _num_buttons; i++) {
        drawbut(i);
    }
}

void GUI_GRAPHICS::update_transform (void) 
{
    // Set up the factors for transforming from the user world to X Windows
    float mult, y1, y2, x1, x2;
    // X Window coordinates go from (0,0) to (width-1,height-1)
    _xmult = ((float) _top_width - 1. - MWIDTH) / (_xright - _xleft);
    _ymult = ((float) _top_height - 1. - T_AREA_HEIGHT)/ (_ybot - _ytop);
    // Need to use same scaling factor to preserve aspect ratio 
    if (fabs(_xmult) <= fabs(_ymult)) {
        mult = fabs( _ymult / _xmult);
        y1 = _ytop - (_ybot - _ytop)*(mult-1.)/2.;
        y2 = _ybot + (_ybot - _ytop)*(mult-1.)/2.;
        _ytop = y1;
        _ybot = y2;
    } else {
        mult = fabs(_xmult / _ymult);
        x1 = _xleft - ( _xright - _xleft)*(mult-1.)/2.;
        x2 = _xright + ( _xright - _xleft)*(mult-1.)/2.;
        _xleft = x1;
        _xright = x2;
    }
    _xmult = ((float) _top_width - 1. - MWIDTH) / (_xright - _xleft);
    _ymult = ((float) _top_height - 1. - T_AREA_HEIGHT)/ (_ybot - _ytop);
}

void GUI_GRAPHICS::update_ps_transform (void) 
{

    // Postscript coordinates start at (0,0) for the lower left hand corner *
    // of the page and increase upwards and to the right.  For 8.5 x 11     *
    // sheet, coordinates go from (0,0) to (612,792).  Spacing is 1/72 inch.*
    // I'm leaving a minimum of half an inch (36 units) of border around    *
    // each edge.                                                           *
    float ps_width, ps_height;
    ps_width = 540.;    // 72 * 7.5 
    ps_height = 720.;   // 72 * 10

    _ps_xmult = ps_width / (_xright - _xleft);
    _ps_ymult = ps_height / (_ytop - _ybot);
    // Need to use same scaling factor to preserve aspect ratio.  
    if (fabs( _ps_xmult) <= fabs( _ps_ymult)) {
        _ps_left = 36.;
        _ps_right = 36. + ps_width;
        _ps_bot = 396. - fabs( _ps_xmult * ( _ytop - _ybot))/2.;
        _ps_top = 396. + fabs( _ps_xmult * ( _ytop - _ybot))/2.;
        // Maintain aspect ratio but watch signs
        _ps_ymult = ( _ps_xmult * _ps_ymult < 0) ? -_ps_xmult : _ps_xmult;
    }
    else {
        _ps_bot = 36.;
        _ps_top = 36. + ps_height;
        _ps_left = 306. - fabs( _ps_ymult * (_xright - _xleft))/2.;
        _ps_right = 306. + fabs( _ps_ymult * (_xright - _xleft))/2.;
        _ps_ymult = ( _ps_xmult * _ps_ymult < 0) ? -_ps_xmult : _ps_xmult;
    }
}

//void GUI_GRAPHICS::event_loop( void (*act_on_button)(float x, float y),
//void (*drawscreen) (void));
void GUI_GRAPHICS::event_loop( void)
{
    // The program's main event loop.  Must be passed a user routine        
    // drawscreen which redraws the screen.  It handles all window resizing 
    // zooming etc. itself.  If the user clicks a button in the graphics   
    // (toplevel) area, the act_on_button routine passed in is called.      

    XEvent report;
    int bnum;
    float x, y;

    turn_on_off(ON);
    while (1) {

        XNextEvent( _display, &report);

        switch ( report.type) {
 
        case Expose:
            if (report.xexpose.count != 0)
                break;
            if (report.xexpose.window == _menu)
                drawmenu(); 
            else if (report.xexpose.window == _toplevel)
                drawscreen();
            else if (report.xexpose.window == _textarea)
                draw_message();
            break;
    
            case ConfigureNotify:
                _top_width = report.xconfigure.width;
                _top_height = report.xconfigure.height;
                update_transform();
            break;

        case ButtonPress:
            if ( report.xbutton.window == _toplevel) {
                x = XTOWORLD(report.xbutton.x);
                y = YTOWORLD(report.xbutton.y);
                //act_on_button(x,y);
            } 
            else { // A menu button was pressed.
                bnum = which_button(report.xbutton.window);
                _button[bnum].ispressed = 1;
                drawbut(bnum);
                XFlush( _display); // Flash the button
                // now call the function associated with this button that
                // was just pressed;
                execute_action_associated_with_button_function( _button[bnum].fcn);
                //_button[bnum].fcn( drawscreen );
    
                _button[bnum].ispressed = 0;
                drawbut( bnum);
                if ( _button[bnum].fcn == PROCEED) { // proceed
                    turn_on_off(OFF);
                    flushinput();
                    return; 
                    // Rather clumsy way of returning control to the simulator;
                }
            }
            break;
    
        } // switch
    
    }
}

void GUI_GRAPHICS::execute_action_associated_with_button_function( BUTTON_FUNCTION fcn)
{
    // call action function associated with buuton pressed;

    switch ( fcn) {
    case UP:
        translate_up();
        break;
    case DOWN:
        translate_down();
        break;
    case LEFT:
        translate_left();
        break;
    case RIGHT:
        translate_right();
        break;  
    case ZOOM_IN:
        zoom_in();
        break;
    case ZOOM_OUT:
        zoom_out();
        break;
    case ZOOM_FIT:
        zoom_fit();
        break;
    case ADJUSTWIN:
        adjustwin();
        break;
    case PS:
        postscript();
        break;
    case PROCEED:
        proceed();
        break;
    case QUIT:
        quit();
        break;
    case TOGGLE_LINKS:
        toggle_links();
        break;
    default:
        assert(0);
    break;
    }
}

void GUI_GRAPHICS::clearscreen (void) 
{
    int savecolor;
    if ( _display_type == SCREEN) {
        XClearWindow ( _display, _toplevel);
    }
    else {
        // erases current page.  Don't use erasepage, since this will erase 
        // everything, (even stuff outside the clipping path) causing   
        // problems if this picture is incorporated into a larger document. */
        savecolor = _currentcolor;
        setcolor (WHITE);
        fprintf( _ps_file, "clippath fill\n\n");
        setcolor (savecolor);
    }
}

int GUI_GRAPHICS::rect_off_screen (float x1, float y1, float x2, float y2) 
{
    // Return 1 if I can quarantee no part of this rectangle will
    //lie within the user drawing area.  Otherwise return 0. 
    //Note:  this routine is only used to help speed (and to shrink ps 
    //files) -- it will be highly effective when the graphics are zoomed
    //in and lots are off-screen.  I don't have to pre-clip for 
    //correctness.

    float xmin, xmax, ymin, ymax;

    xmin = min (_xleft, _xright);
        if (x1 < xmin && x2 < xmin) 
    return (1);

    xmax = max (_xleft, _xright);
        if (x1 > xmax && x2 > xmax)
    return (1);

    ymin = min (_ytop, _ybot);
        if (y1 < ymin && y2 < ymin)
    return (1);

    ymax = max (_ytop, _ybot);
        if (y1 > ymax && y2 > ymax)
    return (1);

    return (0);
}

 
void GUI_GRAPHICS::drawline (float x1, float y1, float x2, float y2) 
{
    // Draw a line from (x1,y1) to (x2,y2) in the user-drawable area. 
    // Coordinates are in world (user) space.                         

    if (rect_off_screen(x1,y1,x2,y2))
        return;

    if ( _display_type == SCREEN) {
        // Xlib.h prototype has x2 and y1 mixed up. *
        XDrawLine( _display, _toplevel, _gc, 
            xcoord(x1), ycoord(y1), xcoord(x2), ycoord(y2));
    }
    else {
        fprintf( _ps_file, "%.2f %.2f %.2f %.2f drawline\n",
            XPOST(x1), YPOST(y1), XPOST(x2), YPOST(y2));
    }
}

void GUI_GRAPHICS::drawrect (float x1, float y1, float x2, float y2)
{
    // (x1,y1) and (x2,y2) are diagonally opposed corners, in world coords.

    unsigned int width, height;
    int xw1, yw1, xw2, yw2, xl, yt;

    if (rect_off_screen(x1,y1,x2,y2))
        return;

    if ( _display_type == SCREEN) { 
        // translate to X Windows calling convention.
        xw1 = xcoord(x1);
        xw2 = xcoord(x2);
        yw1 = ycoord(y1);
        yw2 = ycoord(y2); 
        xl = min(xw1,xw2);
        yt = min(yw1,yw2);
        width = abs (xw1-xw2);
        height = abs (yw1-yw2);
        XDrawRectangle( _display, _toplevel, _gc, xl, yt, width, height);
    }
    else {
        fprintf( _ps_file, "%.2f %.2f %.2f %.2f drawrect\n",
            XPOST(x1),YPOST(y1), XPOST(x2),YPOST(y2));
    }
}

void GUI_GRAPHICS::fillrect (float x1, float y1, float x2, float y2) 
{
    // (x1,y1) and (x2,y2) are diagonally opposed corners in world coords.

    unsigned int width, height;
    int xw1, yw1, xw2, yw2, xl, yt;

    if (rect_off_screen(x1,y1,x2,y2))
        return;

    if ( _display_type == SCREEN) {
        // translate to X Windows calling convention.
        xw1 = xcoord(x1);
        xw2 = xcoord(x2);
        yw1 = ycoord(y1);
        yw2 = ycoord(y2); 
        xl = min(xw1,xw2);
        yt = min(yw1,yw2);
        width = abs (xw1-xw2);
        height = abs (yw1-yw2);
        XFillRectangle( _display, _toplevel, _gc, xl, yt, width, height);
    }
    else {
        fprintf( _ps_file, "%.2f %.2f %.2f %.2f fillrect\n",
            XPOST(x1),YPOST(y1), XPOST(x2),YPOST(y2));
    }
}

float GUI_GRAPHICS::angnorm (float ang) 
{
    // Normalizes an angle to be between 0 and 360 degrees.
    int scale;
    if (ang < 0) {
        scale = (int) (ang / 360. - 1);
    }
    else {
        scale = (int) (ang / 360.);
    }
    ang = ang - scale * 360.;
    return (ang);
}

void GUI_GRAPHICS::drawarc (float xc, float yc, float rad, float startang, float angextent) 
{

    // Draws a circular arc.  X11 can do elliptical arcs quite simply, and
    //PostScript could do them by scaling the coordinate axes.  Too much 
    //work for now, and probably too complex an object for users to draw 
    //much, so I'm just doing circular arcs.  Startang is relative to the
    //Window's positive x direction.  Angles in degrees.  

    int xl, yt;
    unsigned int width, height;

    // Conservative (but fast) clip test -- check containing rectangle of
    //a circle.
    if (rect_off_screen (xc-rad, yc-rad, xc+rad, yc+rad))
        return;

    // X Windows has trouble with very large angles. (Over 360).  
    //Do following to prevent its inaccurate (overflow?) problems. 
    if (fabs(angextent) > 360.) 
        angextent = 360.;

    startang = angnorm( startang);
 
    if ( _display_type == SCREEN) {
        xl = (int) (xcoord(xc) - fabs(_xmult*rad));
        yt = (int) (ycoord(yc) - fabs(_ymult*rad));
        width = (unsigned int) (2*fabs(_xmult*rad));
        height = width;
        XDrawArc ( _display, _toplevel, _gc, xl, yt, width, height,
                   (int) (startang*64), (int) (angextent*64));
    }
    else {
        fprintf( _ps_file, "%.2f %.2f %.2f %.2f %.2f %s stroke\n", 
            XPOST(xc), YPOST(yc), fabs(rad * _ps_xmult), startang, 
            startang+angextent, (angextent < 0) ? "drawarcn" : "drawarc") ;
    }
}

void GUI_GRAPHICS::fillarc (float xc, float yc, float rad, float startang,
    float angextent) 
{

    // Fills a circular arc.  Startang is relative to the Window's positive x 
    //direction.  Angles in degrees.
    int xl, yt;
    unsigned int width, height;

    // Conservative (but fast) clip test -- check containing rectangle of *
    //a circle.
    if (rect_off_screen (xc-rad, yc-rad, xc+rad, yc+rad))
        return;

    // X Windows has trouble with very large angles. (Over 360).  
    //Do following to prevent its inaccurate (overflow?) problems.
    if (fabs(angextent) > 360.) 
        angextent = 360.;

    startang = angnorm( startang);

    if ( _display_type == SCREEN) {
        xl = (int) (xcoord(xc) - fabs(_xmult*rad));
        yt = (int) (ycoord(yc) - fabs(_ymult*rad));
        width = (unsigned int) (2*fabs(_xmult*rad));
        height = width;
        XFillArc( _display, _toplevel, _gc, xl, yt, width, height,
            (int) (startang*64), (int) (angextent*64));
    }
    else {
        fprintf( _ps_file, "%.2f %.2f %.2f %.2f %.2f %s\n", 
            fabs(rad*_ps_xmult), startang, startang+angextent, 
            XPOST(xc), YPOST(yc), (angextent < 0) ? "fillarcn" : "fillarc") ;
    }
}

void GUI_GRAPHICS::fillpoly (T_POINT *points, int npoints) 
{
    XPoint transpoints[MAXPTS];
    int i;
    float xmin, ymin, xmax, ymax;
    if ( npoints > MAXPTS) {
        printf("Error in fillpoly:  Only %d points allowed per polygon.\n", MAXPTS);
        printf("%d points were requested.  Polygon is not drawn.\n",npoints);
        return;
    }

    // Conservative (but fast) clip test -- check containing rectangle of
    //polygon.
    xmin = xmax = points[0].x;
    ymin = ymax = points[0].y;
    for (i=1; i < npoints; i++) {
        xmin = min (xmin,points[i].x);
        xmax = max (xmax,points[i].x);
        ymin = min (ymin,points[i].y);
        ymax = max (ymax,points[i].y);
    }
    if (rect_off_screen(xmin,ymin,xmax,ymax))
        return;

    if ( _display_type == SCREEN) {
        for (i=0; i < npoints; i++) {
            transpoints[i].x = (short) xcoord (points[i].x);
            transpoints[i].y = (short) ycoord (points[i].y);
        }
        XFillPolygon( _display, _toplevel, _gc, transpoints, npoints, 
            Complex, CoordModeOrigin);
    }
    else {
        fprintf( _ps_file, "\n");
        for (i=npoints-1; i>=0; i--) {
            fprintf( _ps_file,  "%.2f %.2f\n", XPOST(points[i].x), YPOST(points[i].y));
        }
        fprintf( _ps_file,  "%d fillpoly\n", npoints);
    }
}

void GUI_GRAPHICS::drawtext (float xc, float yc, char *text, float boundx) 
{
    // Draws text centered on xc,yc if it fits in boundx
    int len, width, xw_off, yw_off; 
 
    len = strlen(text);
    width = XTextWidth( _font_info[_currentfontsize], text, len);
    if (width > fabs(boundx * _xmult)) return; // Don't draw if it won't fit 

    xw_off = int(width/(2.*_xmult));

    // 2 * descent makes this slightly conservative but simplifies code. 
    yw_off = int(( _font_info[_currentfontsize]->ascent +
        2 * _font_info[_currentfontsize]->descent)/(2.*_ymult)); 

    // Note:  text can be clipped when a little bit of it would be visible
    // right now.  Perhaps X doesn't return extremely accurate width and 
    // ascent values, etc?  Could remove this completely by multiplying  
    // xw_off and yw_off by, 1.2 or 1.5.
    if ( rect_off_screen( xc-xw_off, yc-yw_off, xc+xw_off, yc+yw_off))
        return;

    if ( _display_type == SCREEN) {
        XDrawString( _display, _toplevel, _gc, xcoord(xc)-width/2, 
            ycoord(yc) + (_font_info[_currentfontsize]->ascent - 
            _font_info[_currentfontsize]->descent)/2, text, len);
    }
    else {
        fprintf( _ps_file, "(%s) %.2f %.2f censhow\n",text, XPOST(xc), YPOST(yc));
    }
}

void GUI_GRAPHICS::flushinput (void) 
{
    if ( _display_type != SCREEN) return;
    XFlush( _display);
}

void GUI_GRAPHICS::init_world (float x1, float y1, float x2, float y2) 
{
    // Sets the coordinate system the user wants to draw into.
    _xleft = x1;
    _xright = x2;
    _ytop = y1;
    _ybot = y2;
    _saved_xleft = _xleft; // Save initial world coordinates to allow full 
    _saved_xright = _xright; // view button to zoom all the way out.         
    _saved_ytop = _ytop;
    _saved_ybot = _ybot;

    if ( _display_type == SCREEN) {
        update_transform();
    } else {
        update_ps_transform();
    }
}

void GUI_GRAPHICS::draw_message (void) 
{
    // Draw the current message in the text area at the screen bottom.
    int len, width, savefontsize, savecolor;
    float ylow;

    if ( _display_type == SCREEN) {
        XClearWindow( _display, _textarea);
        len = strlen(_user_message);
        width = XTextWidth( _font_info[_menu_font_size], _user_message, len);

        XSetForeground( _display, _gc_menus, _colors[BLACK]);
        XDrawString( _display, _textarea, _gc_menus,
            (_top_width - MWIDTH - width)/2,
            (T_AREA_HEIGHT - 4)/2 + (_font_info[_menu_font_size]->ascent -
            _font_info[_menu_font_size]->descent)/2, _user_message, len);
    }

    else {
        // Draw the message in the bottom margin.  Printer's generally can't
        //print on the bottom 1/4" (area with y < 18 in PostScript coords.)
        savecolor = _currentcolor;
        setcolor (BLACK);
        savefontsize = _currentfontsize;
        setfontsize(_menu_font_size - 2);  // Smaller OK on paper 
        ylow = _ps_bot - 8.; 
        fprintf( _ps_file, "(%s) %.2f %.2f censhow\n", _user_message,
            ( _ps_left + _ps_right)/2., ylow);
        setcolor( savecolor);
        setfontsize( savefontsize);
    }
}

void GUI_GRAPHICS::update_message (char *msg) 
{
    // Changes the _user_message to be displayed on screen.
    strncpy (_user_message, msg, BUFFER_SIZE);
    draw_message ();
}

void GUI_GRAPHICS::close_graphics (void) 
{
    // Release all my drawing structures (through the X server) and
    // close down the connection.
    int i;
    for (i=1; i <= MAX_FONT_SIZE; i++) {
        if ( _font_is_loaded[i])
            XFreeFont( _display, _font_info[i]);
    }
    
    XFreeGC( _display, _gc);
    XFreeGC( _display, _gcxor);
    XFreeGC( _display, _gc_menus);

    if ( _private_cmap != None) {
        XFreeColormap( _display, _private_cmap);
    }
    XCloseDisplay( _display);
    free( _button);
}

int GUI_GRAPHICS::init_postscript (char *fname)
{
    // Opens a file for PostScript output.  The header information,
    // clipping path, etc. are all dumped out.  If the file could  
    // not be opened, the routine returns 0; otherwise it returns 1.
    _ps_file = fopen( fname, "w");
    if (_ps_file == NULL) {
        printf("Error: could not open %s for PostScript output.\n",fname);
        printf("Drawing to screen instead.\n");
        return (0);
    }
    _display_type = POSTSCRIPT; // Graphics go to postscript file now. 

    // Header for minimal conformance with the Adobe structuring convention 
    fprintf( _ps_file, "%%!PS-Adobe-1.0\n");
    fprintf( _ps_file, "%%%%DocumentFonts: Helvetica\n");
    fprintf( _ps_file, "%%%%Pages: 1\n");
    // Set up postscript transformation macros and page boundaries 
    update_ps_transform();
    // Bottom margin is at ps_bot - 15. to leave room for the on-screen message. 
    fprintf( _ps_file, "%%%%BoundingBox: %.2f %.2f %.2f %.2f\n",
             _ps_left, _ps_bot - 15., _ps_right, _ps_top); 
    fprintf( _ps_file, "%%%%EndComments\n");

    fprintf( _ps_file, "/censhow   %%draw a centered string\n");
    fprintf( _ps_file, " { moveto               %% move to proper spot\n");
    fprintf( _ps_file, "   dup stringwidth pop  %% get x length of string\n");
    fprintf( _ps_file, "   -2 div               %% Proper left start\n");
    fprintf( _ps_file, "   yoff rmoveto         %% Move left that much and down half font height\n");
    fprintf( _ps_file, "   show newpath } def   %% show the string\n\n"); 

    fprintf( _ps_file, "/setfontsize     %% set font to desired size and compute "
             "centering yoff\n");
    fprintf( _ps_file, " { /Helvetica findfont\n");
    fprintf( _ps_file, "   exch scalefont\n");
    fprintf( _ps_file, "   setfont         %% Font size set ...\n\n");
    fprintf( _ps_file, "   0 0 moveto      %% Get vertical centering offset\n");
    fprintf( _ps_file, "   (Xg) true charpath\n");
    fprintf( _ps_file, "   flattenpath pathbbox\n");
    fprintf( _ps_file, "   /ascent exch def pop -1 mul /descent exch def pop\n");
    fprintf( _ps_file, "   newpath\n");
    fprintf( _ps_file, "   descent ascent sub 2 div /yoff exch def } def\n\n");

    fprintf( _ps_file, "%% Next two lines for debugging only.\n");
    fprintf( _ps_file, "/str 20 string def\n");
    fprintf( _ps_file, "/pnum {str cvs print (  ) print} def\n");

    fprintf( _ps_file, "/drawline      %% draw a line from (x2,y2) to (x1,y1)\n");
    fprintf( _ps_file, " { moveto lineto stroke } def\n\n");

    fprintf( _ps_file, "/rect          %% outline a rectangle \n");
    fprintf( _ps_file, " { /y2 exch def /x2 exch def /y1 exch def /x1 exch def\n");
    fprintf( _ps_file, "   x1 y1 moveto\n");
    fprintf( _ps_file, "   x2 y1 lineto\n");
    fprintf( _ps_file, "   x2 y2 lineto\n");
    fprintf( _ps_file, "   x1 y2 lineto\n");
    fprintf( _ps_file, "   closepath } def\n\n");

    fprintf( _ps_file, "/drawrect      %% draw outline of a rectanagle\n");
    fprintf( _ps_file, " { rect stroke } def\n\n");

    fprintf( _ps_file, "/fillrect      %% fill in a rectanagle\n");
    fprintf( _ps_file, " { rect fill } def\n\n");

    fprintf ( _ps_file, "/drawarc { arc stroke } def           %% draw an arc\n");
    fprintf ( _ps_file, "/drawarcn { arcn stroke } def "
              "        %% draw an arc in the opposite direction\n\n");

    fprintf ( _ps_file, "%%Fill a counterclockwise or clockwise arc sector, "
              "respectively.\n");
    fprintf ( _ps_file, "/fillarc { moveto currentpoint 5 2 roll arc closepath fill } "
              "def\n");
    fprintf ( _ps_file, "/fillarcn { moveto currentpoint 5 2 roll arcn closepath fill } "
              "def\n\n");

    fprintf ( _ps_file, "/fillpoly { 3 1 roll moveto         %% move to first point\n"
              "   2 exch 1 exch {pop lineto} for   %% line to all other points\n"
              "   closepath fill } def\n\n");

    fprintf( _ps_file, "%%Color Definitions:\n");
    fprintf( _ps_file, "/white { 1 setgray } def\n");
    fprintf( _ps_file, "/black { 0 setgray } def\n");
    fprintf( _ps_file, "/grey55 { .55 setgray } def\n");
    fprintf( _ps_file, "/grey75 { .75 setgray } def\n");
    fprintf( _ps_file, "/blue { 0 0 1 setrgbcolor } def\n");
    fprintf( _ps_file, "/green { 0 1 0 setrgbcolor } def\n");
    fprintf( _ps_file, "/yellow { 1 1 0 setrgbcolor } def\n");
    fprintf( _ps_file, "/cyan { 0 1 1 setrgbcolor } def\n");
    fprintf( _ps_file, "/red { 1 0 0 setrgbcolor } def\n");
    fprintf( _ps_file, "/darkgreen { 0 0.5 0 setrgbcolor } def\n");
    fprintf( _ps_file, "/magenta { 1 0 1 setrgbcolor } def\n");

    fprintf( _ps_file, "\n%%Solid and dashed line definitions:\n");
    fprintf( _ps_file, "/linesolid {[] 0 setdash} def\n");
    fprintf( _ps_file, "/linedashed {[3 3] 0 setdash} def\n");

    fprintf( _ps_file, "\n%%%%EndProlog\n");
    fprintf( _ps_file, "%%%%Page: 1 1\n\n");

    // Set up PostScript graphics state to match current one. 
    force_setcolor( _currentcolor);
    force_setlinestyle( _currentlinestyle);
    force_setlinewidth( _currentlinewidth);
    force_setfontsize( _currentfontsize); 

    // Draw this in the bottom margin -- must do before the clippath is set 
    draw_message();

    // Set clipping on page. 
    fprintf( _ps_file, "%.2f %.2f %.2f %.2f rect ", _ps_left, _ps_bot, _ps_right, _ps_top);
    fprintf( _ps_file, "clip newpath\n\n");

    return (1);
}

void GUI_GRAPHICS::close_postscript (void) 
{
    // Properly ends postscript output and redirects output to screen.
    fprintf( _ps_file, "showpage\n");
    fprintf( _ps_file, "\n%%%%Trailer\n");
    fclose( _ps_file);
    _display_type = SCREEN;
    // Ensure screen world reflects any changes made while printing.
    update_transform(); 

    // Need to make sure that we really set up the graphics context --
    // don't want the change requested to match the current setting and *
    // do nothing -> force the changes.
    force_setcolor( _currentcolor);
    force_setlinestyle( _currentlinestyle);
    force_setlinewidth( _currentlinewidth);
    force_setfontsize( _currentfontsize); 
}


