/*
 * Library for dealing with dpip tags (dillo plugin protocol tags).
 */

#ifndef __DPIP_H__
#define __DPIP_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*
 * Printf like function for building dpip commands.
 * It takes care of dpip escaping of its arguments.
 * NOTE : It ONLY accepts string parameters, and
 *        only one %s per parameter.
 */
char *a_Dpip_build_cmd(const char *format, ...);

/*
 * Task: given a tag and an attribute name, return its value.
 *       (dpip character escaping is removed here)
 * Return value: the attribute value, or NULL if not present or malformed.
 */
char *a_Dpip_get_attr(char *tag, size_t tagsize, char *attrname);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DPIP_H__ */

