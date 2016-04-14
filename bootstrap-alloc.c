#include "bootstrap-alloc.h"
#include "multiboot.h"
#include "print.h"
#include "log.h"

// Bootstrap allocator's mmap
// Contains forbid-kernel region, also has actuall holes
// These holes is memory allocated by bootstrap allocator
int bootstrap_mmap_length = 0;
int bootstrap_mmap_tmp_length;
struct mmap_entry bootstrap_mmap[BOOTSTRAP_MMAP_MAX_LENGTH], bootstrap_mmap_tmp[BOOTSTRAP_MMAP_MAX_LENGTH];

struct bootstrap_copy_iterator {
	struct mmap_iterator super;
	int dst_size;
	int* dst_length;
	struct mmap_entry* dst;
	bool do_cut;
	uint64_t cut_start;
	uint64_t cut_end;
};

static void __bootstrap_copy(struct bootstrap_copy_iterator* self, struct mmap_entry* entry) {
	void put_entry(uint64_t base, uint64_t length, int type) {
		int id = self->dst_size;
		self->dst_size++;
		*self->dst_length = *self->dst_length + sizeof(struct mmap_entry);
		self->dst[id].size = MMAP_ENTRY_SIZE;
		self->dst[id].base_addr = base;
		self->dst[id].length = length;
		self->dst[id].type = type;
	};

	uint64_t entry_end = entry->base_addr + entry->length;
	if (!self->do_cut || self->cut_end <= entry->base_addr || entry_end <= self->cut_start) {
		//No intersection
		put_entry(entry->base_addr, entry->length, entry->type);
		return;
	}
	if (entry->base_addr < self->cut_start) {
		//Starting piece
		put_entry(entry->base_addr, self->cut_start - entry->base_addr, entry->type);
	}
	if (self->cut_end < entry_end) {
		//Ending piece
		put_entry(self->cut_end, entry_end - self->cut_end, entry->type);
	}
}

static void bootstrap_copy_iterator_init(
		struct bootstrap_copy_iterator* self,
		struct mmap_entry* dst, int* dst_length
) {
	self->super.iterate = (mmap_iterator_iterate_t) __bootstrap_copy;
	self->dst = dst;
	self->dst_size = 0;
	self->dst_length = dst_length;
	self->do_cut = false;
}

static void bootstrap_copy_iterator_init_cut(
		struct bootstrap_copy_iterator* self,
		struct mmap_entry* dst, int* dst_length,
		uint32_t cut_start, uint32_t cut_end
) {
	self->super.iterate = (mmap_iterator_iterate_t) __bootstrap_copy;
	self->dst = dst;
	self->dst_size = 0;
	self->dst_length = dst_length;
	self->do_cut = true;
	self->cut_start = cut_start;
	self->cut_end = cut_end;
}

static void bootstrap_del(uint32_t start, uint32_t end) {
	struct bootstrap_copy_iterator iterator;
	bootstrap_mmap_tmp_length = 0;
	bootstrap_copy_iterator_init_cut(&iterator, bootstrap_mmap_tmp, &bootstrap_mmap_tmp_length, start, end);
	mmap_iterate(bootstrap_mmap, bootstrap_mmap_length, (struct mmap_iterator*) &iterator);
	bootstrap_mmap_length = 0;
	bootstrap_copy_iterator_init(&iterator, bootstrap_mmap, &bootstrap_mmap_length);
	mmap_iterate(bootstrap_mmap_tmp, bootstrap_mmap_tmp_length, (struct mmap_iterator*) &iterator);
}

extern char text_phys_begin[];
extern char bss_phys_end[];

void bootstrap_init_mmap(void) {
	struct mboot_info* info = mboot_info_get();
	if ((info->flags & (1 << MBOOT_INFO_MMAP)) == 0) {
		halt("No MMAP present.\n");
	}

	// Copy mmap
	struct bootstrap_copy_iterator iterator;
	bootstrap_copy_iterator_init(&iterator, bootstrap_mmap, &bootstrap_mmap_length);
	mmap_iterate(va(info->mmap_addr), info->mmap_length, (struct mmap_iterator*) &iterator);

	log(LEVEL_LOG, "Purging BIOS memory: [0..%p)", BIOS_SIZE);
	bootstrap_del(0, BIOS_SIZE);
	log(LEVEL_LOG, "Purging kernel memory: [%p..%p)", text_phys_begin, bss_phys_end);
	bootstrap_del((uint64_t) text_phys_begin, (uint64_t) bss_phys_end);

	if ((info->flags & (1 << MBOOT_INFO_MMAP)) != 0) {
		struct mboot_info_mod* mods = (struct mboot_info_mod*) va(info->mods_addr);
		log(
				LEVEL_LOG,
				"Purging modules array memory: [%p..%p)",
				info->mods_addr,
				info->mods_addr + info->mods_count * sizeof(struct mboot_info_mod)
		);
		bootstrap_del(info->mods_addr, info->mods_addr + info->mods_count * sizeof(struct mboot_info_mod));
		for (unsigned int i = 0; i != info->mods_count; ++i) {
			log(LEVEL_LOG, "Purging module %d memory: [%p..%p)", i, mods[i].mod_start, mods[i].mod_end);
			bootstrap_del(mods[i].mod_start, mods[i].mod_end);
		}
	}
	print_mmap(bootstrap_mmap, bootstrap_mmap_length);
}

struct bootstrap_alloc_iterator {
	struct mmap_iterator super;
	uint32_t size;
	phys_t result;
};

static void __bootstrap_alloc(struct bootstrap_alloc_iterator* self, struct mmap_entry* entry) {
	if (self->result) {
		return;
	}
	if (entry->type != MMAP_ENTRY_TYPE_AVAILABLE) {
		return;
	}
	if (entry->length < self->size) {
		return;
	}
	log(LEVEL_VV, "Good entry at %p.", entry->base_addr);
	self->result = entry->base_addr;
	entry->length -= self->size;
	entry->base_addr += self->size;
}

static void bootstrap_alloc_iterator_init(struct bootstrap_alloc_iterator* self,uint32_t size) {
	self->super.iterate = (mmap_iterator_iterate_t)__bootstrap_alloc;
	self->size = size;
	self->result = 0;
}

phys_t bootstrap_alloc(uint32_t size) {
	// We'll just go to our mmap
	// and cut something :)
	log(LEVEL_V, "Asked %u bytes.", size);
	struct bootstrap_alloc_iterator iterator;
	bootstrap_alloc_iterator_init(&iterator, size);
	mmap_iterate(bootstrap_mmap, bootstrap_mmap_length, (struct mmap_iterator*)&iterator);
	return iterator.result;
}
