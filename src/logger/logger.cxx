// ----------------------------------------------------------------------------
// logger.cxx Remote Log Interface for fldigi
//
// Copyright 2006-2009 W1HKJ, Dave Freese
//
// This file is part of fldigi.
//
// Fldigi is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Fldigi is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with fldigi.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#if !defined(__WOE32__) && !defined(__APPLE__)
#  include <sys/ipc.h>
#  include <sys/msg.h>
#endif
#include <errno.h>
#include <string>
#include <fstream>
#include <vector>

#include "icons.h"
#include "logger.h"
#include "lgbook.h"
#include "main.h"
#include "modem.h"
#include "waterfall.h"
#include "fl_digi.h"
#include "trx.h"
#include "debug.h"
#include "macros.h"
#include "status.h"
#include "spot.h"
#include "adif_io.h"
#include "date.h"
#include "configuration.h"
#include "flmisc.h"
#include "strutil.h"
#include "qrunner.h"

#include "logsupport.h"
#include "lookupcall.h"
#include "fd_logger.h"

#include "n3fjp_logger.h"

#include "dx_cluster.h"

#include "network.h"

#include <FL/fl_ask.H>

using namespace std;

//---------------------------------------------------------------------
const char *logmode;

static string log_msg;
static string errmsg;
static string notes;

//======================================================================
// LoTW
static string lotw_fname;
static string lotw_logfname;
static string lotw_send_fname;
static string lotw_log_fname;
static int logcheck_count = 0;

string str_lotw;
//======================================================================
// eQSL
void submit_eQSL(cQsoRec &rec, string msg);
//======================================================================

static string adif;

void writeADIF () {
// open the adif file
	FILE *adiFile;
	string fname;

    fname = TempDir;
    fname.append("log.adif");
	adiFile = fl_fopen (fname.c_str(), "a");
	if (adiFile) {
// write the current record to the file
		fprintf(adiFile,"%s<EOR>\n", adif.c_str());
		fclose (adiFile);
	}
}

void putadif(int num, const char *s, string &str = adif)
{
	char tempstr[100];
	int slen = strlen(s);
	int n = snprintf(tempstr, sizeof(tempstr), "<%s:%d>", fields[num].name, slen);
	if (n == -1) {
		LOG_PERROR("snprintf");
		return;
	}
	str.append(tempstr).append(s);
}

vector<int> lotw_recs_sent;

void clear_lotw_recs_sent()
{
	lotw_recs_sent.clear();
}

void restore_lotwsdates()
{
extern int editNbr;
	if (lotw_recs_sent.empty()) return;

	int recnbr;
	cQsoRec *rec;
	for (size_t n = 0; n < lotw_recs_sent.size(); n++) {
		recnbr = lotw_recs_sent[n];
		rec = qsodb.getRec(recnbr);
		rec->putField(LOTWSDATE, "");
		if (recnbr == editNbr) {
			inpLOTWsentdate_log->value("");
			inpLOTWsentdate_log->redraw();
		}
	}
	clear_lotw_recs_sent();
}

static notify_dialog *alert_window = 0;

void check_lotw_log(void *)
{
	string logtxt;
	FILE * logfile = fl_fopen(lotw_log_fname.c_str(), "r");
	if (!logfile) {
		logcheck_count++;
		if (logcheck_count < 20) {
			Fl::repeat_timeout(1.0, check_lotw_log);
			return;
		}
		LOG_ERROR("%s", "NO tqsl log file in 25 seconds!");
		restore_lotwsdates();
		return;
	}
	char c = fgetc(logfile);
	while (!feof(logfile)) {
		logtxt += c;
		c = fgetc(logfile);
		if (c == EOF) break;
	}
	fclose(logfile);

	size_t p = logtxt.find("UploadFile returns 0");
	if (p != string::npos) {
		if (progdefaults.lotw_quiet_mode && progdefaults.lotw_show_delivery) {
			if (!alert_window) alert_window = new notify_dialog;
			alert_window->notify(_("LoTW upload OK"), 5.0);
			REQ(show_notifier, alert_window);
		}
		clear_lotw_recs_sent();
	} else {
		string errlog = LoTWDir;
		errlog.append("lotw_error_log.txt");
		ofstream errfile(errlog.c_str());
		if (errfile) {
			errfile << logtxt;
			errfile.close();
			logtxt.assign("LoTW upload errors\nCheck file ");
			logtxt.append(errlog);
		}
		if (progdefaults.lotw_quiet_mode) {
			std::string alert = _("LoTW upload Failed\nView LoTW error log:");
			alert.append(errlog);
			if (!alert_window) alert_window = new notify_dialog;
			alert_window->notify(alert.c_str(), 15.0);
			REQ(show_notifier, alert_window);
		}
		restore_lotwsdates();
	}
	remove(lotw_log_fname.c_str());
}

void send_to_lotw(void *)
{
	if (progdefaults.lotw_pathname.empty())
		return;
	if (str_lotw.empty()) return;

	lotw_send_fname = LoTWDir;
	lotw_send_fname.append("fldigi_lotw_").append(zdate());
	lotw_send_fname.append("_").append(ztime());
	lotw_send_fname.append(".adi");

	ofstream outfile(lotw_send_fname.c_str());
	outfile << str_lotw;
	outfile.close();

	str_lotw.clear();

	lotw_log_fname = LoTWDir;
	lotw_log_fname.append("lotw_log.txt");

	string pstring;
	pstring.assign("\"").append(progdefaults.lotw_pathname).append("\"");
	pstring.append(" -d -u -a compliant");

	if (progdefaults.lotw_quiet_mode)
		pstring.append(" -q");

	if (progdefaults.submit_lotw_password)
		pstring.append(" -p \"").append(progdefaults.lotw_pwd).append("\"");

	if (!progdefaults.lotw_location.empty())
		pstring.append(" -l \"").append(progdefaults.lotw_location).append("\"");

	pstring.append(" -t \"").append(lotw_log_fname).append("\"");

	pstring.append(" \"").append(lotw_send_fname).append("\"");

	start_process(pstring);

	logcheck_count = 5;
	Fl::add_timeout(5.0, check_lotw_log);
}

string lotw_rec(cQsoRec &rec)
{
	string temp;
	string strrec;

	putadif(CALL, rec.getField(CALL), strrec);

	putadif(MODE, adif2export(rec.getField(MODE)).c_str(), strrec);
	putadif(SUBMODE, adif2submode(rec.getField(MODE)).c_str(), strrec);

	temp = rec.getField(FREQ);
	temp.resize(7);
	putadif(FREQ, temp.c_str(), strrec);

	putadif(QSO_DATE, rec.getField(QSO_DATE), strrec);

	putadif(BAND, rec.getField(BAND), strrec);

	temp = rec.getField(TIME_ON);
	if (temp.length() > 4) temp.erase(4);
	putadif(TIME_ON, temp.c_str(), strrec);

	strrec.append("<EOR>\n");

	return strrec;
}

void submit_lotw(cQsoRec &rec)
{
	string adifrec = lotw_rec(rec);

	if (adifrec.empty()) return;

	if (progdefaults.submit_lotw) {
		if (str_lotw.empty())
			str_lotw = "Fldigi LoTW upload file\n<ADIF_VER:5>2.2.7\n<EOH>\n";
		str_lotw.append(adifrec);
		Fl::awake(send_to_lotw);
	}
}

void submit_ADIF(cQsoRec &rec)
{
	adif.erase();

	putadif(QSO_DATE, rec.getField(QSO_DATE));
	putadif(TIME_ON, rec.getField(TIME_ON));
	putadif(QSO_DATE_OFF, rec.getField(QSO_DATE_OFF));
	putadif(TIME_OFF, rec.getField(TIME_OFF));
	putadif(CALL, rec.getField(CALL));
	putadif(FREQ, rec.getField(FREQ));
	putadif(MODE, adif2export(rec.getField(MODE)).c_str());
	putadif(SUBMODE, adif2submode(rec.getField(MODE)).c_str());
	putadif(RST_SENT, rec.getField(RST_SENT));
	putadif(RST_RCVD, rec.getField(RST_RCVD));
	putadif(TX_PWR, rec.getField(TX_PWR));
	putadif(NAME, rec.getField(NAME));
	putadif(QTH, rec.getField(QTH));
	putadif(STATE, rec.getField(STATE));
	putadif(VE_PROV, rec.getField(VE_PROV));
	putadif(COUNTRY, rec.getField(COUNTRY));
	putadif(GRIDSQUARE, rec.getField(GRIDSQUARE));
	putadif(STX, rec.getField(STX));
	putadif(SRX, rec.getField(SRX));
	putadif(XCHG1, rec.getField(XCHG1));
	putadif(MYXCHG, rec.getField(MYXCHG));
	notes = rec.getField(NOTES);
	for (size_t i = 0; i < notes.length(); i++)
	    if (notes[i] == '\n') notes[i] = ';';
	putadif(NOTES, notes.c_str());
// these fields will always be blank unless they are added to the main
// QSO log area.
	putadif(IOTA, rec.getField(IOTA));
	putadif(DXCC, rec.getField(DXCC));
	putadif(QSL_VIA, rec.getField(QSL_VIA));
	putadif(QSLRDATE, rec.getField(QSLRDATE));
	putadif(QSLSDATE, rec.getField(QSLSDATE));
	putadif(EQSLRDATE, rec.getField(EQSLRDATE));
	putadif(EQSLSDATE, rec.getField(EQSLSDATE));
	putadif(LOTWRDATE, rec.getField(LOTWRDATE));
	putadif(LOTWSDATE, rec.getField(LOTWSDATE));

	writeADIF();
}

//---------------------------------------------------------------------
// the following IPC message is compatible with xlog remote data spec.
//---------------------------------------------------------------------

#if !defined(__WOE32__) && !defined(__APPLE__)

#define addtomsg(x, y)	log_msg = log_msg + (x) + (y) + LOG_MSEPARATOR

static void send_IPC_log(cQsoRec &rec)
{
	msgtype msgbuf;
	const char   LOG_MSEPARATOR[2] = {1,0};
	int msqid, len;

	int mm, dd, yyyy;
	char szdate[9];
	char sztime[5];
	strncpy(szdate, rec.getField(QSO_DATE_OFF), 8);
	szdate[8] = 0;
	sscanf(&szdate[6], "%d", &dd); szdate[6] = 0;
	sscanf(&szdate[4], "%d", &mm); szdate[4] = 0;
	sscanf(szdate, "%d", &yyyy);
	Date logdate(mm, dd, yyyy);

	log_msg.clear();
	log_msg = string("program:") + PACKAGE_NAME + string(" v ") + PACKAGE_VERSION + LOG_MSEPARATOR;
	addtomsg("version:",	LOG_MVERSION);
	addtomsg("date:",		logdate.szDate(5));
	memset(sztime, 0, sizeof(sztime) / sizeof(char));
	strncpy(sztime, rec.getField(TIME_ON), (sizeof(sztime) / sizeof(char)) - 1);
	addtomsg("TIME:",		sztime);
	memset(sztime, 0, 5);
	strncpy(sztime, rec.getField(TIME_OFF), 4);
	addtomsg("endtime:",            sztime);
	addtomsg("call:",		rec.getField(CALL));
	addtomsg("mhz:",		rec.getField(FREQ));
	addtomsg("mode:",		adif2export(rec.getField(MODE)));
	addtomsg("submode:",	adif2submode(rec.getField(MODE)));
	addtomsg("tx:",			rec.getField(RST_SENT));
	addtomsg("rx:",			rec.getField(RST_RCVD));
	addtomsg("name:",		rec.getField(NAME));
	addtomsg("qth:",		rec.getField(QTH));
	addtomsg("state:",		rec.getField(STATE));
	addtomsg("province:",	        rec.getField(VE_PROV));
	addtomsg("country:",	        rec.getField(COUNTRY));
	addtomsg("locator:",	        rec.getField(GRIDSQUARE));
	addtomsg("serialout:",	        rec.getField(STX));
	addtomsg("serialin:",	        rec.getField(SRX));
	addtomsg("free1:",		rec.getField(XCHG1));
	notes = rec.getField(NOTES);
	for (size_t i = 0; i < notes.length(); i++)
	    if (notes[i] == '\n') notes[i] = ';';
	addtomsg("notes:",		notes.c_str());
	addtomsg("power:",		rec.getField(TX_PWR));

	strncpy(msgbuf.mtext, log_msg.c_str(), sizeof(msgbuf.mtext));
	msgbuf.mtext[sizeof(msgbuf.mtext) - 1] = '\0';

	if ((msqid = msgget(LOG_MKEY, 0666 | IPC_CREAT)) == -1) {
		LOG_PERROR("msgget");
		return;
	}
	msgbuf.mtype = LOG_MTYPE;
	len = strlen(msgbuf.mtext) + 1;
	if (msgsnd(msqid, &msgbuf, len, IPC_NOWAIT) < 0)
		LOG_PERROR("msgsnd");
}

#endif

void submit_record(cQsoRec &rec)
{
#if !defined(__WOE32__) && !defined(__APPLE__)
	send_IPC_log(rec);
#endif
	submit_ADIF(rec);
	if (progdefaults.submit_lotw)
		submit_lotw(rec);
	if (progdefaults.eqsl_when_logged)
		submit_eQSL(rec, "");
	n3fjp_add_record(rec);
}

//---------------------------------------------------------------------

extern void xml_add_record();

void submit_log(void)
{
	if (progdefaults.spot_when_logged) {
		if (!dxcluster_viewer->visible()) dxcluster_viewer->show();
		send_DXcluster_spot();
	}

	if (progStatus.spot_log)
		spot_log(inpCall->value(), inpLoc->value());

	logmode = mode_info[active_modem->get_mode()].adif_name;

	if (progdefaults.xml_logbook && qsodb.isdirty()) {
		xml_add_record();
		qsodb.isdirty(0);
	} else
		AddRecord();

	if (FD_logged_on) FD_add_record();

//	if (progdefaults.eqsl_when_logged)
//		makeEQSL("");

}

//======================================================================
// thread to support sending log entry to eQSL
//======================================================================

pthread_t* EQSLthread = 0;
pthread_mutex_t EQSLmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t EQSLcond = PTHREAD_COND_INITIALIZER;

static void *EQSL_loop(void *args);
static void EQSL_init(void);

void EQSL_close(void);
void EQSL_send();

static std::string EQSL_url = "";
static std::string SEND_url = "";
static std::string EQSL_xmlpage = "";

static bool EQSLEXIT = false;

void update_eQSL_fields(void *)
{
	std::string call;
	std::string date;
	std::string mode = "MODE";

//std::cout << "update_eQSL_fields" << std::endl;
//std::cout << EQSL_url << std::endl;

	size_t p = EQSL_url.find("<CALL:");
	if (p == std::string::npos) return;
	p = EQSL_url.find(">", p);
	if (p == std::string::npos) return;
	p++;
	size_t p2 = EQSL_url.find("<", p);
	if (p2 == std::string::npos) return;
	call = EQSL_url.substr(p, p2 - p);
	p = EQSL_url.find("<QSO_DATE:");
	if (p == std::string::npos) return;
	p = EQSL_url.find(">", p);
	if (p == std::string::npos) return;
	p++;
	p2 = EQSL_url.find("<", p);
	if (p2 == std::string::npos) return;
	date = EQSL_url.substr(p, p2 - p);
	p = EQSL_url.find("<MODE:");
	if (p != std::string::npos) {
		p = EQSL_url.find(">", p);
		if (p != std::string::npos) {
			p2 = EQSL_url.find("<", ++p);
			if (p2 != std::string::npos)
				mode = EQSL_url.substr(p, p2 - p);
		}
	}
	p = EQSL_url.find("<SUBMODE:");
	if (p != std::string::npos) {
		p = EQSL_url.find(">", p);
		if (p != std::string::npos) {
			p2 = EQSL_url.find("<", ++p);
			if (p2 != std::string::npos) {
				std::string submode = EQSL_url.substr(p, p2 - p);
				if (!submode.empty()) mode = submode;
			}
		}
	}
	inpEQSLsentdate_log->value(date.c_str());
	updateRecord();

	std::string qsl_logged = "eQSL logged: ";
	qsl_logged.append(call).append(" on ").append(mode);

	if (progdefaults.eqsl_show_delivery) {
		if (!alert_window) alert_window = new notify_dialog;
		alert_window->notify(qsl_logged.c_str(), 5.0);
		REQ(show_notifier, alert_window);
	}

	LOG_INFO("%s", qsl_logged.c_str());
}

static void cannot_connect_to_eqsl(void *)
{
	std::string msg = "Cannot connect to www.eQSL.cc";

	if (!alert_window) alert_window = new notify_dialog;
	alert_window->notify(msg.c_str(), 5.0);
	REQ(show_notifier, alert_window);
}

static void eqsl_error(void *)
{
	size_t p = EQSL_xmlpage.find("Error:");
	size_t p2 = EQSL_xmlpage.find('\n', p);
	std::string errstr = "eQSL error:";
	errstr.append("testing 1.2.3");
	errstr.append(EQSL_xmlpage.substr(p, p2 - p - 1));
	errstr.append("\n").append(EQSL_url.substr(0,40));

	if (!alert_window) alert_window = new notify_dialog;
	alert_window->notify(errstr.c_str(), 5.0);
	REQ(show_notifier, alert_window);
	LOG_ERROR("%s", errstr.c_str());
}

static void *EQSL_loop(void *args)
{
	SET_THREAD_ID(EQSL_TID);

	SET_THREAD_CANCEL();

	for (;;) {
		TEST_THREAD_CANCEL();
		pthread_mutex_lock(&EQSLmutex);
		pthread_cond_wait(&EQSLcond, &EQSLmutex);
		pthread_mutex_unlock(&EQSLmutex);

		if (EQSLEXIT)
			return NULL;

		size_t p;
		if (get_http(SEND_url, EQSL_xmlpage, 5.0) <= 0) {
			REQ(cannot_connect_to_eqsl, (void *)0);
		}
		else if ((p = EQSL_xmlpage.find("Error:")) != std::string::npos) {
			REQ(eqsl_error, (void *)0);
		} else {
			REQ(update_eQSL_fields, (void *)0);
		}

	}
	return NULL;
}

void EQSL_close(void)
{
	ENSURE_THREAD(FLMAIN_TID);

	if (!EQSLthread)
		return;

	CANCEL_THREAD(*EQSLthread);

	pthread_mutex_lock(&EQSLmutex);
	EQSLEXIT = true;
	pthread_cond_signal(&EQSLcond);
	pthread_mutex_unlock(&EQSLmutex);

	pthread_join(*EQSLthread, NULL);
	delete EQSLthread;
	EQSLthread = 0;
}

static void EQSL_init(void)
{
	ENSURE_THREAD(FLMAIN_TID);

	if (EQSLthread)
		return;
	EQSLthread = new pthread_t;
	EQSLEXIT = false;
	if (pthread_create(EQSLthread, NULL, EQSL_loop, NULL) != 0) {
		LOG_PERROR("pthread_create");
		return;
	}
	MilliSleep(10);
}

void submit_eQSL(cQsoRec &rec, string msg)
{
	string eQSL_data;
	string tempstr;
	char sztemp[100];

	eQSL_data = "upload <adIF_ver:4>4.0 ";
	snprintf(sztemp, sizeof(sztemp),"<EQSL_USER:%d>%s<EQSL_PSWD:%d>%s",
		static_cast<int>(progdefaults.eqsl_id.length()),
		ucasestr(progdefaults.eqsl_id).c_str(),
		static_cast<int>(progdefaults.eqsl_pwd.length()),
		progdefaults.eqsl_pwd.c_str());
	eQSL_data.append(sztemp);
	eQSL_data.append("<PROGRAMID:6>FLDIGI<EOH>");
	if (!progdefaults.eqsl_nick.empty()) {
		snprintf(sztemp, sizeof(sztemp), "<APP_EQSL_QTH_NICKNAME:%d>%s",
			static_cast<int>(progdefaults.eqsl_nick.length()),
			progdefaults.eqsl_nick.c_str());
		eQSL_data.append(sztemp);
	}

	putadif(CALL, rec.getField(CALL), eQSL_data);
	putadif(MODE, adif2export(rec.getField(MODE)).c_str(), eQSL_data);
	putadif(SUBMODE, adif2submode(rec.getField(MODE)).c_str(), eQSL_data);
	tempstr = rec.getField(FREQ);
	tempstr.resize(7);
	putadif(FREQ, tempstr.c_str(), eQSL_data);
	putadif(BAND, rec.getField(BAND), eQSL_data);
	putadif(QSO_DATE, sDate_on.c_str(), eQSL_data);
	tempstr = rec.getField(TIME_ON);
	if (tempstr.length() > 4) tempstr.erase(4);
	putadif(TIME_ON, tempstr.c_str(), eQSL_data);
	putadif(RST_SENT, rec.getField(RST_SENT), eQSL_data);

	size_t p = 0;
	if (msg.empty()) msg = progdefaults.eqsl_default_message;
	if (!msg.empty()) {
// replace message tags {CALL}, {NAME}, {MODE}
		while ((p = msg.find("{CALL}")) != std::string::npos)
			msg.replace(p, 6, inpCall->value());
		while ((p = msg.find("{NAME}")) != std::string::npos)
			msg.replace(p, 6, inpName->value());
		while ((p = msg.find("{MODE}")) != std::string::npos) {
			tempstr.assign(mode_info[active_modem->get_mode()].export_mode);
			tempstr.append("/").append(mode_info[active_modem->get_mode()].export_submode);
			msg.replace(p, 6, tempstr);
		}
		snprintf(sztemp, sizeof(sztemp), "<QSLMSG:%d>%s",
			static_cast<int>(msg.length()),
			msg.c_str());
		eQSL_data.append(sztemp);
	}
	eQSL_data.append("<EOR>\n");

	std::string eQSL_header = progdefaults.eqsl_www_url;

	EQSL_url.assign(eQSL_header).append(eQSL_data);

	tempstr.clear();
	for (size_t n = 0; n < eQSL_data.length(); n++) {
		if (eQSL_data[n] == ' ' || eQSL_data[n] == '\n') tempstr.append("%20");
		else if (eQSL_data[n] == '<') tempstr.append("%3C");
		else if (eQSL_data[n] == '>') tempstr.append("%3E");
		else if (eQSL_data[n] == '_') tempstr.append("%5F");
		else if (eQSL_data[n] == ':') tempstr.append("%3A");
		else if (eQSL_data[n] > ' ' && eQSL_data[n] <= '}')
			tempstr += eQSL_data[n];
	}

	eQSL_data = eQSL_header;
	eQSL_data.append(tempstr);

	sendEQSL(eQSL_data.c_str());

}

void sendEQSL(const char *url)
{
	ENSURE_THREAD(FLMAIN_TID);

	if (!EQSLthread)
		EQSL_init();

	pthread_mutex_lock(&EQSLmutex);
	SEND_url = url;
	pthread_cond_signal(&EQSLcond);
	pthread_mutex_unlock(&EQSLmutex);
}

// this function may be called from several places including macro
// expansion and execution

void makeEQSL(const char *message)
{
	cQsoRec *rec;
	if (qsodb.nbrRecs() <= 0) return;
	rec = qsodb.getRec(qsodb.nbrRecs() - 1); // last record
	submit_eQSL(*rec, message);
}
