// -*- C++ -*- 
#ifndef __fltk_serialization_h__
#define __fltk_serialization_h__
#include <fstream>
#include <iostream>
#include "myboost.h"

#include "message.h"

#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Color_Chooser.H>

// These are so you can override what it means for two menu items to
// be equal when loading choosers..  Default is exact string match.
typedef bool (*chooser_equality_t)(std::string s1, std::string s2); 
void push_chooser_equality(chooser_equality_t n);
void pop_chooser_equality();

bool chooser_equality_exact(std::string s1, std::string s2);
bool chooser_equality_noextension(std::string s1, std::string s2);

extern chooser_equality_t chooser_equality; // Don't manipulate directly please

namespace boost {
namespace serialization {

  //#define FLTK_SERIALIZATION_DEBUG 1

/* All of these serialization functions use a cheap trick to avoid
splitting them into seperate load/save pairs.  First, we read the
current value of the widget into a temporary variable.  Then, we
serialize it; in the case of saving, this saves it to disk.  In the
case of loading, it updates it with the correct value for the future.
Finally, we push it back into the widget.  In the case of saving, this
does nothing; we push the same value back in.  In the case of loading,
this puts the new, correct value in.  I suppose this wastes a few
cycles, but I think it's worth it for clarity. */


template<class Archive> void serialize(Archive & ar, Fl_Input & inp, const unsigned int version)
{
#ifdef FLTK_SERIALIZATION_DEBUG
  info("serializing fl_input with label %s\n", inp.label());
#endif

  std::string tmp = inp.value();
  ar & tmp;
  inp.value(tmp.c_str());
}

template<class Archive> void serialize(Archive & ar, Fl_Button & inp, const unsigned int version)
{
#ifdef FLTK_SERIALIZATION_DEBUG
  info("serializing fl_button with label %s\n", inp.label());
#endif
  int tmp = inp.value();
  ar & tmp;
  inp.value(tmp);
}

template<class Archive> void serialize(Archive & ar, Fl_Valuator & inp, const unsigned int version)
{
#ifdef FLTK_SERIALIZATION_DEBUG
  info("serializing fl_valuator with label %s\n", inp.label());
#endif

  double min,max,value;
  min = inp.minimum();
  max = inp.maximum();
  value = inp.value();
  ar & min;
  ar & max;
  ar & value;
  inp.minimum(min);
  inp.maximum(max);
  inp.value(value);
}

template<class Archive> void serialize(Archive &ar, Fl_Color_Chooser &inp, const unsigned int version)
{
  if(Archive::is_saving::value)
    {
      double r,g,b;
      r = inp.r();
      g = inp.g();
      b = inp.b();      
      ar & r;
      ar & g;
      ar & b;
    }
  else
    {
      double r,g,b;
      ar & r;
      ar & g;
      ar & b;
      inp.rgb(r,g,b);
    }
}

template<class Archive> void serialize(Archive & ar, Fl_Browser & inp, const unsigned int version)
{
#ifdef FLTK_SERIALIZATION_DEBUG
  info("serializing fl_browser with label %s\n", inp.label());
#endif

  for(int i = 1; i < inp.size(); i++)
    {
      void *vtmp;
      std::string tmp;
      tmp = inp.text(i);
      vtmp = inp.data(i);
      ar & tmp;
      ar & vtmp;
      inp.text(i,tmp.c_str());
      inp.data(i,vtmp);
    }
}

/* Note on serializing choices.
   There's some ambiguity of what it means to choose the "same" choice.
   We don't want serialization to break if choices are added, subtracted,
   or rearranged.  So what we do is save the index and label of the choice.
   If the item at the same index has a label match, we'll take that.  Otherwise,
   we search for an item with a label match.  "Label match" is defined with
   a user definable function to allow fuzzy matching (ie. dropping filename
   extensions. */
template<class Archive> void serialize(Archive &ar, Fl_Choice &inp, const unsigned int verison)
{
#ifdef FLTK_SERIALIZATION_DEBUG
  info("serializing fl_choice with label %s\n", inp.label());
#endif

  if(Archive::is_saving::value)
    {
      int index = inp.value();
#ifdef FLTK_SERIALIZATION_DEBUG
      info("serializing fl_choice with label %s, mlabel %s\n", inp.label(),inp.mvalue()->label());
#endif
      std::string name = inp.mvalue()->label();
      ar & index;
      ar & name;
    }
  else
    {
      std::string name;
      int index;
      ar & index;
      ar & name;
#ifdef FLTK_SERIALIZATION_DEBUG
      printf("Looking for %d,%s\n",index,name.c_str());
#endif
      // First try; name and index match.
      if((index < inp.size()) && (index > 0) && inp.text(index) != NULL && chooser_equality(name,inp.text(index)))
	{
	  inp.value(index);
	  return;
	}

      // Second try; name only matches (items were reordered)
      for(int i = 0; i < inp.size(); i++)
	{
	  if(inp.text(i) == NULL)
	    continue;
	  if(chooser_equality(name,inp.text(i)))
	    {
	      inp.value(i);
	      return;
	    }
	}
      // Last resort; hope the index didn't change.
      inp.value(index);
    }
}

/* Note on Fl_Group; I originally thought it would be pretty cool to
make a "group" serializer that saved all the data in the widgets in an
Fl_Group.  On further thought, this would be a very brittle approach;
if the order of the widgets in the group were changed, the new
serialization file would be incompatible with the old.  It would be
very difficult to detect and correct this with versioning.

Alternatively, the widgets could be saved by name (label) instead of
order; however, if anything was relabeled, this would break, and in
fact, if two widgets had the same label, this would break.  Probably
better to hand write the serializer.
*/

} // namespace serialization
} // namespace boost

// When we serialize FLTK controls, we want to automagically cast up
// to Fl_Input, Fl_Button, etc.  since the subclasses don't store any
// extra data.  However, we want to serialize references, not
// pointers, so boost won't try to allocate the widgets.  As such, our
// only alternatives are to a) write a serialization template for
// every stupid FLTK widget, or b) use garbled syntax like ar &
// *(static_cast<Fl_Button *>(ipc->xaxis_label).  Being lazy, I'm
// defining a handy set of functions, flcast(Fl_Widget *) that do the 
// appropriate casts.

#define FLCAST(t) inline t &flcast(t *b) { return *b; };
FLCAST(Fl_Button);
FLCAST(Fl_Input);
FLCAST(Fl_Valuator);
FLCAST(Fl_Choice);
FLCAST(Fl_Color_Chooser);
#undef FLCAST


#endif
