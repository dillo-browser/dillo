#ifndef __TABLE_HH__
#define __TABLE_HH__

/*
 * Classes
 */

class DilloHtml;

/*
 * Table parsing functions
 */

void Html_tag_open_table(DilloHtml *html, const char *tag, int tagsize);
void Html_tag_content_table(DilloHtml *html, const char *tag, int tagsize);
void Html_tag_open_tr(DilloHtml *html, const char *tag, int tagsize);
void Html_tag_content_tr(DilloHtml *html, const char *tag, int tagsize);
void Html_tag_open_td(DilloHtml *html, const char *tag, int tagsize);
void Html_tag_content_td(DilloHtml *html, const char *tag, int tagsize);
void Html_tag_open_th(DilloHtml *html, const char *tag, int tagsize);
void Html_tag_content_th(DilloHtml *html, const char *tag, int tagsize);

#endif /* __TABLE_HH__ */
