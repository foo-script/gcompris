/* gcompris - paratrooper.c
 *
 * Time-stamp: <2006/08/21 23:34:30 bruno>
 *
 * Copyright (C) 2000 Bruno Coudoin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "gcompris/gcompris.h"

#define SOUNDLISTFILE PACKAGE

static int gamewon;

static GList *item_list = NULL;
static GList *item2del_list = NULL;

static GcomprisBoard *gcomprisBoard = NULL;

static gint dummy_id = 0;
static gint drop_tux_id = 0;

static GooCanvasItem *boatitem = NULL;
static gint boat_x, boat_y, boat_landarea_y, boat_length;

static GooCanvasItem *planeitem = NULL;
static gint plane_x, plane_y;
static gint planespeed_x, planespeed_y;
static double windspeed;

typedef struct {
  gint number;
  GooCanvasItem *rootitem;
} CloudItem;


typedef enum
{
  TUX_INPLANE		= 1 << 0,
  TUX_DROPPING		= 1 << 1,
  TUX_FLYING		= 1 << 2,
  TUX_LANDED		= 1 << 3,
  TUX_CRASHED		= 1 << 4
} ParaStatus;


typedef struct {
  ParaStatus status;
  double speed;
  double drift;
  gboolean speed_override;
  double x;
  double y;
  GooCanvasItem *rootitem;
  GooCanvasItem *paratrooper;
  GooCanvasItem *parachute;
  GooCanvasItem *instruct;
} ParatrooperItem;

static  ParatrooperItem	     paratrooperItem;


/* default gnome pixmap directory in which this game tales the icon */
static char *pixmapsdir = "paratrooper/";

static void	 	 start_board (GcomprisBoard *agcomprisBoard);
static void	 	 pause_board (gboolean pause);
static void	 	 end_board (void);
static gboolean	 	 is_our_board (GcomprisBoard *gcomprisBoard);
static void	 	 set_level (guint level);
static gint	 	 key_press(guint keyval, gchar *commit_str, gchar *preedit_str);

static GooCanvasItem	*paratrooper_create_cloud(GooCanvasItem *parent);
static gint		 paratrooper_drop_clouds (GtkWidget *widget, gpointer data);
static gint		 paratrooper_move_items (GtkWidget *widget, gpointer data);
static gint		 paratrooper_move_tux (GtkWidget *widget, gpointer data);
static void		 paratrooper_destroy_item(CloudItem *clouditem);
static void		 paratrooper_destroy_items(void);
static void		 paratrooper_destroy_all_items(void);
static void		 paratrooper_next_level(void);
static gboolean		 item_event (GooCanvasItem  *item,
				     GooCanvasItem  *target,
				     GdkEventButton *event,
				     gpointer data);
static void		 next_state(void);


static  double               speed = 0.0;
static  double               imageZoom = 0.0;

/* Description of this plugin */
static BoardPlugin menu_bp =
{
   NULL,
   NULL,
   "Parachutist",
   "Direct the parachutist to help him or her land safely",
   "Bruno Coudoin <bruno.coudoin@free.fr>",
   NULL,
   NULL,
   NULL,
   NULL,
   start_board,
   pause_board,
   end_board,
   is_our_board,
   key_press,
   NULL,
   set_level,
   NULL,
   NULL,
   NULL,
   NULL
};

/*
 * Main entry point mandatory for each Gcompris's game
 * ---------------------------------------------------
 *
 */

GET_BPLUGIN_INFO(paratrooper)

/*
 * in : boolean TRUE = PAUSE : FALSE = CONTINUE
 *
 */
static void pause_board (gboolean pause)
{
  if(gcomprisBoard==NULL)
    return;

  if(pause)
    {
      if (dummy_id) {
	gtk_timeout_remove (dummy_id);
	dummy_id = 0;
      }
      if (drop_tux_id) {
	gtk_timeout_remove (drop_tux_id);
	drop_tux_id = 0;
      }
    }
  else
    {
      if(gamewon == TRUE) /* the game is won */
	{
	  gcomprisBoard->level++;
	  if(gcomprisBoard->level>gcomprisBoard->maxlevel) { // the current board is finished : bail out
	    gc_bonus_end_display(GC_BOARD_FINISHED_RANDOM);
	    return;
	  }
	}

      // Unpause code
      if(!dummy_id) {
	dummy_id = gtk_timeout_add (1000, (GtkFunction) paratrooper_move_items, NULL);
      }
      if(paratrooperItem.status!=TUX_INPLANE && paratrooperItem.status!=TUX_LANDED) {
	drop_tux_id = gtk_timeout_add (1000, (GtkFunction) paratrooper_move_tux, NULL);
      }

      if(gamewon == TRUE) /* the game is won */
	  paratrooper_next_level();
    }
}

/*
 */
static void start_board (GcomprisBoard *agcomprisBoard)
{

  if(agcomprisBoard!=NULL)
    {
      gcomprisBoard=agcomprisBoard;

      /* disable im_context */
      gcomprisBoard->disable_im_context = TRUE;

      gc_set_background(goo_canvas_get_root_item(gcomprisBoard->canvas),
			"paratrooper/scenery3_background.png");


      /* set initial values for this level */
      gcomprisBoard->level = 1;
      gcomprisBoard->maxlevel = 6;
      gc_bar_set(GC_BAR_LEVEL);

      /* Init of paratrooper struct */
      paratrooperItem.rootitem = NULL;

      paratrooper_next_level();

      pause_board(FALSE);

    }

}

static void
end_board ()
{

  if(gcomprisBoard!=NULL)
    {
      pause_board(TRUE);
      paratrooper_destroy_all_items();
      gcomprisBoard->level = 1;       // Restart this game to zero
    }
  gcomprisBoard = NULL;
}

static void
set_level (guint level)
{

  if(gcomprisBoard!=NULL)
    {
      gcomprisBoard->level=level;
      paratrooper_next_level();
    }
}

static gint key_press(guint keyval, gchar *commit_str, gchar *preedit_str)
{

  if(!gcomprisBoard)
    return FALSE;


  /* Add some filter for control and shift key */
  switch (keyval)
    {
      /* Avoid all this keys to be interpreted by this game */
    case GDK_Shift_L:
    case GDK_Shift_R:
    case GDK_Control_L:
    case GDK_Control_R:
    case GDK_Caps_Lock:
    case GDK_Shift_Lock:
    case GDK_Meta_L:
    case GDK_Meta_R:
    case GDK_Alt_L:
    case GDK_Alt_R:
    case GDK_Super_L:
    case GDK_Super_R:
    case GDK_Hyper_L:
    case GDK_Hyper_R:
    case GDK_Num_Lock:
      return FALSE;
    case GDK_KP_Enter:
    case GDK_Return:
      return TRUE;
    case GDK_Right:
      break;
    case GDK_Left:
      break;
    case GDK_Up:
      paratrooperItem.speed_override = 1;
      if(paratrooperItem.status == TUX_FLYING && paratrooperItem.speed >= 3)
	paratrooperItem.speed--;
      return TRUE;
    case GDK_Down:
      paratrooperItem.speed_override = 1;
      if(paratrooperItem.status == TUX_FLYING && paratrooperItem.speed <= 6)
	paratrooperItem.speed++;
      return TRUE;
    }

  next_state();

  return TRUE;
}

static gboolean
is_our_board (GcomprisBoard *gcomprisBoard)
{
  if (gcomprisBoard)
    {
      if(g_strcasecmp(gcomprisBoard->type, "paratrooper")==0)
	{
	  /* Set the plugin entry */
	  gcomprisBoard->plugin=&menu_bp;

	  return TRUE;
	}
    }
  return FALSE;
}

/*-------------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------------*/

/* set initial values for the next level */
static void paratrooper_next_level()
{
  GdkPixbuf *pixmap = NULL;
  char *str = NULL;

  gamewon = FALSE;

  gc_bar_set_level(gcomprisBoard);

  paratrooper_destroy_all_items();

  /* Try the next level */
  speed=100+(30/(gcomprisBoard->level));
  gcomprisBoard->number_of_sublevel=0;
  /* Make the images tend to 0.5 ratio */
  imageZoom=0.4+(0.2 * (2 - (gcomprisBoard->level-1) % 3));

  /* Setup and Display the plane */
  planespeed_y = 0;
  planespeed_x = 4 + gcomprisBoard->level;
  str = g_strdup_printf("%s%s", pixmapsdir, "tuxplane.png");
  pixmap = gc_pixmap_load(str);
  plane_x = 0;
  plane_y = 40;
  planeitem = goo_canvas_image_new (goo_canvas_get_root_item(gcomprisBoard->canvas),
				    pixmap,
				    (double) plane_x,
				    (double) plane_y,
				     "width", (double) gdk_pixbuf_get_width(pixmap)*imageZoom,
				     "height", (double) gdk_pixbuf_get_height(pixmap)*imageZoom,
				     NULL);
  g_signal_connect(planeitem, "button-press-event",
		     (GtkSignalFunc) item_event,
		     NULL);
  g_signal_connect(planeitem, "enter_notify_event",
		     (GtkSignalFunc) gc_item_focus_event,
		     NULL);
  gdk_pixbuf_unref(pixmap);

  windspeed = (3 + rand() % (100 * gcomprisBoard->level) / 100);
  if(rand()%2==0)
    windspeed *= -1;
  if (gcomprisBoard->level >= 4)
    windspeed *= 2;

  /* Drop a cloud */
  gtk_timeout_add (200,
		   (GtkFunction) paratrooper_drop_clouds, NULL);

  /* Display the target */
  g_free(str);
  str = g_strdup_printf("%s%s", pixmapsdir, "fishingboat.png");
  pixmap = gc_pixmap_load(str);
  boat_x = 350;
  boat_y = gcomprisBoard->height-100;
  boat_landarea_y = boat_y+20;
  boat_length = gdk_pixbuf_get_width(pixmap)*imageZoom;
  boatitem = goo_canvas_image_new (goo_canvas_get_root_item(gcomprisBoard->canvas),
				   pixmap,
				   (double) boat_x,
				   (double) boat_y,
				    "width", (double) gdk_pixbuf_get_width(pixmap)*imageZoom,
				    "height", (double) gdk_pixbuf_get_height(pixmap),
				    NULL);
  gdk_pixbuf_unref(pixmap);

  /* Prepare the parachute */
  if (drop_tux_id) {
    gtk_timeout_remove (drop_tux_id);
    drop_tux_id = 0;
  }

  paratrooperItem.status	= TUX_INPLANE;
  paratrooperItem.x		= 0;
  paratrooperItem.y		= 60;
  paratrooperItem.speed		= 3;

  paratrooperItem.rootitem = \
    goo_canvas_group_new (goo_canvas_get_root_item(gcomprisBoard->canvas), NULL);
  goo_canvas_item_translate(paratrooperItem.rootitem,
			    paratrooperItem.x,
			    paratrooperItem.y);

  g_free(str);
  str = g_strdup_printf("%s%s", pixmapsdir, "minitux.png");
  pixmap = gc_pixmap_load(str);

  paratrooperItem.paratrooper = \
    goo_canvas_image_new (paratrooperItem.rootitem,
			  pixmap,
			  -gdk_pixbuf_get_width(pixmap)/2,
			  -gdk_pixbuf_get_height(pixmap)/2,
			  NULL);

  g_object_set (paratrooperItem.paratrooper, "visibility", GOO_CANVAS_ITEM_INVISIBLE, NULL);
  gdk_pixbuf_unref(pixmap);
  g_signal_connect(paratrooperItem.paratrooper, "button-press-event",
		     (GtkSignalFunc) item_event,
		     NULL);
  g_signal_connect(paratrooperItem.paratrooper, "enter_notify_event",
		     (GtkSignalFunc) gc_item_focus_event,
		     NULL);

  g_free(str);
  str = g_strdup_printf("%s%s", pixmapsdir, "parachute.png");
  pixmap = gc_pixmap_load(str);

  paratrooperItem.parachute = goo_canvas_image_new (paratrooperItem.rootitem,
						    pixmap,
						    (double) -gdk_pixbuf_get_width(pixmap)/2,
						    (double) -(gdk_pixbuf_get_height(pixmap)/2)-60,
						     NULL);
  g_object_set (paratrooperItem.parachute, "visibility", GOO_CANVAS_ITEM_INVISIBLE, NULL);
  gdk_pixbuf_unref(pixmap);

  g_free (str);

  paratrooperItem.instruct = goo_canvas_text_new (goo_canvas_get_root_item (gcomprisBoard->canvas),
						  _("Control fall speed with up and down arrow keys."),
						  (double) gcomprisBoard->width / 2.0,
						  (double) 130,
						  -1,
						  GTK_ANCHOR_CENTER,
						  "font", gc_skin_font_board_medium,
						  "fill_color_rgba", gc_skin_color_title,
						  NULL);
  g_object_set (paratrooperItem.instruct, "visibility", GOO_CANVAS_ITEM_INVISIBLE, NULL);
}


/* Move the plane */
static void paratrooper_move_plane(GooCanvasItem *item)
{

  if(plane_x>gcomprisBoard->width && planespeed_x>0)
    {
      GooCanvasBounds bounds;
      goo_canvas_item_get_bounds    (item,
				     &bounds);

      goo_canvas_item_translate(item,
				-gcomprisBoard->width-(bounds.x2-bounds.x1),
				planespeed_y);
      plane_x = plane_x - gcomprisBoard->width - (bounds.x2-bounds.x1);

      if(paratrooperItem.status!=TUX_INPLANE)
	g_object_set (item, "visibility", GOO_CANVAS_ITEM_INVISIBLE, NULL);
    }

  goo_canvas_item_translate(item, (double)planespeed_x, (double)planespeed_y);
  plane_x+=planespeed_x;
  plane_y+=planespeed_y;
}

static void paratrooper_move_cloud(CloudItem *clouditem)
{
  GooCanvasItem *item = clouditem->rootitem;
  GooCanvasBounds bounds;

  goo_canvas_item_translate(item, windspeed, 0.0);

  goo_canvas_item_get_bounds (item, &bounds);

  /* Manage the wrapping for the cloud */
  if(windspeed<0 && bounds.x2<0)
    {
      goo_canvas_item_translate(item, gcomprisBoard->width, 0.0);
    }
  else if(windspeed>0 && bounds.x1>gcomprisBoard->width)
    {
      goo_canvas_item_translate(item, -gcomprisBoard->width, 0.0);
    }
}

static void paratrooper_destroy_item(CloudItem *clouditem)
{
  GooCanvasItem *item = clouditem->rootitem;

  item_list = g_list_remove (item_list, clouditem);
  item2del_list = g_list_remove (item2del_list, clouditem);
  goo_canvas_item_remove(item);

  g_free(clouditem);
}

/* Destroy items that falls out of the canvas */
static void paratrooper_destroy_items()
{
  CloudItem *clouditem;

  while(g_list_length(item2del_list)>0)
    {
      clouditem = g_list_nth_data(item2del_list, 0);
      paratrooper_destroy_item(clouditem);
    }
}

/* Destroy all the items */
static void paratrooper_destroy_all_items()
{
  CloudItem *clouditem;

  while(g_list_length(item_list)>0)
    {
      clouditem = g_list_nth_data(item_list, 0);
      paratrooper_destroy_item(clouditem);
    }

  if(planeitem)
    {
      goo_canvas_item_remove(planeitem);
      planeitem = NULL;
    }

  if(boatitem)
    {
      goo_canvas_item_remove(boatitem);
      boatitem = NULL;
    }

  if(paratrooperItem.rootitem)
    {
      goo_canvas_item_remove(paratrooperItem.rootitem);
      paratrooperItem.rootitem = NULL;
    }
  if (paratrooperItem.instruct) {
    goo_canvas_item_remove(paratrooperItem.instruct);
    paratrooperItem.instruct = NULL;
  }
}

/*
 * This does the moves of the game items on the play canvas
 *
 */
static gint paratrooper_move_items (GtkWidget *widget, gpointer data)
{
  g_list_foreach (item_list, (GFunc) paratrooper_move_cloud, NULL);

  /* Destroy items that falls out of the canvas */
  paratrooper_destroy_items();

  /* move the plane */
  paratrooper_move_plane(planeitem);

  dummy_id = gtk_timeout_add (speed,
			      (GtkFunction) paratrooper_move_items, NULL);

  return(FALSE);
}

/*
 * This does the moves of the game's paratropper
 *
 */
static gint paratrooper_move_tux (GtkWidget *widget, gpointer data)
{
  double offset;

  /* Manage the wrapping */
  if(paratrooperItem.x<0) {
    paratrooperItem.x+=gcomprisBoard->width;
    goo_canvas_item_translate(paratrooperItem.rootitem, gcomprisBoard->width, 0);
  }
  if(paratrooperItem.x>gcomprisBoard->width) {
    paratrooperItem.x-=gcomprisBoard->width;
    goo_canvas_item_translate(paratrooperItem.rootitem, -gcomprisBoard->width, 0);
  }

  offset = (windspeed / 2 + 15 * paratrooperItem.drift) / 16;
  paratrooperItem.drift = offset;

  if (paratrooperItem.status == TUX_DROPPING && gcomprisBoard->level >= 4)
    paratrooperItem.speed *= 1.05;
  if (paratrooperItem.status == TUX_FLYING && paratrooperItem.speed > 3 &&
      !paratrooperItem.speed_override)
    paratrooperItem.speed /= 1.2;
  goo_canvas_item_translate(paratrooperItem.rootitem, offset, paratrooperItem.speed);
  paratrooperItem.y += paratrooperItem.speed;
  paratrooperItem.x += offset;

  /* Check we reached the target */
  if(paratrooperItem.y>boat_landarea_y)
    {
      if(paratrooperItem.x>boat_x && paratrooperItem.x<boat_x+boat_length &&
	 paratrooperItem.status==TUX_FLYING)
	{
	  g_object_set (paratrooperItem.parachute, "visibility", GOO_CANVAS_ITEM_INVISIBLE, NULL);
	  paratrooperItem.status = TUX_LANDED;
	  next_state();
	}
      else
	{
	  if(paratrooperItem.y<gcomprisBoard->height-20)
	    drop_tux_id = gtk_timeout_add (150,
					   (GtkFunction) paratrooper_move_tux, NULL);
	  else
	    {
	      g_object_set (paratrooperItem.parachute, "visibility", GOO_CANVAS_ITEM_INVISIBLE, NULL);
	      paratrooperItem.status = TUX_CRASHED;
	      next_state();
	    }
	}
    }
  else
    {
      drop_tux_id = gtk_timeout_add (150,
				     (GtkFunction) paratrooper_move_tux, NULL);
    }

  return(FALSE);
}

static GooCanvasItem *paratrooper_create_cloud(GooCanvasItem *parent)
{
  GdkPixbuf *pixmap = NULL;
  GooCanvasItem *itemgroup;
  char *str = NULL;
  int x;
  CloudItem *clouditem;


  str = g_strdup_printf("%s%s", pixmapsdir, "cloud.png");
  pixmap = gc_pixmap_load(str);

  if(windspeed>0)
    x = 0;
  else
    x = gcomprisBoard->width;

  itemgroup = \
    goo_canvas_group_new (parent, NULL);
  goo_canvas_item_translate(itemgroup,
			    x,
			    (rand()%(gcomprisBoard->height-200-
				     (guint)(gdk_pixbuf_get_height(pixmap)*
					     imageZoom))));


  goo_canvas_image_new (itemgroup,
			pixmap,
			(double) -gdk_pixbuf_get_width(pixmap)*imageZoom/2,
			(double) -gdk_pixbuf_get_height(pixmap)*imageZoom/2,
			 "width", (double) gdk_pixbuf_get_width(pixmap)*imageZoom,
			 "height", (double) gdk_pixbuf_get_height(pixmap)*imageZoom,
			 NULL);
  gdk_pixbuf_unref(pixmap);


  /* The plane is always on top */
  goo_canvas_item_raise(planeitem, NULL);

  clouditem = g_malloc(sizeof(CloudItem));
  clouditem->rootitem = itemgroup;

  item_list = g_list_append (item_list, clouditem);

  g_free (str);

  return (itemgroup);
}

static void paratrooper_add_new_item()
{
  paratrooper_create_cloud(goo_canvas_get_root_item(gcomprisBoard->canvas));
}

/*
 * This is called on a low frequency and is used to drop new items
 *
 */
static gint paratrooper_drop_clouds (GtkWidget *widget, gpointer data)
{
  paratrooper_add_new_item();

  return (FALSE);
}



/*
 * This is the state machine of the paratrooper
 */
void next_state()
{

  switch(paratrooperItem.status)
    {
    case TUX_INPLANE:
      gc_sound_play_ogg ("sounds/tuxok.wav", NULL);
      goo_canvas_item_translate(paratrooperItem.rootitem, plane_x+100, 0);
      paratrooperItem.x += plane_x+100;
      g_object_set (paratrooperItem.paratrooper, "visibility", GOO_CANVAS_ITEM_VISIBLE, NULL);
      paratrooperItem.status = TUX_DROPPING;
      paratrooperItem.drift = planespeed_x;
      drop_tux_id = gtk_timeout_add (10, (GtkFunction) paratrooper_move_tux, NULL);
      break;
    case TUX_DROPPING:
      gc_sound_play_ogg ("sounds/eraser2.wav", NULL);
      goo_canvas_item_lower(paratrooperItem.parachute, NULL);
      g_object_set (paratrooperItem.parachute, "visibility", GOO_CANVAS_ITEM_VISIBLE, NULL);
      paratrooperItem.status = TUX_FLYING;
      paratrooperItem.speed_override = 0;
      if (gcomprisBoard->level >= 2) {
	goo_canvas_item_raise (paratrooperItem.instruct, NULL);
	g_object_set (paratrooperItem.instruct, "visibility", GOO_CANVAS_ITEM_VISIBLE, NULL);
      }
      break;
    case TUX_LANDED:
      gc_sound_play_ogg ("sounds/tuxok.wav", NULL);
      g_object_set (paratrooperItem.instruct, "visibility", GOO_CANVAS_ITEM_INVISIBLE, NULL);
      gamewon = TRUE;
      gc_bonus_display(gamewon, GC_BONUS_TUX);
      break;
    case TUX_CRASHED:
      /* Restart */
      gc_sound_play_ogg ("sounds/bubble.wav", NULL);
      g_object_set (paratrooperItem.instruct, "visibility", GOO_CANVAS_ITEM_INVISIBLE, NULL);
      goo_canvas_item_translate(paratrooperItem.rootitem, -paratrooperItem.x, -paratrooperItem.y+60);
      paratrooperItem.status	= TUX_INPLANE;
      paratrooperItem.x		= 0;
      paratrooperItem.y		= 60;
      paratrooperItem.speed	= 3;
      g_object_set (paratrooperItem.paratrooper, "visibility", GOO_CANVAS_ITEM_INVISIBLE, NULL);
      g_object_set (planeitem, "visibility", GOO_CANVAS_ITEM_VISIBLE, NULL);
      break;
    default:
      break;
    }
}

static gboolean
item_event (GooCanvasItem  *item,
	    GooCanvasItem  *target,
	    GdkEventButton *event,
	    gpointer data)
{

  if(!gcomprisBoard)
    return FALSE;

   switch (event->type)
     {
     case GDK_BUTTON_PRESS:
       next_state();
       break;

     default:
       break;
     }

   return FALSE;
 }