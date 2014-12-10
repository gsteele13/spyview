#ifndef __IMAGEWINDOW_FITTING_H__
#define __IMAGEWINDOW_FITTING_H__
#include "Gnuplot_Interface.H"
#include "ImageWindow_Module.H"
#include "myboost.h"
#include "FLTK_Serialization.H"
#include <boost/serialization/map.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/version.hpp>

using namespace boost;

class NewFitFuncWindow; // Forward dcln'; see ImageWindow_Fitting_Ui.fl
class FitControls;

class Fitting;
extern Fitting *Fitter;

class Fitting : public ImageWindow_Module
{
public:
/*** \desc A FitFunction is a class that can be used to fit a row or column of data.
     Each row or column can be fit by a different set of fitting functions, if desired.
**/
  class FitFunction
  {
  public:
    typedef std::set<std::string> vars_t; 
    vars_t vars;                   // List of variables this function uses.
    std::string name;              // Name of this fit function.  Human readable.
    std::string description;       // Human readable description of this function.
    std::string initialization;    // Gnuplot code to initializie this function.  Especially useful for 
                                     // setting up sane default values.  May be called many many times.
    std::string definition;        // Gnuplot expression for this function.
    std::string guess;             // Gnuplot code to setup initial guess using gp. vars called xbar, ybar, sigx, sigy
    std::string center;            // Name of variable that holds an appropriate "center" for the peak, or blank if not applicable.
    std::string width;             // Name of variable that holds an appropriate width for the peak, or blank if na.

    template<class Archive> void serialize(Archive & ar, const unsigned int version)
    {
      ar & vars;
      ar & name; ar & description; ar & initialization;
      ar & definition; ar & guess; ar & center; ar & width;
    };
    void identifyVariables();  // Look through the definition and ID the variables.  Internal use only.
    
    FitFunction(std::string namep, std::string descriptionp, std::string initp, std::string defp, std::string guessp="",std::string centerp="", std::string widthp="") :
      name(namep), description(descriptionp), initialization(initp), definition(defp), guess(guessp), center(centerp), width(widthp) { identifyVariables(); };
    FitFunction() {};
  };
  typedef std::list<FitFunction> FitFunctions_t;
  FitFunctions_t FitFunctions; 
  
  class FitFunctionInstance
  {
  public:
    FitFunctionInstance() : init(false), locked(false) {}; // Boost barfs if bools aren't initialized... Weird.

    FitFunction *func;
    std::string id;
    class variable 
    {
    public:
      double val;   ///< Current value of this variable
      double dev;   ///< Estimated std. dev of variable.
      double scale; ///< For internal use while fitting; used to keep all the values gnuplot uses of order unity.
      bool   hold;  ///< Hold constant in fitting; if true, don't include this variable as a free parameter in the fit.
      std::string name; // Name with the ID appended.
      variable(double valp, bool holdp=false) : val(valp), scale(1.0), hold(holdp) {};
      variable() : val(1.0), scale(1.0), hold(false) {};
      template<class Archive> void serialize(Archive & ar, const unsigned int version)
      {
	ar & val;
	if(version >= 2)
	  ar & dev;
	ar & scale;
	ar & hold;
      };

    };
 
    typedef std::map<std::string, variable> values_t;
    values_t values; // Values for this function in the fit.
    bool init;
    bool locked; // Locked means the variables are not included as free parameters in the fit.
    template<class Archive> void serialize(Archive & ar, const unsigned int version)
    { 
      /* This is a horrible hack, but for some reason boost doesn't fix up this pointer properly. */
      /* so instead we serialize the function name, and then look for it when loading. */
      if(Archive::is_loading::value)
	{
	  func = NULL;
	  std::string tmp;
	  ar & tmp;
	  for(FitFunctions_t::iterator i = Fitter->FitFunctions.begin(); i != Fitter->FitFunctions.end(); i++)
	    if(tmp == i->name)
	      {
		func = &(*i);
		break;
	      }
	  assert(func);
	}
      else
	{
	  ar & func->name;
	}
      ar & locked;
      ar & id;
      ar & values;
      ar & init;
      if(Archive::is_loading::value)
        setupVariableNames();
    };

    void loadVariables(Gnuplot_Interface &gp); // Send strings like x1=1; x2=2; x3=3; to gnuplot
    void fetchVariables(Gnuplot_Interface &gp);  // Get the variables back from gnuplot
    void scaleVariables(); // Set scale to all the variables to keep them near unity
    void unscaleVariables(); // Set the scale of all the variables to unity.
    
    std::string specializeExpression(std::string pltcmd); // Given a string pltcmd, expand variable names like x# to x_0
                                                          // Note that the result will only be an lvalue if scale is 1.0

    void plot(Gnuplot_Interface &gp, bool title=true); // Send a string like fitfunc(x) ti "" to gnuplot
    void listFitVars(Gnuplot_Interface &gp);  // Send a string like x1, x2, x3 to gnuplot
    void setupVariableNames(); // Stuff the name field of variables using the ID.
  };

  typedef std::list<FitFunctionInstance> FitFunctionInstances_t;
  typedef std::map<double,FitFunctionInstances_t> fitGrid_t; // All the fits organized by ordinate.
  fitGrid_t fitGrid;
  FitFunctionInstances_t *fitFuncs; // Current fit functions
  double curOrdinate; // Current ordinate

  typedef std::list<FitFunctionInstances_t> fitFunctionStack_t; // Just for the handiness of the user.
  fitFunctionStack_t fitStack;

  typedef std::pair<double, FitFunctionInstances_t> undoElement;
  typedef std::list<undoElement> undoStack_t;
  undoStack_t undoStack;
  undoStack_t redoStack;

  // These serialization routines are magic -- because we've avoided anything that needs a deep copy,
  // we can just assign, delete, copy, and serialize all the fit data trivially.
  template<class Archive> void serialize(Archive & ar, const unsigned int version)
  {
    ar & FitFunctions;
    ar & fitGrid;    
    fitFuncs = &(fitGrid.begin()->second);
  };

  static FitControls *fc; // Only one fitcontrols for all fitting instances.  Will there ever be more than one, though?
  
  Fitting(ImageWindow *iwp);
  static void Init();
  virtual ~Fitting() {};

  // Callbacks to hook us into imagewindow events.
  int event_callback(int event);
  void overlay_callback();
  void linecut_callback(bool init);

  void setupFitFunc(Gnuplot_Interface &gp, FitFunctionInstance *limit=NULL);  // Define fitfunc(x).  If limit is non-NULL, only include that function.
  void fit(FitFunctionInstance *restrict = NULL, bool undoable=true); // Run a fit.  If restrict is non-null, only optimize that function..  If undoable is false, don't modify the undo stack.
  void FitInSequence();                           // Fit each peak sequentially.  This tends to stabilize peak centers.
  void update();

  // Routines for manipulating fit functions from the UI
  void UpdateFitFunctions();                    // Update the list of fit functions in the fit function browser.
  void HandleEditDialog(NewFitFuncWindow *nff); // Update a fit function based on the result from the edit dialog
  
  void SelectFitFunction();                     // Select a fit function based on the browser.
  FitFunction *CurrentFitFunction();            // Returns current fit function, or null if none selected
  void bump(double pos);                        // Move the fit pos*range to the left/right

  // Routines for manipulating fit function instances from the UI
  FitFunctionInstance *CurrentFitInstance();    // Returns current fit function instance, or null if none selected
  void AddFitInstance();
  void LockFitInstance();                       // Lock a function so it isn't included as free parameters in the fit.
  void DelFitInstance();                        // Remove a fit intsance
  void AddFitInstanceToBrowser(FitFunctionInstance &f); // Add a fit function instance to the fit function browser.
  void ResetFitInstanceBrowser();               // Completely redraw the fit instance browser
  void SelectFitInstance();                     // Call whenever the fit instance selected in the browser changes.

  // Routines for handling the initial guess
  void ClearInitialGuess();
  void UseInitialGuess();

  // Routines for dealing with which line cut the user is looking at.
  void updateDirection();                       // Force ImageWindow to take line cuts in appropriate dirs.
  void moveLineCut(int lines, bool copy);       // Move line cut lines lines to the side.  Copy current fit if data is true.  
  double ordinate();                            // Find out the current x coordinate

  // Routines for manipulating the fit stack
  void PushStack();
  void PopStack();
  void ClearStack();
  void UpdateStackCount();                      // Change the visual marker for stack depth

  // Routines for using the undo stack
  void PushUndo();                              // Add the current line to the undo stack.
  void PopUndo();                               // Automatically select the new line, as well.
  void PopRedo();         
  void UpdateUndoCount();                       // Change visual marker for undo available.

  // Routines for saving/loading
  std::string DefaultName(std::string extn=".fit");
  void SaveFit(std::string fname="");
  void LoadFit(std::string fname="");

  // Routine for outputting the data
  void SaveText(std::string fname);

  void updateFitLimit();
  void fetchFitLimit();
};

BOOST_CLASS_VERSION(Fitting::FitFunctionInstance, 2); // Increment this if the archive format changes.

#endif
