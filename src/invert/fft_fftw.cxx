/*!************************************************************************
 * \file fft_fftw.cxx
 *
 * \brief FFT routines using external libraries
 *
 **************************************************************************
 * Copyright 2010 B.D.Dudson, S.Farley, M.V.Umansky, X.Q.Xu
 *
 * Contact: Ben Dudson, bd512@york.ac.uk
 *
 * This file is part of BOUT++.
 *
 * BOUT++ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BOUT++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with BOUT++.  If not, see <http://www.gnu.org/licenses/>.
 *
 **************************************************************************/

#include <globals.hxx>
#include <options.hxx>
#include <fft.hxx>
#include <bout/constants.hxx>

#include <fftw3.h>
#include <math.h>

#ifdef _OPENMP
#include <omp.h>
#endif

bool fft_options = false;
bool fft_measure;

void fft_init()
{
  if(fft_options)
    return;
  //#pragma omp critical
  {
    Options *opt = Options::getRoot();
    opt = opt->getSection("fft");
    opt->get("fft_measure", fft_measure, false);
    fft_options = true;
  }
}

#ifndef _OPENMP
// Serial code
void cfft(dcomplex *cv, int length, int isign)
{
  static fftw_complex *in, *out;
  static fftw_plan pf, pb;
  static int n = 0;

  if(length != n) {
    if(n > 0) {
      fftw_destroy_plan(pf);
      fftw_destroy_plan(pb);
      fftw_free(in);
      fftw_free(out);
    }
    fft_init();

    in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * length);
    out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * length);

    unsigned int flags = FFTW_ESTIMATE;
    if(fft_measure)
      flags = FFTW_MEASURE;

    pf = fftw_plan_dft_1d(length, in, out, FFTW_FORWARD, flags);
    pb = fftw_plan_dft_1d(length, in, out, FFTW_BACKWARD, flags);

    n = length;
  }

  // Load input data
  for(int i=0;i<n;i++) {
    in[i][0] = cv[i].real();
    in[i][1] = cv[i].imag();
  }

  if(isign < 0) {
    // Forward transform
    fftw_execute(pf);
    for(int i=0;i<n;i++)
      cv[i] = dcomplex(out[i][0], out[i][1]) / static_cast<BoutReal>(n); // Normalise
  }else {
    // Backward
    fftw_execute(pb);
    for(int i=0;i<n;i++)
      cv[i] = dcomplex(out[i][0], out[i][1]);
  }
}
#else
// Parallel thread-safe version of cfft
void cfft(dcomplex *cv, int length, int isign)
{
  static fftw_complex *inall, *outall;
  static fftw_plan *pf, *pb;
  static int size = 0, nthreads;

  int th_id = omp_get_thread_num();
  #pragma omp critical
  {
    // Sort out memory. Also, FFTW planning routines not thread safe
    int n_th = omp_get_num_threads(); // Number of threads
    if((size != length) || (nthreads < n_th)) {
      if(size > 0) {
        // Free all memory
        for(int i=0;i<nthreads;i++) {
          fftw_destroy_plan(pf[i]);
          fftw_destroy_plan(pb[i]);
        }
        delete[] pf;
        delete[] pb;
        fftw_free(inall);
        fftw_free(outall);
      }

      inall = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * length * n_th);
      outall = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * length * n_th);

      pf = new fftw_plan[n_th]; //Never freed
      pb = new fftw_plan[n_th]; //Never freed

      unsigned int flags = FFTW_ESTIMATE;
      if(fft_measure)
        flags = FFTW_MEASURE;

      for(int i=0;i<n_th;i++) {
        pf[i] = fftw_plan_dft_1d(length, inall+i*length, outall+i*length,
                                 FFTW_FORWARD, flags);
        pb[i] = fftw_plan_dft_1d(length, inall+i*length, outall+i*length,
                                 FFTW_BACKWARD, flags);
      }

      nthreads = n_th;
      size = length;
    }
  }
  // Get working arrays for this thread
  fftw_complex *in = inall+th_id*length;
  fftw_complex *out = outall+th_id*length;

  // Load input data
  for(int i=0;i<length;i++) {
    in[i][0] = cv[i].real();
    in[i][1] = cv[i].imag();
  }

  if(isign < 0) {
    // Forward transform
    fftw_execute(pf[th_id]);
    for(int i=0;i<length;i++)
      cv[i] = dcomplex(out[i][0], out[i][1]) / static_cast<BoutReal>(length); // Normalise
  }else {
    // Backward
    fftw_execute(pb[th_id]);
    for(int i=0;i<length;i++)
      cv[i] = dcomplex(out[i][0], out[i][1]);
  }
}
#endif

/***********************************************************
 * Real FFTs
 ***********************************************************/

#ifndef _OPENMP
// Serial code


void rfft(const BoutReal *in, int length, dcomplex *out) {
  // static variables initialized once
  static double *fin;
  static fftw_complex *fout;
  static fftw_plan p;
  static int n = 0;

  // If the current length mismatches n
  if(length != n) {
    // If n has been used before
    if(n > 0) {
      // Free the previously initialized data
      fftw_destroy_plan(p);
      fftw_free(fin);
      fftw_free(fout);
    }

    fft_init();

    // Initialize the input for the fourier transformation
    fin = (double*) fftw_malloc(sizeof(double) * length);
    // Initialize the output of the fourier transformation
    /* NOTE: Only the non-redundant output is given
     *       I.e the offset and the positive frequencies (so no mirroring
     *       around the Nyquist frequency)
     */
    fout = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * (length/2 + 1));

    unsigned int flags = FFTW_ESTIMATE;
    if(fft_measure)
      flags = FFTW_MEASURE;

    /* fftw call
     * Plan a real-input/complex-output discrete Fourier transform (DFT)
     * in 1 dimensions. Returns a fftw_plan (containing pointers etc.)
     * r2c = real to complex
     */
    p = fftw_plan_dft_r2c_1d(length, fin, fout, flags);

    n = length;
  }

  // Put the input to fin
  for(int i=0;i<n;i++)
    fin[i] = in[i];

  // fftw call executing the fft
  fftw_execute(p);
  
  //Normalising factor
  const BoutReal fac = 1.0/((double) n);
  const int nmodes = (n/2) + 1;

  // Store the output in out, and normalize
  for(int i=0;i<nmodes;i++)
    out[i] = dcomplex(fout[i][0], fout[i][1]) * fac; // Normalise
}

const Array<dcomplex> rfft(const Array<BoutReal> &in) {
  ASSERT1(!in.empty()); // Check that there is data
  
  int size = in.size();
  Array<dcomplex> out(size); // Allocates data array
  
  rfft(in.begin(), size, out.begin());
  return out;
}

void irfft(const dcomplex *in, int length, BoutReal *out) {
  // static variables initialized once
  static fftw_complex *fin;
  static double *fout;
  static fftw_plan p;
  static int n = 0;

  // If the current length mismatches n
  if(length != n) {
    // If n has been used before
    if(n > 0) {
      // Free the previously initialized data
      fftw_destroy_plan(p);
      fftw_free(fin);
      fftw_free(fout);
    }

    fft_init();

    // Initilaize the input for the inverse fourier transformation
    /* NOTE: Only the non-redundant input is given
     *       I.e the offset and the positive frequencies (so no mirroring
     *       around the Nyquist frequency)
     */
    fin = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * (length/2 + 1));
    // Initialize the output of the fourier transformation
    fout = (double*) fftw_malloc(sizeof(double) * length);

    unsigned int flags = FFTW_ESTIMATE;
    if(fft_measure)
      flags = FFTW_MEASURE;

    /* fftw call
     * Plan a complex-input/real-output discrete Fourier transform (DFT)
     * in 1 dimensions. Returns a fftw_plan (containing pointers etc.)
     * c2r = complex to real
     */
    p = fftw_plan_dft_c2r_1d(length, fin, fout, flags);

    n = length;
  }

  // Store the real and imaginary parts in the proper way
  const int nmodes = (n/2) + 1;
  for(int i=0;i<nmodes;i++) {
    fin[i][0] = in[i].real();
    fin[i][1] = in[i].imag();
  }

  // fftw call executing the fft
  fftw_execute(p);

  // Store the output of the fftw to the out
  for(int i=0;i<n;i++)
    out[i] = fout[i];
}

#else
// Parallel thread-safe version of rfft and irfft
void rfft(const BoutReal *in, int length, dcomplex *out) {
  static double *finall;
  static fftw_complex *foutall;
  static fftw_plan *p;
  static int size = 0, nthreads;

  int th_id = omp_get_thread_num();
#pragma omp critical(rfft)
  {
    // Sort out memory. Also, FFTW planning routines not thread safe
    int n_th = omp_get_num_threads(); // Number of threads

    if((size != length) || (nthreads < n_th)) {
      if(size > 0) {
        // Free all memory
        for(int i=0;i<nthreads;i++)
          fftw_destroy_plan(p[i]);
        delete[] p;
        fftw_free(finall);
        fftw_free(foutall);
      }

      fft_init();

      finall = (double*) fftw_malloc(sizeof(double) * length * n_th);
      foutall = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * (length/2 + 1) * n_th);
      p = new fftw_plan[n_th]; //Never freed

      unsigned int flags = FFTW_ESTIMATE;
      if(fft_measure)
        flags = FFTW_MEASURE;

      for(int i=0;i<n_th;i++)
        // fftw call
        // Plan a real-input/complex-output discrete Fourier transform (DFT)
        // in 1 dimensions. Returns a fftw_plan (containing pointers etc.)
        p[i] = fftw_plan_dft_r2c_1d(length, finall+i*length,
                                    foutall+i*(length/2 + 1), flags);
      size = length;
      nthreads = n_th;
    }
  }

  // Get working arrays for this thread
  double *fin = finall + th_id * length;
  fftw_complex *fout = foutall + th_id * (length/2 + 1);

  for(int i=0;i<length;i++)
    fin[i] = in[i];

  // fftw call executing the fft
  fftw_execute(p[th_id]);

  //Normalising factor
  const BoutReal fac = 1.0 / static_cast<BoutReal>(length);
  const int nmodes = (length/2) + 1;

  for(int i=0;i<nmodes;i++)
    out[i] = dcomplex(fout[i][0], fout[i][1]) * fac; // Normalise
}

void irfft(const dcomplex *in, int length, BoutReal *out) {
  static fftw_complex *finall;
  static double *foutall;
  static fftw_plan *p;
  static int size = 0, nthreads;

  int th_id = omp_get_thread_num();
  int n_th = omp_get_num_threads(); // Number of threads
#pragma omp critical(irfft)
  {
    // Sort out memory. Also, FFTW planning routines not thread safe

    if((size != length) || (nthreads < n_th)) {
      if(size > 0) {
        // Free all memory
        for(int i=0;i<nthreads;i++)
          fftw_destroy_plan(p[i]);
        delete[] p;
        fftw_free(finall);
        fftw_free(foutall);
      }

      fft_init();

      finall = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * (length/2 + 1) * n_th);
      foutall = (double*) fftw_malloc(sizeof(double) * length * n_th);

      p = new fftw_plan[n_th]; //Never freed

      unsigned int flags = FFTW_ESTIMATE;
      if(fft_measure)
        flags = FFTW_MEASURE;

      for(int i=0;i<n_th;i++)
        p[i] = fftw_plan_dft_c2r_1d(length, finall+i*(length/2 + 1),
                                    foutall+i*length, flags);
      size = length;
      nthreads = n_th;
    }
  }

  // Get working arrays for this thread
  fftw_complex *fin = finall + th_id * (length/2 + 1);
  double *fout = foutall + th_id * length;

  const int nmodes = (length/2) + 1;

  for(int i=0;i<nmodes;i++) {
    fin[i][0] = in[i].real();
    fin[i][1] = in[i].imag();
  }

  // fftw call executing the fft
  fftw_execute(p[th_id]);

  for(int i=0;i<length;i++)
    out[i] = fout[i];
}
#endif

//  Discrete sine transforms (B Shanahan)

void DST(const BoutReal *in, int length, dcomplex *out) {
  static double *fin;
  static fftw_complex *fout;
  static fftw_plan p;
  static int n = 0;

  ASSERT1(length > 0);

  if(length != n) {
    if(n > 0) {
      fftw_destroy_plan(p);
      fftw_free(fin);
      fftw_free(fout);
      }

    //  fft_init();

    // Could be optimized better
    fin = (double*) fftw_malloc(sizeof(double) * 2 * length);
    fout = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * 2 * length);


    unsigned int flags = FFTW_ESTIMATE;
    if(fft_measure)
      flags = FFTW_MEASURE;

    // fftw call
    // Plan a real-input/complex-output discrete Fourier transform (DFT)
    // in 1 dimensions. Returns a fftw_plan (containing pointers etc.)
    p = fftw_plan_dft_r2c_1d(2*(length-1), fin, fout, flags);

    n = length;
  }
  for(int i=0;i<n;i++)
    fin[i] = in[i];

  fin[0] = 0.;
  fin[length-1]=0.;

  for (int j = 1; j < length-1; j++){
      fin[j] = in[j];
      fin[2*(length-1)-j] = - in[j];
  }

  // fftw call executing the fft
  fftw_execute(p);

  out[0]=0.0;
  out[length-1]=0.0;

  for(int i=1;i<length-1;i++)
    out[i] = -fout[i][1] / (static_cast<BoutReal>(length) - 1); // Normalise
}

void DST_rev(dcomplex *in, int length, BoutReal *out) {
  static fftw_complex *fin;
  static double *fout;
  static fftw_plan p;
  static int n = 0;

  ASSERT1(length > 0);

  if(length != n) {
    if(n > 0) {
      fftw_destroy_plan(p);
      fftw_free(fin);
      fftw_free(fout);
    }

    //fft_init();

    // Could be optimized better
    fin = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * 2 * (length-1));
    fout = (double*) fftw_malloc(sizeof(double)  * 2 * (length-1));

    unsigned int flags = FFTW_ESTIMATE;
    if(fft_measure)
      flags = FFTW_MEASURE;

    p = fftw_plan_dft_c2r_1d(2*(length-1), fin, fout, flags);

    n = length;
  }

  for(int i=0;i<n;i++){
    fin[i][0] = in[i].real();
    fin[i][1] = in[i].imag();
  }

  fin[0][0] = 0.; fin[0][1] = 0.;
  fin[length-1][0] = 0.; fin[length-1][1] = 0.;

  for (int j = 1; j < length-1; j++){
    fin[j][0] = 0.; fin[j][1] = -in[j].real()/2.;
    fin[2*(length-1)-j][0] = 0.; fin[2*(length-1)-j][1] =  in[j].real()/2.;
  }

  // fftw call executing the fft
  fftw_execute(p);

  out[0]=0.0;
  out[length-1]=0.0;
  for(int i=1;i<length-1;i++)
    out[i] = fout[i];
}
