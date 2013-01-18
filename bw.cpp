//
//  bw.cpp
//  PacketTraceExplorer
//
//  Created by Junxian Huang on 12/1/12.
//  Copyright (c) 2012 Junxian Huang. All rights reserved.
//

#include "bw.h"

tcp_flow::tcp_flow() {
    svr_ip = 0;
    clt_ip = 0;
    svr_port = 0;
    clt_port = 0;
    start_time = 0;
    end_time = 0;
    idle_time = 0;
    
    target = start_time + GVAL_TIME;
    bwstep = 0.5;
    last_time = -1;
    last_throughput = -1;
    
    has_ts_option_clt = false;
    has_ts_option_svr = false;
    first_byte_time = 0;
    last_byte_time = 0;

    total_down_payloads = 0;
    total_up_payloads = 0;
    bytes_in_fly = 0;
    max_bytes_in_fly = 0;
    packet_count = 0;
    dup_ack_count = 0;
    outorder_seq_count = 0;
    
    total_bw = 0;
    sample_count = 0;

    reset_seq();
    reset_ack();
}

//should only be called called by SYN packet
tcp_flow::tcp_flow(u_int _svr_ip, u_int _clt_ip, u_short _svr_port, u_short _clt_port, double _start_time) {
    svr_ip = _svr_ip;
    clt_ip = _clt_ip;
    svr_port = _svr_port;
    clt_port = _clt_port;
    start_time = _start_time;
    end_time = start_time;
    idle_time = 0;
    
    target = start_time + GVAL_TIME;
    bwstep = 0.5;
    last_time = -1;
    last_throughput = -1;
    
    has_ts_option_clt = false;
    has_ts_option_svr = false;
    first_byte_time = 0;
    last_byte_time = 0;
    
    total_down_payloads = 0;
    total_up_payloads = 0;
    bytes_in_fly = 0;
    max_bytes_in_fly = 0;
    packet_count = 0;
    dup_ack_count = 0;
    outorder_seq_count = 0;
    
    total_bw = 0;
    sample_count = 0;
    
    reset_seq();
    reset_ack();
}

//called during init or any abnormal happens
void tcp_flow::reset_seq() {
    si = -1;
    sx = -1;
    memset(seq_down, 0, sizeof seq_down);
    memset(seq_ts, 0, sizeof seq_ts);
    
    reset_ack();
    //reset ack does not need to reset seq, since seq is ahead of ack
    //but reset seq needs to reset ack, since the existing acks are old
}

//called during init or any abnormal happens
void tcp_flow::reset_ack() {
    ai = -1;
    ax = -1;
    memset(ack_down, 0, sizeof ack_down);
    memset(ack_ts, 0, sizeof ack_ts);
}

//before this function is called, need to make sure that payload_len >= 1358 (should be == 1358 actually)
void tcp_flow::update_seq(u_int seq, u_short payload_len, double ts) {
    if (si != -1 && seq_down[si] > 0 && seq_down[si] > seq) {
        //out of order seq, ideally seq_down[si] == seq
        reset_seq();
        return;
    }
    
    if (si != -1 && ts >= seq_ts[si] + IDLE_THRESHOLD) {
        //there is long idle time between this packet and previous packet
        reset_seq();
        return;
    }
    
    if (payload_len == 0) {
        //does not consider 0-payload data packets, since it's ACK packets for uplink data
        reset_seq();
        return;
    }

    //!!!!!! seq here stores the next seq to send, so the corresponding ACK packet's ack is equal to this seq
    si = get_si_next(si);
    seq_down[si] = seq + payload_len;
    seq_ts[si] = ts;
    if (sx == -1) {
        sx = si;
    } else if (sx == si) {
        sx = get_si_next(si);
    }
}

void tcp_flow::update_ack(u_int ack, u_short payload_len, double ts, double _actual_ts) {
    if (ai != -1 && ack_down[ai] > 0 && ack_down[ai] >= ack) {
        //out of order ack, or dup ack, ideally ack_down[ai] < ack
        reset_ack();
        return;
    }
    
    if (ai != -1 && ts >= ack_ts[ai] + IDLE_THRESHOLD) {
        //there is long idle time between this packet and previous packet
        reset_ack();
        return;
    }
    
    if (payload_len > 0) {
        //we only consider 0-payload ACK packets, so that uplink data packets are not considered
        reset_ack();
        return;
    }

    actual_ts = _actual_ts;
    ai = get_ai_next(ai);
    ack_down[ai] = ack;
    ack_ts[ai] = ts;
    if (ax == -1) {
        ax = ai;
    } else if (ax == ai) {
        ax = get_ai_next(ai);
    }
    
    //check if existing bw sample exists
    int m = ax;
    //at least with two full packet samples
    while (m != ai && ack_down[m] != 0 && ack_ts[ai] - ack_ts[m] >= 20 * gval) { // 1/20 = 5% error
        if (bw_estimate(m)) {
            //reset after one successful bandwidth estimation  ? ? ?
            reset_seq();
            break;
        }
        m = get_ai_next(m);
    }
}

//functions for RTT analysis
void tcp_flow::update_seq_x(u_int seq, u_short payload_len, double ts) {
    if (si != -1 && seq_down[si] > 0 && seq_down[si] > seq) {
        outorder_seq_count++;
        return;
    }
    si = get_si_next(si);
    seq_down[si] = seq + payload_len;
    seq_ts[si] = ts;
    if (sx == -1) {
        sx = si;
    } else if (sx == si) {
        sx = get_si_next(si);
    }
    
    if (payload_len > 0) {
        if (ai == -1) {
            bytes_in_fly = payload_len;
        } else {
            bytes_in_fly = seq_down[si] - ack_down[ai];
        }
        if (bytes_in_fly > max_bytes_in_fly)
            max_bytes_in_fly = bytes_in_fly;
    }
    
    if (last_time < 0) {
        last_time = ts;
    }
    if (payload_len > 0 && ts - last_time > IDLE_THRESHOLD) {
        idle_time += (ts - last_time);
    }
    
    if (packet_count == 1) {
        syn_rtt = ts - last_time;
    } else if (packet_count == 2) {
        syn_ack_rtt = ts - last_time;
    }
    
    last_time = ts;
    packet_count++;
}

void tcp_flow::update_ack_x(u_int ack, u_short payload_len, double _actual_ts) {
    if (ai != -1 && ack_down[ai] > 0 && ack_down[ai] == ack && payload_len == 0) {
        //if payload not 0, this is uplink data packet
        dup_ack_count++;
        return;
    }
    ai = get_ai_next(ai);
    ack_down[ai] = ack;
    ack_ts[ai] = _actual_ts;
    if (ax == -1) {
        ax = ai;
    } else if (ax == ai) {
        ax = get_ai_next(ai);
    }
    
    //ACK RTT analysis
    short s1 = find_seq_by_ack(ack_down[ai], sx, si);
    if (s1 != -1 && payload_len == 0) {
        //cout << "AR " << " " << ack_ts[ai] - seq_ts[s1] << " " << bytes_in_fly << endl;
    }
    
    //update bytes in fly after analysis
    if (bytes_in_fly > 0) {
        bytes_in_fly = seq_down[si] - ack_down[ai];
    }
    
    if (last_time < 0) {
        last_time = _actual_ts;
    }
    if (payload_len > 0 && _actual_ts - last_time > IDLE_THRESHOLD) {
        idle_time += (_actual_ts - last_time);
    }
    
    if (packet_count == 1) {
        syn_rtt = _actual_ts - last_time;
    } else if (packet_count == 2) {
        syn_ack_rtt = _actual_ts - last_time;
    }

    last_time = _actual_ts;
    packet_count++;
}


//test with start_ai and ai
bool tcp_flow::bw_estimate(short start_ai) {
    short s1 = find_seq_by_ack(ack_down[start_ai], sx, si);
    short s2 = find_seq_by_ack(ack_down[ai], sx, si);
    if (s1 == -1 || s2 == -1 || s1 == s2)
        return false;
    
    if ((seq_ts[s2] - seq_ts[s1] == 0) ||
        (seq_ts[s2] - seq_ts[s1] > 0)) {
        double bw = (double)(seq_down[s2] - seq_down[s1]) * 8.0 / (ack_ts[ai] - ack_ts[start_ai]) / ONE_MILLION;
        double bw_send;
        if (seq_ts[s2] - seq_ts[s1] == 0) {
            bw_send = BW_MAX_BITS_PER_SECOND * 2.0 / ONE_MILLION;
        } else {
            bw_send = (double)(seq_down[s2] - seq_down[s1]) * 8.0 / (seq_ts[s2] - seq_ts[s1]) / ONE_MILLION;
        }

        if (bw < 45000000.0 / ONE_MILLION  &&
            bw_send >= BW_MAX_BITS_PER_SECOND / ONE_MILLION) {
            
            if (actual_ts - last_time > bwstep) {
                total_bw += bw;
                sample_count++;
            }
            
            /*
             //time sample for each step
             string big_flow_index = ConvertIPToString(clt_ip) + string("_");
            big_flow_index += ConvertIPToString(svr_ip) + string("_");
            big_flow_index += NumberToString(clt_port) + string("_") + NumberToString(svr_port);
            
            while (actual_ts > target + 0.4 * bwstep) {
                if (abs(last_time - target) <= 0.4 * bwstep) {
                    //cout << "BW_ESTIMATE_SAMPLE " << big_flow_index << " " << target << " " << last_throughput << " " << abs(ack_ts[ai] - ack_ts[start_ai]) << " " << (seq_down[s2] - seq_down[s1]) << endl;
                    total_bw += last_throughput;
                    sample_count++;
                } else {
                    //cout << "BW_ESTIMATE_SAMPLE " << target << " " << 0 << " " << 0 << endl;
                }
                target += bwstep;
            }
            //actual_ts <= target + 0.2 * bwstep
            if (last_time <= target && target <= actual_ts) {
                if (abs(last_time - target) < abs(actual_ts - target)) {
                    //cout << "BW_ESTIMATE_SAMPLE " << big_flow_index << " " << target << " " << last_throughput << " " << (ack_ts[ai] - ack_ts[start_ai]) << " " << (seq_down[s2] - seq_down[s1]) << endl;
                    total_bw += last_throughput;
                    sample_count++;
                } else {
                    //cout << "BW_ESTIMATE_SAMPLE " << big_flow_index << " " << target << " " << bw << " " << (ack_ts[ai] - ack_ts[start_ai]) << " " << (seq_down[s2] - seq_down[s1]) << endl;
                    total_bw += bw;
                    sample_count++;
                }
                target += bwstep;
            } else if (target < last_time) {
                if (abs(last_time - target) <= 0.4 * bwstep) {
                    //cout << "BW_ESTIMATE_SAMPLE " << big_flow_index << " " << target << " " << last_throughput << " " << (ack_ts[ai] - ack_ts[start_ai]) << " " << (seq_down[s2] - seq_down[s1]) << endl;
                    total_bw += last_throughput;
                    sample_count++;
                } else {
                    //cout << "BW_ESTIMATE_SAMPLE " << target << " " << 0 << " " << 0 << endl;
                }
                target += bwstep; //should have already output when scanning last_time
            } else if (target > actual_ts) {
                
            //} else {//impossible to reach here
            }//*/
            
            /*cout << "BW_ESTIMATE " << ConvertIPToString(svr_ip);
            cout << " => " << ConvertIPToString(clt_ip);
            cout.precision(6);
            cout << " time " << fixed << actual_ts;
            cout << " BW " << bw << " Mbps";
            //cout << " start " << hex << ack_down[start_ai] << " end " << ack_down[ai] << dec;
            cout << " ACK_GAP " << ack_down[ai] - ack_down[start_ai];
            cout << " ACK_TIME_GAP " << ack_ts[ai] - ack_ts[start_ai];
            cout << endl;//*/
            
            last_time = actual_ts;
            last_throughput = bw;
            return true;
        }
    }

    return false;
}

short tcp_flow::find_seq_by_ack(u_int ack, short start, short end) {
    int range;
    if (start <= end) {
        range = end - start + 1;
    } else {
        range = end + SEQ_INDEX_MAX - start + 1;
    }
    
    if (range <= 8) {
        for (int i = start ; i <= end ; i++) {
            if (seq_down[i] == ack)
                return i;
        }
    } else {
        int mid = (start + range / 2) % SEQ_INDEX_MAX;
        if (ack == seq_down[mid]) {
            //bingo!
            return mid;
        } else if (ack > seq_down[mid]) {
            //second half
            return find_seq_by_ack(ack, get_si_next(mid), end);
        } else {
            return find_seq_by_ack(ack, start, get_si_previous(mid));
        }
    }
    return -1;
}

short tcp_flow::get_si_next(short c) {
    return (c + 1) % SEQ_INDEX_MAX;
}

short tcp_flow::get_si_previous(short c) {
    return (c - 1 + SEQ_INDEX_MAX) % SEQ_INDEX_MAX;
}

short tcp_flow::get_ai_next(short c) {
    return (c + 1) % ACK_INDEX_MAX;
}

short tcp_flow::get_ai_previous(short c) {
    return (c - 1 + ACK_INDEX_MAX) % ACK_INDEX_MAX;
}

void tcp_flow::print(u_short processed_flags) {
    if (syn_rtt < 0)
        syn_rtt = 0;
    if (syn_ack_rtt < 0)
        syn_ack_rtt = 0;
    double avg_bw = 0;
    if (sample_count > 0)
        avg_bw = (double)(total_bw / (double)sample_count);

    printf("%s ", ConvertIPToString(clt_ip)); // 1
    printf("%s %d %d %.4lf %.4lf %.4lf %.4lf %d %d %d %lld %lld %.4lf %lld %.4lf %.4lf %lld %lld %.4lf %d\n",
           ConvertIPToString(svr_ip), //2
           clt_port, //3
           svr_port, //4
           start_time, //5
           first_byte_time - start_time, //6
           last_byte_time - start_time, //7
           end_time - start_time, //8
           processed_flags, //9
           has_ts_option_clt, //10
           has_ts_option_svr, //11
           total_down_payloads, //12
           total_up_payloads, //13
           idle_time, //14
           max_bytes_in_fly, //15
           syn_rtt, //16
           syn_ack_rtt, //17
           dup_ack_count, //18
           outorder_seq_count, //19
           avg_bw, //20
           sample_count //21
           );    
}
