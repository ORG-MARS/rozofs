/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#include <stdio.h>
#include <stdlib.h>
#include <rpc/pmap_clnt.h>
#include <string.h>
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifndef SIG_PF
#define SIG_PF void(*)(int)
#endif
#include <rozofs/rozofs.h>
#define GEO_SYNC_FILENAME_MAX 32

#include <rozofs/rozofs.h>
#include <rozofs/core/rozofs_rpc_non_blocking_generic_srv.h>
#include <rozofs/core/ruc_buffer_api.h>
#include <rozofs/rpc/rozofs_rpc_util.h>
#include "geo_replica_proto_nb.h"
#include "geo_replica_protosvc_nb.h"



/*
**__________________________________________________________________________
*/
/**
  Server callback  for geo replication protocol:

  That callback is called upon receiving a GW_PROGRAM message
  from the master exportd

    
  @param socket_ctx_p: pointer to the af unix socket
  @param socketId: reference of the socket (not used)
 
   @retval : TRUE-> xmit ready event expected
  @retval : FALSE-> xmit  ready event not expected
*/
void geo_replica_req_rcv_cbk(void *userRef,uint32_t  socket_ctx_idx, void *recv_buf)
{
    uint32_t  *com_hdr_p;
    rozofs_rpc_call_hdr_t   hdr;
    geo_status_ret_t  arg_err;
    char * arguments;
    int size = 0;

    rozorpc_srv_ctx_t *rozorpc_srv_ctx_p = NULL;
    
    com_hdr_p  = (uint32_t*) ruc_buf_getPayload(recv_buf); 
    com_hdr_p +=1;   /* skip the size of the rpc message */

    memcpy(&hdr,com_hdr_p,sizeof(rozofs_rpc_call_hdr_t));
    scv_call_hdr_ntoh(&hdr);
    /*
    ** allocate a context for the duration of the transaction since it might be possible
    ** that the gateway needs to interrogate the exportd and thus needs to save the current
    ** request until receiving the response from the exportd
    */
    rozorpc_srv_ctx_p = rozorpc_srv_alloc_context();
    if (rozorpc_srv_ctx_p == NULL)
    {
       fatal(" Out of rpc context");    
    }
    /*
    ** save the initial transaction id, received buffer and reference of the connection
    */
    rozorpc_srv_ctx_p->src_transaction_id = hdr.hdr.xid;
    rozorpc_srv_ctx_p->recv_buf  = recv_buf;
    rozorpc_srv_ctx_p->socketRef = socket_ctx_idx;
    
    /*
    ** Allocate buffer for decoded aeguments
    */
    rozorpc_srv_ctx_p->decoded_arg = ruc_buf_getBuffer(decoded_rpc_buffer_pool);
    if (rozorpc_srv_ctx_p->decoded_arg == NULL) {
      rozorpc_srv_ctx_p->xmitBuf = rozorpc_srv_ctx_p->recv_buf;
      rozorpc_srv_ctx_p->recv_buf = NULL;
      rozorpc_srv_ctx_p->xdr_result =(xdrproc_t) xdr_geo_status_ret_t;
      arg_err.status = GEO_FAILURE;
      arg_err.geo_status_ret_t_u.error = ENOMEM;        
      rozorpc_srv_forward_reply(rozorpc_srv_ctx_p,(char*)&arg_err);
      rozorpc_srv_release_context(rozorpc_srv_ctx_p);    
      return;
    }    
    arguments = ruc_buf_getPayload(rozorpc_srv_ctx_p->decoded_arg);

    void (*local)(void *, rozorpc_srv_ctx_t *);

    switch (hdr.proc) {
     case GEO_NULL:
	     rozorpc_srv_ctx_p->arg_decoder = (xdrproc_t) xdr_void;
	     rozorpc_srv_ctx_p->xdr_result = (xdrproc_t) xdr_void;
	     local =  geo_null_1_svc_nb;
	     break;

     case GEO_SYNC_REQ:
	     rozorpc_srv_ctx_p->arg_decoder = (xdrproc_t) xdr_geo_sync_req_arg_t;
	     rozorpc_srv_ctx_p->xdr_result = (xdrproc_t) xdr_geo_sync_req_ret_t;
	     local =  geo_sync_req_1_svc_nb;
	     size = sizeof(xdr_geo_sync_req_arg_t);
	     break;

     case GEO_SYNC_GET_NEXT_REQ:
	     rozorpc_srv_ctx_p->arg_decoder = (xdrproc_t) xdr_geo_sync_get_next_req_arg_t;
	     rozorpc_srv_ctx_p->xdr_result = (xdrproc_t) xdr_geo_sync_req_ret_t;
	     local =  geo_sync_get_next_req_1_svc_nb;
	     size = sizeof(xdr_geo_sync_get_next_req_arg_t);
	     break;

     case GEO_SYNC_DELETE_REQ:
	     rozorpc_srv_ctx_p->arg_decoder = (xdrproc_t) xdr_geo_sync_delete_req_arg_t;
	     rozorpc_srv_ctx_p->xdr_result = (xdrproc_t) xdr_geo_status_ret_t;
	     local =  geo_sync_delete_req_1_svc_nb;
	     size = sizeof(xdr_geo_sync_delete_req_arg_t);
	     break;

     case GEO_SYNC_CLOSE_REQ:
	     rozorpc_srv_ctx_p->arg_decoder = (xdrproc_t) xdr_geo_sync_close_req_arg_t;
	     rozorpc_srv_ctx_p->xdr_result = (xdrproc_t) xdr_geo_status_ret_t;
	     local =  geo_sync_close_req_1_svc_nb;
	     size = sizeof(xdr_geo_sync_close_req_arg_t);
	     break;


    default:
      rozorpc_srv_ctx_p->xmitBuf = rozorpc_srv_ctx_p->recv_buf;
      rozorpc_srv_ctx_p->recv_buf = NULL;
      rozorpc_srv_ctx_p->xdr_result =(xdrproc_t) xdr_geo_status_ret_t;
      arg_err.status = GEO_FAILURE;
      arg_err.geo_status_ret_t_u.error = EPROTO;        
      rozorpc_srv_forward_reply(rozorpc_srv_ctx_p,(char*)&arg_err);
      rozorpc_srv_release_context(rozorpc_srv_ctx_p);    
      return;
    }
    
    memset(arguments,0, size);
    ruc_buf_setPayloadLen(rozorpc_srv_ctx_p->decoded_arg,size); // for debug 
    
    /*
    ** decode the payload of the rpc message
    */
    if (!rozorpc_srv_getargs_with_position (recv_buf, (xdrproc_t) rozorpc_srv_ctx_p->arg_decoder, 
                                            (caddr_t) arguments, &rozorpc_srv_ctx_p->position)) 
    {    
      rozorpc_srv_ctx_p->xmitBuf = rozorpc_srv_ctx_p->recv_buf;
      rozorpc_srv_ctx_p->recv_buf = NULL;
      rozorpc_srv_ctx_p->xdr_result = (xdrproc_t)xdr_geo_status_ret_t;
      arg_err.status = GEO_FAILURE;
      arg_err.geo_status_ret_t_u.error = errno;        
      rozorpc_srv_forward_reply(rozorpc_srv_ctx_p,(char*)&arg_err);
      /*
      ** release the context
      */
      rozorpc_srv_release_context(rozorpc_srv_ctx_p);    
      return;
    }  
    
    /*
    ** call the user call-back
    */
    (*local)(arguments, rozorpc_srv_ctx_p);    
}