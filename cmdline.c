#include "cmdline.h"
#include "multiboot.h"
#include "memory.h"
#include "log.h"
#include "string.h"

static int read_arg_num(const char **s) {
	int arg = 0;
	while (**s && **s != ' ') {
		arg = arg * 10 + (**s - '0');
		++*s;
	}
	return arg;
}

static int read_arg_str(const char **s, char *buffer, int n) {
	int res = 0;
	--n;
	while (**s && **s != ' ' && n > 0) {
		*buffer = **s;
		++*s;
		++buffer;
		--n;
		++res;
	}
	*buffer = 0;
	return res;
}

void read_cmdline(void) {
	struct mboot_info* info = (struct mboot_info*)va(mboot_info);
	if ((info->flags & (1 << MBOOT_INFO_CMDLINE)) == 0) {
		log(LEVEL_WARN, "No cmdline available :(");
		return;
	}
	const char* s = (char*) va(info->cmdline);
	log(LEVEL_LOG, "Cmdline: \"%s\".", s);

	char name[50];
	read_arg_str(&s, name, 50);
	log(LEVEL_INFO, "Launched with name \"%s\"", name);

	while (*s) {
		++s;
		if (strncmp(s, "log_lvl=", 8) == 0) {
			s += 8;
			int level = read_arg_num(&s);
			log(LEVEL_INFO, "Level log set to %02d.", level);
			log_set_level(level);
		} else if (strncmp(s, "log_clr=", 7) == 0) {
			s += 8;
			int flag = read_arg_num(&s);
			log(LEVEL_INFO, "Log coloring %sabled.", flag ? "en" : "dis");
			log_set_color_enabled(flag);
		} else {
			log(LEVEL_WARN, "Error in cmdline");
			return;
		}
	}
}
