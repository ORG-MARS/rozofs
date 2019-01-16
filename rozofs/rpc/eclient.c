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

/* need for crypt */
#define _XOPEN_SOURCE 500

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <rozofs/rozofs.h>
#include <rozofs/common/log.h>
#include <rozofs/common/xmalloc.h>
#include <rozofs/core/rozofs_ip_utilities.h>

#include "eproto.h"
#include "eclient.h"


 uint32_t exportd_configuration_file_hash = 0; /**< hash value of the configuration file */

static int rozofs_msite = 0;
static int rozofs_thin = 0;

/**______________________________________________________________________________
*  Re intialize the storage direct access table 
*/
int rozofs_get_msite(void) { return rozofs_msite; }


/**______________________________________________________________________________
*  Whether thin provisionning is configured 
*/
int rozofs_get_thin_provisioning(void) { return rozofs_thin; }


/**______________________________________________________________________________
*  storage direct access table with cid&sid keys
*/
typedef mstorage_t  * storage_table_t[SID_MAX];
storage_table_t     * storage_direct[ROZOFS_CLUSTERS_MAX] = {0};

/**______________________________________________________________________________
*  Re intialize the storage direct access table 
*/
void storage_direct_reinit(void) {
  int i;
  for (i=0; i<ROZOFS_CLUSTERS_MAX; i++) {
    if (storage_direct[i] != 0) {
	  free(storage_direct[i]);
	  storage_direct[i] = NULL;
	}
  }
}
/**______________________________________________________________________________
*  Add a storage address in the storage direct access table 
*/
void storage_direct_add(mstorage_t *storage) {
  int i;
  sid_t sid;
  cid_t cid;
  
  for (i=0; i<storage->sids_nb; i++) {
  
    sid = storage->sids[i];
    cid = storage->cids[i];	

	if (storage_direct[cid] == NULL) {
      storage_direct[cid] = xmalloc(sizeof (storage_table_t));
	}

	if (storage_direct[cid] == NULL) {
      severe("allocation %d",(int)sizeof (storage_table_t));
	  return;
	}

	(*storage_direct[cid])[sid] = storage; 	
  }
}
/**______________________________________________________________________________
*  Get a storage address from the storage direct access table 
*/
mstorage_t * storage_direct_get(cid_t cid, sid_t sid) {

  if (storage_direct[cid] == NULL) {
    return NULL;
  }
  
  return (*storage_direct[cid])[sid]; 	
}






int exportclt_msite_initialize(exportclt_t * clt, const char *host, char *root,int site_number,
        const char *passwd, uint32_t bufsize, uint32_t min_read_size,
        uint32_t retries, struct timeval timeout) {
    int status = -1;
    epgw_mount_msite_ret_t *ret = 0;
    char *md5pass = 0;
    int i = 0;
    epgw_mount_arg_t args;
	int xerrno = 0;	
    DEBUG_FUNCTION;

    memset(&args,0,sizeof(epgw_mount_arg_t));
    /* Prepare mount request */
    strncpy(clt->host, host, ROZOFS_HOSTNAME_MAX);
    clt->root = strdup(root);
    clt->passwd = strdup(passwd);
    clt->retries = retries;
    clt->bufsize = bufsize;
    clt->min_read_size  = min_read_size;
    clt->timeout = timeout;

    args.hdr.gateway_rank = 0; // Not in georeplication
    args.path = clt->root ;

    rpcclt_release(&clt->rpcclt);
    //init_rpcctl_ctx(&clt->rpcclt);    
	
    /* Initialize connection with export server */
    uint16_t export_nb_port = rozofs_get_service_port_export_master_eproto();
    if (rpcclt_initialize
            (&clt->rpcclt, host, EXPORT_PROGRAM, EXPORT_VERSION,
            ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE, export_nb_port,
            clt->timeout) != 0) {
		xerrno = errno;	
        goto error;
    }
    /* Send mount request */
    ret = ep_mount_msite_1(&args, clt->rpcclt.client);
    if (ret == 0) {
        xerrno = EPROTO;
        goto error;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        xerrno = ret->status_gw.ep_mount_msite_ret_t_u.error;
		rozofs_msite = 0;
        goto error;
    }

    /* Check password */
    if (memcmp(ret->status_gw.ep_mount_msite_ret_t_u.export.md5, ROZOFS_MD5_NONE, ROZOFS_MD5_SIZE) != 0) {
        md5pass = crypt(passwd, "$1$rozofs$");
        if (memcmp(md5pass + 10, ret->status_gw.ep_mount_msite_ret_t_u.export.md5, ROZOFS_MD5_SIZE) != 0) {
            xerrno = EACCES;
            goto error;
        }
    }
    
    /*
    ** Is it a multi site config
    */
    if (ret->status_gw.ep_mount_msite_ret_t_u.export.msite & ROZOFS_EXPORT_MSITE_BIT) {
      rozofs_msite = 1;
    }
    else {
      rozofs_msite = 0;
    } 
     
    /*
    ** Is thin provisionning configured
    */      
    if (ret->status_gw.ep_mount_msite_ret_t_u.export.msite & ROZOFS_EXPORT_THIN_PROVISIONNING_BIT) {
      rozofs_thin = 1;
    }
    else {
      rozofs_thin = 0;
    }
                
    /* Copy eid, layout, root fid */
    clt->eid = ret->status_gw.ep_mount_msite_ret_t_u.export.eid;
    clt->layout = ret->status_gw.ep_mount_msite_ret_t_u.export.rl;
    clt->listen_port = ret->status_gw.ep_mount_msite_ret_t_u.export.listen_port;
    clt->bsize = ret->status_gw.ep_mount_msite_ret_t_u.export.bs;
    memcpy(clt->rfid, ret->status_gw.ep_mount_msite_ret_t_u.export.rfid, sizeof (fid_t));

    /* Initialize the list of physical storage nodes */
    list_init(&clt->storages);

    /* For each storage node */
    for (i = 0; i < ret->status_gw.ep_mount_msite_ret_t_u.export.storage_nodes_nb; i++) {

        ep_storage_node_msite_t stor_node = ret->status_gw.ep_mount_msite_ret_t_u.export.storage_nodes[i];

        /* Prepare storage node */
        mstorage_t *mstor = (mstorage_t *) xmalloc(sizeof (mstorage_t));
        memset(mstor, 0, sizeof (mstorage_t));
		mstor->site = stor_node.site;
        strncpy(mstor->host, stor_node.host, ROZOFS_HOSTNAME_MAX);
        mstor->sids_nb = stor_node.sids_nb;
        memcpy(mstor->sids, stor_node.sids, sizeof (sid_t) * stor_node.sids_nb);
        memcpy(mstor->cids, stor_node.cids, sizeof (cid_t) * stor_node.sids_nb);
	    memset(mstor->lbg_id,-1,sizeof(mstor->lbg_id));

        /* Add to the list */
        list_push_back(&clt->storages, &mstor->list);
		
		/*
		** Update direct access storage table
		*/
		storage_direct_add(mstor);
    }

    status = 0;
    goto out;
    
error:
    if (clt->root) free(clt->root);
    clt->root = NULL;
    if (clt->passwd) free(clt->passwd);
    clt->passwd = NULL;  

out:
    if (md5pass)
        free(md5pass);
    if (ret)
        xdr_free((xdrproc_t) xdr_epgw_mount_msite_ret_t, (char *) ret);
    rpcclt_release(&clt->rpcclt);

	errno = xerrno;  
    return status;
}


int exportclt_initialize(exportclt_t * clt, const char *host, char *root,int site_number,
        const char *passwd, uint32_t bufsize, uint32_t min_read_size,
        uint32_t retries, struct timeval timeout) {
    int status = -1;
    epgw_mount_msite_ret_t *ret = 0;
    char *md5pass = 0;
	int xerrno = 0;
    DEBUG_FUNCTION;
	

    /*
    ** Try a multi site mount 1rst
    */
    status = exportclt_msite_initialize(clt, host, root,site_number,passwd, 
	                              bufsize, min_read_size,retries, timeout);		
    if (status == 0) goto out;								
    
    if (clt->root) free(clt->root);
    clt->root = NULL;
    if (clt->passwd) free(clt->passwd);
    clt->passwd = NULL; 
out:
    if (md5pass)
        free(md5pass);
    if (ret)
        xdr_free((xdrproc_t) xdr_epgw_mount_msite_ret_t, (char *) ret);
    rpcclt_release(&clt->rpcclt);
	
	errno = xerrno;   
    return status;
}
		
/*
int exportclt_reload(exportclt_t * clt) {
    int status = -1;
    epgw_mount_ret_t *ret = 0;
    char *md5pass = 0;
    int i = 0;
    int j = 0;
    list_t *p, *q;
    DEBUG_FUNCTION;

    ret = ep_mount_1(&clt->root, clt->rpcclt.client);
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_mount_ret_t_u.error;
        goto out;
    }

    list_for_each_forward_safe(p, q, &clt->mcs) {
        mcluster_t *entry = list_entry(p, mcluster_t, list);
        free(entry->ms);
        list_remove(p);
        free(entry);
    }

    if (memcmp
            (ret->status_gw.ep_mount_ret_t_u.export.md5, ROZOFS_MD5_NONE,
            ROZOFS_MD5_SIZE) != 0) {
        md5pass = crypt(clt->passwd, "$1$rozofs$");
        if (memcmp
                (md5pass + 10, ret->status_gw.ep_mount_ret_t_u.export.md5,
                ROZOFS_MD5_SIZE) != 0) {
            errno = EACCES;
            goto out;
        }
    }

    clt->eid = ret->status_gw.ep_mount_ret_t_u.export.eid;
    clt->rl = ret->status_gw.ep_mount_ret_t_u.export.rl;
    memcpy(clt->rfid, ret->status_gw.ep_mount_ret_t_u.export.rfid, sizeof (fid_t));

    // Initialize the list of clusters
    list_init(&clt->mcs);

    // For each cluster
    for (i = 0; i < ret->status_gw.ep_mount_ret_t_u.export.volume.clusters_nb; i++) {

        ep_cluster_t ep_cluster = ret->status_gw.ep_mount_ret_t_u.export.volume.clusters[i];

        mcluster_t *cluster = (mcluster_t *) xmalloc(sizeof (mcluster_t));

        cluster->cid = ep_cluster.cid;
        cluster->nb_ms = ep_cluster.storages_nb;

        cluster->ms = xmalloc(ep_cluster.storages_nb * sizeof (mclient_t));

        for (j = 0; j < ep_cluster.storages_nb; j++) {

            strcpy(cluster->ms[j].host, ep_cluster.storages[j].host);
            cluster->ms[j].sid = ep_cluster.storages[j].sid;

            //Initialize the connection with the storage
            if (sclient_initialize(&cluster->ms[j]) != 0) {
                fprintf(stderr,
                        "warning failed to join storage (SID: %d): %s, %s\n",
                        ep_cluster.storages[j].sid,
                        ep_cluster.storages[j].host, strerror(errno));
            }

        }
        // Add to the list
        list_push_back(&clt->mcs, &cluster->list);
    }

    // Initialize rozofs
    if (rozofs_initialize(clt->rl) != 0) {
        fatal("can't initialise rozofs %s", strerror(errno));
        goto out;
    }

    status = 0;
out:
    if (md5pass)
        free(md5pass);
    if (ret)
        xdr_free((xdrproc_t) xdr_epgw_mount_ret_t, (char *) ret);
    return status;
}
 */

void exportclt_release(exportclt_t * clt) {
    list_t *p, *q;
    int i = 0;

    DEBUG_FUNCTION;

    /* For each storage node */
    list_for_each_forward_safe(p, q, &clt->storages) {
        mstorage_t *entry = list_entry(p, mstorage_t, list);
        /* For each connection */
        for (i = 0; i < entry->sclients_nb; i++)
            sclient_release(&entry->sclients[i]);
        list_remove(p);
        free(entry);
    }

    if (clt->passwd) {
	  free(clt->passwd);
	  clt->passwd = NULL;
	}
	if (clt->root) {
      free(clt->root);
	  clt->root = NULL;
	}  
    rpcclt_release(&clt->rpcclt);
}

int exportclt_stat(exportclt_t * clt, ep_statfs_t * st) {
    int status = -1;
    epgw_statfs_ret_t *ret = 0;
    int retry = 0;
    DEBUG_FUNCTION;

    while ((retry++ < clt->retries) &&
            (!(clt->rpcclt.client) ||
            !(ret = ep_statfs_1(&clt->eid, clt->rpcclt.client)))) {
	    
        /*
        ** release the socket if already configured to avoid losing fd descriptors
        */
        rpcclt_release(&clt->rpcclt);
	    
        if (rpcclt_initialize
                (&clt->rpcclt, clt->host, EXPORT_PROGRAM, EXPORT_VERSION,
                ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE, 0, clt->timeout) != 0) {
            rpcclt_release(&clt->rpcclt);
            errno = EPROTO;
        }
    }

    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_statfs_ret_t_u.error;
        goto out;
    }
    memcpy(st, &ret->status_gw.ep_statfs_ret_t_u.stat, sizeof (ep_statfs_t));
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_statfs_ret_t, (char *) ret);
    return status;
}

int exportclt_lookup(exportclt_t * clt, fid_t parent, char *name,
        mattr_t * attrs) {
    int status = -1;
    epgw_lookup_arg_t arg;
    epgw_mattr_ret_t *ret = 0;
    int retry = 0;
    DEBUG_FUNCTION;

    arg.arg_gw.eid = clt->eid;
    memcpy(arg.arg_gw.parent, parent, sizeof (uuid_t));
    arg.arg_gw.name = name;
    while ((retry++ < clt->retries) &&
            (!(clt->rpcclt.client) ||
            !(ret = ep_lookup_1(&arg, clt->rpcclt.client)))) {

        /*
        ** release the socket if already configured to avoid losing fd descriptors
        */
        rpcclt_release(&clt->rpcclt);
	
        if (rpcclt_initialize
                (&clt->rpcclt, clt->host, EXPORT_PROGRAM, EXPORT_VERSION,
                ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE, clt->listen_port, clt->timeout) != 0) {
            rpcclt_release(&clt->rpcclt);
            errno = EPROTO;
        }
    }

    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_mattr_ret_t_u.error;
        goto out;
    }
    memcpy(attrs, &ret->status_gw.ep_mattr_ret_t_u.attrs, sizeof (mattr_t));
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_epgw_mattr_ret_t, (char *) ret);
    return status;
}

int exportclt_getattr(exportclt_t * clt, fid_t fid, mattr_t * attrs) {
    int status = -1;
    epgw_mfile_arg_t arg;
    epgw_mattr_ret_t *ret = 0;
    int retry = 0;
    DEBUG_FUNCTION;

    arg.arg_gw.eid = clt->eid;
    memcpy(arg.arg_gw.fid, fid, sizeof (uuid_t));
    while ((retry++ < clt->retries) &&
            (!(clt->rpcclt.client) ||
            !(ret = ep_getattr_1(&arg, clt->rpcclt.client)))) {

        /*
        ** release the socket if already configured to avoid losing fd descriptors
        */
        rpcclt_release(&clt->rpcclt);
	
        if (rpcclt_initialize
                (&clt->rpcclt, clt->host, EXPORT_PROGRAM, EXPORT_VERSION,
                ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE, clt->listen_port, clt->timeout) != 0) {
            rpcclt_release(&clt->rpcclt);
            errno = EPROTO;
        }
    }

    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_mattr_ret_t_u.error;
        goto out;
    }
    memcpy(attrs, &ret->status_gw.ep_mattr_ret_t_u.attrs, sizeof (mattr_t));
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_epgw_mattr_ret_t, (char *) ret);
    return status;
}

int exportclt_setattr(exportclt_t * clt, fid_t fid, mattr_t * attrs, int to_set) {
    int status = -1;
    epgw_setattr_arg_t arg;
    epgw_mattr_ret_t *ret = 0;
    int retry = 0;
    DEBUG_FUNCTION;

    arg.arg_gw.eid = clt->eid;
    memcpy(&arg.arg_gw.attrs, attrs, sizeof (mattr_t));
    memcpy(arg.arg_gw.attrs.fid, fid, sizeof (fid_t));
    arg.arg_gw.to_set = to_set;
    while ((retry++ < clt->retries) &&
            (!(clt->rpcclt.client) ||
            !(ret = ep_setattr_1(&arg, clt->rpcclt.client)))) {

        /*
        ** release the socket if already configured to avoid losing fd descriptors
        */
        rpcclt_release(&clt->rpcclt);
	
        if (rpcclt_initialize
                (&clt->rpcclt, clt->host, EXPORT_PROGRAM, EXPORT_VERSION,
                ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE, clt->listen_port, clt->timeout) != 0) {
            rpcclt_release(&clt->rpcclt);
            errno = EPROTO;
        }
    }
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_mattr_ret_t_u.error;
        goto out;
    }
    memcpy(attrs, &ret->status_gw.ep_mattr_ret_t_u.attrs, sizeof (mattr_t));
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_epgw_mattr_ret_t, (char *) ret);
    return status;
}

int exportclt_readlink(exportclt_t * clt, fid_t fid, char *link) {
    int status = -1;
    epgw_mfile_arg_t arg;
    epgw_readlink_ret_t *ret = 0;
    int retry = 0;
    DEBUG_FUNCTION;

    arg.arg_gw.eid = clt->eid;
    memcpy(&arg.arg_gw.fid, fid, sizeof (uuid_t));

    while ((retry++ < clt->retries) &&
            (!(clt->rpcclt.client) ||
            !(ret = ep_readlink_1(&arg, clt->rpcclt.client)))) {

        /*
        ** release the socket if already configured to avoid losing fd descriptors
        */
        rpcclt_release(&clt->rpcclt);
	
        if (rpcclt_initialize
                (&clt->rpcclt, clt->host, EXPORT_PROGRAM, EXPORT_VERSION,
                ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE, clt->listen_port, clt->timeout) != 0) {
            rpcclt_release(&clt->rpcclt);
            errno = EPROTO;
        }
    }

    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_readlink_ret_t_u.error;
        goto out;
    }
    strncpy(link, ret->status_gw.ep_readlink_ret_t_u.link, PATH_MAX);
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_readlink_ret_t, (char *) ret);
    return status;
}

int exportclt_link(exportclt_t * clt, fid_t inode, fid_t newparent, char *newname, mattr_t * attrs) {
    int status = -1;
    epgw_link_arg_t arg;
    epgw_mattr_ret_t *ret = 0;
    int retry = 0;
    DEBUG_FUNCTION;

    arg.arg_gw.eid = clt->eid;
    memcpy(arg.arg_gw.inode, inode, sizeof (uuid_t));
    memcpy(arg.arg_gw.newparent, newparent, sizeof (uuid_t));
    arg.arg_gw.newname = newname;

    while ((retry++ < clt->retries) &&
            (!(clt->rpcclt.client) ||
            !(ret = ep_link_1(&arg, clt->rpcclt.client)))) {

        /*
        ** release the socket if already configured to avoid losing fd descriptors
        */
        rpcclt_release(&clt->rpcclt);
	
        if (rpcclt_initialize
                (&clt->rpcclt, clt->host, EXPORT_PROGRAM, EXPORT_VERSION,
                ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE, clt->listen_port, clt->timeout) != 0) {
            rpcclt_release(&clt->rpcclt);
            errno = EPROTO;
        }
    }

    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_mattr_ret_t_u.error;
        goto out;
    }
    memcpy(attrs, &ret->status_gw.ep_mattr_ret_t_u.attrs, sizeof (mattr_t));
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_epgw_mattr_ret_t, (char *) ret);
    return status;
}

int exportclt_mknod(exportclt_t * clt, fid_t parent, char *name, uint32_t uid,
        uint32_t gid, mode_t mode, mattr_t * attrs) {
    int status = -1;
    epgw_mknod_arg_t arg;
    epgw_mattr_ret_t *ret = 0;
    int retry = 0;
    DEBUG_FUNCTION;

    arg.arg_gw.eid = clt->eid;
    memcpy(arg.arg_gw.parent, parent, sizeof (uuid_t));
    arg.arg_gw.name = name;
    arg.arg_gw.uid = uid;
    arg.arg_gw.gid = gid;
    arg.arg_gw.mode = mode;

    while ((retry++ < clt->retries) &&
            (!(clt->rpcclt.client) ||
            !(ret = ep_mknod_1(&arg, clt->rpcclt.client)))) {

        /*
        ** release the socket if already configured to avoid losing fd descriptors
        */
        rpcclt_release(&clt->rpcclt);
	
        if (rpcclt_initialize
                (&clt->rpcclt, clt->host, EXPORT_PROGRAM, EXPORT_VERSION,
                ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE, clt->listen_port, clt->timeout) != 0) {
            rpcclt_release(&clt->rpcclt);
            errno = EPROTO;
        }
    }

    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_mattr_ret_t_u.error;
        goto out;
    }
    memcpy(attrs, &ret->status_gw.ep_mattr_ret_t_u.attrs, sizeof (mattr_t));
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_epgw_mattr_ret_t, (char *) ret);
    return status;
}

int exportclt_mkdir(exportclt_t * clt, fid_t parent, char *name, uint32_t uid,
        uint32_t gid, mode_t mode, mattr_t * attrs) {
    int status = -1;
    epgw_mkdir_arg_t arg;
    epgw_mattr_ret_t *ret = 0;
    int retry = 0;
    DEBUG_FUNCTION;

    arg.arg_gw.eid = clt->eid;
    memcpy(arg.arg_gw.parent, parent, sizeof (uuid_t));
    arg.arg_gw.name = name;
    arg.arg_gw.uid = uid;
    arg.arg_gw.gid = gid;
    arg.arg_gw.mode = mode;

    while ((retry++ < clt->retries) &&
            (!(clt->rpcclt.client) ||
            !(ret = ep_mkdir_1(&arg, clt->rpcclt.client)))) {

        /*
        ** release the socket if already configured to avoid losing fd descriptors
        */
        rpcclt_release(&clt->rpcclt);
	
        if (rpcclt_initialize
                (&clt->rpcclt, clt->host, EXPORT_PROGRAM, EXPORT_VERSION,
                ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE, clt->listen_port, clt->timeout) != 0) {
            rpcclt_release(&clt->rpcclt);
            errno = EPROTO;
        }
    }

    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_mattr_ret_t_u.error;
        goto out;
    }
    memcpy(attrs, &ret->status_gw.ep_mattr_ret_t_u.attrs, sizeof (mattr_t));
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_epgw_mattr_ret_t, (char *) ret);
    return status;
}

int exportclt_unlink(exportclt_t * clt, fid_t pfid, char *name, fid_t * fid) {
    int status = -1;
    epgw_unlink_arg_t arg;
    epgw_fid_ret_t *ret = 0;
    int retry = 0;
    DEBUG_FUNCTION;

    arg.arg_gw.eid = clt->eid;
    memcpy(arg.arg_gw.pfid, pfid, sizeof (uuid_t));
    arg.arg_gw.name = name;

    while ((retry++ < clt->retries) &&
            (!(clt->rpcclt.client) ||
            !(ret = ep_unlink_1(&arg, clt->rpcclt.client)))) {

        /*
        ** release the socket if already configured to avoid losing fd descriptors
        */
        rpcclt_release(&clt->rpcclt);
	
        if (rpcclt_initialize
                (&clt->rpcclt, clt->host, EXPORT_PROGRAM, EXPORT_VERSION,
                ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE, clt->listen_port, clt->timeout) != 0) {
            rpcclt_release(&clt->rpcclt);
            errno = EPROTO;
        }
    }

    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_fid_ret_t_u.error;
        goto out;
    }
    memcpy(fid, &ret->status_gw.ep_fid_ret_t_u.fid, sizeof (fid_t));
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_fid_ret_t, (char *) ret);
    return status;
}

int exportclt_rmdir(exportclt_t * clt, fid_t pfid, char *name, fid_t * fid) {
    int status = -1;
    epgw_rmdir_arg_t arg;
    epgw_fid_ret_t *ret = 0;
    int retry = 0;
    DEBUG_FUNCTION;

    arg.arg_gw.eid = clt->eid;
    memcpy(arg.arg_gw.pfid, pfid, sizeof (uuid_t));
    arg.arg_gw.name = name;

    while ((retry++ < clt->retries) &&
            (!(clt->rpcclt.client) ||
            !(ret = ep_rmdir_1(&arg, clt->rpcclt.client)))) {

        /*
        ** release the socket if already configured to avoid losing fd descriptors
        */
        rpcclt_release(&clt->rpcclt);
	
        if (rpcclt_initialize
                (&clt->rpcclt, clt->host, EXPORT_PROGRAM, EXPORT_VERSION,
                ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE, clt->listen_port, clt->timeout) != 0) {
            rpcclt_release(&clt->rpcclt);
            errno = EPROTO;
        }
    }

    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_fid_ret_t_u.error;
        goto out;
    }
    memcpy(fid, &ret->status_gw.ep_fid_ret_t_u.fid, sizeof (fid_t));
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_fid_ret_t, (char *) ret);
    return status;
}

int exportclt_symlink(exportclt_t * clt, char *link, fid_t parent, char *name,
        mattr_t * attrs) {
    int status = -1;
    epgw_symlink_arg_t arg;
    epgw_mattr_ret_t *ret = 0;
    int retry = 0;
    DEBUG_FUNCTION;

    arg.arg_gw.eid = clt->eid;
    arg.arg_gw.link = link;
    arg.arg_gw.name = name;
    memcpy(arg.arg_gw.parent, parent, sizeof (fid_t));

    while ((retry++ < clt->retries) &&
            (!(clt->rpcclt.client) ||
            !(ret = ep_symlink_1(&arg, clt->rpcclt.client)))) {
	    
        /*
        ** release the socket if already configured to avoid losing fd descriptors
        */
        rpcclt_release(&clt->rpcclt);
	
        if (rpcclt_initialize
                (&clt->rpcclt, clt->host, EXPORT_PROGRAM, EXPORT_VERSION,
                ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE, clt->listen_port, clt->timeout) != 0) {
            rpcclt_release(&clt->rpcclt);
            errno = EPROTO;
        }
    }

    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_mattr_ret_t_u.error;
        goto out;
    }
    memcpy(attrs, &ret->status_gw.ep_mattr_ret_t_u.attrs, sizeof (mattr_t));
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_epgw_mattr_ret_t, (char *) ret);
    return status;
}

int exportclt_rename(exportclt_t * clt, fid_t parent, char *name, fid_t newparent, char *newname, fid_t * fid) {
    int status = -1;
    epgw_rename_arg_t arg;
    epgw_rename_ret_t *ret = 0;
    int retry = 0;
    DEBUG_FUNCTION;

    arg.arg_gw.eid = clt->eid;
    memcpy(arg.arg_gw.pfid, parent, sizeof (fid_t));
    arg.arg_gw.name = name;
    memcpy(arg.arg_gw.npfid, newparent, sizeof (fid_t));
    arg.arg_gw.newname = newname;

    while ((retry++ < clt->retries) &&
            (!(clt->rpcclt.client) ||
            !(ret = ep_rename_1(&arg, clt->rpcclt.client)))) {

        /*
        ** release the socket if already configured to avoid losing fd descriptors
        */
        rpcclt_release(&clt->rpcclt);
	
        if (rpcclt_initialize
                (&clt->rpcclt, clt->host, EXPORT_PROGRAM, EXPORT_VERSION,
                ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE, clt->listen_port, clt->timeout) != 0) {
            rpcclt_release(&clt->rpcclt);
            errno = EPROTO;
        }
    }

    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_fid_ret_t_u.error;
        goto out;
    }
    memcpy(fid, &ret->status_gw.ep_fid_ret_t_u.fid, sizeof (fid_t));
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_fid_ret_t, (char *) ret);
    return status;
}


int64_t exportclt_write_block(exportclt_t * clt, fid_t fid, bid_t bid, uint32_t n, dist_t d, uint64_t off, uint32_t len) {
    int64_t length = -1;
    epgw_write_block_arg_t arg;
    epgw_mattr_ret_t *ret = 0;
    int retry = 0;
    DEBUG_FUNCTION;

    arg.arg_gw.eid = clt->eid;
    memcpy(arg.arg_gw.fid, fid, sizeof (fid_t));
    arg.arg_gw.bid = bid;
    arg.arg_gw.nrb = n;
    arg.arg_gw.length = len;
    arg.arg_gw.offset = off;

    arg.arg_gw.dist = d;
    while ((retry++ < clt->retries) &&
            (!(clt->rpcclt.client) ||
            !(ret = ep_write_block_1(&arg, clt->rpcclt.client)))) {

        /*
        ** release the socket if already configured to avoid losing fd descriptors
        */
        rpcclt_release(&clt->rpcclt);
	
        if (rpcclt_initialize
                (&clt->rpcclt, clt->host, EXPORT_PROGRAM, EXPORT_VERSION,
                ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE, clt->listen_port, clt->timeout) != 0) {
            rpcclt_release(&clt->rpcclt);
            errno = EPROTO;
        }
    }
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_mattr_ret_t_u.error;
        goto out;
    }
    length = ret->status_gw.ep_mattr_ret_t_u.attrs.size;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_epgw_mattr_ret_t, (char *) ret);
    return length;
}

int exportclt_readdir(exportclt_t * clt, fid_t fid, uint64_t * cookie, child_t ** children, uint8_t * eof) {
    int status = -1;
    epgw_readdir_arg_t arg;
    epgw_readdir_ret_t *ret = 0;
    int retry = 0;
    ep_children_t it1;
    child_t **it2;
    DEBUG_FUNCTION;

    arg.arg_gw.eid = clt->eid;
    memcpy(arg.arg_gw.fid, fid, sizeof (fid_t));
    arg.arg_gw.cookie = *cookie;

    // Send readdir request to export
    while ((retry++ < clt->retries) &&
            (!(clt->rpcclt.client) ||
            !(ret = ep_readdir_1(&arg, clt->rpcclt.client)))) {
	    
        /*
        ** release the socket if already configured to avoid losing fd descriptors
        */
        rpcclt_release(&clt->rpcclt);
	
        if (rpcclt_initialize
                (&clt->rpcclt, clt->host, EXPORT_PROGRAM, EXPORT_VERSION,
                ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE, clt->listen_port, clt->timeout) != 0) {
            rpcclt_release(&clt->rpcclt);
            errno = EPROTO;
        }
    }
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_readdir_ret_t_u.error;
        goto out;
    }

    // Copy list of children
    it2 = children;
    it1 = ret->status_gw.ep_readdir_ret_t_u.reply.children;
    while (it1 != NULL) {
        *it2 = xmalloc(sizeof (child_t));
        memcpy((*it2)->fid, it1->fid, sizeof (fid_t));
        (*it2)->name = strdup(it1->name);
        it2 = &(*it2)->next;
        it1 = it1->next;
    }
    *it2 = NULL;

    // End of readdir?
    *eof = ret->status_gw.ep_readdir_ret_t_u.reply.eof;
    *cookie = ret->status_gw.ep_readdir_ret_t_u.reply.cookie;

    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_readdir_ret_t, (char *) ret);
    return status;
}

/* not used anymore
int exportclt_open(exportclt_t * clt, fid_t fid) {

    int status = -1;
    epgw_mfile_arg_t arg;
    ep_status_ret_t *ret = 0;
    int retry = 0;
    DEBUG_FUNCTION;

    arg.arg_gw.eid = clt->eid;
    memcpy(arg.arg_gw.fid, fid, sizeof (fid_t));

    while ((retry++ < clt->retries) &&
            (!(clt->rpcclt.client) ||
            !(ret = ep_open_1(&arg, clt->rpcclt.client)))) {

        if (rpcclt_initialize
                (&clt->rpcclt, clt->host, EXPORT_PROGRAM, EXPORT_VERSION,
                ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE) != 0) {
            rpcclt_release(&clt->rpcclt);
            errno = EPROTO;
        }
    }

    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_status_ret_t_u.error;
        goto out;
    }
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_status_ret_t, (char *) ret);
    return status;
}

int exportclt_close(exportclt_t * clt, fid_t fid) {

    int status = -1;
    epgw_mfile_arg_t arg;
    ep_status_ret_t *ret = 0;
    int retry = 0;
    DEBUG_FUNCTION;

    arg.arg_gw.eid = clt->eid;
    memcpy(arg.arg_gw.fid, fid, sizeof (fid_t));

    while ((retry++ < clt->retries) &&
            (!(clt->rpcclt.client) ||
            !(ret = ep_close_1(&arg, clt->rpcclt.client)))) {

        if (rpcclt_initialize
                (&clt->rpcclt, clt->host, EXPORT_PROGRAM, EXPORT_VERSION,
                ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE) != 0) {
            rpcclt_release(&clt->rpcclt);
            errno = EPROTO;
        }
    }

    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_status_ret_t_u.error;
        goto out;
    }
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_status_ret_t, (char *) ret);
    return status;
}
 */

int exportclt_setxattr(exportclt_t * clt, fid_t fid, char * name, void* value,
        uint64_t size, uint8_t flags) {
    int status = -1;
    epgw_setxattr_arg_t arg;
    epgw_setxattr_ret_t *ret = 0;
    int retry = 0;
    DEBUG_FUNCTION;

    arg.arg_gw.eid = clt->eid;
    memcpy(arg.arg_gw.fid, fid, sizeof (fid_t));
    arg.arg_gw.name = name;
    arg.arg_gw.value.value_len = size;
    arg.arg_gw.value.value_val = value;
    arg.arg_gw.flags = flags;

    while ((retry++ < clt->retries) &&
            (!(clt->rpcclt.client) ||
            !(ret = ep_setxattr_1(&arg, clt->rpcclt.client)))) {

        /*
        ** release the socket if already configured to avoid losing fd descriptors
        */
        rpcclt_release(&clt->rpcclt);
	
        if (rpcclt_initialize
                (&clt->rpcclt, clt->host, EXPORT_PROGRAM, EXPORT_VERSION,
                ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE, clt->listen_port, clt->timeout) != 0) {
            rpcclt_release(&clt->rpcclt);
            errno = EPROTO;
        }
    }
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_status_ret_t_u.error;
        goto out;
    }

    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_status_ret_t, (char *) ret);
    return status;
}

int exportclt_getxattr(exportclt_t * clt, fid_t fid, char * name, void * value,
        uint64_t size, uint64_t * value_size) {
    int status = -1;
    epgw_getxattr_arg_t arg;
    epgw_getxattr_ret_t *ret = 0;
    int retry = 0;
    DEBUG_FUNCTION;

    arg.arg_gw.eid = clt->eid;
    memcpy(arg.arg_gw.fid, fid, sizeof (uuid_t));
    arg.arg_gw.name = name;
    arg.arg_gw.size = size;

    while ((retry++ < clt->retries) &&
            (!(clt->rpcclt.client) ||
            !(ret = ep_getxattr_1(&arg, clt->rpcclt.client)))) {

        /*
        ** release the socket if already configured to avoid losing fd descriptors
        */
        rpcclt_release(&clt->rpcclt);
	
        if (rpcclt_initialize
                (&clt->rpcclt, clt->host, EXPORT_PROGRAM, EXPORT_VERSION,
                ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE, clt->listen_port, clt->timeout) != 0) {
            rpcclt_release(&clt->rpcclt);
            errno = EPROTO;
        }
    }

    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_getxattr_ret_t_u.error;
        goto out;
    }

    if (ret->status_gw.ep_getxattr_ret_t_u.value.value_len != 0) {
        memcpy(value, ret->status_gw.ep_getxattr_ret_t_u.value.value_val, ret->status_gw.ep_getxattr_ret_t_u.value.value_len);
    }

    *value_size = ret->status_gw.ep_getxattr_ret_t_u.value.value_len;
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_getxattr_ret_t, (char *) ret);
    return status;
}

int exportclt_removexattr(exportclt_t * clt, fid_t fid, char * name) {
    int status = -1;
    epgw_removexattr_arg_t arg;
    epgw_status_ret_t *ret = 0;
    int retry = 0;
    DEBUG_FUNCTION;

    arg.arg_gw.eid = clt->eid;
    memcpy(arg.arg_gw.fid, fid, sizeof (fid_t));
    arg.arg_gw.name = name;

    while ((retry++ < clt->retries) &&
            (!(clt->rpcclt.client) ||
            !(ret = ep_removexattr_1(&arg, clt->rpcclt.client)))) {

        /*
        ** release the socket if already configured to avoid losing fd descriptors
        */
        rpcclt_release(&clt->rpcclt);
	
        if (rpcclt_initialize
                (&clt->rpcclt, clt->host, EXPORT_PROGRAM, EXPORT_VERSION,
                ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE, clt->listen_port, clt->timeout) != 0) {
            rpcclt_release(&clt->rpcclt);
            errno = EPROTO;
        }
    }
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_status_ret_t_u.error;
        goto out;
    }

    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_status_ret_t, (char *) ret);
    return status;
}

int exportclt_listxattr(exportclt_t * clt, fid_t fid, char * list,
        uint64_t size, uint64_t * list_size) {
    int status = -1;
    epgw_listxattr_arg_t arg;
    epgw_listxattr_ret_t *ret = 0;
    int retry = 0;
    DEBUG_FUNCTION;

    arg.arg_gw.eid = clt->eid;
    memcpy(arg.arg_gw.fid, fid, sizeof (uuid_t));
    arg.arg_gw.size = size;

    while ((retry++ < clt->retries) &&
            (!(clt->rpcclt.client) ||
            !(ret = ep_listxattr_1(&arg, clt->rpcclt.client)))) {

        /*
        ** release the socket if already configured to avoid losing fd descriptors
        */
        rpcclt_release(&clt->rpcclt);
	
        if (rpcclt_initialize
                (&clt->rpcclt, clt->host, EXPORT_PROGRAM, EXPORT_VERSION,
                ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE, clt->listen_port, clt->timeout) != 0) {
            rpcclt_release(&clt->rpcclt);
            errno = EPROTO;
        }
    }

    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status_gw.status == EP_FAILURE) {
        errno = ret->status_gw.ep_listxattr_ret_t_u.error;
        goto out;
    }

    if (ret->status_gw.ep_listxattr_ret_t_u.list.list_len != 0)
        memcpy(list, ret->status_gw.ep_listxattr_ret_t_u.list.list_val,
            ret->status_gw.ep_listxattr_ret_t_u.list.list_len);

    *list_size = ret->status_gw.ep_listxattr_ret_t_u.list.list_len;

    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_listxattr_ret_t, (char *) ret);
    return status;
}
