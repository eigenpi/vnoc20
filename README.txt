cristinel.ababei@marquette.edu
February 2014, Milwaukee WI


Synopsis
========

This is the VNOC 2.0 tool. VNOC is a versatile NOC simulator developed
on top of VNOC 1.0, which in turn was developed starting from "popnet". 

Main features:
-- It's mainly C++ code; clean and with lots of comments for you to
   be able to read/understand it easily. It's pretty fast.
-- It has a simple GUI. This GUI is a port/adaptation of the GUI of VPR tool.
   Note that if you run it with the GUI enabled the runtime is longer.
-- It can simulate several types of traffic, including: 
   uniform random, transpose, hotspot, and selfsimilar. 
   The selfsimilar traffic generation is done using the generator developed
   by Glen Kramer.
-- It has integrated Orion 2 and Orion 3 power models (both under the
   folder orion3/ because Orion 3 inlcudes Orion 2). So, it reports
   power consumption estimates too at the end of the simulation.
   Note however, that I use only Orion 2 because it's designed so that
   we can use it with activities at the level of each router in the NOC.
   This basically captures the effect of the actual traffic on power.
   Orion 3 only estimates the power of a single router, in isolation,
   and with activities at the "inputs" of the router assumed random and 
   uniform. Therefore, we cannot really use it for a whole NOC simulation.
   Orion 3 needs to be extended to be possible to be integrated
   like how people could do it with Orion 1 and Orion 2...
-- It has integrated frequency throttle and frequency boost capability
   to implement DVFS (dynamic voltage and frequency scaling) techniques.
   These techniques are studied and reported in this paper:
   [1] C. Ababei and N. Mastronarde, "Benefits and costs of prediction 
   based DVFS for NoCs at router level," SOCC 2014, Las Vegas NV, Sep. 2014.
   This DVFS "infrastructure" is easy to understand and should
   allow you to implement and study your own DVFS ideas...


Installation
============

VNOC 2.0 was developed on Linux Ubuntu 12.04 LTS (64 bit)

Before you compile and link, you need to install a few prerequisites
(required mainly by the GUI/fonts and the selfsimilar traffic generator).
In a terminal, do:
> sudo apt-get install ia32-libs 
> sudo apt-get install libx11-dev:i386
> sudo apt-get install gcc-multilib g++-multilib libxxf86vm-dev libglu1-mesa-dev libxft-dev

Then, also do (or edit your .bashrc accordingly directly):
> export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/lib32:/usr/lib32:/usr/lib/i386-linux-gnu 

To compile and link VNOC 2.0, expand the downloaded archive and:
1) Edit Makefile to reflect the location where you want to compile and link
2) In a terminal, type "make" inside vnoc2/

If everything went fine, you should see the executable "vnoc" created.


How to use vnoc
===============

Type "vnoc" at the command prompt to see the command-line arguments.

For examples of how to run vnoc, see the scripts under run_scripts/ folder.

Here are a few examples:

vnoc traffic: UNIFORM injection_rate: 0.015 do_dvfs: 0
vnoc traffic: UNIFORM injection_rate: 0.015 do_dvfs: 0 use_gui
vnoc traffic: SELFSIMILAR ary_size: 8 injection_rate: 0.010 cycles: 100000 do_dvfs: 0
vnoc traffic: SELFSIMILAR ary_size: 8 injection_rate: 0.010 cycles: 100000 do_dvfs: 1 use_boost: 0 hist_window: 50
vnoc traffic: SELFSIMILAR ary_size: 8 injection_rate: 0.010 cycles: 100000 do_dvfs: 1 use_boost: 1 hist_window: 50


Additional notes
================

This is regarding the use of the Orion 2.0 power models.
Currently, the default values for the input and output buffer
sizes of the actual NOC simulator are set such that they match
those from the source code of Orion 2.0.
If you change - thru the available arguments of "vnoc" the buffer
sizes - then, the power estimations reported at the end will most
likely reflect values that are innacurate...

If you want to run the simulator "vnoc" for different buffer sizes,
you should first, MANUALLY, go and edit orion3/SIM_port.h to reflect
the new buffer sizes and then recompile the Orion 2 power model
as well as recompile "vnoc". You can do just by: change SIM_port.h,
then delete all object files .o from vnoc2/ and from vnoc2/orion3/,
then finally type make inside vnoc2/. This will compile everything
and recreate the power model as well as "vnoc" executable.

TODO:
This whole shebang of doing things with Orion power models should be 
automated, so that user does not go thru this pain of changing
source code and then recompiling... It's doable but takes time...


Even more notes
===============

If you use vnoc in your research and would like to provide a citation,
please use this reference:
[1] C. Ababei and N. Mastronarde, "Benefits and costs of prediction 
    based DVFS for NoCs at router level," SOCC 2014, Las Vegas NV, Sep. 2014.

If you find any bug or have any suggestions please let me know. Thank you!


Credits are due to
==================

-- Li Shang (while at Princeton) developed "popnet" from which I 
   developed VNOC 1.0. Original source code of popnet is not available
   online anymore...
-- Vaughn Betz (of Univ. of Toronto) developed much of the GUI
   as part of his famous VPR tool:
   http://www.eecg.toronto.edu/~vaughn/vpr/vpr.html
-- Glen Kramer for the Generator of Self-Similar Network Traffic (version 2);
   http://glenkramer.com/ucdavis/code/trf_gen2.html


Copyright
=========
Copyright 2014 by Cristinel (Cris) Ababei, cristinel.ababei@marquette.edu
This Copyright notice applies to all files, called hereafter 
"The Software".
Permission to use, copy, and modify this software and its 
documentation is hereby granted only under the following 
terms and conditions.  Both the above copyright notice and 
this permission notice must appear in all copies of the 
software, derivative works or modified versions, and any 
portions thereof, and both notices must appear in supporting 
documentation.  Permission is granted only for non-commercial 
use.  For commercial use, please contact the author.
This software may be distributed (but not offered for sale 
or transferred for compensation) to third parties, provided 
such third parties agree to abide by the terms and conditions
of this notice.
The Software is provided "as is", and the authors, 
Marquette University, as well as any and all previous 
authors (of portions or modified portions of the software) 
disclaim all warranties with regard to this software, 
including all implied warranties of merchantability
and fitness.  In no event shall the authors or Marquette University 
or any and all previous authors be liable for any special, direct, 
indirect, or consequential damages or any damages whatsoever
resulting from loss of use, data or profits, whether in an
action of contract, negligence or other tortious action,
arising out of or in connection with the use or performance
of this software.
