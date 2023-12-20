/*
 * File: liang.cc
 *
 * Copyright 2012-2016 Sebastian Geerken <sgeerken@dillo.org>
 * Copyright 2012-2013 Johannes Hofmann <Johannes.Hofmann@gmx.de>
 * Copyright 2023 Rodrigo Arias Mallo <rodarima@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Tests the hyphenation of words in different languages with the Liang
 * algorithm. The hyphenator requires the .pat pattern files which can
 * be downloaded from CTAN. */

#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include "dw/fltkcore.hh"
#include "dw/hyphenator.hh"

dw::fltk::FltkPlatform *platform;

void hyph(dw::Hyphenator *h, const char *word, const char *parts)
{
	int p = 0;
	char buf[1024];
	int numBreaks;
	int *breakPos = h->hyphenateWord(platform, word, &numBreaks);
	memset(buf, 0, 1024);
	for (int i = 0; i < numBreaks + 1; i++) {
		int start = (i == 0 ? 0 : breakPos[i - 1]);
		int end = (i == numBreaks ? strlen (word) : breakPos[i]);
		if (i != 0)
			buf[p++] = '-';
		for (int j = start; j < end; j++)
			buf[p++] = word[j];
	}

	if (strcmp(parts, buf) != 0) {
		fprintf(stderr, "mismatch input=%s output=%s expected=%s\n",
				word, buf, parts);
		exit(1);
	}

	printf("%s\n", buf);

	if (breakPos)
		free(breakPos);
}

dw::Hyphenator get_hyphenator(const char *path)
{
	if (access(path, F_OK) != 0) {
		fprintf(stderr, "cannot access %s file: %s", path,
				strerror(errno));
		exit(1);
	}

	return dw::Hyphenator(path, "", 512);
}

void hyph_en_us()
{
	dw::Hyphenator h = get_hyphenator(CUR_SRC_DIR "/hyph-en-us.pat");

	hyph(&h, "supercalifragilisticexpialidocious", "su-per-cal-ifrag-ilis-tic-ex-pi-ali-do-cious");
	hyph(&h, "incredible", "in-cred-i-ble");
	hyph(&h, "hyphenation", "hy-phen-ation");
	hyph(&h, "...", "...");
}

void hyph_de()
{
	dw::Hyphenator h = get_hyphenator(CUR_SRC_DIR "/hyph-de.pat");

	hyph(&h, "...", "...");
	hyph(&h, "weiß", "weiß");
	hyph(&h, "Ackermann", "Acker-mann");
	hyph(&h, "Grundstücksverkehrsgenehmigungszuständigkeits",
			"Grund-stücks-ver-kehrs-ge-neh-mi-gungs-zu-stän-dig-keits");
	hyph(&h, "Donaudampfschifffahrtskapitänsmützenknopf",
			"Do-nau-dampf-schiff-fahrts-ka-pi-täns-müt-zen-knopf");
	hyph(&h, "www.dillo.org", "www.dil-lo.org");
}

int main(void)
{
	platform = new dw::fltk::FltkPlatform();

	hyph_en_us();
	hyph_de();

   return 0;
}
