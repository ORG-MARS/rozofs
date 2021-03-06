#ifndef __rozofs_cluster_admin_status_e2String_h__
#define __rozofs_cluster_admin_status_e2String_h__
#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/
#include <strings.h>

/*___________________________________________________________________
 
   Generated by enum2String.py 
   Date : Friday 2019 March 22, 12:55:37
   Command line : 
 ../tools/enum2String.py -n rozofs_cluster_admin_status_e -f rozofs.h -c 28 -r

  ____________________________________________________________________
*/

/*_________________________________________________________________
 * Builds a string from an integer value supposed to be within
 * the enumerated list rozofs_cluster_admin_status_e
 *
 * @param x : value from rozofs_cluster_admin_status_e to translate into a string
 *
 * The input value is translated into a string deduced from the enum
 * definition. When the input value do not fit any of the predefined
 * values, "??" is returned
 *
 * @return A char pointer to the constant string or "??"
 *_________________________________________________________________*/ 
static inline char * rozofs_cluster_admin_status_e2String (const rozofs_cluster_admin_status_e x) {
  switch(x) {
    case rozofs_cluster_admin_status_undefined   : return("undefined");
    case rozofs_cluster_admin_status_in_service  : return("in service");
    case rozofs_cluster_admin_status_frozen      : return("frozen");
    /* Unexpected value */
    default: return "??";
  }
}
/*_________________________________________________________________
 * Translate a string supposed to be within the enumerated list
 * rozofs_cluster_admin_status_e to its integer value.
 *
 * @param s : the string to translate into an integer
 *
 * The input string is translated into its corresponding integer value.
 * When the input value do not fit any expected string -1 is returned.
 *
 * @return The integer value or -1
 *_________________________________________________________________*/ 
static inline int string2rozofs_cluster_admin_status_e (const char * s) {
  if (strcasecmp(s,"undefined")==0)  	return rozofs_cluster_admin_status_undefined;
  if (strcasecmp(s,"in service")==0)  	return rozofs_cluster_admin_status_in_service;
  if (strcasecmp(s,"frozen")==0)  	return rozofs_cluster_admin_status_frozen;
  /* Unexpected value */
  return -1;
}

#ifdef	__cplusplus
}
#endif
#endif

