#include<stddef.h>
#include"pathnames.h"
#include"cmdline.h"
#define DEF_HANDLER(name)extern int cmdline_##name(char*,char*)
#define DEF_OPTION(_name,_always,_type)&(struct cmdline_option){.name=#_name,.type=(_type),.always=(_always),.handler=&cmdline_##_name}
#define DEF_XOPTION(_name,_handler,_always,_type)&(struct cmdline_option){.name=#_name,.type=(_type),.always=(_always),.handler=&cmdline_##_handler}

DEF_HANDLER(rw);
DEF_HANDLER(ro);
DEF_HANDLER(end);
DEF_HANDLER(verbose);
DEF_HANDLER(init);
DEF_HANDLER(root);
DEF_HANDLER(rootflags);
DEF_HANDLER(rootfstype);
DEF_HANDLER(rootwait);
DEF_HANDLER(logfs);
DEF_HANDLER(logfile);
DEF_HANDLER(dpi);
DEF_HANDLER(dpi_force);
DEF_HANDLER(backlight);
DEF_HANDLER(androidboot_mode);

struct cmdline_option*cmdline_options[]={

	// root.c; rootfs for switchroot
	DEF_OPTION(rw,         false, NO_VALUE),
	DEF_OPTION(ro,         false, NO_VALUE),
	DEF_OPTION(init,       false, REQUIRED_VALUE),
	DEF_OPTION(root,       false, REQUIRED_VALUE),
	DEF_OPTION(rootflags,  false, REQUIRED_VALUE),
	DEF_OPTION(rootfstype, false, REQUIRED_VALUE),
	DEF_OPTION(rootwait,   false, OPTIONAL_VALUE),

	// cmdline.c; end parse general options
	DEF_OPTION(end,        false, OPTIONAL_VALUE),

	// cmdline.c; verbose log
	DEF_OPTION(verbose,    false, OPTIONAL_VALUE),

	// logfs.c; logger persistent storage
	DEF_OPTION(logfs,      false, REQUIRED_VALUE),
	DEF_OPTION(logfile,    false, REQUIRED_VALUE),

	#ifdef ENABLE_GUI
	// gui.c; options for gui
	DEF_OPTION(dpi,        false, REQUIRED_VALUE),
	DEF_OPTION(dpi_force,  false, REQUIRED_VALUE),
	DEF_OPTION(backlight,  false, REQUIRED_VALUE),
	#endif

	// androidboot.c; android bootloader pass arguments
	DEF_XOPTION(androidboot.mode,   androidboot_mode,  true, REQUIRED_VALUE),
	NULL
};

struct boot_options boot_options={

};

char*init_list[]={
	// legacy init
	_PATH_SBIN"/init",
	_PATH_BIN"/init",
	_PATH_USR_SBIN"/init",
	_PATH_USR_BIN"/init",
	_PATH_USR_LOCAL_SBIN"/init",
	_PATH_USR_LOCAL_BIN"/init",

	// systemd
	_PATH_LIB"/systemd/systemd",
	_PATH_USR_LIB"/systemd/systemd",
	_PATH_USR_LOCAL_LIB"/systemd/systemd",

	// initramfs
	"/init",
	"/linuxrc",

	// shell
	_PATH_BIN"/bash",
	_PATH_USR_BIN"/bash",
	_PATH_USR_LOCAL_BIN"/bash"
	_PATH_BIN"/sh",
	_PATH_USR_BIN"/sh",
	_PATH_USR_LOCAL_BIN"/sh",

	NULL
};

char*firmware_list[32]={
	"/vendor/firmware_mnt/image",
	"/vendor/bt_firmware/image",
	"/vendor/firmware",
	_PATH_USR_LOCAL_LIB"/firmware",
	_PATH_LIB"/firmware",
	_PATH_USR_LIB"/firmware",
	NULL
};
