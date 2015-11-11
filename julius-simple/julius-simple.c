#include <julius/juliuslib.h>
static char *infilename = "";
/** 
 * Callback to output final recognition result.
 * This function will be called just after recognition of an input ends
 * 
 */
static void
output_result(Recog *recog, void *dummy)
{
  WORD_INFO *winfo;
  WORD_ID *seq;
  int seqnum;
  int n, num;
  Sentence *s;
  RecogProcess *r;

  char buf[4096];
  int pos;

  // constructs the JSON representation of the result data.
  pos = 0;
  pos += sprintf(&buf[pos], "{\"processes\":[");
  for(r=recog->process_list;r;r=r->next) {
    if (!r->live) continue;
    if (r->config->successive.enabled && r->result.status < 0 && r->config->output.progout_flag) continue;
    if (r->result.status < 0) continue;
    pos += sprintf(&buf[pos], "{\"sentences\":[");
    winfo = r->lm->winfo;
    num = r->result.sentnum;

    for(n=0;n<num;n++) {
      if (r->config->successive.enabled && r->config->output.progout_flag) break;
      pos += sprintf(&buf[pos], "[");
      s = &(r->result.sent[n]);
      seq = s->word;
      seqnum = s->word_num;
      
      int i;
      for (i = 0; i < seqnum; ++i) {
        pos += sprintf(&buf[pos], "{");
        pos += sprintf(&buf[pos], "\"word\":");
        if (seq != NULL) {
          pos += sprintf(&buf[pos], "\"%s\"", winfo->woutput[seq[i]]);
        } else {
          pos += sprintf(&buf[pos], "\"null\"");
        }
#ifdef CONFIDENCE_MEASURE
#ifndef CM_MULTIPLE_ALPHA
        pos += sprintf(&buf[pos], ",");
        pos += sprintf(&buf[pos], "\"confidence\":");
        if (s->confidence != NULL) {
          pos += sprintf(&buf[pos], "\"%5.3f\"", s->confidence[i]);
        } else {
          pos += sprintf(&buf[pos], "\"null\"");
        }
        pos += sprintf(&buf[pos], "}");
#endif
#endif
        if (i != seqnum - 1) {
          pos += sprintf(&buf[pos], ",");
        }
      }
      pos += sprintf(&buf[pos], "]");
    }
    pos += sprintf(&buf[pos], "]}");
  }
  pos += sprintf(&buf[pos], "]}");
  buf[pos] = '\0';

  // writes the data to the socket.
  printf("%s\n", buf);
  fflush(stdout);
}

static boolean
opt_infile(Jconf *jconf, char *arg[], int argnum)
{
  infilename = arg[0];
  printf("Frank: input filename: %s\n", infilename);
  return TRUE;
}
/**
 * Main function
 * 
 */
int
main(int argc, char *argv[])
{
  /**
   * configuration parameter holder
   * 
   */
  Jconf *jconf;

  /**
   * Recognition instance
   * 
   */
  Recog *recog;

  /**
   * speech file name for MFCC file input
   * 
   */
  static char speechfilename[MAXPATHLEN];

  int ret;

  /* by default, all messages will be output to standard out */
  /* to disable output, uncomment below */
  //jlog_set_output(NULL);

  /* output log to a file */
  //FILE *fp; fp = fopen("log.txt", "w"); jlog_set_output(fp);

  /* if no argument, output usage and exit */
  if (argc == 1) {
    fprintf(stderr, "Julius rev.%s - based on ", JULIUS_VERSION);
    j_put_version(stderr);
    fprintf(stderr, "Try '-setting' for built-in engine configuration.\n");
    fprintf(stderr, "Try '-help' for run time options.\n");
    return -1;
  }

  j_add_option("-infile", 1, 1, "input sound file", opt_infile);

  /************/
  /* Start up */
  /************/
  /* 1. load configurations from command arguments */
  jconf = j_config_load_args_new(argc, argv);
  /* else, you can load configurations from a jconf file */
  //jconf = j_config_load_file_new(jconf_filename);
  if (jconf == NULL) {		/* error */
    fprintf(stderr, "Try `-help' for more information.\n");
    return -1;
  }
  
  /* 2. create recognition instance according to the jconf */
  /* it loads models, setup final parameters, build lexicon
     and set up work area for recognition */
  recog = j_create_instance_from_jconf(jconf);
  if (recog == NULL) {
    fprintf(stderr, "Error in startup\n");
    return -1;
  }

  /*********************/
  /* Register callback */
  /*********************/
  /* register result callback functions */
  callback_add(recog, CALLBACK_RESULT, output_result, NULL);

  /**************************/
  /* Initialize audio input */
  /**************************/
  /* initialize audio input device */
  /* ad-in thread starts at this time for microphone */
  if (j_adin_init(recog) == FALSE) {    /* error */
    return -1;
  }

  /* output system information to log */
  j_recog_info(recog);

  /***********************************/
  /* Open input stream and recognize */
  /***********************************/

  if (jconf->input.speech_input == SP_MFCFILE || jconf->input.speech_input == SP_OUTPROBFILE) {
    /* MFCC file input */

    while (get_line_from_stdin(speechfilename, MAXPATHLEN, "enter MFCC filename->") != NULL) {
      if (verbose_flag) printf("\ninput MFCC file: %s\n", speechfilename);
      /* open the input file */
      ret = j_open_stream(recog, speechfilename);
      switch(ret) {
      case 0:			/* succeeded */
	break;
      case -1:      		/* error */
	/* go on to the next input */
	continue;
      case -2:			/* end of recognition */
	return;
      }
      /* recognition loop */
      ret = j_recognize_stream(recog);
      if (ret == -1) return -1;	/* error */
      /* reach here when an input ends */
    }

  } else {
    /* raw speech input (microphone, stdin, file etc.) */
    char *input_file = NULL;
    if (infilename[0] != '\0') {
      input_file = infilename;
    }
    switch(j_open_stream(recog, input_file)) {
    case 0:			/* succeeded */
      break;
    case -1:      		/* error */
      fprintf(stderr, "error in input stream\n");
      return;
    case -2:			/* end of recognition process */
      fprintf(stderr, "failed to begin input stream\n");
      return;
    }
    
    /**********************/
    /* Recognization Loop */
    /**********************/
    /* enter main loop to recognize the input stream */
    /* finish after whole input has been processed and input reaches end */
    ret = j_recognize_stream(recog);
    if (ret == -1) return -1;	/* error */
    
    /*******/
    /* End */
    /*******/
  }

  /* calling j_close_stream(recog) at any time will terminate
     recognition and exit j_recognize_stream() */
  j_close_stream(recog);

  j_recog_free(recog);

  /* exit program */
  return(0);
}
