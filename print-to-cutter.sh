#!/bin/sh
#
# PRINT TO CUTTER
# ---------------
#
# A small shell utility that can be used as a front-end
#	to pstoedit and hpgl-distiller to allow your *nix 
#	applications to print directly to the vinly cutter
#	so long as they generate valid Postscript or EPS output
#
# Written by Paul L Daniels
#
# http://pldaniels.com
# http://pldaniels.com/hpgl-distiller
#
#

#
# HOW TO USE THIS SCRIPT
# ----------------------
#
# (eg, Inkscape).
# Most *nix programs let you print to a program via a pipe
#	(certainly Inkscape does), what you do is tell the software
#	to print to this script, it's that simple.  The script
#	will then dump the Postscript data to /tmp, process it
#	filter it and then finally send the distilled HPGL to the
#	cutter/plotter.
#
#

# Which device has the cutter/plotter attached 
#	
CUTTER=/dev/ttyS1

TMPDIR=/tmp
INF=$TMPDIR/cutter.$$.eps
OUTF=$TMPDIR/cutter.$$.tmp
HPGL=$TMPDIR/cutter.$$.hpgl
REMOVE_INF=0

# If we have a parameter for this script,
#	then it will be the filename of the file
#	we want to have converted (ie, we're not
#	reading from STDIN
#
if [ $# -eq 1 ]; then
	# Input data comes from a file.
#
	INF=$1
	else

	# Input is coming via a pipe to STDIN
#
	cat > $INF
	REMOVE_INF=1
fi

# Convert the Postscript/SVG/DXF/PDF to full HPGL/2
#
pstoedit -f plot-hpgl $INF $OUTF

# Strip out the innapropriate HPGL commands so as 
#	not to confuse our plotter/cutter
#
hpgl-distiller -i $OUTF -o $HPGL

# Send the data to the cutter.
#
cat $HPGL > $CUTTER

# Clean up the temporary files generated
#
rm $OUTF $HPGL
if [ $REMOVE_INF -eq 1 ]; then
	rm $INF
	fi

# END OF SCRIPT

