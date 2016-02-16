#ifndef CONFVAR
#define CONFVAR(def, ...)
#endif

// Please keep this sorted
// See Notes/CONFIG for a better explanation
// Fields: CFG_NAME, DEFAULT VALUE, IN SETUP DEFAULT, NAMES ARRAY
CONFVAR(AUTOFIX,		"on",		0,	"autofix")
CONFVAR(AUTOPOPULATE,		"off",		"",	"auto_populate")
CONFVAR(BAM,			"off",		"",	"BAM")
CONFVAR(BAM_CHECKOUT,		0,		0,	"BAM_checkout")
CONFVAR(BAM_HARDLINKS,		"off",		0,	"BAM_hardlinks")
CONFVAR(BKD_GZIP,		"0",		0,	"bkd_gzip")
CONFVAR(BKWEB,			0,		0,	"bkweb")
CONFVAR(CATEGORY,		0,		0,	"category")
CONFVAR(CHECK_FREQUENCY,	0,		0,	"check_frequency")


// If this default changes, then src/gui/ide/emacs/vc-bk.el
// will need to be changed as well (specifically, the
// vc-bk-checkout-model function).
CONFVAR(CHECKOUT,		"none",		"edit",	"checkout")

CONFVAR(CLOCK_SKEW,		"on",		"",	"clock_skew")
CONFVAR(CLONE_DEFAULT,		"ALL",		"",	"clone_default")
CONFVAR(CONTACT,		0,		0,	"contact")
CONFVAR(COMPRESSION,		"gzip",		0,	"compression")
CONFVAR(DESCRIPTION,		0,		"",	"description")
CONFVAR(DIFFGAP,		"-1",		0,	"diffgap")
CONFVAR(EMAIL,			0,		"",	"email")
CONFVAR(EOLN,			"native"	,"",	"eoln")
CONFVAR(FIX_MERGE,		"off",		0,	"fix_mergedups")
CONFVAR(FAKEGRAFTS,		"off",		0,	"fakegrafts")
CONFVAR(HOMEPAGE,		0,		0,	"homepage")
CONFVAR(KEYWORD,		"none",		"",	"keyword")
CONFVAR(LEGACYGUIS,		"off",		0,	"legacyguis")
CONFVAR(LICENSE,		0,		0,	"license")
CONFVAR(LICENSEURL,		0,		0,	"licenseurl")
CONFVAR(LOCKWAIT,		"30",		0,	"lockwait")
CONFVAR(MAIL_PROXY,		0,		0,	"mail_proxy")
CONFVAR(MASTER,			0,		0,	"master")
CONFVAR(MONOTONIC,		0,		0,	"monotonic")
CONFVAR(NOGRAPHVERIFY,		"off",		"",	"no_graphverify")
CONFVAR(PARALLEL,		"on",		"",	"parallel")
CONFVAR(PARTIAL_CHECK,		"on",		"",	"partial_check")
CONFVAR(POLY,			"off",		0,	"poly")
CONFVAR(STATS_AFTER_PULL,	"off",		"",	"stats_after_pull")
CONFVAR(SYNC,			"off",		"",	"sync")
CONFVAR(TRIGGERS,		"$PRODUCT|.",	"",	"triggers")
CONFVAR(UNIQDB,			0,		0,	"uniqdb")
CONFVAR(UPGRADE_URL,		0,		0,	"upgrade_url")
