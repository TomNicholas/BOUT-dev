/*
 * Implements the shifted metric method for parallel derivatives
 * 
 * By default fields are stored so that X-Z are orthogonal,
 * and so not aligned in Y.
 *
 */

#include <bout/paralleltransform.hxx>
#include <bout/mesh.hxx>
#include <interpolation.hxx>
#include <fft.hxx>
#include <bout/constants.hxx>

#include <cmath>

#include <output.hxx>

ShiftedMetric::ShiftedMetric(Mesh &m) : mesh(m), zShift(&m) {
  // Read the zShift angle from the mesh
  
  if(mesh.get(zShift, "zShift")) {
    // No zShift variable. Try qinty in BOUT grid files
    mesh.get(zShift, "qinty");
  }

  int nmodes = mesh.LocalNz/2 + 1;
  //Allocate storage for complex intermediate
  cmplx.resize(nmodes);
  std::fill(cmplx.begin(), cmplx.end(), 0.0);
}

//As we're attached to a mesh we can expect the z direction to not change
//once we've been created so cache the complex phases used in transformations
//the first time they are needed
ShiftedMetric::arr3Dvec ShiftedMetric::getFromAlignedPhs(CELL_LOC location) {
  // bools so we only calculate the cached values the first time for each location
  static bool first_CENTRE = true, first_XLOW=true, first_YLOW=true;

  switch (location) {
  case CELL_CENTRE: {
    if (first_CENTRE) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      first_CENTRE = false;
      fromAlignedPhs_CENTRE.resize(mesh.LocalNx);

      for(int jx=0;jx<mesh.LocalNx;jx++){
        fromAlignedPhs_CENTRE[jx].resize(mesh.LocalNy);
        for(int jy=0;jy<mesh.LocalNy;jy++){
          fromAlignedPhs_CENTRE[jx][jy].resize(nmodes);
        }
      }
      //To/From field aligned phases
      for(int jx=0;jx<mesh.LocalNx;jx++){
        for(int jy=0;jy<mesh.LocalNy;jy++){
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            fromAlignedPhs_CENTRE[jx][jy][jz] = dcomplex(cos(kwave*zShift(jx,jy)) , -sin(kwave*zShift(jx,jy)));
          }
        }
      }
    }
    return fromAlignedPhs_CENTRE;
    break;
  }
  case CELL_XLOW: {
    if (first_XLOW) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // interpolate zShift to XLOW
      Field2D zShift_XLOW = interp_to(zShift, CELL_XLOW, RGN_ALL);
      zShift_XLOW.applyBoundary("neumann"); // Set boundary guard cells equal to nearest grid cell

      first_XLOW = false;
      fromAlignedPhs_XLOW.resize(mesh.LocalNx);

      for(int jx=0;jx<mesh.LocalNx;jx++){
        fromAlignedPhs_XLOW[jx].resize(mesh.LocalNy);
        for(int jy=0;jy<mesh.LocalNy;jy++){
          fromAlignedPhs_XLOW[jx][jy].resize(nmodes);
        }
      }
      //To/From field aligned phases
      for(int jx=0;jx<mesh.LocalNx;jx++){
        for(int jy=0;jy<mesh.LocalNy;jy++){
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            fromAlignedPhs_XLOW[jx][jy][jz] = dcomplex(cos(kwave*zShift_XLOW(jx,jy)), -sin(kwave*zShift_XLOW(jx,jy)));
          }
        }
      }
    }
    return fromAlignedPhs_XLOW;
    break;
  }
  case CELL_YLOW: {
    if (first_YLOW) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // interpolate zShift to YLOW
      Field2D zShift_YLOW = interp_to(zShift, CELL_YLOW, RGN_ALL);
      zShift_YLOW.applyBoundary("neumann"); // Set boundary guard cells equal to nearest grid cell

      first_YLOW = false;
      fromAlignedPhs_YLOW.resize(mesh.LocalNx);

      for(int jx=0;jx<mesh.LocalNx;jx++){
        fromAlignedPhs_YLOW[jx].resize(mesh.LocalNy);
        for(int jy=0;jy<mesh.LocalNy;jy++){
          fromAlignedPhs_YLOW[jx][jy].resize(nmodes);
        }
      }
      //To/From field aligned phases
      for(int jx=0;jx<mesh.LocalNx;jx++){
        for(int jy=0;jy<mesh.LocalNy;jy++){
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            fromAlignedPhs_YLOW[jx][jy][jz] = dcomplex(cos(kwave*zShift_YLOW(jx,jy)), -sin(kwave*zShift_YLOW(jx,jy)));
          }
        }
      }
    }
    return fromAlignedPhs_YLOW;
    break;
  }
  case CELL_ZLOW: {
    // shifts don't depend on z, so are the same for CELL_ZLOW as for CELL_CENTRE
    return getFromAlignedPhs(CELL_CENTRE);
    break;
  }
  default: {
    // This should never happen
    throw BoutException("Unsupported stagger of phase shifts\n"
                        " - don't know how to interpolate to %s",strLocation(location));
    break;
  }
  };
}

ShiftedMetric::arr3Dvec ShiftedMetric::getToAlignedPhs(CELL_LOC location) {
  // bools so we only calculate the cached values the first time for each location
  static bool first_CENTRE = true, first_XLOW=true, first_YLOW=true;

  switch (location) {
  case CELL_CENTRE: {
    if (first_CENTRE) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      first_CENTRE = false;
      toAlignedPhs_CENTRE.resize(mesh.LocalNx);

      for(int jx=0;jx<mesh.LocalNx;jx++){
        toAlignedPhs_CENTRE[jx].resize(mesh.LocalNy);
        for(int jy=0;jy<mesh.LocalNy;jy++){
          toAlignedPhs_CENTRE[jx][jy].resize(nmodes);
        }
      }
      //To/From field aligned phases
      for(int jx=0;jx<mesh.LocalNx;jx++){
        for(int jy=0;jy<mesh.LocalNy;jy++){
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            toAlignedPhs_CENTRE[jx][jy][jz] = dcomplex(cos(kwave*zShift(jx,jy)), sin(kwave*zShift(jx,jy)));
          }
        }
      }
    }
    return toAlignedPhs_CENTRE;
    break;
  }
  case CELL_XLOW: {
    if (first_XLOW) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // interpolate zShift to XLOW
      Field2D zShift_XLOW = interp_to(zShift, CELL_XLOW, RGN_ALL);
      zShift_XLOW.applyBoundary("neumann"); // Set boundary guard cells equal to nearest grid cell

      first_XLOW = false;
      toAlignedPhs_XLOW.resize(mesh.LocalNx);

      for(int jx=0;jx<mesh.LocalNx;jx++){
        toAlignedPhs_XLOW[jx].resize(mesh.LocalNy);
        for(int jy=0;jy<mesh.LocalNy;jy++){
          toAlignedPhs_XLOW[jx][jy].resize(nmodes);
        }
      }
      //To/From field aligned phases
      for(int jx=0;jx<mesh.LocalNx;jx++){
        for(int jy=0;jy<mesh.LocalNy;jy++){
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            toAlignedPhs_XLOW[jx][jy][jz] = dcomplex(cos(kwave*zShift_XLOW(jx,jy)), sin(kwave*zShift_XLOW(jx,jy)));
          }
        }
      }
    }
    return toAlignedPhs_XLOW;
    break;
  }
  case CELL_YLOW: {
    if (first_YLOW) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // interpolate zShift to YLOW
      Field2D zShift_YLOW = interp_to(zShift, CELL_YLOW, RGN_ALL);
      zShift_YLOW.applyBoundary("neumann"); // Set boundary guard cells equal to nearest grid cell

      first_YLOW = false;
      toAlignedPhs_YLOW.resize(mesh.LocalNx);

      for(int jx=0;jx<mesh.LocalNx;jx++){
        toAlignedPhs_YLOW[jx].resize(mesh.LocalNy);
        for(int jy=0;jy<mesh.LocalNy;jy++){
          toAlignedPhs_YLOW[jx][jy].resize(nmodes);
        }
      }
      //To/From field aligned phases
      for(int jx=0;jx<mesh.LocalNx;jx++){
        for(int jy=0;jy<mesh.LocalNy;jy++){
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            toAlignedPhs_YLOW[jx][jy][jz] = dcomplex(cos(kwave*zShift_YLOW(jx,jy)), sin(kwave*zShift_YLOW(jx,jy)));
          }
        }
      }
    }
    return toAlignedPhs_YLOW;
    break;
  }
  case CELL_ZLOW: {
    // shifts don't depend on z, so are the same for CELL_ZLOW as for CELL_CENTRE
    return getToAlignedPhs(CELL_CENTRE);
    break;
  }
  default: {
    // This should never happen
    throw BoutException("Unsupported stagger of phase shifts\n"
                        " - don't know how to interpolate to %s",strLocation(location));
    break;
  }
  };
}

ShiftedMetric::arr3Dvec ShiftedMetric::getYupPhs1(CELL_LOC location) {
  // bools so we only calculate the cached values the first time for each location
  static bool first_CENTRE = true, first_XLOW=true, first_YLOW=true;

  switch (location) {
  case CELL_CENTRE: {
    if (first_CENTRE) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      first_CENTRE = false;
      yupPhs1_CENTRE.resize(mesh.LocalNx);

      for(int jx=0;jx<mesh.LocalNx;jx++){
        yupPhs1_CENTRE[jx].resize(mesh.LocalNy);
        for(int jy=0;jy<mesh.LocalNy;jy++){
          yupPhs1_CENTRE[jx][jy].resize(nmodes);
        }
      }
      //To/From field aligned phases
      for(int jx=0;jx<mesh.LocalNx;jx++){
        for(int jy=0;jy<mesh.LocalNy;jy++){
          BoutReal yupShift1 = zShift(jx,jy) - zShift(jx,jy+1);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            yupPhs1_CENTRE[jx][jy][jz] = dcomplex(cos(kwave*yupShift1) , -sin(kwave*yupShift1));
          }
        }
      }
    }
    return yupPhs1_CENTRE;
    break;
  }
  case CELL_XLOW: {
    if (first_XLOW) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // interpolate zShift to XLOW
      Field2D zShift_XLOW = interp_to(zShift, CELL_XLOW, RGN_ALL);
      zShift_XLOW.applyBoundary("neumann"); // Set boundary guard cells equal to nearest grid cell

      first_XLOW = false;
      yupPhs1_XLOW.resize(mesh.LocalNx);

      for(int jx=0;jx<mesh.LocalNx;jx++){
        yupPhs1_XLOW[jx].resize(mesh.LocalNy);
        for(int jy=0;jy<mesh.LocalNy;jy++){
          yupPhs1_XLOW[jx][jy].resize(nmodes);
        }
      }
      //To/From field aligned phases
      for(int jx=0;jx<mesh.LocalNx;jx++){
        for(int jy=0;jy<mesh.LocalNy;jy++){
          BoutReal yupShift1 = zShift_XLOW(jx,jy) - zShift_XLOW(jx,jy+1);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            yupPhs1_XLOW[jx][jy][jz] = dcomplex(cos(kwave*yupShift1) , -sin(kwave*yupShift1));
          }
        }
      }
    }
    return yupPhs1_XLOW;
    break;
  }
  case CELL_YLOW: {
    if (first_YLOW) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // interpolate zShift to YLOW
      Field2D zShift_YLOW = interp_to(zShift, CELL_YLOW, RGN_ALL);
      zShift_YLOW.applyBoundary("neumann"); // Set boundary guard cells equal to nearest grid cell

      first_YLOW = false;
      yupPhs1_YLOW.resize(mesh.LocalNx);

      for(int jx=0;jx<mesh.LocalNx;jx++){
        yupPhs1_YLOW[jx].resize(mesh.LocalNy);
        for(int jy=0;jy<mesh.LocalNy;jy++){
          yupPhs1_YLOW[jx][jy].resize(nmodes);
        }
      }
      //To/From field aligned phases
      for(int jx=0;jx<mesh.LocalNx;jx++){
        for(int jy=0;jy<mesh.LocalNy;jy++){
          BoutReal yupShift1 = zShift_YLOW(jx,jy) - zShift_YLOW(jx,jy+1);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            yupPhs1_YLOW[jx][jy][jz] = dcomplex(cos(kwave*yupShift1) , -sin(kwave*yupShift1));
          }
        }
      }
    }
    return yupPhs1_YLOW;
    break;
  }
  case CELL_ZLOW: {
    // shifts don't depend on z, so are the same for CELL_ZLOW as for CELL_CENTRE
    return getYupPhs1(CELL_CENTRE);
    break;
  }
  default: {
    // This should never happen
    throw BoutException("Unsupported stagger of phase shifts\n"
                        " - don't know how to interpolate to %s",strLocation(location));
    break;
  }
  };
}

ShiftedMetric::arr3Dvec ShiftedMetric::getYupPhs2(CELL_LOC location) {
  // bools so we only calculate the cached values the first time for each location
  static bool first_CENTRE = true, first_XLOW=true, first_YLOW=true;

  switch (location) {
  case CELL_CENTRE: {
    if (first_CENTRE) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      first_CENTRE = false;
      yupPhs2_CENTRE.resize(mesh.LocalNx);

      for(int jx=0;jx<mesh.LocalNx;jx++){
        yupPhs2_CENTRE[jx].resize(mesh.LocalNy);
        for(int jy=0;jy<mesh.LocalNy;jy++){
          yupPhs2_CENTRE[jx][jy].resize(nmodes);
        }
      }
      //To/From field aligned phases
      for(int jx=0;jx<mesh.LocalNx;jx++){
        for(int jy=0;jy<mesh.LocalNy;jy++){
          BoutReal yupShift2 = zShift(jx,jy) - zShift(jx,jy+2);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            yupPhs2_CENTRE[jx][jy][jz] = dcomplex(cos(kwave*yupShift2) , -sin(kwave*yupShift2));
          }
        }
      }
    }
    return yupPhs2_CENTRE;
    break;
  }
  case CELL_XLOW: {
    if (first_XLOW) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // interpolate zShift to XLOW
      Field2D zShift_XLOW = interp_to(zShift, CELL_XLOW, RGN_ALL);
      zShift_XLOW.applyBoundary("neumann"); // Set boundary guard cells equal to nearest grid cell

      first_XLOW = false;
      yupPhs2_XLOW.resize(mesh.LocalNx);

      for(int jx=0;jx<mesh.LocalNx;jx++){
        yupPhs2_XLOW[jx].resize(mesh.LocalNy);
        for(int jy=0;jy<mesh.LocalNy;jy++){
          yupPhs2_XLOW[jx][jy].resize(nmodes);
        }
      }
      //To/From field aligned phases
      for(int jx=0;jx<mesh.LocalNx;jx++){
        for(int jy=0;jy<mesh.LocalNy;jy++){
          BoutReal yupShift2 = zShift_XLOW(jx,jy) - zShift_XLOW(jx,jy+2);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            yupPhs2_XLOW[jx][jy][jz] = dcomplex(cos(kwave*yupShift2) , -sin(kwave*yupShift2));
          }
        }
      }
    }
    return yupPhs2_XLOW;
    break;
  }
  case CELL_YLOW: {
    if (first_YLOW) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // interpolate zShift to YLOW
      Field2D zShift_YLOW = interp_to(zShift, CELL_YLOW, RGN_ALL);
      zShift_YLOW.applyBoundary("neumann"); // Set boundary guard cells equal to nearest grid cell

      first_YLOW = false;
      yupPhs2_YLOW.resize(mesh.LocalNx);

      for(int jx=0;jx<mesh.LocalNx;jx++){
        yupPhs2_YLOW[jx].resize(mesh.LocalNy);
        for(int jy=0;jy<mesh.LocalNy;jy++){
          yupPhs2_YLOW[jx][jy].resize(nmodes);
        }
      }
      //To/From field aligned phases
      for(int jx=0;jx<mesh.LocalNx;jx++){
        for(int jy=0;jy<mesh.LocalNy;jy++){
          BoutReal yupShift2 = zShift_YLOW(jx,jy) - zShift_YLOW(jx,jy+2);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            yupPhs2_YLOW[jx][jy][jz] = dcomplex(cos(kwave*yupShift2) , -sin(kwave*yupShift2));
          }
        }
      }
    }
    return yupPhs2_YLOW;
    break;
  }
  case CELL_ZLOW: {
    // shifts don't depend on z, so are the same for CELL_ZLOW as for CELL_CENTRE
    return getYupPhs1(CELL_CENTRE);
    break;
  }
  default: {
    // This should never happen
    throw BoutException("Unsupported stagger of phase shifts\n"
                        " - don't know how to interpolate to %s",strLocation(location));
    break;
  }
  };
}

ShiftedMetric::arr3Dvec ShiftedMetric::getYdownPhs1(CELL_LOC location) {
  // bools so we only calculate the cached values the first time for each location
  static bool first_CENTRE = true, first_XLOW=true, first_YLOW=true;

  switch (location) {
  case CELL_CENTRE: {
    if (first_CENTRE) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      first_CENTRE = false;
      ydownPhs1_CENTRE.resize(mesh.LocalNx);

      for(int jx=0;jx<mesh.LocalNx;jx++){
        ydownPhs1_CENTRE[jx].resize(mesh.LocalNy);
        for(int jy=0;jy<mesh.LocalNy;jy++){
          ydownPhs1_CENTRE[jx][jy].resize(nmodes);
        }
      }
      //To/From field aligned phases
      for(int jx=0;jx<mesh.LocalNx;jx++){
        for(int jy=0;jy<mesh.LocalNy;jy++){
          BoutReal ydownShift1 = zShift(jx,jy) - zShift(jx,jy-1);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            ydownPhs1_CENTRE[jx][jy][jz] = dcomplex(cos(kwave*ydownShift1) , -sin(kwave*ydownShift1));
          }
        }
      }
    }
    return ydownPhs1_CENTRE;
    break;
  }
  case CELL_XLOW: {
    if (first_XLOW) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // interpolate zShift to XLOW
      Field2D zShift_XLOW = interp_to(zShift, CELL_XLOW, RGN_ALL);
      zShift_XLOW.applyBoundary("neumann"); // Set boundary guard cells equal to nearest grid cell

      first_XLOW = false;
      ydownPhs1_XLOW.resize(mesh.LocalNx);

      for(int jx=0;jx<mesh.LocalNx;jx++){
        ydownPhs1_XLOW[jx].resize(mesh.LocalNy);
        for(int jy=0;jy<mesh.LocalNy;jy++){
          ydownPhs1_XLOW[jx][jy].resize(nmodes);
        }
      }
      //To/From field aligned phases
      for(int jx=0;jx<mesh.LocalNx;jx++){
        for(int jy=0;jy<mesh.LocalNy;jy++){
          BoutReal ydownShift1 = zShift_XLOW(jx,jy) - zShift_XLOW(jx,jy-1);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            ydownPhs1_XLOW[jx][jy][jz] = dcomplex(cos(kwave*ydownShift1) , -sin(kwave*ydownShift1));
          }
        }
      }
    }
    return ydownPhs1_XLOW;
    break;
  }
  case CELL_YLOW: {
    if (first_YLOW) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // interpolate zShift to YLOW
      Field2D zShift_YLOW = interp_to(zShift, CELL_YLOW, RGN_ALL);
      zShift_YLOW.applyBoundary("neumann"); // Set boundary guard cells equal to nearest grid cell

      first_YLOW = false;
      ydownPhs1_YLOW.resize(mesh.LocalNx);

      for(int jx=0;jx<mesh.LocalNx;jx++){
        ydownPhs1_YLOW[jx].resize(mesh.LocalNy);
        for(int jy=0;jy<mesh.LocalNy;jy++){
          ydownPhs1_YLOW[jx][jy].resize(nmodes);
        }
      }
      //To/From field aligned phases
      for(int jx=0;jx<mesh.LocalNx;jx++){
        for(int jy=0;jy<mesh.LocalNy;jy++){
          BoutReal ydownShift1 = zShift_YLOW(jx,jy) - zShift_YLOW(jx,jy-1);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            ydownPhs1_YLOW[jx][jy][jz] = dcomplex(cos(kwave*ydownShift1) , -sin(kwave*ydownShift1));
          }
        }
      }
    }
    return ydownPhs1_YLOW;
    break;
  }
  case CELL_ZLOW: {
    // shifts don't depend on z, so are the same for CELL_ZLOW as for CELL_CENTRE
    return getYdownPhs1(CELL_CENTRE);
    break;
  }
  default: {
    // This should never happen
    throw BoutException("Unsupported stagger of phase shifts\n"
                        " - don't know how to interpolate to %s",strLocation(location));
    break;
  }
  };
}

ShiftedMetric::arr3Dvec ShiftedMetric::getYdownPhs2(CELL_LOC location) {
  // bools so we only calculate the cached values the first time for each location
  static bool first_CENTRE = true, first_XLOW=true, first_YLOW=true;

  switch (location) {
  case CELL_CENTRE: {
    if (first_CENTRE) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      first_CENTRE = false;
      ydownPhs2_CENTRE.resize(mesh.LocalNx);

      for(int jx=0;jx<mesh.LocalNx;jx++){
        ydownPhs2_CENTRE[jx].resize(mesh.LocalNy);
        for(int jy=0;jy<mesh.LocalNy;jy++){
          ydownPhs2_CENTRE[jx][jy].resize(nmodes);
        }
      }
      //To/From field aligned phases
      for(int jx=0;jx<mesh.LocalNx;jx++){
        for(int jy=0;jy<mesh.LocalNy;jy++){
          BoutReal ydownShift2 = zShift(jx,jy) - zShift(jx,jy-2);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            ydownPhs2_CENTRE[jx][jy][jz] = dcomplex(cos(kwave*ydownShift2) , -sin(kwave*ydownShift2));
          }
        }
      }
    }
    return ydownPhs2_CENTRE;
    break;
  }
  case CELL_XLOW: {
    if (first_XLOW) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // interpolate zShift to XLOW
      Field2D zShift_XLOW = interp_to(zShift, CELL_XLOW, RGN_ALL);
      zShift_XLOW.applyBoundary("neumann"); // Set boundary guard cells equal to nearest grid cell

      first_XLOW = false;
      ydownPhs2_XLOW.resize(mesh.LocalNx);

      for(int jx=0;jx<mesh.LocalNx;jx++){
        ydownPhs2_XLOW[jx].resize(mesh.LocalNy);
        for(int jy=0;jy<mesh.LocalNy;jy++){
          ydownPhs2_XLOW[jx][jy].resize(nmodes);
        }
      }
      //To/From field aligned phases
      for(int jx=0;jx<mesh.LocalNx;jx++){
        for(int jy=0;jy<mesh.LocalNy;jy++){
          BoutReal ydownShift2 = zShift_XLOW(jx,jy) - zShift_XLOW(jx,jy-2);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            ydownPhs2_XLOW[jx][jy][jz] = dcomplex(cos(kwave*ydownShift2) , -sin(kwave*ydownShift2));
          }
        }
      }
    }
    return ydownPhs2_XLOW;
    break;
  }
  case CELL_YLOW: {
    if (first_YLOW) {
      int nmodes = mesh.LocalNz/2 + 1;
      BoutReal zlength = mesh.coordinates()->zlength();

      // interpolate zShift to YLOW
      Field2D zShift_YLOW = interp_to(zShift, CELL_YLOW, RGN_ALL);
      zShift_YLOW.applyBoundary("neumann"); // Set boundary guard cells equal to nearest grid cell

      first_YLOW = false;
      ydownPhs2_YLOW.resize(mesh.LocalNx);

      for(int jx=0;jx<mesh.LocalNx;jx++){
        ydownPhs2_YLOW[jx].resize(mesh.LocalNy);
        for(int jy=0;jy<mesh.LocalNy;jy++){
          ydownPhs2_YLOW[jx][jy].resize(nmodes);
        }
      }
      //To/From field aligned phases
      for(int jx=0;jx<mesh.LocalNx;jx++){
        for(int jy=0;jy<mesh.LocalNy;jy++){
          BoutReal ydownShift2 = zShift_YLOW(jx,jy) - zShift_YLOW(jx,jy-2);
          for(int jz=0;jz<nmodes;jz++) {
            BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
            ydownPhs2_YLOW[jx][jy][jz] = dcomplex(cos(kwave*ydownShift2) , -sin(kwave*ydownShift2));
          }
        }
      }
    }
    return ydownPhs2_YLOW;
    break;
  }
  case CELL_ZLOW: {
    // shifts don't depend on z, so are the same for CELL_ZLOW as for CELL_CENTRE
    return getYdownPhs1(CELL_CENTRE);
    break;
  }
  default: {
    // This should never happen
    throw BoutException("Unsupported stagger of phase shifts\n"
                        " - don't know how to interpolate to %s",strLocation(location));
    break;
  }
  };
}

/*!
 * Calculate the Y up and down fields
 */
void ShiftedMetric::calcYUpDown(Field3D &f) {
  f.splitYupYdown();
  CELL_LOC location = f.getLocation();
  
  Field3D& yup1 = f.yup();
  yup1.allocate();
  arr3Dvec phases = getYupPhs1(location);
  for(int jx=0;jx<mesh.LocalNx;jx++) {
    for(int jy=mesh.ystart;jy<=mesh.yend;jy++) {
      shiftZ(&(f(jx,jy+1,0)), phases[jx][jy], &(yup1(jx,jy+1,0)));
    }
  }
  if (mesh.ystart>1) {
    Field3D& yup2 = f.yup(2);
    yup2.allocate();
    phases = getYupPhs2(location);
    for(int jx=0;jx<mesh.LocalNx;jx++) {
      for(int jy=mesh.ystart;jy<=mesh.yend;jy++) {
        shiftZ(&(f(jx,jy+2,0)), phases[jx][jy], &(yup2(jx,jy+2,0)));
      }
    }
  }

  Field3D& ydown1 = f.ydown();
  ydown1.allocate();
  phases = getYdownPhs1(location);
  for(int jx=0;jx<mesh.LocalNx;jx++) {
    for(int jy=mesh.ystart;jy<=mesh.yend;jy++) {
      shiftZ(&(f(jx,jy-1,0)), phases[jx][jy], &(ydown1(jx,jy-1,0)));
    }
  }
  if (mesh.ystart > 1) {
    Field3D& ydown2 = f.ydown(2);
    ydown2.allocate();
    phases = getYdownPhs2(location);
    for(int jx=0;jx<mesh.LocalNx;jx++) {
      for(int jy=mesh.ystart;jy<=mesh.yend;jy++) {
        shiftZ(&(f(jx,jy-2,0)), phases[jx][jy], &(ydown2(jx,jy-2,0)));
      }
    }
  }
}
  
/*!
 * Shift the field so that X-Z is not orthogonal,
 * and Y is then field aligned.
 */
const Field3D ShiftedMetric::toFieldAligned(const Field3D &f, const REGION region) {
  return shiftZ(f, getToAlignedPhs(f.getLocation()), region);
}

/*!
 * Shift back, so that X-Z is orthogonal,
 * but Y is not field aligned.
 */
const Field3D ShiftedMetric::fromFieldAligned(const Field3D &f, const REGION region) {
  return shiftZ(f, getFromAlignedPhs(f.getLocation()), region);
}

const Field3D ShiftedMetric::shiftZ(const Field3D &f, const arr3Dvec &phs, const REGION region) {
  ASSERT1(&mesh == f.getMesh());
  ASSERT1(region == RGN_NOX || region == RGN_NOBNDRY); // Never calculate x-guard cells here
  if(mesh.LocalNz == 1)
    return f; // Shifting makes no difference

  Field3D result(f); // Initialize from f, mostly so location get set correctly. (Does not copy data because of copy-on-change).

  for(auto i : f.region2D(region)) {
    shiftZ(f(i.x,i.y), phs[i.x][i.y], result(i.x,i.y));
  }
  
  return result;

}

void ShiftedMetric::shiftZ(const BoutReal *in, const std::vector<dcomplex> &phs, BoutReal *out) {
  // Take forward FFT
  rfft(in, mesh.LocalNz, &cmplx[0]);

  //Following is an algorithm approach to write a = a*b where a and b are
  //vectors of dcomplex.
  //  std::transform(cmplxOneOff.begin(),cmplxOneOff.end(), ptr.begin(), 
  //		 cmplxOneOff.begin(), std::multiplies<dcomplex>());

  const int nmodes = cmplx.size();
  for(int jz=1;jz<nmodes;jz++) {
    cmplx[jz] *= phs[jz];
  }

  irfft(&cmplx[0], mesh.LocalNz, out); // Reverse FFT
}

//Old approach retained so we can still specify a general zShift
const Field3D ShiftedMetric::shiftZ(const Field3D &f, const Field2D &zangle, const REGION region) {
  ASSERT1(&mesh == f.getMesh());
  ASSERT1(region == RGN_NOX || region == RGN_NOBNDRY); // Never calculate x-guard cells here
  ASSERT1(f.getLocation() == zangle.getLocation());
  if(mesh.LocalNz == 1)
    return f; // Shifting makes no difference

  Field3D result(&mesh);
  result.allocate();
  invalidateGuards(result); // Won't set x-guard cells, so allow checking to throw exception if they are used.

  // We only use methods in ShiftedMetric to get fields for parallel operations
  // like interp_to or DDY.
  // Therefore we don't need x-guard cells, so do not set them.
  // (Note valgrind complains about corner guard cells if we try to loop over
  // the whole grid, because zShift is not initialized in the corner guard
  // cells.)
  for(auto i : f.region2D(region)) {
    shiftZ(f(i.x, i.y), mesh.LocalNz, zangle(i.x,i.y), result(i.x, i.y));
  }
  
  return result;
}

void ShiftedMetric::shiftZ(const BoutReal *in, int len, BoutReal zangle,  BoutReal *out) {
  int nmodes = len/2 + 1;

  // Complex array used for FFTs
  cmplxLoc.resize(nmodes);
  
  // Take forward FFT
  rfft(in, len, &cmplxLoc[0]);
  
  // Apply phase shift
  BoutReal zlength = mesh.coordinates()->zlength();
  for(int jz=1;jz<nmodes;jz++) {
    BoutReal kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
    cmplxLoc[jz] *= dcomplex(cos(kwave*zangle) , -sin(kwave*zangle));
  }

  irfft(&cmplxLoc[0], len, out); // Reverse FFT
}

void ShiftedMetric::outputVars(Datafile &file) {
  file.add(zShift, "zShift", 0);
}
