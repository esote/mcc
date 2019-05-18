#include <elf.h>
#include <err.h>
#include <stdint.h>
#include <stdio.h>

#include "mcc.h"

void
write_ehdr32(FILE *const out, struct mcc_opts *const opts)
{
	Elf32_Ehdr e = {0};

	e.e_ident[EI_MAG0] = ELFMAG0;
	e.e_ident[EI_MAG1] = ELFMAG1;
	e.e_ident[EI_MAG2] = ELFMAG2;
	e.e_ident[EI_MAG3] = ELFMAG3;
	e.e_ident[EI_CLASS] = ELFCLASS32;
	/* TODO support big endian */
	e.e_ident[EI_DATA] = ELFDATA2LSB;
	e.e_ident[EI_VERSION] = EV_CURRENT;
	/* TODO support other ABIs */
	e.e_ident[EI_OSABI] = ELFOSABI_SYSV;
	e.e_ident[EI_ABIVERSION] = 0;
	/* TODO support writing other object file types */
	e.e_type = ET_EXEC;
	/* TODO support other architectures */
	e.e_machine = EM_386;
	e.e_version = EV_CURRENT;
	e.e_flags = 0;

	e.e_ehsize = sizeof(Elf32_Ehdr);
	e.e_phentsize = sizeof(Elf32_Phdr);
	e.e_shentsize = sizeof(Elf32_Shdr);

	if (opts->mem.n32 == 0) {
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

	opts->off.n32 = (uint32_t)(e.e_ehsize + e.e_phnum * e.e_phentsize);

	e.e_entry = opts->text_vaddr.n32 + opts->off.n32;

	/* section hdrs start after elf hdr, program hdrs, text, and shstrtab */
	e.e_shoff = opts->off.n32 + opts->len.n32 + NULL_LEN + SHSTRTAB_LEN +
		TEXT_LEN;

	if (opts->mem.n32 != 0) {
		e.e_shoff += BSS_LEN;
	}

	if (fwrite(&e, sizeof(Elf32_Ehdr), 1, out) != 1) {
		err(1, "write ehdr32");
	}
}

void
write_phdr32_text(FILE *const out, struct mcc_opts const *const opts)
{
	Elf32_Phdr text = {0};

	text.p_type = PT_LOAD;
	text.p_offset = 0;
	text.p_vaddr = opts->text_vaddr.n32;
	text.p_paddr = text.p_vaddr;
	text.p_filesz = opts->len.n32 + opts->off.n32;
	text.p_memsz = text.p_filesz;
	text.p_flags = PF_X | PF_R;

	if (opts->self_m) {
		text.p_flags |= PF_W;
	}

	text.p_align = PAGE_SIZE;

	if (fwrite(&text, sizeof(Elf32_Phdr), 1, out) != 1) {
		err(1, "write phdr32 text");
	}
}

void
write_phdr32_bss(FILE *const out, struct mcc_opts const *const opts)
{
	Elf32_Phdr bss = {0};

	bss.p_type = PT_LOAD;
	bss.p_offset = 0;
	bss.p_vaddr = opts->bss_vaddr.n32;
	bss.p_paddr = bss.p_vaddr;
	bss.p_filesz = 0;
	bss.p_memsz = opts->mem.n32;
	bss.p_flags = PF_R | PF_W;
	bss.p_align = PAGE_SIZE;

	if (fwrite(&bss, sizeof(Elf32_Phdr), 1, out) != 1) {
		err(1, "write phdr32 bss");
	}
}

void
write_shdr32_null(FILE *const out)
{
	Elf32_Shdr null = {0};

	null.sh_name = SHT_NULL;

	if (fwrite(&null, sizeof(Elf32_Shdr), 1, out) != 1) {
		err(1, "write shdr32 null");
	}
}

void
write_shdr32_text(FILE *const out, struct mcc_opts const *const opts)
{
	Elf32_Shdr text = {0};

	text.sh_name = TEXT_INDEX;
	text.sh_type = SHT_PROGBITS;
	text.sh_flags = SHF_ALLOC | SHF_EXECINSTR;
	text.sh_addr = opts->text_vaddr.n32 + opts->off.n32;
	text.sh_offset = opts->off.n32;
	text.sh_size = opts->len.n32;
	text.sh_link = 0;
	text.sh_info = 0;
	text.sh_addralign = TEXT_ADDRALIGN;
	text.sh_entsize = 0;

	if (fwrite(&text, sizeof(Elf32_Shdr), 1, out) != 1) {
		err(1, "write shdr32 text");
	}
}

void
write_shdr32_bss(FILE *const out, struct mcc_opts const *const opts)
{
	Elf32_Shdr bss = {0};

	bss.sh_name = BSS_INDEX;
	bss.sh_type = SHT_NOBITS;
	bss.sh_flags = SHF_WRITE | SHF_ALLOC;
	bss.sh_addr = opts->bss_vaddr.n32;
	/* TODO magic */
	bss.sh_offset = 0x1000;
	bss.sh_size = opts->mem.n32;
	bss.sh_link = 0;
	bss.sh_info = 0;
	bss.sh_addralign = BSS_ADDRALIGN;
	bss.sh_entsize = 0;

	if (fwrite(&bss, sizeof(Elf32_Shdr), 1, out) != 1) {
		err(1, "write shdr32 bss");
	}
}

void
write_shdr32_shstrtab(FILE *const out, struct mcc_opts const *const opts)
{
	Elf32_Shdr shstrtab = {0};

	shstrtab.sh_name = SHSTRTAB_INDEX;
	shstrtab.sh_type = SHT_STRTAB;
	shstrtab.sh_flags = 0;
	shstrtab.sh_addr = 0;
	shstrtab.sh_offset = opts->off.n32 + opts->len.n32;
	shstrtab.sh_size = NULL_LEN + SHSTRTAB_LEN + TEXT_LEN;

	if (opts->mem.n32 != 0) {
		shstrtab.sh_size += BSS_LEN;
	}

	shstrtab.sh_link = 0;
	shstrtab.sh_info = 0;
	shstrtab.sh_addralign = SHSTRTAB_ADDRALIGN;
	shstrtab.sh_entsize = 0;

	if (fwrite(&shstrtab, sizeof(Elf32_Shdr), 1, out) != 1) {
		err(1, "write shdr32 shstrtab");
	}
}
