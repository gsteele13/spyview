#include <stdio.h>
#include <list>
#include <boost/regex.hpp>
#include <algorithm>
#include <FL/Fl_Float_Input.H>
#include "ImageWindow.H"
#include "ImageWindow_Fitting.H"
#include "ImageWindow_Fitting_Ui.h"
using namespace std;

// ============== FITTING EXTENSION; allow fitting of individual lines in an image ==================== 
FitControls *Fitting::fc = NULL;
Fitting *Fitter = NULL;
Fitting::Fitting(ImageWindow *iwp) : ImageWindow_Module(iwp)
{
  fitFuncs = NULL;
  curOrdinate = NAN;
  Fitter = this;
  Init();
}

static void fit_hide_callback(Fl_Widget *w, void *o)
{
  assert(Fitter);
  Fitting::fc->win->hide();
  Fitter->iw->line_cut_limit = 0xff;
}
void Fitting::Init()
{
  fc = new FitControls;
  fc->win->callback(fit_hide_callback,fc->win->user_data());

  // Populate a few test fit functions
  Fitter->FitFunctions.push_back(FitFunction("Constant","Contstant offset","","b#","b#=ybar"));
  Fitter->FitFunctions.push_back(FitFunction("Linear","Linear fit","","m#*x+b#","m#=cov/(sixg*sigx); b#=ybar-xbar*m#"));
  Fitter->FitFunctions.push_back(FitFunction("Quadratic","Quadratic fit","","a#+b#*x+c#*x*x"));
  Fitter->FitFunctions.push_back(FitFunction("Cubic","Cubic fit","","a#+b#*x+c#*x*x+d#*(x**3)"));
  Fitter->FitFunctions.push_back(FitFunction("Lorentzian","Lorentzian peak","","a#/((x-x0#)**2/w#**2+1)","a#=sigy + ybar; x0#=xbar; w#=sigx;","x0","w"));
  Fitter->FitFunctions.push_back(FitFunction("Gaussian","Gaussian peak","","a#*exp(-((x-x0#)**2)/(2.0*sigma#))","a#=sigy + ybar; x0#=xbar; sigma#=sigx;","x0","sigma"));
  Fitter->FitFunctions.push_back(FitFunction("Soft Sign","A function that ramps smoothly from zero to its amplitude a.  Also, result of convolving a Lorentzian with a step function.",
					     "","a#*(atan((x-x0#)/w#)/pi+0.5)","a#=sigy; x0#=xbar; w#=cov>0?sigx:-sigx;","x0","w"));
  Fitter->UpdateFitFunctions();
  Fitter->fetchFitLimit();
  //  fc->funcs->value(1);
  //  fc->funcs->do_callback();
}

std::string Fitting::DefaultName(std::string extn)
{
  return string(iw->output_basename) + extn;
}
void Fitting::SaveFit(std::string name)
{
  if(name.empty())
    name = DefaultName();

  const char *fname = name.c_str();
  std::ofstream ofs(fname);
  try
    {
      if(!ofs.good())
 	throw(1);
    }
  catch (...)
    {
      warn("Unable to create or overwrite fit file \"%s\"\n",fname);
    }
 
   try
     {
       boost::archive::text_oarchive oa(ofs);
       oa & (*this);
     }
   catch (boost::archive::archive_exception e)
     {
       error("Fitting serialization error: %s",e.what());
     }
   catch (...)
     {
       error("Unknown serialization error\n");
     }

}

void Fitting::LoadFit(std::string name)
{
  if(name.empty())
    name = DefaultName();
  
  const char *fname = name.c_str();
  try 
    {
      std::ifstream ifs(fname);
      if(!ifs.good())
 	throw(1);
      boost::archive::text_iarchive ia(ifs);
      ia & (*this);
    }
  catch (boost::archive::archive_exception e)
    {
      warn("Unable to load fitting file \"%s\": %s\n",fname,strerror(errno));
      error("Serialization error: %s",e.what());
    }
  catch (...)
    {
      warn("Unable to load fitting file \"%s\": %s\n",fname,strerror(errno));
    }
  // FIXME; we should do something to make sure these fits are actually remotely reasonable for our axis.
  fc->current->clear();
  fc->funcs->clear();
  UpdateFitFunctions();
  curOrdinate = NAN;
  moveLineCut(0,false); // This calls update for us.
}

/* Update the fit controls and line cut if the fit changes. */
void Fitting::update()
{ 
  iw->plotLineCut(true); // Make sure we don't throttle the update. 
  SelectFitInstance(); // Get the fit variables updated, as well.
  iw->redraw_overlay();
};

/* Change the allowed direction for line cuts */
void Fitting::updateDirection()
{
  const Fl_Menu_Item *fmi = fc->lctype->mvalue();
  assert(fmi);
  iw->line_cut_limit = reinterpret_cast<int>(fmi->user_data());
  if(!(iw->line_cut_type & iw->line_cut_limit))
    {
      iw->line_cut_type = iw->line_cut_limit;
      iw->line_cut_xp = iw->id.width/2;
      iw->line_cut_yp = iw->id.height/2;
    }
  iw->plotLineCut();
  iw->gplinecut.prepmouse();
  iw->redraw_overlay();
}

/* this function forces the image window to move it's linecut by "lines" lines.
   Calling it with lines=zero updates the current ordinate.
   If copy is true, copy the fit from the current location to the new location */
void Fitting::moveLineCut(int lines, bool copy)
{
  FitFunctionInstances_t *src = NULL;
  if(copy && fitFuncs)
    {
      assert(fitFuncs);
      src = new FitFunctionInstances_t(*fitFuncs);
    }
  switch(iw->line_cut_type)
    {
    case HORZLINE:
      iw->line_cut_yp += lines;
      if(iw->line_cut_yp >= iw->h)
	iw->line_cut_yp = iw->h-1;
      if(iw->line_cut_yp < 0)
	iw->line_cut_yp = 0;
      break;
    case VERTLINE:
      iw->line_cut_xp += lines;
      if(iw->line_cut_xp >= iw->w)
	iw->line_cut_xp = iw->w-1;
      if(iw->line_cut_xp < 0)
	iw->line_cut_xp = 0;
      break;
    default:
      assert(0);
    }
  if(src)
    {
      double x = ordinate();
      PushUndo();
      fitGrid[x] = *src;
      delete src;
    }
  update();
}

/* event hook -- the only things we capture are left, right, and f.
   left and right have a new meaning if the fit window is open. */
int Fitting::event_callback(int event)
{
  switch(event)
    {
    case FL_SHORTCUT:
      switch(Fl::event_key())
	{
	case FL_Left:
	  if(!fc->win->visible())
	    return 0;
	  moveLineCut(-1,Fl::event_shift());
	  return 1;
	case FL_Right:
	  if(!fc->win->visible())
	    return 0;
	  moveLineCut(+1,Fl::event_shift());
	  return 1;
	case 'f':
	  if(Fl::event_shift())
	    {	
	      if(fc->win->visible())
		{
		  fc->win->hide();
		  iw->line_cut_limit = 0xff;
		}
	      else
		{
		  fc->win->show();
		  updateDirection();
		}
	      return 1;
	    }
	  break;
	}
      break;
    }
  return 0;
};

void Fitting::overlay_callback()
{
  if(!fc->win->visible())
    return;
  int dx = min(iw->xzoom, iw->yzoom);
  if (dx<=0)
    dx = 1;
  dx *= 2;
  int dy =dx;
  if(!(fc->overlayPeaks->value() || fc->overlayWidths->value()))
     return;
  FitFunctionInstance *ffi = CurrentFitInstance();

  for(fitGrid_t::iterator i = fitGrid.begin(); i != fitGrid.end(); i++)
  {
    int x,y;
    switch(iw->line_cut_type)
      {
      case HORZLINE:
	y = iw->id.getY_inv(i->first, iw->yzoom);
	break;
      case VERTLINE:
	x  = iw->id.getX_inv(i->first, iw->xzoom);
	break;
      default:
	assert(0);
      }

    if(fitFuncs == &(i->second))
      fl_color(FL_GREEN);
    else
      fl_color(FL_RED);
    for(FitFunctionInstances_t::iterator j = i->second.begin(); j != i->second.end(); j++)
      {	

	if(j->func->center.empty())
	  continue;
	double c=j->values[j->func->center].val;
	switch(iw->line_cut_type)
	  {
	  case HORZLINE:
	    x = iw->id.getX_inv(c, iw->xzoom);
	    break;
	  case VERTLINE:
	    y = iw->id.getY_inv(c, iw->yzoom);
	  }
	if(fc->overlayPeaks->value())
	  {
	    if(ffi == &(*j))
	      fl_color(FL_YELLOW);
	    fl_line(x-dx,y-dy,x+dx,y+dy);
	    fl_line(x+dx,y-dy,x-dx,y+dy);
	  }
	  
	if((!fc->overlayWidths->value()) || j->func->width.empty())
	  continue;	
	double w =j->values[j->func->width].val;

	switch(iw->line_cut_type)
	  {
	  case HORZLINE:
	    {
	      int x1 = iw->id.getX_inv(c+w,iw->xzoom);
	      int x2 = iw->id.getX_inv(c-w,iw->xzoom);
	      fl_line(x1,y,x2,y);
	      fl_line(x1,y-dy,x1,y+dy);
	      fl_line(x2,y-dy,x2,y+dy);
	    }
	    break;
	  case VERTLINE:
	    {
	      int y1 = iw->id.getY_inv(c+w,iw->yzoom);
	      int y2 = iw->id.getY_inv(c-w,iw->yzoom);
	      fl_line(x,y1,x,y2);
	      fl_line(x-dx,y1,x+dx,y1);
	      fl_line(x-dx,y2,x+dx,y2);
	    }
	    break;
	  }

      }
  }
};

/* Send the appropriate gnuplot commands to set up the fitting function and load all the variables */
void Fitting::setupFitFunc(Gnuplot_Interface &gp, FitFunctionInstance *limit)
{
  gp.cmd("set fit errorvariables\n");
  assert(fitFuncs);
  bool first=true;
  for(FitFunctionInstances_t::iterator i = fitFuncs->begin(); i != fitFuncs->end(); i++)
    {
      i->loadVariables(gp);
      if(!i->func->initialization.empty())
	gp.cmd("%s\n",i->func->initialization.c_str());
    }
  gp.cmd("fitfunc(x)=");
  if(limit)
    {
      limit->plot(gp,false);
    }
  else
    for(FitFunctionInstances_t::iterator i = fitFuncs->begin(); i != fitFuncs->end(); i++)
      {
	if(!first)
	  gp.cmd("+ ");
	first = false;
	i->plot(gp,false);	  
      }
  gp.cmd("\n");
}

void Fitting::FitInSequence()  // Fit each peak sequentially.  This tends to stabilize peak centers.
{
  assert(fitFuncs);
  PushUndo();
  
  double oco;
  do
    {
      for(FitFunctionInstances_t::iterator i = fitFuncs->begin(); i != fitFuncs->end(); i++)
	{
	  if(!i->locked)
	    fit(&(*i),false);
	}
      if(!fc->autoright->value())
	break;
      oco = ordinate();
      moveLineCut(1,false);
      linecut_callback(false);
    }
  while(oco != ordinate());
}

/* Actually perform a fit, and fetch back the variables **/
void Fitting::fit(FitFunctionInstance *limit, bool undoable) 
{
  assert(fitFuncs);
  if(undoable)
    PushUndo();
  Gnuplot_Interface &gp = iw->gplinecut;
  for(FitFunctionInstances_t::iterator i = fitFuncs->begin(); i != fitFuncs->end(); i++)
    {
      i->scaleVariables();
      i->loadVariables(gp);
    }
  if(limit && !fc->superpose->value())
    setupFitFunc(gp,limit);
  else
    setupFitFunc(gp);

  // If peakrange is set, limit the fit to points within +/- peakrange of centers of peaks.
  // Most useful for widely spaced peaks, possibly in conjuction with turning on/off superpose.
  if(atof(fc->peakrange->value()) > 0)
    {
      double s= atof(fc->peakrange->value());
      gp.cmd("rf(x,x0,x1)=((x > x0) && (x < x1)) ? 0 : 1\n"); // rf returns 0 if x is between x0 and x1
      gp.cmd("fit fitfunc(x) '%s' u (1",iw->xsection_fn);
      if(limit && !limit->func->center.empty())
	{
	  double c = limit->values[limit->func->center].val;
	  gp.cmd("*rf($1,%g,%g)",c-s,c+s);
	}
      else
	{
	  for(FitFunctionInstances_t::iterator i = fitFuncs->begin(); i != fitFuncs->end(); i++)
	    {
	      if(!i->func->center.empty())
		{
		  double c = i->values[i->func->center].val;
		  gp.cmd("*rf($1,%g,%g)",c-s,c+s);
		}
	    }
	}
      gp.cmd("> 0.5 ? 1/0 : $1):2 via ");
    }
  else
    gp.cmd("fit fitfunc(x) '%s' u 1:2 via ", iw->xsection_fn);
  if(limit)
    limit->listFitVars(gp);
  else
    {
      bool first=true;
      for(FitFunctionInstances_t::iterator i = fitFuncs->begin(); i != fitFuncs->end(); i++)
	{
	  if(i->locked)
	    continue;
	  if(!first)
	    gp.cmd(",");
	  first=false;
	  i->listFitVars(gp);
	}
    }
  gp.cmd("\n");
  gp.eat();
  if(limit)
    {
      limit->fetchVariables(gp);
      limit->unscaleVariables();
    }
  else    
    for(FitFunctionInstances_t::iterator i = fitFuncs->begin(); i != fitFuncs->end(); i++)
      {
	i->fetchVariables(gp);
	i->unscaleVariables();
      }
  update();
}

void Fitting::bump(double p)
{
  double range;
  switch(iw->line_cut_type)
    {
    case HORZLINE:
      range = fabs(iw->id.xmax-iw->id.xmin);
      break;
    default:
      range = fabs(iw->id.ymax-iw->id.ymin);
    }	
  for(FitFunctionInstances_t::iterator i = fitFuncs->begin(); i != fitFuncs->end(); i++)
    {
      if(!i->func->center.empty())
	i->values[i->func->center].val += range*p;
    }
  update();
}

double Fitting::ordinate()
{
  double ord;
  if(iw->line_cut_type == HORZLINE)
    {
      ord = iw->line_cut_yp;
      ord = iw->id.getY(ord+0.5);
    }
  else
    {
      ord = iw->line_cut_xp;
      ord = iw->id.getX(ord+0.5);
    }  
  return ord;
}

void Fitting::ResetFitInstanceBrowser()
{
  int ov = fc->current->value();
  assert(fitFuncs);
  fc->current->clear();
  for(FitFunctionInstances_t::iterator i = fitFuncs->begin(); i != fitFuncs->end(); i++)
    {
      AddFitInstanceToBrowser(*i);
    }
  if(fc->current->size() >= 1)
    {
      if(fc->current->size() > ov)
	fc->current->value(ov);
      else
	fc->current->value(1);
    }
  SelectFitInstance();
}

/* Plot the extra overlays on the linecut for gnuplot */
void Fitting::linecut_callback(bool init)
{  
  Gnuplot_Interface &gp = iw->gplinecut; // Handy, but not important.
  if(!fc->win->visible())
    return;
  /* First, figure out if we still have the right ordinate */
  double x = ordinate();
  fitFuncs = &(fitGrid[x]);

  if(x != curOrdinate)
    {
      char buf[32];
      snprintf(buf, sizeof(buf),"%g",x);
      fc->line->value(buf);
      curOrdinate = x;
      ResetFitInstanceBrowser();
    }

  FitFunctionInstance *ffi = CurrentFitInstance();
  if(!fc->win->visible())
    return;
  if(init)
    {
      iw->gplinecut.cmd("set samples 1000\n");
      setupFitFunc(gp);
    }
  else
    {
      // Plot these two first so their color's don't change as we add fits.
      if(fc->plot_fit->value() && (fitFuncs->size() > 1))
	gp.cmd(", fitfunc(x) ti \"Fit\"");
      if(fc->plot_residual->value() && (fitFuncs->size() > 1))
	{
	  gp.cmd(", '%s' u 1:($2-fitfunc($1)) ps 0.5 ti \"Residuals\"", iw->xsection_fn);
	}
      if(fc->plot_individual->value())
	{
	  for(FitFunctionInstances_t::iterator i = fitFuncs->begin(); i != fitFuncs->end(); i++)
	    {
	      gp.cmd(", ");
	      i->plot(gp,true);
	      if(&(*i) == ffi)
		gp.cmd("lw 2");  // Make the currently selected fit fat.
	    }
	}
    }
}

/*******************************/
/* FitFunction         related */
/*******************************/
void Fitting::FitFunction::identifyVariables()
{
  static regex variable("([a-zA-Z][a-zA-Z0-9]*)#");
  smatch what;
  vars.clear();
  std::string::const_iterator start = definition.begin();
  std::string::const_iterator stop = definition.end();
  while(regex_search(start,stop,what,variable))
    {
      vars.insert(what[1].str());
      start=what[0].second;      
    }
}


void Fitting::UpdateFitFunctions()
{ 
  fc->funcs->clear();
  for(FitFunctions_t::iterator i = FitFunctions.begin();  i != FitFunctions.end(); i++)
    {
      fc->funcs->add(i->name.c_str(), &(*i));
    }
}

void Fitting::HandleEditDialog(NewFitFuncWindow *nff)
{
  FitFunction *ff;
  FitFunction f;
  
  if(nff->editing == NULL)
    ff = &f;
  else
    ff = nff->editing;
  ff->name = nff->name->value();
  ff->description = nff->description->value();
  ff->initialization = nff->initialization->value();
  ff->definition = nff->definition->value();
  ff->guess = nff->guess->value();
  ff->center = nff->center->value();
  ff->width = nff->width->value();
  ff->identifyVariables();
  if(nff->editing == NULL)
    FitFunctions.push_back(f);
  UpdateFitFunctions();
}

Fitting::FitFunction *Fitting::CurrentFitFunction()
{
  if(fc->funcs->value() == 0)
    return NULL; 
  FitFunction *func = (FitFunction *)fc->funcs->data(fc->funcs->value());
  return func;
}

void Fitting::SelectFitFunction()
{
  FitFunction *ff = CurrentFitFunction();
  if(ff == NULL)
    return; // Fixme; should deselect all
  fc->description->buffer()->text(ff->description.c_str());
  fc->definition->buffer()->text(ff->definition.c_str());  
}

/*******************************/
/* FitFunctionInstance related */
/*******************************/
void Fitting::FitFunctionInstance::loadVariables(Gnuplot_Interface &gp)
{
  for(values_t::iterator i = values.begin(); i != values.end(); i++)
    gp.cmd("%s_%s=%.10g\n", i->first.c_str(), id.c_str(), i->second.val/i->second.scale);
}

/* The gnuplot fitter does badly with variables that are very different absolute magnitudes.
   Rescale them all to be roughly 1 for the purpsose of fitting by calling scaleVariables,
   then unscale them by calling unscaleVariables */
void Fitting::FitFunctionInstance::scaleVariables()
{
  for(values_t::iterator i = values.begin(); i != values.end(); i++)
    i->second.scale = i->second.val;
}

void Fitting::FitFunctionInstance::unscaleVariables()
{
  for(values_t::iterator i = values.begin(); i != values.end(); i++)
    i->second.scale = 1.0;
}

void Fitting::FitFunctionInstance::fetchVariables(Gnuplot_Interface &gp)
{
  for(values_t::iterator i = values.begin(); i != values.end(); i++)
    {
      i->second.val = gp.getvariable(i->second.name.c_str()) * i->second.scale;
      i->second.dev = gp.getvariable((i->second.name + "_err").c_str()) * i->second.scale;
      gp.eat();
    }
}


std::string Fitting::FitFunctionInstance::specializeExpression(std::string pltcmd)
{
// This is quadratic in the number of variables, but ... who cares?
// I suppose if it became an issue, we could search through for #'s, and then read backward
// to find the variable name, but it would be much more complex..
  for(values_t::iterator i = values.begin(); i != values.end(); i++)
    {
      std::string vtmp = string("\\b")+ i->first; // Make sure the variable name is the start of a word.
      vtmp += "#";     
      if(i->second.scale == 1.0)
	pltcmd=regex_replace(pltcmd, regex(vtmp), i->second.name);
      else
	{
	  char buf[64];
	  snprintf(buf,sizeof(buf),"%g",i->second.scale);
	  i->second.scale=atof(buf);
	  char buf2[128];
	  snprintf(buf2,sizeof(buf2),"(%s*%s)",buf,i->second.name.c_str());
	  pltcmd=regex_replace(pltcmd, regex(vtmp), buf2);
	}
    }
  return pltcmd;  
}

void Fitting::FitFunctionInstance::plot(Gnuplot_Interface &gp,bool title)
{
  std::string pltcmd = specializeExpression(func->definition);

  if(title)
    gp.cmd("%s ti \"%s\"", pltcmd.c_str(),(func->name + "_" + id).c_str());
  else
    gp.cmd("%s", pltcmd.c_str());
}

void Fitting::FitFunctionInstance::listFitVars(Gnuplot_Interface &gp)
{
  bool first=true;
  for(values_t::iterator i = values.begin(); i != values.end(); i++)
    {
      if(!first)
	gp.cmd(",");
      else
	first=false;      
      gp.cmd("%s",i->second.name.c_str());
    }
}

void Fitting::AddFitInstanceToBrowser(FitFunctionInstance &f)
{
  std::string name = f.func->name + "_" + f.id;
  if(f.locked)
    name = string("@-")+name;
  fc->current->add(name.c_str(),&f);
  fc->current->value(fc->current->size());
  fc->current->do_callback();
}

void Fitting::DelFitInstance()
{
  assert(fitFuncs);
  PushUndo();
  if(fc->current->value() == 0)
    return;
  int index = fc->current->value();
  FitFunctionInstance *ffi = (FitFunctionInstance *)fc->current->data(index);
  fc->current->remove(index);
  FitFunctionInstances_t::iterator i;
  for(i = fitFuncs->begin(); i != fitFuncs->end(); i++)
    {
      if(&(*i) == ffi)
	break;
    }
  assert(i != fitFuncs->end());
  fitFuncs->erase(i);
  if(fc->current->size() > index)
    fc->current->value(index);
  else 
    fc->current->value(index-1); // This is zero if this was the last instance, which is perfect.
  update();
}

void Fitting::LockFitInstance()
{
  assert(fitFuncs);
  FitFunctionInstance *f = CurrentFitInstance();
  if(!f)
    return;
  f->locked = !f->locked;
  ResetFitInstanceBrowser();
  std::string name = f->func->name + "_" + f->id;
  if(f->locked)
    {
      name = string("@-")+name;
      fc->unlock->show();
      fc->lock->hide();
    }
  else
    {
      fc->unlock->hide();
      fc->lock->show();
    }
  fc->current->text(fc->current->value(),name.c_str());
  
}

void Fitting::AddFitInstance()
{
  assert(fitFuncs);
  PushUndo();
  FitFunction *func = CurrentFitFunction();
  if(func == NULL)
    return; // Fixme; should deselect all
  FitFunctionInstance f;
  f.func = func;

  // Find a unique ID for this.
  char buf[32];
  for(unsigned i = 0; true; i++)
    {
      bool good=true;
      snprintf(buf,sizeof(buf),"%u",i);
      for(FitFunctionInstances_t::iterator i = fitFuncs->begin(); i != fitFuncs->end(); i++)
	if(i->id == buf)
	  {
	    good = false;
	    break;
	  }
      if(good)
	break;
    }
  f.id = buf;

  f.locked = false;
  for(FitFunction::vars_t::iterator i = func->vars.begin(); i != func->vars.end(); i++)
      f.values[*i]; // This actually stuffs it with a default variable
  f.setupVariableNames(); 
  fitFuncs->push_back(f);
  AddFitInstanceToBrowser(fitFuncs->back());

  std::vector<Gnuplot_Interface::point> pts;
  iw->gplinecut.getmouse(pts);
  if(f.values.size() < pts.size()) // Assume if the user clicked on a bunch of things, they want to use that as an initial guess.
    UseInitialGuess();
  iw->gplinecut.prepmouse();
}

void Fitting::FitFunctionInstance::setupVariableNames()
{
  for(values_t::iterator i = values.begin(); i != values.end(); i++)
    {
      i->second.name = i->first;
      i->second.name += "_";
      i->second.name += id;
    }
}

Fitting::FitFunctionInstance *Fitting::CurrentFitInstance()
{
  if(fc->current->value() == 0)
    return NULL; 
  FitFunctionInstance *func = (FitFunctionInstance *)fc->current->data(fc->current->value());
  return func;
}

// Callback for when a user changes one of the variables.
static void varchange_callback(Fl_Float_Input *input, Fitting::FitFunctionInstance::variable *var)
{
  Fitter->PushUndo();
  assert(var);
  assert(input);
  var->val = (atof(input->value()));
  //  Fitter->update();
}

void Fitting::SelectFitInstance()
{
  FitFunctionInstance *fi = CurrentFitInstance();
  if(fi == NULL)
    {
      fc->variables->clear();
      return;
    }

  fc->variables->clear();
  fc->variables->begin();
  int y = fc->variables->y();
  int w = static_cast<int>(fc->variables->w()*.6);
  int x = fc->variables->x()+fc->variables->w()-w-20; // Hack -- how do you find out the width of the scroll bar?
  //  y=0;
  //  x = fc->variables->w()-w;
  for(FitFunctionInstance::values_t::iterator i = fi->values.begin(); i != fi->values.end(); i++)
    {
      Fl_Float_Input *f = new Fl_Float_Input(x,y,w,25,i->first.c_str());
      char buf[64];
      snprintf(buf,sizeof(buf),"%.8g",i->second.val);
      f->value(buf);
      f->box(FL_DOWN_BOX);
      f->align(FL_ALIGN_LEFT);
      f->show();
      f->user_data(&(i->second));
      f->callback((Fl_Callback *) varchange_callback);
      y += f->h()+5;
    }
  fc->variables->end();
  fc->variables->redraw();
  if(fi->locked)
    {
      fc->unlock->show();
      fc->lock->hide();
    }
  else
    {
      fc->lock->show();
      fc->unlock->hide();
    }
}

void Fitting::ClearInitialGuess()
{
  iw->gplinecut.prepmouse();
  update();
}

void Fitting::UseInitialGuess()
{
  std::vector<Gnuplot_Interface::point> points;
  iw->gplinecut.getmouse(points);
  iw->gplinecut.prepmouse();

  double xxbar, yybar, xbar, ybar, xybar;
  xxbar=0.0;
  yybar=0.0;
  xbar=0.0;
  ybar=0.0;
  xybar=0.0;
  for(size_t i = 0; i < points.size(); i++)
    {
      xbar += points[i].x;
      xxbar += points[i].x*points[i].x;
      ybar += points[i].y;
      yybar += points[i].y*points[i].y;
      xybar += points[i].y*points[i].x;
    }
  xbar /= points.size();
  xxbar /= points.size();
  ybar /= points.size();
  yybar /= points.size();
  xybar /= points.size();

  double sigx = sqrt(xxbar-xbar*xbar);
  double sigy = sqrt(yybar-ybar*ybar);
  double cov = xybar-xbar*ybar;
  assert(!isnan(sigx));
  assert(!isnan(sigy));

  FitFunctionInstance *ffi = CurrentFitInstance();
  if(ffi == NULL)
    {
      fprintf(stderr,"How did you do this?\n");
      return;
    }
  if(points.size() < ffi->values.size())
    {
      fprintf(stderr,"Too few points for initial guess\n");
      return;
    }
  if(ffi == NULL)
    return;
  Gnuplot_Interface &gp = iw->gplinecut;

  if(!ffi->func->guess.empty())
    {
      gp.cmd("xbar=%g; ybar=%g; sigx=%g; sigy=%g; cov=%g\n",xbar,ybar,sigx,sigy,cov); // Give these to the initial guess code.
      gp.cmd("%s\n",ffi->specializeExpression(ffi->func->guess).c_str());
      gp.eat();
      ffi->fetchVariables(gp); // Fetch in case the initial expression modified the variables (it probably did...)
    }
#if 1
  ffi->scaleVariables();
  setupFitFunc(gp);
  gp.cmd("fit fitfunc(x) '-' u 1:2 via ");
  ffi->listFitVars(gp);
  gp.cmd("\n");
  for(size_t i = 0; i < points.size(); i++)
    {
      gp.cmd("%g %g\n",points[i].x, points[i].y);
    }
  gp.cmd("e\n");
  gp.eat();
  ffi->fetchVariables(gp);
  ffi->unscaleVariables();
#endif
  update();
}

// Stack manipulation
void Fitting::PushStack()
{
  assert(fitFuncs);
  fitStack.push_back(FitFunctionInstances_t(*fitFuncs));
  UpdateStackCount();
}

void Fitting::PopStack()
{
  PushUndo();
  fitGrid[ordinate()] = fitStack.back();
  fitStack.pop_back();
  ResetFitInstanceBrowser();
  update();

  UpdateStackCount();
}

void Fitting::ClearStack()
{
  fitStack.clear();
  UpdateStackCount();
}

void Fitting::UpdateStackCount()
{
  char buf[32];
  snprintf(buf,sizeof(buf),_STF,fitStack.size());
  fc->stackdepth->value(buf);
}

// Undo stack
static const size_t undoStackDepth=8;
void Fitting::PushUndo()
{
  assert(fitFuncs);
  undoStack.push_back(undoElement(ordinate(), *fitFuncs));  
  redoStack.clear();
  if(undoStack.size() > undoStackDepth)
    undoStack.pop_front();
  UpdateUndoCount();
}


void Fitting::PopUndo()
{  
  double x  = undoStack.back().first;
  redoStack.push_back(undoElement(x, fitGrid[x]));
  if(redoStack.size() > undoStackDepth)
    redoStack.pop_front();

  fitGrid[x] = undoStack.back().second;
  undoStack.pop_back();
  UpdateUndoCount();
  switch(iw->line_cut_type)
    {
    case HORZLINE:
      iw->line_cut_yp = iw->id.getY_inv(x);
      break;
    case VERTLINE:
      iw->line_cut_xp = iw->id.getX_inv(x);
      break;
    default:
      assert(0);
    }
  fitFuncs = &(fitGrid[x]);
  ResetFitInstanceBrowser();
  update();
}


void Fitting::PopRedo()
{  
  double x = redoStack.back().first;
  undoStack.push_back(undoElement(x, fitGrid[x]));
  fitGrid[x] = redoStack.back().second;
  redoStack.pop_back();
  UpdateUndoCount();
  switch(iw->line_cut_type)
    {
    case HORZLINE:
      iw->line_cut_yp = iw->id.getY_inv(x);
      break;
    case VERTLINE:
      iw->line_cut_xp = iw->id.getX_inv(x);
      break;
    default:
      assert(0);
    }
  fitFuncs = &(fitGrid[x]);
  ResetFitInstanceBrowser();
  update();
}

void Fitting::UpdateUndoCount()
{
  static char ubuf[64]; // Ask me why fl_widget label's aren't copied to a private buffer. :(
  if(undoStack.size() > 0)
    {
      fc->undo->activate();
      snprintf(ubuf,sizeof(ubuf),"Undo ("_STF")",undoStack.size());
      fc->undo->label(ubuf);
    }
  else
    {
      fc->undo->deactivate();
      fc->undo->label("Undo");
    }
  static char rbuf[64];
  if(redoStack.size() > 0)
    {
      fc->redo->activate();
      snprintf(rbuf,sizeof(rbuf),"Redo ("_STF")",redoStack.size());
      fc->redo->label(rbuf);
    }
  else
    {
      fc->redo->deactivate();
      fc->redo->label("Redo");
    }

}

void Fitting::updateFitLimit()
{
  Gnuplot_Interface &gp = iw->gplinecut;
  gp.cmd("FIT_LIMIT=%g\n",atof(fc->fitlimit->value()));
  fetchFitLimit();
}

void Fitting::fetchFitLimit()
{
  Gnuplot_Interface &gp = iw->gplinecut;
  gp.eat();

  gp.cmd("print FIT_LIMIT\n");  
  char buf[1024];
  gp.resp(buf,sizeof(buf));
  double l = atof(buf);
  if(isnan(l) || l == 0.0)
    l=1e-5; // Default for gnuplot.
  snprintf(buf,sizeof(buf),"%g",l);
  fc->fitlimit->value(buf);
}

void Fitting::SaveText(std::string fname)
{
  FILE *out = fopen(fname.c_str(),"w+");
  if(out == NULL)
    {
      warn("Unable to open file \"%s\": %s\n", fname.c_str(), strerror(errno));
      return;
    }
  fprintf(out,"# Fit results generated by spyview for %s\n",iw->filename.c_str());
  typedef std::set<std::string> used_t;
  used_t used;

  // Print out helpful comments
  for(fitGrid_t::iterator i = fitGrid.begin(); i != fitGrid.end(); i++)
    {
      for(FitFunctionInstances_t::iterator j = i->second.begin(); j != i->second.end(); j++)
	{
	  FitFunction *func = j->func;
	  used_t::iterator tmp = used.find(func->name);
	  if(tmp == used.end())
	    {
	      used.insert(func->name);
	      int count = 4;
	      fprintf(out,"# [1]%s [2]id [3]ordinate", func->name.c_str());
	      for(FitFunction::vars_t::iterator k = func->vars.begin(); k != func->vars.end(); k++)
		fprintf(out," [%d]%s",count++,k->c_str());
	      for(FitFunction::vars_t::iterator k = func->vars.begin(); k != func->vars.end(); k++)
		fprintf(out," [%d]%s",count++,(*k+"_err").c_str());
	      fprintf(out,"\n");
	    }	  
	}
    }
  for(fitGrid_t::iterator i = fitGrid.begin(); i != fitGrid.end(); i++)
    {
      if(i->second.size() == 0)
	continue;
      fprintf(out,"# position %g\n", i->first);
      for(FitFunctionInstances_t::iterator j = i->second.begin(); j != i->second.end(); j++)
	{
	  FitFunction *func = j->func;
	  fprintf(out,"%s %s %g", func->name.c_str(),j->id.c_str(),i->first);
	  for(FitFunctionInstance::values_t::iterator k= j->values.begin(); k != j->values.end(); k++)
	    fprintf(out," %20.18g", k->second.val);
	  for(FitFunctionInstance::values_t::iterator k= j->values.begin(); k != j->values.end(); k++)
	    fprintf(out," %20.18g", k->second.dev);
	  fprintf(out,"\n");
	}
      fprintf(out,"\n\n");
    }
  fclose(out);
}

