//########################################################################
//
//  rigxml.cxx
//
//  parse a rig control xml file
//
//  copywrite David Freese, w1hkj@w1hkj.com
//
//########################################################################

#include <config.h>

#include <iostream>
#include <fstream>

#include "rigio.h"
#include "rigxml.h"
#include "rigsupport.h"
#ifdef RIGCATTEST
	#include "rigCAT.h"
#else
	#include "main.h"
#endif

//#define DEBUGXML

void parseRIGDEF(size_t &);
void parseRIG(size_t &);
void parseCOMMAND(size_t &);
void parsePORT(size_t &);
void parseREPLY(size_t &);
void parseMODES(size_t &);
void parseBANDWIDTHS(size_t &);
void parseBWCMD(size_t &);
void parseBWREPLY(size_t &);
void parseMODECMD(size_t &);
void parseMODEREPLY(size_t &);
void parseTITLE(size_t &);
void parseLSBMODES(size_t &);
void parseCOMMENTS(size_t &);
void parseDISCARD(size_t &);

void parseIOSsymbol(size_t &);
void parseIOSsize(size_t &);
void parseIOSbytes(size_t &);
void parseIOSbyte(size_t &);
void parseIOSdata(size_t &);
void parseIOSinfo(size_t &);
void parseIOSok(size_t &);
void parseIOSbad(size_t &);
void parseIOSstring(size_t &);
void parseIOSint(size_t &);
void parseIOSfill(size_t &);

void parseBaud(size_t &);
void parseDevice(size_t &);
void parseEcho(size_t &);
void parseRetries(size_t &);
void parseTimeout(size_t &);
void parseWait(size_t &);
void parseDTRinit(size_t &);
void parseDTRptt(size_t &);
void parseRTSinit(size_t &);
void parseRTSptt(size_t &);
void parseRTSCTS(size_t &);

void parseDTYPE(size_t &);
void parseDSIZE(size_t &);
void parseDMAX(size_t &);
void parseDMIN(size_t &);
void parseDRESOL(size_t &);
void parseDREV(size_t &);
void parseDMAKS(size_t &);
void parseDSHIFT(size_t &);

void print(size_t &);

list<XMLIOS>	commands;
list<XMLIOS>	reply;
list<MODE> 		lmodes;
list<BW> 		lbws;
list<BW>		lbwCMD;
list<BW>		lbwREPLY;
list<MODE>		lmodeCMD;
list<MODE>		lmodeREPLY;
list<string> 	LSBmodes;

XMLRIG rig;

XMLIOS iosTemp;

string strXML;

TAGS rigdeftags[] = {
	{"<RIGDEF",		parseRIGDEF},
	{"<RIG", 		parseRIG},
	{"<COMMAND",	parseCOMMAND},
	{"<PORT",		parsePORT},
	{"<REPLY",		parseREPLY},
	{"<BANDWIDTHS", parseBANDWIDTHS},
	{"<BW-CMD",		parseBWCMD},
	{"<BW-REPLY",	parseBWREPLY},
	{"<MODES",		parseMODES},
	{"<MODE-CMD",	parseMODECMD},
	{"<MODE-REPLY", parseMODEREPLY},
	{"<TITLE",		parseTITLE},
	{"<LSBMODES",	parseLSBMODES},
	{"<!--",        parseCOMMENTS},
	{"<PROGRAMMER", parseDISCARD},
	{"<STATUS",		parseDISCARD},
	{0, 0} 
};

TAGS commandtags[] = {
	{"<SIZE",	parseIOSsize},
	{"<SYMBOL",	parseIOSsymbol},
	{"<BYTES",	parseIOSbytes},
	{"<BYTE",	parseIOSbyte},
	{"<DATA",	parseIOSdata},
	{"<STRING", parseIOSstring},
	{"<INT", 	parseIOSint},
	{"<INFO",	parseIOSinfo},
	{"<OK",		parseIOSok},
	{"<BAD",	parseIOSbad},
	{0,0}
};

TAGS replytags[] = {
	{"<SIZE",	parseIOSsize},
	{"<SYMBOL",	parseIOSsymbol},
	{"<BYTES",	parseIOSbytes},
	{"<BYTE",	parseIOSbyte},
	{"<DATA",	parseIOSdata},
	{"<STRING", parseIOSstring},
	{"<INT",	parseIOSint},
	{"<FILL",	parseIOSfill},
	{0,0}
};

TAGS porttags[] = {
	{"<BAUD",	parseBaud},
	{"<DEVICE",	parseDevice},
	{"<ECHO",	parseEcho},
	{"<RETRIES",parseRetries},
	{"<TIMEOUT",parseTimeout},
	{"<WAIT",	parseWait},
	{"<DTRINIT",parseDTRinit},
	{"<DTRPTT",	parseDTRptt},
	{"<RTSINIT",parseRTSinit},
	{"<RTSPTT",	parseRTSptt},
	{"<RTSCTS",	parseRTSCTS},
	{0,0}
};

TAGS datatags[] = {
	{"<DTYPE",	parseDTYPE},
	{"<SIZE",	parseDSIZE},
	{"<MAX",	parseDMAX},
	{"<MIN",	parseDMIN},
	{"<RESOL",	parseDRESOL},
	{"<REV",	parseDREV},
	{"<MASK",	parseDMAKS},
	{"<SHIFT",	parseDSHIFT},
	{0,0}
}
;

//=====================================================================

void print(size_t &p0, int indent)
{
#ifdef DEBUGXML
	size_t tend = strXML.find(">", p0);
	for (int i = 0; i < indent; i++)
		std::cout << "\t";
	std::cout << strXML.substr(p0, tend - p0 + 1).c_str() << std::endl;
#endif
}

size_t tagEnd(size_t p0)
{
	size_t p1, p2, p3;
	p1 = p0;
	string strtag = "</";
	p2 = strXML.find(">", p0);
	p3 = strXML.find(" ", p0);
	if (p2 == string::npos) {
		return p2;
	}
	if (p3 < p2)
		p2 = p3;
	strtag.append(strXML.substr(p1 + 1, p2 - p1 - 1));
	strtag.append(">");
	p3 = strXML.find(strtag, p1);
	return p3;
}

size_t nextTag(size_t p0)
{
	p0 = strXML.find("<", p0+1);
	return p0;
}

string getElement(size_t p0)
{
	size_t p1 = strXML.find(">",p0),
		   p2 = nextTag(p1+1);
	if (p1 == string::npos || p2 == string::npos)
		return "";
	p1++; p2--;
	while (p1 < p2 && strXML[p1] == ' ') p1++; // skip leading spaces
	while (p1 < p2 && strXML[p2] == ' ') p2--; // skip trailing spaces
	return strXML.substr(p1, p2 - p1 + 1);
}

int getInt(size_t p0)
{
	string stemp = getElement(p0);
	if (stemp.length() == 0)
		return 0;
	return atoi(stemp.c_str());
}

float getFloat(size_t p0)
{
	string stemp = getElement(p0);
	if (stemp.length() == 0)
		return 0;
	return atof(stemp.c_str());
}

bool getBool( size_t p0)
{
	string stemp = getElement(p0);
	if (stemp.length() == 0)
		return false;
	if (strcasecmp(stemp.c_str(), "true") == 0)
		return true;
	return false;
}

char getByte(size_t p0)
{
	unsigned int val;
	if (sscanf( getElement(p0).c_str(), "%x", &val ) != 1)
		return 0;
	return (val & 0xFF);
}

string getBytes(size_t p0)
{
	unsigned int val;
	size_t space;
	string stemp = getElement(p0);
	string s;
	while ( stemp.length() ) {
		if (sscanf( stemp.c_str(), "%x", &val) != 1) {
			s = "";
			return s;
		}
		s += (char)(val & 0xFF);
		space = stemp.find(" ");
		if (space == string::npos) break;
		stemp.erase(0, space + 1);
	}
	return s;
}

bool isInt(size_t p0, int &i)
{
//	p0 = nextTag(p0);
	if (strXML.find("<INT", p0) != p0)
		return false;
	i = getInt(p0);
	return true;
}

bool isByte(size_t p0, char &ch)
{
//	p0 = nextTag(p0);
	if (strXML.find("<BYTE", p0) != p0)
		return false;
	ch = getByte(p0);
	return true;
}

bool isBytes( size_t p0, string &s )
{
//	p0 = nextTag(p0);
	if (strXML.find ("<BYTES", p0) != p0)
		return false;
	s = getBytes(p0);
	return true;
}

bool isString( size_t p0, string &s )
{
//	p0 = nextTag(p0);
	if (strXML.find("<STRING", p0) != p0)
		return false;
	s = getElement(p0);
	return true;
}

bool isSymbol( size_t p0, string &s)
{
	if (strXML.find("<SYMBOL", p0) != p0)
		return false;
	s = getElement(p0);
	return true;
}

bool tagIs(size_t &p0, string tag)
{
	return (strXML.find(tag,p0) == p0);
}

//---------------------------------------------------------------------
// Parse modesTO definitions
//---------------------------------------------------------------------

void parseMODEdefs(size_t &p0, list<MODE> &lmd)
{
	size_t pend = tagEnd(p0);
	size_t elend;
	char ch;
	int n;
	string stemp;
	string strELEMENT;
	if (pend == string::npos) {
		p0++;
		return;
	}
	print(p0,0);
	p0 = nextTag(p0);
	while (p0 != string::npos && p0 < pend && tagIs(p0, "<ELEMENT")) {
		elend = tagEnd(p0);
		p0 = nextTag(p0);
		if (isSymbol(p0, strELEMENT)) {
			p0 = tagEnd(p0);
			p0 = nextTag(p0);
			while (p0 != string::npos && p0 < elend) {
				print(p0,1);
				if ( isBytes(p0, stemp) ) {
					lmd.push_back(MODE(strELEMENT,stemp));
				}
				else if ( isByte(p0, ch) ) {
					stemp = ch;
					lmd.push_back(MODE(strELEMENT,stemp));
				}
				else if ( isInt(p0, n) ) {
					stemp = (char)(n & 0xFF);
					lmd.push_back(MODE(strELEMENT, stemp));
				}
				else if ( isString(p0, stemp) ) {
					lmd.push_back(MODE(strELEMENT,stemp));
				}
				p0 = tagEnd(p0);
				p0 = nextTag(p0);
			}
		}
		p0 = nextTag(p0);
	}
	p0 = pend;
}

void parseMODES(size_t &p0)
{
	parseMODEdefs(p0, lmodes);
}


void parseMODECMD(size_t &p0)
{
	parseMODEdefs(p0, lmodeCMD);
}

void parseMODEREPLY(size_t &p0)
{
	parseMODEdefs(p0, lmodeREPLY);
}

void parseLSBMODES(size_t &p0)
{
	size_t pend = tagEnd(p0);
	string sMode;
	print(p0,0);
	p0 = nextTag(p0);
	while (p0 < pend && isString(p0, sMode)) {
		LSBmodes.push_back(sMode);
		print (p0,1);
		p0 = tagEnd(p0);
		p0 = nextTag(p0);
	}
	p0 = pend;
}

//---------------------------------------------------------------------
// Parse Bandwidth definitions
//---------------------------------------------------------------------

void parseBWdefs(size_t &p0, list<BW> &lbw)
{
	size_t pend = tagEnd(p0);
	size_t elend;
	char ch;
	int n;
	string strELEMENT;
	string stemp;
	if (pend == string::npos) {
		p0++;
		return;
	}
	print(p0,0);
	p0 = nextTag(p0);
	while (p0 != string::npos && p0 < pend && tagIs(p0, "<ELEMENT")) {
		elend = tagEnd(p0);
		p0 = nextTag(p0);
		if (isSymbol(p0, strELEMENT)) {
			p0 = tagEnd(p0);
			p0 = nextTag(p0);
			while (p0 != string::npos && p0 < elend) {
				print(p0,1);
				if ( isBytes(p0, stemp) ) {
					lbw.push_back(BW(strELEMENT,stemp));
				}
				else if ( isByte(p0, ch) ) {
					stemp = ch;
					lbw.push_back(BW(strELEMENT,stemp));
				}
				else if ( isInt(p0, n) ) {
					stemp = (char)(n & 0xFF);
					lbw.push_back(BW(strELEMENT, stemp));
				}
				else if ( isString(p0, stemp) ) {
					lbw.push_back(BW(strELEMENT,stemp));
				}
				p0 = tagEnd(p0);
				p0 = nextTag(p0);
			}
		}
		p0 = nextTag(p0);
	}
	p0 = pend;
}

void parseBANDWIDTHS(size_t &p0)
{
	parseBWdefs(p0, lbws);
}

void parseBWCMD(size_t &p0)
{
	parseBWdefs(p0, lbwCMD);
}

void parseBWREPLY(size_t &p0)
{
	parseBWdefs(p0, lbwREPLY);
}

//---------------------------------------------------------------------
// Parse Title definition
//---------------------------------------------------------------------

void parseTITLE(size_t &p0)
{
	size_t pend = tagEnd(p0);
	windowTitle = getElement(p0);
	p0 = pend;
}

//---------------------------------------------------------------------
// Parse Rig definition
//---------------------------------------------------------------------

void parseRIG(size_t &p0)
{
	size_t pend = tagEnd(p0);
	rig.SYMBOL = getElement(p0);
	p0 = pend;
}

//---------------------------------------------------------------------
// Parse Port definition
//---------------------------------------------------------------------

void parseBaud(size_t &p1)
{
	int baud;
	baud = atoi (getElement (p1).c_str());
	rig.baud = baud;
}

void parseDevice(size_t &p1)
{
	string stemp = getElement(p1);
	rig.port = stemp;
}

void parseEcho(size_t &p1)
{
	rig.echo = getBool(p1);
}

void parseRetries(size_t &p1)
{
	rig.retries = getInt(p1);
}

void parseTimeout(size_t &p1)
{
	rig.timeout = getInt(p1);
}

void parseWait(size_t &p1)
{
	rig.wait = getInt(p1);
}

void parseDTRinit(size_t &p1)
{
	int val = getInt(p1);
	if (val > 0)
		rig.dtr = true;
	else
		rig.dtr = false;
}

void parseDTRptt(size_t &p1)
{
	rig.dtrptt = getBool(p1);
}

void parseRTSinit(size_t &p1)
{
	int val = getInt(p1);
	if (val > 0)
		rig.rts = true;
	else
		rig.rts = false;
}

void parseRTSptt(size_t &p1)
{
	rig.rtsptt = getBool(p1);
}

void parseRTSCTS(size_t &p1)
{
	rig.rtscts = getBool(p1);
}

//===================================================
void parsePORT(size_t &p0)
{
	size_t pend = tagEnd(p0);
	size_t p1;
	TAGS *pv;

	print(p0,0);

	rig.clear();
	p1 = nextTag(p0);
	while (p1 < pend) {
		pv = porttags;
		while (pv->tag) {
			if (strXML.find(pv->tag, p1) == p1) {
				print(p1, 1);
				if (pv->fp) 
					(pv->fp)(p1);
				break;
			}
			pv++;
		}
		p1 = tagEnd(p1);
		p1 = nextTag(p1);
	}
	if (rig.port.size()) {
		rigio.Baud( rig.baud );
		rigio.Device( rig.port );
		rigio.Retries(rig.retries);
		rigio.Timeout(rig.timeout);
		rigio.RTS(rig.rts);
		rigio.RTSptt(rig.rtsptt);
		rigio.DTR(rig.dtr);
		rigio.DTRptt(rig.dtrptt);
		rigio.RTSCTS(rig.rtscts);
	}
	p0 = pend;
#ifdef DEBUGXML
	std::cout << "port: " << rig.port.c_str() << std::endl;
	std::cout << "baud: " << rig.baud << std::endl;
	std::cout << "retries: " << rig.retries << std::endl;
	std::cout << "timeout: " << rig.timeout << std::endl;
	std::cout << "initial rts: " << (rig.rts ? "+12" : "-12") << std::endl;
	std::cout << "use rts ptt: " << (rig.rtsptt ? "T" : "F") << std::endl;
	std::cout << "initial dts: " << (rig.dtr ? "+12" : "-12") << std::endl;
	std::cout << "use dtr ptt: " << (rig.dtrptt ? "T" : "F") << std::endl;
	std::cout << "use flowcontrol: " << (rig.rtscts ? "T" : "F") << std :: endl;
#endif	
}

//---------------------------------------------------------------------
// Parse IOS (serial stream format) definitions
//---------------------------------------------------------------------

void parseIOSsize(size_t &p0)
{
	iosTemp.size = getInt(p0);
}

void parseIOSbytes(size_t &p0)
{
	if (iosTemp.data.size == 0)
		iosTemp.str1.append(getBytes(p0));
	else
		iosTemp.str2.append(getBytes(p0));
}

void parseIOSbyte(size_t &p0)
{
	if (iosTemp.data.size == 0)
		iosTemp.str1 += getByte(p0);
	else
		iosTemp.str2 += getByte(p0);
}

void parseIOSstring(size_t &p0)
{
	if (iosTemp.data.size == 0)
		iosTemp.str1 += getElement(p0);
	else
		iosTemp.str2 += getElement(p0);
}

void parseIOSint(size_t &p0)
{
	if (iosTemp.data.size == 0)
		iosTemp.str1 += (char)(getInt(p0) & 0xFF);
	else
		iosTemp.str2 += (char)(getInt(p0) & 0xFF);
}

void parseDTYPE(size_t &p1)
{
	print(p1,2);
	iosTemp.data.dtype = getElement(p1);
}

void parseDSIZE(size_t &p1)
{
	print(p1,2);
	iosTemp.data.size = getInt(p1);
}

void parseDMAX(size_t &p1)
{
	print(p1,2);
	iosTemp.data.max = getInt(p1);
}

void parseDMIN(size_t &p1)
{
	print(p1,2);
	iosTemp.data.min = getInt(p1);
}

void parseDRESOL(size_t &p1)
{
	print(p1,2);
	iosTemp.data.resolution = getFloat(p1);
}

void parseDREV(size_t &p1)
{
	print(p1,2);
	iosTemp.data.reverse = getBool(p1);
}

void parseDMAKS(size_t &p1)
{
	print(p1,2);
	iosTemp.data.andmask = getInt(p1);
}

void parseDSHIFT(size_t &p1)
{
	print(p1,2);
	iosTemp.data.shiftbits = getInt(p1);
}

void parseIOSdata(size_t &p0)
{
	size_t pend = tagEnd(p0);
	size_t p1;
	TAGS *pv;

	p1 = nextTag(p0);
	while (p1 < pend) {
		pv = datatags;
		while (pv->tag) {
			if (strXML.find(pv->tag, p1) == p1) {
				print(p1, 1);
				if (pv->fp) 
					(pv->fp)(p1);
				break;
			}
			pv++;
		}
		p1 = tagEnd(p1);
		p1 = nextTag(p1);
	}
}

void parseIOSinfo(size_t &p0)
{
	string strR = getElement(p0);
	if (strR.empty()) return;
	iosTemp.info = strR;
}

void parseIOSok(size_t &p0)
{
	string strR = getElement(p0);
	if (strR.empty()) return;
	iosTemp.ok = strR;
}

void parseIOSbad(size_t &p0)
{
	string strR = getElement(p0);
	if (strR.empty()) return;
	iosTemp.bad = strR;
}

void parseIOSsymbol(size_t &p0)
{
	string strR = getElement(p0);
	if (strR.empty()) return;
	iosTemp.SYMBOL = strR;
}

void parseIOSfill(size_t &p0)
{
	if (iosTemp.data.size == 0)
		iosTemp.fill1 = getInt(p0);
	else
		iosTemp.fill2 = getInt(p0);
}

//=======================================================================

bool parseIOS(size_t &p0, TAGS *valid)
{
	size_t pend = tagEnd(p0);
	size_t p1;
	TAGS *pv;

	print(p0,0);

	iosTemp.clear();
	p1 = nextTag(p0);
	while (p1 < pend) {
		pv = valid;
		while (pv->tag) {
			if (strXML.find(pv->tag, p1) == p1) {
				print(p1, 1);
				if (pv->fp) 
					(pv->fp)(p1);
				break;
			}
			pv++;
		}
		p1 = tagEnd(p1);
		p1 = nextTag(p1);
//		if (pv->tag == 0) p1 = pend;
	}
	p0 = pend;
	return (!iosTemp.SYMBOL.empty());
}

void parseCOMMAND(size_t &p0)
{
	if (parseIOS(p0, commandtags))
		commands.push_back(iosTemp);
}

void parseREPLY(size_t &p0)
{
	if (parseIOS(p0, replytags))
		reply.push_back(iosTemp);
}

void parseCOMMENTS(size_t &p0)
{
	p0 = strXML.find("-->",p0);
}

void parseRIGDEF(size_t &p0)
{
	p0 = nextTag(p0);
}

void parseDISCARD(size_t &p0)
{
	size_t pend = tagEnd(p0);
	if (pend == string::npos) p0++;
	else p0 = pend;
}

void parseXML()
{
	size_t p0;
	p0 = 0;
	p0 = nextTag(p0);
	TAGS *pValid;
	while (p0 != string::npos) {
		pValid = rigdeftags;
		while (pValid->tag) {
			if (strXML.find(pValid->tag, p0) == p0)
				break;
			pValid++;
		}
		if (pValid->tag)
			(pValid->fp)(p0);
		else
			parseDISCARD(p0);
		p0 = nextTag(p0);
	}
}

bool readRigXML()
{
	char szLine[256];
	int lines = 0;

	commands.clear();
	reply.clear();
	lmodes.clear();
	lmodeCMD.clear();
	lmodeREPLY.clear();
	lbws.clear();
	lbwCMD.clear();
	lbwREPLY.clear();
	LSBmodes.clear();
	strXML = "";

	ifstream xmlfile(xmlfname.c_str(), ios::in);
	if (xmlfile) {
		while (!xmlfile.eof()) {
			lines++;
			memset(szLine, 0, sizeof(szLine));
			xmlfile.getline(szLine,255);
			strXML.append(szLine);
		}
		xmlfile.close();
		parseXML();
		return true;
	}
	return false;
}

