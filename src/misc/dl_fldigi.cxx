#include <config.h>

#ifdef __MINGW32__
#  include "compat.h"
#endif

#include <iostream>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/file.h>

#include <FL/Fl_Choice.H>

#include "configuration.h"
#include "util.h"
#include "fl_digi.h"
#include "dl_fldigi.h"
#include "util.h"
#include "confdialog.h"
#include "fl_digi.h"
#include "main.h"
#include "threads.h"
#include "qrunner.h"

#include "irrXML.h"
using namespace std;
using namespace irr; // irrXML is located 
using namespace io;  // in the namespace irr::io

#define DL_FLDIGI_DEBUG
#define DL_FLDIGI_CACHE_FILE "dl_fldigi_cache.xml"

int rxTimer = 0;
time_t rawtime;
struct tm * timeinfo;
  
time_t seconds;


struct dl_fldigi_post_threadinfo
{
	CURL *curl;
	char *post_data;
};

struct dl_fldigi_download_threadinfo
{
	CURL *curl;
	FILE *file;
};

struct payload
{
	char *name;
	char *sentence_delimiter;
	char *field_delimiter;
	int fields;
	char *callsign;
	int  shift;
	int  baud;
	int  coding;
	int time;
	int latitude;
	int longitude;
	int altitude;
	struct payload *next;
};

bool dl_fldigi_downloaded_once = false;
int dl_fldigi_initialised = 0;
const char *dl_fldigi_cache_file;
struct payload *payload_list = NULL;

static void *dl_fldigi_post_thread(void *thread_argument);
static void *dl_fldigi_download_thread(void *thread_argument);
void dl_fldigi_delete_payloads();

void dl_fldigi_init()
{
	CURLcode r;
	const char *home;
	char *cache_file;
	size_t i, fsz;

	#ifdef DL_FLDIGI_DEBUG
		fprintf(stderr, "dl_fldigi: dl_fldigi_init() was executed in thread %li\n", pthread_self());
	#endif

	/* The only thread-unsafe step of dl_fldigi. Needs to be run once, at the start, when there are no other threads. */
	r = curl_global_init(CURL_GLOBAL_ALL);

	if (r != 0)
	{
		fprintf(stderr, "dl_fldigi: curl_global_init failed: (%i) %s\n", r, curl_easy_strerror(r));
		exit(EXIT_FAILURE);
	}

	home = HomeDir.c_str();

	fsz = strlen(home) + strlen(DL_FLDIGI_CACHE_FILE) + 1;
	i = 0;

	cache_file = (char *) malloc(fsz);

	if (cache_file == NULL)
	{
		fprintf(stderr, "dl_fldigi: denied %zi bytes of RAM for 'cache_file'\n", fsz);
		exit(EXIT_FAILURE);
	}

	memcpy(cache_file + i, home, strlen(home));
	i += strlen(home);

	memcpy(cache_file + i, DL_FLDIGI_CACHE_FILE, strlen(DL_FLDIGI_CACHE_FILE));
	i += strlen(DL_FLDIGI_CACHE_FILE);

	cache_file[i] = '\0';
	i++;

	if (i != fsz)
	{
		fprintf(stderr, "dl_fldigi: assertion failed \"i == fsz\" (i = %zi, fsz = %zi) \n", i, fsz);
	}

	dl_fldigi_cache_file = cache_file;

	#ifdef DL_FLDIGI_DEBUG
		fprintf(stderr, "dl_fldigi: cache file is '%s'\n", dl_fldigi_cache_file);
	#endif

	dl_fldigi_initialised = 1;
	full_memory_barrier();
}

void dl_fldigi_post(const char *data, const char *identity)
{
	char *data_safe, *identity_safe, *post_data;
	size_t i, data_length, identity_length, post_data_length;
	struct dl_fldigi_post_threadinfo *t;
	pthread_t thread;
	CURL *curl;
	CURLcode r1, r2, r3;

	/* The first of two globals accessed by this function */
	if (!dl_fldigi_initialised)
	{
		fprintf(stderr, "dl_fldigi: a call to dl_fldigi_post was aborted; dl_fldigi has not been initialised\n");
		return;
	}

	#ifdef DL_FLDIGI_DEBUG
		fprintf(stderr, "dl_fldigi: dl_fldigi_post() was executed in \"parent\" thread %li\n", pthread_self());
		fprintf(stderr, "dl_fldigi: begin attempting to post string '%s' and identity '%s'\n", data, identity);
	#endif

	curl = curl_easy_init();

	if (!curl)
	{
		fprintf(stderr, "dl_fldigi: curl_easy_init failed\n");
		return;
	}

	data_safe     = curl_easy_escape(curl, data, 0);
	identity_safe = curl_easy_escape(curl, identity, 0);

	if (data_safe != NULL)
	{
		data_length     = strlen(data_safe);
	}
	else
	{
		data_length = 0;
	}

	if (identity_safe != NULL)
	{
		identity_length = strlen(identity_safe);
	}
	else
	{
		identity_length = 0;
	}

	#define POST_DATAKEY        "string="
	#define POST_IDENTITYKEY    "&identity="

	post_data_length = data_length + identity_length + strlen(POST_DATAKEY) + strlen(POST_IDENTITYKEY) + 1;
	post_data = (char *) malloc(post_data_length);

	if (post_data == NULL)
	{
		fprintf(stderr, "dl_fldigi: denied %zi bytes of RAM for 'post_data'\n", post_data_length);
		curl_easy_cleanup(curl);
		return;
	}

	/* Cook up "string=$data_safe&identity=$identity_safe" */
	i = 0;

	memcpy(post_data + i, POST_DATAKEY, strlen(POST_DATAKEY));
	i += strlen(POST_DATAKEY);

	if (data_length != 0)
	{
		memcpy(post_data + i, data_safe, data_length);
		i += data_length;
	}

	memcpy(post_data + i, POST_IDENTITYKEY, strlen(POST_IDENTITYKEY));
	i += strlen(POST_IDENTITYKEY);

	if (identity_length != 0)
	{
		memcpy(post_data + i, identity_safe, identity_length);
		i += identity_length;
	}

	post_data[i] = '\0';
	i ++;

	if (i != post_data_length)
	{
		fprintf(stderr, "dl_fldigi: assertion failed \"i == post_data_length\" (i = %zi, post_data_length = %zi) \n", i, post_data_length);
	}

	curl_free(data_safe);
	curl_free(identity_safe);

	/* The second of two globals accessed by this function: progdefaults.dl_online */
	if (progdefaults.dl_online)
	{
		#ifdef DL_FLDIGI_DEBUG
			fprintf(stderr, "dl_fldigi: preparing to post '%s'\n", post_data);
		#endif
	}
	else
	{
		fprintf(stderr, "dl_fldigi: (offline mode) would have posted '%s'\n", post_data);
		curl_easy_cleanup(curl);
		return;
	}

	r1 = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
	if (r1 != 0)
	{
		fprintf(stderr, "dl_fldigi: curl_easy_setopt (CURLOPT_POSTFIELDS) failed: %s\n", curl_easy_strerror(r1));
		curl_easy_cleanup(curl);
		return;
	}

	r2 = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post_data_length - 1);
	if (r2 != 0)
	{
		fprintf(stderr, "dl_fldigi: curl_easy_setopt (CURLOPT_POSTFIELDSIZE) failed: %s\n", curl_easy_strerror(r2));
		curl_easy_cleanup(curl);
		return;
	}

	r3 = curl_easy_setopt(curl, CURLOPT_URL, "http://www.robertharrison.org/listen/listen.php");
	if (r3 != 0)
	{
		fprintf(stderr, "dl_fldigi: curl_easy_setopt (CURLOPT_URL) failed: %s\n", curl_easy_strerror(r3));
		curl_easy_cleanup(curl);
		return;
	}

	t = (struct dl_fldigi_post_threadinfo *) malloc(sizeof(struct dl_fldigi_post_threadinfo));

	if (t == NULL)
	{
		fprintf(stderr, "dl_fldigi: denied %zi bytes of RAM for 'struct dl_fldigi_post_threadinfo'\n", sizeof(struct dl_fldigi_post_threadinfo));
		curl_easy_cleanup(curl);
		return;
	}

	t->curl = curl;
	t->post_data = post_data;

	/* !! */
	full_memory_barrier();

	/* the 4th argument passes the thread the information it needs */
	if (pthread_create(&thread, NULL, dl_fldigi_post_thread, (void *) t) != 0)
	{
		perror("dl_fldigi: post pthread_create");
		curl_easy_cleanup(curl);
		return;
	}

	#ifdef DL_FLDIGI_DEBUG
		fprintf(stderr, "dl_fldigi: created a thread to finish the posting, returning now\n");
	#endif
}

void *dl_fldigi_post_thread(void *thread_argument)
{
	struct dl_fldigi_post_threadinfo *t;
	t = (struct dl_fldigi_post_threadinfo *) thread_argument;
	CURLcode result;

	#ifdef DL_FLDIGI_DEBUG
		fprintf(stderr, "dl_fldigi: (thread %li) posting '%s'\n", pthread_self(), t->post_data);
	#endif

	//put_status("dl_fldigi: sentence uploading...", 10);

	result = curl_easy_perform(t->curl);

	/* tests CURL-success only */
	if (result == 0)
	{
		#ifdef DL_FLDIGI_DEBUG
			fprintf(stderr, "dl_fldigi: (thread %li) curl result (%i) Success!\n", pthread_self(), result);
		#endif

		//put_status("dl_fldigi: sentence uploaded!", 10);
	}
	else
	{
		#ifdef DL_FLDIGI_DEBUG
			fprintf(stderr, "dl_fldigi: (thread %li) curl result (%i) %s\n", pthread_self(), result, curl_easy_strerror(result));	
		#endif

		//put_status("dl_fldigi: sentence upload failed", 10);
	}

	curl_easy_cleanup(t->curl);
	free(t->post_data);
	free(t);

	pthread_exit(0);
}

void dl_fldigi_download()
{
	pthread_t thread;
	struct dl_fldigi_download_threadinfo *t;
	CURL *curl;
	CURLcode r1, r3;
	FILE *file;
	int r2;

	if (!dl_fldigi_initialised)
	{
		fprintf(stderr, "dl_fldigi: a call to dl_fldigi_download was aborted; dl_fldigi has not been initialised\n");
		return;
	}

	if (!progdefaults.dl_online)
	{
		fprintf(stderr, "dl_fldigi: a call to dl_fldigi_download was aborted: refusing to download a file whist in offline mode.\n");
		return;
	}

	#ifdef DL_FLDIGI_DEBUG
		fprintf(stderr, "dl_fldigi: dl_fldigi_download() was executed in \"parent\" thread %li\n", pthread_self());
		fprintf(stderr, "dl_fldigi: begin download attempt...\n");
	#endif

	curl = curl_easy_init();

	if (!curl)
	{
		fprintf(stderr, "dl_fldigi: curl_easy_init failed\n");
		return;
	}

	r1 = curl_easy_setopt(curl, CURLOPT_URL, "http://www.robertharrison.org/listen/allpayloads.php");
	if (r1 != 0)
	{
		fprintf(stderr, "dl_fldigi: curl_easy_setopt (CURLOPT_URL) failed: %s\n", curl_easy_strerror(r1));
		curl_easy_cleanup(curl);
		return;
	}

	t = (struct dl_fldigi_download_threadinfo *) malloc(sizeof(struct dl_fldigi_download_threadinfo));

	if (t == NULL)
	{
		fprintf(stderr, "dl_fldigi: denied %zi bytes of RAM for 'struct dl_fldigi_download_threadinfo'\n", sizeof(struct dl_fldigi_download_threadinfo));
		curl_easy_cleanup(curl);
		return;
	}

	file = fopen(dl_fldigi_cache_file, "w");

	if (file == NULL)
	{
		perror("dl_fldigi: fopen cache file");
		curl_easy_cleanup(curl);
		return;
	}

	r2 = flock(fileno(file), LOCK_EX | LOCK_NB);

	if (r2 == EWOULDBLOCK)
	{
		fprintf(stderr, "dl_fldigi: cache file is locked; not downloading\n");
		curl_easy_cleanup(curl);
		fclose(file);
		return;
	}
	else if (r2 != 0)
	{
		perror("dl_fldigi: flock cache file");
		curl_easy_cleanup(curl);
		fclose(file);
		return;
	}

	r3 = curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
	if (r3 != 0)
	{
		fprintf(stderr, "dl_fldigi: curl_easy_setopt (CURLOPT_WRITEDATA) failed: %s\n", curl_easy_strerror(r3));
		curl_easy_cleanup(curl);
		flock(fileno(file), LOCK_UN);
		fclose(file);
		return;
	}

	t->curl = curl;
	t->file = file;

	/* !! */
	full_memory_barrier();

	/* the 4th argument passes the thread the information it needs */
	if (pthread_create(&thread, NULL, dl_fldigi_download_thread, (void *) t) != 0)
	{
		perror("dl_fldigi: download pthread_create");
		flock(fileno(file), LOCK_UN);
		fclose(file);
		return;
	}

	#ifdef DL_FLDIGI_DEBUG
		fprintf(stderr, "dl_fldigi: created a thread to perform the download, returning now\n");
	#endif
}

void *dl_fldigi_download_thread(void *thread_argument)
{
	struct dl_fldigi_download_threadinfo *t;
	t = (struct dl_fldigi_download_threadinfo *) thread_argument;
	CURLcode result;
	int r1;

	#ifdef DL_FLDIGI_DEBUG
		fprintf(stderr, "dl_fldigi: (thread %li) performing download...\n", pthread_self());
	#endif

	//put_status("dl_fldigi: payload information: downloading...", 10);

	result = curl_easy_perform(t->curl);

	curl_easy_cleanup(t->curl);

	if (result == 0)
	{
		/* Swap our exclusive lock created in download() for a shared lock.
		 * Relocking it shared means that update_payloads() can get its own shared lock on the
		 * file without blocking, but download() cannot open its exclusive lock that might
		 * start a new download thread. This effectivly reserves DL_FLDIGI_TID for our use. */

		r1 = flock(fileno(t->file), LOCK_SH | LOCK_NB);

		if (r1 != 0)
		{
			//put_status("dl_fldigi: payload information: download failed", 10);
			perror("dl_fldigi: f-re-lock cache file failed");
			flock(fileno(t->file), LOCK_UN);
			fclose(t->file);
			free(t);
			pthread_exit(0);
		}

		#ifdef DL_FLDIGI_DEBUG
			fprintf(stderr, "dl_fldigi: (thread %li) curl result (%i) Success!\n", pthread_self(), result);
		#endif

		//put_status("dl_fldigi: payload information: downloaded!", 10);

		/* ask qrunner to deal with this */
		SET_THREAD_ID(DL_FLDIGI_TID);
		REQ(dl_fldigi_update_payloads);
	}
	else
	{
		#ifdef DL_FLDIGI_DEBUG
			fprintf(stderr, "dl_fldigi: (thread %li) curl result (%i) %s\n", pthread_self(), result, curl_easy_strerror(result));	
		#endif

		//put_status("dl_fldigi: payload information: download failed", 10);
	}

	flock(fileno(t->file), LOCK_UN);
	fclose(t->file);
	free(t);

	pthread_exit(0);
}

void dl_fldigi_delete_payloads()
{
	struct payload *d, *n;
	int i;

	#ifdef DL_FLDIGI_DEBUG
		fprintf(stderr, "dl_fldigi: (thread %li) cleaning in-memory payload information (structs)...\n", pthread_self());
	#endif

	if (bHAB)
	{
		habFlightXML->clear();
	}

	d = payload_list;
	i = 0;

	while (d != NULL)
	{
		#define auto_free(x)   do { void *a = (x); if (a != NULL) { free(a); } } while (0)

		auto_free(d->name);
		auto_free(d->sentence_delimiter);
		auto_free(d->field_delimiter);
		auto_free(d->callsign);

		n = d->next;
		free(d);
		d = n;

		i++;
	}

	payload_list = NULL;

	#ifdef DL_FLDIGI_DEBUG
		fprintf(stderr, "dl_fldigi: (thread %li) cleaned %i payload structs...\n", pthread_self(), i);
	#endif
}

void dl_fldigi_update_payloads()
{
	FILE *file;
	int r1, r_shift, r_baud;
	const char *r_coding, *r_dbfield;
	IrrXMLReader *xml;
	struct payload *p, *n;
	int i, x;

	#ifdef DL_FLDIGI_DEBUG
		fprintf(stderr, "dl_fldigi: (thread %li) attempting to update UI...\n", pthread_self());
	#endif

	file = fopen(dl_fldigi_cache_file, "r");

	if (file == NULL)
	{
		perror("dl_fldigi: fopen cache file (read)");
		return;
	}

	r1 = flock(fileno(file), LOCK_SH | LOCK_NB);

	if (r1 == EWOULDBLOCK)
	{
		fprintf(stderr, "dl_fldigi: cache file is locked; not updating UI\n");
		fclose(file);
		return;
	}
	else if (r1 != 0)
	{
		perror("dl_fldigi: flock cache file (read)");
		fclose(file);
		return;
	}

	#ifdef DL_FLDIGI_DEBUG
		fprintf(stderr, "dl_fldigi: opened file, now updating UI...\n");
	#endif

	dl_fldigi_delete_payloads();

	xml = createIrrXMLReader(file);

	if (!xml)
	{
		fprintf(stderr, "dl_fldigi: parsing payload info: createIrrXMLReader failed\n");
		flock(fileno(file), LOCK_UN);
		fclose(file);
		return;
	}

	p = payload_list;
	i = 0;

	while (xml->read())
	{
		if (xml->getNodeType() == EXN_ELEMENT)
		{
			if (strcmp("name", xml->getNodeName()) == 0)
			{
				n = (struct payload *) malloc(sizeof(struct payload));
				
				if (n == NULL)
				{
					fprintf(stderr, "dl_fldigi: denied %zi bytes of RAM for 'struct payload'\n", sizeof(struct payload));
					dl_fldigi_delete_payloads();
					delete xml;
					flock(fileno(file), LOCK_UN);
					fclose(file);
					return;
				}

				/* Nulls all the char pointers, the next pointer, zeroes the ints */
				memset(n, 0, sizeof(struct payload));

				if (p == NULL)
				{
					payload_list = n;
					p = n;
				}
				else
				{
					p->next = n;
					p = p->next;
				}

				xml->read();
				p->name = strdup(xml->getNodeData());
				xml->read();

				if (bHAB)
				{
					habFlightXML->add(p->name);
					x = 0; //x = 1 not 0 as the SEQ count in the xml files also starts at 1
				}

				#ifdef DL_FLDIGI_DEBUG
					fprintf(stderr, "dl_fldigi: adding payload %i: '%s'\n", i, p->name);
				#endif

				i++;
			}
			else if (strcmp("sentence_delimiter", xml->getNodeName()) == 0)
			{
				xml->read();
				p->sentence_delimiter = strdup(xml->getNodeData());
				xml->read();
			}
			else if (strcmp("field_delimiter", xml->getNodeName()) == 0)
			{
				xml->read();
				p->field_delimiter = strdup(xml->getNodeData());
				xml->read();
			}
			else if (strcmp("fields", xml->getNodeName()) == 0)
			{
				xml->read();
				p->fields = atoi(xml->getNodeData());
				xml->read();
			}
			else if (strcmp("callsign", xml->getNodeName()) == 0)
			{
				xml->read();
				p->callsign = strdup(xml->getNodeData());
				xml->read();
			}
			else if (strcmp("shift", xml->getNodeName()) == 0)
			{
				xml->read();
				r_shift = atoi(xml->getNodeData());
				xml->read();

				if (r_shift == 170)
				{
					p->shift = 4;
				}
				else if (r_shift == 350)
				{
					p->shift = 7;
				}
				else if (r_shift == 425)
				{
					p->shift = 8;
				}
			}
			else if (strcmp("baud", xml->getNodeName()) == 0)
			{
				xml->read();
				r_baud = atoi(xml->getNodeData());
				xml->read();

				if (r_baud == 45)
				{
					p->baud = 0;
				}
				else if (r_baud == 50)
				{
					p->baud = 2;
				}
				else if (r_baud == 100)
				{
					p->baud = 5;
				}
				else if (r_baud == 150)
				{
					p->baud = 7;
				}
				else if (r_baud == 200)
				{
					p->baud = 8;
				}
				else if (r_baud == 300)
				{
					p->baud = 9;
				}
			}
			else if (strcmp("coding", xml->getNodeName()) == 0)
			{
				/* Why does this need to be commented? XXX */
				// xml->read();
				r_coding = xml->getNodeData();
				xml->read();

				if (strcmp("baudot", r_coding) == 0)
				{
					p->coding = 0;
				}
				else if (strcmp("ascii-7", r_coding) == 0)
				{
					p->coding = 1;
				}
				else if (strcmp("ascii-8", r_coding) == 0)
				{
					p->coding = 2;
				}
			}
			else if (strcmp("dbfield", xml->getNodeName()) == 0)
			{
			xml->read();
			r_dbfield = xml->getNodeData();
			x++;
			if (strcmp("time", r_dbfield) == 0)
				{
					cout << "time = " << x << endl;
					p->time = x;
				}
			else if (strcmp("latitude", r_dbfield) == 0)
				{
					cout << "latitude = " << x << endl;
					p->latitude = x;
				}
			else if (strcmp("longitude", r_dbfield) == 0)
				{
					cout << "longitude = " << x << endl;
					p->longitude = x;
				}
			else if (strcmp("altitude", r_dbfield) == 0)
				{
					cout << "altitude = " << x << endl;
					p->altitude = x;
				}
			}
		}
	}

	#ifdef DL_FLDIGI_DEBUG
		fprintf(stderr, "dl_fldigi: UI updated: added %i payloads.\n", i);
	#endif

	if (bHAB && progdefaults.xmlPayloadname.length() != 0)
	{
		#ifdef DL_FLDIGI_DEBUG
			fprintf(stderr, "dl_fldigi: post UI update: attempting to re-select (but not configure) payload '%s'\n", progdefaults.xmlPayloadname.c_str());
		#endif

		habFlightXML->value(habFlightXML->find_item(progdefaults.xmlPayloadname.c_str()));
	}

	//put_status("dl_fldigi: payload information loaded", 10);

	delete xml;
	flock(fileno(file), LOCK_UN);
	fclose(file);
}

void cb_dl_fldigi_select_payload(Fl_Widget *o, void *a)
{
	if (o == habFlightXML)
	{
		#ifdef DL_FLDIGI_DEBUG
			fprintf(stderr, "dl_fldigi: select_payload callback started by habFlightXML\n");
			fprintf(stderr, "dl_fldigi: set current payload name to '%s' (have not configured)\n", habFlightXML->text());
		#endif

		progdefaults.xmlPayloadname = habFlightXML->text();
		progdefaults.changed = true;
	}
	else if (o == habConfigureButton)
	{
		#ifdef DL_FLDIGI_DEBUG
			fprintf(stderr, "dl_fldigi: select_payload callback started by habConfigureButton\n");
			fprintf(stderr, "dl_fldigi: configuring current payload name '%s'\n", progdefaults.xmlPayloadname.c_str());
		#endif

		dl_fldigi_select_payload(progdefaults.xmlPayloadname.c_str());
	}
	else
	{
		fprintf(stderr, "dl_fldigi: select_payload callback started by unknown source.\n");
		return;
	}
}

void dl_fldigi_select_payload(const char *name)
{
	struct payload *p;
	string s;
	int i;

	#ifdef DL_FLDIGI_DEBUG
		fprintf(stderr, "dl_fldigi: (thread %li) attempting to find and configure payload '%s'...\n", pthread_self(), name);
	#endif

	p = payload_list;
	i = 0;

	while (p != NULL)
	{
		if (p->name != NULL && strcmp(p->name, name) == 0)
		{
			#ifdef DL_FLDIGI_DEBUG
				fprintf(stderr, "dl_fldigi: found payload '%s' at link %i...\n", p->name, i);
				fprintf(stderr, "dl_fldigi: configuring payload '%s'...\n", p->name);
			#endif

			/* TODO: Currently, we don't set progdefaults.changed (I think it might become annoying?).
			 * This might be a bad idea. */

			init_modem_sync(MODE_RTTY);

			progdefaults.xmlSentence_delimiter = p->sentence_delimiter;
			progdefaults.xmlField_delimiter = p->field_delimiter;
			progdefaults.xmlFields = p->fields;
			progdefaults.xmlCallsign = p->callsign;
			progdefaults.rtty_shift = p->shift;
			progdefaults.rtty_baud = p->baud;
			progdefaults.rtty_bits = p->coding;
			progdefaults.xml_time = p->time;
			progdefaults.xml_latitude = p->latitude;
			progdefaults.xml_longitude = p->longitude;
			progdefaults.xml_altitude = p->altitude;

			selShift->value(progdefaults.rtty_shift);
			selBaud->value(progdefaults.rtty_baud);
			selBits->value(progdefaults.rtty_bits);
			resetRTTY();

			#ifdef DL_FLDIGI_DEBUG
				fprintf(stderr, "dl_fldigi: configured payload '%s'...\n", p->name);
			#endif

			/* This way of doing concatenation is a bit ugly. */
			//s = "dl_fldigi: configured modem for payload ";
			//s += p->name;
			//put_status(s.c_str(), 10);

			return;
		}

		p = p->next;
		i++;
	}

	fprintf(stderr, "dl_fldigi: searched %i payloads; unable to find '%s' for configuring\n", i, name);
}
