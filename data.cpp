//
//  data.cpp
//  PacketTraceExplorer
//
//  Created by Junxian Huang on 11/18/12.
//  Copyright (c) 2012 Junxian Huang. All rights reserved.
//

#include "data.h"

using namespace std;

pcap_dumper_t *dumper;

uint64 packet_count;
uint64 no_ip_count;
uint64 tcp_count;
uint64 udp_count;
uint64 icmp_count;
uint64 ignore_count1;
uint64 ignore_count2;
uint64 flow_count;
att_ether_hdr *peth;
ip *pip;
tcphdr *ptcp;
udphdr *pudp;
bool b1, b2;
bool is_first;
uint64 start_time_sec;
uint64 last_time_sec;
uint64 end_time_sec;
u_int ip1, ip2;
u_int ip_clt, ip_svr;
u_short port_clt, port_svr;
u_short payload_len;
double ts;
double last_prune_time;
double ack_delay;
u_int expected_ack;
u_int *opt_ts;

__gnu_cxx::hash_set<u_int> enb_ip;
__gnu_cxx::hash_set<u_int> core_ip;
__gnu_cxx::hash_map<u_int, u_int> enb_load;
__gnu_cxx::hash_map<u_int, u_int>::iterator itmap;

map<uint64, tcp_flow>::iterator flow_it;
map<uint64, tcp_flow>::iterator flow_it_tmp;
map<uint64, tcp_flow> client_flows; //only sample 1 flow for each client
map<string, pair<double, double> > big_flows;
map<string, double> gval; // g value
map<string, u_int> u_int_start;
map<string, double> double_start;
map<string, pair<double, double> >::iterator big_flow_it_tmp;
uint64 flow_index;
string big_flow_index;
bool is_target_flow;
tcp_flow *flow = NULL;
client_bw *bw_udp = NULL;
client_bw *bw_tcp = NULL;

void dispatcher_handler(u_char *c, const struct pcap_pkthdr *header, const u_char *pkt_data) {
    //c is not used
    packet_count++;
    //cout << header->len << " " << header->caplen << " time " << (header->ts.tv_sec) << endl;
    
    //record time
    if (is_first) {
        is_first = false;
        start_time_sec = header->ts.tv_sec;
        last_time_sec = start_time_sec;
        end_time_sec = start_time_sec;
        
        if (RUNNING_LOCATION == RLOC_CONTROL_SERVER || RUNNING_LOCATION == RLOC_CONTROL_CLIENT) {
            TIME_BASE = (double)(header->ts.tv_sec) + (double)header->ts.tv_usec / (double)USEC_PER_SEC;
        }
        cout << "TIME_BASE " << fixed << TIME_BASE << endl;
    } else {
        end_time_sec = header->ts.tv_sec;
    }
    
    ts = ((double)(header->ts.tv_sec) + (double)header->ts.tv_usec / (double)USEC_PER_SEC) - TIME_BASE;
    
    peth = (att_ether_hdr *)pkt_data;
    if (*((u_short *)(pkt_data + ETHER_HDR_LEN - 2)) == ETHERTYPE_IP) {
        pip = (ip *)(pkt_data + ETHER_HDR_LEN);
        b1 = isClient(pip->ip_src);
        b2 = isClient(pip->ip_dst);
        if ((b1 && !b2) || (!b1 && b2)) { //uplink or downlink
            if (b1 && !b2) { // uplink
                ip1 = peth->src_ip;
                ip2 = peth->dst_ip;
            } else { //downlink
                ip1 = peth->dst_ip;
                ip2 = peth->src_ip;
            }
            
            //enb_ip.insert(ip1);
            //core_ip.insert(ip2);
            /* //ENB load info
             enb_load[ip1]++;
             if (end_time_sec - LOAD_SAMPLE_PERIOD >= last_time_sec) {
             last_time_sec = end_time_sec;
             for (itmap = enb_load.begin() ; itmap != enb_load.end() ; itmap++) {
             cout << "ENBLOAD " << ConvertIPToString(itmap->first) << " " <<
             end_time_sec << " " << itmap->second << endl;
             }
             cout << endl;
             enb_load.clear();
             }//*/
        } else if (b1 && b2) { //ignore 1, both are client IP
            ignore_count1++;
        } else { //ignore 2, none are client IP
            ignore_count2++;
        }
        
        switch (pip->ip_p) {
            case IPPROTO_TCP:
                
                //break; //TODO only look at UDP now
                
                tcp_count++;
                ptcp = (tcphdr *)((u_char *)pip + BYTES_PER_32BIT_WORD * pip->ip_hl); //cast of u_char* is necessary
                payload_len = bswap16(pip->ip_len) - BYTES_PER_32BIT_WORD * (pip->ip_hl + ptcp->th_off);
                
                if (RUNNING_LOCATION == RLOC_CONTROL_CLIENT) {
                    //TCP client side throughput sampling
                    if (b1 && !b2 && bw_tcp == NULL && (ptcp->th_flags & TH_SYN) != 0) {
                        bw_tcp = new client_bw(ts, 1.0);
                    }
                    
                    //BW estimation ground truth
                    if (!b1 && b2) { //downlink
                        //sample throughput here, using differnt time window
                        bw_tcp->add_packet(payload_len, ts);
                    }//*/
                    
                    //G value ground truth
                    /*if (b1 && !b2) { // uplink
                        if ((ptcp->th_flags & TH_ACK) != 0) {
                            opt_ts = (u_int *)((u_char *)ptcp + 24);
                            if (double_start < 0) {
                                u_int_start = bswap32(*opt_ts);
                                double_start = ts;
                            } else {
                                cout << "G_GROUNDTRUTH " << (ts - double_start) / (bswap32(*opt_ts) - u_int_start) << endl;
                            }
                        }
                    }//*/
                    
                    //analyze client trace to understand the delay between data and ack
                    /*if (b1 && !b2) { // uplink
                        if (ack_delay >= 0 && expected_ack == bswap32(ptcp->th_ack)) {
                            cout << "ACK_DELAY " << ts - ack_delay << endl;
                            ack_delay = -1;
                        } else if (ack_delay >= 0 && expected_ack < bswap32(ptcp->th_ack)) {
                            //abandon this sample
                            ack_delay = -1;
                        }
                    } else if (!b1 && b2) { //downlink
                        if (ack_delay < 0 && payload_len > 0) {
                            ack_delay = ts;
                            expected_ack = bswap32(ptcp->th_seq) + payload_len;
                        }
                    }*/
                } else if (RUNNING_LOCATION == RLOC_ATT_SERVER ||
                           RUNNING_LOCATION == RLOC_ATT_CLIENT ||
                           RUNNING_LOCATION == RLOC_CONTROL_SERVER) {
                    if (b1 && !b2) { // uplink
                        ip_clt = pip->ip_src.s_addr;
                        ip_svr = pip->ip_dst.s_addr;
                        port_clt = bswap16(ptcp->th_sport);
                        port_svr = bswap16(ptcp->th_dport);
                    } else if (!b1 && b2) { //downlink
                        ip_clt = pip->ip_dst.s_addr;
                        ip_svr = pip->ip_src.s_addr;
                        port_clt = bswap16(ptcp->th_dport);
                        port_svr = bswap16(ptcp->th_sport);
                    } else {
                        break;
                    } 
                    
                    //big flow analysis
                    big_flow_index = ConvertIPToString(ip_clt) + string("_");
                    big_flow_index += ConvertIPToString(ip_svr) + string("_");
                    big_flow_index += NumberToString(port_clt) + string("_") + NumberToString(port_svr);
                    big_flow_it_tmp = big_flows.find(big_flow_index);
                    is_target_flow = false;
                    if (big_flow_it_tmp != big_flows.end()) {
                        //found big flow
                        if (ts >= big_flows[big_flow_index].first && ts <= big_flows[big_flow_index].first + big_flows[big_flow_index].second) {
                            //target flow found
                            is_target_flow = true;
                        }
                    }
                    
                    if (!is_target_flow) {
                        break;
                    }//*/
                    
                    //dump interested flow
                    if (//big_flow_index.compare("10.134.12.177_96.17.164.36_1556_1935") == 0 ||
                        //big_flow_index.compare("10.34.229.236_90.84.51.8_40214_80") == 0 ||
                        //big_flow_index.compare("10.8.102.148_107.14.33.153_2463_80") == 0 ||
                        //big_flow_index.compare("10.9.63.12_68.142.123.131_1216_80") == 0 ||
                        //big_flow_index.compare("10.9.63.12_8.254.15.254_1504_80") == 0) {
                        big_flow_index.compare("10.46.6.151_74.125.227.65_54587_80") == 0 //Youtube traffic, reusing flow
                        ) {
                        pcap_dump((u_char *)dumper, header, pkt_data);
                        pcap_dump_flush(dumper);
                     }//*/
                    
                    //flow statistics analysis
                    flow_index = port_clt * (((uint64)1) << 32) + ip_clt;
                    flow_it_tmp = client_flows.find(flow_index);
                    if (flow_it_tmp != client_flows.end()) {
                        //found flow
                    } else if (flow_it_tmp == client_flows.end() && (ptcp->th_flags & TH_SYN) != 0 && (b1 && !b2)) {
                        //no flow found, now uplink SYN packet
                        gval[big_flow_index] = -1.0;
                        double_start[big_flow_index] = -1.0;
                        u_int_start[big_flow_index] = 0;
                        
                        client_flows[flow_index].clt_ip = ip_clt;
                        flow_count++;
                        
                        flow = &client_flows[flow_index];
                        flow->svr_ip = ip_svr;
                        flow->clt_port = port_clt;
                        flow->svr_port = port_svr;
                        flow->start_time = ts;
                        flow->end_time = ts;
                        
                        if (RUNNING_LOCATION == RLOC_ATT_SERVER) {
                            if (client_flows.size() >= 100000 && ts - last_prune_time >= FLOW_MAX_IDLE_TIME / 2) {
                            //if (ts - last_prune_time >= FLOW_MAX_IDLE_TIME) {
                                cout << "Flowsize " << ts << " " << client_flows.size() << endl;
                                last_prune_time = ts; //only prune once for every 100 seconds
                                for (flow_it = client_flows.begin() ; flow_it != client_flows.end() ; ) {
                                    flow_it_tmp = flow_it;
                                    flow_it++;
                                    if (flow_it_tmp->second.end_time < ts - FLOW_MAX_IDLE_TIME)
                                        client_flows.erase(flow_it_tmp);
                                }//*/
                            }
                        }//*/
                    } else {
                        //no flow found and not link SYN packet
                        //could be ACKs after RST/FIN packets
                        //could be packets for long lived TCP flow
                        //just ignore
                        break;
                    }
                    
                    flow = &client_flows[flow_index];
                    flow->end_time = ts;
                    //if a terminate flow packet is here, terminate flow and output flow statistics
                    if ((ptcp->th_flags & TH_FIN) != 0 || (ptcp->th_flags & TH_RST) != 0) {
                        
                        flow->print((ptcp->th_flags & TH_FIN) | (ptcp->th_flags & TH_RST));
                        //delete this flow
                        client_flows.erase(flow_index);
                        
                    } else { // all packets of this flow goes to here except for the FIN and RST packets
                        if (b1 && !b2) { // uplink
                            if (payload_len > 0) {
                                flow->total_up_payloads += payload_len;
                            }
                            if (BYTES_PER_32BIT_WORD * ptcp->th_off == 32 || BYTES_PER_32BIT_WORD * ptcp->th_off == 40)
                                //there is TCP timestamp options, 40 for SYN packet
                                flow->has_ts_option_clt = true;
                        } else if (!b1 && b2) { //downlink
                            if (payload_len > 0) {
                                flow->total_down_payloads += payload_len;
                                flow->last_byte_time = ts;
                                if (flow->first_byte_time == 0) {
                                    flow->first_byte_time = ts;
                                }
                            }
                            if (BYTES_PER_32BIT_WORD * ptcp->th_off == 32 || BYTES_PER_32BIT_WORD * ptcp->th_off == 40) //there is TCP timestamp options
                                flow->has_ts_option_svr = true;
                        }
                    }
                    
                    /*//RTT and TCP pattern analysis
                    if (ip_clt == flow->clt_ip && ip_svr == flow->svr_ip &&
                        port_clt == flow->clt_port && port_svr == flow->svr_port) {
                        if (b1 && !b2) { // uplink
                            flow->update_ack_x(bswap32(ptcp->th_ack), payload_len, ts);
                        } else if (!b1 && b2) { //downlink
                            flow->update_seq_x(bswap32(ptcp->th_seq), payload_len, ts);
                        }
                    }//*/
                    
                    
                    //BWE analysis
                    if (gval[big_flow_index] < 0) {
                        //do G inference for BW estimate
                        if (b1 && !b2) { // uplink
                            if ((ptcp->th_flags & TH_ACK) != 0) {
                                opt_ts = (u_int *)((u_char *)ptcp + 24);
                                if (double_start[big_flow_index] < 0) {
                                    u_int_start[big_flow_index] = bswap32(*opt_ts);
                                    double_start[big_flow_index] = ts;
                                    
                                    //cout << "TCP header length of the first uplink ACK " << BYTES_PER_32BIT_WORD * ptcp->th_off << endl;
                                    if (BYTES_PER_32BIT_WORD * ptcp->th_off < 32) {
                                        //no TCP timestamp options, can't use G inference and BW estimation
                                        double_start[big_flow_index] = -1.0;
                                    }
                                } else if (double_start[big_flow_index] > 0 && ts - double_start[big_flow_index] > GVAL_TIME) {
                                    gval[big_flow_index] = (ts - double_start[big_flow_index]) / (bswap32(*opt_ts) - u_int_start[big_flow_index]);
                                    cout << big_flow_index << " G_VALUE infered time offset " << (ts - double_start[big_flow_index]) << " G: " << gval[big_flow_index] << " seconds/tick" << endl;
                                    flow->gval = gval[big_flow_index];
                                }
                            }
                        }
                    } else {
                        //do BW estimation
                        if (ip_clt == flow->clt_ip && ip_svr == flow->svr_ip &&
                            port_clt == flow->clt_port && port_svr == flow->svr_port) {
                            if (b1 && !b2) { // uplink
                                if ((ptcp->th_flags & TH_ACK) != 0) {
                                    opt_ts = (u_int *)((u_char *)ptcp + 24);
                                    flow->update_ack(bswap32(ptcp->th_ack), payload_len, gval[big_flow_index] * bswap32(*opt_ts), ts);
                                }
                            } else if (!b1 && b2) { //downlink
                                //payload_len > < 0 check inside update_seq
                                flow->update_seq(bswap32(ptcp->th_seq), payload_len, ts);
                            }
                        }
                    }//*/
                    
                    
                    /*//G inference
                    if (RUNNING_LOCATION == RLOC_CONTROL_SERVER) {
                        if (b1 && !b2) { // uplink
                            if ((ptcp->th_flags & TH_ACK) != 0) {
                                opt_ts = (u_int *)((u_char *)ptcp + 24);
                                if (double_start[big_flow_index] < 0 && ts > 100) {
                                    u_int_start[big_flow_index] = bswap32(*opt_ts);
                                    double_start[big_flow_index] = ts;
                                } else if (double_start[big_flow_index] > 0) {
                                    gval[big_flow_index] = (ts - double_start[big_flow_index]) / (bswap32(*opt_ts) - u_int_start[big_flow_index]);
                                    if (packet_count % 3 == 0)
                                        cout << "G_INFERENCE " << (ts - double_start[big_flow_index]) << " " << gval[big_flow_index] << endl;
                                }
                            }
                        }
                    }//*/
                    
                    /*if (RUNNING_LOCATION == RLOC_CONTROL_SERVER && packet_count >= 206870 && packet_count <= 243382) {
                        if (b1 && !b2) { // uplink
                            cout << ts << " " << packet_count;
                            cout << " src " << ConvertIPToString(pip->ip_src.s_addr) << " ";
                            cout << " dst " << ConvertIPToString(pip->ip_dst.s_addr) << " ";
                            cout << bswap32(ptcp->th_ack) << endl;//
                        } else if (!b1 && b2) { //downlink
                            /*cout << ts << " " << packet_count;
                            cout << " src " << ConvertIPToString(pip->ip_src.s_addr) << " ";
                            cout << " dst " << ConvertIPToString(pip->ip_dst.s_addr) << " ";
                            cout << bswap32(ptcp->th_seq) << endl;//
                        }

                    }*/
                    /*if (packet_count == 38084) {
                        cout << "create flow here" << endl;
                        flow = new tcp_flow(ip_svr, ip_clt, port_svr, port_clt);
                    }
                    if (packet_count >= 38084) {
                        //flow is not null
                        if (ip_clt == flow->clt_ip &&
                            ip_svr == flow->svr_ip &&
                            port_clt == flow->clt_port &&
                            port_svr == flow->svr_port) {
                            if (b1 && !b2) { // uplink
                                //cout << "target uplink packet " << packet_count << endl;
                                //target flow, ack packets
                                if (bswap32(ptcp->th_ack) > 0) {
                                    flow->update_ack(bswap32(ptcp->th_ack), ts);
                                }
                            } else if (!b1 && b2) { //downlink
                                //cout << "target downlink packet " << packet_count << endl;
                                //target flow
                                if (payload_len >= TCP_MAX_PAYLOAD) {
                                    flow->update_seq(bswap32(ptcp->th_seq), payload_len, ts);
                                }
                            }
                        }
                    }//*/
                }
                
                break;
            case IPPROTO_UDP:
                udp_count++;
                pudp = (udphdr *)((u_char *)pip + sizeof(ip));
                payload_len = bswap16(pudp->uh_ulen) - UDP_HDR_LEN;
                if (RUNNING_LOCATION == RLOC_CONTROL_CLIENT) {
                    //UDP client side throughput sampling
                    if (!b1 && b2) { //downlink
                        //sample throughput here, using differnt time window
                        if (bw_udp == NULL) {
                            bw_udp = new client_bw(ts, 0.5);
                        }
                        bw_udp->add_packet(payload_len, ts);
                    }
                }
                /*//DNS analysis
                if (b1 && !b2) { // uplink
                    ip_clt = pip->ip_src.s_addr;
                    ip_svr = pip->ip_dst.s_addr;
                    port_clt = bswap16(pudp->uh_sport);
                    port_svr = bswap16(pudp->uh_dport);
                } else if (!b1 && b2) { //downlink
                    ip_clt = pip->ip_dst.s_addr;
                    ip_svr = pip->ip_src.s_addr;
                    port_clt = bswap16(pudp->uh_dport);
                    port_svr = bswap16(pudp->uh_sport);
                } else {
                    break;
                }
                
                if (port_svr == 53) {
                
                    //UDP flow statistics analysis
                    flow_index = port_clt * (((uint64)1) << 32) + ip_clt;
                    flow_it_tmp = client_flows.find(flow_index);
                    if (flow_it_tmp != client_flows.end()) {
                        //found flow
                    } else if (flow_it_tmp == client_flows.end() && (b1 && !b2)) {
                        client_flows[flow_index].clt_ip = ip_clt;
                        flow_count++;
                    
                        flow = &client_flows[flow_index];
                        flow->svr_ip = ip_svr;
                        flow->clt_port = port_clt;
                        flow->svr_port = port_svr;
                        flow->start_time = ts;
                        flow->end_time = ts;
                    
                        if (RUNNING_LOCATION == RLOC_ATT_SERVER) {
                            if (client_flows.size() >= 100000 && ts - last_prune_time >= FLOW_MAX_IDLE_TIME / 2) {
                                //if (ts - last_prune_time >= FLOW_MAX_IDLE_TIME) {
                                cout << "Flowsize " << ts << " " << client_flows.size() << endl;
                                last_prune_time = ts; //only prune once for every 100 seconds
                                for (flow_it = client_flows.begin() ; flow_it != client_flows.end() ; ) {
                                    flow_it_tmp = flow_it;
                                    flow_it++;
                                    if (flow_it_tmp->second.end_time < ts - FLOW_MAX_IDLE_TIME)
                                        client_flows.erase(flow_it_tmp);
                                }//
                            }
                        }//
                    } else {
                        break;
                    }
                
                    flow = &client_flows[flow_index];
                    if (b1 && !b2) { // uplink
                        flow->end_time = ts;
                    } else if (!b1 && b2) { //downlink
                        cout << "D " << ConvertIPToString(flow->svr_ip) << " " << (ts - flow->end_time) << endl;
                        client_flows.erase(flow_index);
                    }
                }//*/
                
                //cout << "UDP src port " << bswap16(pudp->uh_sport) << " dst port " << bswap16(pudp->uh_dport) << endl;
                break;
            case IPPROTO_ICMP:
                icmp_count++;
                break;
            default:
                break;
        }
    } else {
        no_ip_count++;
        //cout << "Packet " << packet_count << " not IP " << peth->ether_type << endl;
    }
    
    //if (packet_count > 1000)
    //    exit(0);
}

//called for each trace file, should not reset persistent variables across different traces here
int init_global() {
    is_first = true;
    start_time_sec = 0;
    last_time_sec = 0;
    end_time_sec = 0;
    
    packet_count = 0;
    no_ip_count = 0;
    tcp_count = 0;
    udp_count = 0;
    icmp_count = 0;
    ignore_count1 = 0;
    ignore_count2 = 0;
    flow_count = 0;
    
    ack_delay = -1;
    
    enb_ip.clear();
    core_ip.clear();
    enb_load.clear();
    client_flows.clear();
    last_prune_time = 0;
    
    big_flow_index = "default";
    big_flows.clear();
    gval.clear();
    u_int_start.clear();
    double_start.clear();
    if (RUNNING_LOCATION == RLOC_ATT_SERVER || RUNNING_LOCATION == RLOC_ATT_CLIENT) {
        //readin big flow data
        const char *big_flow_path;
        if (RUNNING_LOCATION == RLOC_ATT_SERVER) {
            big_flow_path = "/q/gp13/dpi/tcprx/work/data/flow_target.data";
        } else if (RUNNING_LOCATION == RLOC_ATT_CLIENT) {
            big_flow_path = "/Users/hjx/Documents/4G/figures/traffic/data/flow_target2.data";
        }
        
        string key;
        double start, duration;
        ifstream ifs(big_flow_path);
        while (ifs >> key >> start >> duration) {
            big_flows[key] = make_pair(start, duration);
            //big_flows[key] = pair<double, double>(start, duration); //the same
        }
    }
    cout << "BIG_FLOWS size " << big_flows.size() << endl;
    
    //dump interest.pcap
    pcap_t *px = pcap_open_dead(DLT_EN10MB, 64);
    dumper = pcap_dump_open(px, "interest.pcap");
}

int read_pcap_trace(const char * filename) {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *fp;
    /* Open the capture file */
    if ((fp = pcap_open_offline(filename, errbuf)) == NULL) {
        fprintf(stderr,"Unable to open the file %s\n", filename);
        return 0;
    }
    
    if (pcap_datalink(fp) == DLT_LINUX_SLL) {
        ETHER_HDR_LEN = 16;
    } else {
        ETHER_HDR_LEN = 14;
    }
    
    
    /* read and dispatch packets until EOF is reached */
    pcap_loop(fp, 0, dispatcher_handler, NULL);
    pcap_close(fp);
    
    /*__gnu_cxx::hash_set<u_int>::iterator it;
     for (it = enb_ip.begin() ; it != enb_ip.end() ; it++)
     cout << "ENB " << ConvertIPToString(*it) << endl;
     for (it = core_ip.begin() ; it != core_ip.end() ; it++)
     cout << "CORE " << ConvertIPToString(*it) << endl;//*/
    
    cout << "PacketCount " << packet_count << " " << filename << endl;
    cout << "NoIpCount " << no_ip_count << " " << filename << endl;
    cout << "TcpCount " << tcp_count << " " << filename << endl;
    cout << "UdpCount " << udp_count << " " << filename << endl;
    cout << "IcmpCount " << icmp_count << " " << filename << endl;
    cout << "IgnoreCount1 " << ignore_count1 << " " << filename << endl;
    cout << "IgnoreCount2 " << ignore_count2 << " " << filename << endl;
    cout << "FlowCount " << flow_count << " " << filename << endl; // count of all flows, some may not closed at all
    cout << "StartTime " << start_time_sec << endl;
    cout << "EndTime " << end_time_sec << endl;
    cout << "Duration " << end_time_sec - start_time_sec << endl;
    return 1;
}
