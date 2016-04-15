#include "initramfs.h"
#include "multiboot.h"
#include "log.h"
#include "memory.h"
#include "string.h"
#include "fs.h"

char* adjust(void* ptr, uint64_t offset) {
	virt_t p = (virt_t) ptr + offset;
	if (p % 4 != 0) {
		p += 4 - p % 4;
	}
	return (char*) p;
}

static uint64_t parse(void* p, int size) {
	int res = 0;
	for (char* s = (char*) p; size; --size, ++s) {
		res = res * 16 + (('0' <= *s && *s <= '9') ? *s - '0' : *s - 'A' + 10);
	}
	return res;
}

static void __load(struct cpio_header* header, void* end) {
	while (header + 1 <= (struct cpio_header*) end) {
		int mode = parse(&header->mode, 8);
		uint64_t name_len = parse(&header->namesize, 8);
		uint64_t file_len = parse(&header->filesize, 8);
		static char name[FILE_NAME];
		strncpy(name, (char*)header + sizeof(struct cpio_header), name_len);

		char* data = adjust(header, sizeof(struct cpio_header) + name_len);
		header = (struct cpio_header*) adjust(data, file_len);
		log(LEVEL_V, "Node: mode=%X, name=\"%s\", size=%llu.", mode, name, file_len);

		if (strncmp(name, ".", 2) == 0) {
			log(LEVEL_V, "Skipping root dir.");
			continue;
		}
		if (strncmp(name, CPIO_END_OF_ARCHIVE, strlen(CPIO_END_OF_ARCHIVE) + 1) == 0) {
			log(LEVEL_V, "Stop, hammertime!");
			return;
		}

		if (S_ISDIR(mode)) {
			if (!mkdir(name)) {
				halt("Failed to create directory \"%s\".", name);
			}
		} else {
			struct file_desc* file = open(name, O_CREAT | O_TRUNCATE);
			if (file == NULL) {
				halt("Failed to create file \"%s\".", name);
			}
			uint64_t written = write(file, data, file_len);
			if (written != file_len) {
				halt("Failed to write the whole file \"%s\" (written %llu/%llu).", name, written, file_len);
			}
			close(file);
		}
	}
}

void initramfs_load(void) {
	struct mboot_info* info = (struct mboot_info*) mboot_info_get();
	if ((info->flags & (1 << MBOOT_INFO_MODS)) == 0) {
		halt("No modules present.");
	}

	struct mboot_info_mod* mods = (struct mboot_info_mod*) va(info->mods_addr);
	for (unsigned int i = 0; i != info->mods_count; ++i) {
		if (mods[i].mod_end - mods[i].mod_start < sizeof(struct cpio_header)) {
			continue;
		}
		struct cpio_header* header = (struct cpio_header*) va(mods[i].mod_start);
		if (strncmp(header->magic, CPIO_MAGIC, strlen(CPIO_MAGIC)) != 0) {
			continue;
		}
		log(LEVEL_INFO, "Found initramfs as mod#%u (%s).", i, mods[i].string == 0 ? "<null>" : va(mods[i].string));
		__load(header, va(mods[i].mod_end));
	}
}
