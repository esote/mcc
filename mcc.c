/*
 * mcc is a machine code compiler.
 * Copyright (C) 2019 Esote
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <sys/stat.h>

#include <elf.h>
#include <err.h>
#include <getopt.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t	byte_len(FILE *const);

uint32_t	write_elf_hdr(FILE *const, uint32_t const);

void	write_text_hdr(FILE *const, uint32_t const, uint32_t const, int const);
void	write_bss_hdr(FILE *const, uint32_t const);

void	write_mcode(FILE *const, FILE *const);

void	write_section_names(FILE *const);

void	write_null_section(FILE *const);
void	write_text_section(FILE *const, uint32_t const, uint32_t const);
void	write_bss_section(FILE *const, uint32_t const);
void	write_shstrtab_section(FILE *const, uint32_t const, uint32_t const);

#define BSS_VADDR	0x804a000
#define TEXT_VADDR	0x8049000

/* length of section header string including null terminator */
#define NULL_LEN	(0 + 1)
#define SHSTRTAB_LEN	(9 + 1)
#define TEXT_LEN	(5 + 1)
#define BSS_LEN		(4 + 1)

#define SHSTRTAB_ADDRALIGN	(1 << 0)
#define BSS_ADDRALIGN		(1 << 2)
#define TEXT_ADDRALIGN		(1 << 4)

/* index in section header string table section */
#define SHSTRTAB_INDEX	1
#define TEXT_INDEX	(SHSTRTAB_LEN + 1)
#define BSS_INDEX	(SHSTRTAB_LEN + TEXT_LEN+1)

#define PAGE_SIZE	4096

#define COMMENT	';'

static unsigned char const zero = 0;

int
main(int argc, char *argv[])
{
	FILE *in;
	FILE *out;
	char *iname;
	char const *oname;
	uint32_t len;
	uint32_t mem;
	uint32_t off;
	int ch;
	int self_m;

	oname = "a.out";
	mem = 0;
	self_m = 0;

	while ((ch = getopt(argc, argv, "m:o:s")) != -1) {
		switch (ch) {
		case 'm':
			/* TODO remove BSS section if mem == 0 */
			mem = atoi(optarg);
			break;
		case 'o':
			oname = optarg;
			break;
		case 's':
			self_m = 1;
			break;
		default:
			(void)fprintf(stderr,
				"usage: %s [-s] [-m memsize] [-o file] "
				"[file]\n",
				argv[0]);
			return 1;
		}
	}

	if (mem == 0) {
		warnx("mem is 0, using .bss could segfault or overflow into "
			"other sections");
	}

	argv += optind;

	if ((iname = *argv) == NULL) {
		errx(1, "missing file argument");
	}

	if ((in = fopen(iname, "r")) == NULL) {
		err(1, "open in");
	}

	if ((out = fopen(oname, "w")) == NULL) {
		err(1, "open out");
	}

	len = byte_len(in);

	off = write_elf_hdr(out, len);

	write_text_hdr(out, off, len, self_m);
	write_bss_hdr(out, mem);

	write_mcode(in, out);

	write_section_names(out);

	write_null_section(out);
	write_text_section(out, off, len);
	write_bss_section(out, mem);
	write_shstrtab_section(out, off, len);

	if (fclose(in) == EOF) {
		err(1, "close in");
	}

	if (fclose(out) == EOF) {
		err(1, "close out");
	}

	if (chmod(oname, 00775) == -1) {
		err(1, "chmod");
	}

	return 0;
}

uint32_t
byte_len(FILE *const in)
{
	unsigned char buf;
	uint32_t len;

	len = 0;

	while (fread(&buf, 1, 1, in) == 1) {
		switch (buf) {
		case COMMENT:
			/* gulp until newline or EOF */
			while (1) {
				if (fread(&buf, 1, 1, in) == 0) {
					goto out;
				} else if (buf == '\n') {
					break;
				}
			}
			break;
		case '0':
		case '1':
			len++;
			break;
		}
	}

out:
	if (len % 8 != 0) {
		errx(1, "weird bytes");
	}

	if (fseek(in, 0, SEEK_SET) == -1) {
		err(1, "fseek");
	}

	return len / 8;
}

uint32_t
write_elf_hdr(FILE *const out, uint32_t const len)
{
	Elf32_Ehdr e = {0};

	e.e_ident[EI_MAG0] = ELFMAG0;
	e.e_ident[EI_MAG1] = ELFMAG1;
	e.e_ident[EI_MAG2] = ELFMAG2;
	e.e_ident[EI_MAG3] = ELFMAG3;
	/* TODO support 64-bit */
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

	e.e_phnum = 2;
	e.e_shnum = 4;
	e.e_shstrndx = 3;

	/* program hdrs start after elf hdr */
	e.e_phoff = e.e_ehsize;

	e.e_entry = TEXT_VADDR + e.e_ehsize + e.e_phnum * e.e_phentsize;

	/* section hdrs start after elf hdr, program hdrs, text, and shstrtab */
	e.e_shoff = e.e_ehsize + e.e_phnum * e.e_phentsize + len +
		NULL_LEN + SHSTRTAB_LEN + TEXT_LEN + BSS_LEN;

	if (fwrite(&e, sizeof(Elf32_Ehdr), 1, out) != 1) {
		err(1, "write elf hdr");
	}

	/* return offset to mcode */
	return e.e_ehsize + e.e_phnum * e.e_phentsize;
}

void
write_text_hdr(FILE *const out, uint32_t const off, uint32_t const len,
	int const self_m)
{
	Elf32_Phdr text = {0};

	text.p_type = PT_LOAD;
	text.p_offset = 0;
	text.p_vaddr = TEXT_VADDR;
	text.p_paddr = text.p_vaddr;
	text.p_filesz = len + off;
	text.p_memsz = text.p_filesz;
	text.p_flags = PF_X | PF_R;

	if (self_m) {
		text.p_flags |= PF_W;
	}

	text.p_align = PAGE_SIZE;

	if (fwrite(&text, sizeof(Elf32_Phdr), 1, out) != 1) {
		err(1, "write text hdr");
	}
}

void
write_bss_hdr(FILE *const out, uint32_t const mem)
{
	Elf32_Phdr bss = {0};

	bss.p_type = PT_LOAD;
	bss.p_offset = 0;
	bss.p_vaddr = BSS_VADDR;
	bss.p_paddr = bss.p_vaddr;
	bss.p_filesz = 0;
	bss.p_memsz = mem;
	bss.p_flags = PF_R | PF_W;
	bss.p_align = PAGE_SIZE;

	if (fwrite(&bss, sizeof(Elf32_Phdr), 1, out) != 1) {
		err(1, "write bss hdr");
	}
}

void
write_mcode(FILE *const in, FILE *const out)
{
	uint32_t i;
	unsigned char buf;
	unsigned char cur;

	cur = 0;

	for (i = 8; fread(&buf, 1, 1, in) == 1; ) {
		switch (buf) {
		case COMMENT:
			/* gulp until newline or EOF */
			while (1) {
				if (fread(&buf, 1, 1, in) == 0) {
					return;
				} else if (buf == '\n') {
					break;
				}
			}
			break;
		case '0':
		case '1':
			cur |= (buf - '0') << --i;

			if (i == 0) {
				if (fwrite(&cur, 1, 1, out) != 1) {
					err(1, "write mcode");
				}

				i = 8;
				cur = 0;
			}
			break;
		}
	}
}

void
write_section_names(FILE *const out)
{
	static char const *const shstrtab = ".shstrtab";
	static char const *const text = ".text";
	static char const *const bss = ".bss";

	/* null section name */
	if (fwrite(&zero, NULL_LEN, 1, out) != 1) {
		err(1, "write null name");
	}

	if (fwrite(shstrtab, SHSTRTAB_LEN, 1, out) != 1) {
		err(1, "write shstrtab name");
	}

	if (fwrite(text, TEXT_LEN, 1, out) != 1) {
		err(1, "write text name");
	}

	if (fwrite(bss, BSS_LEN, 1, out) != 1) {
		err(1, "write bss name");
	}
}

void
write_null_section(FILE *const out)
{
	Elf32_Shdr null = {0};

	null.sh_name = SHT_NULL;

	if (fwrite(&null, sizeof(Elf32_Shdr), 1, out) != 1) {
		err(1, "write null section");
	}
}

void
write_text_section(FILE *const out, uint32_t const off, uint32_t const len)
{
	Elf32_Shdr text = {0};

	text.sh_name = TEXT_INDEX;
	text.sh_type = SHT_PROGBITS;
	text.sh_flags = SHF_ALLOC | SHF_EXECINSTR;
	text.sh_addr = TEXT_VADDR + off;
	text.sh_offset = off;
	text.sh_size = len;
	text.sh_link = 0;
	text.sh_info = 0;
	text.sh_addralign = TEXT_ADDRALIGN;
	text.sh_entsize = 0;

	if (fwrite(&text, sizeof(Elf32_Shdr), 1, out) != 1) {
		err(1, "write text section");
	}
}

void
write_bss_section(FILE *const out, uint32_t const mem)
{
	Elf32_Shdr bss = {0};

	bss.sh_name = BSS_INDEX;
	bss.sh_type = SHT_NOBITS;
	bss.sh_flags = SHF_WRITE | SHF_ALLOC;
	bss.sh_addr = BSS_VADDR;
	/* TODO magic */
	bss.sh_offset = 0x1000;
	bss.sh_size = mem;
	bss.sh_link = 0;
	bss.sh_info = 0;
	bss.sh_addralign = BSS_ADDRALIGN;
	bss.sh_entsize = 0;

	if (fwrite(&bss, sizeof(Elf32_Shdr), 1, out) != 1) {
		err(1, "write bss section");
	}
}

void
write_shstrtab_section(FILE *const out, uint32_t const off, uint32_t const len)
{
	Elf32_Shdr shstrtab = {0};

	shstrtab.sh_name = SHSTRTAB_INDEX;
	shstrtab.sh_type = SHT_STRTAB;
	shstrtab.sh_flags = 0;
	shstrtab.sh_addr = 0;
	/* TODO magic */
	shstrtab.sh_offset = off + len;
	shstrtab.sh_size = NULL_LEN + SHSTRTAB_LEN + TEXT_LEN + BSS_LEN;
	shstrtab.sh_link = 0;
	shstrtab.sh_info = 0;
	shstrtab.sh_addralign = SHSTRTAB_ADDRALIGN;
	shstrtab.sh_entsize = 0;

	if (fwrite(&shstrtab, sizeof(Elf32_Shdr), 1, out) != 1) {
		err(1, "write shstrtab section");
	}
}
