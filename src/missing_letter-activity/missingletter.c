/* gcompris - missingletter.c
 *
 * Copyright (C) 2001 Pascal Georges
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

#include <string.h>

#include "gcompris/gcompris.h"

/* libxml includes */
#include <libxml/tree.h>
#include <libxml/parser.h>

#define SOUNDLISTFILE PACKAGE

#define MAX_PROPOSAL 6
typedef struct _Board {
  gchar *pixmapfile;
  gchar *question;
  gchar *answer;
  gchar *text[MAX_PROPOSAL + 1];
  guint solution;
} Board;

static GcomprisBoard *gcomprisBoard = NULL;
static gboolean board_paused = TRUE;

static void		 start_board (GcomprisBoard *agcomprisBoard);
static void		 pause_board (gboolean pause);
static void		 end_board (void);
static gboolean		 is_our_board (GcomprisBoard *gcomprisBoard);
static void		 set_level (guint level);
static void		 process_ok(gchar *answer);
static void		 highlight_selected(GooCanvasItem *);
static void		 game_won(void);
static void		 config_start(GcomprisBoard *agcomprisBoard,
					     GcomprisProfile *aProfile);
static void		 config_stop(void);

static int gamewon;

/* XML */
static gboolean		 read_xml_file(char *fname);
static void		 init_xml(guint level);
static void		 add_xml_data(xmlDocPtr, xmlNodePtr, GNode *);
static void		 parse_doc(xmlDocPtr doc);
static gboolean		 read_xml_file(char *fname);
static void		 destroy_board_list();
static void		 destroy_board(Board * board);

/* This is the list of boards */
static GList *board_list = NULL;

#define HORIZONTAL_SEPARATION 30

/* ================================================================ */
static int board_number; // between 0 and board_list.length-1

static GooCanvasItem *boardRootItem = NULL;

static GooCanvasItem *text    = NULL;
static GooCanvasItem *text_s  = NULL;
static GooCanvasItem *selected_button = NULL;

static void missing_letter_create_item(GooCanvasItem *parent);
static void missing_letter_destroy_all_items(void);
static void missing_letter_next_level(void);
static gboolean item_event (GooCanvasItem  *item,
			    GooCanvasItem  *target,
			    GdkEventButton *event,
			    gpointer data);

/* Description of this plugin */
static BoardPlugin menu_bp =
  {
    NULL,
    NULL,
    N_("Reading"),
    N_("Learn how to read"),
    "Pascal Georges pascal.georges1@free.fr>",
    NULL,
    NULL,
    NULL,
    NULL,
    start_board,
    pause_board,
    end_board,
    is_our_board,
    NULL,
    NULL,
    set_level,
    NULL,
    NULL,
    config_start,
    config_stop
  };

/*
 * Main entry point mandatory for each Gcompris's game
 * ---------------------------------------------------
 *
 */

GET_BPLUGIN_INFO(missingletter)

/*
 * in : boolean TRUE = PAUSE : FALSE = CONTINUE
 *
 */
static void pause_board (gboolean pause)
{
  if(gcomprisBoard==NULL)
    return;

  gc_bar_hide(FALSE);

  if(gamewon == TRUE && pause == FALSE) /* the game is won */
    {
      game_won();
    }

  board_paused = pause;
}

/*
 */
static void
start_board (GcomprisBoard *agcomprisBoard)
{
  gchar *filename = NULL;
  GHashTable *config = gc_db_get_board_conf();

  gc_locale_set(g_hash_table_lookup( config, "locale"));

  g_hash_table_destroy(config);

  if(agcomprisBoard!=NULL)
    {
      gcomprisBoard=agcomprisBoard;
      gc_set_background(goo_canvas_get_root_item(gcomprisBoard->canvas),
			"missing_letter/missingletter-bg.jpg");
      gcomprisBoard->level=1;
      gcomprisBoard->sublevel=1;

      /* Calculate the maxlevel based on the available data file for this board */
      gcomprisBoard->maxlevel=1;

      while( (filename = gc_file_find_absolute("%s/board%d.xml",
					       gcomprisBoard->boarddir,
					       gcomprisBoard->maxlevel++,
					       NULL)) )
	{
	  g_free(filename);

	}
      g_free(filename);

      gcomprisBoard->maxlevel -= 2;

      gc_bar_set(GC_BAR_CONFIG | GC_BAR_LEVEL);

      missing_letter_next_level();

      gamewon = FALSE;
      pause_board(FALSE);
    }
}

static void end_board ()
{

  if(gcomprisBoard!=NULL)
    {
      pause_board(TRUE);
      gc_score_end();
      missing_letter_destroy_all_items();
      destroy_board_list();
    }

  gc_locale_reset();

  gcomprisBoard = NULL;
}

static void
set_level (guint level)
{

  if(gcomprisBoard!=NULL)
    {
      gcomprisBoard->level=level;
      gcomprisBoard->sublevel=1;
      missing_letter_next_level();
    }
}

static gboolean
is_our_board (GcomprisBoard *gcomprisBoard)
{
  if (gcomprisBoard)
    {
      if(g_strcasecmp(gcomprisBoard->type, "missingletter")==0)
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
/* set initial values for the next level */
static void
missing_letter_next_level()
{
  gc_bar_set_level(gcomprisBoard);

  missing_letter_destroy_all_items();
  selected_button = NULL;
  gamewon = FALSE;

  destroy_board_list();
  init_xml(gcomprisBoard->level);

  gcomprisBoard->number_of_sublevel = g_list_length(board_list);

  gc_score_end();
  gc_score_start(SCORESTYLE_NOTE,
		 50,
		 gcomprisBoard->height - 45,
		 gcomprisBoard->number_of_sublevel);


  gc_score_set(gcomprisBoard->sublevel);

  /* Try the next level */
  missing_letter_create_item(goo_canvas_get_root_item(gcomprisBoard->canvas));
}

static void
missing_letter_next_sublevel()
{
  missing_letter_destroy_all_items();
  selected_button = NULL;
  gamewon = FALSE;

  gc_score_set(gcomprisBoard->sublevel);

  /* Try the next level */
  missing_letter_create_item(goo_canvas_get_root_item(gcomprisBoard->canvas));
}

/* ==================================== */
/* Destroy all the items */
static void
missing_letter_destroy_all_items()
{
  if(boardRootItem!=NULL)
    goo_canvas_item_remove(boardRootItem);

  boardRootItem = NULL;
}
/* ==================================== */
static void
missing_letter_create_item(GooCanvasItem *parent)
{
  int xOffset, yOffset;
  GdkPixbuf *button_pixmap = NULL;
  GdkPixbuf *pixmap = NULL;
  Board *board;

  /* This are the values of the area in which we must display the image */
  gint img_area_x = 290;
  gint img_area_y = 80;
  gint img_area_w = 440;
  gint img_area_h = 310;

  /* this is the coordinate of the text to find */
  gint txt_area_x = 515;
  gint txt_area_y = 435;

  guint vertical_separation;
  gint i;

  board_number = gcomprisBoard->sublevel-1;

  g_assert(board_number >= 0  && board_number < g_list_length(board_list));

  boardRootItem = \
    goo_canvas_group_new (goo_canvas_get_root_item(gcomprisBoard->canvas),
			  NULL);

  button_pixmap = gc_skin_pixmap_load("button.png");
  /* display the image */
  board = g_list_nth_data(board_list, board_number);
  g_assert(board != NULL);

  pixmap = gc_pixmap_load(board->pixmapfile);

  text_s = goo_canvas_text_new (boardRootItem,
				_(board->question),
				(double) txt_area_x + 1.0,
				(double) txt_area_y + 1.0,
				-1,
				GTK_ANCHOR_CENTER,
				"font", gc_skin_font_board_huge_bold,
				"fill_color_rgba", gc_skin_get_color("missingletter/shadow"),
				NULL);

  text = goo_canvas_text_new (boardRootItem,
			      _(board->question),
			      (double) txt_area_x,
			      (double) txt_area_y,
			      -1,
			      GTK_ANCHOR_CENTER,
			      "font", gc_skin_font_board_huge_bold,
			      "fill_color_rgba", gc_skin_get_color("missingletter/question"),
			      NULL);

  goo_canvas_image_new (boardRootItem,
			pixmap,
			img_area_x+(img_area_w - gdk_pixbuf_get_width(pixmap))/2,
			img_area_y+(img_area_h - gdk_pixbuf_get_height(pixmap))/2,
			NULL);
  gdk_pixbuf_unref(pixmap);

  /* Calc the number of proposals */
  i = 0;
  while(board->text[i++]);

  vertical_separation = 10 + 20 / i;

  yOffset = ( gcomprisBoard->height
	      - i * gdk_pixbuf_get_height(button_pixmap)
	      - 2 * vertical_separation) / 2 - 20;
  xOffset = (img_area_x - gdk_pixbuf_get_width(button_pixmap))/2;

  i = 0;
  while(board->text[i])
    {
      GooCanvasItem *button;
      GooCanvasItem *item;
      GooCanvasItem *group = goo_canvas_group_new (boardRootItem,
						   NULL);

      button = goo_canvas_image_new (group,
				     button_pixmap,
				     xOffset,
				     yOffset,
				     NULL);

      g_object_set_data(G_OBJECT(group),
		      "answer", board->answer);

      g_object_set_data(G_OBJECT(group),
		      "button", button);

      g_object_set_data(G_OBJECT(group),
			"solution", GINT_TO_POINTER(board->solution));

      g_signal_connect(button, "button_press_event",
		       (GtkSignalFunc) item_event,
		       GINT_TO_POINTER(i));

      item = goo_canvas_text_new (group,
				  board->text[i],
				  xOffset + gdk_pixbuf_get_width(button_pixmap)/2 + 1.0,
				  yOffset + gdk_pixbuf_get_height(button_pixmap)/2 + 1.0,
				  -1,
				  GTK_ANCHOR_CENTER,
				  "font", gc_skin_font_board_huge_bold,
				  "fill_color_rgba", gc_skin_color_shadow,
				  NULL);

      g_signal_connect(item, "button_press_event",
		       (GtkSignalFunc) item_event,
		       GINT_TO_POINTER(i));

      item = goo_canvas_text_new (group,
				  board->text[i],
				  xOffset + gdk_pixbuf_get_width(button_pixmap)/2,
				  yOffset + gdk_pixbuf_get_height(button_pixmap)/2,
				  -1,
				  GTK_ANCHOR_CENTER,
				  "font", gc_skin_font_board_huge_bold,
				  "fill_color_rgba", gc_skin_color_text_button,
				  NULL);

      g_signal_connect(item, "button_press_event",
		       (GtkSignalFunc) item_event,
		       GINT_TO_POINTER(i));

      yOffset += gdk_pixbuf_get_height(button_pixmap) + vertical_separation;

      i++;
    }

  gdk_pixbuf_unref(button_pixmap);
}

/* ==================================== */
static void game_won() {
  gcomprisBoard->sublevel++;

  if(gcomprisBoard->sublevel>gcomprisBoard->number_of_sublevel)
    {
      /* Try the next level */
      gcomprisBoard->sublevel=1;
      gcomprisBoard->level++;
      if(gcomprisBoard->level>gcomprisBoard->maxlevel)
	{
	  gc_bonus_end_display(GC_BOARD_FINISHED_TUXPLANE);
	  return;
	}
      missing_letter_next_level();
    }
  else
    missing_letter_next_sublevel();
}

/* ==================================== */
static gboolean
process_ok_timeout()
{
  gc_bonus_display(gamewon, GC_BONUS_FLOWER);
  return FALSE;
}

static void
process_ok(gchar *answer)
{
  if (gamewon) {
    g_object_set(text,   "text", answer, NULL);
    g_object_set(text_s, "text", answer, NULL);
  }
  // leave time to display the right answer
  gc_bar_hide(TRUE);
  g_timeout_add(TIME_CLICK_TO_BONUS, process_ok_timeout, NULL);
}

/* ==================================== */
static gboolean
item_event (GooCanvasItem *item,
	    GooCanvasItem *target,
	    GdkEventButton *event,
	    gpointer data)
{
  gint button_id = GPOINTER_TO_INT(data);
  GooCanvasItem *button;

  if(board_paused)
    return FALSE;

  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
      board_paused = TRUE;

      gint solution = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(goo_canvas_item_get_parent(item)),
							"solution"));

      if ( button_id == solution )
	gamewon = TRUE;
      else
	gamewon = FALSE;

      button = (GooCanvasItem*)g_object_get_data(G_OBJECT(goo_canvas_item_get_parent(item)),
						   "button");

      gchar *answer = \
	(gchar*)g_object_get_data(G_OBJECT(goo_canvas_item_get_parent(item)),
				  "answer");

      highlight_selected(button);
      process_ok(answer);
      break;

    default:
      break;
    }
  return FALSE;
}
/* ==================================== */
static void
highlight_selected(GooCanvasItem * button)
{
  GdkPixbuf *button_pixmap_selected = NULL, *button_pixmap = NULL;

  if (selected_button != NULL && selected_button != button)
    {
      button_pixmap = gc_skin_pixmap_load("button.png");
      g_object_set(selected_button, "pixbuf", button_pixmap, NULL);
      gdk_pixbuf_unref(button_pixmap);
    }

  if (selected_button != button)
    {
      button_pixmap_selected = gc_skin_pixmap_load("button_selected.png");
      g_object_set(button, "pixbuf", button_pixmap_selected, NULL);
      selected_button = button;
      gdk_pixbuf_unref(button_pixmap_selected);
    }
}

/* ===================================
 *                XML stuff
 *                Ref : shapegame.c
 * ==================================== */
static void
init_xml(guint level)
{
  gchar *filename;

  filename = gc_file_find_absolute("%s/board%d.xml",
				   gcomprisBoard->boarddir,
				   level);

  g_assert(read_xml_file(filename)== TRUE);
  g_free(filename);
}

/* ==================================== */
static void
add_xml_data(xmlDocPtr doc, xmlNodePtr xmlnode, GNode * child)
{
  Board * board = g_new0(Board,1);
  guint text_index = 0;

  xmlnode = xmlnode->xmlChildrenNode;

  while ((xmlnode = xmlnode->next) != NULL) {

    if (!strcmp((char *)xmlnode->name, "pixmapfile"))
      board->pixmapfile = \
	(gchar *)xmlNodeListGetString(doc, xmlnode->xmlChildrenNode, 1);

    if (!strcmp((char *)xmlnode->name, "data"))
      {
	gchar *data = \
	  gettext((gchar *)xmlNodeListGetString(doc,
						xmlnode->xmlChildrenNode,
						1));
	gchar **all_answer = g_strsplit(data, "/", MAX_PROPOSAL + 2);
	guint i = 0;
	/* Dont free data, it's a gettext static message */

	board->answer = g_strdup(all_answer[i++]);
	board->question = g_strdup(all_answer[i++]);
	board->solution = 0;

	while(all_answer[i] && text_index < MAX_PROPOSAL)
	  {
	    board->text[text_index++] = g_strdup(all_answer[i++]);
	  }

	g_strfreev(all_answer);

      }
    xmlnode = xmlnode->next;
  }

  /* Check there is enough information to play it */
  if ( board->pixmapfile == NULL
       || board->text[0] == NULL
       || board->text[1] == NULL )
    {
      gc_dialog(_("Data file for this level is not properly formatted."),
		gc_board_stop);
      g_free(board);
      return;
    }

  /* Randomize the set */
  {
    guint c = text_index * 2;
    gchar *text;

    while(c--)
      {
	guint i1 = g_random_int_range(0, text_index);
	guint i2 = g_random_int_range(0, text_index);

	/* Swap entries */
	text = board->text[i1];
	board->text[i1] = board->text[i2];
	board->text[i2] = text;

	if(i1 == board->solution)
	  board->solution = i2;
	else if(i2 == board->solution)
	  board->solution = i1;
      }
  }

  /* Insert boards randomly in the list */
  if(g_random_int_range(0, 2))
    board_list = g_list_append (board_list, board);
  else
    board_list = g_list_prepend (board_list, board);
}

/* ==================================== */
static void
parse_doc(xmlDocPtr doc)
{
  xmlNodePtr node;

  for(node = doc->children->children; node != NULL; node = node->next) {
    if ( g_strcasecmp((gchar *)node->name, "Board") == 0 )
      add_xml_data(doc, node,NULL);
  }

}

/* ==================================== */
/* read an xml file into our memory structures and update our view,
   dump any old data we have in memory if we can load a new set */
static gboolean
read_xml_file(char *fname)
{
  /* pointer to the new doc */
  xmlDocPtr doc;

  g_return_val_if_fail(fname!=NULL,FALSE);

  /* parse the new file and put the result into newdoc */
  doc = gc_net_load_xml(fname);

  /* in case something went wrong */
  if(!doc)
    return FALSE;

  if(/* if there is no root element */
     !doc->children ||
     /* if it doesn't have a name */
     !doc->children->name ||
     /* if it isn't a ImageId node */
     g_strcasecmp((char *)doc->children->name,"missing_letter")!=0) {
    xmlFreeDoc(doc);
    return FALSE;
  }

  parse_doc(doc);
  xmlFreeDoc(doc);
  return TRUE;
}
/* ======================================= */
static void
destroy_board_list()
{
  Board *board;

  while(g_list_length(board_list)>0)
    {
      board = g_list_nth_data(board_list, 0);
      board_list = g_list_remove (board_list, board);
      destroy_board(board);
    }
}

/* ======================================= */
static void
destroy_board(Board * board)
{
  guint i = 0;

  g_free(board->pixmapfile);
  g_free(board->answer);
  g_free(board->question);
  while(board->text[i])
    g_free(board->text[i++]);

  g_free(board);
}

/* ************************************* */
/* *            Configuration          * */
/* ************************************* */


/* ======================= */
/* = config_start        = */
/* ======================= */

static GcomprisProfile *profile_conf;
static GcomprisBoard   *board_conf;

/* GHFunc */
static void save_table (gpointer key,
			gpointer value,
			gpointer user_data)
{
  gc_db_set_board_conf ( profile_conf,
			 board_conf,
			 (gchar *) key,
			 (gchar *) value);
}

static GcomprisConfCallback
conf_ok(GHashTable *table)
{
  if (!table){
    if (gcomprisBoard)
      pause_board(FALSE);
    return NULL;
  }

  g_hash_table_foreach(table, (GHFunc) save_table, NULL);

  if (gcomprisBoard){
    gc_locale_reset();

    GHashTable *config;

    if (profile_conf)
      config = gc_db_get_board_conf();
    else
      config = table;

    gc_locale_set(g_hash_table_lookup( config, "locale"));

    if (profile_conf)
      g_hash_table_destroy(config);

    destroy_board_list();

    init_xml(gcomprisBoard->level);

    missing_letter_next_level();

  }

    board_conf = NULL;
  profile_conf = NULL;

  return NULL;
}

static void
config_start(GcomprisBoard *agcomprisBoard,
		    GcomprisProfile *aProfile)
{
  board_conf = agcomprisBoard;
  profile_conf = aProfile;

  if (gcomprisBoard)
    pause_board(TRUE);

  gchar *label = g_strdup_printf(_("<b>%s</b> configuration\n for profile <b>%s</b>"),
				 agcomprisBoard->name,
				 aProfile ? aProfile->name : "");

  gc_board_config_window_display( label,
				 (GcomprisConfCallback )conf_ok);

  g_free(label);

  /* init the combo to previously saved value */
  GHashTable *config = gc_db_get_conf( profile_conf, board_conf);

  gchar *locale = g_hash_table_lookup( config, "locale");

  gc_board_config_combo_locales( locale);

}


/* ======================= */
/* = config_stop        = */
/* ======================= */
static void
config_stop()
{
}