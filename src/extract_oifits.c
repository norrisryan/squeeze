/*
Version: 2.0
SQUEEZE 2 - Image reconstruction software for optical interferometry,
based on Monte-Carlo Markov Chain algorithms.

Copyright (c) 2006-2014 Fabien Baron, John Monnier, Michael Ireland

This code makes use of the OIFITSLIB library written by Dr John Young
(University of Cambridge).

Based on SQUEEZE 1 written by Prof. Fabien Baron (Georgia State University)
and on MACIM written by Dr. Michael Ireland with contributions from
Prof. John Monnier (University of Michigan).

SQUEEZE is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

SQUEEZE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Squeeze.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "../lib/oifitslib/src/oifitslib/exchange.h"
#include <complex.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

void find_vec_minmax(double *output_min, double *output_max, double *vec, long N)
{
  double min = 0;
  double max = 0;
  for (long i = 0; i < N; i++)
    {
      if (vec[i] > max)
        max = vec[i];
      if ((vec[i] < min) || (i == 0))
        min = vec[i];
    }
  *output_min = min;
  *output_max = max;
}

void add_new_uv(long *obs_index, long *uvindex, double new_u, double new_v, double new_uv_lambda, double new_uv_dlambda, double new_uv_time, double *table_u,
                double *table_v, double *table_uv_lambda, double *table_uv_dlambda, double *table_uv_time, double uvtol);

int import_single_epoch_oifits(char *filename, bool use_visamp, bool use_v2, bool use_t3amp, bool use_t4amp, bool use_visphi, bool use_t3phi, bool use_t4phi,
                               double visampa, double visamps, double v2a, double v2s, double t3ampa, double t3amps, double t4ampa, double t4amps,
                               double visphia, double visphis, double t3phia, double t3phis, double t4phia, double t4phis, double fluxs, double cwhm,
                               double uvtol, int *pnwavr, double **pwavmin, double **pwavmax, bool wavauto, double *timemin, double *timemax)
{

  //  printf("wavauto = %d\n", wavauto);

  int nwavr;
  double *wavmin, *wavmax;
  if (wavauto == FALSE) // nwavr set outside, classic mode
    {
      nwavr = *pnwavr;
      wavmin = *pwavmin;
      wavmax = *pwavmax;
    }
  else
    {
      // read in OI_WAVE tables
      STATUS status = 0;
      fitsfile *infile;
      fits_open_file(&infile, filename, READONLY, &status);
      oi_wavelength wave;
      nwavr = 0;
      // First scan just to get the total number of wavelengths
      // while (status == 0)
      //{
      read_next_oi_wavelength(infile, &wave, &status);
      if (status == 0)
        {
          printf("OIFITS import -- Scanning OI_WAVELENGTH table: %s with %d wavebands\n", wave.insname, wave.nwave);
          // nwavr+=wave.nwave;
          nwavr = wave.nwave;
          free_oi_wavelength(&wave);
        }
      //}
      fits_close_file(infile, &status);
      // now allocate memory
      wavmin = malloc(nwavr * sizeof(double));
      wavmax = malloc(nwavr * sizeof(double));

      // reopen file
      status = 0;
      // TODO or BUG : import more than one set of wavelengths
      // this may require merging tables, etc.
      fits_open_file(&infile, filename, READONLY, &status);
      read_next_oi_wavelength(infile, &wave, &status);
      if (status == 0)
        {
          printf("OIFITS import -- Importing OI_WAVELENGTH table: %s\n", wave.insname);
          // Note: OIFITS stores as eff_wave and eff_band, and we use min/max
          for (int w = 0; w < wave.nwave; w++)
            {
              wavmin[w] = wave.eff_wave[w] - 0.5 * wave.eff_band[w];
              wavmax[w] = wave.eff_wave[w] + 0.5 * wave.eff_band[w];
              printf("OIFITS import -- channel %d  = %lf um to\t %lf um\n", w, wavmin[w] * 1e6, wavmax[w] * 1e6);
            }
        }
      fits_close_file(infile, &status);

      // output the variable for use in main squeeze
      *pwavmin = wavmin;
      *pwavmax = wavmax;
      *pnwavr = nwavr;
    }

  // oi_array array;
  oi_target targets;
  oi_wavelength wave;
  oi_vis vis_table;
  oi_vis2 vis2_table;
  oi_t3 t3_table;
  oi_t4 t4_table;
  int nvis_tables, nv2_tables, nt3_tables, nt4_tables;
  double *v2, *v2_sig, *t4phi, *t4phi_sig, *t4amp, *t4amp_sig, *t3phi, *t3phi_sig, *t3amp, *t3amp_sig, *visamp, *visamp_sig, *visphi, *visphi_sig;
  double temp;
  double *time_t4, *time_t3, *time_vis, *time_v2;
  float *lambda_t4, *lambda_t3, *lambda_vis, *lambda_v2;
  float *dlambda_t4, *dlambda_t3, *dlambda_vis, *dlambda_v2;
  char *flag_t4, *flag_t3, *flag_vis, *flag_v2;
  long uvindex = 0, uvindex0;
  long tempindex, tempindex0;
  bool valid_v2, valid_t4amp, valid_t4phi, valid_t3amp, valid_t3phi, valid_visamp, valid_visphi;
  long i, j, k, w;
  int status, status2;

  fitsfile *fptr;
  fitsfile *fptr2;

  /* Read new FITS file */
  status = 0;
  printf("OIFITS import -- Enumerating tables for: %s...\n", filename);
  fflush(stdout);
  fits_open_file(&fptr, filename, READONLY, &status);

  if (status)
    {
      fits_report_error(stderr, status);
      exit(1);
    }

  read_oi_target(fptr, &targets, &status);
  fits_close_file(fptr, &status);
  free_oi_target(&targets);

  //
  // Count the number of tables
  //

  // Vis tables
  // BUG: this version of squeeze supports only all diff VIS or all absolute VIS, i.e. no mixing
  status = 0;
  nvis = 0;
  nvisamp = 0;
  nvisphi = 0;
  nvisamp_orphans = 0;
  nvisphi_orphans = 0;
  nvis_tables = 0;
  if ((use_visamp == TRUE) || (use_visphi == TRUE))
    {
      fits_open_file(&fptr, filename, READONLY, &status);
      read_next_oi_vis(fptr, &vis_table, &status);
      while (status == 0)
        {
          nvis = nvis + vis_table.numrec * vis_table.nwave;
          nvis_tables++;
          // TBD: our oifitslib with T4 is not compatible with differential vis
          //  if((diffvis == FALSE ) && (strcmp(vis_table.phityp, "differential") == 0))
          //  {
          //    diffvis=TRUE;
          //    printf("OIFITS import -- Differential visibilities detected\n");
          //  }
          // printf("Vis Table\t %i\t Points %ld Channels %d\n", nvis_tables, nvis, vis_table.nwave);
          free_oi_vis(&vis_table);
          read_next_oi_vis(fptr, &vis_table, &status);
        }
      fits_close_file(fptr, &status);

      printf("OIFITS import -- OI_VIS  \tTables %i\t Entries %ld\n", nvis_tables, nvis);
      fflush(stdout);
    }

  // VIS2 tables
  status = 0;
  nv2_tables = 0;
  nv2 = 0;
  if (use_v2 == TRUE)
    {
      fits_open_file(&fptr, filename, READONLY, &status);
      read_next_oi_vis2(fptr, &vis2_table, &status);
      while (status == 0)
        {
          nv2 = nv2 + vis2_table.numrec * vis2_table.nwave;
          nv2_tables++;
          free_oi_vis2(&vis2_table);
          read_next_oi_vis2(fptr, &vis2_table, &status);
        }

      fits_close_file(fptr, &status);

      printf("OIFITS import -- OI_VIS2 \tTables %i\t Entries %ld\n", nv2_tables, nv2);
      fflush(stdout);
    }

  // T3 tables
  status = 0;
  nt3 = 0;
  nt3_tables = 0;
  nt3amp = 0;
  nt3phi = 0;
  nt3amp_orphans = 0;
  nt3phi_orphans = 0;
  if ((use_t3phi == TRUE) || (use_t3amp == TRUE))
    {
      fits_open_file(&fptr, filename, READONLY, &status);
      read_next_oi_t3(fptr, &t3_table, &status);
      while (status == 0)
        {
          nt3 = nt3 + t3_table.numrec * t3_table.nwave;
          nt3_tables++;
          // printf("T3 Table %i. Total T3 Points %ld\n",nt3_tables,nt3);
          free_oi_t3(&t3_table);
          read_next_oi_t3(fptr, &t3_table, &status);
        }
      fits_close_file(fptr, &status);

      printf("OIFITS import -- OI_T3   \tTables %i\t Entries %ld\n", nt3_tables, nt3);
      fflush(stdout);
    }

  // T4 tables
  status = 0;
  nt4 = 0;
  nt4_tables = 0;
  nt4amp = 0;
  nt4phi = 0;
  nt4amp_orphans = 0;
  nt4phi_orphans = 0;
  if ((use_t4phi == TRUE) || (use_t4amp == TRUE))
    {
      fits_open_file(&fptr, filename, READONLY, &status);
      read_next_oi_t4(fptr, &t4_table, &status);
      while (status == 0)
        {
          nt4 = nt4 + t4_table.numrec * t4_table.nwave;
          nt4_tables++;
          free_oi_t4(&t4_table);
          read_next_oi_t4(fptr, &t4_table, &status);
        }
      fits_close_file(fptr, &status);

      printf("OIFITS import -- OI_T4   \tTables %i\t Entries %ld\n", nt4_tables, nt4);
      fflush(stdout);
    }

  // Allocate memory
  nuv = nt4 * 4 + nt3 * 3 + nvis + nv2; // maximum number of unique uv points defined in the OIFITS

  // UV points
  u = malloc(nuv * sizeof(double));
  v = malloc(nuv * sizeof(double));
  uv_lambda = malloc(nuv * sizeof(double));
  uv_dlambda = malloc(nuv * sizeof(double));
  uv_time = malloc(nuv * sizeof(double));

  // Lookup tables for observable to corresponding UV numbers

  visin = malloc(nvis * sizeof(long));
  visamp = malloc(nvis * sizeof(double));
  visamp_sig = malloc(nvis * sizeof(double));
  visphi = malloc(nvis * sizeof(double));
  visphi_sig = malloc(nvis * sizeof(double));
  time_vis = malloc(nvis * sizeof(double));
  lambda_vis = malloc(nvis * sizeof(float));
  dlambda_vis = malloc(nvis * sizeof(float));
  flag_vis = malloc(nvis * sizeof(char));

  if (diffvis == TRUE)
    {
      dvisnwav = malloc(nvis * sizeof(long));
      dvisindx = malloc(nvis * sizeof(long *));
    }

  v2in = malloc(nv2 * sizeof(long));
  v2 = malloc(nv2 * sizeof(double));
  v2_sig = malloc(nv2 * sizeof(double));
  time_v2 = malloc(nv2 * sizeof(double));
  lambda_v2 = malloc(nv2 * sizeof(float));
  dlambda_v2 = malloc(nv2 * sizeof(float));
  flag_v2 = malloc(nv2 * sizeof(char));

  t3in1 = malloc(nt3 * sizeof(long));
  t3in2 = malloc(nt3 * sizeof(long));
  t3in3 = malloc(nt3 * sizeof(long));
  t3amp = malloc(nt3 * sizeof(double));
  t3amp_sig = malloc(nt3 * sizeof(double));
  t3phi = malloc(nt3 * sizeof(double));
  t3phi_sig = malloc(nt3 * sizeof(double));
  time_t3 = malloc(nt3 * sizeof(double));
  lambda_t3 = malloc(nt3 * sizeof(float));
  dlambda_t3 = malloc(nt3 * sizeof(float));
  flag_t3 = malloc(nt3 * sizeof(char));

  t4in1 = malloc(nt4 * sizeof(long));
  t4in2 = malloc(nt4 * sizeof(long));
  t4in3 = malloc(nt4 * sizeof(long));
  t4in4 = malloc(nt4 * sizeof(long));
  t4amp = malloc(nt4 * sizeof(double));
  t4amp_sig = malloc(nt4 * sizeof(double));
  t4phi = malloc(nt4 * sizeof(double));
  t4phi_sig = malloc(nt4 * sizeof(double));
  time_t4 = malloc(nt4 * sizeof(double));
  lambda_t4 = malloc(nt4 * sizeof(float));
  dlambda_t4 = malloc(nt4 * sizeof(float));
  flag_t4 = malloc(nt4 * sizeof(char));

  // V2
  if (nv2_tables > 0)
    {
      printf("OIFITS import -- Importing V2 data...\n");
      tempindex = 0;
      status = 0;
      status2 = 0;
      fits_open_file(&fptr, filename, READONLY, &status);
      fits_open_file(&fptr2, filename, READONLY, &status2);

      while (tempindex < nv2)
        {

          read_next_oi_vis2(fptr, &vis2_table, &status);
          read_oi_wavelength(fptr2, vis2_table.insname, &wave, &status2);
          printf("Reading V2 tables...\n");
          for (i = 0; i < vis2_table.numrec; i++)
            {

              for (j = 0; j < vis2_table.nwave; j++)
                {
                  valid_v2 = (use_v2 == TRUE) &&
                             !(((vis2_table.record[i]).flag[j] != 0) || isnan((vis2_table.record[i]).vis2data[j]) || isnan((vis2_table.record[i]).vis2err[j]) ||
                               isinf((vis2_table.record[i]).vis2data[j]) || isinf((vis2_table.record[i]).vis2err[j]) ||
                               ((vis2_table.record[i]).vis2data[j] == DOUBLENULLVALUE) || ((vis2_table.record[i]).vis2data[j] == FLOATNULLVALUE) ||
                               ((vis2_table.record[i]).vis2err[j] == DOUBLENULLVALUE) || ((vis2_table.record[i]).vis2err[j] == FLOATNULLVALUE) ||
                               (vis2_table.record[i]).vis2err[j] <= 0);
                  if (valid_v2 == TRUE)
                    {

                      // Add new V2 data
                      v2[tempindex] = (vis2_table.record[i]).vis2data[j];
                      v2_sig[tempindex] = 1. / (vis2_table.record[i]).vis2err[j];
                      time_v2[tempindex] = (vis2_table.record[i]).mjd;
                      lambda_v2[tempindex] = wave.eff_wave[j];
                      dlambda_v2[tempindex] = wave.eff_band[j];
                      flag_v2[tempindex] = (vis2_table.record[i]).flag[j];

                      // Add uv information if new uv point is not redundant
                      add_new_uv(&v2in[tempindex], &uvindex,
                                 (vis2_table.record[i]).ucoord / lambda_v2[tempindex], // new_u
                                 (vis2_table.record[i]).vcoord / lambda_v2[tempindex], // new_v
                                 wave.eff_wave[j],                                     // new_uv_lambda
                                 wave.eff_band[j],                                     // new_uv_dlambda
                                 (vis2_table.record[i]).mjd,                           // new_uvtime
                                 u, v, uv_lambda, uv_dlambda, uv_time, uvtol);

                      // Move to next V2 point
                      tempindex++;
                    }
                  else
                    {
                      nv2--;
                      printf("OIFITS import -- Discarded V2 point, Record: %ld\t Wav: %ld Flag: %d V2: %f V2err: %f \n", i, j, (vis2_table.record[i]).flag[j],
                             (vis2_table.record[i]).vis2data[j], (vis2_table.record[i]).vis2err[j]);
                    }
                }
            }

          free_oi_wavelength(&wave);
          free_oi_vis2(&vis2_table);
        }
      fits_close_file(fptr, &status);
      fits_close_file(fptr2, &status2);
    }

  // T3 tables
  if (nt3_tables > 0)
    {
      printf("OIFITS import -- Importing T3 data...\n");
      tempindex = 0;
      nt3amp = 0;
      nt3phi = 0;
      nt3amp_orphans = 0;
      nt3phi_orphans = 0;
      status = 0;
      status2 = 0;
      fits_open_file(&fptr, filename, READONLY, &status);
      fits_open_file(&fptr2, filename, READONLY, &status2);
      while (tempindex < nt3)
        {

          read_next_oi_t3(fptr, &t3_table, &status);
          read_oi_wavelength(fptr2, t3_table.insname, &wave, &status2);
          printf("Reading T3 tables...\n");
          for (i = 0; i < t3_table.numrec; ++i)
            {

              for (j = 0; j < t3_table.nwave; ++j)
                {

                  // note: negative t3amp such as t3amp = -.01 +/- 0.03 are valid
                  // as long as we interpret that as a distribution, i.e. the true t3amp > 0
                  valid_t3amp =
                      (use_t3amp == TRUE) &&
                      !(((t3_table.record[i]).t3amperr[j] <= 0) || isnan((t3_table.record[i]).t3amp[j]) || isnan((t3_table.record[i]).t3amperr[j]) ||
                        isinf((t3_table.record[i]).t3amp[j]) || isinf((t3_table.record[i]).t3amperr[j]) || ((t3_table.record[i]).t3amp[j] == DOUBLENULLVALUE) ||
                        ((t3_table.record[i]).t3amp[j] == FLOATNULLVALUE) || ((t3_table.record[i]).t3amperr[j] == DOUBLENULLVALUE) ||
                        ((t3_table.record[i]).t3amperr[j] == FLOATNULLVALUE) || ((t3_table.record[i]).flag[j] != 0));

                  valid_t3phi =
                      (use_t3phi == TRUE) &&
                      !(((t3_table.record[i]).t3phierr[j] <= 0) || isnan((t3_table.record[i]).t3phi[j]) || isnan((t3_table.record[i]).t3phierr[j]) ||
                        isinf((t3_table.record[i]).t3phi[j]) || isinf((t3_table.record[i]).t3phierr[j]) || ((t3_table.record[i]).t3phi[j] == DOUBLENULLVALUE) ||
                        ((t3_table.record[i]).t3phi[j] == FLOATNULLVALUE) || ((t3_table.record[i]).t3phierr[j] == DOUBLENULLVALUE) ||
                        ((t3_table.record[i]).t3phierr[j] == FLOATNULLVALUE) || ((t3_table.record[i]).flag[j] != 0));

                  if ((valid_t3amp == TRUE) || (valid_t3phi == TRUE))
                    {

                      // Add new T3

                      if (valid_t3amp == TRUE)
                        {
                          nt3amp++;
                          t3amp[tempindex] = (t3_table.record[i]).t3amp[j];
                          t3amp_sig[tempindex] = 1. / (t3_table.record[i]).t3amperr[j];
                        }
                      else
                        {
                          nt3phi_orphans++;
                          t3amp[tempindex] = 0;
                          t3amp_sig[tempindex] = 0;
                        }

                      if (valid_t3phi == TRUE)
                        {
                          nt3phi++;
                          t3phi[tempindex] = (t3_table.record[i]).t3phi[j];
                          t3phi_sig[tempindex] = 1. / (t3_table.record[i]).t3phierr[j];
                        }
                      else
                        {
                          nt3amp_orphans++;
                          t3phi[tempindex] = 0;
                          t3phi_sig[tempindex] = 0;
                        }

                      time_t3[tempindex] = (t3_table.record[i]).mjd;
                      lambda_t3[tempindex] = wave.eff_wave[j];
                      dlambda_t3[tempindex] = wave.eff_band[j];
                      flag_t3[tempindex] = (t3_table.record[i]).flag[j];

                      // Add uv information if new uv points are not redundant
                      add_new_uv(&t3in1[tempindex], &uvindex,
                                 (t3_table.record[i]).u1coord / lambda_t3[tempindex], // new_u
                                 (t3_table.record[i]).v1coord / lambda_t3[tempindex], // new_v
                                 wave.eff_wave[j],                                    // new_uv_lambda
                                 wave.eff_band[j],                                    // new_uv_dlambda
                                 (t3_table.record[i]).mjd,                            // new_uvtime
                                 u, v, uv_lambda, uv_dlambda, uv_time, uvtol);

                      add_new_uv(&t3in2[tempindex], &uvindex,
                                 (t3_table.record[i]).u2coord / lambda_t3[tempindex], // new_u
                                 (t3_table.record[i]).v2coord / lambda_t3[tempindex], // new_v
                                 wave.eff_wave[j],                                    // new_uv_lambda
                                 wave.eff_band[j],                                    // new_uv_dlambda
                                 (t3_table.record[i]).mjd,                            // new_uvtime
                                 u, v, uv_lambda, uv_dlambda, uv_time, uvtol);

                      add_new_uv(&t3in3[tempindex], &uvindex,
                                 ((t3_table.record[i]).u1coord + (t3_table.record[i]).u2coord) / lambda_t3[tempindex], // new_u
                                 ((t3_table.record[i]).v1coord + (t3_table.record[i]).v2coord) / lambda_t3[tempindex], // new_v
                                 wave.eff_wave[j],                                                                     // new_uv_lambda
                                 wave.eff_band[j],                                                                     // new_uv_dlambda
                                 (t3_table.record[i]).mjd,                                                             // new_uvtime
                                 u, v, uv_lambda, uv_dlambda, uv_time, uvtol);

                      tempindex++;
                    }
                  else
                    {
                      printf("OIFITS import -- Discarded T3 point, Record: %ld Wav: %ld Flag: %d T3amp: %f T3amperr: %f T3phi: %f T3phierr: %f\n", i, j,
                             (t3_table.record[i]).flag[j], (t3_table.record[i]).t3amp[j], (t3_table.record[i]).t3amperr[j], (t3_table.record[i]).t3phi[j],
                             (t3_table.record[i]).t3phierr[j]);

                      nt3--;
                    }
                }
            }

          free_oi_wavelength(&wave);
          free_oi_t3(&t3_table);
        }
      fits_close_file(fptr, &status);
      fits_close_file(fptr2, &status2);
    }

  // T4 tables
  if (nt4_tables > 0)
    {
      printf("OIFITS import -- Importing T4 data...\n");
      tempindex = 0;
      nt4amp = 0;
      nt4phi = 0;
      nt4amp_orphans = 0;
      nt4phi_orphans = 0;
      status = 0;
      status2 = 0;
      fits_open_file(&fptr, filename, READONLY, &status);
      fits_open_file(&fptr2, filename, READONLY, &status2);
      while (tempindex < nt4)
        {

          read_next_oi_t4(fptr, &t4_table, &status);
          read_oi_wavelength(fptr2, t4_table.insname, &wave, &status2);
          printf("Reading T4 tables...\n");
          for (i = 0; i < t4_table.numrec; ++i)
            {

              for (j = 0; j < t4_table.nwave; ++j)
                {

                  // note: negative t4amp such as t4amp = -.01 +/- 0.03 are valid
                  // as long as we interpret that as a distribution, i.e. the true t4amp > 0
                  valid_t4amp =
                      (use_t4amp == TRUE) &&
                      !(((t4_table.record[i]).t4amperr[j] <= 0) || isnan((t4_table.record[i]).t4amp[j]) || isnan((t4_table.record[i]).t4amperr[j]) ||
                        isinf((t4_table.record[i]).t4amp[j]) || isinf((t4_table.record[i]).t4amperr[j]) || ((t4_table.record[i]).t4amp[j] == DOUBLENULLVALUE) ||
                        ((t4_table.record[i]).t4amp[j] == FLOATNULLVALUE) || ((t4_table.record[i]).t4amperr[j] == DOUBLENULLVALUE) ||
                        ((t4_table.record[i]).t4amperr[j] == FLOATNULLVALUE) || ((t4_table.record[i]).flag[j] != 0));

                  valid_t4phi =
                      (use_t4phi == TRUE) &&
                      !(((t4_table.record[i]).t4phierr[j] <= 0) || isnan((t4_table.record[i]).t4phi[j]) || isnan((t4_table.record[i]).t4phierr[j]) ||
                        isinf((t4_table.record[i]).t4phi[j]) || isinf((t4_table.record[i]).t4phierr[j]) || ((t4_table.record[i]).t4phi[j] == DOUBLENULLVALUE) ||
                        ((t4_table.record[i]).t4phi[j] == FLOATNULLVALUE) || ((t4_table.record[i]).t4phierr[j] == DOUBLENULLVALUE) ||
                        ((t4_table.record[i]).t4phierr[j] == FLOATNULLVALUE) || ((t4_table.record[i]).flag[j] != 0));

                  if ((valid_t4amp == TRUE) || (valid_t4phi == TRUE))
                    {

                      // Add new t4

                      if (valid_t4amp == TRUE)
                        {
                          nt4amp++;
                          t4amp[tempindex] = (t4_table.record[i]).t4amp[j];
                          t4amp_sig[tempindex] = 1. / (t4_table.record[i]).t4amperr[j];
                        }
                      else
                        {
                          nt4phi_orphans++;
                          t4amp[tempindex] = 0;
                          t4amp_sig[tempindex] = 0;
                        }

                      if (valid_t4phi == TRUE)
                        {
                          nt4phi++;
                          t4phi[tempindex] = (t4_table.record[i]).t4phi[j];
                          t4phi_sig[tempindex] = 1. / (t4_table.record[i]).t4phierr[j];
                        }
                      else
                        {
                          nt4amp_orphans++;
                          t4phi[tempindex] = 0;
                          t4phi_sig[tempindex] = 0;
                        }

                      time_t4[tempindex] = (t4_table.record[i]).mjd;
                      lambda_t4[tempindex] = wave.eff_wave[j];
                      dlambda_t4[tempindex] = wave.eff_band[j];
                      flag_t4[tempindex] = (t4_table.record[i]).flag[j];

                      // Add uv information if new uv points are not redundant
                      add_new_uv(&t4in1[tempindex], &uvindex,
                                 (t4_table.record[i]).u1coord / lambda_t4[tempindex], // new_u
                                 (t4_table.record[i]).v1coord / lambda_t4[tempindex], // new_v
                                 wave.eff_wave[j],                                    // new_uv_lambda
                                 wave.eff_band[j],                                    // new_uv_dlambda
                                 (t4_table.record[i]).mjd,                            // new_uvtime
                                 u, v, uv_lambda, uv_dlambda, uv_time, uvtol);

                      add_new_uv(&t4in2[tempindex], &uvindex,
                                 (t4_table.record[i]).u2coord / lambda_t4[tempindex], // new_u
                                 (t4_table.record[i]).v2coord / lambda_t4[tempindex], // new_v
                                 wave.eff_wave[j],                                    // new_uv_lambda
                                 wave.eff_band[j],                                    // new_uv_dlambda
                                 (t4_table.record[i]).mjd,                            // new_uvtime
                                 u, v, uv_lambda, uv_dlambda, uv_time, uvtol);

                      add_new_uv(&t4in3[tempindex], &uvindex,
                                 (t4_table.record[i]).u3coord / lambda_t4[tempindex], // new_u
                                 (t4_table.record[i]).v3coord / lambda_t4[tempindex], // new_v
                                 wave.eff_wave[j],                                                                     // new_uv_lambda
                                 wave.eff_band[j],                                                                     // new_uv_dlambda
                                 (t4_table.record[i]).mjd,                                                             // new_uvtime
                                 u, v, uv_lambda, uv_dlambda, uv_time, uvtol);

                      add_new_uv(&t4in4[tempindex], &uvindex,
                                 ((t4_table.record[i]).u1coord + (t4_table.record[i]).u2coord + (t4_table.record[i]).u3coord) / lambda_t4[tempindex], // new_u
                                 ((t4_table.record[i]).v1coord + (t4_table.record[i]).v2coord + (t4_table.record[i]).v3coord) / lambda_t4[tempindex], // new_v
                                 wave.eff_wave[j],                                                                     // new_uv_lambda
                                 wave.eff_band[j],                                                                     // new_uv_dlambda
                                 (t4_table.record[i]).mjd,                                                             // new_uvtime
                                 u, v, uv_lambda, uv_dlambda, uv_time, uvtol);
                      tempindex++;
                    }
                  else
                    {
                      printf("OIFITS import -- Discarded T4 point, Record: %ld Wav: %ld Flag: %d t4amp: %f t4amperr: %f t4phi: %f t4phierr: %f\n", i, j,
                             (t4_table.record[i]).flag[j], (t4_table.record[i]).t4amp[j], (t4_table.record[i]).t4amperr[j], (t4_table.record[i]).t4phi[j],
                             (t4_table.record[i]).t4phierr[j]);

                      nt4--;
                    }
                }
            }

          free_oi_wavelength(&wave);
          free_oi_t4(&t4_table);
        }
      fits_close_file(fptr, &status);
      fits_close_file(fptr2, &status2);
    }

  if (nvis_tables > 0)
    {
      // VIS table -- general case
      printf("OIFITS import -- Importing VIS tables...\n");
      tempindex = 0;
      nvisamp = 0;
      nvisphi = 0;
      nvisamp_orphans = 0;
      nvisphi_orphans = 0;

      status = 0;
      status2 = 0;
      fits_open_file(&fptr, filename, READONLY, &status);
      fits_open_file(&fptr2, filename, READONLY, &status2);

      while (tempindex < nvis)
        {
          read_next_oi_vis(fptr, &vis_table, &status);
          read_oi_wavelength(fptr2, vis_table.insname, &wave, &status2);
          for (i = 0; i < vis_table.numrec; i++)
            {
              uvindex0 = uvindex;
              tempindex0 = tempindex;
              for (j = 0; j < vis_table.nwave; j++)
                {
                  valid_visamp = (use_visamp == TRUE) &&
                                 !(((vis_table.record[i]).visamperr[j] <= 0) || isnan((vis_table.record[i]).visamp[j]) ||
                                   isnan((vis_table.record[i]).visamperr[j]) || isinf((vis_table.record[i]).visamp[j]) ||
                                   isinf((vis_table.record[i]).visamperr[j]) || ((vis_table.record[i]).visamp[j] == DOUBLENULLVALUE) ||
                                   ((vis_table.record[i]).visamp[j] == FLOATNULLVALUE) || ((vis_table.record[i]).visamperr[j] == DOUBLENULLVALUE) ||
                                   ((vis_table.record[i]).visamperr[j] == FLOATNULLVALUE) || ((vis_table.record[i]).flag[j] != 0));

                  valid_visphi = (use_visphi == TRUE) &&
                                 !(((vis_table.record[i]).visphierr[j] <= 0) || isnan((vis_table.record[i]).visphi[j]) ||
                                   isnan((vis_table.record[i]).visphierr[j]) || isinf((vis_table.record[i]).visphi[j]) ||
                                   isinf((vis_table.record[i]).visphierr[j]) || ((vis_table.record[i]).visphi[j] == DOUBLENULLVALUE) ||
                                   ((vis_table.record[i]).visphi[j] == FLOATNULLVALUE) || ((vis_table.record[i]).visphierr[j] == DOUBLENULLVALUE) ||
                                   ((vis_table.record[i]).visphierr[j] == FLOATNULLVALUE) || ((vis_table.record[i]).flag[j] != 0));

                  if ((valid_visamp == TRUE) || (valid_visphi == TRUE))
                    {

                      if (valid_visamp == TRUE)
                        {
                          nvisamp++;
                          visamp[tempindex] = (vis_table.record[i]).visamp[j];
                          visamp_sig[tempindex] = 1. / (vis_table.record[i]).visamperr[j];
                        }
                      else
                        {
                          nvisphi_orphans++;
                          visamp[tempindex] = 0;
                          visamp_sig[tempindex] = 0;
                        }

                      if (valid_visphi == TRUE)
                        {
                          nvisphi++;
                          visphi[tempindex] = (vis_table.record[i]).visphi[j];
                          visphi_sig[tempindex] = 1. / (vis_table.record[i]).visphierr[j];
                        }
                      else
                        {
                          nvisamp_orphans++;
                          visphi[tempindex] = 0;
                          visphi_sig[tempindex] = 0;
                        }

                      time_vis[tempindex] = (vis_table.record[i]).mjd;
                      lambda_vis[tempindex] = wave.eff_wave[j];
                      dlambda_vis[tempindex] = wave.eff_band[j];
                      flag_vis[tempindex] = (vis_table.record[i]).flag[j];

                      add_new_uv(&visin[tempindex], &uvindex, (vis_table.record[i]).ucoord / lambda_vis[tempindex],
                                 (vis_table.record[i]).vcoord / lambda_vis[tempindex],
                                 wave.eff_wave[j],          // new_uv_lambda
                                 wave.eff_band[j],          // new_uv_lambda
                                 (vis_table.record[i]).mjd, // new_uvtime
                                 u, v, uv_lambda, uv_dlambda, uv_time, uvtol);

                      tempindex++;
                    }
                  else
                    {
                      if ((use_visamp == TRUE) && (use_visphi == TRUE))
                        printf("OIFITS import -- Bad vis at tempindex %ld -- visamp test: %d -- visphi test: %d \n", tempindex, valid_visamp, valid_visphi);
                      nvis--;
                    }
                }

              if (diffvis == TRUE)
                // we need to wait for all the uv points to have been defined
                // tempindex0 points to the first point in the channel
                // tempindex should be at the last point
                {
                  // if dvis can be defined: there are enough wavelengths & no points were flagged/bad
                  if ((vis_table.nwave > 1) && ((tempindex - tempindex0) == vis_table.nwave))
                    {
                      // go through each points
                      for (j = 0; j < vis_table.nwave; j++)
                        {
                          // number of channels required to compute reference channel
                          // we just add up whatever is in visrefmap
                          dvisnwav[tempindex0 + j] = 0;
                          // TBD
                          // for(k=0;k<vis_table.nwave;k++)
                          //  if((vis_table.record[i]).visrefmap[j*vis_table.nwave+k] !=0)
                          //    dvisnwav[tempindex0 + j] += 1;

                          // indexes of visibilities that are to be averaged to for the reference channels
                          // I chose to keep the full nwave size instead of collapsing it
                          // so an index of -1 means the wavelength is not to be used
                          dvisindx[tempindex0 + j] = (long *)malloc(vis_table.nwave * sizeof(long));
                          // printf("DVIS: %ld ",tempindex0+j);
                          for (k = 0; k < vis_table.nwave; k++)
                            {
                              // TBD
                              //  if( (vis_table.record[i]).visrefmap[j*vis_table.nwave+k] !=0)
                              //    dvisindx[tempindex0 + j][k] = visin[tempindex0 + k];
                              //  else
                              dvisindx[tempindex0 + j][k] = -1;
                            }
                        }
                    }
                  else
                    {
                      printf("OOOOPS BUG when reading differential vis!\n");
                      for (j = tempindex0; j <= tempindex; j++)
                        dvisnwav[j] = -1; // in effect, dvis computation will be skipped
                    }
                }
            }

          free_oi_wavelength(&wave);
          free_oi_vis(&vis_table);
        }

      fits_close_file(fptr, &status);
      fits_close_file(fptr2, &status2);
    }

  printf("OIFITS import -- SUMMARY for %s\n", filename);
  if (nv2 > 0)
    {
      printf("OIFITS import -- V2: %ld powerspectrum imported\n", nv2);
    }

  if (nt3 > 0)
    {
      printf("OIFITS import -- T3: %ld bispectrum imported\n", nt3);
      printf("OIFITS import --     T3AMP : %ld", nt3amp);
      if (nt3amp_orphans > 0)
        printf(" -- %ld orphans (without corresponding T3PHI)\n", nt3amp_orphans);
      else
        printf(" -- no orphan points\n");
      printf("OIFITS import --     T3PHI : %ld", nt3phi);
      if (nt3phi_orphans > 0)
        printf(" -- %ld orphans (without corresponding T3AMP)\n", nt3phi_orphans);
      else
        printf(" -- no orphan points\n");
    }

  if (nt4 > 0)
    {
      printf("OIFITS import -- T4: %ld bispectrum imported\n", nt4);
      printf("OIFITS import --     T4AMP : %ld", nt4amp);
      if (nt4amp_orphans > 0)
        printf(" -- %ld orphans (without corresponding T4PHI)\n", nt4amp_orphans);
      else
        printf(" -- no orphan points\n");
      printf("OIFITS import --     T4PHI : %ld", nt4phi);
      if (nt4phi_orphans > 0)
        printf(" -- %ld orphans (without corresponding T4AMP)\n", nt4phi_orphans);
      else
        printf(" -- no orphan points\n");
    }

  if (nvis > 0)
    {
      if (diffvis == TRUE)
        printf("OIFITS import -- VIS: %ld differential visibilities imported\n", nvis);
      else
        printf("OIFITS import -- VIS: %ld complex visibilities imported\n", nvis);
      printf("OIFITS import --     VISAMP: %ld", nvisamp);
      if (nvisamp_orphans > 0)
        printf(" -- %ld orphans (without corresponding VISPHI)\n", nvisamp_orphans);
      else
        printf(" -- no orphan points\n");
      printf("OIFITS import --     VISPHI: %ld", nvisphi);
      if (nvisphi_orphans > 0)
        printf(" -- %ld orphans (without corresponding VISAMP)\n", nvisphi_orphans);
      else
        printf(" -- no orphan points\n");
    }
  printf("OIFITS import -- Unique uv points:\t%ld (out of %ld)\n", uvindex, nuv);
  printf("OIFITS import --                  \t(using uvtol=%lf)\n", uvtol);
  nuv = uvindex;

  //
  // Check for smallest/largest wavelength
  //

  double minwav, maxwav;
  find_vec_minmax(&minwav, &maxwav, uv_lambda, nuv);
  printf("OIFITS import -- Wavelength range:\t%lf - %lf um\n", minwav * 1e6, maxwav * 1e6);

  if (nwavr == 1)
    {
      wavmin[0] = minwav;
      wavmax[0] = maxwav;
    }

  double mintime, maxtime;
  find_vec_minmax(&mintime, &maxtime, uv_time, nuv);
  printf("OIFITS import -- MJD range:\t%lf - %lf \n", mintime, maxtime);

  if (ntimer == 1)
    {
      timemin[0] = mintime;
      timemax[0] = maxtime;
    }

  //
  // Assign uv points to reconstruction channels for polychromatic reconstruction
  //

  long *nuv_chan = malloc(nwavr * sizeof(long)); // will store number of uv points in each wavelength channel
  for (w = 0; w < nwavr; w++)
    nuv_chan[w] = 0;
  uvwav2chan = malloc(nuv * sizeof(double));
  for (i = 0; i < nuv; i++)
    {
      uvwav2chan[i] = -1;
      for (w = 0; w < nwavr; w++)
        {
          if ((uv_lambda[i] >= wavmin[w]) && (uv_lambda[i] <= wavmax[w]))
            {
              uvwav2chan[i] = w;
              nuv_chan[w]++;
              break;
            }
        }

      if (uvwav2chan[i] < 0)
        {
          printf("WARNING -- Discarded uv point outside of waveband channels: %ld at uv_lambda[i]: %le\n", i, uv_lambda[i]);
        }
    }

  for (w = 0; w < nwavr; w++)
    {
      printf("OIFITS import -- Channel: %ld (%lf - %lf um) has %ld uv points\n", w, wavmin[w] * 1e6, wavmax[w] * 1e6, nuv_chan[w]);
      if (nuv_chan[w] == 0)
        {
          printf("OIFITS import -- No data in channel %ld\n", w);
          getchar();
        }
    }
  free(nuv_chan);

  // Fill the data vector and do flux scaling if need be
  // The idea behind this is to make the data easier to filter/use
  // Order is VISAMP, V2, T3AMP, T4AMP, VISPHI, T3PHI, T4PHI
  // We are also getting rid of all the bad data detected in previous steps
  // and correcting for zeroflux for amplitudes
  // and converting angles to radians
  data     = malloc((nvisamp + nv2 + nt3amp + nt4amp + nt3phi + nvisphi + nt4phi) * sizeof(double));
  data_err = malloc((nvisamp + nv2 + nt3amp + nt4amp + nt3phi + nvisphi + nt4phi) * sizeof(double));
  const long visampoffset = 0;
  const long v2offset = nvisamp;
  const long t3ampoffset = nvisamp + nv2;
  const long t4ampoffset = nvisamp + nv2 + nt3amp;
  const long visphioffset = nvisamp + nv2 + nt3amp + nt4amp;
  const long t3phioffset = nvisamp + nv2 + nt3amp + nt4amp + nvisphi;
  const long t4phioffset = nvisamp + nv2 + nt3amp + nt4amp + nvisphi + nt3phi;

  if (fluxs != 1.0)
    printf("OIFITS import -- Applying zeroflux scaling factor: %lf\n", fluxs);

  for (i = 0; i < nvisamp; i++)
    {
      data[visampoffset + i] = visamp[i] / fluxs;
      data_err[visampoffset + i] = visamp_sig[i] * fluxs;
    }

  for (i = 0; i < nv2; i++)
    {
      data[v2offset + i] = v2[i] / (fluxs * fluxs);
      data_err[v2offset + i] = v2_sig[i] * (fluxs * fluxs);
    }

  for (i = 0; i < nt3amp; i++)
    {
      data[t3ampoffset + i] = t3amp[i] / (fluxs * fluxs * fluxs);
      data_err[t3ampoffset + i] = t3amp_sig[i] * (fluxs * fluxs * fluxs);
    }

  for (i = 0; i < nt4amp; i++)
    {
      data[t4ampoffset + i] = t4amp[i];
      data_err[t4ampoffset + i] = t4amp_sig[i];
    }

  for (i = 0; i < nvisphi; i++)
    {
      data[visphioffset + i] = visphi[i] / 180. * M_PI;
      data_err[visphioffset + i] = visphi_sig[i];
    }

  for (i = 0; i < nt3phi; i++)
    {
      data[t3phioffset + i] = t3phi[i] / 180. * M_PI;
      data_err[t3phioffset + i] = t3phi_sig[i];
    }

  for (i = 0; i < nt4phi; i++)
    {
      data[t4phioffset + i] = t4phi[i] / 180. * M_PI;
      data_err[t4phioffset + i] = t4phi_sig[i];
    }

  // Error scaling + check for negative errors + convert from degrees to radians

  if ((v2s != 1.0) || (v2a != 0.0))
    printf("OIFITS import -- V2 rescaling: mult=%lf add= %lf\n", v2s, v2a);
  if ((t3amps != 1.0) || (t3ampa != 0.0))
    printf("OIFITS import -- T3AMP rescaling: mult=%lf add= %lf\n", t3amps, t3ampa);
  if ((t3phis != 1.0) || (t3phia != 0.0))
    printf("OIFITS import -- T3PHI rescaling: mult=%lf add= %lf\n", t3phis, t3phia);
  if ((t4amps != 1.0) || (t4ampa != 0.0))
    printf("OIFITS import -- T4AMP rescaling: mult=%lf add= %lf\n", t4amps, t4ampa);
  if ((t4phis != 1.0) || (t4phia != 0.0))
    printf("OIFITS import -- T4PHI rescaling: mult=%lf add= %lf\n", t4phis, t4phia);
  if ((visamps != 1.0) || (visampa != 0.0))
    printf("OIFITS import -- VISAMP rescaling: mult=%lf add= %lf\n", visamps, visampa);
  if ((visphis != 1.0) || (visphia != 0.0))
    printf("OIFITS import -- VISPHI rescaling: mult=%lf add= %lf\n", visphis, visphia);

  // TBD: clean this up by using the new datatypes in SQUEEZE 3 -- this shouldn't be longer than a few lines.
  for (i = 0; i < nvisamp; i++) // TBD: depends on whether diff vis !
    {
      if (data_err[visampoffset + i] > 0)
        temp = (1. / data_err[visampoffset + i] * visamps + visampa);
      else
        temp = 0;

      if (temp > 0)
        data_err[visampoffset + i] = 1. / temp;
      else
        data_err[visampoffset + i] = 0.;
    }

  for (i = 0; i < nv2; i++)
    {
      if (data_err[v2offset + i] > 0)
        temp = (1. / data_err[v2offset + i] * v2s + v2a);
      else
        temp = 0;

      if (temp > 0)
        data_err[v2offset + i] = 1. / temp;
      else
        data_err[v2offset + i] = 0.;
    }

  for (i = 0; i < nt3amp; i++)
    {
      if (data_err[t3ampoffset + i] > 0)
        temp = (1. / data_err[t3ampoffset + i] * t3amps + t3ampa);
      else
        temp = 0;

      if (temp > 0)
        data_err[t3ampoffset + i] = 1. / temp;
      else
        data_err[t3ampoffset + i] = 0.;
    }

  for (i = 0; i < nt4amp; i++)
    {
      if (data_err[t4ampoffset + i] > 0)
        temp = (1. / data_err[t4ampoffset + i] * t4amps + t4ampa);
      else
        temp = 0;

      if (temp > 0)
        data_err[t4ampoffset + i] = 1. / temp;
      else
        data_err[t4ampoffset + i] = 0.;
    }

  for (i = 0; i < nvisphi; i++)
    {
      if (data_err[visphioffset + i] > 0)
        temp = (1. / data_err[visphioffset + i] * visphis + visphia) / 180. * M_PI;
      else
        temp = 0;

      if (temp > 0)
        data_err[visphioffset + i] = 1. / temp;
      else
        data_err[visphioffset + i] = 0.;
    }

  for (i = 0; i < nt3phi; i++)
    {
      if (data_err[t3phioffset + i] > 0)
        temp = (1. / data_err[t3phioffset + i] * t3phis + t3phia) / 180. * M_PI;
      else
        temp = 0;

      if (temp > 0)
        data_err[t3phioffset + i] = 1. / temp;
      else
        data_err[t3phioffset + i] = 0.;
    }

  for (i = 0; i < nt4phi; i++)
    {
      if (data_err[t4phioffset + i] > 0)
        temp = (1. / data_err[t4phioffset + i] * t4phis + t4phia) / 180. * M_PI;
      else
        temp = 0;

      if (temp > 0)
        data_err[t4phioffset + i] = 1. / temp;
      else
        data_err[t4phioffset + i] = 0.;
    }

  free(v2);
  free(v2_sig);
  free(t3phi);
  free(t3phi_sig);
  free(t3amp);
  free(t3amp_sig);

  free(t4phi);
  free(t4phi_sig);
  free(t4amp);
  free(t4amp_sig);

  free(visamp);
  free(visamp_sig);
  free(visphi);
  free(visphi_sig);

  free(time_vis);
  free(time_v2);
  free(time_t3);
  free(time_t4);

  free(lambda_vis);
  free(lambda_v2);
  free(lambda_t3);
  free(lambda_t4);

  free(dlambda_vis);
  free(dlambda_v2);
  free(dlambda_t3);
  free(dlambda_t4);

  free(flag_t3);
  free(flag_t4);
  free(flag_vis);
  free(flag_v2);

  return (status); /* zero means ok */
}
/*
void add_new_uv(long *obs_index, long *uvindex, double new_u, double new_v, double new_uv_lambda, double new_uv_dlambda, double new_uv_time, double *table_u,
                double *table_v, double *table_uv_lambda, double *table_uv_dlambda, double *table_uv_time, double uvtol)
{
  // Check previous uv points for redundancy, and only create a new uv point if needed

  // First check for redundancy
  long i;
  long redundant_index = -1;
  const double tol = uvtol * uvtol;

#ifdef _OPENMP
#pragma omp parallel
#pragma omp for
#endif
  for (i = 0; i < *uvindex; i++)
    {
      // Need to be clever here, as this is a major source of slowdown for very large datasets

      if ((table_u[i] - new_u) * (table_u[i] - new_u) + (table_v[i] - new_v) * (table_v[i] - new_v) < tol)
        if (fabs((table_uv_lambda[i] - new_uv_lambda) / table_uv_lambda[i]) < 1e-6)
          {
            redundant_index = i;
#ifdef _OPENMP
#pragma omp cancel for
#else
            break;
#endif
          }
    }

  if (redundant_index == -1) // the current point is not redundant with a previous one
    {
      table_u[*uvindex] = new_u;
      table_v[*uvindex] = new_v;
      table_uv_time[*uvindex] = new_uv_time;
      table_uv_lambda[*uvindex] = new_uv_lambda;
      table_uv_dlambda[*uvindex] = new_uv_dlambda;
      *obs_index = *uvindex;
      (*uvindex)++;
    }
  else
    {
      *obs_index = redundant_index; // redundant, refer to the uv point with which the new uv point is redundant
    }
}
*/

void add_new_uv(long *obs_index, long *uvindex, double new_u, double new_v, double new_uv_lambda, double new_uv_dlambda, double new_uv_time, double *table_u,
                double *table_v, double *table_uv_lambda, double *table_uv_dlambda, double *table_uv_time, double uvtol)
{
  // Check previous uv points for redundancy, and only create a new uv point if needed
    table_u[*uvindex] = new_u;
      table_v[*uvindex] = new_v;
      table_uv_time[*uvindex] = new_uv_time;
      table_uv_lambda[*uvindex] = new_uv_lambda;
      table_uv_dlambda[*uvindex] = new_uv_dlambda;
      *obs_index = *uvindex;
      (*uvindex)++;
}















int write_best_oifits(char *filestring, double complex *mod_vis)
{
  FILE *fileptr = NULL;
  char filename[80] = "output.bispectrum";
  long i;
  double complex modt3;
  if (filestring[0] != 0)
    strcpy(filename, filestring);
  if ((fileptr = fopen(filename, "w")) == NULL)
    return -1;
  for (i = 0; i < nt3; i++)
    {
      modt3 = mod_vis[t3in1[i]] * mod_vis[t3in2[i]] * conj(mod_vis[t3in3[i]]);
      fprintf(fileptr, "%10.7lf %10.7lf\n", cabs(modt3), carg(modt3));
    }
  for (i = 0; i < nv2; i++)
    {
      fprintf(fileptr, "%10.7lf %10.7lf\n", creal(mod_vis[v2in[i]]) * creal(mod_vis[v2in[i]]) + cimag(mod_vis[v2in[i]]) * cimag(mod_vis[v2in[i]]), 0.0);
    }
  for (i = 0; i < nvis; i++)
    {
      fprintf(fileptr, "%10.7lf %10.7lf\n", cabs(mod_vis[visin[i]]), carg(mod_vis[visin[i]]));
    }
  fclose(fileptr);
  return 0;
}
