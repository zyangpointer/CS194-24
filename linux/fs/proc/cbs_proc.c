/* cs194-24 Lab 2 */

#include "cbs_proc.h"
#include <linux/kernel.h>
#define CBS_PROC_H

enum snap_event
{
    SNAP_EVENT_CBS_SCHED,     /* Triggers when the CBS scheduler context switches a task */
};

enum snap_trig
{
    SNAP_TRIG_BEDGE,    /* Triggers on the edge before an event starts */
    SNAP_TRIG_AEDGE,    /* Triggers on the after before an event starts */
};

struct snap_bucket {
	enum snap_event s_event;
	enum snap_trig s_trig;
	int device;
	int bucket_depth;
	struct cbs_proc_t* proc_list;
};

struct snap_buffer {
	struct snap_bucket * buckets;
	int num_buckets;
};
struct cbs_proc {
	//TODO: put pointer to the rq in here
	long pid;
	cbs_time_t creation_time;
	cbs_time_t start_time;
	cbs_time_t end_time; //initialize to -1
	cbs_time_t period;
	cbs_time_t compute_time;
	enum cbs_state state;
	int valid; //1 if it is, 0 if its not
	struct cbs_proc* next; //pointer to next struct in the list
}; 

extern struct snap_buffer buffer;

int cbs_snap(char *buf)
{
	int len=0;

	int index;

	for(index = 0; index < buffer.num_buckets; index++){
		struct snap_bucket bucket = buffer.buckets[index];
		len += sprintf(buf+len, "Bucket #%i\n", index);
		if (bucket.s_event == SNAP_EVENT_CBS_SCHED){
			len += sprintf(buf+len, "EVENT: CBS\n");
		} else {
			len += sprintf(buf+len, "EVEVNT: UNKNOWN\n");
		}
		if (bucket.s_trig == SNAP_TRIG_AEDGE){
			len += sprintf(buf+len, "TRIG: AEDGE\n");
		} else if (bucket.s_trig == SNAP_TRIG_BEDGE){
			len += sprintf(buf+len, "TRIG: BEDGE\n");
		} else {
			len += sprintf(buf+len, "TRIG: UKNOWN\n");
		}
		len += sprintf(buf+len, "DEPTH: %i\n", bucket.bucket_depth);
		len += sprintf(buf+len, "\n");
	}

	return len;
}

/*
 * Lists the CBS history
 *
 * This must call "func(ID, arg)" exactly once for every process
 * invocation in the history window.  The oldest history entry must
 * get called first, and the newest last.  The integer paramater
 * defines the snapshot index to iterate over.
 */
void cbs_list_history(int sid, cbs_func_t func, void *arg){
	

}

/*
 * Shows the currently running CBS process
 *
 * This is similar to _history, but it just calls with the currently
 * running process.
 */
void cbs_list_current(int sid, cbs_func_t func, void *arg){

}

/*
 * Shows the next CBS process that will be run.
 */
void cbs_list_next(int sid, cbs_func_t func, void *arg){

}

/*
 * Lists every process known to CBS except the currently running
 * process and next process to run.
 *
 * These are called in no particular order.
 */
void cbs_list_rest(int sid, cbs_func_t func, void *arg){

}

/*
 * Obtains the kernel-PID (a system wide unique identifier) for a process.
 */
long cbs_get_pid(cbs_proc_t p){
	struct cbs_proc * proc;
	proc = p;
	return proc->pid;
}
/*
 * Obtains the creation time of a process.
 *
 * This time is in arbitrary units, but it must be monotonically
 * increasing.  The key is that a (pid, ctime) pair is enough to
 * uniquely identify a process.
 */
cbs_time_t cbs_get_ctime(cbs_proc_t p){
	struct cbs_proc * proc;
	proc = p;
	return proc->creation_time;
}

/*
 * Obtains the start time of a process
 *
 * This returns the time when the process was started for entries in
 * the CBS history, and "-1" for all other entries.
 */
cbs_time_t cbs_get_start(cbs_proc_t p){
	struct cbs_proc * proc;
	proc = p;
	return proc->start_time;
}

/*
 * Obtains the end time of a process
 *
 * Just like _start, this returns -1 on non-history processes
 */
cbs_time_t cbs_get_end(cbs_proc_t p){
	struct cbs_proc * proc;
	proc = p;
	return proc->end_time;
}

/*
 * Obtains the period of a process
 */
cbs_time_t cbs_get_period(cbs_proc_t p){
	struct cbs_proc * proc;
	proc = p;
	return proc->period;
}

/*
 * Obtains the compute time of a process
 *
 * The process will be allocated exactly (for real-time) or
 * approximately (for CBS) this much execution time once for every
 * period it runs.
 */
cbs_time_t cbs_get_compute(cbs_proc_t p){
	struct cbs_proc *proc;
	proc = p;
	return proc->compute_time;
}

/*
 * Obtains the current run state of a process.
 */
enum cbs_state cbs_get_state(cbs_proc_t p){
	struct cbs_proc *proc;
	proc = p;
	return proc->state;
}