// Xuefei Li @ 2015/11/02 5:00PM EST.
// RSVP Technologies Inc.

#include "app.h"

// events
#define SPEECHREADY     ("SPEECHREADY ")
#define SPEECHSTART     ("SPEECHSTART ")
#define SPEECHSTOP      ("SPEECHSTOP ")
#define ENGINEACTIVE    ("ENGINEACTIVE ")
#define ENGINEINACTIVE  ("ENGINEINACTIVE ")
#define ENGINEPAUSE     ("ENGINEPAUSE ")
#define ENGINERESUME    ("ENGINERESUME ")
#define RESULTSTART     ("RESULTSTART ")
#define RESULTEND       ("RESULTEND ")

// Referenced from adin_tcpip.c
extern int adinnet_asd;
// Referenced from main.c
extern boolean separate_score_flag;

static boolean have_progout = FALSE;
static int adinnet_out = FALSE;

/* for short pause segmentation and successive decoding */
static WORD_ID confword[MAXSEQNUM];
static int confwordnum;

  /**
   * Event callback to be called when the engine becomes active and
   * start running.  (ex. resume by j_request_resume())
   * 
   */
static void
event_process_online(Recog *recog, void *dummy)
{
  wt_raw(adinnet_asd, ENGINEACTIVE, strlen(ENGINEACTIVE));
}

  /**
   * Event callback to be called when the engine becomes inactive and
   * stop running.  (ex. pause or terminate by user request)
   * 
   */
static void
event_process_offline(Recog *recog, void *dummy)
{
  wt_raw(adinnet_asd, ENGINEINACTIVE, strlen(ENGINEINACTIVE));
}

  /**
   * Event callback to be called when engine is ready for recognition
   * and start listening to the audio input.
   * 
   */
static void
event_speech_ready(Recog *recog, void *dummy)
{
  wt_raw(adinnet_asd, SPEECHREADY, strlen(SPEECHREADY));
}

  /**
   * Event callback to be called when input speech processing starts.
   * This will be called at speech up-trigger detection by level and
   * zerocross.  When the detection is disabled (i.e. file input),
   * This will be called immediately after opening the file.
   * 
   */
static void
event_speech_start(Recog *recog, void *dummy)
{
  wt_raw(adinnet_asd, SPEECHSTART, strlen(SPEECHSTART));
}

  /**
   * Event callback to be called when input speech ends.  This will be
   * called at speech down-trigger detection by level and zerocross.
   * When the detection is disabled (i.e. file input), this will be called
   * just after the whole input has been read.
   * 
   */
static void
event_speech_stop(Recog *recog, void *dummy)
{
  wt_raw(adinnet_asd, SPEECHSTOP, strlen(SPEECHSTOP));
}

  /**
   * Event callback to be called when a valid input segment has been found
   * and speech recognition process starts.  This can be used to know the
   * actual start timing of recognition process.  On short-pause segmentation
   * mode and decoder-based VAD mode, this will be called only once at a
   * triggered long input.  @sa CALLBACK_EVENT_SEGMENT_BEGIN.
   * 
   */
static void
event_recog_begin(Recog *recog, void *dummy)
{
  // noop
}

  /**
   * Event callback to be called when a valid input segment has ended
   * up, speech recognition process ends and return to wait status for
   * another input to come.  On short-pause segmentation mode and
   * decoder-based VAD mode, this will be called only once after a
   * triggered long input.  @sa CALLBACK_EVENT_SEGMENT_END.
   * 
   */
static void
event_recog_end(Recog *recog, void *dummy)
{
  // noop
}

  /**
   * On short-pause segmentation and decoder-based VAD mode, this
   * callback will be called at the beginning of each segment,
   * segmented by short pauses.
   * 
   */
static void
event_segment_begin(Recog *recog, void *dummy)
{
  // noop
}

  /**
   * On short-pause segmentation and decoder-based VAD mode, this
   * callback will be called at the end of each segment,
   * segmented by short pauses.
   * 
   */
static void
event_segment_end(Recog *recog, void *dummy)
{
  // noop
}

  /**
   * Event callback to be called when the 1st pass of recognition process
   * starts for the input.
   * 
   */
static void
event_pass1_begin(Recog *recog, void *dummy)
{
  // noop
}

  /**
   * Event callback to be called periodically at every input frame.  This can
   * be used to get progress status of the first pass at each frame.
   * 
   */
static void
event_pass1_frame(Recog *recog, void *dummy)
{
  // noop
}

  /**
   * Result callback to be called periodically at the 1st pass of
   * recognition process, to get progressive output.
   * 
   */
static void
result_pass1_interim(Recog *recog, void *dummy)
{
  // noop
}

  /**
   * Result callback to be called just at the end of 1st pass, to provide
   * recognition status and result of the 1st pass.
   * 
   */
static void
result_pass1(Recog *recog, void *dummy)
{
  // noop
}

  /**
   * When compiled with "--enable-word-graph", this callback will be called
   * at the end of 1st pass to provide word graph generated at the 1st pass.
   * 
   */
static void
result_pass1_graph(Recog *recog, void *dummy)
{
  // noop
}

  /**
   * Event callback to be called when the 1st pass of recognition process
   * ends for the input and proceed to 2nd pass.
   * 
   */
static void
event_pass1_end(Recog *recog, void *dummy)
{
  // noop
}

  /**
   * Status callback to be called after the 1st pass to provide information
   * about input (length etc.)
   * 
   */
static void
status_param(Recog *recog, void *dummy)
{
  // noop
}

  /**
   * Event callback to be called when the 2nd pass of recognition
   * process starts.
   * 
   */
static void
event_pass2_begin(Recog *recog, void *dummy)
{
  wt_raw(adinnet_asd, RESULTSTART, strlen(RESULTSTART));
}

  /**
   * Event callback to be called when the 2nd pass of recognition
   * process ends.
   * 
   */
static void
event_pass2_end(Recog *recog, void *dummy)
{
  wt_raw(adinnet_asd, RESULTEND, strlen(RESULTEND));
}

  /**
   * Result callback to provide final recognition result and status.
   * 
   */
static void
result_pass2(Recog *recog, void *dummy)
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
  pos += sprintf(&buf[pos], "]} ");

  // writes the data to the socket.
  int total_bytes_written = 0;
  while (total_bytes_written < pos) {
    int bytes_written = wt_raw(adinnet_asd, &buf[total_bytes_written], pos - total_bytes_written);
    if (bytes_written == -1) {
      break;
    }
    total_bytes_written += bytes_written; 
  }
}

  /**
   * Result callback to provide result of GMM computation, if GMM is used.
   * 
   */
static void
result_gmm(Recog *recog, void *dummy)
{
  // noop
}

  /**
   * Result callback to provide the whole word lattice generated at
   * the 2nd pass.  Use with "-lattice" option.
   * 
   */
static void
result_graph(Recog *recog, void *dummy)
{
  // noop
}

  /**
   * Result callback to provide the whole confusion network generated at
   * the 2nd pass.  Use with "-confnet" option.
   * 
   */
static void
result_confnet(Recog *recog, void *dummy)
{
  // noop
}

  /**
   * A/D-in plugin callback to access to the captured input.  This
   * will be called at every time a small audio fragment has been read
   * into Julius.  This callback will be processed first in Julius,
   * and after that Julius will process the content for recognition.
   * This callback can be used to monitor or modify the raw audio
   * input in user-side application.
   * 
   */
static void
adin_captured(Recog *recog, SP16 *speech, int samplenum, void *dummy)
{
  // noop
}

  /**
   * A/D-in plugin callback to access to the triggered input.  This
   * will be called for input segments triggered by level and
   * zerocross.  After processing this callback, Julius will process
   * the content for recognition.  This callback can be used to
   * monitor or modify the triggered audio input in user-side
   * application.
   * 
   */
static void
adin_triggered(Recog *recog, SP16 *speech, int samplenum, void *dummy)
{
  // noop
}

  /**
   * Event callback to be called when the engine becomes paused.
   * 
   */
static void
event_pause(Recog *recog, void *dummy)
{
  wt_raw(adinnet_asd, ENGINEPAUSE, strlen(ENGINEPAUSE));
}

  /**
   * Event callback to be called when the engine becomes resumed.
   * 
   */
static void
event_resume(Recog *recog, void *dummy)
{
  wt_raw(adinnet_asd, ENGINERESUME, strlen(ENGINERESUME));

}

static boolean
opt_adinnetout(Jconf *jconf, char *arg[], int argnum)
{
  adinnet_out = TRUE;
  return TRUE;
}

boolean
write_output_to_adinnet()
{
  return adinnet_out;
}

void
adinnet_add_option()
{
  j_add_option("-adinnetout", 0, 0, "output to adinnet socket", opt_adinnetout);
}

void
setup_output_adinnet(Recog *recog, void *data)
{
  callback_add(recog, CALLBACK_EVENT_PROCESS_ONLINE, event_process_online, data);
  callback_add(recog, CALLBACK_EVENT_PROCESS_OFFLINE, event_process_offline, data);
  callback_add(recog, CALLBACK_EVENT_SPEECH_READY, event_speech_ready, data);
  callback_add(recog, CALLBACK_EVENT_SPEECH_START, event_speech_start, data);
  callback_add(recog, CALLBACK_EVENT_SPEECH_STOP, event_speech_stop, data);
  callback_add(recog, CALLBACK_EVENT_RECOGNITION_BEGIN, event_recog_begin, data);
  callback_add(recog, CALLBACK_EVENT_RECOGNITION_END, event_recog_end, data);
  if (recog->jconf->decodeopt.segment) { /* short pause segmentation */
    callback_add(recog, CALLBACK_EVENT_SEGMENT_BEGIN, event_segment_begin, data);
    callback_add(recog, CALLBACK_EVENT_SEGMENT_END, event_segment_end, data);
  }
  callback_add(recog, CALLBACK_EVENT_PASS1_BEGIN, event_pass1_begin, data);
  {
    JCONF_SEARCH *s;
    boolean ok_p;
    ok_p = TRUE;
    for(s=recog->jconf->search_root;s;s=s->next) {
      if (s->output.progout_flag) ok_p = FALSE;
    }
    if (ok_p) {      
      have_progout = FALSE;
    } else {
      have_progout = TRUE;
    }
  }
  if (!recog->jconf->decodeopt.realtime_flag && verbose_flag && ! have_progout) {
    callback_add(recog, CALLBACK_EVENT_PASS1_FRAME, event_pass1_frame, data);
  }
  callback_add(recog, CALLBACK_RESULT_PASS1_INTERIM, result_pass1_interim, data);
  callback_add(recog, CALLBACK_RESULT_PASS1, result_pass1, data);
#ifdef WORD_GRAPH
  callback_add(recog, CALLBACK_RESULT_PASS1_GRAPH, result_pass1_graph, data);
#endif
  callback_add(recog, CALLBACK_EVENT_PASS1_END, event_pass1_end, data);
  callback_add(recog, CALLBACK_STATUS_PARAM, status_param, data);
  callback_add(recog, CALLBACK_EVENT_PASS2_BEGIN, event_pass2_begin, data);
  callback_add(recog, CALLBACK_EVENT_PASS2_END, event_pass2_end, data);
  callback_add(recog, CALLBACK_RESULT, result_pass2, data); // rejected, failed
  callback_add(recog, CALLBACK_RESULT_GMM, result_gmm, data);
  /* below will be called when "-lattice" is specified */
  callback_add(recog, CALLBACK_RESULT_GRAPH, result_graph, data);
  /* below will be called when "-confnet" is specified */
  callback_add(recog, CALLBACK_RESULT_CONFNET, result_confnet, data);
  callback_add_adin(recog, CALLBACK_ADIN_CAPTURED, adin_captured, data);
  callback_add_adin(recog, CALLBACK_ADIN_TRIGGERED, adin_triggered, data);
  callback_add(recog, CALLBACK_EVENT_PAUSE, event_pause, data);
  callback_add(recog, CALLBACK_EVENT_RESUME, event_resume, data);
}
