#include "spypal_interface.H"
#include "spypal_import.H"

void spypal_wizard::init()
{
  import_cm = NULL;
  import_l = 0;
  grad->spw = this;
  populate_swatches(swatches);
  reset_tooltip();
  recalculate(true);
}

void spypal_wizard::recalculate(bool reposition)
{
  bool hit;
  assert(grad->sliders.size() >= 2);
  grad->sliders.sort(Fl_Spypal_GSlider::cmp);
  std::vector<SpypalWaypoint> wps;

  for(sliders_t::iterator i = grad->sliders.begin(); i != grad->sliders.end(); i++)
    {
      SpypalWaypoint wp;
      wp.c = (*i)->c;
      wp.loc = (*i)->loc;
      wp.locked = (*i)->locked;
      wps.push_back(wp);
    }
  hit = cache_hit(wps);
  //  printf("Recalculating: cache was %s\n", hit ? "hit" : "not hit");
  if(!hit)
    {
      generate_go(wps,(int)opt_direction->mvalue()->user_data(),true,
		  (ccspace *)colorspace->mvalue()->user_data(),
		  (ccspace *)metricspace->mvalue()->user_data(),
		  atoi(steps->value()));
      cache_waypoints = wps;
      cache_cmap = cmap;
      cache_steps = atoi(steps->value());
      cache_direction = (int)opt_direction->mvalue()->user_data();
      cache_colorspace = (ccspace *)colorspace->mvalue()->user_data();
      cache_metricspace = (ccspace *)metricspace->mvalue()->user_data();
;
    }
  else
    {
      wps = cache_waypoints;      
      cmap = cache_cmap;
    }

  if(reposition)
    grad->reposition_sliders(wps);

  if(!hit)
    {
      dump_colormap_memory();
      grad->update_image();       
      if(colormap_callback)
	colormap_callback();
    }
}


void spypal_wizard::set_tooltip(const char *str)
{
  if(tooltip)
    tooltip->value(str);
}

void spypal_wizard::reset_tooltip()
{
  if(grad->sliders.size() == 2)
    set_tooltip("Drag a swatch to the gradient bar to add a waypoint.  Click on a waypoint or swatch to edit its color.\nB1: Drag  B3: Lock a waypoint B2: Edit a waypoint.");
  else
    set_tooltip("Drag a swatch to the gradient bar to add a waypoint.  Delete a waypoint by dragging it away. Click on a waypoint or swatch to edit its color.\nB1: Drag  B3: Lock a waypoint B2: Edit a waypoint.");
}

void spypal_wizard::save(std::string fname)
{
  std::ofstream ofs(fname.c_str());
  try
    {
      if(!ofs.good())
 	throw(1);
    }
  catch (const std::exception& ex)
    {
      warn("Unable to create or overwrite Spypal colormap file \"%s\": %s\n",fname.c_str(),ex.what());
    }
  catch ( ... ) 
    {
      warn("Unable to create or overwrite Spypal colormap file \"%s\": %s\n",fname.c_str(),"unknown cause");
    }
 
  try
    {
      boost::archive::text_oarchive oa(ofs);
      oa & (*this);
    }
  catch (boost::archive::archive_exception e)
    {
      error("ImagePrinter serialization error: %s",e.what());
    }
  catch (...)
    {
      error("Unknown serialization error\n");
    }
}

void spypal_wizard::load(std::string fname)
{
  try 
    {
      std::ifstream ifs(fname.c_str());
      if(!ifs.good())
 	throw(1);
      boost::archive::text_iarchive ia(ifs);
      ia & (*this);
      recalculate(true);
      grad->redraw();
    }
  catch (boost::archive::archive_exception e)
    {
      error("Serialization error: %s",e.what());
    }
  catch (const std::exception& ex)
    {
      warn("Unable to load Spypal colormap \"%s\": %s\n",fname.c_str(),ex.what());    
    }
  catch (...)
    {
      warn("Unable to load Spypal colormap \"%s\": %s\n",fname.c_str(),"unknown error");
    }

}

void spypal_wizard::make_swatch(double R, double G, double B, const char *name)
{  
  int x = Fl::box_dx(swatches->box()) + swatches->x() + ms_c*(swatch_w+swatch_space);
  int y = Fl::box_dy(swatches->box()) + swatches->y() + ms_r*(swatch_h+swatch_space);
  Fl_Spypal_Swatch *s = new Fl_Spypal_Swatch(x,y,swatch_w,swatch_h,name);
  s->box(FL_UP_BOX);
  s->setGradient(grad);
  cc_sRGB.set(s->c,R,G,B);  
  ms_c++;
  if(ms_c >= ms_cols)
    {
      ms_c = 0;
      ms_r++;
    }
}

void spypal_wizard::populate_row(double g)
{
  double s = 1.0/sqrt(2);
  make_swatch(g,0,0);
  make_swatch(g,s*g,0,0);
  make_swatch(g,g,0,0);
  make_swatch(s*g,g,0,0);
  make_swatch(0,g,0,0);

  make_swatch(0,g,s*g);
  make_swatch(0,g,g);
  make_swatch(0,s*g,g);
  make_swatch(0,0,g);

  make_swatch(s*g,0,g);
  make_swatch(g,0,g);
  make_swatch(g,0,g*s);
}

void spypal_wizard::populate_swatches(Fl_Scroll *t)
{
  printf("Populating swatches\n");
  swatches = t;
  t->clear();
  t->begin();
  // 10 is scrollbar size guess.
  ms_cols=(t->w()-Fl::box_dw(t->box())-10)/(swatch_w+swatch_space);
  ms_r = 0; ms_c = 0;

  static const int ngray=11;
  for(int i = 0; i <= ngray; i++)
    {
      double g = ((double)i)/ngray;
      make_swatch(g,g,g);
    }
  next_swatch_row();

  populate_row(1.0);
  next_swatch_row();
  populate_row(pow(2.0,-0.5));
  next_swatch_row();
  populate_row(pow(2.0,-1.0));
  next_swatch_row();
  populate_row(pow(2.0,-2.0));

  t->end();
}

bool spypal_wizard::cache_hit(std::vector<SpypalWaypoint> &wps)
{
  if(cache_direction != (int)opt_direction->mvalue()->user_data())
    return false;
  if(cache_steps != atoi(steps->value()))
    return false;
  if(cache_colorspace != (ccspace *)colorspace->mvalue()->user_data())
    return false;
  if(cache_metricspace != (ccspace *)metricspace->mvalue()->user_data())
    return false;
  if(wps.size() != cache_waypoints.size())
    return false;
  for(unsigned i = 0; i < wps.size(); i++)
    {
      if(wps[i].c != cache_waypoints[i].c)
	return false;
      if(wps[i].locked != cache_waypoints[i].locked)
	return false;
      if(wps[i].locked && (cache_waypoints[i].loc != wps[i].loc))
	return false;
    }
  return true;
}

void spypal_wizard::space_evenly()
{
  int s = grad->sliders.size()-1;
  int j =0;
  for(sliders_t::iterator i = grad->sliders.begin(); i != grad->sliders.end(); i++)
    {
      (*i)->loc = ((double)j++)/s;
      (*i)->locked = true;
    }
  recalculate(true);
  grad->redraw();
}

void spypal_wizard::copy_cmap(unsigned char *cmp, unsigned int lp)
{
  if(import_cm)
    free(import_cm);
  import_cm = (unsigned char *)malloc(lp*3);
  assert(import_cm);
  memcpy(import_cm,cmp,lp*3);
  import_l=lp;
}

void spypal_wizard::import_update()
{
  assert(import_cm);
  assert(spypal);
  ccspace *cs =   (ccspace *)colorspace->mvalue()->user_data();
  ccspace *ms =   (ccspace *)metricspace->mvalue()->user_data();
  assert(cs);

  SpypalWaypoints_t wps(waypoints->value());

  //  double f = spypal_anneal(wps,cs,import_cm,l);
  double f = spypal_bisect_anneal(wps,cs,import_cm,import_l);
  char buf[1024];
  snprintf(buf,sizeof(buf),"%g (%.1f)",sqrt(f),sqrt(spypal_worst_error()));
  import_error->value(buf);  
  spypal->grad->set(wps);

  snprintf(buf,sizeof(buf),"%d",import_l);
  spypal->steps->value(buf);

  if(spypal_worst_error() <= sqrt(3.0)+1e-6)
    import_error->color(FL_GREEN);
  else if(spypal_worst_error() <= 4.0*sqrt(3.0)+1e-6)
    import_error->color(FL_YELLOW);
  else
    import_error->color(FL_RED);

  spypal->recalculate(true);
}
void spypal_wizard::import_residuals()
{
  if(import_l != spypal_colormap_size)
    return;
  FILE *f = fopen("spypal_residuals.dat","w");
  fprintf(f,"#tgt_r tgt_g tgt_b cmap_r cmap_g cmap_b\n");
  for(unsigned i = 0; i < import_l; i++)
    fprintf(f,"%d %d %d %d %d %d\n",
	    import_cm[i*3],import_cm[i*3+1],import_cm[i*3+2],
	    spypal_colormap[i*3],
	    spypal_colormap[i*3+1],
	    spypal_colormap[i*3+2]);  
  fclose(f);
}
