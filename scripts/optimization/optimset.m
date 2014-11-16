## Copyright (C) 2007-2013 John W. Eaton
## Copyright (C) 2009 VZLU Prague
##
## This file is part of Octave.
##
## Octave is free software; you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 3 of the License, or (at
## your option) any later version.
##
## Octave is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with Octave; see the file COPYING.  If not, see
## <http://www.gnu.org/licenses/>.

## -*- texinfo -*-
## @deftypefn  {Function File} {} optimset ()
## @deftypefnx {Function File} {} optimset (@var{par}, @var{val}, @dots{})
## @deftypefnx {Function File} {} optimset (@var{old}, @var{par}, @var{val}, @dots{})
## @deftypefnx {Function File} {} optimset (@var{old}, @var{new})
## Create options struct for optimization functions.
##
## Valid parameters are:
##
## @table @asis
## @item AutoScaling
##
## @item ComplexEqn
##
## @item Display
## Request verbose display of results from optimizations.  Values are:
##
## @table @asis
## @item @qcode{"off"} [default]
## No display.
##
## @item @qcode{"iter"}
## Display intermediate results for every loop iteration.
##
## @item @qcode{"final"}
## Display the result of the final loop iteration.
##
## @item @qcode{"notify"}
## Display the result of the final loop iteration if the function has
## failed to converge.
## @end table
##
## @item FinDiffType
##
## @item FunValCheck
## When enabled, display an error if the objective function returns an invalid
## value (a complex number, NaN, or Inf).  Must be set to @qcode{"on"} or
## @qcode{"off"} [default].  Note: the functions @code{fzero} and
## @code{fminbnd} correctly handle Inf values and only complex values or NaN
## will cause an error in this case. 
##
## @item GradObj
## When set to @qcode{"on"}, the function to be minimized must return a
## second argument which is the gradient, or first derivative, of the
## function at the point @var{x}.  If set to @qcode{"off"} [default], the
## gradient is computed via finite differences.
##
## @item Jacobian
## When set to @qcode{"on"}, the function to be minimized must return a
## second argument which is the Jacobian, or first derivative, of the
## function at the point @var{x}.  If set to @qcode{"off"} [default], the
## Jacobian is computed via finite differences.
##
## @item MaxFunEvals
## Maximum number of function evaluations before optimization stops.
## Must be a positive integer.
##
## @item MaxIter
## Maximum number of algorithm iterations before optimization stops.
## Must be a positive integer.
##
## @item OutputFcn
## A user-defined function executed once per algorithm iteration.
##
## @item TolFun
## Termination criterion for the function output.  If the difference in the
## calculated objective function between one algorithm iteration and the next
## is less than @code{TolFun} the optimization stops.  Must be a positive
## scalar.
##
## @item TolX
## Termination criterion for the function input.  If the difference in @var{x},
## the current search point, between one algorithm iteration and the next is
## less than @code{TolX} the optimization stops.  Must be a positive scalar.
##
## @item TypicalX
##
## @item Updating
## @end table
## @end deftypefn

function retval = optimset (varargin)

  nargs = nargin ();

  ## Add more as needed.
  opts = __all_opts__ ();

  if (nargs == 0)
    if (nargout == 0)
      ## Display possibilities.
      puts ("\nAll possible optimization options:\n\n");
      printf ("  %s\n", opts{:});
      puts ("\n");
    else
      ## Return struct with all options initialized to []
      retval = cell2struct (repmat ({[]}, size (opts)), opts, 2);
    endif
  elseif (nargs == 1 && ischar (varargin{1}))
    ## Return defaults for named function.
    fcn = varargin{1};
    try
      retval = feval (fcn, "defaults");
    catch
      error ("optimset: no defaults for function '%s'", fcn);
    end_try_catch
  elseif (nargs == 2 && isstruct (varargin{1}) && isstruct (varargin{2}))
    ## Set slots in old from nonempties in new.  Should we be checking
    ## to ensure that the field names are expected?
    old = varargin{1};
    new = varargin{2};
    fnames = fieldnames (old);
    ## skip validation if we're in the internal query
    validation = ! isempty (opts);
    for [val, key] = new
      if (validation)
        ## Case insensitive lookup in all options.
        i = strncmpi (opts, key, length (key));
        nmatch = sum (i);
        ## Validate option.
        if (nmatch == 1)
          key = opts{find (i)};
        elseif (nmatch == 0)
          warning ("unrecognized option: %s", key);
        else
          fmt = sprintf ("ambiguous option: %%s (%s%%s)",
                         repmat ("%s, ", 1, nmatch-1));
          warning (fmt, key, opts{i});
        endif
      endif
      old.(key) = val;
    endfor
    retval = old;
  elseif (rem (nargs, 2) && isstruct (varargin{1}))
    ## Set values in old from name/value pairs.
    pairs = reshape (varargin(2:end), 2, []);
    retval = optimset (varargin{1}, cell2struct (pairs(2, :), pairs(1, :), 2));
  elseif (rem (nargs, 2) == 0)
    ## Create struct.  Default values are replaced by those specified by
    ## name/value pairs.
    pairs = reshape (varargin, 2, []);
    retval = optimset (struct (), cell2struct (pairs(2, :), pairs(1, :), 2));
  else
    print_usage ();
  endif

endfunction


%!assert (optimget (optimset ("tolx", 1e-2), "tOLx"), 1e-2)
%!assert (isfield (optimset ("tolFun", 1e-3), "TolFun"))
%!warning (optimset ("Max", 10));
%!warning (optimset ("foobar", 13));

%!error (optimset ("%NOT_A_REAL_FUNCTION_NAME%"))

