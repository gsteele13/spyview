#include "spypal_wizard.h"

class spypal_wizard : public spypal_wizard_base
{
public:
  spypal_wizard() {};
  void init();
   
  void recalculate(bool reposition=false); // Iff reposition is true, reposition the sliders.
  void space_evenly(); // Reposition the sliders to be even.

  // Tooltip status
  void set_tooltip(const char *str);
  void reset_tooltip();

  // Swatch code.  Really should be in its own class.
  void populate_swatches(Fl_Scroll *t); // Populate swatch library.
  void make_swatch(double R, double G, double B, const char *name=0);
  void next_swatch_row() { if(ms_c == 0) return; ms_r++; ms_c = 0; };
  void populate_row(double g);

  // Save/load code.
  template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
  {
    ar & *grad;
    ar & flcast(steps);
    ar & flcast(opt_direction);
    if(version >= 2)
      {
	ar & flcast(colorspace);
      }
    if(version >= 3)
      {
	ar & flcast(metricspace);
	ar & flcast(advanced);
	ar & flcast(grad_style);
      }
#ifndef WIN32
    else
      {
	metricspace->value(
	  metricspace->find_item(colorspace->mvalue()->label()));
	if(((colorspace->mvalue()->user_data() == &cc_sRGB) ||
	    (colorspace->mvalue()->user_data() == &cc_CIELuv)) &&
	   opt_direction->mvalue()->user_data() == (void *)OPTS_STRAIGHT)
	  {
	    advanced->value(false);
	    if(colorspace->mvalue()->user_data() == &cc_sRGB)
	      grad_style->value(grad_style->find_item("sRGB"));
	    else
	      grad_style->value(grad_style->find_item("Luv"));
	  }
	else
	  advanced->value(true);	
      }
#endif
  }

  void save(std::string fname);
  void load(std::string fname);

  typedef Fl_Spypal_Gradient::sliders_t sliders_t; // Save typing.

  // Import controls
  unsigned char *import_cm;
  unsigned int import_l;
  void copy_cmap(unsigned char *cmp, unsigned int lp);
  void import_update();
  void import_residuals();

protected:
  // Swatch info... Really should be a "swatch table" class..
  int ms_r,ms_c, ms_cols;
  static const int swatch_w=35;
  static const int swatch_h=35;
  static const int swatch_space=2;

  // Cache for palette calculations
  bool cache_hit(std::vector<SpypalWaypoint> &colors); // True if current settings are same as cached settings.
  int  cache_steps;
  const ccspace *cache_colorspace;
  const ccspace *cache_metricspace;
  int cache_direction;
  std::vector<SpypalWaypoint> cache_waypoints;
  cmap_t cache_cmap;
};

BOOST_CLASS_VERSION(spypal_wizard, 3); // Increment this if the archive format changes.
