/**
  * hpgl-distiller HPGL Distiller
  *
  * Written and maintained by Paul L Daniels [ pldaniels at pldaniels com ]
  *
  * This purpose of this program is to strip out non-applicable HPGL commands
  * of which may confuse various plotters/cutters.
  *
  * Original version written: Wednesday December 27th, 2006
  *
  *
  * Process description:
  *
  *	Generating distillered HPGL from a SVG/DXF/PS/EPS/(all formats supported by pstoedit)
  *	is a multistep process at this stage.
  *
  *	1. Generate the full HPGL output via pstoedit
  *			pstoedit -f plot-hpgl somefile.eps output.hpgl
  *
  *	2. Distill the full HPGL
  *			hpgl-distiller -i output.hpgl -o distillered.hpgl
  *
  *	3. Send the HPGL to the printer
  *			cat distillered.hpgl > /dev/ttyS1   (for a serial port cutter)
  *
  **/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>

#define HPGLD_VERSION "0.9.1"
#define HPGLD_DEFAULT_INIT_STRING "IN;PU;" // Initialize and Pen-up
char HPGLD_HELP[]="hpgl-distiller: HPGL Distiller (for vinyl cutters)\n"
	"Written by Paul L Daniels.\n"
	"Distributed under the BSD Revised licence\n"
	"This software is available at http://pldaniels.com/hpgl-distiller\n"
	"\n"
	"Usage: hpgl-distiller -i <input HPGL> -o <output file> [-v] [-d] [-I <initialization string>] [-h]\n"
	"\n"
	"\t-i <input HPGL> : Specifies which file contains the full HPGL file that is to be distilled.\n"
	"\t-o <output file> : specifies which file the distilled HPGL is to be saved to.\n"
	"\t-I <initialization string> : Specifies a HPGL sequence to be prepended to the output file.\n"
	"\t-s <slew time in mS>: Specifies how long to wait between commands.\n"
	"\n"
	"\t-b : Determine bounding box and normalise to origin (use in conjunction with -x -y if desired)\n"
	"\t-x <offset>: Apply this offset to all X values.\n"
	"\t-y <offset>: Apply this offset to all y values.\n"
	"\n"
	"\t-v : Display current software version\n"
	"\t-d : Enable debugging output (verbose)\n"
	"\t-h : Display this help.\n"
	"\n";

struct hpgld_glb {
	int status;
	int debug;
	int slew;

	int find_bb;
	int bb_xo;
	int bb_yo;
	int bb_width;
	int bb_height;

	int xoffset;
	int yoffset;


	char *init_string;
	char *input_filename;
	char *output_filename;
};

/**
  * HPGLD_show_version
  *
  * Display the current HPGL-Distiller version
  */
int HPGLD_show_version( struct hpgld_glb *glb ) {
	fprintf(stderr,"%s\n", HPGLD_VERSION);
	return 0;
}

/**
  * HPGLD_show_help
  *
  * Display the help data for this program
  */
int HPGLD_show_help( struct hpgld_glb *glb ) {
	HPGLD_show_version(glb);
	fprintf(stderr,"%s\n", HPGLD_HELP);

	return 0;

}

/**
  * HPGLD_init
  *
  * Initializes any variables or such as required by
  * the program.
  */
int HPGLD_init( struct hpgld_glb *glb )
{
	glb->status = 0;
	glb->debug = 0;
	glb->slew = 0;
	glb->init_string = HPGLD_DEFAULT_INIT_STRING;
	glb->input_filename = NULL;
	glb->output_filename = NULL;

	glb->xoffset = 0;
	glb->yoffset = 0;
	glb->bb_xo = 0;
	glb->bb_yo = 0;
	glb->bb_width = -1;
	glb->bb_height = -1;

	return 0;
}

/**
  * HPGLD_parse_parameters
  *
  * Parses the command line parameters and sets the
  * various HPGL-Distiller settings accordingly
  */
int HPGLD_parse_parameters( int argc, char **argv, struct hpgld_glb *glb )
{

	char c;

	do {
		c = getopt(argc, argv, "I:i:o:dvh");
		switch (c) { 
			case EOF: /* finished */
				break;

			case 'i':
				glb->input_filename = strdup(optarg);
				break;

			case 'o':
				glb->output_filename = strdup(optarg);
				break;

			case 'I':
				glb->init_string = strdup(optarg);
				break;

			case 's':
				glb->slew = atoi(optarg) *1000;
				break;

			case 'b':
				glb->find_bb = 1;
				break;

			case 'x':
				glb->xoffset = atoi(optarg);
				break;

			case 'y':
				glb->yoffset = atoi(optarg);
				break;

			case 'd':
				glb->debug = 1;
				break;

			case 'h':
				HPGLD_show_help(glb);
				exit(1);
				break;

			case 'v':
				HPGLD_show_version(glb);
				exit(1);
				break;

			case '?':
				break;

			default:
				fprintf(stderr, "internal error with getopt\n");
				exit(1);
				
		} /* switch */
	} while (c != EOF); /* do */
	return 0;
}



/**
  * HPGL-Distiller,  main body
  *
  * This program inputs an existing HPGL file which
  * may contain many spurilous commands that aren't relevant
  * to the device that the HPGL is being sent to.
  *
  * One such device is a vinyl cutter.  There's no 
  * relevance for commands like Pen-width, line type
  * etc, it's all simple Pen-down and move cuts.
  *
  */
int main(int argc, char **argv) {

	struct hpgld_glb glb;
	FILE *fi, *fo;
	char *data, *p;
	struct stat st;
	int stat_result;
	size_t file_size;
	size_t read_size;

	long int px, py, ox, oy;

	ox = oy = 0;



	/* Initialize our global data structure */
	HPGLD_init(&glb);


	/* Decyper the command line parameters provided */
	HPGLD_parse_parameters(argc, argv, &glb);

	/* Check the fundamental sanity of the variables in
		the global data structure to ensure that we can
		actually perform some meaningful work
		*/
	if ((glb.input_filename == NULL)) {
		fprintf(stderr,"Error: Input filename is NULL.\n");
		exit(1);
	}
	if ((glb.output_filename == NULL)) {
		fprintf(stderr,"Error: Output filename is NULL.\n");
		exit(1);
	}

	/* Check to see if we can get information about the
		input file. Specifically we'll be wanting the 
		file size so that we can allocate enough memory
		to read in the whole file at once (not the best
		method but it's certainly a lot easier than trying
		to read in line at a time when the file can potentially
		contain non-ASCII chars 
		*/
	stat_result = stat(glb.input_filename, &st);
	if (stat_result) {
		fprintf(stderr,"Cannot stat '%s' (%s)\n", argv[1], strerror(errno));
		return 1;
	}

	/* Get the filesize and attempt to allocate a block of memory
		to hold the entire file
		*/
	file_size = st.st_size;
	data = malloc(sizeof(char) *(file_size +1));
	if (data == NULL) {
		fprintf(stderr,"Cannot allocate enough memory to read input HPGL file '%s' of size %d bytes (%s)\n", glb.input_filename, file_size, strerror(errno));
		return 2;
	}

	/* Attempt to open the input file as read-only 
	 */
	fi = fopen(glb.input_filename,"r");
	if (!fi) {
		fprintf(stderr,"Cannot open input file '%s' for reading (%s)\n", glb.input_filename, strerror(errno));
		return 3;
	}

	/* Attempt to open the output file in write mode
		(no appending is done, any existing file will be
		overwritten
		*/
	fo = fopen(glb.output_filename,"w");
	if (!fo) {
		fprintf(stderr,"Cannot open input file '%s' for writing (%s)\n", glb.output_filename, strerror(errno));
		return 4;
	}

	/* Read in the entire input file .
		Compare the size read with the size
		of the file as a double check.
		*/
	read_size = fread(data, sizeof(char), file_size, fi);
	fclose(fi);
	if (read_size != file_size) {
		fprintf(stderr,"Error, the file size (%d bytes) and the size of the data read (%d bytes) do not match.\n", file_size, read_size);
		return 5;
	}


	/* Put a terminating '\0' at the end of the data so
		that our string tokenizing functions won't overrun
		into no-mans-land and cause segfaults
		*/
	data[file_size] = '\0';

	/* Generate any preliminary intializations to the output
		file first.
		*/
	fprintf(fo,"%s\n", glb.init_string);

	/* Go through the entire data block read from the
		input file and break it off one piece at a time
		as delimeted/tokenized by ;, \r or \n characters.
		*/
	p = strtok(data, ";\n\r");
	while (p) {

		if (glb.debug) printf("in: %s  ",p);

		/* Compare the segment of data obtained
			from the tokenizing process to the list
			of 'valid' HPGL commands that we want to
			accept.  Anything not in this list we 
			simply discard.
			*/
		if (
				(strncmp(p,"IN",2)==0)		// Initialize
				||(strncmp(p,"PA",2)==0)	// Plot Absolute
				||(strncmp(p,"PD",2)==0)	// Pen Down
				||(strncmp(p,"PU",2)==0)	// Pen Up
				||(strncmp(p,"PG",2)==0)	// Page Feed
//				||(strncmp(p,"PM",2)==0)
				||(strncmp(p,"PR",2)==0)	// Plot Relative
				||(strncmp(p,"!PG",3)==0)
			) {

			/* Write the token to the output file */
			fprintf(fo,"%s;\n",p);
			fflush(fo);

			if (glb.slew) {
				if (strchr(p,',')) {
					int r;
					long int distance;
					/** we have some digits in the string, so let's extract the coords **/
					r = sscanf(p, "%ld,%ld", &px, &py);
					if (r == 2) {
						distance = sqrt((( px - ox ) * ( px -ox )) + (( py -oy) * (py -oy)));
						fprintf(stderr,"Distance: %ld\n", distance);
						usleep(distance *glb.slew);
						ox = px;
					  	oy = py;
					} else {
						usleep(10 *glb.slew);
					}
				}
			}

			if (glb.debug) printf("good\n");
		} else {


			if (glb.debug) printf("ignored\n");
		} /* if strncmp... */

		/* Get the next block of data from the input data
			*/
		p = strtok(NULL,";\n\r");


	} /** while more tokenizing **/

	/* Close the output file */
	fclose(fo);

	/* Release the memory we allocated to hold the input HPGL file */
	free(data);

	return 0;
}

/** END **/


