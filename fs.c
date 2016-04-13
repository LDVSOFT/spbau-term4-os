#include "fs.h"
#include "slab-allocator.h"
#include "buddy.h"
#include "log.h"
#include "print.h"
#include "string.h"
#include "utils.h"

static struct slab_allocator dir_entry_allocator;
static struct slab_allocator file_desc_allocator;
static struct slab_allocator dir_desc_allocator;
static struct file root;

static bool file_resize(struct file* file, uint64_t new_size_level) {
	phys_t new_phys = buddy_alloc(new_size_level);
	if (new_phys == (uint64_t) NULL) {
		log(LEVEL_ERROR, "Cannot resize file @%p (level %d -> %d): no memory!", file, file->size_level, new_size_level);
		return false;
	}
	char* new_data = (char*) va(new_phys);
	if (file->size_level != -1) {
		memcpy(new_data, file->data, buddy_size(min(file->size_level, new_size_level)));
		buddy_free(pa(file->data));
	}
	file->data = new_data;
	file->size_level = new_size_level;
	return true;
}

static bool file_init(struct file* file, enum file_type file_type) {
	mutex_init(&file->lock);
	file->type = file_type;
	switch (file_type) {
		case T_REGULAR:
			file->size_level = -1;
			file->size = 0;
			file->data = NULL;
			return file_resize(file, 0);
		case T_DIRECTORY:
			list_init(&file->entries_head);
			return true;
		default:
			halt("Unsupported file type.");
			return false;
	}
}

static bool dir_entry_init(struct dir_entry* entry, const char* filename, enum file_type type) {
	list_init(&entry->link);
	strncpy(entry->name, filename, FILE_NAME);
	bool result = file_init(&entry->file, type);
	if (!result) {
		log(LEVEL_ERROR, "Failed initialising file %s.", filename);
	}
	return result;
}

void fs_init(void) {
	slab_init_for(&dir_entry_allocator, struct dir_entry);
	slab_init_for(&file_desc_allocator, struct file_desc);
	slab_init_for(&dir_desc_allocator, struct directory_desc);

	file_init(&root, T_DIRECTORY);
}

static struct file* path_step(struct file* dir, const char* pathname) {
	if (dir->type != T_DIRECTORY) {
		return NULL;
	}
	if (*pathname == '/') {
		++pathname;
	}
	int n = 0;
	for (; pathname[n] && pathname[n] != '/' && n < FILE_NAME - 1; ++n) ;

	struct file* result = NULL;
	for (
			struct list_node* list_node = list_first(&dir->entries_head);
			list_node != &dir->entries_head;
			list_node = list_node->next
	) {
		struct dir_entry* entry = LIST_ENTRY(list_node, struct dir_entry, link);
		if (strncmp(pathname, entry->name, n) != 0) {
			continue;
		}
		result = &entry->file;
	}
	return result;
}

static struct file* file_open(const char* pathname, enum file_type type, int flags) {
	// Find dir
	int len = strlen(pathname);
	int last_sep = len - 1;
	for (; last_sep >= 0 && pathname[last_sep] != '/'; --last_sep) ;
	if (last_sep == len - 1) {
		log(LEVEL_WARN, "%s: slash in the end.", pathname);
		return NULL;
	}

	struct file* dir = &root;
	int i = 0;
	while (dir != NULL && i < last_sep) {
		mutex_lock(&dir->lock);
		struct file* newdir = path_step(dir, pathname + i);
		mutex_unlock(&dir->lock);
		dir = newdir;
		++i;
		for (; pathname[i] != '/'; ++i) ;
	}
	if (dir == NULL) {
		return NULL;
	}
	mutex_lock(&dir->lock);
	// Try to find file...
	{
		struct file* file = path_step(dir, pathname + i);
		if (file != NULL || (flags & O_CREAT) == 0) {
			mutex_unlock(&dir->lock);
			return file;
		}
	}
	// Create new one
	struct dir_entry* dir_entry = (struct dir_entry*) slab_alloc(&dir_entry_allocator);
	if (dir_entry == NULL) {
		log(LEVEL_ERROR, "No memory to create new dir entry.");
		mutex_unlock(&dir->lock);
		return NULL;
	}
	if (!dir_entry_init(dir_entry, pathname + last_sep + 1, type)) {
		slab_free(dir_entry);
		return NULL;
	}
	list_add_tail(&dir->entries_head, &dir_entry->link);
	mutex_unlock(&dir->lock);
	return &dir_entry->file;
}

struct file_desc* open(const char* pathname, int flags) {
	struct file* file = file_open(pathname, T_REGULAR, flags);
	if (file == NULL) {
		log(LEVEL_WARN, "%s: not found.", pathname);
		return NULL;
	}
	struct file_desc* file_desc = (struct file_desc*) slab_alloc(&file_desc_allocator);
	if (file_desc == NULL) {
		log(LEVEL_ERROR, "No memory to open %s.", pathname);
		return NULL;
	}
	mutex_lock(&file->lock);
	file_desc->file = file;
	if (flags & O_TRUNCATE) {
		file_resize(file, 0);
		file->size = 0;
	}
	file_desc->pos = (flags & O_APPEND) ? file->size : 0;
	mutex_unlock(&file->lock);

	return file_desc;
}

uint64_t read(struct file_desc* fd, char* buffer, uint64_t size) {
	mutex_lock(&fd->file->lock);
	uint64_t amount = 0;
	if (fd->pos < fd->file->size) {
		amount = min_u64(size, fd->file->size - fd->pos);
	}
	memcpy(buffer, fd->file->data + fd->pos, amount);
	fd->pos += amount;
	mutex_unlock(&fd->file->lock);
	return amount;
}

uint64_t write(struct file_desc* fd, const char* buffer, uint64_t size) {
	mutex_lock(&fd->file->lock);
	if (fd->pos + size > buddy_size(fd->file->size_level)) {
		//Try to relocate...
		if (!file_resize(fd->file, fd->file->size_level + 1)) {
			log(LEVEL_WARN, "Failed to increment file size.");
		}
	}
	uint64_t amount = 0;
	if (fd->pos < buddy_size(fd->file->size_level)) {
		amount = min_u64(size, buddy_size(fd->file->size_level) - fd->pos);
	}
	memcpy(fd->file->data + fd->pos, buffer, amount);
	fd->pos += amount;
	fd->file->size = max_u64(fd->file->size, fd->pos);
	mutex_unlock(&fd->file->lock);
	return amount;
}

void close(struct file_desc* file) {
	slab_free(file);
}

bool mkdir(const char* pathname) {
	struct file* dir = file_open(pathname, T_DIRECTORY, 0);
	if (dir) {
		return false;
	}
	return file_open(pathname, T_DIRECTORY, O_CREAT);
}

struct directory_desc* opendir(const char* pathname) {
	struct file* file = file_open(pathname, T_DIRECTORY, 0);
	if (file == NULL) {
		log(LEVEL_WARN, "%s: not found.", pathname);
		return NULL;
	}
	struct directory_desc* dir_desc = (struct directory_desc*) slab_alloc(&dir_desc_allocator);
	if (dir_desc == NULL) {
		log(LEVEL_ERROR, "No memory to open %s.", pathname);
		return NULL;
	}
	mutex_lock(&file->lock);
	dir_desc->dir = file;
	dir_desc->current = list_first(&file->entries_head);
	mutex_unlock(&file->lock);

	return dir_desc;
}

struct dir_entry* readdir(struct directory_desc* dir_desc) {
	mutex_lock(&dir_desc->dir->lock);
	struct dir_entry* result = NULL;
	if (dir_desc->current != &dir_desc->dir->entries_head) {
		result = LIST_ENTRY(dir_desc->current, struct dir_entry, link);
		dir_desc->current = dir_desc->current->next;
	}
	mutex_unlock(&dir_desc->dir->lock);
	return result;
}

void closedir(struct directory_desc* dir_desc) {
	slab_free(dir_desc);
}

static void __ls(struct file* file, int offset) {
	mutex_lock(&file->lock);
	if (file->type == T_REGULAR) {
		printf("regular size=%ull level=%d data@%p.\n", file->size, file->size_level, file->data);
	} else {
		printf("directory.\n");
		for (
				struct list_node* list_node = list_first(&file->entries_head);
				list_node != &file->entries_head;
				list_node = list_node->next
		) {
			struct dir_entry* dir_entry = LIST_ENTRY(list_node, struct dir_entry, link);
			for (int i = 0; i != offset + 1; ++i) {
				printf("  ");
			}
			printf("  %s: ", dir_entry->name);
			__ls(&dir_entry->file, offset + 1);
		}
	}
	mutex_unlock(&file->lock);
}

void ls(void) {
	printf("/: ");
	__ls(&root, 0);
}
