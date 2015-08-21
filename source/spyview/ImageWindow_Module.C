#include "ImageWindow_Module.H"
#include "ImageWindow.H"

ImageWindow_Module::ImageWindow_Module(ImageWindow *iwp) : iw(iwp)
{
  iw->registerModule(this);
}
