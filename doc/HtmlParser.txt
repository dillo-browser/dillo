 October 2001, --Jcid
 Last update: Jul 2009

                        ---------------
                        THE HTML PARSER
                        ---------------


   Dillo's  parser is more than just a HTML parser, it does XHTML
and  plain  text  also.  It  has  parsing 'modes' that define its
behaviour while working:

   typedef enum {
     DILLO_HTML_PARSE_MODE_INIT = 0,
     DILLO_HTML_PARSE_MODE_STASH,
     DILLO_HTML_PARSE_MODE_STASH_AND_BODY,
     DILLO_HTML_PARSE_MODE_BODY,
     DILLO_HTML_PARSE_MODE_VERBATIM,
     DILLO_HTML_PARSE_MODE_PRE
   } DilloHtmlParseMode;


   The  parser  works  upon a token-grained basis, i.e., the data
stream is parsed into tokens and the parser is fed with them. The
process  is  simple:  whenever  the  cache  has new data, it is
passed to Html_write, which groups data into tokens and calls the
appropriate functions for the token type (tag, space, or word).

   Note:   when  in  DILLO_HTML_PARSE_MODE_VERBATIM,  the  parser
doesn't  try  to  split  the  data stream into tokens anymore; it
simply collects until the closing tag.

------
TOKENS
------

  * A chunk of WHITE SPACE     --> Html_process_space


  * TAG                        --> Html_process_tag

    The tag-start is defined by two adjacent characters:

      first : '<'
      second: ALPHA | '/' | '!' | '?'

      Note: comments are discarded ( <!-- ... --> )


    The tag's end is not as easy to find, nor to deal with!:

    1)  The  HTML  4.01  sec.  3.2.2 states that "Attribute/value
    pairs appear before the final '>' of an element's start tag",
    but it doesn't define how to discriminate the "final" '>'.

    2) '<' and '>' should be escaped as '&lt;' and '&gt;' inside
    attribute values.

    3)  The XML SPEC for XHTML states:
      AttrValue ::== '"' ([^<&"] | Reference)* '"'   |
                     "'" ([^<&'] | Reference)* "'"

    Current  parser  honors  the XML SPEC.

    As  it's  a  common  mistake  for human authors to mistype or
    forget  one  of  the  quote  marks of an attribute value; the
    parser   solves  the  problem  with  a  look-ahead  technique
    (otherwise  the  parser  could  skip significant amounts of
    properly-written HTML).



  * WORD                       --> Html_process_word

    A  word is anything that doesn't start with SPACE, that's
    outside  of  a  tag, up to the first SPACE or tag start.

    SPACE = ' ' | \n | \r | \t | \f | \v


-----------------
THE PARSING STACK
-----------------

  The parsing state of the document is kept in a stack:

  class DilloHtml {
     [...]
     lout::misc::SimpleVector<DilloHtmlState> *stack;
     [...]
  };

  struct _DilloHtmlState {
     CssPropertyList *table_cell_props;
     DilloHtmlParseMode parse_mode;
     DilloHtmlTableMode table_mode;
     bool cell_text_align_set;
     DilloHtmlListMode list_type;
     int list_number;

     /* TagInfo index for the tag that's being processed */
     int tag_idx;

     dw::core::Widget *textblock, *table;

     /* This is used to align list items (especially in enumerated lists) */
     dw::core::Widget *ref_list_item;

     /* This is used for list items etc; if it is set to TRUE, breaks
        have to be "handed over" (see Html_add_indented and
        Html_eventually_pop_dw). */
     bool hand_over_break;
  };

  Basically,  when a TAG is processed, a new state is pushed into
the  'stack'  and  its  'style'  is  set  to  reflect the desired
appearance (details in DwStyle.txt).

  That way, when a word is processed later (added to the Dw), all
the information is within the top state.

  Closing TAGs just pop the stack.


