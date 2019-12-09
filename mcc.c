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

uint64_t	byte_len(FILE *const, int const);
void		write_mcode(FILE *const, FILE *const, int const);
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
	int ch;
	int hex;
	int use_64;
	int use_bss_addr;
	int use_text_addr;

	opts.self_m = 0;
	opts.mem.n64 = 0;
	opts.bss_vaddr.n64 = 0x402000;
	opts.text_vaddr.n64 = 0x401000;

	oname = "a.out";

	hex = 0;

	/* default 64-bit */
	use_64 = 1;

	use_bss_addr = 0;
	use_text_addr = 0;

	while ((ch = getopt(argc, argv, "3b:hm:o:st:")) != -1) {
		switch (ch) {
		case '3':
			use_64 = 0;
			break;
		case 'b':
			opts.bss_vaddr.n64 = strtoull(optarg, &end, 0);

			if (errno == EINVAL || errno == ERANGE) {
				err(1, "bss address invalid");
			} else if (optarg == end) {
				errx(1, "no bss address read");
			}

			use_bss_addr = 1;
			break;
		case 'h':
			hex = 1;
			break;
		case 'm':
			opts.mem.n64 = strtoull(optarg, &end, 10);

			if (errno == EINVAL || errno == ERANGE) {
				err(1, "memsize invalid");
			} else if (optarg == end) {
				errx(1, "no memsize read");
			} else if (opts.mem.n64 == 0) {
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
		case 't':
			opts.text_vaddr.n64 = strtoull(optarg, &end, 0);

			if (errno == EINVAL || errno == ERANGE) {
				err(1, "text address invalid");
			} else if (optarg == end) {
				err(1, "no text address read");
			}

			use_text_addr = 1;
			break;
		default:
			(void)fprintf(stderr,
				"usage: %s [-3s] [-b addr] [-m memsize] "
				"[-o file] [-t addr] file\n",
				argv[0]);
			return 1;
		}
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

	opts.len.n64 = byte_len(in, hex);

	if (!use_64) {
		if (opts.mem.n64 > UINT32_MAX) {
			warnx("memsize is larger than 32 bits");
		}

		if (opts.len.n64 > UINT32_MAX) {
			warnx("input file byte length is larger than 32 bits");
		}

		if (!use_bss_addr) {
			opts.bss_vaddr.n32 = 0x804a000;
		} else if (opts.bss_vaddr.n64 > UINT32_MAX) {
			warnx("bss address is larger than 32 bits");
		}

		if (!use_text_addr) {
			opts.text_vaddr.n32 = 0x8049000;
		} else if (opts.text_vaddr.n64 > UINT32_MAX) {
			warnx("text address is larger than 32 bits");
		}
	}

	if (use_64) {
		write_ehdr64(out, &opts);
		write_phdr64_text(out, &opts);
	} else {
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

	write_mcode(in, out, hex);
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
byte_len(FILE *const in, int const hex)
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
			if (hex) {
				i += 4;
			} else {
				i++;
			}
			break;
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
			if (hex) {
				i += 4;
			}
			break;
		}

		if (i == 8) {
			i = 0;
			len++;
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
write_mcode(FILE *const in, FILE *const out, int const hex)
{
	size_t const bytesize = hex ? 2 : 8;
	int const base = hex ? 16 : 2;

	char buf[bytesize+1];
	uint64_t i;
	char *end;
	unsigned char cur;
	char rbuf;

	buf[bytesize] = '\0';

	for (i = 0; fread(&rbuf, 1, 1, in);) {
		switch (rbuf) {
		case COMMENT:
			/* gulp until newline or EOF */
			while (1) {
				if (fread(&rbuf, 1, 1, in) == 0) {
					return;
				} else if (rbuf == '\n') {
					break;
				}
			}
			break;
		case '0':
		case '1':
			buf[i++] = rbuf;
			break;
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
			if (hex) {
				buf[i++] = rbuf;
			}
			break;
		}

		if (i == bytesize) {
			cur = (unsigned char)strtol(buf, &end, base);

			if (errno == EINVAL || errno == ERANGE) {
				err(1, "mcode byte invalid");
			} else if (buf == end) {
				err(1, "mcode no byte read");
			}

			i = 0;
			if (fwrite(&cur, 1, 1, out) != 1) {
				err(1, "write mcode");
			}
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
