#include <elf.h>
#include <err.h>
#include <stdint.h>
#include <stdio.h>

#include "mcc.h"

void
write_ehdr64(FILE *const out, struct mcc_opts *const opts)
{
	Elf64_Ehdr e = {0};

	e.e_ident[EI_MAG0] = ELFMAG0;
	e.e_ident[EI_MAG1] = ELFMAG1;
	e.e_ident[EI_MAG2] = ELFMAG2;
	e.e_ident[EI_MAG3] = ELFMAG3;
	e.e_ident[EI_CLASS] = ELFCLASS64;
	/* TODO support big endian */
	e.e_ident[EI_DATA] = ELFDATA2LSB;
	e.e_ident[EI_VERSION] = EV_CURRENT;
	/* TODO support other ABIs */
	e.e_ident[EI_OSABI] = ELFOSABI_SYSV;
	e.e_ident[EI_ABIVERSION] = 0;
	/* TODO support writing other object file types */
	e.e_type = ET_EXEC;
	/* TODO support other architectures */
	e.e_machine = EM_X86_64;
	e.e_version = EV_CURRENT;
	e.e_flags = 0;

	e.e_ehsize = sizeof(Elf64_Ehdr);
	e.e_phentsize = sizeof(Elf64_Phdr);
	e.e_shentsize = sizeof(Elf64_Shdr);

	if (opts->mem.n64 == 0) {
		e.e_phnum = 1;
		e.e_shnum = 3;
		e.e_shstrndx = 2;
	} else {
		e.e_phnum = 2;
		e.e_shnum = 4;
		e.e_shstrndx = 3;
	}

	/* program hdrs start after elf hdr */
	e.e_phoff = e.e_ehsize;

	opts->off.n64 = (uint64_t)(e.e_ehsize + e.e_phnum * e.e_phentsize);

	e.e_entry = opts->text_vaddr.n64 + opts->off.n64;

	/* section hdrs start after elf hdr, program hdrs, text, and shstrtab */
	e.e_shoff = opts->off.n64 + opts->len.n64 + NULL_LEN + SHSTRTAB_LEN +
		TEXT_LEN;

	if (opts->mem.n64 != 0) {
		e.e_shoff += BSS_LEN;
	}

	if (fwrite(&e, sizeof(Elf64_Ehdr), 1, out) != 1) {
		err(1, "write ehdr64");
	}
}

void
write_phdr64_text(FILE *const out, struct mcc_opts const *const opts)
{
	Elf64_Phdr text = {0};

	text.p_type = PT_LOAD;
	text.p_offset = 0;
	text.p_vaddr = opts->text_vaddr.n64;
	text.p_paddr = text.p_vaddr;
	text.p_filesz = opts->len.n64 + opts->off.n64;
	text.p_memsz = text.p_filesz;
	text.p_flags = PF_X | PF_R;

	if (opts->self_m) {
		text.p_flags |= PF_W;
	}

	text.p_align = PAGE_SIZE;

	if (fwrite(&text, sizeof(Elf64_Phdr), 1, out) != 1) {
		err(1, "write phdr64 text");
	}
}

void
write_phdr64_bss(FILE *const out, struct mcc_opts const *const opts)
{
	Elf64_Phdr bss = {0};

	bss.p_type = PT_LOAD;
	bss.p_offset = 0;
	bss.p_vaddr = opts->bss_vaddr.n64;
	bss.p_paddr = bss.p_vaddr;
	bss.p_filesz = 0;
	bss.p_memsz = opts->mem.n64;
	bss.p_flags = PF_R | PF_W;
	bss.p_align = PAGE_SIZE;

	if (fwrite(&bss, sizeof(Elf64_Phdr), 1, out) != 1) {
		err(1, "write phdr64 bss");
	}
}

void
write_shdr64_null(FILE *const out)
{
	Elf64_Shdr null = {0};

	null.sh_name = SHT_NULL;

	if (fwrite(&null, sizeof(Elf64_Shdr), 1, out) != 1) {
		err(1, "write shdr64 null");
	}
}

void
write_shdr64_text(FILE *const out, struct mcc_opts const *const opts)
{
	Elf64_Shdr text = {0};

	text.sh_name = TEXT_INDEX;
	text.sh_type = SHT_PROGBITS;
	text.sh_flags = SHF_ALLOC | SHF_EXECINSTR;
	text.sh_addr = opts->text_vaddr.n64 + opts->off.n64;
	text.sh_offset = opts->off.n64;
	text.sh_size = opts->len.n64;
	text.sh_link = 0;
	text.sh_info = 0;
	text.sh_addralign = TEXT_ADDRALIGN;
	text.sh_entsize = 0;

	if (fwrite(&text, sizeof(Elf64_Shdr), 1, out) != 1) {
		err(1, "write shdr64 text");
	}
}

void
write_shdr64_bss(FILE *const out, struct mcc_opts const *const opts)
{
	Elf64_Shdr bss = {0};

	bss.sh_name = BSS_INDEX;
	bss.sh_type = SHT_NOBITS;
	bss.sh_flags = SHF_WRITE | SHF_ALLOC;
	bss.sh_addr = opts->bss_vaddr.n64;
	/* TODO magic */
	bss.sh_offset = 0x1000;
	bss.sh_size = opts->mem.n64;
	bss.sh_link = 0;
	bss.sh_info = 0;
	bss.sh_addralign = BSS_ADDRALIGN;
	bss.sh_entsize = 0;

	if (fwrite(&bss, sizeof(Elf64_Shdr), 1, out) != 1) {
		err(1, "write shdr64 bss");
	}
}

void
write_shdr64_shstrtab(FILE *const out, struct mcc_opts const *const opts)
{
	Elf64_Shdr shstrtab = {0};

	shstrtab.sh_name = SHSTRTAB_INDEX;
	shstrtab.sh_type = SHT_STRTAB;
	shstrtab.sh_flags = 0;
	shstrtab.sh_addr = 0;
	shstrtab.sh_offset = opts->off.n64 + opts->len.n64;
	shstrtab.sh_size = NULL_LEN + SHSTRTAB_LEN + TEXT_LEN;

	if (opts->mem.n64 != 0) {
		shstrtab.sh_size += BSS_LEN;
	}

	shstrtab.sh_link = 0;
	shstrtab.sh_info = 0;
	shstrtab.sh_addralign = SHSTRTAB_ADDRALIGN;
	shstrtab.sh_entsize = 0;

	if (fwrite(&shstrtab, sizeof(Elf64_Shdr), 1, out) != 1) {
		err(1, "write shdr64 shstrtab");
	}
}
