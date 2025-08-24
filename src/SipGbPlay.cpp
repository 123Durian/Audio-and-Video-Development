#include "SipGbPlay.h"
#include "Gb28181Session.h"

SipGbPlay::SipGbPlay()
{

}

SipGbPlay::~SipGbPlay()
{

}

void SipGbPlay::OnStateChanged(pjsip_inv_session *inv, pjsip_event *e)
{
	LOG(INFO)<<"OnStateChanged";
}
void SipGbPlay::OnNewSession(pjsip_inv_session *inv, pjsip_event *e)
{
	LOG(INFO)<<"OnNewSession";
}




//该代码就是检查远程的sdp是否合法，如果合法就保存ip和端口号
void SipGbPlay::OnMediaUpdate(pjsip_inv_session *inv_ses, pj_status_t status)
{
	LOG(INFO)<<"OnMediaUpdate";
    //判断invite会话是否是有效的，如果是空直接返回return
    if(NULL == inv_ses)
    {
        return;
    }


    pjsip_tx_data* tdata;
    //创建一个sdp_session对象
    const pjmedia_sdp_session* remote_sdp = NULL;
    //该函数会把当前远端的sdp赋值给remote_sdp
    pjmedia_sdp_neg_get_active_remote(inv_ses->neg,&remote_sdp);
    if(NULL == remote_sdp)
    {
        //结束当前的 SIP INVITE 会话。&tdata：输出参数，函数将生成的响应消息数据存放在 tdata 指针中
        pjsip_inv_end_session(inv_ses,PJSIP_SC_UNSUPPORTED_MEDIA_TYPE,NULL,&tdata);
        pjsip_inv_send_msg(inv_ses,tdata);
        return;
    }

    //检查媒体的个数
    if(remote_sdp->media_count <= 0 || NULL == remote_sdp->media[remote_sdp->media_count-1])
    {
        pjsip_inv_end_session(inv_ses,PJSIP_SC_FORBIDDEN,NULL,&tdata);
        pjsip_inv_send_msg(inv_ses,tdata);
        return;
    }


    //获取推流服务器的端口
    pjmedia_sdp_media* m = remote_sdp->media[remote_sdp->media_count-1];
    int sdp_port = m->desc.port;

    //获取推流服务器的ip
    pjmedia_sdp_conn* c = remote_sdp->conn;
    string ip(c->addr.ptr,c->addr.slen);

    //LOG(INFO)<<"remote rtp ip:"<<ip<<" remote rtp port:"<<sdp_port;


    //inv_ses表示一个invite会话对象，
    //动态的进行父转子
    Gb28181Session* rtpsession = dynamic_cast<Gb28181Session*>((Session*)inv_ses->mod_data[0]);
    //在回调函数中直接创建RTP
    rtpsession->CreateRtpSession(ip,sdp_port);

    return;

}

void SipGbPlay::OnSendAck(pjsip_inv_session *inv, pjsip_rx_data *rdata)
{
    pjsip_tx_data tdata;
	pjsip_inv_create_ack(inv, rdata->msg_info.cseq->cseq,&inv->last_ack);
	// pjsip_tpselector tp_sel;
	// tp_sel.type = PJSIP_TPSELECTOR_TRANSPORT;
	// tp_sel.u.transport = inv->invite_tsx->transport;
	// pjsip_dlg_set_transport(inv->dlg, &tp_sel);
    pjsip_dlg_send_request(inv->dlg, inv->last_ack, -1, NULL);
	//pjsip_inv_send_msg(inv, tdata);
    //printBodyData((char*)rdata->msg_info.msg->body->data);

    return;
}