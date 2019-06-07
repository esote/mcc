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

#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "mcc.h"
#include "write.h"

uint64_t	byte_len(FILE *const);
void		write_mcode(FILE *const, FILE *const);
void		write_section_names(FILE *const, struct mcc_opts const *const);

#define COMMENT	';'

int
main(int argc, char *argv[])
{
	FILE *in;
	FILE *out;
	struct mcc_opts opts;
	char *end;
	char const *iname;
	char const *oname;
	uint64_t len;
	uint64_t mem;
	int use_64;
	int ch;

	opts.self_m = 0;

	oname = "a.out";
	mem = 0;

	/* default 64-bit */
	use_64 = 1;
	opts.bss_vaddr.n64 = 0x402000;
	opts.text_vaddr.n64 = 0x401000;

	while ((ch = getopt(argc, argv, "3m:o:s")) != -1) {
		switch (ch) {
		case '3':
			use_64 = 0;
			opts.bss_vaddr.n32 = 0x804a000;
			opts.text_vaddr.n32 = 0x8049000;
			break;
		case 'm':
			mem = strtoull(optarg, &end, 10);

			if (errno == EINVAL || errno == ERANGE) {
				err(1, "memsize invalid");
			} else if (optarg == end) {
				errx(1, "no memsize read");
			} else if (mem == 0) {
				warnx("if memsize is zero no .bss section will "
					"be written");
			}

			break;
		case 'o':
			oname = optarg;
			break;
		case 's':
			opts.self_m = 1;
			break;
		default:
			(void)fprintf(stderr,
				"usage: %s [-3s] [-m memsize] [-o file] file\n",
				argv[0]);
			return 1;
		}
	}

	if (!use_64 && mem > UINT32_MAX) {
		warnx("memsize may overflow");
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

	if (!use_64 && len > UINT32_MAX) {
		warnx("input file byte length may overflow");
	}

	if (use_64) {
		opts.mem.n64 = mem;
		opts.len.n64 = len;
		write_ehdr64(out, &opts);
		write_phdr64_text(out, &opts);
	} else {
		opts.mem.n32 = (uint32_t)mem;
		opts.len.n32 = (uint32_t)len;
		write_ehdr32(out, &opts);
		write_phdr32_text(out, &opts);
	}

	if (opts.mem.n64 != 0) {
		if (use_64) {
			write_phdr64_bss(out, &opts);
		} else {
			write_phdr32_bss(out, &opts);
		}
	}

	write_mcode(in, out);
	write_section_names(out, &opts);

	if (use_64) {
		write_shdr64_null(out);
		write_shdr64_text(out, &opts);

		if (opts.mem.n64 != 0) {
			write_shdr64_bss(out, &opts);
		}

		write_shdr64_shstrtab(out, &opts);
	} else {
		write_shdr32_null(out);
		write_shdr32_text(out, &opts);

		if (opts.mem.n32 != 0) {
			write_shdr32_bss(out, &opts);
		}

		write_shdr32_shstrtab(out, &opts);
	}

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

uint64_t
byte_len(FILE *const in)
{
	uint64_t i;
	uint64_t len;
	unsigned char buf;

	len = 0;

	for (i = 0; fread(&buf, 1, 1, in) == 1; ) {
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
			if (++i == 8) {
				i = 0;
				len++;
			}

			break;
		}
	}

out:
	if (i != 0) {
		errx(1, "weird bytes");
	}

	if (fseek(in, 0, SEEK_SET) == -1) {
		err(1, "fseek");
	}

	return len;
}

void
write_mcode(FILE *const in, FILE *const out)
{
	uint64_t i;
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
			cur = (unsigned char)(cur | (buf - '0') << --i);

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
write_section_names(FILE *const out, struct mcc_opts const *const opts)
{
	static char const *const shstrtab = ".shstrtab";
	static char const *const text = ".text";
	static char const *const bss = ".bss";
	static unsigned char const zero = 0;

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

	if (opts->mem.n64 != 0 && fwrite(bss, BSS_LEN, 1, out) != 1) {
		err(1, "write bss name");
	}
}
