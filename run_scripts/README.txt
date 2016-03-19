vnoc is just a local copy of the vnoc executable, which is
used to run the following scripts to collect simulation data
in files saved under /vnoc2/results/

-- case: base
> perl run_self_base.scr

-- case: freq. throttle yes, frequency boost no
> perl run_self_hw_50.scr

-- case: freq. throttle yes, frequency boost yes
> perl run_self_boost_hw_50.scr
