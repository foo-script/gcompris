#  gcompris - adam.py
#
# Copyright (C) 2003, 2008 Bruno Coudoin, Adam 'foo-script' Rakowski
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, see <http://www.gnu.org/licenses/>.
#
# adam activity.
import gtk
import gtk.gdk
import gobject
import gcompris
import gcompris.utils
import gcompris.skin
import gcompris.bonus
import gcompris.sound
import goocanvas
import pango
from random import randint, shuffle, sample

from gcompris import gcompris_gettext as _

#TODO:
#add multilevel feature
#fix sounds, something does not work
#add timer routines. Unlimited time sounds poor

class Gcompris_adam:

  def __init__(self, gcomprisBoard):
    self.gcomprisBoard = gcomprisBoard
    gcomprisBoard.disable_im_context = True

  def draw_ball_at(self, priority, label, at_x, at_y, num_of_balls):
    r, g, b = self.starting_color
    
    goocanvas.Ellipse(
      parent = self.rootitem,
      x = at_x,
      y = at_y,
      radius_x = self.ball_radius,
      radius_y = self.ball_radius,
      fill_color_rgba = (r<<24) | (g<<16) | (b>>8) | ( 255/num_of_balls*priority),
      stroke_color = "blue",
    )
    goocanvas.Text(
      parent = self.rootitem,
      x = at_x,
      y = at_y+self.ball_radius/2,
      width = 2*self.ball_radius,
      text = str(label),
      alignment = pango.ALIGN_CENTER
    )
  
  def draw_all_balls(self, num_of_balls=5):  
    # An order of balls depends on color, not a number in it
    self.labels = range(1, num_of_balls+1)
    self.priorities = range(1, num_of_balls+1)
    shuffle(self.labels)
    #a references to all the ball-objects will be stored here
    
    #We don't want overlapping balls, so we will put each ball on something like net
    possible_x = range(self.ball_radius+5, gcompris.BOARD_WIDTH-self.ball_radius-5, self.ball_radius+10)
    possible_y = range(10, gcompris.BOARD_HEIGHT-100, self.ball_radius+10)

    #From all possibilites we choose only some balls, num_of_balls 
    self.positions_x = sample(possible_x, num_of_balls)
    self.positions_y = sample(possible_y, num_of_balls)
  
    for ball in range(num_of_balls):
      self.draw_ball_at(self.priorities[ball], self.labels[ball], self.positions_x[ball], self.positions_y[ball], num_of_balls)
    

  def start(self):
    gcompris.bar_set(gcompris.BAR_ABOUT)	#temporary, until it does not support levels
    gcompris.set_default_background(self.gcomprisBoard.canvas.get_root_item())
    self.rootitem = goocanvas.Group(parent = self.gcomprisBoard.canvas.get_root_item())		
    
    self.ball_radius=20
    #if number of tries is exceeded, then a game goes over - temporary unused
    self.tries=4
    #Select a random color. The chosen one is the darkest shade.
    #Depending on its priority, the ball will be lighter or equal shade as choosen one
    self.starting_color = [ randint(60, 255) for i in range(3) ]
    
    #a message below destroys aesthetics of activity
    '''self.instruction = goocanvas.Text(
      parent = self.rootitem,
      x = 600.0,
      y = 300.0,
      width = 250,
      text = _("Type the numbers of balls,"
               " begin from the lightest."),
      fill_color = "black",
      anchor = gtk.ANCHOR_CENTER,
      alignment = pango.ALIGN_CENTER
      )
    '''
    self.draw_all_balls()

  def end(self):
    self.rootitem.remove()

  def ok(self):
    pass

  def repeat(self):
    pass

  def config(self):
    pass

  def repeat(self):
    pass

  #mandatory but unused yet
  def config_stop(self):
    pass

  # Configuration function.
  def config_start(self, profile):
    pass

  def key_press(self, keyval, commit_str, preedit_str):
    if keyval >= 48 and keyval <= 57:
      user_choosed = keyval-48
      
      if len(self.labels) > 0:	#there's still something to do
	if self.labels[0] == user_choosed:
	  goocanvas.Image(
	    parent = self.rootitem,
	    pixbuf = gcompris.utils.load_pixmap("adam/smile.svg"),
	    x = self.positions_x[0],
	    y = self.positions_y[0]
	  )
	  self.priorities=self.priorities[1:]
	  self.labels=self.labels[1:]
	  self.positions_x=self.positions_x[1:]
	  self.positions_y=self.positions_y[1:]
	else:
	  self.tries=self.tries-1;
	  if self.tries<0:
	    #failed
	    self.tries=4
	  gcompris.sound.play_ogg("sounds/youcannot.wav")
      else:	#won!
	gcompris.bonus.display(gcompris.bonus.WIN, gcompris.bonus.TUX)
	gcompris.sound.play_ogg("sounds/tuxok.wav")

  def pause(self, pause):
    pass

  def set_level(self, level):
    pass
