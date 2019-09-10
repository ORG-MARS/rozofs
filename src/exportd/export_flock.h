/*
 Copyright (c) 2010 Fizians SAS. <http://www.fizians.com>
 This file is part of Rozofs.

 Rozofs is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published
 by the Free Software Foundation, version 2.

 Rozofs is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see
 <http://www.gnu.org/licenses/>.
 */

#ifndef _EXPORT_FLOCK_H
#define _EXPORT_FLOCK_H

#include <rozofs/rozofs.h>
#include <rozofs/common/list.h>
#include <rozofs/common/htable.h>
#include <rozofs/common/mattr.h>
#include <rozofs/rpc/eproto.h>
#include "exp_cache.h"

#define FILE_LOCK_POLL_DELAY_MAX  (common_config.client_flock_timeout)

typedef struct _rozofs_file_lock_t {
  list_t           next_fid_lock;
  list_t           next_client_lock;
  time_t           last_poll_time;
  lv2_entry_t    * lv2;
  struct ep_lock_t lock;
} rozofs_file_lock_t;


void                 lv2_cache_free_file_lock(uint32_t eid,rozofs_file_lock_t * lock) ;
rozofs_file_lock_t * lv2_cache_allocate_file_lock(lv2_entry_t * lv2,uint32_t eid, ep_lock_t * lock, ep_client_info_t * info) ;


/*
*___________________________________________________________________
* Format a string describing the lock set
*
* 
* @retval 1 when locks are compatible, 0 else
*___________________________________________________________________
*/
int rozofs_format_flockp_string(char * string,lv2_entry_t *lv2);
/*
*___________________________________________________________________
* lock service init 
*
* @param none
*___________________________________________________________________
*/
void file_lock_service_init(void);
/*
*___________________________________________________________________
* lock remove fid locks 
*
* @param lock_list: linked list of the locks
*___________________________________________________________________
*/
void file_lock_remove_fid_locks(list_t * lock_list);
/*
*___________________________________________________________________
* Remove all the locks of a client and then remove the client 
*
* @param client_ref reference of the client to remove
*___________________________________________________________________
*/
void file_lock_remove_client(uint32_t eid, uint64_t client_ref) ;
/*
*___________________________________________________________________
* Receive a poll request from a client
*
* @param client_ref reference of the client to remove
*___________________________________________________________________
*/
void file_lock_poll_client(uint32_t eid, uint64_t client_ref, ep_client_info_t * info) ;
/*
*___________________________________________________________________
* Receive a poll request from a client
*
* @param eid            export identifier
* @param client_ref     reference of the client to remove
* @param info           client information
* @param forget_locks   Client has just been restarted and has forgotten every lock
*___________________________________________________________________
*/
int file_lock_clear_client_file_lock(uint32_t eid, uint64_t client_ref, ep_client_info_t * info);
/*
*___________________________________________________________________
* Receive a poll request from a client
*
* @param eid            export identifier
* @param lv2            File information in cache
* @param client_ref     reference of the client/owner to remove
* @param info           client information
*___________________________________________________________________
*/
int file_lock_poll_owner(uint32_t eid, lv2_entry_t * lv2, ep_lock_t * lock, ep_client_info_t * info) ;
/*
*___________________________________________________________________
* Check whether two lock2 must free or update lock1
*
* @param bsize       The blok size as defined in ROZOFS_BSIZE_E
* @param lock_free   The free lock operation
* @param lock_set    The set lock that must be checked
*
* @retval 1 when locks are compatible, 0 else
*___________________________________________________________________
*/
int must_file_lock_be_removed(lv2_entry_t * lv2,uint32_t eid,uint8_t bsize, struct ep_lock_t * lock_free, struct ep_lock_t * lock_set, rozofs_file_lock_t ** new_lock_ctx, ep_client_info_t * info) ;
/*
*___________________________________________________________________
* Check whether two locks are compatible in oreder to set a new one.
* We have to check the effective range and not the user range
*
* @param lock1   1rst lock
* @param lock2   2nd lock
* 
* @retval 1 when locks are compatible, 0 else
*___________________________________________________________________
*/
int are_file_locks_compatible(struct ep_lock_t * lock1, struct ep_lock_t * lock2) ;
/*
*___________________________________________________________________
* Check whether two locks are overlapping. This has to be check at user 
* level in order to merge the different requested locks into one.
*
* @param lock1   1rst lock
* @param lock2   2nd lock
*
* @retval 1 when locks overlap, 0 else
*___________________________________________________________________
*/
int are_file_locks_overlapping(struct ep_lock_t * lock1, struct ep_lock_t * lock2);
/*
*___________________________________________________________________
* Try to concatenate overlapping locks in lock1
*
* @param bsize   The blok size as defined in ROZOFS_BSIZE_E
* @param lock1   1rst lock
* @param lock2   2nd lock
*
* @retval 1 when locks overlap, 0 else
*___________________________________________________________________
*/
int try_file_locks_concatenate(uint8_t bsize, struct ep_lock_t * lock1, struct ep_lock_t * lock2);
/*
*___________________________________________________________________
* To be called after a relaod in order to clean up deleted export
*
*___________________________________________________________________
*/
void file_lock_reload();

char * display_file_lock(char * pChar) ;
char * display_file_lock_client(char * pChar, uint64_t client_ref) ;
char * display_file_lock_clients(char * pChar);
char * display_file_lock_clients_json(char * pChar);
void rozofs_reload_flockp(lv2_entry_t * lv2, export_tracking_table_t *trk_tb_p);

#endif
