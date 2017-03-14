/*
 *  SimInf, a framework for stochastic disease spread simulations
 *  Copyright (C) 2015  Pavol Bauer
 *  Copyright (C) 2015 - 2017  Stefan Engblom
 *  Copyright (C) 2015 - 2017  Stefan Widgren
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Rdefines.h>
#include <R_ext/Rdynload.h>
#include <R_ext/Visibility.h>
#include "SimInf.h"

/* Declare functions to register */
SEXP SEIR_run(SEXP, SEXP, SEXP);
SEXP SIR_run(SEXP, SEXP, SEXP);
SEXP SISe_run(SEXP, SEXP, SEXP);
SEXP SISe3_run(SEXP, SEXP, SEXP);
SEXP SISe3_sp_run(SEXP, SEXP, SEXP);
SEXP SISe_sp_run(SEXP, SEXP, SEXP);
SEXP siminf_ldata_sp(SEXP, SEXP, SEXP);
SEXP siminf_have_openmp();

static const R_CallMethodDef callMethods[] =
{
    {"SEIR_run", (DL_FUNC) &SEIR_run, 3},
    {"SIR_run", (DL_FUNC) &SIR_run, 3},
    {"SISe_run", (DL_FUNC) &SISe_run, 3},
    {"SISe3_run", (DL_FUNC) &SISe3_run, 3},
    {"SISe3_sp_run", (DL_FUNC) &SISe3_sp_run, 3},
    {"SISe_sp_run", (DL_FUNC) &SISe_sp_run, 3},
    {"siminf_have_openmp", (DL_FUNC) &siminf_have_openmp, 0},
    {"siminf_ldata_sp", (DL_FUNC) &siminf_ldata_sp, 3},
    {NULL, NULL, 0}
};

void attribute_visible
R_init_SimInf(DllInfo *info)
{
    R_registerRoutines(info, NULL, callMethods, NULL, NULL);
    R_useDynamicSymbols(info, FALSE);
    R_forceSymbols(info, FALSE);
    R_RegisterCCallable("SimInf", "SimInf_run", (DL_FUNC) &SimInf_run);
}
