#ifndef MCC_H
#define MCC_H

union size {
	uint32_t	n32;
	uint64_t	n64;
};

struct mcc_opts {
	union size	bss_vaddr;
	union size	text_vaddr;

	union size	len;
	union size	off;

	union size	mem;

	int	self_m;
};

#define PAGE_SIZE	4096

#define SHSTRTAB_ADDRALIGN	(1 << 0)
#define BSS_ADDRALIGN		(1 << 2)
#define TEXT_ADDRALIGN		(1 << 4)

/* length of section header string including null terminator */
#define NULL_LEN	(0 + 1)
#define SHSTRTAB_LEN	(9 + 1)
#define TEXT_LEN	(5 + 1)
#define BSS_LEN		(4 + 1)

/* index in section header string table section */
#define SHSTRTAB_INDEX	1
#define TEXT_INDEX	(SHSTRTAB_LEN + 1)
#define BSS_INDEX	(SHSTRTAB_LEN + TEXT_LEN+1)

#endif /* MCC_H */
