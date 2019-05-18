#ifndef WRITE_H
#define WRITE_H

void	write_ehdr64(FILE *const, struct mcc_opts *const);

void	write_phdr64_text(FILE *const, struct mcc_opts const *const);
void	write_phdr64_bss(FILE *const, struct mcc_opts const *const);

void	write_shdr64_null(FILE *const);
void	write_shdr64_text(FILE *const, struct mcc_opts const *const);
void	write_shdr64_bss(FILE *const, struct mcc_opts const *const);
void	write_shdr64_shstrtab(FILE *const, struct mcc_opts const *const);

void	write_ehdr32(FILE *const, struct mcc_opts *const);

void	write_phdr32_text(FILE *const, struct mcc_opts const *const);
void	write_phdr32_bss(FILE *const, struct mcc_opts const *const);

void	write_shdr32_null(FILE *const);
void	write_shdr32_text(FILE *const, struct mcc_opts const *const);
void	write_shdr32_bss(FILE *const, struct mcc_opts const *const);
void	write_shdr32_shstrtab(FILE *const, struct mcc_opts const *const);

#endif /* WRITE_H */
