/****
  cclass.H/C
 
  classes to make it easy to manipulate colors in different colorspaces.
  (c) 2009 Oliver Dial
***/
#include <assert.h>
#include <stdio.h>
#include "cclass.H"
#include "ciexyz64.h" // CIE tristimulus values.

ccolor cc_WhitePoint(0.9505,1.0000,1.0890); // Can this be justified?
const cc_CIEXYZ_class cc_CIEXYZ;
const cc_CIExyY_class cc_CIExyY;
const cc_CIELAB_class cc_CIELAB;
const cc_sRGB_class cc_sRGB;
const cc_CIELuv_class cc_CIELuv;
const cc_HSV_class cc_HSV;
const cc_CIECAM02_Jab_class cc_CIECAM02;
const double ccspace::eps = 1e-4;

void cc_CIELAB_class::visible_gamut(double L, std::vector<double> &av, std::vector<double> &bv) const
{
  ccolor c;
  set(c,L,0,0);
  double wY;
  wY = c.Y;
  for(unsigned i = 0; i < sizeof(cie_std_obs)/sizeof(cie_std_obs[0]); i++)
    {
      double cx, cy, cz;
      cx = cie_std_obs[i][1];
      cy = cie_std_obs[i][2];
      cz = cie_std_obs[i][3];
      double s=wY/cy;
      c.X = cx * s;
      c.Y = cy * s;
      c.Z = cz * s;
      double l,a,b;
      get(c,l,a,b);
      av.push_back(a);
      bv.push_back(b);
    }
}

void cc_CIELuv_class::visible_gamut(double L, std::vector<double> &av, std::vector<double> &bv) const
{
  ccolor c;
  set(c,L,0,0);
  double wY;
  wY = c.Y;
  for(unsigned i = 0; i < sizeof(cie_std_obs)/sizeof(cie_std_obs[0]); i++)
    {
      double cx, cy, cz;
      cx = cie_std_obs[i][1];
      cy = cie_std_obs[i][2];
      cz = cie_std_obs[i][3];
      double s=wY/cy;
      c.X = cx * s;
      c.Y = cy * s;
      c.Z = cz * s;
      double l,a,b;
      get(c,l,a,b);
      av.push_back(a);
      bv.push_back(b);
    }
}

