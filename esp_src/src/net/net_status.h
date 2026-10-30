/*
 * SML-STAT-NET: the ESP's network and services, for the PIC's menu and
 * status. Sent on change (uptime aside) and every 10 s; the periodic frame
 * also brings a freshly reset PIC into SML mode.
 */
#ifndef NET_STATUS_H
#define NET_STATUS_H

void net_status_service(void);

#endif // NET_STATUS_H
