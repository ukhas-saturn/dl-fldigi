// generated by Fast Light User Interface Designer (fluid) version 1.0304

#ifndef notifydialog_h
#define notifydialog_h
#include <FL/Fl.H>
#include "table.h"
#include "flinput2.h"
#include "flslider2.h"
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
extern Fl_Group *grpNotifyEvent;
#include <FL/Fl_Choice.H>
extern Fl_Choice *mnuNotifyEvent;
extern Fl_Input2 *inpNotifyRE;
#include <FL/Fl_Light_Button.H>
extern Fl_Light_Button *btnNotifyEnabled;
extern Fl_Group *grpNotifyFilter;
#include <FL/Fl_Round_Button.H>
extern Fl_Round_Button *chkNotifyFilterCall;
extern Fl_Input2 *inpNotifyFilterCall;
extern Fl_Round_Button *chkNotifyFilterDXCC;
#include <FL/Fl_Button.H>
extern Fl_Button *btnNotifyFilterDXCC;
#include <FL/Fl_Check_Button.H>
extern Fl_Check_Button *chkNotifyFilterNWB;
extern Fl_Check_Button *chkNotifyFilterLOTW;
extern Fl_Check_Button *chkNotifyFilterEQSL;
extern Fl_Group *grpNotifyDup;
extern Fl_Check_Button *chkNotifyDupIgnore;
extern Fl_Choice *mnuNotifyDupWhich;
extern Fl_Spinner2 *cntNotifyDupTime;
extern Fl_Check_Button *chkNotifyDupBand;
extern Fl_Check_Button *chkNotifyDupMode;
extern Fl_Group *grpNotifyAction;
extern Fl_Spinner2 *cntNotifyActionLimit;
extern Fl_Input2 *inpNotifyActionDialog;
extern Fl_Button *btnNotifyActionDialogDefault;
extern Fl_Spinner2 *cntNotifyActionDialogTimeout;
extern Fl_Input2 *inpNotifyActionRXMarker;
extern Fl_Button *btnNotifyActionMarkerDefault;
extern Fl_Input2 *inpNotifyActionMacro;
extern Fl_Button *btnNotifyActionMacro;
extern Fl_Input2 *inpNotifyActionProgram;
extern Fl_Button *btnNotifyActionProgram;
extern Fl_Button *btnNotifyAdd;
extern Fl_Button *btnNotifyRemove;
extern Fl_Button *btnNotifyUpdate;
extern Fl_Button *btnNotifyTest;
extern Fl_Button *btnNotifyClose;
extern Table *tblNotifyList;
Fl_Double_Window* make_notify_window();
extern Table *tblNotifyFilterDXCC;
extern Fl_Input2 *inpNotifyDXCCSearchCountry;
extern Fl_Input2 *inpNotifyDXCCSearchCallsign;
extern Fl_Button *btnNotifyDXCCSelect;
extern Fl_Button *btnNotifyDXCCDeselect;
extern Fl_Button *btnNotifyDXCCClose;
Fl_Double_Window* make_dxcc_window();
#endif
