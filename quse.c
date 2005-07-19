/*
 * Copyright 2005 Gentoo Foundation
 * Distributed under the terms of the GNU General Public License v2
 * $Header: /var/cvsroot/gentoo-projects/portage-utils/quse.c,v 1.21 2005/07/19 22:26:46 solar Exp $
 *
 * 2005 Ned Ludd        - <solar@gentoo.org>
 * 2005 Mike Frysinger  - <vapier@gentoo.org>
 *
 ********************************************************************
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 *
 */



#define QUSE_FLAGS "eavKL" COMMON_FLAGS
static struct option const quse_long_opts[] = {
	{"exact",     no_argument, NULL, 'e'},
	{"all",       no_argument, NULL, 'a'},
	{"keywords",  no_argument, NULL, 'K'},
	{"licence",   no_argument, NULL, 'L'},
	/* {"format",     a_argument, NULL, 'F'}, */
	COMMON_LONG_OPTS
};
static const char *quse_opts_help[] = {
	"Show exact non regexp matching using strcmp",
	"Show annoying things in IUSE",
	"Use the KEYWORDS vs IUSE",
	"Use the LICENSE vs IUSE",
	/* "Use your own variable formats. -F NAME=", */
	COMMON_OPTS_HELP
};
#define quse_usage(ret) usage(ret, QUSE_FLAGS, quse_long_opts, quse_opts_help, APPLET_QUSE)

static void print_highlighted_use_flags(char *str, int ind, int argc, char **argv) {
	char *p;
	size_t pos, len;

	short highlight = 0;
	int i;

	if (!color) {
		printf("%s", str);
		return;
	}

	remove_extra_space(str);
	rmspace(str);

	len = strlen(str);

	for (pos = 0; pos < len; pos++) {
		highlight = 0;
		if ((p = strchr(str, ' ')) != NULL)
			*p = 0;
		pos += strlen(str);
		for (i = ind; i < argc; ++i)
			if (strcmp(str, argv[i]) == 0)
				highlight = 1;
		if (highlight)
			printf("%s%s%s ", BOLD, str, NORM);
		else
			printf("%s%s%s%s ", NORM, MAGENTA, str, NORM);
		if (p != NULL)
			str = p + 1;
	}
}

int quse_main(int argc, char **argv)
{
	FILE *fp;
	char *p;

	char buf0[_POSIX_PATH_MAX];
	char buf1[_POSIX_PATH_MAX];
	char buf2[_POSIX_PATH_MAX];

	char ebuild[_POSIX_PATH_MAX];

	const char *search_var = NULL;
	const char *search_vars[] = { "IUSE=", "KEYWORDS=", "LICENSE=", search_var };
	short all = 0;
	int regexp_matching = 1 , i, idx = 0;
	size_t search_len;

	DBG("argc=%d argv[0]=%s argv[1]=%s",
	    argc, argv[0], argc > 1 ? argv[1] : "NULL?");

	while ((i = GETOPT_LONG(QUSE, quse, "")) != -1) {
		switch (i) {
		case 'e': regexp_matching = 0; break;
		case 'a': all = 1; break;
		case 'K': idx = 1;  break;
		case 'L': idx = 2; break;
		/* case 'F': idx = 3, search_vars[idx] = xstrdup(optarg); break; */
		COMMON_GETOPTS_CASES(quse)
		}
	}
	if (argc == optind && !all)
		quse_usage(EXIT_FAILURE);

	if (all) optind = argc;
	initialize_ebuild_flat();	/* sets our pwd to $PORTDIR */

	search_len = strlen(search_vars[idx]);
	assert(search_len < sizeof(buf0));

	if ((fp = fopen(CACHE_EBUILD_FILE, "r")) == NULL)
		return 1;
	while ((fgets(ebuild, sizeof(ebuild), fp)) != NULL) {
		FILE *newfp;
		if ((p = strchr(ebuild, '\n')) != NULL)
			*p = 0;
		if ((newfp = fopen(ebuild, "r")) != NULL) {
			unsigned int lineno = 0;
			while ((fgets(buf0, sizeof(buf0), newfp)) != NULL) {
				int ok = 0;
				lineno++;

				if ((strncmp(buf0, search_vars[idx], search_len)) != 0)
					continue;

				if ((p = strchr(buf0, '\n')) != NULL)
					*p = 0;
				if (verbose) {
					if ((strchr(buf0, '\t') != NULL)
					|| (strchr(buf0, '\\') != NULL)
					|| (strchr(buf0, '\'') != NULL)
					|| (strstr(buf0, "  ") != NULL))
					warn("# Line %d of %s has an annoying %s", lineno, ebuild, buf0);
				}
#ifdef THIS_SUCKS
				if ((p = strrchr(&buf0[search_len+1], '\\')) != NULL) {

				multiline:
					*p = ' ';
					memset(buf1, 0, sizeof(buf1));

					if ((fgets(buf1, sizeof(buf1), newfp)) == NULL)
						continue;
					lineno++;

					if ((p = strchr(buf1, '\n')) != NULL)
						*p = 0;
					snprintf(buf2, sizeof(buf2), "%s %s", buf0, buf1);
					remove_extra_space(buf2);
					strcpy(buf0, buf2);
					if ((p = strrchr(buf1, '\\')) != NULL)
						goto multiline;
				}
#else
				remove_extra_space(buf0);
#endif	
				while ((p = strrchr(&buf0[search_len+1], '"')) != NULL)  *p = 0;
				while ((p = strrchr(&buf0[search_len+1], '\'')) != NULL) *p = 0;
				while ((p = strrchr(&buf0[search_len+1], '\\')) != NULL) *p = ' ';

				if ((size_t)strlen(buf0) < (size_t)(search_len+1)) {
					// warnf("err '%s'/%lu <= %lu; line %d\n", buf0, (unsigned long)strlen(buf0), (unsigned long)(search_len+1), lineno);
					continue;
				}

				if ((argc == optind) || (all)) {
					ok = 1;
				} else {
					ok = 0;
					if (regexp_matching) {
						for (i = optind; i < argc; ++i) {
							if (rematch(argv[i], &buf0[search_len+1], REG_NOSUB) == 0) {
								ok = 1;
								break;
							}
						}
					} else {
						remove_extra_space(buf0);
						assert(buf0 != NULL);
						strcpy(buf1, buf0);
						while ((p = strchr(buf1, ' ')) != NULL) {
							*p = 0;
							// assert(p + 1 != NULL);
							for (i = (size_t) optind; i < argc && argv[i] != NULL; i++) {
							//	assert(buf1 != NULL);
							//	assert(argv[i] != NULL);
							//	printf("%s %s %d %d %d %d\n", argv[i], buf1, i, sizeof(buf0), sizeof(buf1), sizeof(buf2));
								if (strcmp(buf1, argv[i]) == 0) {
									ok = 1;
									break;
								}
							//	printf("- %d strcmp(%s, %s) = 0\n", i, argv[i], buf1);
							}
							// assert(p + 1 != NULL);
							strcpy(buf2, p + 1);
							strcpy(buf1, buf2);
						}
					}
				}
				if (ok) {
					printf("%s%s%s ", CYAN, ebuild, NORM);
					print_highlighted_use_flags(&buf0[search_len+1], optind, argc, argv);
					puts(NORM);
				}
				break;
			}
			fclose(newfp);
		} else {
			if (!reinitialize)
				warnf("(cache update pending) %s : %s", ebuild, strerror(errno));
			reinitialize = 1;
		}
	}
	fclose(fp);
	return EXIT_SUCCESS;
}
