/*
 * money_widget.c
 *
 * Copyright (C) 2002  Robert Wilhelm
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Bruno Coudoin
 *
 */

#include <stdlib.h>
#include <string.h>
#include "money_widget.h"

struct _Money_WidgetPrivate {
  GooCanvasItem	*rootItem;	/* The canvas to display our euros in             */
  double		 x1;		/* Coordinate of the widget                       */
  double		 y1;
  double		 x2;
  double		 y2;
  guint			 columns;	/* Number of columns				  */
  guint			 lines;		/* Number of lines				  */
  guint			 next_spot;	/* Next spot to display a money item		  */
  double		 total;		/* The number of euro in this pocket              */
  Money_Widget		*targetWidget;	/* Target money widget to add when remove here	  */
  gboolean		 display_total;	/* Display or not the total of this pocket        */

  GooCanvasItem	*item_total;	/* Item to display the total                      */
  GList			*moneyItemList;	/* List of all the items			  */
};


typedef struct _MoneyList        MoneyList;

struct _MoneyList {
  const gchar		*image;
  const double		 value;
};

// List of images to use in the game
static const MoneyList euroList[] =
{
  { "money/euro/c1c.png",		0.01  },
  { "money/euro/c2c.png",		0.02  },
  { "money/euro/c5c.png",		0.05  },
  { "money/euro/c10c.png",		0.1   },
  { "money/euro/c20c.png",		0.20  },
  { "money/euro/c50c.png",		0.5   },
  { "money/euro/c1e.png",		1.0   },
  { "money/euro/c2e.png",		2.0   },
  { "money/euro/p5e.png",		5.0   },
  { "money/euro/p10e.png",		10.0  },
  { "money/euro/p20e.png",		20.0  },
  { "money/euro/p50e.png",		50.0  },
  { NULL,				-1.0  }
};

typedef struct {
  Money_Widget		*moneyWidget;
  GooCanvasItem	*item;
  MoneyEuroType		 value;
  gboolean		 inPocket;
} MoneyItem;

#define BORDER_GAP	6

/* Prototypes */
static void class_init (Money_WidgetClass *class);
static void init (Money_Widget *pos);
static void money_display_total(Money_Widget *moneyWidget);
static gint item_event(GooCanvasItem *item, GdkEvent *event, MoneyItem *moneyitem);

GtkType
money_widget_get_type ()
{
	static guint money_widget_type = 0;

	if (!money_widget_type) {
		GtkTypeInfo money_widget_info = {
			"Money_Widget",
			sizeof (Money_Widget),
			sizeof (Money_WidgetClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			(gpointer) NULL,
			(gpointer) NULL,
			(GtkClassInitFunc) NULL
		};
		money_widget_type = gtk_type_unique (gtk_object_get_type (),
						 &money_widget_info);
	}

	return money_widget_type;
}

#if 0
static void
finalize (GtkObject *object)
{
  MoneyItem	  *moneyitem;

  Money_Widget *moneyWidget = (Money_Widget *) object;

  printf("ERROR : Finalize is NEVER CALLED\n");
  /* FIXME: CLEANUP CODE UNTESTED */
  while(g_list_length(moneyWidget->priv->moneyItemList)>0)
    {
      moneyitem = g_list_nth_data(moneyWidget->priv->moneyItemList, 0);
      moneyWidget->priv->moneyItemList = g_list_remove (moneyWidget->priv->moneyItemList,
							moneyitem);
      g_free(moneyitem);
    }

  g_free (moneyWidget->priv);

  moneyWidget->priv = NULL;

}
#endif

static void
class_init (Money_WidgetClass *class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;

  //2  object_class->destroy = finalize;
}

static void
init (Money_Widget *pos)
{
	pos->priv = g_new0 (Money_WidgetPrivate, 1);

}

GtkObject *
money_widget_new ()
{
	return GTK_OBJECT (gtk_type_new (money_widget_get_type ()));
}

Money_Widget *
money_widget_copy (Money_Widget *pos)
{
	Money_Widget *cpPos;

	cpPos = MONEY_WIDGET (money_widget_new ());

	memcpy (cpPos->priv, pos->priv, sizeof (Money_WidgetPrivate));

	return cpPos;
}

void
money_widget_set_target (Money_Widget *moneyWidget,
			 Money_Widget *targetWidget)
{
  moneyWidget->priv->targetWidget = targetWidget;
}

void
money_widget_set_position (Money_Widget *moneyWidget,
			   GooCanvasItem *rootItem,
			   double x1,
			   double y1,
			   double x2,
			   double y2,
			   guint  columns,
			   guint  lines,
			   gboolean display_total)
{
  g_return_if_fail (moneyWidget != NULL);

  moneyWidget->priv->rootItem = rootItem;
  moneyWidget->priv->x1 = x1;
  moneyWidget->priv->y1 = y1;
  moneyWidget->priv->x2 = x2;
  moneyWidget->priv->y2 = y2;
  moneyWidget->priv->columns = columns;
  moneyWidget->priv->lines = lines;
  moneyWidget->priv->next_spot = 0;
  moneyWidget->priv->display_total = display_total;

  /* Uncomment to display the limits
    goo_canvas_item_new (GOO_CANVAS_GROUP(rootItem),
			 goo_canvas_rect_get_type (),
			 "x1", (double) x1,
			 "y1", (double) y1,
			 "x2", (double) x2,
			 "y2", (double) y2,
			 "stroke-color", "red",
			 "width_pixels", 2,
			 NULL);
  */

  moneyWidget->priv->item_total =  goo_canvas_text_new(rootItem,
						       "",
						       (double) x1+(x2-x1)/2,
						       (double) y2 + 10,
						       -1,
						       GTK_ANCHOR_CENTER,
						       "font", gc_skin_font_board_big,
						       "fill-color", "white",
						       NULL);

}

static void money_display_total(Money_Widget *moneyWidget)
{
  gchar *tmpstr;
  g_return_if_fail (moneyWidget != NULL);

  tmpstr = g_strdup_printf("%.2f €", moneyWidget->priv->total);
  if(moneyWidget->priv->display_total)
    g_object_set (moneyWidget->priv->item_total,
			   "text", tmpstr,
			   NULL);
  g_free(tmpstr);

}

void
money_widget_add (Money_Widget *moneyWidget, MoneyEuroType value)
{
  GooCanvasItem *item   = NULL;
  GdkPixbuf       *pixmap = NULL;
  double	   xratio, yratio, ratio;
  double	   block_width, block_height;
  MoneyItem	  *moneyitem;
  guint		   i, length;

  g_return_if_fail (moneyWidget != NULL);

  /* Search for an hidden item with the same value */
  length = g_list_length(moneyWidget->priv->moneyItemList);
  for(i=0; i<length; i++)
    {
      moneyitem = (MoneyItem *)g_list_nth_data(moneyWidget->priv->moneyItemList, i);

      if(moneyitem && !moneyitem->inPocket && moneyitem->value == value)
	{
	  g_object_set (moneyitem->item, "visibility", GOO_CANVAS_ITEM_VISIBLE, NULL);
	  moneyitem->inPocket = TRUE;
	  moneyWidget->priv->total += euroList[value].value;
	  money_display_total(moneyWidget);
	  return;
	}
    }

  /* There is no already suitable item create, create a new one */

  if(moneyWidget->priv->next_spot > moneyWidget->priv->columns * moneyWidget->priv->lines)
    g_message("More money items requested than the pocket size\n");

  block_width  = (moneyWidget->priv->x2 - moneyWidget->priv->x1) / moneyWidget->priv->columns;
  block_height = (moneyWidget->priv->y2 - moneyWidget->priv->y1) / moneyWidget->priv->lines;

  pixmap = gc_pixmap_load((gchar *)euroList[value].image);

  xratio =  block_width  / (gdk_pixbuf_get_width (pixmap) + BORDER_GAP);
  yratio =  block_height / (gdk_pixbuf_get_height(pixmap) + BORDER_GAP);
  ratio = yratio = MIN(xratio, yratio);

  item =  goo_canvas_image_new ( moneyWidget->priv->rootItem,
				 pixmap,
				 moneyWidget->priv->x1 +
				 (moneyWidget->priv->next_spot % moneyWidget->priv->columns) * block_width
				 +  block_width/2 - (gdk_pixbuf_get_width(pixmap) * ratio)/2,
				 moneyWidget->priv->y1 +
				 (moneyWidget->priv->next_spot / moneyWidget->priv->columns)
				 * block_height
				 + block_height/2 - (gdk_pixbuf_get_height(pixmap) * ratio)/2,
				 "width",  (double) gdk_pixbuf_get_width(pixmap)  * ratio,
				 "height", (double) gdk_pixbuf_get_height(pixmap) * ratio,
				 NULL);

  moneyitem = g_malloc(sizeof(MoneyItem));
  moneyitem->moneyWidget = moneyWidget;
  moneyitem->item   = item;
  moneyitem->value  = value;
  moneyitem->inPocket = TRUE;

  moneyWidget->priv->moneyItemList = g_list_append (moneyWidget->priv->moneyItemList,
						    moneyitem);

  g_signal_connect(GTK_OBJECT(item), "enter_notify_event", (GtkSignalFunc) item_event, moneyitem);

  gdk_pixbuf_unref(pixmap);

  moneyWidget->priv->next_spot++;

  moneyWidget->priv->total += euroList[value].value;

  money_display_total(moneyWidget);
}

void
money_widget_remove(Money_Widget *moneyWidget, MoneyEuroType value)
{
  g_return_if_fail (moneyWidget != NULL);

  moneyWidget->priv->total -= euroList[value].value;

  money_display_total(moneyWidget);
}

double
money_widget_get_total (Money_Widget *moneyWidget)
{
  if(moneyWidget == NULL)
    return 0;

  return moneyWidget->priv->total;
}


static gint
item_event(GooCanvasItem *item, GdkEvent *event, MoneyItem *moneyItem)
{

  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
      switch(event->button.button)
	{
	case 1:
	  g_object_set (item, "visibility", GOO_CANVAS_ITEM_INVISIBLE, NULL);
	  moneyItem->inPocket = FALSE;
	  money_widget_remove(moneyItem->moneyWidget, moneyItem->value);

	  if(moneyItem->moneyWidget->priv->targetWidget != NULL)
	    money_widget_add(moneyItem->moneyWidget->priv->targetWidget,
			     moneyItem->value);

	  break;
	default:
	  break;
	}
    default:
      break;
    }
  return FALSE;
}