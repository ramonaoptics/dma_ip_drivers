/*
 * This file is part of the QDMA userspace application
 * to enable the user to execute the QDMA functionality
 *
 * Copyright (c) 2018-present,  Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is licensed under BSD-style license (found in the
 * LICENSE file in the root directory of this source tree)
 */

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/genetlink.h>
#include <qdma_nl.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>

#include "nl_user.h"
#include "cmd_parse.h"

void xnl_close(struct xnl_cb *cb)
{
	// closed automatically
	nl_socket_free(cb->sk);
}

static int xnl_send(struct xnl_cb *cb, struct nl_msg * msg)
{
	int rv;
	struct nlmsghdr* hdr = nlmsg_hdr(msg);
	// This is a hack for now, we should not have to convert from a
	// header to message
	rv = nl_send_auto(cb->sk, msg);
	if (rv != hdr->nlmsg_len) {
		perror("nl send err");
		return -1;
	}
	return 0;
}

static int xnl_recv(struct xnl_cb *cb, struct nl_msg *msg, int dlen)
{
	int rv;
	// I feel like I need to justy the use of such a low level function.
	// All the other recv functions I found in libnl allocate the
	// message for you. This is pretty unecessary since we already know
	// the approximate message size. Therefore, I don't want to use them.
	// We basically recreated a simple version of this stuff.
	int fd = nl_socket_get_fd(cb->sk);
	struct nlmsghdr * hdr = nlmsg_hdr(msg);
	struct genlmsghdr * gehdr = genlmsg_hdr(hdr);

	// set the netlink header to 0
	memset(hdr, 0, NLMSG_HDRLEN);

	rv = recv(fd, hdr, dlen, 0);
	if (rv < 0) {
		perror("nl recv err");
		return -1;
	}
	/* as long as there is attribute, even if it is shorter than expected */
	if (!NLMSG_OK(hdr, rv) && (rv <= NLMSG_HDRLEN + NLMSG_LENGTH(GENL_HDRLEN))) {
		fprintf(stderr,
			"nl recv:, invalid message, cmd 0x%x, %d,%d.\n",
			gehdr->cmd, dlen, rv);
		return -1;
	}

	if (hdr->nlmsg_type == NLMSG_ERROR) {
		fprintf(stderr, "nl recv, msg error, cmd 0x%x\n",
			gehdr->cmd);
		return -1;
	}

	return 0;
}

int xnl_connect(struct xnl_cb *cb, int vf)
{
	struct nl_sock *sk;
	int rv = -1;
	int fam;

	sk = nl_socket_alloc();
  if (sk == NULL){
    fprintf(stderr, "%s, error alocating netlink socket.\n", __FUNCTION__);
		rv = -ENOMEM;
    goto fail_socket_alloc;
  }
	rv = genl_connect(sk);
	if (rv != 0){
		printf("Could not connect to socket\n");
		goto fail_nl_connect;
	}

	if(vf){
		fam = genl_ctrl_resolve(sk, XNL_NAME_VF);
	} else{
		fam = genl_ctrl_resolve(sk, XNL_NAME_PF);
	}
	if (fam < 0){
		printf("Error resolving family name.");
		goto fail_family_resolve;
	}

	// Xilinx's driver doesn't ack messages.
	nl_socket_disable_auto_ack(sk);
	cb->family = fam;
	cb->sk = sk;
	return 0;
fail_family_resolve:
fail_nl_connect:
  nl_socket_free(sk);
fail_socket_alloc:
  return rv;
}

/** Take the message from the netlink socket, and put the relevant info in xcmd
 *
 *
 */
static int recv_attrs(struct nl_msg *msg, struct xcmd_info *xcmd)
{
	struct genlmsghdr* g = genlmsg_hdr(nlmsg_hdr(msg));
	unsigned char *p = genlmsg_user_data(g, 0);
	int maxlen = genlmsg_user_datalen(g, 0);

#if 0
	printf("nl recv, hdr len %d, data %d, gen op 0x%x, %s, ver 0x%x.\n",
		hdr->n.nlmsg_len, maxlen, hdr->g.cmd, xnl_op_str[hdr->g.cmd],
		hdr->g.version);
#endif

	xcmd->attr_mask = 0;
	while (maxlen > 0) {
		struct nlattr *na = (struct nlattr *)p;
		int len = NLA_ALIGN(na->nla_len);

		if (na->nla_type >= XNL_ATTR_MAX) {
			fprintf(stderr, "unknown attr type %d, len %d.\n",
				na->nla_type, na->nla_len);
			return -EINVAL;
		}

		xcmd->attr_mask |= 1 << na->nla_type;

		if (na->nla_type == XNL_ATTR_GENMSG) {
			printf("%s\n", (char *)(na + 1));
		} else if (na->nla_type == XNL_ATTR_DRV_INFO) {
			strncpy(xcmd->drv_str, (char *)(na + 1), 128);
		} else {
			xcmd->attrs[na->nla_type] = *(uint32_t *)(na + 1);
		}

		p += len;
		maxlen -= len;
	}

	return 0;
}

static void get_dev_stat(struct xcmd_info *xcmd)
{
	unsigned long long mmh2c_pkts;
	unsigned long long mmc2h_pkts;
	unsigned long long sth2c_pkts;
	unsigned long long stc2h_pkts;
	unsigned int pkts;

	pkts = xcmd->attrs[XNL_ATTR_DEV_STAT_MMH2C_PKTS1];
	mmh2c_pkts = pkts;
	pkts = xcmd->attrs[XNL_ATTR_DEV_STAT_MMH2C_PKTS2];
	mmh2c_pkts |= (((unsigned long long)pkts) << 32);

	pkts = xcmd->attrs[XNL_ATTR_DEV_STAT_MMC2H_PKTS1];
	mmc2h_pkts = pkts;
	pkts = xcmd->attrs[XNL_ATTR_DEV_STAT_MMC2H_PKTS2];
	mmc2h_pkts |= (((unsigned long long)pkts) << 32);

	pkts = xcmd->attrs[XNL_ATTR_DEV_STAT_STH2C_PKTS1];
	sth2c_pkts = pkts;
	pkts = xcmd->attrs[XNL_ATTR_DEV_STAT_STH2C_PKTS2];
	sth2c_pkts |= (((unsigned long long)pkts) << 32);

	pkts = xcmd->attrs[XNL_ATTR_DEV_STAT_STC2H_PKTS1];
	stc2h_pkts = pkts;
	pkts = xcmd->attrs[XNL_ATTR_DEV_STAT_STC2H_PKTS2];
	stc2h_pkts |= (((unsigned long long)pkts) << 32);

	printf("qdma%s%05x:statistics\n", xcmd->vf ? "vf" : "", xcmd->if_bdf);
	printf("Total MM H2C packets processed = %llu\n", mmh2c_pkts);
	printf("Total MM C2H packets processed = %llu\n", mmc2h_pkts);
	printf("Total ST H2C packets processed = %llu\n", sth2c_pkts);
	printf("Total ST C2H packets processed = %llu\n", stc2h_pkts);
}

static int recv_nl_msg(struct nl_msg *msg, struct xcmd_info *xcmd)
{
	struct genlmsghdr* g = genlmsg_hdr(nlmsg_hdr(msg));
	unsigned int op = g->cmd;
	unsigned int usr_bar;

	recv_attrs(msg, xcmd);

	switch(op) {
	case XNL_CMD_DEV_LIST:
		break;
	case XNL_CMD_DEV_INFO:
		xcmd->config_bar = xcmd->attrs[XNL_ATTR_DEV_CFG_BAR];
		usr_bar = (int)xcmd->attrs[XNL_ATTR_DEV_USR_BAR];
		xcmd->qmax = xcmd->attrs[XNL_ATTR_DEV_QSET_MAX];
		xcmd->stm_bar = xcmd->attrs[XNL_ATTR_DEV_STM_BAR];

		if (usr_bar+1 == 0)
			xcmd->user_bar = 2;
		else
			xcmd->user_bar = usr_bar;

#ifdef DEBUG
		printf("qdma%s%05x:\t%02x:%02x.%02x\t",
			xcmd->vf ? "vf" : "", xcmd->if_bdf,
			xcmd->attrs[XNL_ATTR_PCI_BUS],
			xcmd->attrs[XNL_ATTR_PCI_DEV],
			xcmd->attrs[XNL_ATTR_PCI_FUNC]);
		printf("config bar: %d, user bar: %d, max #. QP: %d\n",
			xcmd->config_bar, xcmd->user_bar, xcmd->qmax);
#endif
		break;
	case XNL_CMD_DEV_STAT:
		get_dev_stat(xcmd);
		break;
	case XNL_CMD_DEV_STAT_CLEAR:
		break;
	case XNL_CMD_VERSION:
		break;
	case XNL_CMD_Q_LIST:
		break;
	case XNL_CMD_Q_ADD:
		break;
	case XNL_CMD_Q_START:
		break;
	case XNL_CMD_Q_STOP:
		break;
	case XNL_CMD_Q_DEL:
		break;
	case XNL_CMD_REG_RD:
		xcmd->u.reg.val = xcmd->attrs[XNL_ATTR_REG_VAL];
		break;
	case XNL_CMD_REG_WRT:
		xcmd->u.reg.val = xcmd->attrs[XNL_ATTR_REG_VAL];
		break;
	default:
		break;
	}

	return 0;
}

static void xnl_msg_add_extra_config_attrs(struct nl_msg *msg,
                                       struct xcmd_info *xcmd)
{
	if (xcmd->u.qparm.sflags & (1 << QPARM_RNGSZ_IDX))
		nla_put_u32(msg, XNL_ATTR_QRNGSZ_IDX,
		                     xcmd->u.qparm.qrngsz_idx);
	if (xcmd->u.qparm.sflags & (1 << QPARM_C2H_BUFSZ_IDX))
		nla_put_u32(msg, XNL_ATTR_C2H_BUFSZ_IDX,
		                     xcmd->u.qparm.c2h_bufsz_idx);
	if (xcmd->u.qparm.sflags & (1 << QPARM_CMPTSZ))
		nla_put_u32(msg,  XNL_ATTR_CMPT_DESC_SIZE,
		                     xcmd->u.qparm.cmpt_entry_size);
	if (xcmd->u.qparm.sflags & (1 << QPARM_SW_DESC_SZ))
		nla_put_u32(msg,  XNL_ATTR_SW_DESC_SIZE,
		                     xcmd->u.qparm.sw_desc_sz);
	if (xcmd->u.qparm.sflags & (1 << QPARM_CMPT_TMR_IDX))
		nla_put_u32(msg,  XNL_ATTR_CMPT_TIMER_IDX,
		                     xcmd->u.qparm.cmpt_tmr_idx);
	if (xcmd->u.qparm.sflags & (1 << QPARM_CMPT_CNTR_IDX))
		nla_put_u32(msg,  XNL_ATTR_CMPT_CNTR_IDX,
		                     xcmd->u.qparm.cmpt_cntr_idx);
	if (xcmd->u.qparm.sflags & (1 << QPARM_CMPT_TRIG_MODE))
		nla_put_u32(msg,  XNL_ATTR_CMPT_TRIG_MODE,
		                     xcmd->u.qparm.cmpt_trig_mode);
	if (xcmd->u.qparm.sflags & (1 << QPARM_PIPE_GL_MAX))
		nla_put_u32(msg,  XNL_ATTR_PIPE_GL_MAX,
		                     xcmd->u.qparm.pipe_gl_max);
	if (xcmd->u.qparm.sflags & (1 << QPARM_PIPE_FLOW_ID))
		nla_put_u32(msg,  XNL_ATTR_PIPE_FLOW_ID,
		                     xcmd->u.qparm.pipe_flow_id);
	if (xcmd->u.qparm.sflags & (1 << QPARM_PIPE_SLR_ID))
		nla_put_u32(msg,  XNL_ATTR_PIPE_SLR_ID,
		                     xcmd->u.qparm.pipe_slr_id);
	if (xcmd->u.qparm.sflags & (1 << QPARM_PIPE_TDEST))
		nla_put_u32(msg,  XNL_ATTR_PIPE_TDEST,
		                     xcmd->u.qparm.pipe_tdest);
}

static int get_cmd_resp_buf_len(struct xcmd_info *xcmd)
{
	int buf_len = XNL_RESP_BUFLEN_MAX;
	unsigned int row_len = 50;

	switch (xcmd->op) {
	        case XNL_CMD_Q_DESC:
	        	row_len *= 2;
	        case XNL_CMD_Q_CMPT:
	        	buf_len += ((xcmd->u.qparm.range_end -
	        			xcmd->u.qparm.range_start)*row_len);
	        	break;
	        case XNL_CMD_INTR_RING_DUMP:
	        	buf_len += ((xcmd->u.intr.end_idx -
					     xcmd->u.intr.start_idx)*row_len);
	        	break;
	        case XNL_CMD_DEV_LIST:
	        case XNL_CMD_Q_START:
	        case XNL_CMD_Q_STOP:
	        case XNL_CMD_Q_DEL:
	        	return buf_len;
	        case XNL_CMD_Q_LIST:
	        case XNL_CMD_Q_DUMP:
	        	break;
	        default:
	        	buf_len = XNL_RESP_BUFLEN_MIN;
	        	break;
	}
	if ((xcmd->u.qparm.flags & XNL_F_QDIR_BOTH) == XNL_F_QDIR_BOTH)
		buf_len *= 2;
	if (xcmd->u.qparm.flags & XNL_F_QDIR_BOTH)
		buf_len *= xcmd->u.qparm.num_q;

	return buf_len;
}

#define genlmsg_alloc_size(size) nlmsg_alloc_size(NLMSG_LENGTH(GENL_HDRLEN) + (size))

int xnl_send_cmd(struct xnl_cb *cb, struct xcmd_info *xcmd)
{

	int dlen = get_cmd_resp_buf_len(xcmd);
	int i;
	int rv;
	enum xnl_st_c2h_cmpt_desc_size cmpt_desc_size;
	struct sockaddr_nl addr;
	addr.nl_family = cb->family;
	addr.nl_pid = 0; // auto
	addr.nl_groups = 0; // not used

	struct nl_msg * msg =	genlmsg_alloc_size(dlen);

	if (!msg) {
		fprintf(stderr, "%s: OOM, %s, op %s,0x%x.\n", __FUNCTION__,
			xcmd->ifname, xnl_op_str[xcmd->op], xcmd->op);
		return -ENOMEM;
	}
	nlmsg_set_dst(msg, &addr);

	void * ret = genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, cb->family,
	 						0, 0,  xcmd->op, 0);
	if (ret == NULL){
		goto out;
	}

	nla_put_u32(msg, XNL_ATTR_DEV_IDX, xcmd->if_bdf);

	switch(xcmd->op) {
        case XNL_CMD_DEV_LIST:
        case XNL_CMD_DEV_INFO:
        case XNL_CMD_DEV_STAT:
        case XNL_CMD_DEV_STAT_CLEAR:
        case XNL_CMD_Q_LIST:
		/* no parameter */
		break;
        case XNL_CMD_Q_ADD:
		nla_put_u32(msg, XNL_ATTR_QIDX, xcmd->u.qparm.idx);
		nla_put_u32(msg, XNL_ATTR_NUM_Q, xcmd->u.qparm.num_q);
		nla_put_u32(msg, XNL_ATTR_QFLAG, xcmd->u.qparm.flags);
		break;
        case XNL_CMD_Q_START:
        	xnl_msg_add_extra_config_attrs(msg, xcmd);
        case XNL_CMD_Q_STOP:
        case XNL_CMD_Q_DEL:
        case XNL_CMD_Q_DUMP:
		nla_put_u32(msg, XNL_ATTR_QIDX, xcmd->u.qparm.idx);
		nla_put_u32(msg, XNL_ATTR_NUM_Q, xcmd->u.qparm.num_q);
		nla_put_u32(msg, XNL_ATTR_QFLAG, xcmd->u.qparm.flags);
		break;
        case XNL_CMD_Q_DESC:
        case XNL_CMD_Q_CMPT:
		nla_put_u32(msg, XNL_ATTR_QIDX, xcmd->u.qparm.idx);
		nla_put_u32(msg, XNL_ATTR_NUM_Q, xcmd->u.qparm.num_q);
		nla_put_u32(msg, XNL_ATTR_QFLAG, xcmd->u.qparm.flags);
		nla_put_u32(msg, XNL_ATTR_RANGE_START,
					xcmd->u.qparm.range_start);
		nla_put_u32(msg, XNL_ATTR_RANGE_END,
					xcmd->u.qparm.range_end);
		nla_put_u32(msg, XNL_ATTR_RSP_BUF_LEN, dlen);
		break;
        case XNL_CMD_Q_RX_PKT:
		nla_put_u32(msg, XNL_ATTR_QIDX, xcmd->u.qparm.idx);
		nla_put_u32(msg, XNL_ATTR_NUM_Q, xcmd->u.qparm.num_q);
		/* hard coded to C2H */
		nla_put_u32(msg, XNL_ATTR_QFLAG, XNL_F_QDIR_C2H);
		break;
        case XNL_CMD_INTR_RING_DUMP:
		nla_put_u32(msg, XNL_ATTR_INTR_VECTOR_IDX,
		                     xcmd->u.intr.vector);
		nla_put_u32(msg, XNL_ATTR_INTR_VECTOR_START_IDX,
		                     xcmd->u.intr.start_idx);
		nla_put_u32(msg, XNL_ATTR_INTR_VECTOR_END_IDX,
		                     xcmd->u.intr.end_idx);
		nla_put_u32(msg, XNL_ATTR_RSP_BUF_LEN, dlen);
		break;
        case XNL_CMD_REG_RD:
		nla_put_u32(msg, XNL_ATTR_REG_BAR_NUM,
    							 xcmd->u.reg.bar);
		nla_put_u32(msg, XNL_ATTR_REG_ADDR,
							 xcmd->u.reg.reg);
		break;
        case XNL_CMD_REG_WRT:
		nla_put_u32(msg, XNL_ATTR_REG_BAR_NUM,
    							 xcmd->u.reg.bar);
		nla_put_u32(msg, XNL_ATTR_REG_ADDR,
							 xcmd->u.reg.reg);
		nla_put_u32(msg, XNL_ATTR_REG_VAL,
							 xcmd->u.reg.val);
		break;
#ifdef ERR_DEBUG
        case XNL_CMD_Q_ERR_INDUCE:
		nla_put_u32(msg, XNL_ATTR_QIDX, xcmd->u.qparm.idx);
		nla_put_u32(msg, XNL_ATTR_QFLAG, xcmd->u.qparm.flags);
		nla_put_u32(msg, XNL_ATTR_QPARAM_ERR_INFO,
		                     xcmd->u.qparm.err_info);
		break;
#endif
	default:
		break;
	}

	rv = xnl_send(cb, msg);
	if (rv < 0)
		goto out;

	rv = xnl_recv(cb, msg, dlen);
	if (rv < 0)
		goto out;

	rv = recv_nl_msg(msg, xcmd);
out:
	nlmsg_free(msg);
	return rv;
}
