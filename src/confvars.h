#ifndef CONFVAR
#define CONFVAR(def, ...)
#endif

// Please keep this sorted
// See Notes/CONFIG for a better explanation
// Fields: CFG_NAME, TYPE, DEFAULT VALUE, DEFAULT ON VAL, NAMES ARRAY

CONFVAR(AUTOFIX,	BOOL,	"off",		{"autofix"})
CONFVAR(AUTOPOPULATE,	BOOL,	"off",		{"auto_populate", \
						 "auto-populate", \
						 "autopopulate"})
CONFVAR(BAM,		SIZE,	"off",		{"BAM"})
CONFVAR(BAM_HARDLINKS,	BOOL,	"off",		{"BAM_hardlinks", \
						 "binpool_hardlinks"})
CONFVAR(BKD_GZIP,	INT,	"0",		{"bkd_gzip"})
CONFVAR(BKWEB,		STR,	0,		{"bkweb"})
CONFVAR(CATEGORY,	STR,	0,		{"category"})
CONFVAR(CHECK_FREQUENCY,SIZE,	0,		{"check_frequency"})
CONFVAR(CLOCK_SKEW,	STR,	"on",		{"clock_skew", "trust_window"})
CONFVAR(CLONE_DEFAULT,	STR,	"ALL",		{"clone_default", \
						 "clone-default"})
CONFVAR(CONTACT,	STR,	0,		{"contact"})
CONFVAR(COMPRESSION,	STR,	"gzip",		{"compression"})
CONFVAR(DESCRIPTION,	STR,	0,		{"description"})
CONFVAR(DIFFGAP,	INT,	"-1",		{"diffgap"})
CONFVAR(EMAIL,		STR,	0,		{"email"})
CONFVAR(EOLN,		STR,	"native",	{"eoln"})
CONFVAR(FAKEGRAFTS,	BOOL,	"off",		{"fakegrafts"})
CONFVAR(HOMEPAGE,	STR,	0,		{"homepage"})
CONFVAR(KEYWORD,	STR,	"none",		{"keyword"})
CONFVAR(LEGACYGUIS,	BOOL,	"off",		{"legacyguis"})
CONFVAR(LICENSE,	STR,	0,		{"license"})
CONFVAR(LICENSEURL,	STR,	0,		{"licenseurl"})
CONFVAR(LOCKWAIT,	INT,	"30",		{"lockwait"})
CONFVAR(MAIL_PROXY,	STR,	0,		{"mail_proxy", "mail-proxy"})
CONFVAR(MASTER,		STR,	0,		{"master"})
CONFVAR(MONOTONIC,	STR,	0,		{"monotonic"})
CONFVAR(NOGRAPHVERIFY,	BOOL,	"off",		{"no_graphverify"})
CONFVAR(PARALLEL,	INT,	0,		{"parallel"})
CONFVAR(PARTIAL_CHECK,	BOOL,	"on",		{"partial_check"})
CONFVAR(POLY,		BOOL,	"off",		{"poly"})
CONFVAR(STATS_AFTER_PULL,BOOL,	"off",		{"stats_after_pull"})
CONFVAR(SYNC,		BOOL,	"off",		{"sync"})
CONFVAR(TRIGGERS,	STR,	"$PRODUCT|.",	{"triggers"})
CONFVAR(UPGRADE_URL,	STR,	0,		{"upgrade_url", "upgrade-url"})
