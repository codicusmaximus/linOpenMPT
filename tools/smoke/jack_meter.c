/* Listens to every JACK output port whose name matches argv[1] (a regex)
   for argv[2] seconds and prints the peak and RMS level seen. */
#include <jack/jack.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXP 8
static jack_port_t *in[MAXP];
static int nin = 0;
static double sumsq = 0, peak = 0;
static long frames = 0;

static int process(jack_nframes_t n, void *arg)
{
	for(int p = 0; p < nin; p++)
	{
		const float *buf = jack_port_get_buffer(in[p], n);
		for(jack_nframes_t i = 0; i < n; i++)
		{
			double v = buf[i];
			sumsq += v * v;
			if(fabs(v) > peak) peak = fabs(v);
		}
	}
	frames += n;
	return 0;
}

int main(int argc, char **argv)
{
	const char *pattern = argc > 1 ? argv[1] : ".*";
	int seconds = argc > 2 ? atoi(argv[2]) : 5;
	jack_client_t *c = jack_client_open("meter", JackNoStartServer, NULL);
	if(!c) { fprintf(stderr, "no jack server\n"); return 2; }
	jack_set_process_callback(c, process, NULL);
	const char **ports = NULL;
	for(int tries = 0; tries < 50 && (!ports || !ports[0]); tries++)
	{
		if(ports) jack_free(ports);
		ports = jack_get_ports(c, pattern, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput);
		if(!ports || !ports[0]) usleep(200000);
	}
	if(!ports || !ports[0]) { fprintf(stderr, "no ports match %s\n", pattern); return 3; }
	for(int i = 0; ports[i] && nin < MAXP; i++)
	{
		char name[32];
		snprintf(name, sizeof name, "in%d", nin);
		in[nin++] = jack_port_register(c, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	}
	jack_activate(c);
	for(int i = 0; i < nin; i++)
	{
		jack_connect(c, ports[i], jack_port_name(in[i]));
		printf("listening to %s\n", ports[i]);
	}
	sleep(seconds);
	jack_deactivate(c);
	double rms = frames ? sqrt(sumsq / ((double)frames * nin)) : 0;
	printf("frames=%ld peak=%.4f rms=%.4f\n", frames, peak, rms);
	jack_client_close(c);
	return peak > 0.001 ? 0 : 1;
}
