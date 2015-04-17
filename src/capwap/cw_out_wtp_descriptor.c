#include "log.h"
#include "conn.h"

#include "itemstore.h"
#include "capwap_items.h"
#include "capwap.h"


static int cw_put_enc_subelems(uint8_t *dst)
{
	int n=2;
	
	dst+=cw_put_byte(dst,n);	

	int i;
	for (i=0; i<n; i++){
		dst+=cw_put_byte(dst,0);
		dst+=cw_put_byte(dst,0);
		dst+=cw_put_byte(dst,0);
	}

	return 3*n+1;
}



int cw_out_wtp_descriptor(struct conn *conn, struct cw_action_out *a, uint8_t * dst)
{
	// XXX Dummy WTP Descriptor Header
	uint8_t *d = dst+4;

	d+=cw_put_byte(d,2); 
	d+=cw_put_byte(d,0);
	d+=cw_put_enc_subelems(dst);

	cw_item_t * i;
	i = cw_itemstore_get(conn->outgoing,CW_ITEM_WTP_HARDWARE_VERSION);
	if ( i ) {	
	 	d += cw_put_version(d,CW_SUBELEM_WTP_HARDWARE_VERSION,i->data);
	}
	else {
		cw_log(LOG_ERR, "Can't send hard version in WTP descriptor, not set.");
	}


	int len = d-dst-4;
	return len + cw_put_elem_hdr(dst,a->elem_id,len);
}	